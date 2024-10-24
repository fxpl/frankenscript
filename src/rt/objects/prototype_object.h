
#include "dyn_object.h"

namespace rt::objects
{
  class PrototypeObject : public objects::DynObject
  {
    std::string name;

  public:
    PrototypeObject(std::string name_, objects::DynObject* prototype = nullptr)
    : objects::DynObject(prototype), name(name_)
    {}

    std::string get_name()
    {
      std::stringstream stream;
      stream << "[" << name << "]";
      return stream.str();
    }
  };
}