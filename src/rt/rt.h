#pragma once

#include "../lang/interpreter.h"
#include "objects/visit.h"
#include "ui.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace rt
{
  using BuiltinFuncPtr = std::function<std::optional<objects::DynObject*>(
    verona::interpreter::FrameObj*, size_t)>;
  void add_builtin(std::string name, BuiltinFuncPtr func);
  objects::DynObject* get_builtin(std::string name);
  verona::interpreter::FrameObj*
  make_frame(verona::interpreter::FrameObj* parent);

  objects::DynObject* make_func(verona::interpreter::Bytecode* body);
  objects::DynObject* make_iter(objects::DynObject* iter_src);
  objects::DynObject* make_str(std::string str_value);
  objects::DynObject* make_object();
  objects::DynObject* make_cown(objects::DynObject* region);

  void freeze(objects::DynObject* obj);
  objects::DynObject* create_region();

  std::optional<objects::DynObject*>
  get(objects::DynObject* src, std::string key);
  std::optional<objects::DynObject*>
  get(objects::DynObject* src, objects::DynObject* key);
  std::string get_key(objects::DynObject* key);
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
  objects::DynObject* get_bool(bool value);

  void add_reference(objects::DynObject* src, objects::DynObject* target);
  void remove_reference(objects::DynObject* src, objects::DynObject* target);
  void move_reference(
    objects::DynObject* src,
    objects::DynObject* dst,
    objects::DynObject* target);

  size_t pre_run(rt::ui::UI* ui);
  void post_run(size_t count, rt::ui::UI* ui);

  objects::DynObject* iter_next(objects::DynObject* iter);
  std::optional<verona::interpreter::Bytecode*>
  try_get_bytecode(objects::DynObject* func);
  std::optional<BuiltinFuncPtr> try_get_builtin_func(objects::DynObject* func);

  void merge_regions(objects::DynObject* src, objects::DynObject* sink);
  void dissolve_region(objects::DynObject* bridge);

  /// This notifies the given cown that something has changed and the state
  /// might need to be updated. This can cause a cown in the pending state to be
  /// released.
  void cown_update_state(objects::DynObject* cown);
  bool is_cown_released(objects::DynObject* cown);

  void aquire_cown(objects::DynObject* cown);
  void release_cown(objects::DynObject* cown);

  // This increases the rc without asking questions. Very much a
  // hack but I don't care anymore.
  void hack_inc_rc(objects::DynObject* obj);
} // namespace rt
