#pragma once

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

namespace verona::interpreter
{
  struct Bytecode;
}

namespace rt::objects
{
  class DynObject;
  struct Region;
} // namespace rt::objects

namespace rt::ui
{
  class MermaidUI;
  class ScheduleDiagram;
  class ObjectGraphDiagram;
}

namespace rt::core
{
  class Behavior
  {
    friend class rt::ui::MermaidUI;
    friend class rt::ui::ScheduleDiagram;
    friend class rt::ui::ObjectGraphDiagram;

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

    static void set_active_behavior(std::shared_ptr<Behavior>);
    static std::shared_ptr<Behavior> get_active_behavior();

  private:
    // This map is a collection of all behaviors that have started running and
    // therefore also have a local region. This is needed here for the lovely
    // mermaid output. This uses an ordered map in the hope that the diagram
    // will keep the same layout every iteration.
    //
    // It uses behavior pointers since it's being updated from inside methods
    // where `this` is a pointer and not a `shared_ptr`. This should be fine
    // since each behavior should call `complete()` before being freed thereby
    // also updating this list.
    static std::map<int, Behavior*> s_running_behaviors;
    static std::shared_ptr<Behavior> s_active_behavior;

  private:
    // Static member for naming
    static int s_behavior_counter;

    // A unique ID, this is used for drawing and naming, it isn't needed for
    // scheduling.
    int id;
    std::string name;

    // The IDs of the cowns this behavior is waiting on. This is used to create
    // a better mermaid diagram, it isn't needed for scheduling.
    std::map<int, objects::DynObject*> ordered_cown;

    // The local region of this behavior. This has to be swapped into the global
    // `local_region` when this behavior runs.
    objects::Region* local_region;

  public:
    Status status;
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
      objects::DynObject* code_,
      std::vector<objects::DynObject*> cowns_,
      std::optional<std::string> name_ = std::nullopt);

    std::string get_name();
    std::string id_str();

    verona::interpreter::Bytecode* spawn();
    // This completes the behavior by releasing all cowns
    // decreffing all held objects
    void complete();
  };

  typedef std::shared_ptr<Behavior> behavior_ptr;
} // namespace rt::core
