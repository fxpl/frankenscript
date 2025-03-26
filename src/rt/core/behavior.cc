#include "../behavior.h"

#include "../rt.h"

#include <iostream>
#include <sstream>

namespace rt::core
{
  int Behavior::s_behavior_counter = 0;

  Behavior::Behavior(
    rt::objects::DynObject* code_,
    std::vector<rt::objects::DynObject*> cowns_,
    std::optional<std::string> name_)
  : id(s_behavior_counter++), cowns(cowns_), code(code_)
  {
    for (auto c : cowns)
    {
      this->ordered_cown[rt::get_cown_id(c)] = c;
    }

    if (name_)
    {
      this->name = name_.value();
    }
    else
    {
      std::stringstream ss;
      ss << "Behavior_" << this->id;
      this->name = ss.str();
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
      rt::aquire_cown(c);
    }

    return rt::try_get_bytecode(this->code).value();
  }

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
  }
} // namespace rt::core
