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

    add_builtin("mermaid_hide", [mermaid](auto frame, auto args) {
      if (args < 0)
      {
        ui::error("mermaid_hide() expected >= 1 arguments");
      }

      for (int i = 0; i < args; i++)
      {
        auto value = frame->stack_pop("value to hide");
        mermaid->add_always_hide(value);
        rt::remove_reference(frame->object(), value);
      }

      return std::nullopt;
    });

    add_builtin("mermaid_show", [mermaid](auto frame, auto args) {
      if (args < 0)
      {
        ui::error("mermaid_show() expected >= 1 arguments");
      }

      for (int i = 0; i < args; i++)
      {
        auto value = frame->stack_pop("value to show");
        mermaid->remove_unreachable_hide(value);
        mermaid->remove_always_hide(value);
        rt::remove_reference(frame->object(), value);
      }

      return std::nullopt;
    });

    add_builtin("mermaid_show_all", [mermaid](auto, auto args) {
      if (args != 0)
      {
        ui::error("mermaid_show_all() expected 0 arguments");
      }

      mermaid->unreachable_hide.clear();
      mermaid->always_hide.clear();

      return std::nullopt;
    });

    add_builtin("mermaid_show_tainted", [mermaid](auto frame, auto args) {
      if (args < 1)
      {
        ui::error("mermaid_show_tainted() expected >= 1 arguments");
      }

      std::vector<rt::objects::DynObject*> taint;
      for (int i = 0; i < args; i++)
      {
        auto value = frame->stack_pop("value to taint");
        mermaid->add_taint(value);
        taint.push_back(value);
      }

      // Mermaid output
      std::vector<rt::objects::DynObject*> roots{frame->object()};
      mermaid->output(roots, "Builtin: display taint");

      for (auto tainted : taint)
      {
        mermaid->remove_taint(tainted);
        rt::remove_reference(frame->object(), tainted);
      }

      return std::nullopt;
    });

    add_builtin("mermaid_taint", [mermaid](auto frame, auto args) {
      if (args < 0)
      {
        ui::error("mermaid_taint() expected >= 1 arguments");
      }

      for (int i = 0; i < args; i++)
      {
        auto value = frame->stack_pop("value to taint");
        mermaid->add_taint(value);
        rt::remove_reference(frame->object(), value);
      }

      return std::nullopt;
    });

    add_builtin("mermaid_untaint", [mermaid](auto frame, auto args) {
      if (args < 0)
      {
        ui::error("mermaid_untaint() expected >= 1 arguments");
      }

      for (int i = 0; i < args; i++)
      {
        auto value = frame->stack_pop("value to taint");
        mermaid->remove_taint(value);
        rt::remove_reference(frame->object(), value);
      }

      return std::nullopt;
    });

    add_builtin("mermaid_show_cown_region", [mermaid](auto, auto args) {
      if (args != 0)
      {
        ui::error("mermaid_show_cown_region() expected 0 arguments");
      }
      mermaid->show_cown_region();
      return std::nullopt;
    });
    add_builtin("mermaid_hide_cown_region", [mermaid](auto, auto args) {
      if (args != 0)
      {
        ui::error("mermaid_hide_cown_region() expected 0 arguments");
      }
      mermaid->hide_cown_region();
      return std::nullopt;
    });

    add_builtin("mermaid_show_immutable_region", [mermaid](auto, auto args) {
      if (args != 0)
      {
        ui::error("mermaid_show_immutable_region() expected 0 arguments");
      }
      mermaid->draw_immutable_region = true;
      return std::nullopt;
    });
    add_builtin("mermaid_hide_immutable_region", [mermaid](auto, auto args) {
      if (args != 0)
      {
        ui::error("mermaid_hide_immutable_region() expected 0 arguments");
      }
      mermaid->draw_immutable_region = false;
      return std::nullopt;
    });

    add_builtin("mermaid_show_functions", [mermaid](auto, auto args) {
      if (args != 0)
      {
        ui::error("mermaid_show_functions() expected 0 arguments");
      }
      mermaid->show_functions();
      return std::nullopt;
    });
    add_builtin("mermaid_hide_functions", [mermaid](auto, auto args) {
      if (args != 0)
      {
        ui::error("mermaid_hide_functions() expected 0 arguments");
      }
      mermaid->hide_functions();
      return std::nullopt;
    });

    add_builtin("breakpoint", [mermaid](auto, auto args) {
      if (args != 0)
      {
        ui::error("breakpoint() expected 0 arguments");
      }

      mermaid->break_next();

      return std::nullopt;
    });
  }

  void ctor_builtins()
  {
    add_builtin("cown", [](auto frame, auto args) {
      if (args != 1)
      {
        ui::error("cown() expected 1 argument");
      }

      auto region = frame->stack_pop("region for cown creation");
      auto cown = make_cown(region);
      rt::move_reference(frame->object(), cown, region);

      return cown;
    });

    add_builtin("Region", [](auto frame, auto args) {
      if (args != 0)
      {
        ui::error("Region() expected 0 arguments");
      }

      auto value = rt::create_region();
      return value;
    });

    add_builtin("create", [](auto frame, auto args) {
      if (args != 1)
      {
        ui::error("create() expected 1 argument");
      }

      auto obj = make_object();
      // RC transferred
      rt::set_prototype(obj, frame->stack_pop("prototype source"));

      return obj;
    });
  }

  bool close_function_impl(
    verona::interpreter::FrameObj* frame, size_t args, bool force_close)
  {
    if (args != 1)
    {
      ui::error("close() expected 1 argument");
    }

    auto bridge = frame->stack_pop("region to close");
    auto region = objects::get_region(bridge);
    if (region->bridge != bridge)
    {
      std::stringstream ss;
      ss << bridge << " is not the bridge object of the region";
      ui::error(ss.str(), bridge);
    }

    // The region has an LRC from being on the stack
    if (region->combined_lrc() == 1)
    {
      // The region might be deleted if this was the only pointer, so we can
      // only check that the region is closed, if the rc is 2
      if (bridge->get_rc() == 2)
      {
        rt::remove_reference(frame->object(), bridge);
        assert(region->is_closed());
      }
      else
      {
        rt::remove_reference(frame->object(), bridge);
      }
      // We have to return `true` since the region might be deleted after this
      return true;
    }
    else
    {
      // We have to remove our reference, as it would otherwise break the
      // `is_closed()` check from the forced close
      rt::remove_reference(frame->object(), bridge);
      if (force_close)
      {
        region->close();
      }
      else
      {
        region->try_close();
      }
    }

    return region->is_closed();
  }

  void action_builtins()
  {
    add_builtin("freeze", [](auto frame, auto args) {
      if (args != 1)
      {
        ui::error("freeze() expected 1 argument");
      }

      auto value = frame->stack_pop("object to freeze");
      freeze(value);
      rt::remove_reference(frame->object(), value);

      return std::nullopt;
    });

    add_builtin("unreachable", [](auto, auto) {
      ui::error("unreachable code was called");
      return std::nullopt;
    });
    add_builtin("pass", [](auto, auto args) {
      if (args != 0)
      {
        ui::error("pass() expected 0 arguments");
      }
      return std::nullopt;
    });

    add_builtin("close", [](auto frame, auto args) {
      close_function_impl(frame, args, true);
      return std::nullopt;
    });
    add_builtin("try_close", [](auto frame, auto args) {
      auto result = close_function_impl(frame, args, false);
      auto result_obj = rt::get_bool(result);
      // The return will be linked to the frame by the interpreter, but the RC
      // has to be increased here.
      result_obj->change_rc(1);
      return result_obj;
    });
  }

  void pragma_builtins()
  {
    add_builtin("pragma_disable_implicit_freezing", [](auto, auto args) {
      if (args != 0)
      {
        ui::error("pragma_disable_implicit_freezing() expected 0 arguments");
      }
      objects::Region::pragma_implicit_freezing = false;
      return std::nullopt;
    });

    add_builtin(
      "pragma_mermaid_draw_regions_nested", [](auto frame, auto args) {
        if (args != 1)
        {
          ui::error("pragma_mermaid_draw_regions_nested() expected 1 argument");
        }
        auto value = frame->stack_pop("pragma bool");
        if (value == rt::get_true())
        {
          ui::MermaidUI::pragma_draw_regions_nested = true;
        }
        else if (value == rt::get_false())
        {
          ui::MermaidUI::pragma_draw_regions_nested = false;
        }
        else
        {
          ui::error("given object is not a boolean", value);
        }
        rt::remove_reference(frame->object(), value);

        return std::nullopt;
      });
  }

  void init_builtins(ui::UI* ui)
  {
    mermaid_builtins(ui);
    ctor_builtins();
    action_builtins();
    pragma_builtins();
  }
}
