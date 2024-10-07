#include <iostream>
#include <string>
#include <vector>

#include "objects/dyn_object.h"
#include "rt.h"
#include "env.h"

namespace rt {

objects::DynObject *make_func(verona::interpreter::Bytecode *body) {
  return new env::BytecodeFuncObject(body);
}
objects::DynObject *make_iter(objects::DynObject *iter_src) {
  return new env::KeyIterObject(iter_src->fields);
}
objects::DynObject *make_str(std::string value) {
  return new env::StringObject(value);
}
objects::DynObject *make_object() {
  return new objects::DynObject();
}

objects::DynObject *make_frame(objects::DynObject *parent) {
  return new env::FrameObject(parent);
}

thread_local objects::RegionPointer objects::DynObject::local_region = new Region();

void freeze(objects::DynObject *obj) { obj->freeze(); }

void create_region(objects::DynObject *object) { object->create_region(); }

objects::DynObject *get(objects::DynObject *obj, std::string key) {
  return obj->get(key);
}

std::string get_key(objects::DynObject* key) {
  // TODO Add some checking.  This is need to lookup the correct function in the prototype chain.
  if (key->get_prototype() != &env::stringPrototypeObject) {
    ui::error("Key must be a string.");
  }
  env::StringObject *str_key = reinterpret_cast<env::StringObject*>(key);
  return str_key->as_key();
}

objects::DynObject *get(objects::DynObject *obj, objects::DynObject *key) {
  return get(obj, get_key(key));
}

objects::DynObject *set(objects::DynObject *obj, std::string key, objects::DynObject *value) {
  return obj->set(key, value);
}

objects::DynObject *set(objects::DynObject *obj, objects::DynObject *key, objects::DynObject *value) {
  return set(obj, get_key(key), value);
}

// TODO [[nodiscard]]
objects::DynObject *set_prototype(objects::DynObject *obj, objects::DynObject *proto) {
  if (proto->is_primitive() != nullptr) {
    ui::error("Cannot set a primitive as a prototype.");
  }
  if (obj->is_primitive() != nullptr) {
    ui::error("Cannot set a prototype on a primitive object.");
  }
  return obj->set_prototype(proto);
}

objects::DynObject *get_true() {
  return &env::TrueObject;
}
objects::DynObject *get_false() {
  return &env::FalseObject;
}

void add_reference(objects::DynObject *src, objects::DynObject *target) {
  objects::DynObject::add_reference(src, target);
}

void remove_reference(objects::DynObject *src, objects::DynObject *target) {
  objects::DynObject::remove_reference(src, target);
}

void move_reference(objects::DynObject *src, objects::DynObject *dst, objects::DynObject *target) {
  objects::DynObject::move_reference(src, dst, target);
}

size_t pre_run() {
  std::cout << "Running test..." << std::endl;
  return objects::DynObject::get_count();
}

void post_run(size_t initial_count, ui::UI& ui) {
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

objects::DynObject *iter_next(objects::DynObject *iter) {
  assert(!iter->is_immutable());
  if (iter->get_prototype() != &env::keyIterPrototypeObject) {
    ui::error("Object is not an iterator.");
  }

  return reinterpret_cast<env::KeyIterObject*>(iter)->iter_next();
}

verona::interpreter::Bytecode* get_bytecode(objects::DynObject *func) {
  if (func->get_prototype() == &env::bytecodeFuncPrototypeObject) {
    return reinterpret_cast<env::BytecodeFuncObject*>(func)->get_bytecode();
  } else {
    ui::error("Object is not a function.");
    return {};
  }
}

} // namespace rt
