#pragma once

#include <cstddef>
#include <memory>
#include <unordered_map>
#include <vector>

namespace rt::objects
{
  class DynObject;
} // namespace rt::objects

namespace verona::interpreter
{
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

  class Behavior
  {
    // Static member for naming
    static int s_behavior_counter;

    int id;

  public:
    // Instance members to describe the behavior
    std::vector<rt::objects::DynObject*> cowns;
    // This uses a function object opposed to a Bytecode* to not leak memory
    rt::objects::DynObject* code;
    // The number of behaviors that this behavior is waiting on
    int pred_ctn = 0;
    std::shared_ptr<Behavior> succ = nullptr;
    bool is_complete = false;

    Behavior(
      rt::objects::DynObject* code_,
      std::vector<rt::objects::DynObject*> cowns_)
    : cowns(cowns_), code(code_)
    {
      id = s_behavior_counter;
      s_behavior_counter += 1;
    }

    std::string name();

    Bytecode* spawn();
    // This completes the behavior by releasing all cowns
    // decreffing all held objects and informing its successor.
    void complete();
  };

  class Scheduler
  {
    // All behaviors that are ready to run
    std::vector<std::shared_ptr<Behavior>> ready = {};
    // A map from cowns to the last behavior that is waiting on them.
    //
    // The cowns in the key are weak pointers, they should never be
    // dereferenced.
    std::unordered_map<uintptr_t, std::shared_ptr<Behavior>> cowns = {};

  public:
    void add(std::shared_ptr<Behavior> behavior);

    void start(Bytecode* main);

  private:
    std::shared_ptr<Behavior> get_next();
  };
}
