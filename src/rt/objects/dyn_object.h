#pragma once

#include "../../lang/interpreter.h"
#include "../rt.h"
#include "region.h"
#include "visit.h"

#include <atomic>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace rt::core
{
  class CownObject;
}

namespace rt::objects
{
  constexpr uintptr_t ImmutableTag{1};
  const std::string PrototypeField{"__proto__"};
  const std::string ParentField{"__parent__"};

  Region* get_region(DynObject* obj);

  Region* get_local_region();
  void set_local_region(Region* region);

  // Representation of objects
  class DynObject
  {
    friend class Reference;
    friend objects::DynObject* rt::make_iter(objects::DynObject* obj);
    friend class ui::MermaidUI;
    friend class ui::ObjectGraphDiagram;
    friend class core::CownObject;
    friend void destruct(DynObject* obj);
    friend void dealloc(DynObject* obj);
    template<typename Pre, typename Post>
    friend void visit(Edge, Pre, Post);
    friend Region* get_region(DynObject* obj);
    friend void add_to_region(Region* r, DynObject* target, DynObject* source);
    friend void merge_regions(DynObject* src, DynObject* sink);
    friend void move_objects(Region* src, Region* sink);

    // TODO: Not concurrency safe
    inline static size_t count{0};
    // TODO: Not concurrency safe
    inline static std::set<DynObject*> all_objects{};

    size_t rc{1};
    RegionPointer region{nullptr};
    DynObject* prototype{nullptr};

    std::map<std::string, DynObject*> fields{};

  public:
    size_t change_rc(signed delta)
    {
      if (!(is_immutable() || is_cown()))
      {
        assert(delta == 0 || rc != 0);
        rc += delta;
        // Check not underflowing.
        assert(rc >> 62 == 0);
        return rc;
      }

      // TODO Follow SCC chain to the root.
      // Have to use atomic as this can be called from multiple threads.
      return std::atomic_ref(rc).fetch_add(delta, std::memory_order_relaxed) +
        delta;
    }

    // prototype is borrowed, the caller does not need to provide an RC.
    DynObject(
      DynObject* prototype_ = nullptr,
      Region* containing_region = get_local_region())
    : prototype(prototype_)
    {
      assert(containing_region != nullptr);
      count++;
      all_objects.insert(this);
      region = containing_region;
      containing_region->objects.insert(this);

      if (prototype != nullptr)
      {
        // prototype->change_rc(1);
        objects::add_reference(this, prototype);
      }
      std::cout << "Allocate: " << this << std::endl;
    }

    // TODO This should use prototype lookup for the destructor.
    virtual ~DynObject()
    {
      // Erase from set of all objects, and remove count if found.
      auto matched = all_objects.erase(this);
      count -= matched;

      // If it wasn't in the all_objects set, then it was a special object
      // that we don't track for leaks, otherwise, we need to check if the
      // RC is zero.
      if (change_rc(0) != 0 && matched != 0)
      {
        std::stringstream stream;
        stream << this;
        stream << "  still has references";
        ui::error(stream.str(), this);
      }

      auto r = get_region(this);
      if (!is_immutable() && r != nullptr)
        r->objects.erase(this);

      std::cout << "Deallocate: " << get_name() << std::endl;
    }

    size_t get_rc()
    {
      return rc;
    }

    /// @brief The string representation of this value to
    /// TODO remove virtual once we have primitive functions.
    virtual std::string get_name()
    {
      std::stringstream stream;
      stream << this;
      return stream.str();
    }

    /// TODO remove virtual once we have primitive functions.
    virtual DynObject* is_primitive()
    {
      return nullptr;
    }

    virtual bool is_opaque()
    {
      return false;
    }

    bool is_cown()
    {
      return region.get_ptr() == objects::cown_region;
    }

    void freeze()
    {
      std::vector<Region*> dead_regions;
      // TODO SCC algorithm
      visit(this, [&](Edge e) {
        auto obj = e.target;
        if (!obj || obj->is_immutable() || obj->is_cown())
          return false;

        auto r = get_region(obj);
        r->objects.erase(obj);
        // FIXME: Region can remain clean, if the RC was 1 when this was called.
        r->mark_dirty();

        // There are several options to deal with region objects that become
        // frozen. They should be kept to some extent, to keep the existing
        // object structure.
        //
        // The simplest approach taken here, is to simply remove the prototype
        // therefore turning the object into an ordinary dictionary.
        if (r->bridge == obj)
        {
          r->bridge = nullptr;
          auto old_proto = obj->set_prototype(nullptr);
          rt::remove_reference(this, old_proto);
          dead_regions.push_back(r);
        }

        // Make obj immutable
        obj->region.set_ptr(immutable_region);
        immutable_region->objects.insert(obj);

        return true;
      });

      // The termination has to be delayed to make sure that all object are
      // frozen before the termination.
      for (auto r : dead_regions)
      {
        r->terminate_region();
      }
    }

    bool is_immutable()
    {
      return region.get_ptr() == immutable_region;
    }

    [[nodiscard]] std::optional<DynObject*> get(std::string name)
    {
      auto result = fields.find(name);
      if (result != fields.end())
        return result->second;

      if (name == PrototypeField)
        return prototype;

      // Search the prototype chain.
      // TODO make this iterative.
      if (prototype != nullptr)
        return prototype->get(name);

      // No field or prototype chain found.
      return std::nullopt;
    }

    /// A destructive read of the value.
    [[nodiscard]] DynObject* erase(std::string name)
    {
      auto result = fields.find(name);
      if (result != fields.end())
      {
        auto value = result->second;
        fields.erase(result);
        return value;
      }

      if (name == PrototypeField)
      {
        auto value = prototype;
        prototype = nullptr;
        return value;
      }

      // Search the prototype chain.
      // TODO make this iterative.
      if (prototype != nullptr)
        return prototype->erase(name);

      // No field or prototype chain found.
      return nullptr;
    }

    void assert_modifiable()
    {
      if (is_immutable())
      {
        ui::error("Cannot mutate immutable object", this);
      }

      if (is_opaque())
      {
        if (is_cown())
        {
          ui::error("Cannot mutate a cown that is not aquired", this);
        }
        else
        {
          ui::error("Cannot mutate opaque object", this);
        }
      }
    }

    [[nodiscard]] virtual DynObject* set(std::string name, DynObject* value)
    {
      assert_modifiable();

      if (name == PrototypeField)
      {
        return set_prototype(value);
      }

      DynObject* old = fields[name];
      fields[name] = value;
      return old;
    }

    // The caller must provide an rc for value.
    [[nodiscard]] DynObject* set_prototype(DynObject* value)
    {
      assert_modifiable();
      DynObject* old = prototype;
      prototype = value;
      return old;
    }

    DynObject* get_prototype()
    {
      return prototype;
    }

    static size_t get_count()
    {
      return count;
    }

    static std::set<DynObject*> get_objects()
    {
      return all_objects;
    }
  };

  template<typename Pre, typename Post>
  inline void visit(Edge e, Pre pre, Post post)
  {
    if (!pre(e))
      return;

    constexpr bool HasPost = !std::is_same_v<Post, NopDO>;
    constexpr uintptr_t POST{1};

    std::vector<std::pair<utils::TaggedPointer<DynObject>, std::string>> stack;

    auto visit_object = [&](DynObject* obj) {
      if (obj == nullptr)
        return;
      if constexpr (HasPost)
        stack.push_back({{obj, POST}, ""});
      // TODO This will need to depend on the type of object.
      for (auto& [key, field] : obj->fields)
        stack.push_back({obj, key});
      if (obj->prototype != nullptr)
        stack.push_back({obj, PrototypeField});
    };

    visit_object(e.target);

    while (!stack.empty())
    {
      auto [obj, key] = stack.back();
      auto obj_ptr = obj.get_ptr();
      stack.pop_back();

      if (HasPost && obj.get_tag() == POST)
      {
        post(obj_ptr);
        continue;
      }

      DynObject* next = obj_ptr->get(key).value();
      if (pre({obj_ptr, key, next}))
      {
        visit_object(next);
      }
    }
  }

  template<typename Pre, typename Post>
  inline void visit(DynObject* start, Pre pre, Post post)
  {
    visit(Edge{nullptr, "", start}, pre, post);
  }

  template<typename Pre, typename Post>
  inline void visit(Region* start, Pre pre, Post post)
  {
    for (auto obj : start->get_objects())
    {
      visit(obj, pre, post);
    }
  }

} // namespace rt::objects
