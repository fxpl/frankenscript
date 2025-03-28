#include "../behavior.h"

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
  int Behavior::s_behavior_counter = 0;
  std::shared_ptr<Behavior> Behavior::s_active_behavior = nullptr;

  void Behavior::set_active_behavior(std::shared_ptr<Behavior> active)
  {
    s_active_behavior = active;
    if (active)
    {
      objects::set_local_region(active->local_region);
    }
  }

  std::shared_ptr<Behavior> Behavior::get_active_behavior()
  {
    return s_active_behavior;
  }

  Behavior::Behavior(
    rt::objects::DynObject* code_,
    std::vector<rt::objects::DynObject*> cowns_,
    std::optional<std::string> name_)
  : id(s_behavior_counter++), cowns(cowns_), code(code_)
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
      ss << "Behavior_" << id;
      name = ss.str();
    }
  }

  std::string Behavior::get_name()
  {
    return this->name;
  }

  std::string Behavior::id_str()
  {
    std::stringstream ss;
    ss << "B" << this->id;
    return ss.str();
  }

  verona::interpreter::Bytecode* Behavior::spawn()
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

  // FIXME: Currently both the scheduler and the behavior has a function
  // to complete a behavior. All of this should really be in one place. It
  // might be better to move all of this into the scheduler.
  void Behavior::complete()
  {
    this->status = Status::Done;
    rt::remove_reference(nullptr, this->code);
    this->code = nullptr;

    for (auto c : this->cowns)
    {
      rt::release_cown(c);
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
