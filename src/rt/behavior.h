#pragma once

#include <map>
#include <memory>
#include <set>
#include <vector>

namespace verona::interpreter
{
  struct Bytecode;
}

namespace rt::objects
{
  class DynObject;
} // namespace rt::objects

namespace rt::ui
{
  class ScheduleDiagram;
}

namespace rt::core
{
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
    std::map<int, objects::DynObject*> ordered_cown;

  public:
    // The cowns as they were passed in to the cown. These have to be provided
    // to the new Interpreter to populate the frame
    std::vector<objects::DynObject*> cowns;
    // This uses a function object opposed to a Bytecode* to not leak memory
    objects::DynObject* code;
    // The number of behaviors that this behavior is waiting on
    int pred_ctn = 0;
    // Behaviors which are waiting on this behavior. These will be notified once
    // this behavior completes
    std::set<std::shared_ptr<Behavior>> succ;

    Behavior(
      objects::DynObject* code_, std::vector<objects::DynObject*> cowns_);

    std::string name();
    std::string id_str();

    verona::interpreter::Bytecode* spawn();
    // This completes the behavior by releasing all cowns
    // decreffing all held objects
    void complete();
  };

  typedef std::shared_ptr<Behavior> behavior_ptr;
} // namespace rt::core
