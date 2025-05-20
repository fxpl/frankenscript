#pragma once

#include "../rt/behaviour.h"

#include <cstddef>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>
#include <random>

namespace rt::objects
{
  class DynObject;
} // namespace rt::objects

namespace verona::interpreter
{
  // Move Interpreter def here (Including Exec* structs)
  class Interpreter;
  struct Bytecode;

  void delete_bytecode(Bytecode* bytecode);

  class FrameObj
  {
  public:
    virtual rt::objects::DynObject* object() = 0;

    /// This pushes the given value onto the stack and increases the RC unless
    /// `rc_add` is set to false
    virtual void stack_push(
      rt::objects::DynObject* value, const char* info, bool rc_add = true) = 0;
    /// This pops the value from the stack. The RC change has to be done by the
    /// caller.
    virtual rt::objects::DynObject* stack_pop(char const* info) = 0;
    virtual size_t get_stack_size() = 0;
    virtual rt::objects::DynObject* stack_get(size_t index) = 0;

    virtual bool stack_is_empty()
    {
      return this->get_stack_size() == 0;
    }
  };

  // FIXME: The implementation of this should probably be in a different file...
  class Scheduler
  {
    // All behaviours that are ready to run
    std::vector<rt::core::behaviour_ptr> ready = {};
    // A map from cowns to the last behaviour that will as some point own them.
    // Ergo this will initially map to the creating behaviour
    //
    // The cowns in the key are weak pointers, they should never be
    // dereferenced.
    std::unordered_map<rt::objects::DynObject*, rt::core::behaviour_ptr> cowns =
      {};
    // This feels hacky but also like the best solution? I can't even blame this
    // on C++
    std::unordered_map<rt::core::behaviour_ptr, Interpreter*> running = {};

    // Necessary to print the line and information of a scheduled behaviour in
    // one go, since Call nodes do not store what line they were called from
    // Solely written to by add()
    // Solely read by start()
    std::string next_schedule_msg;

    // FIXME:
    // This should likely be gotten by requesting the current
    // behaviour in the runtime and then looking up the interpreter
    // from the behaviour. But no, this is faster;
    Interpreter* current_int;

    /// @brief Do we desire interactive execution
    bool interactive{true};
    /// @brief Indicates if this is the first break and the help message should
    /// be printed.
    bool first_break{true};
    /// @brief Indicates how many steps should be taken until
    /// prompting user again, in the case of interactive exec
    size_t steps{0};

    /// @brief RNG for concurrency
    std::mt19937 rng;

    // Track completed behaviours, only needed for testing
    // Onus is on user to provide distinct names, if manual
    // ones are used
    std::vector<std::string> completed_behaviours;

    // Allows printing of schedule/breakpoint, needed when proccessing a 'ExecPrint' action.
    // Indicates if the previous action proccessed was of type 'ExecSchedule' or 
    //'ExecBreakpoint', respectively. Necessary, as we can't obtain the information
    // required for a proper print from the trieste 'Call' node. That is, the node that
    // leads the Interpreter to propagate 'ExecSchedule' or 'ExecBreakpoint' to the Scheduler.
    bool prev_schedule_call{false};
    bool prev_breakpoint_call{false};

  public:
    Scheduler();
    ~Scheduler();


    void add(rt::core::behaviour_ptr behaviour);
    void add_thread(Bytecode* target_bytecode, rt::objects::DynObject* bridge);

    void start(Bytecode* main_block, bool interactive, int seed);

    void signal_new_cown(rt::objects::DynObject* cown, rt::core::behaviour_ptr behaviour);
    void pending_cown_released(rt::objects::DynObject* cown, rt::core::behaviour_ptr behaviour);
    //void new_pending_cown(rt::objects::DynObject* cown, rt::core::behaviour_ptr behaviour);
    
    // Used for testing
    bool is_executable(const std::string behaviour_name);
    bool is_complete(const std::string behaviour_name);
    bool is_executable_or_complete(const std::string behaviour_name);

  private:
    size_t prompt_user();
    void complete_behaviour(rt::core::behaviour_ptr behaviour);
    void draw_schedule(std::string message, bool entering_behaviour = false);
    rt::core::behaviour_ptr get_next();
    void handle_exec_print_action(const std::string& line_string, const std::string& name, bool& should_break);
    void step(bool& should_break);
  };
}
