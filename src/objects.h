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
#include "tagged_pointer.h"

namespace objects {
constexpr uintptr_t ImmutableTag{1};

// TODO: Make this nicer
std::string mermaid_path{};
void set_output(std::string path) { mermaid_path = path; }

[[noreturn]] static void error(const std::string &msg) {
  std::cerr << "Error: " << msg << std::endl;
  abort();
}

class DynObject;

struct Edge {
  DynObject *src;
  std::string key;
  DynObject *dst;
};

// Representation of objects
class DynObject {
  friend class Reference;

  // Represents the region of objects
  struct Region {
    size_t local_reference_count{0};
    Region *parent{nullptr};
    size_t parent_reference_count{0};
    std::set<Region *> children{};

    static void dec_lrc(Region *r) {
      // Edge triggered LRC for parent.
      while (true) {
        r->local_reference_count--;
        if (r->local_reference_count != 0)
          break;
        if (r->parent == nullptr)
          break;
        r = r->parent;
      }
    }

    static void inc_lrc(Region *r) {
      // Edge triggered LRC for parent.
      while (true) {
        r->local_reference_count++;
        if (r->local_reference_count != 1)
          break;
        if (r->parent == nullptr)
          break;
        r = r->parent;
      }
    }

    static void dec_prc(Region *r) {
      r->parent_reference_count--;
      if (r->parent_reference_count != 0)
        return;

      dec_lrc(r->parent);

      r->parent = nullptr;
    }

    static void set_parent(Region *r, Region *p) {
      // This edge becomes a parent edge, so remove from local reference count?
      r->local_reference_count--;
      r->parent_reference_count++;

      // Check if already parented, if so increment the parent reference count.
      if (r->parent == p) {
        return;
      }

      // Check if already parented to another region.
      if (r->parent != nullptr)
        error("Region already has a parent");

      // Set the parent and increment the parent reference count.
      r->parent = p;
      assert(r->parent_reference_count == 1);

      // If the region has local references, then we need the parent to have a
      // local reference to.
      if (r->local_reference_count == 0)
        return;

      inc_lrc(r->parent);
    }
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

  static Region *get_region(DynObject *obj) {
    if ((obj == nullptr) || obj->is_immutable())
      return nullptr;
    return obj->region.get_ptr();
  }

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

          std::cout << "Removing parent reference from region: "
                    << old_dst->name << std::endl;
          Region::dec_prc(old_dst_region);
          return result;
        },
        [&](DynObject *obj) { delete obj; });
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

      if (obj->region.get_ptr() == 0) {
        std::cout << "Adding object to region: " << obj->name
                  << " rc = " << obj->rc << std::endl;
        rc_of_added_objects += obj->rc;
        internal_references++;
        obj->region = {r};
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
  DynObject(std::string name) : name(name) {
    count++;
    std::cout << "Allocate: " << name << std::endl;
  }

  ~DynObject() {
    count--;
    std::cout << "Deallocate: " << name << std::endl;
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

  static void mermaid(std::vector<Edge> &roots) {
    auto out = std::ofstream(mermaid_path);
    std::map<DynObject *, std::size_t> visited;
    std::map<Region *, std::vector<std::size_t>> region_strings;
    std::vector<std::size_t> immutable_objects;
    size_t id = 2;
    visited[frame()] = 0;
    visited[nullptr] = 1;
    immutable_objects.push_back(1);
    out << "```mermaid" << std::endl;
    out << "graph TD" << std::endl;
    out << "id0[frame]" << std::endl;
    out << "id1[null]" << std::endl;
    for (auto &root : roots) {
      visit({root.src, root.key, root.dst},
            [&](Edge e) {
              DynObject *dst = e.dst;
              std::string key = e.key;
              DynObject *src = e.src;
              out << "  id" << visited[src] << " -->|" << key << "| ";
              size_t curr_id;
              if (visited.find(dst) != visited.end()) {
                out << "id" << visited[dst] << std::endl;
                return false;
              }

              curr_id = id++;
              visited[dst] = curr_id;
              out << "id" << curr_id << "[ " << dst->name << " - " << dst->rc
                  << " ]" << std::endl;

              auto region = get_region(dst);
              if (region != nullptr) {
                region_strings[region].push_back(curr_id);
              }

              if (dst->is_immutable()) {
                immutable_objects.push_back(curr_id);
              }
              return true;
            },
            {});
    }

    for (auto [region, objects] : region_strings) {
      if (region->parent == nullptr)
        continue;
      out << "  region" << region << "  -->|parent| region" << region->parent
          << std::endl; 
    }

    for (auto [region, objects] : region_strings) {
      out << "subgraph  " << std::endl;
      out << "  region" << region << "[meta\\nlrc=" << region->local_reference_count << "\\nprc=" << region->parent_reference_count << "]"
          << std::endl;
      for (auto obj : objects) {
        out << "  id" << obj << std::endl;
      }
      out << "end" << std::endl;
    }

    out << "subgraph Immutable" << std::endl;
    for (auto obj : immutable_objects) {
      out << "  id" << obj << std::endl;
    }
    out << "end" << std::endl;
    out << "```" << std::endl;
  }

  void mermaid() {
    std::vector<Edge> roots;
    roots.push_back({frame(), "root", this});
    mermaid(roots);
  }

  void create_region() {
    Region *r = new Region();
    add_to_region(r);
    // Add root reference as external.
    r->local_reference_count++;
  }

  static DynObject *frame() {
    static DynObject frame{"frame"};
    return &frame;
  }
};
} // namespace objects