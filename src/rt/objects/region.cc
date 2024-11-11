#include "dyn_object.h"
#include "region_object.h"

#include <algorithm>

namespace rt::objects
{
  Region* get_region(DynObject* obj)
  {
    assert(obj != nullptr);
    return obj->region.get_ptr();
  }

  thread_local objects::RegionPointer local_region = new Region();

  Region* get_local_region()
  {
    return local_region;
  }

  void set_local_region(Region* region)
  {
    local_region = region;
  }

  void implicit_freeze(DynObject* target)
  {
    // The `freeze()` call is the only required thing, the rest is just needed
    // for helpful UI output.
    std::set<DynObject*> pre_objects = immutable_region->objects;
    target->freeze();
    std::set<DynObject*> post_objects = immutable_region->objects;

    std::vector<DynObject*> effected_nodes;
    std::set_difference(
      post_objects.begin(),
      post_objects.end(),
      pre_objects.begin(),
      pre_objects.end(),
      std::back_inserter(effected_nodes));

    std::stringstream ss;
    ss << "Internal: Implicit freeze effected " << effected_nodes.size()
       << " node(s) starting from " << target;

    ui::globalUI()->highlight(ss.str(), effected_nodes);
  }

  void add_to_region(Region* r, DynObject* target)
  {
    size_t internal_references{0};
    size_t rc_of_added_objects{0};

    visit(target, [&](Edge e) {
      auto obj = e.target;
      if (obj == nullptr || obj->is_immutable())
        return false;

      if (obj->region.get_ptr() == get_local_region())
      {
        std::cout << "Adding object to region: " << obj->get_name()
                  << " rc = " << obj->get_rc() << std::endl;
        rc_of_added_objects += obj->get_rc();
        internal_references++;
        obj->region = {r};
        get_local_region()->objects.erase(obj);
        r->objects.insert(obj);
        return true;
      }

      auto obj_region = get_region(obj);
      if (obj_region == r)
      {
        std::cout << "Adding internal reference to object: " << obj->get_name()
                  << std::endl;
        internal_references++;
        return false;
      }

      if (obj->get_prototype() != objects::regionPrototypeObject())
      {
        if (Region::pragma_implicit_freezing)
        {
          implicit_freeze(obj);
          return false;
        }
        else
        {
          ui::error(
            "Cannot reference an object from another region",
            {r->bridge, "", obj});
        }
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

  void remove_region_reference(Region* src, Region* target)
  {
    if (src == target)
    {
      std::cout << "Same region, no need to do anything" << std::endl;
      return;
    }

    // Handle immutable case
    if (target == immutable_region)
      return;

    // Handle cown case
    if (target == cown_region)
      return;

    if (src == get_local_region())
    {
      Region::dec_lrc(target);
      return;
    }

    if (src)
    {
      assert(target->parent == src);
      std::cout << "Removing parent reference from region: " << src << " to "
                << target << std::endl;
      if (target->combined_lrc() != 0)
      {
        Region::dec_sbrc(target);
        target->parent = nullptr;
        return;
      }
      target->parent = nullptr;

      Region::to_collect.insert(target);
    }
    return;
  }

  void add_region_reference(Region* src_region, DynObject* target)
  {
    assert(target != nullptr);
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
      add_to_region(src_region, target);
      return;
    }

    auto is_region =
      target->get_prototype() == objects::regionPrototypeObject();
    if (is_region && target_region->parent == nullptr)
    {
      Region::set_parent(target_region, src_region);
      return;
    }

    if (Region::pragma_implicit_freezing)
    {
      implicit_freeze(target);
      return;
    }

    if (is_region)
    {
      ui::error(
        "Cannot reference an object from another region",
        {src_region->bridge, "", target});
    }
    else
    {
      std::stringstream ss;
      ss << "Cannot reference region " << target << " from "
         << src_region->bridge << " since it already has a parent";
      ui::error(ss.str(), {src_region->bridge, "", target});
    }
  }

  void add_reference(DynObject* src, DynObject* target)
  {
    assert(src != nullptr);
    if (target == nullptr)
      return;

    target->change_rc(1);

    auto src_region = get_region(src);
    add_region_reference(src_region, target);
  }

  void remove_reference(DynObject* src_initial, DynObject* old_dst_initial)
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

  void move_reference(DynObject* src, DynObject* dst, DynObject* target)
  {
    assert(src != nullptr);
    assert(dst != nullptr);
    if (target == nullptr || target->is_immutable() || target->is_cown())
    {
      return;
    }

    auto src_region = get_region(src);
    auto dst_region = get_region(dst);
    if (src_region == dst_region)
    {
      return;
    }

    auto target_region = get_region(target);

    add_region_reference(dst_region, target);
    // Note that target_region and get_region(target) are not necessarily the
    // same.
    remove_region_reference(src_region, target_region);
  }

  void Region::clean_lrcs_and_close(Region* to_close_reg)
  {
    if (
      dirty_regions.empty() &&
      (to_close_reg == nullptr || to_close_reg->is_closed()))
    {
      return;
    }

    if (to_close_reg)
    {
      std::cout << "Cleaning LRCs and closing " << to_close_reg << std::endl;
    }
    else
    {
      std::cout << "Cleaning LRCs" << std::endl;
    }

    for (auto r : dirty_regions)
    {
      r->local_reference_count = 0;
    }

    bool continue_visit = true;
    std::set<DynObject*> seen;
    visit(get_local_region(), [&](Edge e) {
      auto src = e.src;
      auto dst = e.target;
      if (!src || !dst)
      {
        return continue_visit;
      }

      auto dst_reg = get_region(dst);
      if (dst_reg == get_local_region())
      {
        // Insert and continue if this was a new value
        return seen.insert(dst).second;
      }

      auto invalidate = dst_reg == to_close_reg;
      invalidate |=
        (to_close_reg && to_close_reg->sub_region_reference_count != 0 &&
         Region::is_ancestor(dst_reg, to_close_reg));
      if (invalidate)
      {
        auto old = src->set(e.key, nullptr);
        assert(old == dst);
        add_reference(src, nullptr);
        remove_reference(src, dst);

        continue_visit &= to_close_reg->is_closed();
        return false;
      }

      if (dirty_regions.contains(dst_reg))
      {
        dst_reg->local_reference_count += 1;
      }

      return false;
    });

    for (auto r : dirty_regions)
    {
      std::cout << "Corrected LRC of " << r << " to "
                << r->local_reference_count << std::endl;
      r->is_lrc_dirty = false;
      if (r->combined_lrc() == 0)
      {
        action(r);
      }
    }
    dirty_regions.clear();

    assert(
      (!to_close_reg || to_close_reg->is_closed()) &&
      "The region should be closed now");
  }

  void Region::clean_lrcs()
  {
    clean_lrcs_and_close(nullptr);
  }

  void Region::close()
  {
    clean_lrcs_and_close(this);
  }

  bool Region::try_close()
  {
    if (is_closed())
    {
      return true;
    }

    if (this->is_lrc_dirty || this->sub_region_reference_count != 0)
    {
      clean_lrcs();
    }

    return is_closed();
  }

  void destruct(DynObject* obj)
  {
    // Called from the region destructor.
    // Remove all references to other objects.
    // If in the same region, then just remove the RC, but don't try to collect
    // as the whole region is being torndown including any potential cycles.
    auto same_region = [](DynObject* src, DynObject* target) {
      assert(src != nullptr);
      assert(target != nullptr);
      return get_region(src) == get_region(target);
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

    if ((obj->prototype != nullptr) && same_region(obj, obj->prototype))
    {
      // TODO When freeze is no longer immortal, this will need to be updated.
      obj->prototype->change_rc(-1);
    }
    else
    {
      auto old_value = obj->set_prototype(nullptr);
      remove_reference(obj, old_value);
    }
  }

  void dealloc(DynObject* obj)
  {
    // Called from the region destructor.
    // So remove from region if in one.
    // This ensures we don't try to remove it from the set that is being
    // iterated.
    obj->region = nullptr;
    delete obj;
  }

  DynObject* create_region()
  {
    Region* r = new Region();
    RegionObject* obj = new RegionObject(r);
    r->bridge = obj;
    r->local_reference_count++;
    std::cout << "Created region " << r << " with bridge " << obj << std::endl;
    return obj;
  }

  void Region::action(Region* r)
  {
    if ((r->local_reference_count == 0) && (r->parent == nullptr))
    {
      // TODO, this can be hooked to perform delayed operations like send.
      //  Needs to check for sub_region_reference_count for send, but not
      //  deallocate.

      if (r != get_local_region() && r != cown_region)
      {
        to_collect.insert(r);
        std::cout << "Collecting region: " << r << std::endl;
      }
    }
  }
}
