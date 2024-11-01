#include "../rt/rt.h"
#include "bytecode.h"
#include "trieste/trieste.h"

#include <iostream>
#include <optional>
#include <variant>
#include <vector>

namespace verona::interpreter
{

  // ==============================================
  // Public UI
  // ==============================================
  struct Bytecode
  {
    trieste::Node body;
  };

  void delete_bytecode(Bytecode* bytecode)
  {
    delete bytecode;
  }

  // ==============================================
  // Statement Effects
  // ==============================================
  struct ExecNext
  {};

  struct ExecJump
  {
    trieste::Location target;
  };

  struct ExecFunc
  {
    trieste::Node body;
    size_t arg_ctn;
  };

  struct ExecReturn
  {
    std::optional<rt::objects::DynObject*> value;
  };

  // ==============================================
  // Interpreter/state
  // ==============================================
  struct InterpreterFrame
  {
    trieste::NodeIt ip;
    trieste::Node body;
    rt::objects::DynObject* frame;
    std::vector<rt::objects::DynObject*> stack;

    ~InterpreterFrame()
    {
      rt::remove_reference(frame, frame);
    }
  };

  class Interpreter
  {
    rt::ui::UI* ui;
    std::vector<InterpreterFrame*> frame_stack;

    InterpreterFrame* push_stack_frame(trieste::Node body)
    {
      rt::objects::DynObject* parent_obj = nullptr;
      if (!frame_stack.empty())
      {
        parent_obj = frame_stack.back()->frame;
      }

      auto frame = new InterpreterFrame{
        body->begin(), body, rt::make_frame(parent_obj), {}};
      frame_stack.push_back(frame);
      return frame;
    }

    InterpreterFrame* pop_stack_frame()
    {
      auto frame = frame_stack.back();
      frame_stack.pop_back();
      delete frame;

      if (frame_stack.empty())
      {
        return nullptr;
      }
      else
      {
        return frame_stack.back();
      }
    }

    InterpreterFrame* parent_stack_frame()
    {
      return frame_stack[frame_stack.size() - 2];
    }

    std::vector<rt::objects::DynObject*>& stack()
    {
      return frame_stack.back()->stack;
    }

    rt::objects::DynObject* frame()
    {
      return frame_stack.back()->frame;
    }

    rt::objects::DynObject* global_frame()
    {
      return frame_stack.front()->frame;
    }

    rt::objects::DynObject* pop(char const* data_info)
    {
      auto v = stack().back();
      stack().pop_back();
      std::cout << "pop " << v << " (" << data_info << ")" << std::endl;
      return v;
    }

    std::variant<ExecNext, ExecJump, ExecFunc, ExecReturn>
    run_stmt(trieste::Node& node)
    {
      // ==========================================
      // Operators that shouldn't be printed
      // ==========================================
      if (node == Print)
      {
        // Console output
        std::cout << node->location().view() << std::endl << std::endl;

        // Mermaid output
        std::vector<rt::objects::DynObject*> roots{frame()};
        ui->output(roots, std::string(node->location().view()));

        // Continue
        return ExecNext{};
      }
      if (node == Label)
      {
        return ExecNext{};
      }

      // ==========================================
      // Operators that should be printed
      // ==========================================
      std::cout << "Op: " << node->type().str() << std::endl;
      if (node == CreateObject)
      {
        rt::objects::DynObject* obj = nullptr;

        assert(
          !node->empty() && "CreateObject has to specify the type of data");
        auto payload = node->at(0);
        if (payload == Dictionary)
        {
          obj = rt::make_object();
        }
        else if (payload == String)
        {
          obj = rt::make_str(std::string(payload->location().view()));
        }
        else if (payload == KeyIter)
        {
          auto v = pop("iterator source");
          obj = rt::make_iter(v);
          rt::remove_reference(frame(), v);
        }
        else if (payload == Func)
        {
          assert(
            payload->size() == 1 &&
            "CreateObject: A bytecode function requires a body node");
          obj = rt::make_func(new Bytecode{payload->at(0)});
        }
        else
        {
          assert(false && "CreateObject has to specify a value");
        }

        stack().push_back(obj);
        std::cout << "push " << obj << std::endl;
        return ExecNext{};
      }

      if (node == Null)
      {
        stack().push_back(nullptr);
        std::cout << "push nullptr" << std::endl;
        return ExecNext{};
      }

      if (node == LoadFrame)
      {
        std::string field{node->location().view()};
        auto v = rt::get(frame(), field);
        if (!v)
        {
          if (field == "True")
          {
            v = rt::get_true();
          }
          else if (field == "False")
          {
            rt::get_false();
          }
        }

        rt::add_reference(frame(), v);
        stack().push_back(v);
        std::cout << "push " << v << std::endl;
        return ExecNext{};
      }

      if (node == LoadGlobal)
      {
        std::string field{node->location().view()};

        // Local frame
        auto v = rt::get(frame(), field);

        // User globals
        if (!v)
        {
          v = rt::get(global_frame(), field);
        }

        // Builtin globals
        if (!v)
        {
          v = rt::get_builtin(field);
        }

        rt::add_reference(frame(), v);
        stack().push_back(v);
        std::cout << "push " << v << std::endl;
        return ExecNext{};
      }

      if (node == StoreFrame)
      {
        assert(stack().size() >= 1 && "the stack is too small");
        auto v = pop("value to store");
        std::string field{node->location().view()};
        auto v2 = rt::set(frame(), field, v);
        rt::remove_reference(frame(), v2);
        return ExecNext{};
      }

      if (node == SwapFrame)
      {
        assert(stack().size() >= 1 && "the stack is too small");
        auto new_var = pop("swap value");
        std::string field{node->location().view()};

        auto old_var = rt::set(frame(), field, new_var);
        stack().push_back(old_var);

        return ExecNext{};
      }

      if (node == LoadField)
      {
        assert(stack().size() >= 2 && "the stack is too small");
        auto k = pop("lookup-key");
        auto v = pop("lookup-value");

        if (!v)
        {
          std::cerr << std::endl;
          std::cerr << "Error: Tried to access a field on `None`" << std::endl;
          std::abort();
        }

        auto v2 = rt::get(v, k);
        stack().push_back(v2);
        std::cout << "push " << v2 << std::endl;
        rt::add_reference(frame(), v2);
        rt::remove_reference(frame(), k);
        rt::remove_reference(frame(), v);
        return ExecNext{};
      }

      if (node == StoreField)
      {
        assert(stack().size() >= 3 && "the stack is too small");
        auto v = pop("value to store");
        auto k = pop("lookup-key");
        auto v2 = pop("lookup-value");
        auto v3 = rt::set(v2, k, v);
        rt::move_reference(frame(), v2, v);
        rt::remove_reference(frame(), k);
        rt::remove_reference(frame(), v2);
        rt::remove_reference(v2, v3);
        return ExecNext{};
      }

      if (node == SwapField)
      {
        assert(stack().size() >= 3 && "the stack is too small");
        auto new_var = pop("swap value");
        auto key = pop("lookup-key");
        auto obj = pop("lookup-value");
        auto old_var = rt::set(obj, key, new_var);
        stack().push_back(old_var);

        rt::move_reference(frame(), obj, new_var);
        rt::move_reference(obj, frame(), old_var);
        rt::remove_reference(frame(), obj);
        rt::remove_reference(frame(), key);

        return ExecNext{};
      }

      if (node == Eq || node == Neq)
      {
        auto b = pop("Rhs");
        auto a = pop("Lhs");

        auto bool_result = (a == b);
        if (node == Neq)
        {
          bool_result = !bool_result;
        }

        const char* result_str;
        rt::objects::DynObject* result;
        if (bool_result)
        {
          result = rt::get_true();
          result_str = "true";
        }
        else
        {
          result = rt::get_false();
          result_str = "false";
        }
        rt::add_reference(frame(), result);
        stack().push_back(result);
        std::cout << "push " << result << " (" << result_str << ")"
                  << std::endl;

        rt::remove_reference(frame(), a);
        rt::remove_reference(frame(), b);
        return ExecNext{};
      }

      if (node == Jump)
      {
        return ExecJump{node->location()};
      }

      if (node == JumpFalse)
      {
        auto v = pop("jump condition");
        auto jump = (v == rt::get_false());
        rt::remove_reference(frame(), v);
        if (jump)
        {
          return ExecJump{node->location()};
        }
        else
        {
          return ExecNext{};
        }
      }

      if (node == IterNext)
      {
        auto it = pop("iterator");

        auto obj = rt::iter_next(it);
        rt::remove_reference(frame(), it);

        stack().push_back(obj);
        std::cout << "push " << obj << " (next from iter)" << std::endl;
        return ExecNext{};
      }

      if (node == ClearStack)
      {
        if (!stack().empty())
        {
          while (!stack().empty())
          {
            rt::remove_reference(frame(), stack().back());
            stack().pop_back();
          }
        }
        return ExecNext{};
      }

      if (node == Call)
      {
        auto func = pop("function");
        auto arg_ctn = std::stoul(std::string(node->location().view()));

        if (auto bytecode = rt::try_get_bytecode(func))
        {
          rt::remove_reference(frame(), func);
          return ExecFunc{bytecode.value()->body, arg_ctn};
        }
        else if (auto builtin = rt::try_get_builtin_func(func))
        {
          // This calls the built-in function with the current frame and current
          // stack. This makes the implementation on the interpreter side a lot
          // easier and makes builtins more powerful. The tradeoff is that the
          // arguments are still in reverse order on the stack, and the function
          // can potentially modify the "calling" frame.
          auto result = (builtin.value())(frame(), &stack(), arg_ctn);
          if (result)
          {
            stack().push_back(result.value());
          }
          rt::remove_reference(frame(), func);
          return ExecNext{};
        }
        else
        {
          rt::ui::error("Object is not a function", func);
        }
      }

      if (node == Dup)
      {
        // This breaks the normal idea of a stack machine, but every other
        // solution would require more effort and would be messier
        auto dup_idx = std::stoul(std::string(node->location().view()));
        auto stack_size = stack().size();
        assert(
          dup_idx < stack_size &&
          "the stack is too small for this duplication");

        auto var = stack()[stack_size - dup_idx - 1];
        stack().push_back(var);
        std::cout << "push " << var << std::endl;
        rt::add_reference(frame(), var);

        return ExecNext{};
      }

      if (node == Return)
      {
        return ExecReturn{};
      }

      if (node == ReturnValue)
      {
        auto value = pop("return value");
        // RC is transfered to the stack of the parent frame
        return ExecReturn{value};
      }

      std::cerr << "unhandled bytecode" << std::endl;
      node->str(std::cerr);
      std::abort();
    }

  public:
    Interpreter(rt::ui::UI* ui_) : ui(ui_) {}

    void run(trieste::Node main)
    {
      auto frame = push_stack_frame(main);

      while (frame)
      {
        const auto action = run_stmt(*frame->ip);

        if (std::holds_alternative<ExecNext>(action))
        {
          frame->ip++;
        }
        else if (std::holds_alternative<ExecJump>(action))
        {
          auto jump = std::get<ExecJump>(action);
          auto label_node = frame->body->look(jump.target);
          assert(label_node.size() == 1);
          frame->ip = frame->body->find(label_node[0]);
          // Skip the label node
          frame->ip++;
        }
        else if (std::holds_alternative<ExecFunc>(action))
        {
          auto func = std::get<ExecFunc>(action);

          // Make sure the stored ip, continues after the call
          frame->ip++;

          frame = push_stack_frame(func.body);
          auto parent_frame = parent_stack_frame();

          // Setup the new frame
          for (size_t i = 0; i < func.arg_ctn; i++)
          {
            auto value = parent_frame->stack.back();
            frame->stack.push_back(value);
            rt::move_reference(parent_frame->frame, frame->frame, value);
            parent_frame->stack.pop_back();
          }
        }
        else if (std::holds_alternative<ExecReturn>(action))
        {
          frame->ip = frame->body->end();
        }
        else
        {
          assert(false && "unhandled statement action");
        }

        if (frame->ip == frame->body->end())
        {
          if (std::holds_alternative<ExecReturn>(action))
          {
            auto return_ = std::get<ExecReturn>(action);
            if (return_.value.has_value())
            {
              auto parent = parent_stack_frame();
              auto value = return_.value.value();
              rt::move_reference(frame->frame, parent->frame, value);
              parent->stack.push_back(value);
            }
          }

          frame = pop_stack_frame();
        }
      }
    }
  };

  void start(trieste::Node main_body, int step_counter)
  {
    auto ui = rt::ui::globalUI();
    if (ui->is_mermaid())
    {
      reinterpret_cast<rt::ui::MermaidUI*>(ui)->set_step_counter(step_counter);
    }

    size_t initial = rt::pre_run(ui);

    Interpreter inter(ui);
    inter.run(main_body);

    rt::post_run(initial, ui);
  }

} // namespace verona::interpreter
