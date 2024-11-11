#pragma once

#include "prototype_object.h"
#include "region.h"

namespace rt::objects
{
  // The prototype object for region entry point objects
  inline PrototypeObject* regionPrototypeObject()
  {
    static PrototypeObject* proto = new PrototypeObject("RegionObject", nullptr, objects::immutable_region);
    return proto;
  }

  class RegionObject : public DynObject
  {
  public:
    RegionObject(Region* region) : DynObject(regionPrototypeObject(), region) {}

    std::string get_name() override
    {
      auto region = get_region(this);

      std::stringstream stream;
      stream << this << std::endl;

      // Happens if an existing region has been frozen
      if (region == immutable_region)
      {
        stream << "lrc=<frozen>" << std::endl;
        stream << "sbrc=<frozen>";
      }
      else
      {
        stream << "lrc=" << region->local_reference_count
               << (region->is_lrc_dirty ? " (dirty)" : "") << std::endl;
        stream << "sbrc=" << region->sub_region_reference_count;
      }
      return stream.str();
    }
  };
}
