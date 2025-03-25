#pragma once

#include <cstddef>
#include <vector>



namespace rt::objects
{
  class DynObject;
} // namespace rt::objects

namespace rt::ui
{
  class UI;
}

namespace verona::interpreter
{
  struct Bytecode;

  void delete_bytecode(Bytecode* bytecode);

  class Interpreter;

  class Behaviour
  {
  // TODO make public attributes private
  private:
  public:
    Bytecode* thunk;
    size_t count;
    std::vector<rt::objects::DynObject*> cowns;
    //Behaviour(Bytecode thunk_) : thunk(thunk_) {}
    Behaviour(Bytecode* t, std::vector<rt::objects::DynObject*> c)
    {
      this->thunk = t;
      this->count = c.size();
      this->cowns = c;
    };
  };

  class Scheduler
  {
    public:
    // Technically only schedule() needs to be visible in builtin.cc
    virtual void start(Bytecode* main_body, rt::ui::UI* ui) = 0;
    // TODO arg should permit either Behaviour or thread
    virtual void schedule(Behaviour* b) = 0;

  };
  class BocScheduler : public Scheduler {
    private:
        std::vector<Behaviour*> behaviours;
        std::vector<Behaviour*> ready_behaviours;
    
        void update_ready_behaviours();
        void mark_as_done(size_t step);
    
    public:
        void start(Bytecode* main_body, rt::ui::UI* ui) override;
        void schedule(Behaviour* b) override;
    };


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










}
