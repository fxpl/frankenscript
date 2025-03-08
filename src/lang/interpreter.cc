#include "../rt/rt.h"
#include "bytecode.h"
#include "trieste/trieste.h"

#include <iostream>
#include <optional>
#include <variant>
#include <vector>

// Concurrency related
#include <random>
#include <unordered_map>
#include "lang.h"
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

  struct ExecConc
  {
    trieste::Node body;
    size_t arg_ctn;
  };
  

  // ==============================================
  // Interpreter/state
  // ==============================================
  struct InterpreterFrame
  {
    trieste::NodeIt ip;
    trieste::Node body;
    FrameObj* frame;
  };

  class Interpreter
  {
    rt::ui::UI* ui;
    std::vector<InterpreterFrame*> frame_stack;
    // Original run() uses local var to track the frame, however with threads/behaviours
    // the object must store this instead
    InterpreterFrame* frame_tracker;


    InterpreterFrame* pop_stack_frame()
    {
      auto frame = frame_stack.back();
      frame_stack.pop_back();
      rt::remove_reference(frame->frame->object(), frame->frame->object());
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


    FrameObj* frame()
    {
      return frame_stack.back()->frame;
    }

    FrameObj* global_frame()
    {
      return frame_stack.front()->frame;
    }

    std::variant<ExecNext, ExecJump, ExecFunc, ExecReturn, ExecConc>
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
        std::vector<rt::objects::DynObject*> roots{frame()->object()};
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
          auto v = frame()->stack_pop("iterator source");
          obj = rt::make_iter(v);
          rt::remove_reference(frame()->object(), v);
        }
        else if (payload == Func)
        {
          assert(
            payload->size() == 1 &&
            "CreateObject: A bytecode function requires a body node");
          
          if (node->location().view().starts_with("when ("))
          {
            //auto arg_ctn = std::stoul(std::string(node->location().view()));
            // TODO extract cown args
            ulong arg_ctn = 1;
            auto func = rt::make_func(new Bytecode{payload->at(0)});
            auto bytecode = rt::try_get_bytecode(func);
            //std::cout << arg_ctn << std::endl;
            std::cout << node->location().view() << std::endl;
            // Technically not necessary, however trieste Node was generated
            // expecting a function definition
            frame()->stack_push(func, "new object", false);
            return ExecConc{bytecode.value()->body, arg_ctn};
          }
          obj = rt::make_func(new Bytecode{payload->at(0)});
        }
        else
        {
          assert(false && "CreateObject has to specify a value");
        }

        // NO: rt::add_reference since objects are created with an rc of 1
        frame()->stack_push(obj, "new object", false);
        return ExecNext{};
      }

      if (node == Null)
      {
        frame()->stack_push(nullptr, "null");
        return ExecNext{};
      }

      if (node == LoadFrame)
      {
        std::string field{node->location().view()};
        auto v = rt::get(frame()->object(), field);
        if (!v)
        {
          if (field == "True")
          {
            v = rt::get_true();
          }
          else if (field == "False")
          {
            v = rt::get_false();
          }
        }

        frame()->stack_push(v, "load from frame");
        return ExecNext{};
      }

      if (node == LoadGlobal)
      {
        std::string field{node->location().view()};

        // Local frame
        auto v = rt::get(frame()->object(), field);

        // User globals
        if (!v)
        {
          v = rt::get(global_frame()->object(), field);
        }

        // Builtin globals
        if (!v)
        {
          v = rt::get_builtin(field);
        }

        frame()->stack_push(v, "load from global");
        return ExecNext{};
      }

      if (node == StoreFrame)
      {
        assert(frame()->get_stack_size() >= 1 && "the stack is too small");
        auto v = frame()->stack_pop("value to store");
        std::string field{node->location().view()};
        auto v2 = rt::set(frame()->object(), field, v);
        rt::remove_reference(frame()->object(), v2);
        return ExecNext{};
      }

      if (node == SwapFrame)
      {
        assert(frame()->get_stack_size() >= 1 && "the stack is too small");
        auto new_var = frame()->stack_pop("swap value");
        std::string field{node->location().view()};

        auto old_var = rt::set(frame()->object(), field, new_var);
        // RC stays the same
        frame()->stack_push(old_var, "swaped", false);

        return ExecNext{};
      }

      if (node == LoadField)
      {
        assert(frame()->get_stack_size() >= 2 && "the stack is too small");
        auto k = frame()->stack_pop("lookup-key");
        auto v = frame()->stack_pop("lookup-value");

        if (!v)
        {
          std::cerr << std::endl;
          std::cerr << "Error: Tried to access a field on `None`" << std::endl;
          std::abort();
        }

        auto v2 = rt::get(v, k);
        frame()->stack_push(v2, "loaded field");
        rt::remove_reference(frame()->object(), k);
        rt::remove_reference(frame()->object(), v);
        return ExecNext{};
      }

      if (node == StoreField)
      {
        assert(frame()->get_stack_size() >= 3 && "the stack is too small");
        auto v = frame()->stack_pop("value to store");
        auto k = frame()->stack_pop("lookup-key");
        auto v2 = frame()->stack_pop("lookup-value");
        auto v3 = rt::set(v2, k, v);
        rt::move_reference(frame()->object(), v2, v);
        rt::remove_reference(frame()->object(), k);
        rt::remove_reference(frame()->object(), v2);
        rt::remove_reference(v2, v3);
        return ExecNext{};
      }

      if (node == SwapField)
      {
        assert(frame()->get_stack_size() >= 3 && "the stack is too small");
        auto new_var = frame()->stack_pop("swap value");
        auto key = frame()->stack_pop("lookup-key");
        auto obj = frame()->stack_pop("lookup-value");
        auto old_var = rt::set(obj, key, new_var);
        // RC stays the same
        frame()->stack_push(old_var, "swapped value", false);

        rt::move_reference(obj, frame()->object(), old_var);
        rt::move_reference(frame()->object(), obj, new_var);
        rt::remove_reference(frame()->object(), obj);
        rt::remove_reference(frame()->object(), key);

        return ExecNext{};
      }

      if (node == Eq || node == Neq)
      {
        auto b = frame()->stack_pop("Rhs");
        auto a = frame()->stack_pop("Lhs");

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
        frame()->stack_push(result, result_str);

        rt::remove_reference(frame()->object(), a);
        rt::remove_reference(frame()->object(), b);
        return ExecNext{};
      }

      if (node == Jump)
      {
        return ExecJump{node->location()};
      }

      if (node == JumpFalse)
      {
        auto v = frame()->stack_pop("jump condition");
        auto jump = (v == rt::get_false());
        rt::remove_reference(frame()->object(), v);
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
        auto it = frame()->stack_pop("iterator");

        auto obj = rt::iter_next(it);
        rt::remove_reference(frame()->object(), it);

        frame()->stack_push(obj, "next from iter", false);
        return ExecNext{};
      }

      if (node == ClearStack)
      {
        while (!frame()->stack_is_empty())
        {
          auto value = frame()->stack_pop("value to clear");
          rt::remove_reference(frame()->object(), value);
        }
        return ExecNext{};
      }

      if (node == Call)
      {
        auto func = frame()->stack_pop("function");
        std::cout << node->location().view() << "\n" << std::endl;
        auto arg_ctn = std::stoul(std::string(node->location().view()));

        if (auto bytecode = rt::try_get_bytecode(func))
        {
          rt::remove_reference(frame()->object(), func);
          return ExecFunc{bytecode.value()->body, arg_ctn};
        }
        else if (auto builtin = rt::try_get_builtin_func(func))
        {
          // This calls the built-in function with the current frame and current
          // stack. This makes the implementation on the interpreter side a lot
          // easier and makes builtins more powerful. The tradeoff is that the
          // arguments are still in reverse order on the stack, and the function
          // can potentially modify the "calling" frame.
          auto result = (builtin.value())(frame(), arg_ctn);
          if (result)
          {
            auto value = result.value();
            frame()->stack_push(value, "result from builtin", false);
          }
          rt::remove_reference(frame()->object(), func);
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
        auto stack_size = frame()->get_stack_size();
        assert(
          dup_idx < stack_size &&
          "the stack is too small for this duplication");

        auto var = frame()->stack_get(stack_size - dup_idx - 1);
        frame()->stack_push(var, "duplicated value");

        return ExecNext{};
      }

      if (node == Return)
      {
        return ExecReturn{};
      }

      if (node == ReturnValue)
      {
        auto value = frame()->stack_pop("return value");
        // RC is transfered to the stack of the parent frame
        return ExecReturn{value};
      }

      std::cerr << "unhandled bytecode" << std::endl;
      node->str(std::cerr);
      std::abort();
    }

  public:
    Interpreter(rt::ui::UI* ui_) : ui(ui_) {}


    //void run(trieste::Node main)

    // Returns: Has thread run to completion?
    // TODO refactor clumsy passing of interpreter_stack later
    bool run_step()
    {
      auto frame = this->frame_tracker;

      if (frame)
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
            auto value = parent_frame->frame->stack_pop("argument");
            frame->frame->stack_push(value, "argument", false);
            rt::move_reference(
              parent_frame->frame->object(), frame->frame->object(), value);
          }
        }
        else if (std::holds_alternative<ExecReturn>(action))
        {
          frame->ip = frame->body->end();
        }
        else if (std::holds_alternative<ExecConc>(action))
        {
          frame->ip++;
          auto func = std::get<ExecConc>(action);
          std::cout << func.body->location().view() << std::endl;
          //verona::wf::grouping.
          
          //Interpreter inter(ui);
          //interpreter_stack.push_back(&inter);
          //inter.push_stack_frame(func.body);
          //schedule_when()
          // make new interpreter
          // push it
          // add "when_x" to its defined funcs
          // (ergo we need to track 'x'? No, each interpreter will only have one "when" )
          // (Potential issue)
          // add call "when_x" 
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
              parent->frame->stack_push(value, "return", false);
              rt::move_reference(
                frame->frame->object(), parent->frame->object(), value);
            }
          }

          this->frame_tracker = pop_stack_frame();
        }
        return false;
      }
      else
      {
        return true;
      }
    }

    InterpreterFrame* push_stack_frame(trieste::Node body)
    {
      FrameObj* parent_obj = nullptr;
      if (!frame_stack.empty())
      {
        parent_obj = frame_stack.back()->frame;
      }

      auto frame =
        new InterpreterFrame{body->begin(), body, rt::make_frame(parent_obj)};
      frame_stack.push_back(frame);
      frame_tracker = frame;
      return frame;
    }
  };

  class Sceduler {
    virtual void start(trieste::Node main_body) 
    {

    };
    //ScedulerKind kind();
  };

  // class ThreadSceduler: public Sceduler {
  //   std::vector<Interpreter*>  active;

  //   void start(trieste::Node main_body) override {
  //     while (!active.is_empty()) {
  //       auto step = rand() & active.size();

  //       auto res = active[step].run_step();
  //       if (res == Done) {
  //         active.remove(idx);
  //       }
  //     }
  //   }

  //   void spawn(thread: (Body, args)) {
  //     auto i = new Interpreter(body);
  //     i->frame->push(args...);
  //     active.push(i);
  //   }
  // };
  
  class Behaviour
  {
  private:
      trieste::Node thunk;
      size_t aquired_cowns{0};
      std::vector<rt::objects::DynObject*> cowns;
  public:
      Behaviour(trieste::Node thunk_) : thunk(thunk_) {}
      Behaviour(trieste::Node t, std::vector<rt::objects::DynObject*> c)
      {
        
      }
  };
  

  class BocSceduler: public Sceduler {
    std::vector<Behaviour*> behaviors;

    void start(trieste::Node main_body) {
      // Init main execution
      auto i = new Interpreter(rt::ui::globalUI());
      i


      while (!behaviors.is_empty()) {
        // 1. Check dependency graph
        // 2. Get ready behaviors
        ready_behaviors = magic();
        
        auto step = rand() & ready_behaviors.size();
        
        auto behavior = ready_behaviors[step];
        auto i = new Interpreter(body);
        i->frame->push(args...);
        i->run_to_completion();

        this->mark_as_done(behavior);
        // Removes behavior from this->behaviors
        // Updates dependency graph
      }
    }
    void scedule(behavior: (func, cown)) {

    }
  }
  
  void BoC_concurrency(std::vector<Interpreter*> interpreter_stack)
  {
    // One behaviour/interpreter queue for each cown? No actually concurrency needed so should work
    // Once behaviour is in, no cown queue move from pending to ready
    // Note that for b1: when(c1.c2), given e.g. global order c1 < c2, c2_queue.add(b1)
    // cant occur prior to c1_queue.remove(b1) (ergo b1 must aquire cowns one at a time with respect to global order)
    //ready_queue;
    //pending_queue;



    std::mt19937 gen(42);
    std::uniform_int_distribution<int> dist(0, 100);
    Interpreter* chosen_interpreter;
    //size_t chosen_index;

    size_t num_current_threads{1};
    bool finished = false;
    //cown_hashmap;

    while (num_current_threads != 0)
    {
      //chosen_index = dist(gen) % num_current_threads;
      //chosen_interpreter = interpreter_stack[chosen_index];
      chosen_interpreter = interpreter_stack.front();
      finished = chosen_interpreter->run_step();
      std::cout << "Size: \n" << interpreter_stack.size() << std::endl;
      if (finished)
      {
        std::cout << "Finished thread\n" << std::endl;
        num_current_threads--;
      }
      
    }

  }

  void start(trieste::Node main_body, int step_counter)
  {
    auto ui = rt::ui::globalUI();
    if (ui->is_mermaid())
    {
      reinterpret_cast<rt::ui::MermaidUI*>(ui)->set_step_counter(step_counter);
    }

    size_t initial = rt::pre_run(ui);

    const size_t max_threads{2};
    std::vector<Interpreter*> interpreter_stack;
    for (size_t i = 0; i < max_threads; i++)
    {
      interpreter_stack.push_back(new Interpreter(ui));
    }
    

    interpreter_stack.front()->push_stack_frame(main_body);

    std::mt19937 gen(42);
    std::uniform_int_distribution<int> dist(0, 100);
    Interpreter* chosen_interpreter;
    //size_t chosen_index;

    size_t num_current_threads{1};
    bool finished = false;


    while (num_current_threads != 0)
    {
      //chosen_index = dist(gen) % num_current_threads;
      //chosen_interpreter = interpreter_stack[chosen_index];
      chosen_interpreter = interpreter_stack.front();
      finished = chosen_interpreter->run_step();
      std::cout << "Size: \n" << interpreter_stack.size() << std::endl;
      if (finished)
      {
        std::cout << "Finished thread\n" << std::endl;
        num_current_threads--;
      }
      
    }
    //BoC_concurrency(interpreter_stack);
  

    //inter.run(main_body);

    rt::post_run(initial, ui);
  }

} // namespace verona::interpreter
