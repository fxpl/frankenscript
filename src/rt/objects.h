#pragma once

#include <atomic>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "nop.h"
#include "region.h"
#include "rt.h"
#include "tagged_pointer.h"

namespace objects {
constexpr uintptr_t ImmutableTag{1};
constexpr std::string PrototypeField{"__proto__"};

namespace value {
  /// @brief This class represents the value of an object when it's used in an
  /// expression. For example, let's consider `k`
  ///
  /// ```
  /// a = {}
  /// k = "key"
  /// a[k] = {}
  /// ```
  ///
  /// In this example, `k` is a string literal. The access `a[k]` will retrieve the
  /// string value from `k` and use it as a key in `a`.
  ///
  /// Note that dictionaries are still modeled in `DynObject`. This is analogous
  /// to Python. While string values and integers are used as values in
  /// expressions, they still have fields that store information and methods.
  class Value {
  public:
    virtual ~Value() {}

    virtual std::string* str_value() { return nullptr; }
    std::string* expect_str_value() {
      auto value = this->str_value();
      assert(value && "expected a string, but received null");
      return value;
    }

    virtual DynObject* iter_next() { return nullptr; }

    /// @brief The string representation of this value to 
    virtual std::string display_str() = 0;
  };

  /// @brief A string literal.
  class StrValue : public Value {
    std::string value;

  public:
    StrValue(std::string value_): value(value_) {}

    std::string* str_value() { return &this->value; }

    std::string display_str() {
      std::stringstream stream;
      stream << "'" << this->value << "'";
      return stream.str();
    }
  };

  /// @brief This iterates over they keys of a given map
  class KeyIterValue : public Value {
    std::map<std::string, DynObject *>::iterator iter;
    std::map<std::string, DynObject *>::iterator iter_end;

  public:
    KeyIterValue(std::map<std::string, DynObject *> &fields)
      : iter(fields.begin()), iter_end(fields.end()) {}

    virtual DynObject* iter_next() {
      DynObject *obj = nullptr;
      if (this->iter != this->iter_end) {
        obj = make_object(this->iter->first, "");
        this->iter++;
      }

      return obj;
    }

    std::string display_str() {
      return "&lt;iterator&gt;";
    }
  };
}

using NopDO = utils::Nop<DynObject *>;

template <typename Pre, typename Post = NopDO>
inline void visit(Edge e, Pre pre, Post post = {});

template <typename Pre, typename Post = NopDO>
inline void visit(DynObject* start, Pre pre, Post post = {});

// Representation of objects
class DynObject {
  friend class Reference;
  friend DynObject* make_iter(DynObject *obj);
  friend void mermaid(std::vector<Edge> &roots, std::ostream &out);
  friend void destruct(DynObject *obj);
  friend void dealloc(DynObject *obj);
  template <typename Pre, typename Post>
  friend void visit(Edge, Pre, Post);

  // Represents the region of specific object. Uses small pointers to
  // encode special regions.
  using RegionPointer = utils::TaggedPointer<Region>;


  // TODO: Not concurrency safe
  inline static size_t count{0};
  // TODO: Not concurrency safe
  inline static std::set<DynObject *> all_objects{};

  size_t rc{1};
  RegionPointer region{nullptr};
  DynObject* prototype{nullptr};

  std::map<std::string, DynObject *> fields{};
  std::string name{};
  value::Value* value {nullptr};

  static Region *get_region(DynObject *obj) {
    if ((obj == nullptr) || obj->is_immutable())
      return nullptr;
    return obj->region.get_ptr();
  }

  bool is_local_object() { return region.get_ptr() == get_local_region(); }


  static void remove_region_reference(Region *src, Region *target) {
    if (src == target) {
      std::cout << "Same region, no need to do anything" << std::endl;
      return;
    }

    // Handle immutable case
    if (target == nullptr)
      return;

    if (src == get_local_region()) {
      Region::dec_lrc(target);
      return;
    }

    assert(target->parent == src);
    Region::dec_prc(target);
    return;
  }

  static void add_region_reference(Region *src_region, DynObject *target) {
    if (target->is_immutable())
      return;

    auto target_region = get_region(target);
    if (src_region == target_region)
      return;

    if (src_region == get_local_region()) {
      Region::inc_lrc(target_region);
      return;
    }

    if (target_region == get_local_region()) {
      target->add_to_region(src_region);
      return;
    }

    Region::set_parent(target_region, src_region);
  }

  size_t change_rc(size_t delta) {
    std::cout << "Change RC: " << get_name() << " " << rc << " + " << (ssize_t)delta
              << std::endl;
    if (!is_immutable()) {
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

  void add_to_region(Region *r) {
    size_t internal_references{0};
    size_t rc_of_added_objects{0};
    visit(this, [&](Edge e) {
      auto obj = e.target;
      if (obj == nullptr || obj->is_immutable())
        return false;

      if (obj->is_local_object()) {
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
      if (obj_region == r) {
        std::cout << "Adding internal reference to object: " << obj->get_name()
                  << std::endl;
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
  DynObject(std::string name_, value::Value* value_, bool global = false) : name(name_), value(value_) {
    count++;
    all_objects.insert(this);
    if (global) {
      change_rc(-1);
    } else {
      auto local_region = get_local_region();
      region = local_region;
      local_region->objects.insert(this);
    }
    std::cout << "Allocate: " << name << std::endl;
  }

  ~DynObject() {
    count--;
    all_objects.erase(this);
    if (change_rc(0) != 0) {
      if (this != frame())
        error("Object still has references");
    }

    auto r = get_region(this);
    if (!is_immutable() && r != nullptr)
      r->objects.erase(this);

    if (this->value) {
      delete this->value;
      this->value = nullptr;
    }
    std::cout << "Deallocate: " << name << std::endl;
  }

  std::string get_name() {
    if (!name.empty())
      return name;

    std::stringstream stream;
    stream << this;
    return stream.str();
  }

  value::Value* get_value() {
    return this->value;
  }

  // Place holder for the frame object.  Used in various places if we don't have
  // an entry point.
  inline static DynObject *frame() {
    thread_local static DynObject frame{"frame", nullptr, true};
    return &frame;
  }

  void freeze() {
    // TODO SCC algorithm
    visit(this, [](Edge e) {
      auto obj = e.target;
      if (obj->is_immutable())
        return false;

      get_region(obj)->objects.erase(obj);
      obj->region.set_tag(ImmutableTag);
      return true;
    });
  }


  bool is_immutable() { return region.get_tag() == ImmutableTag; }

  [[nodiscard]] DynObject *get(std::string name) {
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

  [[nodiscard]] DynObject *set(std::string name, DynObject *value) {
    if (is_immutable()) {
      error("Cannot mutate immutable object");
    }
    DynObject *old = fields[name];
    fields[name] = value;
    return old;
  }

  [[nodiscard]] DynObject* set_prototype(DynObject* value) {
    if (is_immutable()) {
      error("Cannot mutate immutable object");
    }
    DynObject* old = prototype;
    prototype = value;
    return old;
  }

  static void add_reference(DynObject *src, DynObject *target) {
    if (target == nullptr)
      return;

    target->change_rc(1);

    auto src_region = get_region(src);
    add_region_reference(src_region, target);
  }

  static void remove_reference(DynObject *src_initial,
                               DynObject *old_dst_initial) {
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
        [&](DynObject *obj) { delete obj; });

    Region::collect();
  }

  static void move_reference(DynObject *src, DynObject *dst,
                             DynObject *target) {
    if (target == nullptr || target->is_immutable())
      return;

    auto src_region = get_region(src);
    auto dst_region = get_region(dst);

    if (src_region == dst_region)
      return;

    auto target_region = get_region(target);

    add_region_reference(dst_region, target);
    // Note that target_region and get_region(target) are not necessarily the same.
    remove_region_reference(src_region, target_region);
  }

  void create_region() {
    Region *r = new Region();
    add_to_region(r);
    // Add root reference as external.
    r->local_reference_count++;
  }

  static size_t get_count() { return count; }

  static void set_local_region(Region *r) { frame()->region = {r}; }

  static Region *get_local_region() {
    return DynObject::frame()->region.get_ptr();
  }

  static std::set<DynObject *> get_objects() { return all_objects; }
};

void destruct(DynObject *obj) {
  auto same_region = [](DynObject *src, DynObject *target) {
    return DynObject::get_region(src) == DynObject::get_region(target);
  };
  for (auto &[key, field] : obj->fields) {
    if (field == nullptr)
      continue;
    if (same_region(obj,field)) {
      // Same region just remove the rc, but don't try to collect.
      field->change_rc(-1);
      continue;
    }

    auto old_value = obj->set(key, nullptr);
    remove_reference(obj, old_value);
  }

  if (same_region(obj, obj->prototype)) {
    obj->prototype->change_rc(-1);
  } else {
    auto old_value = obj->set_prototype(nullptr);
    remove_reference(obj, old_value);
  }
}

void dealloc(DynObject *obj) {
  // Called from the region destructor.
  // So remove from region if in one.
  // This ensures we don't try to remove it from the set that is being iterated.
  obj->region = nullptr;
  delete obj;
}

template <typename Pre, typename Post>
inline void visit(Edge e, Pre pre, Post post) {
  if (!pre(e))
    return;

  constexpr bool HasPost = !std::is_same_v<Post, NopDO>;
  constexpr uintptr_t POST{1};

  std::vector<std::pair<utils::TaggedPointer<DynObject>, std::string>> stack;

  auto visit_object = [&](DynObject *obj) {
    if (obj == nullptr)
      return;
    if constexpr (HasPost)
      stack.push_back({{obj, POST}, ""});
    // TODO This will need to depend on the type of object.
    for (auto &[key, field] : obj->fields)
      stack.push_back({obj, key});
    if (obj->prototype != nullptr)
      stack.push_back({obj, PrototypeField});
  };

  visit_object(e.target);

  while (!stack.empty()) {
    auto [obj, key] = stack.back();
    auto obj_ptr = obj.get_ptr();
    stack.pop_back();

    if (HasPost && obj.get_tag() == POST) {
      post(obj_ptr);
      continue;
    }

    DynObject *next = obj_ptr->get(key);
    if (pre({obj_ptr, key, next})) {
      visit_object(next);
    }
  }
}


template <typename Pre, typename Post>
inline void visit(DynObject* start, Pre pre, Post post)
{
  visit(Edge{DynObject::frame(), "", start}, pre, post);
}

} // namespace objects
