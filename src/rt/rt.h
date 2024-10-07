#pragma once

#include <string>
#include <vector>

#include "../lang/interpreter.h"
#include "objects/visit.h"
#include "ui.h"

namespace objects {

DynObject *make_func(verona::interpreter::Bytecode *body);
DynObject *make_iter(DynObject *iter_src);
DynObject *make_str(std::string str_value);
DynObject *make_object();
DynObject *make_frame(DynObject *parent);

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

size_t pre_run();
void post_run(size_t count, rt::ui::UI& ui);

namespace value {
  DynObject *iter_next(DynObject *iter);
  verona::interpreter::Bytecode* get_bytecode(objects::DynObject *func);
}

} // namespace objects
