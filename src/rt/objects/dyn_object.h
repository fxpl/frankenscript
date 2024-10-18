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

namespace rt::objects
{
  constexpr uintptr_t ImmutableTag{1};
  const std::string PrototypeField{"__proto__"};
  const std::string ParentField{"__parent__"};

  // Representation of objects
  class DynObject
  {
    friend class Reference;
    friend objects::DynObject* rt::make_iter(objects::DynObject* obj);
    friend void rt::ui::mermaid(
      std::vector<objects::DynObject*>& roots,
      std::ostream& out,
      std::vector<objects::DynObject*>* taint);
    friend void destruct(DynObject* obj);
    friend void dealloc(DynObject* obj);
    template<typename Pre, typename Post>
    friend void visit(Edge, Pre, Post);

    thread_local static RegionPointer local_region;

    // TODO: Not concurrency safe
    inline static size_t count{0};
    // TODO: Not concurrency safe
    inline static std::set<DynObject*> all_objects{};

    size_t rc{1};
    RegionPointer region{nullptr};
    DynObject* prototype{nullptr};

    std::map<std::string, DynObject*> fields{};

    static Region* get_region(DynObject* obj)
    {
      if ((obj == nullptr) || obj->is_immutable())
        return nullptr;
      return obj->region.get_ptr();
    }

    bool is_local_object()
    {
      return region.get_ptr() == get_local_region();
    }

    static void remove_region_reference(Region* src, Region* target)
    {
      if (src == target)
      {
        std::cout << "Same region, no need to do anything" << std::endl;
        return;
      }

      // Handle immutable case
      if (target == nullptr)
        return;

      if (src == get_local_region())
      {
        Region::dec_lrc(target);
        return;
      }

      if (src)
      {
        assert(target->parent == src);
        Region::dec_prc(target);
      }
      return;
    }

    static void add_region_reference(Region* src_region, DynObject* target)
    {
      if (target->is_immutable())
        return;

      auto target_region = get_region(target);
      if (src_region == target_region)
        return;

      if (src_region == get_local_region())
      {
        Region::inc_lrc(target_region);
        return;
      }

      if (target_region == get_local_region())
      {
        target->add_to_region(src_region);
        return;
      }

      Region::set_parent(target_region, src_region);
    }

    size_t change_rc(signed delta)
    {
      std::cout << "Change RC: " << get_name() << " " << rc << " + " << delta
                << std::endl;
      if (!is_immutable())
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

    void add_to_region(Region* r)
    {
      size_t internal_references{0};
      size_t rc_of_added_objects{0};
      visit(this, [&](Edge e) {
        auto obj = e.target;
        if (obj == nullptr || obj->is_immutable())
          return false;

        if (obj->is_local_object())
        {
          std::cout << "Adding object to region: " << obj->get_name()
                    << " rc = " << obj->rc << std::endl;
          rc_of_added_objects += obj->rc;
          internal_references++;
          obj->region = {r};
          get_local_region()->objects.erase(obj);
          r->objects.insert(obj);
          return true;
        }

        auto obj_region = get_region(obj);
        if (obj_region == r)
        {
          std::cout << "Adding internal reference to object: "
                    << obj->get_name() << std::endl;
          internal_references++;
          return false;
        }

        Region::set_parent(obj_region, r);
        Region::dec_lrc(obj_region);
        return false;
      });
      r->local_reference_count += rc_of_added_objects - internal_references;

      std::cout << "Added " << rc_of_added_objects - internal_references
                << " to LRC of region" << std::endl;
      std::cout << "Region LRC: " << r->local_reference_count << std::endl;
      std::cout << "Internal references found: " << internal_references
                << std::endl;
    }

  public:
    // prototype is borrowed, the caller does not need to provide an RC.
    DynObject(DynObject* prototype_ = nullptr, bool first_frame = false)
    : prototype(prototype_)
    {
      if (!first_frame)
      {
        count++;
        all_objects.insert(this);
        auto local_region = get_local_region();
        region = local_region;
        local_region->objects.insert(this);
      }

      if (prototype != nullptr)
        prototype->change_rc(1);
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
        ui::error(stream.str());
      }

      auto r = get_region(this);
      if (!is_immutable() && r != nullptr)
        r->objects.erase(this);

      std::cout << "Deallocate: " << get_name() << std::endl;
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

    void freeze()
    {
      // TODO SCC algorithm
      visit(this, [](Edge e) {
        auto obj = e.target;
        if (!obj || obj->is_immutable())
          return false;

        auto r = get_region(obj);
        if (r != nullptr)
        {
          get_region(obj)->objects.erase(obj);
        }
        obj->region.set_tag(ImmutableTag);

        return !obj->is_cown();
      });
    }

    bool is_immutable()
    {
      return region.get_tag() == ImmutableTag;
    }

    virtual bool is_cown()
    {
      return false;
    }
    virtual bool is_cown_acquired()
    {
      ui::error("is_cown_acquired() should only be called on cowns");
      return false;
    }
    bool is_opaque()
    {
      return this->is_cown() && !this->is_cown_acquired();
    }

    [[nodiscard]] DynObject* get(std::string name)
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
      return nullptr;
    }

    [[nodiscard]] DynObject* set(std::string name, DynObject* value)
    {
      if (is_immutable() && this->is_cown())
      {
        ui::error("Cannot mutate immutable object");
      }
      DynObject* old = fields[name];
      fields[name] = value;
      return old;
    }

    // The caller must provide an rc for value.
    [[nodiscard]] DynObject* set_prototype(DynObject* value)
    {
      // No need to check for a cown, since cowns already have a set prototype
      if (is_immutable())
      {
        ui::error("Cannot mutate immutable object");
      }
      DynObject* old = prototype;
      prototype = value;
      return old;
    }

    DynObject* get_prototype()
    {
      return prototype;
    }

    static void add_reference(DynObject* src, DynObject* target)
    {
      if (target == nullptr)
        return;

      target->change_rc(1);

      auto src_region = get_region(src);
      add_region_reference(src_region, target);
    }

    static void
    remove_reference(DynObject* src_initial, DynObject* old_dst_initial)
    {
      visit(
        {src_initial, "", old_dst_initial},
        [&](Edge e) {
          if (e.target == nullptr)
            return false;

          std::cout << "Remove reference from: " << e.src->get_name() << " to "
                    << e.target->get_name() << std::endl;
          bool result = e.target->change_rc(-1) == 0;

          remove_region_reference(get_region(e.src), get_region(e.target));
          return result;
        },
        [&](DynObject* obj) { delete obj; });

      Region::collect();
    }

    static void
    move_reference(DynObject* src, DynObject* dst, DynObject* target)
    {
      // An immutable cown can't be moved to another region
      if (target == nullptr || target->is_immutable())
        return;

      auto src_region = get_region(src);
      auto dst_region = get_region(dst);

      if (src_region == dst_region)
        return;

      auto target_region = get_region(target);

      add_region_reference(dst_region, target);
      // Note that target_region and get_region(target) are not necessarily the
      // same.
      remove_region_reference(src_region, target_region);
    }

    void create_region()
    {
      Region* r = new Region();
      add_to_region(r);
      // Add root reference as external.
      r->local_reference_count++;
    }

    static size_t get_count()
    {
      return count;
    }

    static Region* get_local_region()
    {
      return local_region;
    }

    static std::set<DynObject*> get_objects()
    {
      return all_objects;
    }
  };

  inline void destruct(DynObject* obj)
  {
    // Called from the region destructor.
    // Remove all references to other objects.
    // If in the same region, then just remove the RC, but don't try to collect
    // as the whole region is being torndown including any potential cycles.
    auto same_region = [](DynObject* src, DynObject* target) {
      return DynObject::get_region(src) == DynObject::get_region(target);
    };
    for (auto& [key, field] : obj->fields)
    {
      if (field == nullptr)
        continue;
      if (same_region(obj, field))
      {
        // Same region just remove the rc, but don't try to collect.
        field->change_rc(-1);
        continue;
      }

      auto old_value = obj->set(key, nullptr);
      remove_reference(obj, old_value);
    }

    if (same_region(obj, obj->prototype))
    {
      obj->prototype->change_rc(-1);
    }
    else
    {
      auto old_value = obj->set_prototype(nullptr);
      remove_reference(obj, old_value);
    }
  }

  inline void dealloc(DynObject* obj)
  {
    // Called from the region destructor.
    // So remove from region if in one.
    // This ensures we don't try to remove it from the set that is being
    // iterated.
    obj->region = nullptr;
    delete obj;
  }

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

      DynObject* next = obj_ptr->get(key);
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

} // namespace rt::objects
