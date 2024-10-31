#include "../core.h"
#include "../rt.h"

namespace rt::core
{
  rt::objects::DynObject*
  pop(std::vector<rt::objects::DynObject*>* stack, char const* data_info)
  {
    auto v = stack->back();
    stack->pop_back();
    std::cout << "pop " << v << " (" << data_info << ")" << std::endl;
    return v;
  }

  void mermaid_builtins(ui::UI* ui)
  {
    if (!ui->is_mermaid())
    {
      return;
    }
    auto mermaid = reinterpret_cast<ui::MermaidUI*>(ui);

    add_builtin("mermaid_hide", [mermaid](auto frame, auto stack, auto args) {
      assert(args >= 1);

      for (int i = 0; i < args; i++)
      {
        auto value = stack->back();
        mermaid->add_always_hide(value);
        rt::remove_reference(frame, value);
        stack->pop_back();
      }

      return std::nullopt;
    });

    add_builtin("mermaid_show", [mermaid](auto frame, auto stack, auto args) {
      assert(args >= 1);

      for (int i = 0; i < args; i++)
      {
        auto value = stack->back();
        mermaid->remove_unreachable_hide(value);
        mermaid->remove_always_hide(value);
        rt::remove_reference(frame, value);
        stack->pop_back();
      }

      return std::nullopt;
    });

    add_builtin("mermaid_show_all", [mermaid](auto, auto, auto args) {
      assert(args == 0);

      mermaid->unreachable_hide.clear();
      mermaid->always_hide.clear();

      return std::nullopt;
    });

    add_builtin(
      "mermaid_show_tainted", [mermaid](auto frame, auto stack, auto args) {
        assert(args >= 1);

        std::vector<rt::objects::DynObject*> taint;
        for (int i = 0; i < args; i++)
        {
          auto value = stack->back();
          mermaid->add_taint(value);
          taint.push_back(value);
          rt::remove_reference(frame, value);
          stack->pop_back();
        }

        // Mermaid output
        std::vector<rt::objects::DynObject*> roots{frame};
        mermaid->output(roots, "Builtin: display taint");

        for (auto tainted : taint)
        {
          mermaid->remove_taint(tainted);
        }

        return std::nullopt;
      });

    add_builtin("mermaid_taint", [mermaid](auto frame, auto stack, auto args) {
      assert(args >= 1);

      for (int i = 0; i < args; i++)
      {
        auto value = stack->back();
        mermaid->add_taint(value);
        rt::remove_reference(frame, value);
        stack->pop_back();
      }

      return std::nullopt;
    });

    add_builtin(
      "mermaid_untaint", [mermaid](auto frame, auto stack, auto args) {
        assert(args >= 1);

        for (int i = 0; i < args; i++)
        {
          auto value = stack->back();
          mermaid->remove_taint(value);
          rt::remove_reference(frame, value);
          stack->pop_back();
        }

        return std::nullopt;
      });

    add_builtin("mermaid_show_cown_region", [mermaid](auto, auto, auto args) {
      assert(args == 0);
      mermaid->show_cown_region();
      return std::nullopt;
    });
    add_builtin("mermaid_hide_cown_region", [mermaid](auto, auto, auto args) {
      assert(args == 0);
      mermaid->hide_cown_region();
      return std::nullopt;
    });

    add_builtin("mermaid_show_functions", [mermaid](auto, auto, auto args) {
      assert(args == 0);
      mermaid->show_functions();
      return std::nullopt;
    });
    add_builtin("mermaid_hide_functions", [mermaid](auto, auto, auto args) {
      assert(args == 0);
      mermaid->hide_functions();
      return std::nullopt;
    });

    add_builtin("breakpoint", [mermaid](auto, auto, auto args) {
      assert(args == 0);

      mermaid->break_next();

      return std::nullopt;
    });
  }

  void ctor_builtins()
  {
    add_builtin("cown", [](auto frame, auto stack, auto args) {
      assert(args == 1);

      auto region = pop(stack, "region for cown creation");
      auto cown = make_cown(region);
      rt::move_reference(frame, cown, region);

      return cown;
    });

    add_builtin("create", [](auto frame, auto stack, auto args) {
      assert(args == 1);

      auto obj = make_object();
      // RC transferred
      rt::set_prototype(obj, pop(stack, "prototype source"));

      return obj;
    });
  }

  void action_builtins()
  {
    add_builtin("freeze", [](auto frame, auto stack, auto args) {
      assert(args == 1);

      auto value = pop(stack, "object to freeze");
      freeze(value);
      rt::remove_reference(frame, value);

      return std::nullopt;
    });

    add_builtin("freeze_proto", [](auto frame, auto stack, auto args) {
      assert(args == 1);

      auto value = pop(stack, "object to freeze the prototype");
      if (value && value->get_prototype())
      {
        freeze(value->get_prototype());
      }
      rt::remove_reference(frame, value);

      return std::nullopt;
    });

    add_builtin("Region", [](auto frame, auto stack, auto args) {
      assert(args == 0);

      auto value = rt::create_region();
      return value;
    });

    add_builtin("unreachable", [](auto, auto, auto) {
      ui::error("this method should never be called");
      return std::nullopt;
    });
  }

  void init_builtins(ui::UI* ui)
  {
    mermaid_builtins(ui);
    ctor_builtins();
    action_builtins();
  }
}
