#include <iostream>
#include <string>
#include <vector>

#include "mermaid.h"
#include "objects.h"
#include "rt.h"

namespace objects {

DynObject *make_iter(DynObject *iter_src) {
  auto iter = new DynObject("", new value::KeyIterValue(iter_src->fields));
  assert(iter->get_value());

  return iter;
}
DynObject *make_object(std::string value, std::string name) {
  auto obj = new DynObject(name, new value::StrValue(value));
  assert(obj->get_value());
  return obj;
}
DynObject *make_object(std::string name) { return new DynObject(name, nullptr); }

DynObject *get_frame() { return DynObject::frame(); }

void freeze(DynObject *obj) { obj->freeze(); }

void create_region(DynObject *object) { object->create_region(); }

DynObject *get(DynObject *obj, std::string key) {
  return obj->get(key);
}
DynObject *get(DynObject *obj, DynObject *key) {
  auto value = key->get_value();
  assert(value);
  return get(obj, *value->expect_str_value());
}

DynObject *set(DynObject *obj, std::string key, DynObject *value) {
  return obj->set(key, value);
}
DynObject *set(DynObject *obj, DynObject *key, DynObject *value) {
  return set(obj, *key->get_value()->expect_str_value(), value);
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
    auto value = iter->get_value();
    assert(value && "the given `DynObject` doesn't have a value");
    return value->iter_next();
  }
}

} // namespace objects
