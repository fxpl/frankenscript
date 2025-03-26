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

namespace rt::ui
{
  class ScheduleDiagram;
}

namespace verona::interpreter
{
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

  public:
    void add(rt::core::behavior_ptr behavior);

    void start(Bytecode* main);

  private:
    void complete(rt::core::behavior_ptr behavior);
    void draw_scedule(std::string message);
    rt::core::behavior_ptr get_next();
  };
}
