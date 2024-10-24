#include "dyn_object.h"

namespace rt::objects
{
  Region* get_region(DynObject* obj)
  {
    if ((obj == nullptr) || obj->is_immutable())
      return nullptr;
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


  void remove_region_reference(Region* src, Region* target)
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
      target->parent = nullptr;
    }
    return;
  }

  void add_region_reference(Region* src_region, DynObject* target)
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
      add_to_region(src_region, target);
      return;
    }

    Region::set_parent(target_region, src_region);
  }

  void add_reference(DynObject* src, DynObject* target)
  {
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

  inline void destruct(DynObject* obj)
  {
    // Called from the region destructor.
    // Remove all references to other objects.
    // If in the same region, then just remove the RC, but don't try to collect
    // as the whole region is being torndown including any potential cycles.
    auto same_region = [](DynObject* src, DynObject* target) {
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


  void create_region(DynObject* obj)
  {
    Region* r = new Region();
    add_to_region(r, obj);
    // Add root reference as external.
    r->local_reference_count++;
  }

}
