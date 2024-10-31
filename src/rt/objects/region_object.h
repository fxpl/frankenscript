#pragma once

#include "prototype_object.h"

namespace rt::objects
{
  // The prototype object for region entry point objects
  inline PrototypeObject* regionObjectPrototypeObject()
  {
    static PrototypeObject* proto = new PrototypeObject("RegionObject");
    return proto;
  }

  class RegionObject : public DynObject
  {
  public:
    RegionObject(Region* region)
    : DynObject(regionObjectPrototypeObject(), region)
    {}
  };
}