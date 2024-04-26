#pragma once

namespace utils
{
  template <typename T>
  class Nop
  {
    public:
    void operator()(T) const { }
  };
}