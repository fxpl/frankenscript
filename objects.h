#pragma once

#include <atomic>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "nop.h"
#include "tagged_pointer.h"

namespace objects {
constexpr uintptr_t ImmutableTag{1};

// Representation of objects
class DynObject {
  friend class Reference;

  // Represents the region of objects
  struct Region {
    size_t local_reference_count{0};
    Region *parent{nullptr};
    std::set<Region *> children{};
  };

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

  static Region* get_region(DynObject* obj) {
    if ((obj == nullptr) || obj->is_immutable())
      return nullptr;
    return obj->region.get_ptr();
  }

  template <typename Pre, typename Post = Nop>
  void visit(Pre pre, Post post = {}, DynObject* parent = nullptr) {
    if (!pre(this, "root", parent))
      return;

    constexpr bool HasPost = !std::is_same_v<Post, Nop>;
    constexpr uintptr_t POST{1};

    std::vector<std::pair<utils::TaggedPointer<DynObject>, std::string>> stack;

    auto visit_object = [&](DynObject* obj) {
      if (obj == nullptr)
        return;
      if constexpr (HasPost)
        stack.push_back({{obj, POST}, ""});
      for (auto &[key, field] : obj->fields)
        stack.push_back({obj, key});
    };

    visit_object(this);

    while (!stack.empty()) {
      auto [obj, key] = stack.back();
      auto obj_ptr = obj.get_ptr();
      stack.pop_back();

      if (HasPost && obj.get_tag() == POST) {
        post(obj_ptr);
        continue;
      }

      DynObject* next = obj_ptr->get(key);
      if (pre(next, key, obj_ptr)) {
        visit_object(next);
      }
    }
  }

  static void remove_reference(DynObject *src, DynObject *old_dst) {
    old_dst->visit([&](DynObject *old_dst, std::string, DynObject* src) {
      if (old_dst == nullptr)
        return false;

      auto old_dst_region = get_region(old_dst);
      auto src_region = get_region(src);
      bool result = old_dst->change_rc(-1) == 0;

      if (old_dst_region != nullptr && src_region == nullptr)
      {
        old_dst_region->local_reference_count--;
        std::cout << "Removed reference from region: " << old_dst->name << std::endl;
        std::cout << "Region LRC: " << old_dst_region->local_reference_count << std::endl;
      }

      return result;
    },
    [&](DynObject *obj) { delete obj; },
    src);
  }

  static void add_reference(DynObject* src, DynObject* new_dst)
  {
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
    visit([&](DynObject *obj, std::string, DynObject*) {
      if (obj->is_immutable())
        return false;

      if (obj->region.get_ptr() == 0) {
        std::cout << "Adding object to region: " << obj->name << " rc = " << obj->rc
                  << std::endl;
        rc_of_added_objects += obj->rc;
        internal_references++;
        obj->region = {r};
        return true;
      }

      if (obj->region.get_ptr() == r) {
        std::cout << "Adding internal reference to object: " << obj->name
                  << std::endl;
        internal_references++;
        return false;
      }

      // TODO: Handle nested regions correctly!
      std::cout << "Error: Object already in a region" << std::endl;
      abort();
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
  DynObject(std::string name) : name(name) {
    count++;
    std::cout << "Allocate: " << name << std::endl;
  }

  ~DynObject() {
    count--;
    std::cout << "Deallocate: " << name << std::endl;
  }

  void inc_rc() { change_rc(1); add_reference(nullptr, this);}

  void dec_rc() { remove_reference(nullptr, this); }

  void freeze() {
    // TODO SCC algorithm
    visit([](DynObject *obj, std::string, DynObject*) {
      if (obj->region.get_tag() == ImmutableTag)
        return false;

      obj->region.set_tag(ImmutableTag);
      return true;
    });
  }

  DynObject *get(std::string name) { return fields[name]; }

  void set(std::string name, DynObject *value) {
    if (is_immutable())
    {
      std::cout << "Cannot mutate immutable object" << std::endl;
      abort();
      return;
    }
    DynObject *old = fields[name];
    fields[name] = value;
    update_notification(this, old, value);
  }

  void print() {
    std::map<DynObject *, size_t> visited;
    size_t id = 1;
    size_t i = 0;
    visit(
        [&](DynObject *obj, std::string key, DynObject*) {
          std::fill_n(std::ostream_iterator<char>(std::cout), i, ' ');
          std::cout << key;

          size_t curr_id;
          if (visited.find(obj) != visited.end()) {
            std::cout << " -> " << id - visited[obj] << std::endl;
            return false;
          }

          curr_id = id++;
          visited[obj] = curr_id;
          std::cout << (/*obj==nullptr? "null" : */(obj->is_immutable() ? "|" : ":"))
                    << std::endl;
          i += 2;
          return true;
        },
        [&](DynObject *obj) { i -= 2; });
  }

  void create_region() {
    Region *r = new Region();
    add_to_region(r);
    // Add root reference as external.
    r->local_reference_count++;
  }
};
} // namespace objects