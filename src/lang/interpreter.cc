#include "bytecode.h"
#include "../rt/rt.h"
#include "trieste/trieste.h"

#include <iostream>
#include <vector>
#include <optional>

namespace verona::interpreter {

struct Bytecode {
  trieste::Node body;
};

std::tuple<bool, std::optional<trieste::Location>> run_stmt(trieste::Node& node, std::vector<objects::DynObject *> &stack, objects::UI* ui) {
  if (node == Print) {
    std::cout << node->location().view() << std::endl << std::endl;
    return {true, {}};
  }

  if (node == CreateObject)
  {
    std::cout << "create object" << std::endl;
    objects::DynObject *obj = nullptr;
    
    assert(!node->empty() && "CreateObject has to specify the type of data");
    auto payload = node->at(0);
    if (payload == Dictionary) {
      obj = objects::make_object();
    } else if (payload == String) {
      obj = objects::make_object(std::string(payload->location().view()));
    } else if (payload == KeyIter) {
      auto v = stack.back();
      stack.pop_back();
      std::cout << "pop " << v << " (iterator source)" << std::endl;
      obj = objects::make_iter(v);
      remove_reference(objects::get_frame(), v);
    } else if (payload == Proto) {
      obj = objects::make_object();
      auto v = stack.back();
      stack.pop_back();
      // RC transferred
      objects::set_prototype(obj, v);
    } else if (payload == Func) {
      assert(payload->size() == 1 && "CreateObject: A bytecode function requires a body node");
      // TODO This leaks memory
      obj = objects::make_func(new Bytecode { payload->at(0) });
    } else {
      assert(false && "CreateObject has to specify a value");
    }

    stack.push_back(obj);
    std::cout << "push " << obj << std::endl;
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
    assert(stack.size() >= 2 && "the stack is too small");
    std::cout << "load field" << std::endl;
    auto k = stack.back();
    stack.pop_back();
    std::cout << "pop " << k << " (lookup-key)" << std::endl;
    auto v = stack.back();
    stack.pop_back();
    std::cout << "pop " << v << " (lookup-value)" << std::endl;

    if (!v) {
      std::cerr << std::endl;
      std::cerr << "Error: Tried to access a field on `None`" << std::endl;
      std::abort();
    }

    auto v2 = objects::get(v, k);
    stack.push_back(v2);
    std::cout << "push " << v2 << std::endl;
    objects::add_reference(objects::get_frame(), v2);
    objects::remove_reference(objects::get_frame(), k);
    objects::remove_reference(objects::get_frame(), v);
    return {false, {}};
  }

  if (node == StoreField)
  {
    std::cout << "store field" << std::endl;
    auto v = stack.back();
    stack.pop_back();
    std::cout << "pop " << v << " (value to store)" << std::endl;
    auto k = stack.back();
    stack.pop_back();
    std::cout << "pop " << k << " (lookup-key)" << std::endl;
    auto v2 = stack.back();
    stack.pop_back();
    std::cout << "pop " << v2 << " (lookup-value)" << std::endl;
    auto v3 = objects::set(v2, k, v);
    move_reference(objects::get_frame(), v2, v);
    remove_reference(objects::get_frame(), k);
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

  if (node == Eq || node == Neq)
  {
    std::cout << "compare objects" << std::endl;
    auto a = stack.back();
    stack.pop_back();
    std::cout << "pop " << a << std::endl;
    auto b = stack.back();
    stack.pop_back();
    std::cout << "pop " << b << std::endl;

    auto bool_result = (a == b);
    if (node == Neq) {
      bool_result = !bool_result;
    }

    std::string result;
    if (bool_result) {
      result = "True";
    } else {
      result = "False";
    }
    auto frame = objects::get_frame();
    auto v = objects::get(frame, result);
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

  if (node == IterNext)
  {
    std::cout << "next iterator value" << std::endl;
    auto it = stack.back();
    stack.pop_back();
    std::cout << "pop " << it << "(iterator)" << std::endl;

    auto obj = objects::value::iter_next(it);
    remove_reference(objects::get_frame(), it);

    stack.push_back(obj);
    std::cout << "push " << obj  << " (next from iter)" << std::endl;
    return {false, {}};
  }

  if (node == Call) {
    std::cout << "function call" << std::endl;
    auto func = stack.back();
    stack.pop_back();
    std::cout << "pop " << func << "(function)" << std::endl;

    std::vector<objects::DynObject *> call_stack;
    auto arg_ctn = std::stoul(std::string(node->location().view()));
    assert(stack.size() >= arg_ctn && "The stack doesn't have enough values for this call");
    for (size_t ctn = 0; ctn < arg_ctn; ctn++) {
      call_stack.push_back(stack.back());
      stack.pop_back();
    }

    // TODO Return value
    objects::value::call(func, call_stack, ui);
    remove_reference(objects::get_frame(), func);

    // stack.push_back(obj);
    std::cout << "push " << "TODO obj"  << " (return value)" << std::endl;
    return {false, {}};
  }

  if (node == PushFrame) {
    std::cout << "push frame" << std::endl;
    objects::push_frame();
    return {false, {}};
  }

  if (node == PopFrame) {
    std::cout << "pop frame" << std::endl;
    objects::pop_frame();
    return {false, {}};
  }

  std::cerr << "unhandled bytecode" << std::endl;
  node->str(std::cerr);
  std::abort();
}

bool run_to_print(trieste::NodeIt &it, trieste::Node body, std::vector<objects::DynObject *> &stack, objects::UI* ui) {
  auto end = body->end();
  if (it == end)
    return false;

  while (it != end) {
    const auto [is_print, jump_label] = run_stmt(*it, stack, ui);
    if (is_print) {
      return true;
    }
    if (jump_label) {
      auto label_node = body->look(jump_label.value());
      assert(label_node.size() == 1);
      it = body->find(label_node[0]);
    }
    ++it;
  }

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
    out << "```" << std::endl;
    out << message << std::endl;
    out << "```" << std::endl;
    objects::mermaid(edges, out);
    if (interactive) {
      out.close();
      std::cout << "Press a key!" << std::endl;
      getchar();
      out.open(path);
    }
  }
};

std::optional<objects::DynObject *> run_body(trieste::Node body, std::vector<objects::DynObject *> &stack, objects::UI* ui) {
  auto it = body->begin();
  while (run_to_print(it, body, stack, ui))
  {
    assert(stack.empty() && "the stack must be empty to generate a valid output");
    std::vector<objects::Edge> edges{{nullptr, "?", objects::get_frame()}};
    ui->output(edges, std::string((*it)->location().view()));
    it++;
  }
  assert(stack.empty() && "the stack must be empty when the body ends");

  return {};
}
std::optional<objects::DynObject *> run_body(Bytecode *body, std::vector<objects::DynObject *> &stack, objects::UI* ui) {
  return run_body(body->body, stack, ui);
}

void start(trieste::Node main_body, bool interactive) {
  size_t initial = objects::pre_run();

  // Our main has no arguments, menaing an empty starting stack.
  std::vector<objects::DynObject *> stack;
  UI ui(interactive);
  run_body(main_body, stack, &ui);

  objects::post_run(initial, ui);
}

} // namespace verona::interpreter