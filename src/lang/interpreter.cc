#include "bytecode.h"
#include "../rt/rt.h"
#include "trieste/trieste.h"

#include <iostream>
#include <vector>
#include <optional>

namespace verona::interpreter {


std::tuple<bool, std::optional<trieste::Location>> run_stmt(trieste::Node& node, std::vector<objects::DynObject *> &stack) {
  if (node == Print) {
    std::cout << node << std::endl;
    return {true, {}};
  }

  if (node == CreateObject)
  {
    std::cout << "create object" << std::endl;
    auto v = objects::make_object();
    stack.push_back(v);
    std::cout << "push " << v << std::endl;
    return {false, {}};
  }

  if (node == Null)
  {
    std::cout << "null" << std::endl;
    stack.push_back(nullptr);
    std::cout << "push nullptr" << std::endl;
    return {false, {}};
  }

  if (node == LoadFrame)
  {
    std::cout << "load frame" << std::endl;
    auto frame = objects::get_frame();
    std::string field{node->location().view()};
    auto v = objects::get(frame, field);
    objects::add_reference(frame, v);
    stack.push_back(v);
    std::cout << "push " << v << std::endl;
    return {false, {}};
  }

  if (node == StoreFrame)
  {
    std::cout << "store frame" << std::endl;
    auto frame = objects::get_frame();
    auto v = stack.back();
    stack.pop_back();
    std::cout << "pop " << v << std::endl;
    std::string field{node->location().view()};
    auto v2 = objects::set(frame, field, v);
    remove_reference(frame, v2);
    return {false, {}};
  }

  if (node == LoadField)
  {
    std::cout << "load field" << std::endl;
    auto v = stack.back();
    stack.pop_back();
    std::cout << "pop " << v << std::endl;
    std::string field{node->location().view()};
    auto v2 = objects::get(v, field);
    stack.push_back(v2);
    std::cout << "push " << v2 << std::endl;
    objects::add_reference(objects::get_frame(), v2);
    objects::remove_reference(objects::get_frame(), v);
    return {false, {}};
  }

  if (node == StoreField)
  {
    std::cout << "store field" << std::endl;
    auto v = stack.back();
    stack.pop_back();
    std::cout << "pop " << v << std::endl;
    auto v2 = stack.back();
    stack.pop_back();
    std::cout << "pop " << v2 << std::endl;
    std::string field{node->location().view()};
    auto v3 = objects::set(v2, field, v);
    move_reference(objects::get_frame(), v2, v);
    remove_reference(objects::get_frame(), v2);
    remove_reference(v2, v3);
    return {false, {}};
  }

  if (node == CreateRegion)
  {
    std::cout << "create region" << std::endl;
    auto v = stack.back();
    stack.pop_back();
    std::cout << "pop " << v << std::endl;
    objects::create_region(v);
    remove_reference(objects::get_frame(), v);
    return {false, {}};
  }

  if (node == FreezeObject)
  {
    std::cout << "freeze object" << std::endl;
    auto v = stack.back();
    stack.pop_back();
    std::cout << "pop " << v << std::endl;
    objects::freeze(v);
    remove_reference(objects::get_frame(), v);
    return {false, {}};
  }

  if (node == Cmp)
  {
    std::cout << "compare objects" << std::endl;
    auto a = stack.back();
    stack.pop_back();
    std::cout << "pop " << a << std::endl;
    auto b = stack.back();
    stack.pop_back();
    std::cout << "pop " << b << std::endl;

    std::string result;
    if (a == b) {
      result = "True";
    } else {
      result = "False";
    }
    auto frame = objects::get_frame();
    auto v = objects::get(frame, result);
    // This reference addition is theoretically not needed, since `True`
    // and `False` are frozen.
    objects::add_reference(frame, v);
    stack.push_back(v);
    std::cout << "push " << v << " (" << result << ")"<< std::endl;

    remove_reference(objects::get_frame(), a);
    remove_reference(objects::get_frame(), b);
    return {false, {}};
  }

  // Noop
  if (node == Label) {
    return {false, {}};
  }

  if (node == Jump)
  {
    std::cout << "jump to " << node->location().view() << std::endl;
    return {false, node->location()};
  }

  if (node == JumpFalse)
  {
    std::cout << "jump if stack has `False`" << std::endl;
    auto v = stack.back();
    stack.pop_back();

    auto false_obj = objects::get(objects::get_frame(), "False");
    std::optional<trieste::Location> loc = {};
    if (v == false_obj) {
      loc = node->location();
    }

    remove_reference(objects::get_frame(), v);
    return {false, loc};
  }

  std::cerr << "unhandled bytecode" << std::endl;
  node->str(std::cerr);
  std::abort();
}

bool run_to_print(trieste::NodeIt &it, trieste::Node top) {
  auto end = top->end();
  if (it == end)
    return false;

  std::vector<objects::DynObject *> stack;

  while (it != end) {
    const auto [is_print, jump_label] = run_stmt(*it, stack);
    if (is_print)
      return true;
    if (jump_label) {
      auto label_node = top->look(jump_label.value());
      assert(label_node.size() == 1);
      it = top->find(label_node[0]);
    }
    ++it;
  }

  assert(stack.empty());
  return false;
}

class UI : public objects::UI
{
  bool interactive;
  std::string path;
  std::ofstream out;

public:
  UI(bool interactive_) : interactive(interactive_) {
    path = "mermaid.md";
    out.open(path);
  }

  void output(std::vector<objects::Edge> &edges, std::string message) {
    objects::mermaid(edges, out);
    if (interactive) {
      out.close();
      std::cout << "Press a key!" << std::endl;
      getchar();
      out.open(path);
    }
    else
    {
      out << message << std::endl;
    }
  }
};

void run(trieste::Node node, bool interactive = true) {
  UI ui(interactive);
  size_t initial = objects::pre_run();
  auto it = node->begin();
  std::vector<objects::Edge> edges{{nullptr, "?", objects::get_frame()}};
  while (run_to_print(it, node))
  {
    ui.output(edges, (*it)->str());
    it++;
  }
  objects::post_run(initial, ui);
}

} // namespace verona::interpreter