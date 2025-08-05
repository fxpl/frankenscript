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
  class ConcurrentEntity
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
      Blocked,
      Waiting,
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
        case Status::Blocked:
          return "Blocked";
        case Status::Waiting:
          return "Waiting";
        case Status::Done:
          return "Done";
        default:
          return "Unknown";
      }
    }

    static void set_active_entity(std::shared_ptr<ConcurrentEntity>);
    static std::shared_ptr<ConcurrentEntity> get_active_entity();

  private:
    static std::shared_ptr<ConcurrentEntity> s_active_behaviour;
    // Static members for naming
    static int s_behaviour_counter;
    static int s_thread_counter;

    // A unique ID, this is used for drawing and naming, it isn't needed for
    // scheduling.
    int id;
    std::string name;
    // Used for drawing
    static int s_entity_counter;

    // The IDs of the cowns this behaviour is waiting on. This is used to create
    // a better mermaid diagram, it isn't needed for scheduling.
    std::map<int, objects::DynObject*> ordered_cown;

    // The local region of this behaviour. This has to be swapped into the
    // global `local_region` when this behaviour runs.
    objects::Region* local_region;

  public:
    // Only needed for threads
    bool is_behaviour;
    objects::DynObject* bridge;
    // Only needed for threads

    // This maps the cowns of this behaviour to the previous behaviour this
    // is waiting on. This is used to draw the dependencies, it is not used
    // for sceduling.
    // Both of these pointers are weak reference.
    std::map<objects::DynObject*, ConcurrentEntity*> cown_deps;

    Status status;
    // The args as they were passed in to the behaviour. These have to be
    // provided to the new Interpreter to populate the frame.
    std::vector<objects::DynObject*> args;
    // Cowns that are created owing to the entitys frankenscript code
    std::vector<objects::DynObject*> created_cowns;
    // This uses a function object opposed to a Bytecode* to not leak memory
    objects::DynObject* code;
    // The number of behaviours that this behaviour is waiting on
    int pred_ctn = 0;
    // Number of cowns behaviour is waiting on
    size_t cown_ctn{0};
    // ConcurrentEntitys which are waiting on this behaviour. These will be
    // notified once this behaviour completes. Currently needed for mermaid
    std::set<std::shared_ptr<ConcurrentEntity>> succ;
    // Map from a cown to the next behaviour waiting for it, needed since all
    // cowns are not necessarily released at the end of entities.
    std::map<objects::DynObject*, std::shared_ptr<ConcurrentEntity>> cown_succ;

    ConcurrentEntity(
      objects::DynObject* code_,
      std::vector<objects::DynObject*> cowns_,
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

  typedef std::shared_ptr<ConcurrentEntity> entity_ptr;
} // namespace rt::core
