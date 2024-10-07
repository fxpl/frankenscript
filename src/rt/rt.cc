#include <iostream>
#include <string>
#include <vector>

#include "mermaid.h"
#include "objects.h"
#include "rt.h"

namespace objects {

DynObject *make_func(verona::interpreter::Bytecode *body) {
  return new BytecodeFuncObject(body);
}
DynObject *make_iter(DynObject *iter_src) {
  return new KeyIterObject(iter_src->fields);
}
DynObject *make_object(std::string value) {
  return new StringObject(value);
}
DynObject *make_object() { return new DynObject(); }

thread_local std::vector<DynObject *> DynObject::frame_stack = { FrameObject::create_first_stack() };

void push_frame() {
  auto parent = DynObject::frame();
  DynObject::push_frame(new FrameObject(parent));
}
DynObject *get_frame() { return DynObject::frame(); }
void pop_frame() {
  DynObject::pop_frame();
}

void freeze(DynObject *obj) { obj->freeze(); }

void create_region(DynObject *object) { object->create_region(); }

DynObject *get(DynObject *obj, std::string key) {
  return obj->get(key);
}

std::string get_key(DynObject* key) {
  // TODO Add some checking.  This is need to lookup the correct function in the prototype chain.
  if (key->get_prototype() != &stringPrototypeObject) {
    error("Key must be a string.");
  }
  StringObject *str_key = reinterpret_cast<StringObject*>(key);
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
    error("Cannot set a primitive as a prototype.");
  }
  if (obj->is_primitive() != nullptr) {
    error("Cannot set a prototype on a primitive object.");
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
  objects::DynObject::set_local_region(new Region());
  std::cout << "Running test..." << std::endl;
  return objects::DynObject::get_count();
}

void post_run(size_t initial_count, UI& ui) {
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
    if (iter->get_prototype() != &objects::keyIterPrototypeObject) {
      error("Object is not an iterator.");
    }

    return reinterpret_cast<KeyIterObject*>(iter)->iter_next();
  }

  verona::interpreter::Bytecode* get_bytecode(objects::DynObject *func) {
    if (func->get_prototype() == &objects::bytecodeFuncPrototypeObject) {
      return reinterpret_cast<BytecodeFuncObject*>(func)->get_bytecode();
    } else {
      error("Object is not a function.");
      return {};
    }
  }
}

} // namespace objects
