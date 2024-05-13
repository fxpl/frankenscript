#include <iostream>
#include <string>
#include <vector>

#include "mermaid.h"
#include "objects.h"
#include "rt.h"

namespace objects {

DynObject *make_object(std::string name) { return new DynObject(name); }

DynObject *get_frame() { return DynObject::frame(); }

void freeze(DynObject *obj) { obj->freeze(); }

void create_region(DynObject *object) { object->create_region(); }

DynObject *get(DynObject *obj, std::string key) { return obj->get(key); }

DynObject *set(DynObject *obj, std::string key, DynObject *value) {
  return obj->set(key, value);
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

void post_run(size_t initial_count) {
  std::cout << "Test complete - checking for cycles in local region..."
            << std::endl;
  if (objects::DynObject::get_count() != initial_count) {
    std::cout << "Cycles detected in local region." << std::endl;
    auto objs = objects::DynObject::get_local_region()->get_objects();
    std::vector<objects::Edge> edges;
    for (auto obj : objs) {
      edges.push_back({nullptr, "?", obj});
    }
    objects::mermaid(edges);
    std::cout << "Press a key!" << std::endl;
    getchar();
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
    objects::mermaid(edges);
    std::cout << "Press a key!" << std::endl;
    getchar();

    std::exit(1);
  } else {
    std::cout << "No memory leaks detected!" << std::endl;
  }
}

void set_output(std::string path);
void mermaid(std::vector<Edge> &roots);
} // namespace objects