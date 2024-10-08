#pragma once

#include "objects/visit.h"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace rt::ui
{
  class UI
  {
  public:
    virtual void output(std::vector<objects::Edge>&, std::string) {}
  };

  void mermaid(std::vector<objects::Edge>& roots, std::ostream& out);

  [[noreturn]] inline void error(const std::string& msg)
  {
    std::cerr << "Error: " << msg << std::endl;
    std::exit(1);
  }
} // namespace rt::ui
