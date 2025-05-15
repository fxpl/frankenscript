#include "interpreter.h"

#include "../rt/behaviour.h"
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


  // Handled by Interpreter #########################################
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

  // Handled by Scheduler #########################################
  
  struct ExecPrint
  {
    std::string value;
  };

  struct ExecSchedule
  {
    std::string value;
  };

  struct ExecBreakpoint
  {
    std::string value;
  };

  struct MainComplete
  {};

  // The set of actions handled by the Interpreter is given by "BasicCommands / SchedulerCommands" 
  using BasicCommands = std::variant<ExecNext, ExecJump, ExecFunc, ExecReturn, ExecPrint, ExecSchedule, ExecBreakpoint>;
  // Set of actions handled by the Scheduler
  using SchedulerCommands = std::variant<ExecPrint, ExecSchedule, ExecBreakpoint, MainComplete>;

  struct ExecInScheduler
  {
    SchedulerCommands action;
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
    rt::core::behaviour_ptr behaviour;

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

    BasicCommands
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

    Interpreter(
      rt::ui::UI* ui_,
      trieste::Node block,
      std::vector<rt::objects::DynObject*> start_stack,
      rt::core::behaviour_ptr behaviour_)
    : ui(ui_), behaviour(behaviour_)
    {
      // There is a question where the active behaviour should be set.
      //
      // Python mixes the runtime and interpreter a bit more. There the runtime
      // has access to the interpreter state. So, it would be possible to store
      // the behaviour in the interpreter state and have it accessible to cowns.
      //
      // However, in FrankenScript the runtime is more passive, meaning that
      // the interpreter drives the runtime and provides all needed information.
      auto old_behaviour = rt::get_active_behaviour();
      rt::set_active_behaviour(this->behaviour);

      auto frame = push_stack_frame(block);

      for (auto elem : start_stack)
      {
        frame->frame->stack_push(elem, "staring stack");
      }

      rt::set_active_behaviour(old_behaviour);
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

      rt::set_active_behaviour(this->behaviour);
      assert(frame);

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
          auto sub_action = narrow_variant<ExecPrint, ExecSchedule, ExecBreakpoint, MainComplete>(action);
          assert(!std::holds_alternative<MainComplete>(sub_action));
          return ExecInScheduler{sub_action, finished};
        }

      }
      // Last instruction in main might not be any of the other three
      return ExecInScheduler{MainComplete{}, true};
    }
  };

  void start(trieste::Node main_body, int step_counter, std::string output, bool interactive, int seed)
  {
    auto ui = rt::ui::globalUI();
    ui->set_output_file(output);
    if (ui->is_mermaid())
    {
      reinterpret_cast<rt::ui::MermaidUI*>(ui)->set_step_counter(step_counter);
    }
    Scheduler s;

    size_t initial = rt::pre_run(ui, &s);

    s.start(new Bytecode{main_body}, interactive, seed);

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

  std::string format_behaviour_name(std::string name)
  {
    std::stringstream ss;
    ss << "`" << name << "`";
    return ss.str();
  }

  void Scheduler::signal_new_cown(rt::objects::DynObject* cown, rt::core::behaviour_ptr behaviour)
  {
    // cown must be new
    assert(this->cowns.find(cown) == this->cowns.end());
    // This slightly complicates the description of the cowns map, since a cown will now initially map to
    // the behaviour that creates it  
    this->cowns[cown] = behaviour;
  }

  void Scheduler::pending_cown_released(rt::objects::DynObject* cown, rt::core::behaviour_ptr behaviour)
  {
    // Is there a successor waiting on the cown 
    auto cown_info = behaviour->cown_succ.find(cown);
    if (cown_info != behaviour->cown_succ.end())
    {
      auto succ = cown_info->second;
      succ->cown_ctn -= 1;
      if (succ->cown_ctn == 0)
      {
        succ->status = rt::core::Behaviour::Status::Ready;
        this->ready.push_back(succ);
      }
      behaviour->cown_succ.erase(cown);
      
    }
  }


  void Scheduler::add(rt::core::behaviour_ptr behaviour)
  {
    assert(behaviour->status == rt::core::Behaviour::Status::New);
    

    for (auto cown : behaviour->cowns)
    {
      // Get the last behaviour that is waiting on the cown
      auto cown_info = this->cowns.find(cown);
      if (cown_info != cowns.end())
      {
        auto predecessor = cown_info->second;
        // If a behaviour isn't Done, set the successor
        if (predecessor->status != rt::core::Behaviour::Status::Done)
        {
          predecessor->cown_succ[cown] = behaviour;
          behaviour->cown_ctn += 1;
          // Only needed for Mermaid:
          if (predecessor->succ.insert(behaviour).second)
          {
            behaviour->pred_ctn += 1;
          }
          behaviour->cown_deps[cown] = predecessor.get();
        }
      }
      // Update pointer to the last pending behaviour
      this->cowns[cown] = behaviour;
    }

    std::stringstream ss;
    if (behaviour->pred_ctn == 0)
    {
      this->ready.push_back(behaviour);
      behaviour->status = rt::core::Behaviour::Status::Ready;
      ss << "New behaviour " << format_behaviour_name(behaviour->get_name()) << " is ready";
    }
    else
    {
      behaviour->status = rt::core::Behaviour::Status::Pending;
      ss << "New behaviour " << format_behaviour_name(behaviour->get_name()) << " is pending";
    }


    this->next_schedule_msg = ss.str();
  }

  void print_help()
  {
    std::cout << "Commands:" << std::endl;
    std::cout << "- <b>      : Run behaviour number b until the next break point" << std::endl;
    std::cout << "- s<b>,<n> : Run behaviour number b n step (default n = 0)" << std::endl;
    std::cout << "- h        : Prints this message " << std::endl << std::endl;
  }


  std::string ellips_block(const std::string& input, const size_t desired_lines) {
    std::istringstream iss(input);
    std::string line;
    std::string throaway_line;
    
    // Split input into lines
    size_t i{0};
    std::string result = "";
    while (i < desired_lines && std::getline(iss, line)) 
    {
      result += line + "\n";
      i++;
    }
    if (std::getline(iss, line))
    {
      std::string indentation;
      for (char c : line)
      {
          if (c == ' ' || c == '\t')
            indentation += c;
          else
              break;
      }
      result += indentation + "[...]\n";
    }
    
    return result;
  }

  void Scheduler::handle_exec_print_action(
    const std::string& line_string,
    const std::string& name,
    bool& should_break)
  {
      auto shortened_string = ellips_block(line_string, 4);
      std::stringstream draw_ss;
      std::stringstream terminal_ss;

      draw_ss << shortened_string;
      terminal_ss << ">>> " << draw_ss.str();

      if (this->prev_schedule_call)
      {
          this->prev_schedule_call = false;
          should_break = true;
          draw_ss << this->next_schedule_msg << std::endl;
          terminal_ss << "!!! Scheduled new behaviour" << std::endl;
      }
      else if (this->prev_breakpoint_call)
      {
          this->prev_breakpoint_call = false;
          should_break = true;
          draw_ss << "Reached breakpoint in " << name << std::endl;
          terminal_ss << "!!! Reached breakpoint in " << name << std::endl;
      }

      std::cout << terminal_ss.str();
      draw_schedule(draw_ss.str());
  }

  void Scheduler::step(bool& should_break)
  {
    if (this->interactive)
    {
      if (steps == 0) 
      {
        should_break = true;
      }
      steps--;
    }
    else
    {
      should_break = true;
    }
  }

  void Scheduler::start(Bytecode* main_block, bool interactive_arg, int seed_arg)
  {
    auto main_function = rt::make_func(main_block);
    // Hack: Needed to keep the main function alive. Otherwise, it'll be freed
    // thereby also deleting the trieste nodes.
    rt::hack_inc_rc(main_function);
    this->interactive = interactive_arg;
    this->rng.seed(seed_arg);
    auto behaviour = std::make_shared<rt::core::Behaviour>(
      main_function, std::vector<rt::objects::DynObject*>{}, this, "main");
    behaviour->status = rt::core::Behaviour::Status::Ready;
    this->ready.push_back(behaviour);

    if (this->interactive)
    {
      print_help();
    }

    while (behaviour)
    {
      Interpreter* inter;
      if (behaviour->status == rt::core::Behaviour::Status::Ready)
      {
        auto block = behaviour->spawn();

        inter = new Interpreter(
          rt::ui::globalUI(), block->body, behaviour->cowns, behaviour);
        this->running[behaviour] = inter;
      }
      else if (behaviour->status == rt::core::Behaviour::Status::Running)
      {
        inter = this->running[behaviour];
        assert(inter);
      }
      else
      {
        assert(false && "HOW DID IT BREAK THIS BADLY?");
      }

      this->current_int = inter;
      auto result = inter->resume();
      auto action = result.action;
      bool should_break{false};
      if (std::holds_alternative<ExecPrint>(action))
      {
        handle_exec_print_action(std::get<ExecPrint>(action).value, 
        format_behaviour_name(behaviour->get_name()),
        should_break);
        // We only utilize these for printing, thus they should be reset once printing is done
        assert(!this->prev_schedule_call && !this->prev_breakpoint_call);
        step(should_break);
      }
      else if (std::holds_alternative<ExecSchedule>(action)){
        this->prev_schedule_call = true;
        // Assumption: Schedule is implemented through a builtin function call
        // Its Call node will always be followed by a Print node 
        assert(!result.exec_complete);
      }
      else if (std::holds_alternative<ExecBreakpoint>(action))
      {
        this->prev_breakpoint_call = true;
        // Assumption: Breakpoint is implemented through a builtin function call
        // Its Call node will always be followed by a Print node 
        assert(!result.exec_complete);
      }
      else if (std::holds_alternative<MainComplete>(action))
      {
        assert(result.exec_complete);
      }
      else 
      {
        assert(false && "Unsuported operation");
      }

      if (result.exec_complete)
      {
        should_break = true;
        this->complete_behaviour(behaviour);
        
        std::stringstream ss;
        ss << "Completed " << format_behaviour_name(behaviour->get_name()) << std::endl;
        std::cout << "!!! " << ss.str();
        draw_schedule(ss.str());
      }
      if (should_break)
      {
        behaviour = this->get_next();
        if (this->interactive && behaviour) 
        {
          std::stringstream ss;
          ss << "Entering behaviour " << format_behaviour_name(behaviour->get_name()) << std::endl;
          std::cout << "!!! " << ss.str();
          draw_schedule(ss.str(), true);
        }
      }
    }

    rt::remove_reference(nullptr, main_function);
  }



  void Scheduler::complete_behaviour(rt::core::behaviour_ptr behaviour)
  {
    behaviour->complete();
    std::erase(this->ready, behaviour);
    for (auto cown_info : behaviour->cown_succ)
    {
      auto succ  = cown_info.second;
      succ->cown_ctn -= 1;
      if (succ->cown_ctn == 0)
      {
        succ->status = rt::core::Behaviour::Status::Ready;
        this->ready.push_back(succ);
      }
    }
    behaviour->cown_succ.clear();

    this->completed_behaviours.push_back(behaviour->get_name());
    
    // for (auto succ : behaviour->succ)
    // {
      //   succ->pred_ctn -= 1;
      //   if (succ->pred_ctn == 0)
      //   {
        //     succ->status = rt::core::Behaviour::Status::Ready;
        //     this->ready.push_back(succ);
        //   }
        // }
    behaviour->succ.clear();
  }

  void Scheduler::draw_schedule(std::string message, bool entering_behaviour)
  {

    // FIXME: We should really get wrid of the UI* abstraction. There is no way
    // that we'll ever change the output at this point and it just makes several
    // things harder, like this:
    auto ui = rt::ui::globalUI();
    assert(ui->is_mermaid());
    auto mermaid = reinterpret_cast<rt::ui::MermaidUI*>(ui);
    if (entering_behaviour)
    {
      mermaid->close_file();
    }
    mermaid->output(message);
  }
  
  size_t Scheduler::prompt_user()
  {
    size_t selected;
    while (true)
    {
    // Prompt the user:
        std::cout << std::endl;
        std::cout << "Available behaviours:" << std::endl;
        for (unsigned int idx = 0; idx < this->ready.size(); idx += 1)
        {
            auto b = this->ready[idx];
            std::cout << "- " << idx << ": " << b->get_name();
    
            if (b->status == rt::core::Behaviour::Status::Running)
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
          auto ui = rt::ui::globalUI();
          assert(ui->is_mermaid());
          auto mermaid = reinterpret_cast<rt::ui::MermaidUI*>(ui);
          mermaid->close_file();
          exit(0);
        }

        if (line == "h")
        {
          print_help();
          continue;
        }
        
    
        // Check for step command
        if (line[0] == 's')
        {
          if (line.size() > 1)
          {
            size_t comma_pos = line.find(',');
            // s<b>, <n>
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
                    //std::cout << "\nStepping behaviour " << idx << " for " << steps << " times." << std::endl;
                    break;
                }
            }
            // s<b>
            std::istringstream iss(line.substr(1));
            size_t n = 0;
            if (iss >> n && n < this->ready.size())
            {
              selected = n;
              steps = 0;
              break;
            }
          }
          // s
          else if (this->ready.size() == 1)
          {
            selected = 0;
            steps = 0;
            break;
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
    }
    return selected;
  }

  rt::core::behaviour_ptr Scheduler::get_next()
  {
    if (this->ready.empty())
    {
      return nullptr;
    }

    size_t selected;
    if (this->interactive)
    {
      selected = prompt_user();
    }
    else
    {
      std::uniform_int_distribution<int> dist(0, this->ready.size() - 1);
      selected = dist(rng);
    }

    auto behaviour = this->ready[selected];
    return behaviour;
  }

  // ################### TESTING FUNCTIONALITY ####################################

  bool Scheduler::is_executable(const std::string behaviour_name)
  {
    for (auto behaviour : this->ready)
    {
      if (behaviour->get_name() == behaviour_name)
      {
        return true;
      }
    }
    return false;
  }

  bool Scheduler::is_complete(const std::string sought_name)
  {
    for (auto behaviour_name : this->completed_behaviours)
    {
      if (behaviour_name == sought_name)
      {
        return true;
      }
    }
    return false;
  }

} // namespace verona::interpreter
