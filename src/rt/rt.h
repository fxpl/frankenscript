#pragma once

#include "../lang/interpreter.h"
#include "objects/visit.h"
#include "ui.h"

#include <string>
#include <vector>

namespace rt
{

  objects::DynObject* make_func(verona::interpreter::Bytecode* body);
  objects::DynObject* make_iter(objects::DynObject* iter_src);
  objects::DynObject* make_str(std::string str_value);
  objects::DynObject* make_object();
  objects::DynObject* make_frame(objects::DynObject* parent);
  objects::DynObject* make_cown(objects::DynObject* region);

  void freeze(objects::DynObject* obj);
  void create_region(objects::DynObject* objects);

  objects::DynObject* get(objects::DynObject* src, std::string key);
  objects::DynObject* get(objects::DynObject* src, objects::DynObject* key);
  objects::DynObject*
  set(objects::DynObject* dst, std::string key, objects::DynObject* value);
  objects::DynObject* set(
    objects::DynObject* dst,
    objects::DynObject* key,
    objects::DynObject* value);

  objects::DynObject*
  set_prototype(objects::DynObject* obj, objects::DynObject* proto);

  objects::DynObject* get_true();
  objects::DynObject* get_false();

  void add_reference(objects::DynObject* src, objects::DynObject* target);
  void remove_reference(objects::DynObject* src, objects::DynObject* target);
  void move_reference(
    objects::DynObject* src,
    objects::DynObject* dst,
    objects::DynObject* target);

  size_t pre_run();
  void post_run(size_t count, rt::ui::UI& ui);

  objects::DynObject* iter_next(objects::DynObject* iter);
  verona::interpreter::Bytecode* get_bytecode(objects::DynObject* func);

} // namespace rt
