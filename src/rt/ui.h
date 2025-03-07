#pragma once

#include "objects/visit.h"

#include <cassert>
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
    virtual void set_output_file(std::string path_) = 0;

    virtual void output(std::vector<objects::DynObject*>&, std::string) {}

    virtual void highlight(std::string, std::vector<objects::DynObject*>&) {}

    virtual void error(std::string) {}

    virtual void error(std::string, std::vector<objects::DynObject*>&) {}

    virtual void error(std::string, std::vector<objects::Edge>&) {}

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
  public:
    static inline bool pragma_draw_regions_nested = true;
    static inline bool highlight_unreachable = false;

  private:
    friend class MermaidDiagram;
    friend void core::mermaid_builtins(ui::UI* ui);

    /// @brief Indicates if this is the first break and the help message should
    /// be printed.
    bool first_break = true;
    /// @brief Indicates how many steps should be taken until entering
    /// interactive mode again.
    int steps;
    std::string path = "mermaid.md";
    std::ofstream out;

    /// Only visible if they're reachable.
    ///
    /// Defaults to `rt::core::globals()`
    std::set<objects::DynObject*> unreachable_hide;
    /// Nodes that should never be visible.
    std::set<objects::DynObject*> always_hide;
    /// Nodes that should be tainted, meaning they and all reachable
    /// nodes are highlighted.
    std::set<rt::objects::DynObject*> taint;
    /// Indicates if the cown region show be explicitly drawn
    bool draw_cown_region = false;
    bool draw_immutable_region = false;
    /// Indicates if local functions should be visible
    bool draw_funcs = false;

    std::vector<objects::DynObject*> highlight_objects;
    std::vector<objects::DynObject*> error_objects;
    std::vector<objects::Edge> error_edges;

  public:
    MermaidUI();

    void set_output_file(std::string path_) override
    {
      assert(path_.ends_with(".md"));
      this->path = path_;
    }

    void output(
      std::vector<objects::DynObject*>& roots, std::string message) override;

    void highlight(
      std::string message,
      std::vector<objects::DynObject*>& highlight) override;

    void next_action();

    void set_step_counter(int steps_)
    {
      this->steps = steps_;
    }

    void break_next()
    {
      steps = 0;
    }

    bool should_break()
    {
      return steps == 0;
    }

    bool is_mermaid() override
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

    void show_functions()
    {
      draw_funcs = true;
    }

    void hide_functions()
    {
      draw_funcs = false;
    }

    void hide_cown_region();
    void show_cown_region();

    void error(std::string) override;

    void
    error(std::string msg, std::vector<objects::DynObject*>& errors) override
    {
      std::swap(error_objects, errors);
      error(msg);
    };

    void error(std::string msg, std::vector<objects::Edge>& errors) override
    {
      std::swap(error_edges, errors);
      error(msg);
    };

  private:
    /// Uses the local region to construct a reasonable set of roots, used for
    /// UI methods that don't start from the local frame.
    std::vector<objects::DynObject*> local_root_objects();
  };

  inline UI* globalUI()
  {
    static UI* ui = new MermaidUI();
    return ui;
  }

  [[noreturn]] inline void error(const std::string& msg)
  {
    globalUI()->error(msg);
    std::exit(1);
  }

  [[noreturn]] inline void
  error(const std::string& msg, std::vector<objects::DynObject*>& errors)
  {
    globalUI()->error(msg, errors);
    std::exit(1);
  }

  [[noreturn]] inline void
  error(const std::string& msg, objects::DynObject* error_node)
  {
    std::vector<objects::DynObject*> errors = {error_node};
    error(msg, errors);
  }

  [[noreturn]] inline void
  error(const std::string& msg, std::vector<objects::Edge>& errors)
  {
    globalUI()->error(msg, errors);
    std::exit(1);
  }

  [[noreturn]] inline void
  error(const std::string& msg, objects::Edge error_edge)
  {
    std::vector<objects::Edge> errors = {error_edge};
    error(msg, errors);
  }

} // namespace rt::ui
