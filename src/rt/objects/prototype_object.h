#pragma once

#include "dyn_object.h"

namespace rt::objects
{
  class PrototypeObject : public objects::DynObject
  {
    std::string name;

  public:
    PrototypeObject(
      std::string name_,
      objects::DynObject* prototype = nullptr,
      objects::Region* region = objects::immutable_region)
    : objects::DynObject(prototype, region), name(name_)
    {}

    std::string get_name()
    {
      std::stringstream stream;
      stream << "[" << name << "]";
      return stream.str();
    }
  };
}