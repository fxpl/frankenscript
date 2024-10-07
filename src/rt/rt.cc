#include <iostream>
#include <string>
#include <vector>

#include "objects/dyn_object.h"
#include "rt.h"
#include "env.h"

namespace objects {

DynObject *make_func(verona::interpreter::Bytecode *body) {
  return new rt::env::BytecodeFuncObject(body);
}
DynObject *make_iter(DynObject *iter_src) {
  return new rt::env::KeyIterObject(iter_src->fields);
}
DynObject *make_str(std::string value) {
  return new rt::env::StringObject(value);
}
DynObject *make_object() {
  return new DynObject();
}

DynObject *make_frame(DynObject *parent) {
  return new rt::env::FrameObject(parent);
}

thread_local RegionPointer DynObject::local_region = new Region();

void freeze(DynObject *obj) { obj->freeze(); }

void create_region(DynObject *object) { object->create_region(); }

DynObject *get(DynObject *obj, std::string key) {
  return obj->get(key);
}

std::string get_key(DynObject* key) {
  // TODO Add some checking.  This is need to lookup the correct function in the prototype chain.
  if (key->get_prototype() != &rt::env::stringPrototypeObject) {
    rt::ui::error("Key must be a string.");
  }
  rt::env::StringObject *str_key = reinterpret_cast<rt::env::StringObject*>(key);
  return str_key->as_key();
}

DynObject *get(DynObject *obj, DynObject *key) {
  return get(obj, get_key(key));
}

DynObject *set(DynObject *obj, std::string key, DynObject *value) {
  return obj->set(key, value);
}

DynObject *set(DynObject *obj, DynObject *key, DynObject *value) {
  return set(obj, get_key(key), value);
}

// TODO [[nodiscard]]
DynObject *set_prototype(DynObject *obj, DynObject *proto) {
  if (proto->is_primitive() != nullptr) {
    rt::ui::error("Cannot set a primitive as a prototype.");
  }
  if (obj->is_primitive() != nullptr) {
    rt::ui::error("Cannot set a prototype on a primitive object.");
  }
  return obj->set_prototype(proto);
}

void add_reference(DynObject *src, DynObject *target) {
  DynObject::add_reference(src, target);
}

void remove_reference(DynObject *src, DynObject *target) {
  DynObject::remove_reference(src, target);
}

void move_reference(DynObject *src, DynObject *dst, DynObject *target) {
  DynObject::move_reference(src, dst, target);
}

size_t pre_run() {
  std::cout << "Running test..." << std::endl;
  return objects::DynObject::get_count();
}

void post_run(size_t initial_count, rt::ui::UI& ui) {
  std::cout << "Test complete - checking for cycles in local region..."
            << std::endl;
  if (objects::DynObject::get_count() != initial_count) {
    std::cout << "Cycles detected in local region." << std::endl;
    auto objs = objects::DynObject::get_local_region()->get_objects();
    std::vector<objects::Edge> edges;
    for (auto obj : objs) {
      edges.push_back({nullptr, "?", obj});
    }
    ui.output(edges, "Cycles detected in local region.");
  }
  objects::DynObject::get_local_region()->terminate_region();
  if (objects::DynObject::get_count() != initial_count) {
    std::cout << "Memory leak detected!" << std::endl;
    std::cout << "Initial count: " << initial_count << std::endl;
    std::cout << "Final count: " << objects::DynObject::get_count()
              << std::endl;

    std::vector<objects::Edge> edges;
    for (auto obj : objects::DynObject::get_objects()) {
      edges.push_back({nullptr, "?", obj});
    }
    ui.output(edges, "Memory leak detected!");

    std::exit(1);
  } else {
    std::cout << "No memory leaks detected!" << std::endl;
  }
}

namespace value {
  DynObject *iter_next(DynObject *iter) {
    assert(!iter->is_immutable());
    if (iter->get_prototype() != &rt::env::keyIterPrototypeObject) {
      rt::ui::error("Object is not an iterator.");
    }

    return reinterpret_cast<rt::env::KeyIterObject*>(iter)->iter_next();
  }

  verona::interpreter::Bytecode* get_bytecode(objects::DynObject *func) {
    if (func->get_prototype() == &rt::env::bytecodeFuncPrototypeObject) {
      return reinterpret_cast<rt::env::BytecodeFuncObject*>(func)->get_bytecode();
    } else {
      rt::ui::error("Object is not a function.");
      return {};
    }
  }
}

} // namespace objects
