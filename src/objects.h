#pragma once

#include <atomic>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "nop.h"
#include "region.h"
#include "tagged_pointer.h"

namespace objects {
constexpr uintptr_t ImmutableTag{1};

class DynObject;

struct Edge {
  DynObject *src;
  std::string key;
  DynObject *dst;
};

// Representation of objects
class DynObject {
  friend class Reference;
  friend void mermaid(std::vector<Edge> &roots);
  friend void destruct(DynObject *obj);
  friend void dealloc(DynObject *obj);

  // Represents the region of specific object. Uses small pointers to
  // encode special regions.
  using RegionPointer = utils::TaggedPointer<Region>;

  using Nop = utils::Nop<DynObject *>;

  inline static size_t count{0};

  size_t rc{1};
  RegionPointer region{nullptr};
  std::map<std::string, DynObject *> fields{};
  std::string name{};

  bool is_immutable() { return region.get_tag() == ImmutableTag; }

  static Region *get_region(DynObject *obj) {
    if ((obj == nullptr) || obj->is_immutable())
      return nullptr;
    return obj->region.get_ptr();
  }

  bool is_local_object() { return region.get_ptr() == nullptr; }

  template <typename Pre, typename Post = Nop>
  void visit(Pre pre, Post post = {}) {
    visit(Edge{frame(), "", this}, pre, post);
  }

  template <typename Pre, typename Post = Nop>
  static void visit(Edge e, Pre pre, Post post = {}) {
    if (!pre(e))
      return;

    constexpr bool HasPost = !std::is_same_v<Post, Nop>;
    constexpr uintptr_t POST{1};

    std::vector<std::pair<utils::TaggedPointer<DynObject>, std::string>> stack;

    auto visit_object = [&](DynObject *obj) {
      if (obj == nullptr)
        return;
      if constexpr (HasPost)
        stack.push_back({{obj, POST}, ""});
      for (auto &[key, field] : obj->fields)
        stack.push_back({obj, key});
    };

    visit_object(e.dst);

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

  static void remove_reference(DynObject *src, DynObject *old_dst) {
    visit(
        {src, "", old_dst},
        [&](Edge e) {
          if (e.dst == nullptr)
            return false;

          auto old_dst_region = get_region(e.dst);
          auto src_region = get_region(e.src);
          bool result = e.dst->change_rc(-1) == 0;

          if (old_dst_region == src_region)
            return result;

          if (src_region == nullptr) {
            Region::dec_lrc(old_dst_region);
            return result;
          }

          // Handle immutable case
          if (old_dst_region == nullptr)
            return result;

          std::cout << "Removing parent reference from region: "
                    << old_dst->name << std::endl;
          Region::dec_prc(old_dst_region);
          return result;
        },
        [&](DynObject *obj) { delete obj; });

    Region::collect();
  }

  static void add_reference(DynObject *src, DynObject *new_dst) {
    if ((new_dst == nullptr) || new_dst->is_immutable())
      return;

    auto src_region = get_region(src);
    auto new_dst_region = get_region(new_dst);
    if (src_region == new_dst_region)
      return;

    if (src_region != nullptr) {
      new_dst->add_to_region(src_region);
      return;
    }

    new_dst_region->local_reference_count++;
  }

  // Call after the update.
  static void update_notification(DynObject *src, DynObject *old_dst,
                                  DynObject *new_dst) {
    add_reference(src, new_dst);
    remove_reference(src, old_dst);
  }

  size_t change_rc(size_t delta) {
    std::cout << "Change RC: " << name << " " << rc << " + " << (ssize_t)delta
              << std::endl;
    if (!is_immutable()) {
      rc += delta;
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
    visit([&](Edge e) {
      auto obj = e.dst;
      if (obj == nullptr || obj->is_immutable())
        return false;

      if (obj->is_local_object()) {
        std::cout << "Adding object to region: " << obj->name
                  << " rc = " << obj->rc << std::endl;
        rc_of_added_objects += obj->rc;
        internal_references++;
        obj->region = {r};
        r->objects.insert(obj);
        return true;
      }

      auto obj_region = get_region(obj);
      if (obj_region == r) {
        std::cout << "Adding internal reference to object: " << obj->name
                  << std::endl;
        internal_references++;
        return false;
      }

      Region::set_parent(obj_region, r);
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
  DynObject(std::string name, bool global = false) : name(name) {
    count++;
    std::cout << "Allocate: " << name << std::endl;
  }

  ~DynObject() {
    count--;
    if (change_rc(0) != 0) {
      if (this != frame())
        error("Object still has references");
    }

    auto r = get_region(this);
    if (r != nullptr)
      r->objects.erase(this);
    std::cout << "Deallocate: " << name << std::endl;
  }

  // Place holder for the frame object.  Used in various places if we don't have
  // an entry point.
  inline static DynObject *frame() {
    static DynObject frame{"frame"};
    return &frame;
  }

  void inc_rc() {
    change_rc(1);
    add_reference(nullptr, this);
  }

  void dec_rc() { remove_reference(nullptr, this); }

  void freeze() {
    // TODO SCC algorithm
    visit([](Edge e) {
      auto obj = e.dst;
      if (obj->is_immutable())
        return false;

      // TODO remove from region if in one.
      obj->region.set_tag(ImmutableTag);
      return true;
    });
  }

  DynObject *get(std::string name) { return fields[name]; }

  void set(std::string name, DynObject *value) {
    if (is_immutable()) {
      error("Cannot mutate immutable object");
      return;
    }
    DynObject *old = fields[name];
    fields[name] = value;
    update_notification(this, old, value);
  }

  void create_region() {
    Region *r = new Region();
    add_to_region(r);
    // Add root reference as external.
    r->local_reference_count++;
  }

  static size_t get_count() { return count; }
};

void destruct(DynObject *obj) {
  for (auto &[key, field] : obj->fields) {
    if (field == nullptr)
      continue;
    auto src_region = DynObject::get_region(obj);
    auto dst_region = DynObject::get_region(field);
    if (src_region == dst_region) {
      // Same region just remove the rc, but don't try to collect.
      field->change_rc(-1);
      continue;
    }

    obj->set(key, nullptr);
  }
}

void dealloc(DynObject *obj) {
  // Called from the region destructor.
  // So remove from region if in one.
  // This ensures we don't try to remove it from the set that is being iterated.
  obj->region = nullptr;
  delete obj;
}

} // namespace objects