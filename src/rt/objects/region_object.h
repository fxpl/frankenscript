#pragma once

#include "prototype_object.h"
#include "region.h"

namespace rt::objects
{
  // The prototype object for region entry point objects
  inline PrototypeObject* regionPrototypeObject()
  {
    static PrototypeObject* proto =
      new PrototypeObject("RegionObject", nullptr, objects::immutable_region);
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
      // Happens if an existing region has been frozen
      if (region == immutable_region)
      {
        stream << this << std::endl;
        stream << "lrc=<frozen>" << std::endl;
        stream << "sbrc=<frozen>";
      }
      // False if region was at some point merged with another
      // Merged regions should not display 'lrc' nor 'sbrc'
      else if (this != region->bridge)
      {
        stream << this;
      }
      else
      {
        stream << this << std::endl;
        stream << "lrc=" << region->local_reference_count
               << (region->is_lrc_dirty ? " (dirty)" : "") << std::endl;
        stream << "sbrc=" << region->sub_region_reference_count;
      }
      return stream.str();
    }
  };
}
