#pragma once

#include <set>
#include <cassert>

#include "output.h"


namespace objects {
  // Represents the region of objects
  struct Region {
    // The local reference count is sum of the following:
    //   * the number of references to objects in the region from local region.
    //   * the number of direct subregions, whose LRC is non-zero
    // Using non-zero LRC for subregions ensures we cannot send a region if a subregion
    // has references into it.  Using zero and non-zero means we reduce the number of
    // updates.
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
    std::set<Region *> object{};

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

      // This is the last parent reference, so region no longer has a parent.
      // If it has internal references, then we need to decrement the parents
      // local reference count.
      if (r->local_reference_count != 0)
        dec_lrc(r->parent);

      // Unset parent pointer.
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
} // namespace objects