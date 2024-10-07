#pragma once

#include <string>

namespace objects {
class DynObject;

struct Edge {
  DynObject *src;
  std::string key;
  DynObject *target;
};
}
