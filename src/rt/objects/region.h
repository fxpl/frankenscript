#pragma once

#include "../../utils/tagged_pointer.h"
#include "../ui.h"

#include <cassert>
#include <set>

namespace rt::objects
{
  class DynObject;
  struct Region;

  void add_to_region(Region* r, DynObject* target);
  void remove_region_reference(Region* src, Region* target);
  void add_region_reference(Region* src_region, DynObject* target);
  void add_reference(DynObject* src, DynObject* target);
  void remove_reference(DynObject* src_initial, DynObject* old_dst_initial);
  void move_reference(DynObject* src, DynObject* dst, DynObject* target);
  void create_region(DynObject* object);
  void destruct(DynObject* obj);
  void dealloc(DynObject* obj);

  // Represents the region of objects
  struct Region
  {
    static inline thread_local std::vector<Region*> to_collect{};
    // The local reference count is the number of references to objects in the
    // region from local region. Using non-zero LRC for subregions ensures we
    // cannot send a region if a subregion has references into it.  Using zero
    // and non-zero means we reduce the number of updates.
    size_t local_reference_count{0};

    // For nested regions, this points at the owning region.
    // This guarantees that the regions for trees.
    Region* parent{nullptr};

    // The number of direct subregions, whose LRC is non-zero
    size_t sub_region_reference_count{0};

    // The objects in this region.
    // TODO: make this more efficient.
    std::set<DynObject*> objects{};

    ~Region()
    {
      std::cout << "Destroying region: " << this << std::endl;
    }

    size_t combined_lrc()
    {
      return local_reference_count + sub_region_reference_count;
    }

    static void action(Region* r)
    {
      if ((r->local_reference_count == 0) && (r->parent == nullptr))
      {
        // TODO, this can be hooked to perform delayed operations like send.
        //  Needs to check for sub_region_reference_count for send, but not
        //  deallocate.

        to_collect.push_back(r);
        std::cout << "Collecting region: " << r << std::endl;
      }
    }

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

    static void set_parent(Region* r, Region* p)
    {
      assert(r->local_reference_count != 0);

      // Check if already parented to another region.
      if (r->parent != nullptr)
        ui::error(
          "Region already has a parent: Creating region DAG not supported!");

      // Prevent creating a cycle
      auto ancestors = p->parent;
      while (ancestors != nullptr)
      {
        if (ancestors == r)
          ui::error("Cycle created in region hierarchy");
        ancestors = ancestors->parent;
      }

      // Set the parent and increment the parent reference count.
      r->parent = p;

      // If the sub-region has local references, then we need the parent to have
      // a local reference to.
      if (r->local_reference_count == 0)
        return;

      inc_sbrc(r);
    }

    void terminate_region()
    {
      to_collect.push_back(this);
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
        auto r = to_collect.back();
        to_collect.pop_back();
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
