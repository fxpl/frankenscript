#pragma once

#include "../../utils/tagged_pointer.h"
#include "../ui.h"

#include <cassert>
#include <set>

namespace rt::objects
{
  class DynObject;
  struct Region;

  void add_to_region(Region* r, DynObject* target, DynObject* source = nullptr);
  void remove_region_reference(Region* src, Region* target);
  void add_reference(DynObject* src, DynObject* target);
  void remove_reference(DynObject* src_initial, DynObject* old_dst_initial);
  void move_reference(DynObject* src, DynObject* dst, DynObject* target);
  void clean_lrcs();
  DynObject* create_region();
  void destruct(DynObject* obj);
  void dealloc(DynObject* obj);
  void merge_regions(DynObject* src, DynObject* sink);
  void dissolve_region(DynObject* bridge);

  // Represents the region of objects
  struct Region
  {
    static inline thread_local std::set<Region*> to_collect{};
    // This keeps track of all dirty regions. When walking to local region
    // to correct the LRC it can be done for all dirty regions at once
    static inline thread_local std::set<Region*> dirty_regions{};

    /// Indicates if implicit freezing is enabled
    static inline bool pragma_implicit_freezing = false;

    // The local reference count is the number of references to objects in the
    // region from local region. Using non-zero LRC for subregions ensures we
    // cannot send a region if a subregion has references into it.  Using zero
    // and non-zero means we reduce the number of updates.
    size_t local_reference_count{0};

    // The LRC might be dirty if nodes from this region has been frozen and the
    // LRC hasn't been validated after the move. This flag can be stored as
    // part of the LRC or other pointers in this struct.
    bool is_lrc_dirty = false;

    // For nested regions, this points at the owning region.
    // This guarantees that the regions for trees.
    Region* parent{nullptr};

    // The number of direct subregions, whose LRC is non-zero
    size_t sub_region_reference_count{0};

    // The objects in this region.
    // TODO: make this more efficient.
    std::set<DynObject*> objects{};

    // Entry point object for the region.
    DynObject* bridge{nullptr};

    // Bridge children of the region
    std::set<DynObject*> direct_subregions{};

    ~Region()
    {
      std::cout << "Destroying region: " << this << " with bridge "
                << this->bridge << std::endl;
    }

    size_t combined_lrc()
    {
      return local_reference_count + sub_region_reference_count;
    }

    static void action(Region*);

    static void dec_lrc(Region* r)
    {
      assert(r->local_reference_count != 0);
      r->local_reference_count--;
      // Edge triggered LRC for parent.
      if (r->combined_lrc() == 0)
        dec_sbrc(r);
      else
        action(r);
    }

    // Decrements sbrc for ancestors of 'r'
    static void dec_sbrc(Region* r)
    {
      while (r->parent != nullptr)
      {
        r = r->parent;
        r->sub_region_reference_count--;
        if (r->combined_lrc() != 0)
          break;
      }
      action(r);
    }

    static void inc_lrc(Region* r)
    {
      r->local_reference_count++;
      // Edge triggered LRC for parent.
      if (r->combined_lrc() == 1)
        inc_sbrc(r);
    }

    static void inc_sbrc(Region* r)
    {
      while (r->parent != nullptr)
      {
        r = r->parent;
        r->sub_region_reference_count++;
        if (r->combined_lrc() != 1)
          break;
      }
    }

    static bool is_ancestor(Region* child, Region* ancestor)
    {
      while (child)
      {
        if (child->parent == ancestor)
        {
          return true;
        }
        child = child->parent;
      }
      return false;
    }

    static void set_parent(Region* r, Region* p)
    {
      assert(r->local_reference_count != 0);

      // Check if already parented to another region.
      if (r->parent != nullptr)
      {
        ui::error(
          "Region already has a parent: Creating region DAG not supported!",
          r->bridge);
      }

      // Prevent creating a cycle
      if (is_ancestor(p, r))
      {
        ui::error("Cycle created in region hierarchy", r->bridge);
      }

      p->direct_subregions.insert(r->bridge);
      // Set the parent and increment the parent reference count.
      r->parent = p;

      // If the sub-region has local references, then we need the parent to have
      // a local reference to.
      if (r->local_reference_count == 0)
        return;

      inc_sbrc(r);
    }

    /// Cleans the LRC's and forces the region to close, by setting all local
    /// references to `None`
    static void clean_lrcs_and_close(Region* reg = nullptr);
    static void clean_lrcs();

    bool is_closed()
    {
      return this->combined_lrc() == 0;
    }

    /// Forces the region to be closed, by setting all references from the local
    /// reagion to `None`
    void close();
    /// Checks if the region can be closed, returns false if it remain open.
    bool try_close();

    void mark_dirty()
    {
      is_lrc_dirty = true;
      dirty_regions.insert(this);
    }

    void terminate_region()
    {
      to_collect.insert(this);
      collect();
    }

    std::vector<DynObject*> get_objects()
    {
      std::vector<DynObject*> result;
      for (auto o : objects)
        result.push_back(o);
      return result;
    }

    static void collect()
    {
      // Reentrancy guard.
      static thread_local bool collecting = false;
      if (collecting)
        return;

      collecting = true;

      std::cout << "Starting collection" << std::endl;
      while (!to_collect.empty())
      {
        auto r = *to_collect.begin();
        dirty_regions.erase(r);
        to_collect.erase(r);
        // Note destruct could re-enter here, ensure we don't hold onto a
        // pointer into to_collect.
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

  // Represents the region of specific object. Uses small pointers to
  // encode special regions.
  using RegionPointer = utils::TaggedPointer<Region>;

  inline Region immutable_region_impl;
  inline constexpr Region* immutable_region{&immutable_region_impl};

  inline Region cown_region_impl;
  inline constexpr Region* cown_region{&cown_region_impl};
} // namespace rt::objects
