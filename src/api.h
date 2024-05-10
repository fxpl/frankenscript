#pragma once

#include <iostream>
#include <string>
#include <initializer_list>
#include <utility>

#include "rt/rt.h"

namespace api {
using namespace objects;

class Reference;

void set_output(std::string path) { objects::set_output(path); }

class Object {
  friend class Reference;
  friend void
  mermaid(std::initializer_list<std::pair<std::string, Object &>> roots);

  DynObject *object;

  Object(DynObject *object) : object(object) {}

  void copy(DynObject *new_object) {
    auto old = object;
    if (new_object != nullptr)
      inc_rc(new_object);
    object = new_object;
    if (old != nullptr)
      dec_rc(old);
  }

  void move(DynObject *new_object) {
    auto old = object;
    object = new_object;
    if (old != nullptr)
      dec_rc(old);
  }

public:
  Object() : object(nullptr) {}

  Object(Object &other) : object(other.object) {
    if (object != nullptr)
      inc_rc(object);
  }

  Object(Object &&other) : object(other.object) { other.object = nullptr; }

  Object &operator=(Object &other) {
    copy(other.object);
    return *this;
  }

  Object &operator=(Object &&other) {
    move(other.object);
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
    if (object != nullptr)
      dec_rc(object);
  }

  void freeze() { objects::freeze(object); }

  void create_region() { objects::create_region(object); }
};

class Reference {
  friend class Object;
  std::string key;
  DynObject *object;

  void copy(DynObject *new_object) {
    auto old = object;
    objects::set_copy(object, key, new_object);
  }

  void move(DynObject *&new_object) {
    // Underlying RC transferred to the field
    set_move(object, key, new_object);
    // Need to remove the lrc if appropriate.
    remove_local_reference(new_object);
    // Clear the pointer to provide linearity of move.
    new_object = nullptr;
  }

  DynObject *get() { return objects::get(object, key); }

  Reference(std::string name, DynObject *object) : key(name), object(object) {}

public:
  Reference &operator=(Object &other) {
    copy(other.object);
    return *this;
  }

  Reference &operator=(Object &&other) {
    move(other.object);
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
    objects::set_copy(object, key, nullptr);
    return *this;
  }

  Reference operator[](std::string name) { return {name, objects::get(object, key)}; }
};

Reference Object::operator[](std::string name) {
  return Reference(name, object);
}

Object &Object::operator=(Reference &other) {
  copy(other.get());
  return *this;
}

Object &Object::operator=(Reference &&other) {
  copy(other.get());
  return *this;
}

void mermaid(std::initializer_list<std::pair<std::string, Object &>> roots) {
  std::vector<objects::Edge> edges;
  for (auto &root : roots) {
    edges.push_back(
        {get_frame(), root.first, root.second.object});
  }
  objects::mermaid(edges);
  std::cout << "Press a key!" << std::endl;
  getchar();
}

template <typename F> void run(F &&f) {
  size_t initial_count = objects::pre_run();
  f();
  objects::post_run(initial_count);
};
} // namespace api