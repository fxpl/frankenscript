#pragma once

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
    friend class rt::ui::ScheduleDiagram;

  public:
    enum class Status
    {
      New,
      Pending,
      Ready,
      Running,
      Done,
    };

    static std::string status_to_string(Status status)
    {
      switch (status)
      {
        case Status::New:
          return "New";
        case Status::Pending:
          return "Pending";
        case Status::Ready:
          return "Ready";
        case Status::Running:
          return "Running";
        case Status::Done:
          return "Done";
        default:
          return "Unknown";
      }
    }

    Status status;

  private:
    // Static member for naming
    static int s_behavior_counter;

    // A unique ID, this is used for drawing and naming, it isn't needed for
    // scheduling.
    int id;

    // The IDs of the cowns this behavior is waiting on. This is used to create
    // a better mermaid diagram, it isn't needed for scheduling.
    std::map<int, rt::objects::DynObject*> ordered_cown;

  public:
    // The cowns as they were passed in to the cown. These have to be provided
    // to the new Interpreter to populate the frame
    std::vector<rt::objects::DynObject*> cowns;
    // This uses a function object opposed to a Bytecode* to not leak memory
    rt::objects::DynObject* code;
    // The number of behaviors that this behavior is waiting on
    int pred_ctn = 0;
    // Behaviors which are waiting on this behavior. These will be notified once
    // this behavior completes
    std::set<std::shared_ptr<Behavior>> succ;

    Behavior(
      rt::objects::DynObject* code_,
      std::vector<rt::objects::DynObject*> cowns_);

    std::string name();
    std::string id_str();

    Bytecode* spawn();
    // This completes the behavior by releasing all cowns
    // decreffing all held objects and informing its successors.
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
    std::unordered_map<rt::objects::DynObject*, std::shared_ptr<Behavior>>
      cowns = {};

  public:
    void add(std::shared_ptr<Behavior> behavior);

    void start(Bytecode* main);

  private:
    void complete(std::shared_ptr<Behavior> behavior);
    void draw_scedule(std::string message);
    std::shared_ptr<Behavior> get_next();
  };
}
