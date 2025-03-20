#pragma once

#include <cstddef>



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
    public:
    // Technically only schedule() needs to be visible in builtin.cc
    virtual void start(Bytecode* main_body, rt::ui::UI* ui) = 0;
    virtual void schedule() = 0;

  };

  class Behaviour
  {
  // TODO make public attributes private
  private:
  public:
    trieste::Node thunk;
    size_t count;
    std::vector<rt::objects::DynObject*> cowns;
    Behaviour(trieste::Node thunk_) : thunk(thunk_) {}
    Behaviour(trieste::Node t, std::vector<rt::objects::DynObject*> c)
      {
        this->thunk = t;
        this->count = c.size();
        this->cowns = c;
      }

  };

  class BocScheduler: public Scheduler {
    std::vector<Behaviour*> behaviours;
    std::vector<Behaviour*> ready_behaviours;

    void update_ready_behaviours()
    {
      return;
    }
    void mark_as_done(size_t step)
    {
      this->ready_behaviours.erase(ready_behaviours.begin() + step);
    }
    public:
    void start(Bytecode* main_body, rt::ui::UI* ui) override{
      //auto ui = rt::ui::globalUI();
      // Run main execution
        verona::interpreter::Interpreter* main_inter = new Interpreter(ui);
        main_inter->run(main_body*);

        // Handle any resulting behaviours 
        while (behaviours.size() != 0) {
          // 1. Check dependency graph
          // 2. Get ready behaviours
          this->update_ready_behaviours();
          
          auto step = rand() & ready_behaviours.size();
          
          auto behaviour = ready_behaviours[step];
          // TODO need new constructor
          auto inter = new Interpreter(behaviour->thunk, ui);
          //rt::move_reference(NULL, inter->frame()->object(), behaviour);
          for (size_t i = 0; i < behaviour->cowns.size(); i++)
          {
            // TODO RC add should be false?
            inter->frame()->stack_push(behaviour->cowns[i], "cown object", false);
          }
          inter->run(NULL);

          this->mark_as_done(step);
          // Removes behaviour from this->behaviours
          // Updates dependency graph
        }
      }
      void schedule() override
      {
        return;
      }
  };




}
