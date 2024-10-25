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

    add_builtin("region", [](auto frame, auto stack, auto args) {
      assert(args == 1);

      auto value = pop(stack, "region source");
      rt::create_region(value);
      rt::remove_reference(frame, value);

      return std::nullopt;
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
