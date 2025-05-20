#pragma once

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>


namespace verona::interpreter
{
  struct Bytecode;
  class Scheduler;
}

namespace rt::objects
{
  class DynObject;
  struct Region;
} // namespace rt::objects

namespace rt::ui
{
  class MermaidUI;
  class ObjectGraphDiagram;
}

namespace rt::core
{
  class Behaviour
  {
    friend class rt::ui::MermaidUI;
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

    static void set_active_behaviour(std::shared_ptr<Behaviour>);
    static std::shared_ptr<Behaviour> get_active_behaviour();
  
  private:
    static std::shared_ptr<Behaviour> s_active_behaviour;
    // Static member for naming
    static int s_behaviour_counter;

    // A unique ID, this is used for drawing and naming, it isn't needed for
    // scheduling.
    int id;
    std::string name;

    // The IDs of the cowns this behaviour is waiting on. This is used to create
    // a better mermaid diagram, it isn't needed for scheduling.
    std::map<int, objects::DynObject*> ordered_cown;

    // The local region of this behaviour. This has to be swapped into the global
    // `local_region` when this behaviour runs.
    objects::Region* local_region;
    // "Necessary" to signal the status change 'Pending --> Released' for cowns to the Scheduler
    // Ergo change is propagated like so: cown->behaviour(owner)->scheduler
    verona::interpreter::Scheduler* scheduler;

  public:
    // Only needed for threads
    bool is_behaviour;
    objects::DynObject* bridge;
    // Only needed for threads

    // This maps the cowns of this behaviour to the previous behaviour this
    // is waiting on. This is used to draw the dependencies, it is not used
    // for sceduling.
    // Both of these pointers are weak reference.
    std::map<objects::DynObject*, Behaviour*> cown_deps;

    Status status;
    // The cowns as they were passed in to the behaviour. These have to be provided
    // to the new Interpreter to populate the frame. Note that creating a 
    // cown in a behaviour will expand its set of cowns. Ergo, the set of aquired
    // cowns is a subset.
    // TODO split created and aquired cowns into two separate structures  
    std::vector<objects::DynObject*> cowns;
    // This uses a function object opposed to a Bytecode* to not leak memory
    objects::DynObject* code;
    // The number of behaviours that this behaviour is waiting on
    int pred_ctn = 0;
    // Number of cowns behaviour is waiting on
    size_t cown_ctn{0};
    // Behaviours which are waiting on this behaviour. These will be notified once
    // this behaviour completes.
    // Currently needed for mermaid
    std::set<std::shared_ptr<Behaviour>> succ;
    // Map from a cown to the next behaviour waiting for it, needed since all cowns are not 
    // necessarily released at the end of behaviours. 
    std::map<objects::DynObject*, std::shared_ptr<Behaviour>> cown_succ;


    Behaviour(
      objects::DynObject* code_,
      std::vector<objects::DynObject*> cowns_,
      verona::interpreter::Scheduler* scheduler_,
      std::optional<std::string> name_ = std::nullopt,
      bool is_behaviour_ = true,
      objects::DynObject* bridge_ = nullptr);

    std::string get_name();
    std::string id_str();

    verona::interpreter::Bytecode* spawn();
    // This completes the behaviour by releasing all cowns
    // decreffing all held objects
    void complete();
    
    void signal_new_cown(rt::objects::DynObject* cown);
    void signal_early_release(rt::objects::DynObject* cown);
  };

  typedef std::shared_ptr<Behaviour> behaviour_ptr;
} // namespace rt::core
