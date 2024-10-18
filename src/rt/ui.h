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
    virtual void output(std::vector<objects::DynObject*>&, std::string) {}
  };

  void mermaid(std::vector<objects::DynObject*>& roots, std::ostream& out, std::vector<objects::DynObject*>* taint = nullptr);

  [[noreturn]] inline void error(const std::string& msg)
  {
    std::cerr << "Error: " << msg << std::endl;
    std::exit(1);
  }
} // namespace rt::ui
