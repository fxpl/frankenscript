#pragma once

#include "../rt/behavior.h"

#include <cstddef>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

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
    // All behaviors that are ready to run
    std::vector<rt::core::behavior_ptr> ready = {};
    // A map from cowns to the last behavior that is waiting on them.
    //
    // The cowns in the key are weak pointers, they should never be
    // dereferenced.
    std::unordered_map<rt::objects::DynObject*, rt::core::behavior_ptr> cowns =
      {};
    // This feels hacky but also like the best solution? I can't even blame this
    // on C++
    std::unordered_map<rt::core::behavior_ptr, Interpreter*> running = {};

    // FIXME: To not pause twice for a new behavior (schedule::Add) and
    // inter->pause() we'll store a message here for the next
    // draw scedule.
    // TO be clear, this is super duper hacky and shouldn't be done
    // like this.
    std::optional<std::string> next_schedule_msg;

    // FIXME:
    // This should likely be gotten by requesting the current
    // behavior in the runtime and then looking up the interpreter
    // from the behavior. But no, this is faster;
    Interpreter* current_int;

    // @brief Do we desire interactive execution
    bool interactive{true};
    /// @brief Indicates if this is the first break and the help message should
    /// be printed.
    bool first_break{true};
    /// @brief Indicates how many steps should be taken until
    /// prompting user again, in the case of interactive exec
    size_t steps{0};
    // For faster debugging
    bool prompt_user_for_steps{true};

    // @brief seed for concurrency
    int seed{42};

  public:
    Scheduler();
    ~Scheduler();

    void add(rt::core::behavior_ptr behavior);

    void start(Bytecode* main_block, bool interactive, int seed, bool no_steps);

    // void new_pending_cown(rt::objects::DynObject* cown, rt::core::behavior_ptr behavior);
    // void pending_cown_released(rt::objects::DynObject* cown, rt::core::behavior_ptr behavior);

  private:
    void prompt_steps();
    void complete(rt::core::behavior_ptr behavior);
    void draw_schedule(std::string message);
    rt::core::behavior_ptr get_next();
  };
}
