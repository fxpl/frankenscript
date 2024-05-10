#pragma once

#include <string>
#include <vector>

namespace objects {
class DynObject;

struct Edge {
  DynObject *src;
  std::string key;
  DynObject *dst;
};

DynObject *make_object(std::string name = "");
DynObject *get_frame();
void inc_rc(DynObject *obj);
void dec_rc(DynObject *obj);
void remove_local_reference(DynObject *obj);
void freeze(DynObject *obj);
DynObject *get(DynObject *obj, std::string key);
void set_copy(DynObject *obj, std::string key, DynObject *value);
void set_move(DynObject *obj, std::string key, DynObject *value);
void create_region(DynObject *objects);

size_t get_object_count();

size_t pre_run();
void post_run(size_t count);

void set_output(std::string path);
void mermaid(std::vector<Edge> &roots);
} // namespace objects