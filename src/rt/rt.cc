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
  void add_builtin(std::string name, BuiltinFuncPtr func)
  {
    auto builtin = new core::BuiltinFuncObject(func);
    core::globals()->insert(builtin);
    core::global_names()->insert({name, builtin});
  }

  objects::DynObject* get_builtin(std::string name)
  {
    auto globals = core::global_names();

    auto search = globals->find(name);
    if (search != globals->end())
    {
      return search->second;
    }
    return nullptr;
  }

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

  verona::interpreter::FrameObj*
  make_frame(verona::interpreter::FrameObj* parent)
  {
    if (parent)
    {
      return new core::FrameObject(parent->object());
    }
    else
    {
      return new core::FrameObject(nullptr);
    }
  }

  objects::DynObject* make_cown(objects::DynObject* region)
  {
    return new core::CownObject(region);
  }

  void freeze(objects::DynObject* obj)
  {
    // Cown specific handling of the freeze operation is handled by the
    // `freeze()` implementation of the object
    obj->freeze();
  }

  objects::DynObject* create_region()
  {
    return objects::create_region();
  }

  std::optional<objects::DynObject*>
  get(objects::DynObject* obj, std::string key)
  {
    if (obj->is_opaque())
    {
      if (obj->is_cown())
      {
        ui::error("Cannot access data on a cown that is not aquired", obj);
      }
      else
      {
        ui::error("Cannot access data on an opaque type", obj);
      }
    }
    return obj->get(key);
  }

  std::string get_key(objects::DynObject* key)
  {
    // TODO Add some checking.  This is need to lookup the correct function in
    // the prototype chain.
    if (key && key->get_prototype() != core::stringPrototypeObject())
    {
      ui::error("Key must be a string.", key);
    }
    core::StringObject* str_key = reinterpret_cast<core::StringObject*>(key);
    return str_key->as_key();
  }

  std::optional<objects::DynObject*>
  get(objects::DynObject* obj, objects::DynObject* key)
  {
    return get(obj, get_key(key));
  }

  objects::DynObject*
  set(objects::DynObject* obj, std::string key, objects::DynObject* value)
  {
    if (!obj)
    {
      ui::error("fields can't be set on `None`");
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
    if (proto && proto->is_primitive() != nullptr)
    {
      ui::error("Cannot set a primitive as a prototype.", proto);
    }
    if (obj == nullptr)
    {
      ui::error("Cannot set a prototype on null.");
    }
    if (obj->is_primitive() != nullptr)
    {
      ui::error("Cannot set a prototype on a primitive object.", obj);
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

  objects::DynObject* get_bool(bool value)
  {
    if (value)
    {
      return get_true();
    }
    else
    {
      return get_false();
    }
  }

  void add_reference(objects::DynObject* src, objects::DynObject* target)
  {
    objects::add_reference(src, target);
  }

  void remove_reference(objects::DynObject* src, objects::DynObject* target)
  {
    objects::remove_reference(src, target);
  }

  void move_reference(
    objects::DynObject* src,
    objects::DynObject* dst,
    objects::DynObject* target)
  {
    objects::move_reference(src, dst, target);
  }

  size_t pre_run(ui::UI* ui)
  {
    std::cout << "Initilizing global objects" << std::endl;
    core::globals();
    core::init_builtins(ui);

    if (ui->is_mermaid())
    {
      auto mermaid = reinterpret_cast<ui::MermaidUI*>(ui);
      for (auto global : *core::globals())
      {
        mermaid->add_unreachable_hide(global);
      }
    }

    std::cout << "Running test..." << std::endl;
    return objects::DynObject::get_count();
  }

  void post_run(size_t initial_count, ui::UI* ui)
  {
    std::cout << "Test complete - checking for cycles in local region..."
              << std::endl;
    objects::Region::clean_lrcs();
    objects::Region::collect();
    auto globals = core::globals();
    if (objects::DynObject::get_count() != initial_count)
    {
      std::cout << "Cycles detected in local region." << std::endl;
      auto roots = objects::get_local_region()->get_objects();
      roots.erase(
        std::remove_if(
          roots.begin(),
          roots.end(),
          [&globals](auto x) { return globals->contains(x); }),
        roots.end());
      ui::MermaidUI::highlight_unreachable = true;
      ui->output(roots, "Cycles detected in local region.");
    }

    // Freeze global objects, to allow the termination of the local region
    std::cout << "Freezing global objects" << std::endl;
    for (auto obj : *globals)
    {
      obj->freeze();
    }

    objects::get_local_region()->terminate_region();
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
      ui::MermaidUI::highlight_unreachable = true;
      ui->output(roots, "Memory leak detected!");

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
    if (iter && iter->get_prototype() != core::keyIterPrototypeObject())
    {
      ui::error("Object is not an iterator.", iter);
    }

    return reinterpret_cast<core::KeyIterObject*>(iter)->iter_next();
  }

  std::optional<verona::interpreter::Bytecode*>
  try_get_bytecode(objects::DynObject* func)
  {
    if (func && func->get_prototype() == core::bytecodeFuncPrototypeObject())
    {
      return reinterpret_cast<core::BytecodeFuncObject*>(func)->get_bytecode();
    }
    return std::nullopt;
  }

  std::optional<BuiltinFuncPtr> try_get_builtin_func(objects::DynObject* func)
  {
    if (func && func->get_prototype() == core::builtinFuncPrototypeObject())
    {
      return reinterpret_cast<core::BuiltinFuncObject*>(func)->get_func();
    }
    return std::nullopt;
  }

  void merge_regions(objects::DynObject* src, objects::DynObject* sink)
  {
    if (!src || src->get_prototype() != objects::regionPrototypeObject())
    {
      ui::error("Source is not a region", src);
    }
    if (!sink || sink->get_prototype() != objects::regionPrototypeObject())
    {
      ui::error("Sink is not a region", sink);
    }
    objects::merge_regions(src, sink);
  }

  void dissolve_region(objects::DynObject* bridge)
  {
    if (!bridge || bridge->get_prototype() != objects::regionPrototypeObject())
    {
      ui::error("Argument is not a region", bridge);
    }
    objects::dissolve_region(bridge);
  }

  bool is_cown_released(objects::DynObject* cown)
  {
    if (cown->get_prototype() != core::cownPrototypeObject())
    {
      ui::error("The given object is not a cown", cown);
    }

    return reinterpret_cast<core::CownObject*>(cown)->is_released();
  }

  void cown_update_state(objects::DynObject* cown)
  {
    if (cown->get_prototype() != core::cownPrototypeObject())
    {
      ui::error("The given object is not a cown", cown);
    }

    reinterpret_cast<core::CownObject*>(cown)->update_status();
  }
} // namespace rt
