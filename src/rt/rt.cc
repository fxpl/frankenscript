#include "rt.h"

#include "core.h"
#include "objects/dyn_object.h"

#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace rt
{

  objects::DynObject* make_func(verona::interpreter::Bytecode* body)
  {
    return new core::BytecodeFuncObject(body);
  }
  objects::DynObject* make_iter(objects::DynObject* iter_src)
  {
    return new core::KeyIterObject(iter_src->fields);
  }
  objects::DynObject* make_str(std::string value)
  {
    return new core::StringObject(value);
  }
  objects::DynObject* make_object()
  {
    return new objects::DynObject();
  }

  objects::DynObject* make_frame(objects::DynObject* parent)
  {
    return new core::FrameObject(parent);
  }

  objects::DynObject* make_cown(objects::DynObject* region)
  {
    return new core::CownObject(region);
  }

  thread_local objects::RegionPointer objects::DynObject::local_region =
    new Region();

  void freeze(objects::DynObject* obj)
  {
    // Cown specific handling of the freeze operation is handled by the
    // `freeze()` implementation of the object
    obj->freeze();
  }

  void create_region(objects::DynObject* object)
  {
    object->create_region();
  }

  objects::DynObject* get(objects::DynObject* obj, std::string key)
  {
    if (obj->is_opaque())
    {
      ui::error("opaque objects can't be accessed");
    }
    return obj->get(key);
  }

  std::string get_key(objects::DynObject* key)
  {
    // TODO Add some checking.  This is need to lookup the correct function in
    // the prototype chain.
    if (key->get_prototype() != core::stringPrototypeObject())
    {
      ui::error("Key must be a string.");
    }
    core::StringObject* str_key = reinterpret_cast<core::StringObject*>(key);
    return str_key->as_key();
  }

  objects::DynObject* get(objects::DynObject* obj, objects::DynObject* key)
  {
    return get(obj, get_key(key));
  }

  objects::DynObject*
  set(objects::DynObject* obj, std::string key, objects::DynObject* value)
  {
    if (obj->is_opaque())
    {
      // Overwriting data can change the RC and then call destructors of the
      // type this action therefore requires the cown to be aquired
      ui::error("opaque objects can't be modified");
    }

    return obj->set(key, value);
  }

  objects::DynObject* set(
    objects::DynObject* obj, objects::DynObject* key, objects::DynObject* value)
  {
    return set(obj, get_key(key), value);
  }

  // TODO [[nodiscard]]
  objects::DynObject*
  set_prototype(objects::DynObject* obj, objects::DynObject* proto)
  {
    if (proto->is_primitive() != nullptr)
    {
      ui::error("Cannot set a primitive as a prototype.");
    }
    if (obj->is_primitive() != nullptr)
    {
      ui::error("Cannot set a prototype on a primitive object.");
    }
    return obj->set_prototype(proto);
  }

  objects::DynObject* get_true()
  {
    return core::trueObject();
  }
  objects::DynObject* get_false()
  {
    return core::falseObject();
  }

  void add_reference(objects::DynObject* src, objects::DynObject* target)
  {
    objects::DynObject::add_reference(src, target);
  }

  void remove_reference(objects::DynObject* src, objects::DynObject* target)
  {
    objects::DynObject::remove_reference(src, target);
  }

  void move_reference(
    objects::DynObject* src,
    objects::DynObject* dst,
    objects::DynObject* target)
  {
    objects::DynObject::move_reference(src, dst, target);
  }

  size_t pre_run()
  {
    std::cout << "Initilizing global objects" << std::endl;
    core::globals();

    std::cout << "Running test..." << std::endl;
    return objects::DynObject::get_count();
  }

  void post_run(size_t initial_count, ui::UI& ui)
  {
    std::cout << "Test complete - checking for cycles in local region..."
              << std::endl;
    auto globals = core::globals();
    if (objects::DynObject::get_count() != initial_count)
    {
      std::cout << "Cycles detected in local region." << std::endl;
      auto roots = objects::DynObject::get_local_region()->get_objects();
      roots.erase(
        std::remove_if(
          roots.begin(),
          roots.end(),
          [&globals](auto x) { return globals->contains(x); }),
        roots.end());
      ui.output(roots, "Cycles detected in local region.");
    }

    // Freeze global objects, to low the termination of the local region
    std::cout << "Freezing global objects" << std::endl;
    for (auto obj : *globals)
    {
      obj->freeze();
    }

    objects::DynObject::get_local_region()->terminate_region();
    if (objects::DynObject::get_count() != initial_count)
    {
      std::cout << "Memory leak detected!" << std::endl;
      std::cout << "Initial count: " << initial_count << std::endl;
      std::cout << "Final count: " << objects::DynObject::get_count()
                << std::endl;

      std::vector<objects::DynObject*> roots;
      for (auto obj : objects::DynObject::get_objects())
      {
        roots.push_back(obj);
      }
      ui.output(roots, "Memory leak detected!");

      std::exit(1);
    }
    else
    {
      std::cout << "No memory leaks detected!" << std::endl;
    }
  }

  objects::DynObject* iter_next(objects::DynObject* iter)
  {
    assert(!iter->is_immutable());
    if (iter->get_prototype() != core::keyIterPrototypeObject())
    {
      ui::error("Object is not an iterator.");
    }

    return reinterpret_cast<core::KeyIterObject*>(iter)->iter_next();
  }

  verona::interpreter::Bytecode* get_bytecode(objects::DynObject* func)
  {
    if (func->get_prototype() == core::bytecodeFuncPrototypeObject())
    {
      return reinterpret_cast<core::BytecodeFuncObject*>(func)->get_bytecode();
    }
    else
    {
      ui::error("Object is not a function.");
      return {};
    }
  }

} // namespace rt
