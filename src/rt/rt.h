#pragma once

#include <string>
#include <vector>

namespace objects {
class DynObject;

struct Edge {
  DynObject *src;
  std::string key;
  DynObject *target;
};

DynObject *make_object(std::string name = "");
DynObject *get_frame();
void clear_frame();

void freeze(DynObject *obj);
void create_region(DynObject *objects);

DynObject *get(DynObject *src, std::string key);
DynObject *set(DynObject *dst, std::string key, DynObject *value);

void add_reference(DynObject *src, DynObject *target);
void remove_reference(DynObject *src, DynObject *target);
void move_reference(DynObject *src, DynObject *dst, DynObject *target);

size_t get_object_count();

size_t pre_run();
void post_run(size_t count);

void set_output(std::string path);
void mermaid(std::vector<Edge> &roots);
} // namespace objects