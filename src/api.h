#pragma once

#include <iostream>
#include <string>
#include <fstream>
#include <initializer_list>
#include <utility>

#include "rt/rt.h"

namespace api {
using namespace objects;

class Reference;

class Object {
  friend class Reference;
  friend void
  mermaid(std::initializer_list<std::pair<std::string, Object &>> roots);

  DynObject *value;

  Object(DynObject *object) : value(object) {}

  void copy(DynObject *new_object) {
    add_reference(get_frame(), new_object);
    auto old_object = value;
    value = new_object;
    remove_reference(get_frame(), old_object);
  }

  void move(DynObject *new_value) {
    auto old_value = value;
    value = new_value;
    remove_reference(get_frame(), old_value);
  }

public:
  Object() : value(nullptr) {}

  Object(Object &other) : value(other.value) {
    add_reference(get_frame(), value);
  }

  Object(Object &&other) : value(other.value) { other.value = nullptr; }

  Object &operator=(Object &other) {
    copy(other.value);
    return *this;
  }

  Object &operator=(Object &&other) {
    move(other.value);
    return *this;
  }

  Object &operator=(std::nullptr_t) {
    move(nullptr);
    return *this;
  }

  Object &operator=(Reference &other);
  Object &operator=(Reference &&other);

  static Object create(std::string name) { return make_object(name); }

  Reference operator[](std::string name);

  ~Object() {
    remove_reference(get_frame(), value);
  }

  void freeze() { objects::freeze(value); }

  void create_region() { objects::create_region(value); }
};

class Reference {
  friend class Object;
  std::string key;
  DynObject *src;

  void copy(DynObject *new_object) {
    add_reference(src, new_object);
    auto old = objects::set(src, key, new_object);
    remove_reference(src, old);
  }

  DynObject *get() { return objects::get(src, key); }

  Reference(std::string name, DynObject *object) : key(name), src(object) {}

public:
  Reference &operator=(Object &other) {
    copy(other.value);
    return *this;
  }

  Reference &operator=(Object &&other) {
    auto old_value = objects::set(src, key, other.value);
    move_reference(get_frame(), src, other.value);
    remove_reference(src, old_value);
    //  move semantics.
    other.value = nullptr;
    return *this;
  }

  Reference &operator=(Reference &other) {
    copy(other.get());
    return *this;
  }

  Reference &operator=(Reference &&other) {
    copy(other.get());
    return *this;
  }

  Reference &operator=(std::nullptr_t) {
    auto old_value = objects::set(src, key, nullptr);
    remove_reference(src, old_value);
    return *this;
  }

  Reference operator[](std::string name) { return {name, objects::get(src, key)}; }
};

Reference Object::operator[](std::string name) {
  return Reference(name, value);
}

Object &Object::operator=(Reference &other) {
  copy(other.get());
  return *this;
}

Object &Object::operator=(Reference &&other) {
  copy(other.get());
  return *this;
}

struct UI : objects::UI
{
  void output(std::vector<objects::Edge> &edges, std::string message) {
    std::ofstream out("mermaid.md");
    objects::mermaid(edges, out);
    std::cout << message << std::endl;
    std::cout << "Press a key!" << std::endl;
    getchar();
  }
};

void mermaid(std::initializer_list<std::pair<std::string, Object &>> roots) {
  UI ui;
  std::vector<objects::Edge> edges;
  for (auto &root : roots) {
    edges.push_back(
        {get_frame(), root.first, root.second.value});
  }
  
  ui.output(edges, "");
}

template <typename F> void run(F &&f) {
  UI ui;
  size_t initial_count = objects::pre_run();
  f();
  objects::post_run(initial_count, ui);
};
} // namespace api