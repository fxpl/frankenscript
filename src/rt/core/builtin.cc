#include "../core.h"
#include "../rt.h"

namespace rt::core
{
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
        remove_reference(frame, value);
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
        remove_reference(frame, value);
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
  }

  void init_builtins(ui::UI* ui)
  {
    mermaid_builtins(ui);
  }
}
