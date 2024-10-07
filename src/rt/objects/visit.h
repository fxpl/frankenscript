#pragma once

#include "../../utils/nop.h"

#include <string>

namespace rt::objects
{
  class DynObject;

  struct Edge
  {
    DynObject* src;
    std::string key;
    DynObject* target;
  };

  using NopDO = utils::Nop<DynObject*>;

  template<typename Pre, typename Post = NopDO>
  inline void visit(Edge e, Pre pre, Post post = {});

  template<typename Pre, typename Post = NopDO>
  inline void visit(DynObject* start, Pre pre, Post post = {});
}
