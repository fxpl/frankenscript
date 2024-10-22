#pragma once

#include "objects/visit.h"

#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace rt::ui
{
  class UI
  {
  public:
    virtual void output(std::vector<objects::DynObject*>&, std::string) {}
  };

  class MermaidDiagram;
  class MermaidUI : public UI
  {
    friend class MermaidDiagram;

    bool interactive;
    std::string path;
    std::ofstream out;

    /// Only visible if they're reachable.
    ///
    /// Defaults to `rt::core::globals()`
    std::set<objects::DynObject*> unreachable_hide;
    /// Nodes that should never be visible.
    std::set<objects::DynObject*> always_hide;
    /// Nodes who's reachability should be highlighted.
    std::set<objects::DynObject*> taint;

  public:
    MermaidUI(bool interactive);

    void
    output(std::vector<rt::objects::DynObject*>& roots, std::string message);
  };

  [[noreturn]] inline void error(const std::string& msg)
  {
    std::cerr << "Error: " << msg << std::endl;
    std::exit(1);
  }
} // namespace rt::ui
