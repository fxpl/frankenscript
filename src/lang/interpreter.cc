#include "bytecode.h"
#include "../rt/rt.h"
#include "trieste/trieste.h"

#include <iostream>
#include <vector>

namespace verona::interpreter {

bool run_stmt(trieste::NodeIt node, std::vector<objects::DynObject *> &stack) {
  if (*node == Print) {
    std::cout << *node << std::endl;
    return true;
  }

  if (*node == CreateObject)
  {
    std::cout << "create object" << std::endl;
    auto v = objects::make_object();
    stack.push_back(v);
    std::cout << "push " << v << std::endl;
    return false;
  }

  if (*node == Null)
  {
    std::cout << "null" << std::endl;
    stack.push_back(nullptr);
    std::cout << "push nullptr" << std::endl;
    return false;
  }

  if (*node == LoadFrame)
  {
    std::cout << "load frame" << std::endl;
    auto frame = objects::get_frame();
    std::string field{(*node)->location().view()};
    auto v = objects::get(frame, field);
    objects::add_reference(frame, v);
    stack.push_back(v);
    std::cout << "push " << v << std::endl;
    return false;
  }

  if (*node == StoreFrame)
  {
    std::cout << "store frame" << std::endl;
    auto frame = objects::get_frame();
    auto v = stack.back();
    stack.pop_back();
    std::cout << "pop " << v << std::endl;
    std::string field{(*node)->location().view()};
    auto v2 = objects::set(frame, field, v);
    remove_reference(frame, v2);
    return false;
  }

  if (*node == LoadField)
  {
    std::cout << "load field" << std::endl;
    auto v = stack.back();
    stack.pop_back();
    std::cout << "pop " << v << std::endl;
    std::string field{(*node)->location().view()};
    auto v2 = objects::get(v, field);
    stack.push_back(v2);
    std::cout << "push " << v2 << std::endl;
    objects::add_reference(objects::get_frame(), v2);
    objects::remove_reference(objects::get_frame(), v);
    return false;
  }

  if (*node == StoreField)
  {
    std::cout << "store field" << std::endl;
    auto v = stack.back();
    stack.pop_back();
    std::cout << "pop " << v << std::endl;
    auto v2 = stack.back();
    stack.pop_back();
    std::cout << "pop " << v2 << std::endl;
    std::string field{(*node)->location().view()};
    auto v3 = objects::set(v2, field, v);
    move_reference(objects::get_frame(), v2, v);
    remove_reference(objects::get_frame(), v2);
    remove_reference(v2, v3);
    return false;
  }

  if (*node == CreateRegion)
  {
    std::cout << "create region" << std::endl;
    auto v = stack.back();
    stack.pop_back();
    std::cout << "pop " << v << std::endl;
    objects::create_region(v);
    remove_reference(objects::get_frame(), v);
    return false;
  }

  if (*node == FreezeObject)
  {
    std::cout << "freeze object" << std::endl;
    auto v = stack.back();
    stack.pop_back();
    std::cout << "pop " << v << std::endl;
    objects::freeze(v);
    remove_reference(objects::get_frame(), v);
    return false;
  }

  return false;
}

bool run_to_print(trieste::NodeIt &node, trieste::NodeIt end) {
  if (node == end)
    return false;

  std::vector<objects::DynObject *> stack;

  while (node != end) {
    if (run_stmt(node, stack))
      return true;
    ++node;
  }
  return false;
}

void run(trieste::Node node) {
  size_t initial = objects::pre_run();
  auto it = node->begin();
  std::vector<objects::Edge> edges{{nullptr, "?", objects::get_frame()}};
  while (run_to_print(it, node->end()))
  {
    objects::mermaid(edges);
    getchar();
    it++;
  }
  objects::post_run(initial);
}

} // namespace verona::interpreter