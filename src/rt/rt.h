#pragma once

#include <string>
#include <vector>
#include <cassert>
#include <optional>

#include "../lang/interpreter.h"

namespace objects {
class DynObject;

struct Edge {
  DynObject *src;
  std::string key;
  DynObject *target;
};

DynObject *make_func(verona::interpreter::Bytecode *body);
DynObject *make_iter(DynObject *iter_src);
DynObject *make_object(std::string str_value);
DynObject *make_object();

/// @brief This pushes a new frame onto the frame stack
void push_frame();
/// @brief Returns the current frame at the top of the frame stack.
DynObject *get_frame();
/// @brief This pops the current frame.
void pop_frame();

void freeze(DynObject *obj);
void create_region(DynObject *objects);

DynObject *get(DynObject *src, std::string key);
DynObject *get(DynObject *src, DynObject *key);
DynObject *set(DynObject *dst, std::string key, DynObject *value);
DynObject *set(DynObject *dst, DynObject *key, DynObject *value);

DynObject *set_prototype(DynObject *obj, DynObject *proto);

void add_reference(DynObject *src, DynObject *target);
void remove_reference(DynObject *src, DynObject *target);
void move_reference(DynObject *src, DynObject *dst, DynObject *target);

size_t get_object_count();

struct UI
{
  virtual void output(std::vector<objects::Edge> &, std::string ) {}
};

size_t pre_run();
void post_run(size_t count, UI& ui);

void mermaid(std::vector<Edge> &roots, std::ostream &out);

namespace value {
  DynObject *iter_next(DynObject *iter);
  verona::interpreter::Bytecode* get_bytecode(objects::DynObject *func);
}

} // namespace objects
