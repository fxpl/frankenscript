#pragma once

#include <cassert>
#include <set>

#include "output.h"

namespace objects {
class DynObject;

void destruct(DynObject *obj);
void dealloc(DynObject *obj);

// Represents the region of objects
struct Region {

  static inline thread_local std::vector<Region *> to_collect{};
  // The local reference count is sum of the following:
  //   * the number of references to objects in the region from local region.
  //   * the number of direct subregions, whose LRC is non-zero
  // Using non-zero LRC for subregions ensures we cannot send a region if a
  // subregion has references into it.  Using zero and non-zero means we reduce
  // the number of updates.
  size_t local_reference_count{0};

  // For nested regions, this points at the owning region.
  // This guarantees that the regions for trees.
  Region *parent{nullptr};

  // The parent reference count is the number of references to the region from
  // the parent region.  In classic Verona we considered this as 0 or 1, but by
  // tracking dynamically we can allow multiple references.
  size_t parent_reference_count{0};

  // The objects in this region.
  // TODO: make this more efficient.
  std::set<DynObject *> objects{};

  static void dec_lrc(Region *r) {
    // Edge triggered LRC for parent.
    while (true) {
      assert(r->local_reference_count != 0);
      r->local_reference_count--;
      if (r->local_reference_count != 0)
        return;
      if (r->parent == nullptr)
        break;
      r = r->parent;
    }
    if (r->local_reference_count == 0) {
      assert(r->parent == nullptr);
      // TODO, this can be hooked to perform delayed operations like send.

      to_collect.push_back(r);
      std::cout << "Collecting region: " << r << std::endl;
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
    std::cout << "Dropping parent reference: " << r << std::endl;
    assert(r->parent_reference_count != 0);
    r->parent_reference_count--;
    if (r->parent_reference_count != 0)
      return;

    // This is the last parent reference, so region no longer has a parent.
    // If it has internal references, then we need to decrement the parents
    // local reference count.
    if (r->local_reference_count != 0)
      dec_lrc(r->parent);
    else {
      std::cout << "Collecting region: " << r << std::endl;
      to_collect.push_back(r);
    }
    // Unset parent pointer.
    r->parent = nullptr;
  }

  static void set_parent(Region *r, Region *p) {
    assert(r->local_reference_count != 0);
    // This edge becomes a parent edge, so remove from local reference count?
    //    r->local_reference_count--; // TODO check with examples
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

  void terminate_region() {
    to_collect.push_back(this);
    collect();
  }

  std::vector<DynObject *> get_objects() {
    std::vector<DynObject *> result;
    for (auto o : objects)
      result.push_back(o);
    return result;
  }

  static void collect() {
    // Reentrancy guard.
    static thread_local bool collecting = false;
    if (collecting)
      return;

    collecting = true;

    std::cout << "Starting collection" << std::endl;
    while (!to_collect.empty()) {
      auto r = to_collect.back();
      to_collect.pop_back();
      // Note destruct could re-enter here, ensure we don't hold onto a pointer
      // into to_collect.
      for (auto o : r->objects)
        destruct(o);
      for (auto o : r->objects)
        dealloc(o);
      r->objects.clear();

      delete r;
    }
    std::cout << "Finished collection" << std::endl;

    collecting = false;
  }
};
} // namespace objects