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
  int Behaviour::s_behaviour_counter = 0;
  std::shared_ptr<Behaviour> Behaviour::s_active_behaviour = nullptr;

  void Behaviour::set_active_behaviour(std::shared_ptr<Behaviour> active)
  {
    s_active_behaviour = active;
    if (active)
    {
      objects::set_local_region(active->local_region);
    }
  }

  std::shared_ptr<Behaviour> Behaviour::get_active_behaviour()
  {
    return s_active_behaviour;
  }

  Behaviour::Behaviour(
    rt::objects::DynObject* code_,
    std::vector<rt::objects::DynObject*> cowns_,
    std::optional<std::string> name_)
  : id(s_behaviour_counter++), cowns(cowns_), code(code_)
  {
    for (auto c : cowns)
    {
      ordered_cown[rt::get_cown_id(c)] = c;
    }

    if (name_)
    {
      name = name_.value();
    }
    else
    {
      std::stringstream ss;
      ss << "Behaviour_" << id;
      name = ss.str();
    }
  }

  std::string Behaviour::get_name()
  {
    return this->name;
  }

  std::string Behaviour::id_str()
  {
    std::stringstream ss;
    ss << "B" << this->id;
    return ss.str();
  }

  verona::interpreter::Bytecode* Behaviour::spawn()
  {
    assert(this->status == Status::Ready);
    this->status = Status::Running;

    for (auto c : this->cowns)
    {
      rt::aquire_cown(c, this);
    }

    this->local_region = objects::Region::new_local_region();

    return rt::try_get_bytecode(this->code).value();
  }

  // FIXME: Currently both the scheduler and the behaviour has a function
  // to complete a behaviour. All of this should really be in one place. It
  // might be better to move all of this into the scheduler.
  void Behaviour::complete()
  {
    this->status = Status::Done;
    rt::remove_reference(nullptr, this->code);
    this->code = nullptr;

    for (auto c : this->cowns)
    {
      if (rt::is_owner(c, this))
      {
        rt::release_cown(c, this);  
      }
      rt::remove_reference(nullptr, c);
    }
    this->cowns.clear();

    for (auto [cown, waiting_on] : cown_deps)
    {
      if (waiting_on == this)
      {
        cown_deps.erase(cown);
      }
    }
  }
} // namespace rt::core
