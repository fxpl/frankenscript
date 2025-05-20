#include "../behaviour.h"
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

    // Handled in Scheduler
    add_builtin(rt::core::breakpoint_func_name, [mermaid](auto, auto args) {
      
      if (args != 0)
      {
        ui::error("breakpoint() expected 0 arguments");
      }

      //mermaid->break_next();

      return std::nullopt;
    });
  }

  void ctor_builtins()
  {
    add_builtin("Cown", [](auto frame, auto args) {
      if (args < 1 && args > 2)
      {
        ui::error("Cown() expected 1 or 2 arguments");
      }

      objects::DynObject* name = nullptr;
      if (args == 2)
      {
        name = frame->stack_pop("name");
      }

      auto region = frame->stack_pop("region for cown creation");
      auto cown = make_cown(region, name);
      rt::move_reference(frame->object(), cown, region);
      rt::remove_reference(frame->object(), name);

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
    verona::interpreter::FrameObj* frame, size_t args)
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
      region->try_close();
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
      close_function_impl(frame, args);
      return std::nullopt;
    });
    add_builtin("is_closed", [](auto frame, auto args) {
      auto result = close_function_impl(frame, args);
      auto result_obj = rt::get_bool(result);
      // The return will be linked to the frame by the interpreter, but the RC
      // has to be increased here.
      result_obj->change_rc(1);
      return result_obj;
    });
    add_builtin("merge", [](auto frame, auto args) {
      if (args != 2)
      {
        ui::error("merge() expected 2 arguments");
      }

      auto sink = frame->stack_pop("Sink");
      auto src = frame->stack_pop("Source");
      rt::remove_reference(frame->object(), sink);
      rt::remove_reference(frame->object(), src);
      rt::merge_regions(src, sink);
      return std::nullopt;
    });

    add_builtin("dissolve", [](auto frame, auto args) {
      if (args != 1)
      {
        ui::error("dissolve() expected 1 argument");
      }

      auto bridge = frame->stack_pop("region to dissolve");
      rt::dissolve_region(bridge);
      rt::remove_reference(frame->object(), bridge);

      return std::nullopt;
    });

    add_builtin("is_released", [](auto frame, auto args) {
      if (args != 1)
      {
        ui::error("is_released() expected 1 argument");
      }

      auto cown = frame->stack_pop("cown to check");
      auto result = rt::is_cown_released(cown);
      rt::remove_reference(frame->object(), cown);

      auto result_obj = rt::get_bool(result);
      // The return will be linked to the frame by the interpreter, but the RC
      // has to be increased here.
      result_obj->change_rc(1);

      return result_obj;
    });

    add_builtin("print", [](auto frame, auto args) {
      if (args != 1)
      {
        ui::error("print() expected 1 argument");
      }

      auto value = frame->stack_pop("value to print");
      auto name = value->get_name();
      // Lazy way of dealing with stringPrototypeObject
      if (name[0] == '\"')
      {
        name.erase(0, 1);
        name.erase(name.size() - 1);
      }
      std::cout << name << std::endl;
      rt::remove_reference(frame->object(), value);

      return std::nullopt;
    });
  }

  void pragma_builtins()
  {
    add_builtin("pragma_enable_implicit_freezing", [](auto, auto args) {
      if (args != 0)
      {
        ui::error("pragma_enable_implicit_freezing() expected 0 arguments");
      }
      objects::Region::pragma_implicit_freezing = true;
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

  void concurrency_builtins(verona::interpreter::Scheduler* scheduler)
  {

    add_builtin("Thread", [=](auto frame, auto args) {
      if (args < 1)
      {
        ui::error("Thread() expected at least 1 argument");
      }
      
      // args (Stored on the stack in reverse order)
      // -1 since the first argument is the func
      std::vector<objects::DynObject*> kwargs = {};
      for (int i = 0; i < args - 1; i++)
      {
        auto value = frame->stack_pop("arg");
        kwargs.push_back(value);
      }

      // Check for proper args here, or in start() --> in start()

      // func
      auto func = frame->stack_pop("func");
      if(!rt::try_get_bytecode(func))
        ui::error("No valid function provided");
      // Function needs to stay alive even if the 
      // concurrent entity that defined it terminates   
      rt::hack_inc_rc(func);
      freeze(func);   
      // create Thread obj
      auto thread_obj = make_thread(func, kwargs);
      rt::move_reference(frame->object(), thread_obj, func);
      return thread_obj;      

    });

    add_builtin("start", [=](auto frame, auto args) {
      if (args != 1)
      {
        ui::error("start() expected 1 argument");
      }
      auto thread_obj = frame->stack_pop("thread");
      // Is there a proper target func?
      auto target = rt::get(thread_obj, "target");
      if (!target.has_value())
        ui::error("No target", thread_obj);
      auto target_bytecode = rt::try_get_bytecode(target.value());
      if (!target_bytecode.has_value())
        ui::error("Target is not a valid function");
      

      // Can we reference the arguments from a new region without issue?
      auto kwargs = rt::get_thread_args(thread_obj);
      auto bridge = rt::objects::create_region();
      auto count = kwargs.size();
      for (auto arg : kwargs)
      {
        assert(arg);
        // args where pushed first to last 
        std::stringstream ss;
        ss << "arg" << count;
        count--;
        // Ideally we'd instead call set() using the proper identifiers,
        // these would first need to be stored in builtin func 'Thread'   
        auto old_var = rt::set(bridge, ss.str(), arg);
        assert(!old_var);
      }
      auto region = objects::get_region(bridge);
      // Regions are created with an lrc of 1
      if (region->combined_lrc() > 1)
        ui::error("region is not closed", bridge);
      
      scheduler->add(
        std::make_shared<rt::core::Behaviour>(target.value(), kwargs, scheduler, std::nullopt, false, bridge));
      rt::remove_reference(frame->object(), thread_obj);
      return std::nullopt;      

    });

    add_builtin(rt::core::schedule_func_name, [=](auto frame, auto args) {
      // cowns (Stored on the stack in reverse order)
      // -1 since the first argument is the actual behaviour
      std::vector<objects::DynObject*> cowns = {};
      for (int i = 0; i < args - 1; i++)
      {
        auto value = frame->stack_pop("cown");
        cowns.push_back(value);
      }

      std::optional<std::string> name;
      // The last argument might be a name for the behaviour
      if (
        !cowns.empty() &&
        cowns.back()->get_prototype() == rt::core::stringPrototypeObject())
      {
        auto name_obj = cowns.back();
        name = dynamic_cast<rt::core::StringObject*>(name_obj)->as_key();
        rt::remove_reference(frame->object(), name_obj);
        cowns.pop_back();
      }
      // when
      auto behaviour = frame->stack_pop("behaviour");
      scheduler->add(
        std::make_shared<rt::core::Behaviour>(behaviour, cowns, scheduler, name));

      // @Max, Interesting for your report: Some kind of ownership transfer is
      // needed here. Freezing is "the easiest" untill we get into the mess that
      // function objects in cpython are. It could be interesting to see if we
      // can't just transfer ownership to the behaviour region.
      freeze(behaviour);

      return std::nullopt;
    });
    add_builtin("is_executable", [=](auto frame, auto args) {
      
      if (args != 1)
      {
        std::stringstream ss;
        ss << "is_executable" << " expected 1 argument";
        ui::error(ss.str());
      }
  
      auto behaviour_name = frame->stack_pop("behaviour_name");
      if (behaviour_name->get_prototype() != stringPrototypeObject())
      {
        ui::error("given arg is not a string", behaviour_name);
      }
      // Remove string object whitespace
      auto s = behaviour_name->get_name();
      if (s[0] == '\"')
      {
        s.erase(0, 1);
        s.erase(s.size() - 1);
      }
      auto result = scheduler->is_executable(s);
      auto result_obj = rt::get_bool(result);
      result_obj->change_rc(1);
      rt::remove_reference(frame->object(), behaviour_name);
      return result_obj;
    });
    add_builtin("is_complete", [=](auto frame, auto args) {
      
      if (args != 1)
      {
        std::stringstream ss;
        ss << "is_complete" << " expected 1 argument";
        ui::error(ss.str());
      }
  
      auto behaviour_name = frame->stack_pop("behaviour_name");
      if (behaviour_name->get_prototype() != stringPrototypeObject())
      {
        ui::error("given arg is not a string", behaviour_name);
      }
      // Remove string object whitespace
      auto s = behaviour_name->get_name();
      if (s[0] == '\"')
      {
        s.erase(0, 1);
        s.erase(s.size() - 1);
      }
      auto result = scheduler->is_complete(s);
      auto result_obj = rt::get_bool(result);
      result_obj->change_rc(1);
      rt::remove_reference(frame->object(), behaviour_name);
      return result_obj;
    });
    add_builtin("is_executable_or_complete", [=](auto frame, auto args) {
      
      if (args != 1)
      {
        std::stringstream ss;
        ss << "is_executable_or_complete" << " expected 1 argument";
        ui::error(ss.str());
      }
  
      auto behaviour_name = frame->stack_pop("behaviour_name");
      if (behaviour_name->get_prototype() != stringPrototypeObject())
      {
        ui::error("given arg is not a string", behaviour_name);
      }
      // Remove string object whitespace
      auto s = behaviour_name->get_name();
      if (s[0] == '\"')
      {
        s.erase(0, 1);
        s.erase(s.size() - 1);
      }
      auto result = scheduler->is_executable_or_complete(s);
      auto result_obj = rt::get_bool(result);
      result_obj->change_rc(1);
      rt::remove_reference(frame->object(), behaviour_name);
      return result_obj;
    });
  }

  void init_builtins(ui::UI* ui, verona::interpreter::Scheduler* scheduler)
  {
    mermaid_builtins(ui);
    ctor_builtins();
    action_builtins();
    pragma_builtins();
    concurrency_builtins(scheduler);
  }
}
