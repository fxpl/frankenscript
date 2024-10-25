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

    virtual bool is_mermaid() = 0;
  };
}

namespace rt::core
{
  void mermaid_builtins(ui::UI* ui);
}

namespace rt::ui
{
  class MermaidDiagram;

  class MermaidUI : public UI
  {
    friend class MermaidDiagram;
    friend void core::mermaid_builtins(ui::UI* ui);

    /// @brief Indicates if this is the first break and the help message should
    /// be printed.
    bool first_break = true;
    /// @brief Indicates how many steps should be taken until entering
    /// interactive mode again.
    int steps;
    std::string path;
    std::ofstream out;

    /// Only visible if they're reachable.
    ///
    /// Defaults to `rt::core::globals()`
    std::set<objects::DynObject*> unreachable_hide;
    /// Nodes that should never be visible.
    std::set<objects::DynObject*> always_hide;
    /// @brief Nodes that should be tainted, meaning they and all reachable
    /// nodes are highlighted.
    std::set<rt::objects::DynObject*> taint;

  public:
    MermaidUI(int step_counter);

    void output(std::vector<objects::DynObject*>& roots, std::string message);

    void next_action();

    void break_next()
    {
      steps = 0;
    }

    bool should_break()
    {
      return steps == 0;
    }

    bool is_mermaid()
    {
      return true;
    }

    void add_unreachable_hide(objects::DynObject* obj)
    {
      unreachable_hide.insert(obj);
    }

    void remove_unreachable_hide(objects::DynObject* obj)
    {
      unreachable_hide.erase(obj);
    }

    void add_always_hide(objects::DynObject* obj)
    {
      always_hide.insert(obj);
    }

    void remove_always_hide(objects::DynObject* obj)
    {
      always_hide.erase(obj);
    }

    void add_taint(objects::DynObject* obj)
    {
      taint.insert(obj);
    }

    void remove_taint(objects::DynObject* obj)
    {
      taint.erase(obj);
    }
  };

  [[noreturn]] inline void error(const std::string& msg)
  {
    std::cerr << "Error: " << msg << std::endl;
    std::exit(1);
  }
} // namespace rt::ui
