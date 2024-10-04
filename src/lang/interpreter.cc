#include "bytecode.h"
#include "../rt/rt.h"
#include "trieste/trieste.h"

#include <iostream>
#include <vector>
#include <optional>
#include <variant>

namespace verona::interpreter {

// ==============================================
// Public UI
// ==============================================
struct Bytecode {
  trieste::Node body;
};

void delete_bytecode(Bytecode* bytecode) {
  delete bytecode;
}

// ==============================================
// Statement Effects
// ==============================================
struct ExecNext {};
struct ExecJump {
  trieste::Location target;
};
struct ExecFunc {
  trieste::Node body;
  size_t arg_ctn;
};
struct ExecReturn {
  std::optional<objects::DynObject *> value;
};

// ==============================================
// Interpreter/state
// ==============================================
struct InterpreterFrame {
  std::vector<objects::DynObject *> stack;
  trieste::NodeIt ip;
  trieste::Node body;
  // Used for sanity checks
  objects::DynObject *frame;
};

class Interpreter {
  objects::UI* ui;
  std::vector<InterpreterFrame *> frame_stack;
  // This is the top level stack
  std::vector<objects::DynObject *> stack;

  objects::DynObject* pop(char const* data_info) {
    auto v = stack.back();
    stack.pop_back();
    std::cout << "pop " << v << " (" << data_info << ")" << std::endl;
    return v;
  }

  std::variant<ExecNext, ExecJump, ExecFunc, ExecReturn> run_stmt(trieste::Node& node) {
    // ==========================================
    // Operators that shouldn't be printed
    // ==========================================
    if (node == Print) {
      // Console output
      std::cout << node->location().view() << std::endl << std::endl;

      // Mermaid output
      std::vector<objects::Edge> edges{{nullptr, "?", objects::get_frame()}};
      ui->output(edges, std::string(node->location().view()));

      // Continue
      return ExecNext {};
    }
    if (node == Label) {
      return ExecNext {};
    }

    // ==========================================
    // Operators that should be printed
    // ==========================================
    std::cout << "Op: " << node->type().str() << std::endl;
    if (node == CreateObject)
    {
      objects::DynObject *obj = nullptr;
      
      assert(!node->empty() && "CreateObject has to specify the type of data");
      auto payload = node->at(0);
      if (payload == Dictionary) {
        obj = objects::make_object();
      } else if (payload == String) {
        obj = objects::make_object(std::string(payload->location().view()));
      } else if (payload == KeyIter) {
        auto v = pop("iterator source");
        obj = objects::make_iter(v);
        remove_reference(objects::get_frame(), v);
      } else if (payload == Proto) {
        obj = objects::make_object();
        // RC transferred
        objects::set_prototype(obj, pop("prototype source"));
      } else if (payload == Func) {
        assert(payload->size() == 1 && "CreateObject: A bytecode function requires a body node");
        obj = objects::make_func(new Bytecode { payload->at(0) });
      } else {
        assert(false && "CreateObject has to specify a value");
      }

      stack.push_back(obj);
      std::cout << "push " << obj << std::endl;
      return ExecNext {};
    }
    
    if (node == Null)
    {
      stack.push_back(nullptr);
      std::cout << "push nullptr" << std::endl;
      return ExecNext {};
    }

    if (node == LoadFrame)
    {
      auto frame = objects::get_frame();
      std::string field{node->location().view()};
      auto v = objects::get(frame, field);
      objects::add_reference(frame, v);
      stack.push_back(v);
      std::cout << "push " << v << std::endl;
      return ExecNext {};
    }

    if (node == StoreFrame)
    {
      auto frame = objects::get_frame();
      auto v = pop("value to store");
      std::string field{node->location().view()};
      auto v2 = objects::set(frame, field, v);
      remove_reference(frame, v2);
      return ExecNext {};
    }

    if (node == LoadField)
    {
      assert(stack.size() >= 2 && "the stack is too small");
      auto k = pop("lookup-key");
      auto v = pop("lookup-value");

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
      return ExecNext {};
    }

    if (node == StoreField)
    {
      auto v = pop("value to store");
      auto k = pop("lookup-key");
      auto v2 = pop("lookup-value");
      auto v3 = objects::set(v2, k, v);
      move_reference(objects::get_frame(), v2, v);
      remove_reference(objects::get_frame(), k);
      remove_reference(objects::get_frame(), v2);
      remove_reference(v2, v3);
      return ExecNext {};
    }

    if (node == CreateRegion)
    {
      auto v = pop("region source");
      objects::create_region(v);
      remove_reference(objects::get_frame(), v);
      return ExecNext {};
    }

    if (node == FreezeObject)
    {
      auto v = pop("object to freeze");
      objects::freeze(v);
      remove_reference(objects::get_frame(), v);
      return ExecNext {};
    }

    if (node == Eq || node == Neq)
    {
      auto b = pop("Rhs");
      auto a = pop("Lhs");

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
      return ExecNext {};
    }

    if (node == Jump)
    {
      return ExecJump { node->location() };
    }

    if (node == JumpFalse)
    {
      auto v = pop("jump condition");
      auto false_obj = objects::get(objects::get_frame(), "False");
      auto jump = (v == false_obj);
      remove_reference(objects::get_frame(), v);
      if (jump) {
        return ExecJump { node->location() };
      } else {
        return ExecNext {};
      }
    }

    if (node == IterNext)
    {
      auto it = pop("iterator");

      auto obj = objects::value::iter_next(it);
      remove_reference(objects::get_frame(), it);

      stack.push_back(obj);
      std::cout << "push " << obj  << " (next from iter)" << std::endl;
      return ExecNext {};
    }

    if (node == ClearStack) {
      if (!stack.empty()) {
        while (!stack.empty()) {
          remove_reference(objects::get_frame(), stack.back());
          stack.pop_back();
        }
      }
      return ExecNext {};
    }

    if (node == Call) {
      auto func = pop("function");
      auto arg_ctn = std::stoul(std::string(node->location().view()));
      auto action = ExecFunc { objects::value::get_bytecode(func)->body , arg_ctn };
      remove_reference(objects::get_frame(), func);
      return action;
    }

    if (node == Return) {
      return ExecReturn {};
    }

    if (node == ReturnValue) {
      auto value = pop("return value");
      // RC is transfered to the stack of the parent frame
      return ExecReturn { value };
    }

    std::cerr << "unhandled bytecode" << std::endl;
    node->str(std::cerr);
    std::abort();
  }

public:
  Interpreter(objects::UI* ui_): ui(ui_) {}

  void run(trieste::Node main) {
    // Push the main frame, to later clear the locally defined functions and vars
    objects::push_frame();

    auto it = main->begin();
    auto body = main;

    while (it != body->end()) {
      const auto action = run_stmt(*it);

      if (std::holds_alternative<ExecNext>(action)) {
        it++;
      } else if (std::holds_alternative<ExecJump>(action)) {
        auto jump = std::get<ExecJump>(action);
        auto label_node = body->look(jump.target);
        assert(label_node.size() == 1);
        it = body->find(label_node[0]);
        // Skip the label node
        it++;
      } else if (std::holds_alternative<ExecFunc>(action)) {
        auto func = std::get<ExecFunc>(action);

        // Make sure the stored it, continues after the call
        it++;

        // Store the current frame
        auto parent_frame = new InterpreterFrame { std::move(stack), it, body, objects::get_frame() };
        frame_stack.push_back(parent_frame);
        objects::push_frame();

        // Setup the new frame
        body = func.body;
        it = body->begin();
        stack = {};
        for (size_t i = 0; i < func.arg_ctn; i++) {
          stack.push_back(parent_frame->stack.back());
          parent_frame->stack.pop_back();
        }
      } else if (std::holds_alternative<ExecReturn>(action)) {
        it = body->end();
      } else {
        assert(false && "unhandled statement action");
      }

      if (it == body->end() && !frame_stack.empty()) {
        // Handle frame stack poping
        auto frame = frame_stack.back();
        frame_stack.pop_back();
        objects::pop_frame();

        assert(frame->frame == objects::get_frame() && "the interpreter and object frames need to be synced");

        it = frame->ip;
        body = frame->body;
        stack = frame->stack;
        delete frame;

        if (std::holds_alternative<ExecReturn>(action)) {
          auto return_ = std::get<ExecReturn>(action);
          if (return_.value.has_value()) {
            stack.push_back(return_.value.value());
          }
        }
      }
    }

    // Pop the main frame, to clear local functions and variables
    objects::pop_frame();
  }
};

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

void start(trieste::Node main_body, bool interactive) {
  size_t initial = objects::pre_run();

  UI ui(interactive);
  Interpreter inter(&ui);
  inter.run(main_body);

  objects::post_run(initial, ui);
}

} // namespace verona::interpreter