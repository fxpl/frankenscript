#include "interpreter.h"

#include "../rt/behavior.h"
#include "../rt/rt.h"
#include "bytecode.h"
#include "trieste/trieste.h"

#include <iostream>
#include <optional>
#include <ranges>
#include <variant>
#include <vector>
#include <random>

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
  // Handled by Interpreter
  struct ExecNext
  {};

  // Handled by Interpreter
  struct ExecJump
  {
    trieste::Location target;
  };

  // Handled by Interpreter
  struct ExecFunc
  {
    trieste::Node body;
    size_t arg_ctn;
  };

  // Handled by Interpreter
  struct ExecReturn
  {
    std::optional<rt::objects::DynObject*> value;
  };

  // Scheduler
  struct ExecPrint
  {
    std::string value;
  };

  // Scheduler?
  struct ExecSchedule
  {
    std::string value;
  };
  // Scheduler

  struct ExecBreakpoint
  {
    std::string value;
  };
  using AllCommandsVariant = std::variant<ExecNext, ExecJump, ExecFunc, ExecReturn, ExecPrint, ExecSchedule, ExecBreakpoint>;
  using Subaction_variant = std::variant<ExecPrint, ExecSchedule, ExecBreakpoint>;

  struct ExecInScheduler
  {
    Subaction_variant action;
    bool exec_complete;
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
    rt::core::behavior_ptr behavior;

    InterpreterFrame* top_frame()
    {
      if (frame_stack.empty())
      {
        return nullptr;
      }
      else
      {
        return frame_stack.back();
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
      return frame;
    }

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

    AllCommandsVariant
    run_stmt(trieste::Node& node)
    {
      // ==========================================
      // Operators that shouldn't be printed
      // ==========================================
      if (node == Print)
      {
        auto message = std::string(node->location().view());

        // Mermaid output
        //ui->output(message);

        return ExecPrint{message};
      }
      if (node == Label)
      {
        return ExecNext{};
      }

      // ==========================================
      // Operators that should be printed
      // ==========================================
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

        if (!v)
        {
          std::stringstream ss;
          ss << "The name " << field << " is undefined in the current frame";
          rt::ui::error(ss.str(), frame()->object());
        }

        frame()->stack_push(v.value(), "load from frame");
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
          auto builtin = rt::get_builtin(field);
          // Convert ptr -> optional
          if (builtin)
          {
            v = builtin;
          }
        }

        if (!v)
        {
          std::stringstream ss;
          ss << "The name `" << field
             << "` is undefined in the current and global frame";
          rt::ui::error(ss.str(), frame()->object());
        }

        frame()->stack_push(v.value(), "load from global");
        return ExecNext{};
      }

      if (node == StoreFrame)
      {
        if (frame()->get_stack_size() < 1)
        {
          rt::ui::error("Interpreter: The stack is too small");
        }
        auto v = frame()->stack_pop("value to store");
        std::string field{node->location().view()};
        auto v2 = rt::set(frame()->object(), field, v);
        rt::remove_reference(frame()->object(), v2);
        return ExecNext{};
      }

      if (node == SwapFrame)
      {
        if (frame()->get_stack_size() < 1)
        {
          rt::ui::error("Interpreter: The stack is too small");
        }
        auto new_var = frame()->stack_pop("swap value");
        std::string field{node->location().view()};

        auto old_var = rt::set(frame()->object(), field, new_var);
        // RC stays the same
        frame()->stack_push(old_var, "swaped", false);

        return ExecNext{};
      }

      if (node == LoadField)
      {
        if (frame()->get_stack_size() < 2)
        {
          rt::ui::error("Interpreter: The stack is too small");
        }
        auto k = frame()->stack_pop("lookup-key");
        auto v = frame()->stack_pop("lookup-value");

        if (!v)
        {
          std::stringstream ss;
          ss << "Tried to access the field `" << rt::get_key(k)
             << "` on `None`";
          rt::ui::error(ss.str(), nullptr);
        }

        auto v2 = rt::get(v, k);
        if (!v2)
        {
          std::stringstream ss;
          ss << "the field `" << rt::get_key(k) << "` is not defined on " << v;
          rt::ui::error(ss.str(), v);
        }

        frame()->stack_push(v2.value(), "loaded field");
        rt::remove_reference(frame()->object(), k);
        rt::remove_reference(frame()->object(), v);
        return ExecNext{};
      }

      if (node == StoreField)
      {
        if (frame()->get_stack_size() < 3)
        {
          rt::ui::error("Interpreter: The stack is too small");
        }
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
        if (frame()->get_stack_size() < 3)
        {
          rt::ui::error("Interpreter: The stack is too small");
        }
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
          auto is_schedule = rt::is_schedule_builtin(func);
          if (result)
          {
            auto value = result.value();
            frame()->stack_push(value, "result from builtin", false);
          }
          rt::remove_reference(frame()->object(), func);

          if (is_schedule)
          {
            return ExecSchedule{};
          }
          else if(rt::is_breakpoint_builtin(func))
          {
            return ExecBreakpoint{};
          }
          else
          {
            return ExecNext{};
          }
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
        if (dup_idx > stack_size)
        {
          rt::ui::error(
            "Interpreter: the stack is too small for this duplication");
        }

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
    bool prev_breakpoint_call = false;
    Interpreter(
      rt::ui::UI* ui_,
      trieste::Node block,
      std::vector<rt::objects::DynObject*> start_stack,
      rt::core::behavior_ptr behavior_)
    : ui(ui_), behavior(behavior_)
    {
      // There is a question where the active behavior should be set.
      //
      // Python mixes the runtime and interpreter a bit more. There the runtime
      // has access to the interpreter state. So, it would be possible to store
      // the behavior in the interpreter state and have it accessible to cowns.
      //
      // However, in FrankenScript the runtime is more passive, meaning that
      // the interpreter drives the runtime and provides all needed information.
      auto old_behavior = rt::get_active_behavior();
      rt::set_active_behavior(this->behavior);

      auto frame = push_stack_frame(block);

      for (auto elem : start_stack)
      {
        frame->frame->stack_push(elem, "staring stack");
      }

      rt::set_active_behavior(old_behavior);
    }

    // resume() helper
    template<typename... Subset, typename... Superset>
    std::variant<Subset...> narrow_variant(const std::variant<Superset...>& original)
    {
        return std::visit([](auto&& val) -> std::variant<Subset...> {
            using T = std::decay_t<decltype(val)>;
            if constexpr ((std::is_same_v<T, Subset> || ...))
            {
                return val; // allowed type
            }
            else
            {
                throw std::bad_variant_access(); // or handle error
            }
        }, original);
    }
    
    ExecInScheduler
     resume()
    {
      auto return_to_scheduler{false};
      auto frame = top_frame();

      rt::set_active_behavior(this->behavior);
      assert(frame && "Should never exit while-loop");

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
        else if (
          std::holds_alternative<ExecPrint>(action) ||
          std::holds_alternative<ExecSchedule>(action) ||
          std::holds_alternative<ExecBreakpoint>(action))
        {
          frame->ip++;
          return_to_scheduler = true;
        }
        else
        {
          assert(false && "Unsuported operation");
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

          frame = pop_stack_frame();
        }
        auto finished{false};
        if (!frame)
        {
          finished = true;
        }
        if (return_to_scheduler)
        {
          auto sub_action = narrow_variant<ExecPrint, ExecSchedule, ExecBreakpoint>(action);
          return ExecInScheduler{sub_action, finished};
        }

      }

      assert(false && "Should never exit while-loop");
      return ExecInScheduler{};
    }
  };

  void start(trieste::Node main_body, int step_counter, std::string output, bool interactive, int seed, bool prompt_steps)
  {
    auto ui = rt::ui::globalUI();
    ui->set_output_file(output);
    if (ui->is_mermaid())
    {
      reinterpret_cast<rt::ui::MermaidUI*>(ui)->set_step_counter(step_counter);
    }
    Scheduler s;

    size_t initial = rt::pre_run(ui, &s);

    s.start(new Bytecode{main_body}, interactive, seed, prompt_steps);

    rt::post_run(initial, ui);
  }

  Scheduler::Scheduler()
  {
    auto ui = rt::ui::globalUI();
    assert(ui->is_mermaid());
    reinterpret_cast<rt::ui::MermaidUI*>(ui)->scheduler_ready_list =
      &this->ready;
  }

  Scheduler::~Scheduler()
  {
    auto ui = rt::ui::globalUI();
    assert(ui->is_mermaid());
    reinterpret_cast<rt::ui::MermaidUI*>(ui)->scheduler_ready_list = nullptr;
  }

  void Scheduler::add(rt::core::behavior_ptr behavior)
  {
    assert(behavior->status == rt::core::Behavior::Status::New);

    for (auto cown : behavior->cowns)
    {
      // Get the last behavior that is waiting on the cown
      auto cown_info = this->cowns.find(cown);
      if (cown_info != cowns.end())
      {
        auto predecessor = cown_info->second;
        // If a behavior is pending, set the successor
        if (predecessor->status != rt::core::Behavior::Status::Done)
        {
          if (predecessor->succ.insert(behavior).second)
          {
            behavior->pred_ctn += 1;
          }
          // Only needed for Mermaid:
          behavior->cown_deps[cown] = predecessor.get();
        }
      }
      // Update pointer to the last pending behavior
      this->cowns[cown] = behavior;
    }

    std::stringstream ss;
    if (behavior->pred_ctn == 0)
    {
      this->ready.push_back(behavior);
      behavior->status = rt::core::Behavior::Status::Ready;
      ss << "New behavior `" << behavior->get_name() << "` is ready";
    }
    else
    {
      behavior->status = rt::core::Behavior::Status::Pending;
      ss << "New behavior `" << behavior->get_name() << "` is pending";
    }

    // Two solutions:
    // 1. Draw the schedule here and make sure that ExecSchedule doesn't
    //    draw the scheudle
    // 2. Store the message but use it explicitly
    //this->next_schedule_msg = ss.str();
    draw_schedule(ss.str());
    std::cout << "!!! " << "Scheduled `" << behavior->get_name() << "`" << std::endl;
  }

  void Scheduler::start(Bytecode* main_block, bool i, int s, bool prompt_steps)
  {
    auto main_function = rt::make_func(main_block);
    // Hack: Needed to keep the main function alive. Otherwise, it'll be freed
    // thereby also deleting the trieste nodes.
    rt::hack_inc_rc(main_function);
    prompt_user_for_steps = false;
    this->interactive = i;
    this->rng.seed(s);
    this->prompt_user_for_steps = prompt_steps;
    // :notes: I imagine a world without ugly c++ :notes:
    auto behavior = std::make_shared<rt::core::Behavior>(
      main_function, std::vector<rt::objects::DynObject*>{}, "main");
    behavior->status = rt::core::Behavior::Status::Ready;
    this->ready.push_back(behavior);


    while (behavior)
    {
      Interpreter* inter;
      if (behavior->status == rt::core::Behavior::Status::Ready)
      {
        auto block = behavior->spawn();

        inter = new Interpreter(
          rt::ui::globalUI(), block->body, behavior->cowns, behavior);
        this->running[behavior] = inter;
      }
      else if (behavior->status == rt::core::Behavior::Status::Running)
      {
        inter = this->running[behavior];
        assert(inter);
      }
      else
      {
        assert(false && "HOW DID IT BREAK THIS BADLY?");
      }

      this->current_int = inter;
      auto result = inter->resume();
      auto action = result.action;
      auto should_break{false};
      if (std::holds_alternative<ExecPrint>(action))
      {
        auto message = std::get<ExecPrint>(action).value;
        std::cout << ">>> " << message << std::endl;
        if (this->current_int->prev_breakpoint_call)
        {
          this->current_int->prev_breakpoint_call = false;
          std::cout << "!!! " << "Reached breakpoint in " << behavior->get_name() << std::endl;
          should_break = true;
        }
        // TODO whole string
        
        draw_schedule(message);
        if (this->interactive)
        {
          if (steps == 0) 
          {
            should_break = true;
          }
          steps--;
        }
          
      }
      else if (std::holds_alternative<ExecSchedule>(action)){
        // Don't draw since `add()` already did this
        should_break = true;
      }
      else if (std::holds_alternative<ExecBreakpoint>(action))
      {
        // should_break = true;
        // std::stringstream ss_print;
        // ss_print << "Line " << std::get<ExecBreakpoint>(action).value << ":" << std::endl;
        // std::stringstream ss_schedule;
        // ss_schedule  << "Reached breakpoint in " << behavior->get_name() << std::endl;
        // std::stringstream ss_draw;
        // ss_draw << ss_print.str() << ss_schedule.str();
        // draw_schedule(ss_draw.str());
        // std::cout << "<<< " << ss_print.str() << "!!! " << ss_schedule.str();
        // TODO only do this and let ExecPrint handle the rest, since ExecPrint follows
        // the call of breakpoint()
        this->current_int->prev_breakpoint_call = true;
      }
      
      else {}
      //else if (std::holds_alternative<ExecComplete>(action))
      if (result.exec_complete)
      {
        should_break = true;
        std::cout << "!!! " << "Completed " << behavior->get_name() << std::endl;
        this->complete(behavior);
        std::stringstream ss;
        ss << "Completed " << behavior->get_name() << std::endl;
        draw_schedule(ss.str());
      }
      if (this->interactive)
      {
        if (should_break) 
        {
          behavior = this->get_next();
          steps = std::numeric_limits<int>::max();
        }
      }
      // "Always" call if not interactive, otherwise there is no way
      // to seed for certain execution strains.
      // The expection being breakpoints
      else if (!this->current_int->prev_breakpoint_call)
      {
        behavior = this->get_next();
      }
      
      



    }

    rt::remove_reference(nullptr, main_function);
  }

  void print_help()
  {
    std::cout << "Commands:" << std::endl;
    std::cout << "- s <n>: Run n step (default n = 0) [Default]" << std::endl;
    std::cout << "- r    : Runs until the next break point" << std::endl;
    std::cout << "- h    : Prints this message " << std::endl;
  }

  void Scheduler::prompt_steps()
  {
    if (this->first_break)
    {
      print_help();
      first_break = false;
    }
  }

  void Scheduler::complete(rt::core::behavior_ptr behavior)
  {
    behavior->complete();
    std::erase(this->ready, behavior);
    for (auto succ : behavior->succ)
    {
      succ->pred_ctn -= 1;
      if (succ->pred_ctn == 0)
      {
        succ->status = rt::core::Behavior::Status::Ready;
        this->ready.push_back(succ);
      }
    }
    behavior->succ.clear();
  }

  void Scheduler::draw_schedule(std::string message)
  {
    if (this->next_schedule_msg)
    {
      message = this->next_schedule_msg.value();
      this->next_schedule_msg.reset();
    }

    // FIXME: We should really get wrid of the UI* abstraction. There is no way
    // that we'll ever change the output at this point and it just makes several
    // things harder, like this:
    auto ui = rt::ui::globalUI();
    assert(ui->is_mermaid());
    auto mermaid = reinterpret_cast<rt::ui::MermaidUI*>(ui);
    mermaid->output(message);
    // TODO where do we want to close file, ergo cut off output?
    //mermaid->close_file();
  }
  
  rt::core::behavior_ptr Scheduler::get_next()
  {
    if (this->ready.empty())
    {
      return nullptr;
    }

    size_t selected;
    if (this->interactive)
    {
      while (true)
      {
      // Prompt the user:
          std::cout << std::endl;
          std::cout << "Available behaviors:" << std::endl;
          for (unsigned int idx = 0; idx < this->ready.size(); idx += 1)
          {
              auto b = this->ready[idx];
              std::cout << "- " << idx << ": " << b->get_name();
      
              if (b->status == rt::core::Behavior::Status::Running)
              {
                  std::cout << " (continue)";
              }
              std::cout << std::endl;
          }
      
          // Get user input
          std::cout << "> ";
          std::string line;
          std::getline(std::cin, line);
      
          // Check for quit
          if (line == "q")
          {
              exit(0);
          }
      
          // Check for step command
          if (line.size() > 1 && line[0] == 's')
          {
              size_t comma_pos = line.find(',');
              if (comma_pos != std::string::npos)
              {
                  std::string idx_str = line.substr(1, comma_pos - 1);
                  std::string count_str = line.substr(comma_pos + 1);
      
                  size_t idx = 0, count = 0;
                  std::istringstream idx_iss(idx_str);
                  std::istringstream count_iss(count_str);
      
                  if (idx_iss >> idx && count_iss >> count && idx < this->ready.size())
                  {
                      selected = idx;
                      steps = count;
                      //std::cout << "\nStepping behavior " << idx << " for " << count << " times." << std::endl;
                      break;
                  }
              }
          }
          else
          {
            steps = std::numeric_limits<int>::max();
            // Check for Enter press
            if (this->ready.size() == 1 && line == "")
            {
              selected = 0;
              break;
            }
            // Handle normal selection
            std::istringstream iss(line);
            size_t n = 0;
            if (iss >> n && n < this->ready.size())
            {
              selected = n;
              break;
            }
          }
        // if (prompt_user_for_steps)
        // {
        //   prompt_steps();
        // }
        // else
        // {
        //   steps = std::numeric_limits<int>::max();
        // }

      }
    }
    else
    {
      // TODO 
      std::uniform_int_distribution<int> dist(0, this->ready.size() - 1);
      selected = dist(rng);
    }

    auto behavior = this->ready[selected];
    return behavior;
  }

} // namespace verona::interpreter
