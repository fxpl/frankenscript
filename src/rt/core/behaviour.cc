#include "../behaviour.h"

#include "../objects/region.h"
#include "../rt.h"

#include <iostream>
#include <sstream>

namespace rt::objects
{
  void set_local_region(Region* region);
}

namespace rt::core
{
  int ConcurrentEntity::s_behaviour_counter = 0;
  int ConcurrentEntity::s_thread_counter = 0;
  int ConcurrentEntity::s_entity_counter = 0;
  std::shared_ptr<ConcurrentEntity> ConcurrentEntity::s_active_behaviour =
    nullptr;

  void
  ConcurrentEntity::set_active_entity(std::shared_ptr<ConcurrentEntity> active)
  {
    s_active_behaviour = active;
    if (active)
    {
      objects::set_local_region(active->local_region);
    }
  }

  std::shared_ptr<ConcurrentEntity> ConcurrentEntity::get_active_entity()
  {
    return s_active_behaviour;
  }

  ConcurrentEntity::ConcurrentEntity(
    rt::objects::DynObject* code_,
    std::vector<rt::objects::DynObject*> args_,
    std::optional<std::string> name_,
    bool is_behaviour_,
    rt::objects::DynObject* bridge_)
  : args(args_), code(code_), is_behaviour(is_behaviour_), bridge(bridge_)
  {
    this->status = Status::New;
    this->local_region = objects::Region::new_local_region();
    id = ++ConcurrentEntity::s_entity_counter;
    int naming_id;

    if (is_behaviour)
    {
      naming_id = ++ConcurrentEntity::s_behaviour_counter;
      for (auto c : args)
      {
        ordered_cown[rt::get_cown_id(c)] = c;
      }
    }
    else
      naming_id = ++rt::core::ConcurrentEntity::s_thread_counter;

    if (name_)
    {
      name = name_.value();
    }
    else
    {
      std::stringstream ss;
      if (is_behaviour)
        ss << "Behaviour_" << naming_id;
      else
        ss << "Thread_" << naming_id;
      name = ss.str();
    }
  }

  std::string ConcurrentEntity::get_name()
  {
    return this->name;
  }

  std::string ConcurrentEntity::id_str()
  {
    std::stringstream ss;
    if (is_behaviour)
      ss << "B";
    else
      ss << "T";
    ss << this->id;
    return ss.str();
  }

  verona::interpreter::Bytecode* ConcurrentEntity::spawn()
  {
    assert(this->status == Status::Ready);
    this->status = Status::Running;

    if (is_behaviour)
    {
      for (auto c : this->args)
      {
        rt::aquire_cown(c, this);
      }
    }

    auto test = rt::try_get_bytecode(this->code).value();
    return test;
  }

  // FIXME: Currently both the scheduler and the behaviour has a function to
  // complete a behaviour. All of this should really be in one place. might be
  // better to move all of this into the scheduler.
  void ConcurrentEntity::complete()
  {
    if (this->is_behaviour)
    {
      for (auto c : this->args)
      {
        assert(rt::is_owner(c, this));
        rt::release_cown(c, this);
        rt::remove_reference(nullptr, c);
      }

      for (auto c : this->created_cowns)
      {
        // Entity isn't necessarily still the owner
        if (rt::is_owner(c, this))
        {
          rt::release_cown(c, this);
        }
        rt::remove_reference(nullptr, c);
      }
    }
    else
    {
      for (auto c : this->created_cowns)
      {
        // Entity isn't necessarily still the owner
        if (rt::is_owner(c, this))
        {
          // Thread may have locked cown
          rt::try_release_created_cown(c);
        }
        rt::remove_reference(nullptr, c);
      }
    }
    this->args.clear();

    for (auto [cown, waiting_on] : cown_deps)
    {
      if (waiting_on == this)
      {
        cown_deps.erase(cown);
      }
    }
  }

  void ConcurrentEntity::signal_new_cown(rt::objects::DynObject* cown)
  {
    this->created_cowns.push_back(cown);
    // this->scheduler->signal_new_cown(cown, s_active_behaviour);
  }

} // namespace rt::core
