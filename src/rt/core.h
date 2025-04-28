#include "../lang/interpreter.h"
#include "behaviour.h"
#include "objects/prototype_object.h"
#include "objects/region.h"
#include "objects/region_object.h"
#include "rt.h"

#include <map>

namespace rt::core
{
  const std::string BoC_schedule_func_name = "spawn_behaviour";
  const std::string thread_schedule_func_name = "spawn_thread";
  const std::string breakpoint_func_name = "breakpoint";

  using PrototypeObject = objects::PrototypeObject;

  inline PrototypeObject* framePrototypeObject()
  {
    static PrototypeObject* proto = new PrototypeObject("Frame");
    return proto;
  }

  class FrameObject : public objects::DynObject,
                      public verona::interpreter::FrameObj
  {
    static int s_frame_id_counter;
    static constexpr std::string_view STACK_PREFIX = "_stack";
    static inline thread_local std::vector<std::string> stack_keys;
    size_t stack_size = 0;

    FrameObject() : objects::DynObject(framePrototypeObject()) {}

    std::string& stack_name(size_t idx)
    {
      if (idx == stack_keys.size())
      {
        std::stringstream ss;
        ss << STACK_PREFIX << "[" << idx << "]";
        stack_keys.push_back(ss.str());
      }

      return stack_keys[idx];
    }

  public:
    FrameObject(objects::DynObject* parent_frame)
    : objects::DynObject(framePrototypeObject())
    {
      if (parent_frame)
      {
        auto old_value = this->set(objects::ParentField, parent_frame);
        objects::add_reference(this, parent_frame);
        assert(!old_value);
      }

      std::stringstream ss;
      ss << "<Frame " << s_frame_id_counter++ << ">";
      name = ss.str();
    }

    static FrameObject* create_first_stack()
    {
      return new FrameObject();
    }

    rt::objects::DynObject* object()
    {
      return this;
    }

    void stack_push(
      rt::objects::DynObject* value, const char* info, bool rc_add = true)
    {
      auto old = this->set(stack_name(stack_size), value);
      assert(old == nullptr && "the stack already had a value");
      stack_size += 1;

      if (rc_add)
      {
        rt::add_reference(this, value);
      }
    }

    rt::objects::DynObject* stack_pop(char const* info)
    {
      stack_size -= 1;
      auto value = erase(stack_name(stack_size));
      return value;
    }

    size_t get_stack_size()
    {
      return stack_size;
    }

    rt::objects::DynObject* stack_get(size_t index)
    {
      return get(stack_name(index)).value();
    }
  };

  inline PrototypeObject* funcPrototypeObject()
  {
    static PrototypeObject* proto = new PrototypeObject("Function");
    return proto;
  }

  inline PrototypeObject* bytecodeFuncPrototypeObject()
  {
    static PrototypeObject* proto =
      new PrototypeObject("BytecodeFunction", funcPrototypeObject());
    return proto;
  }

  inline PrototypeObject* builtinFuncPrototypeObject()
  {
    static PrototypeObject* proto =
      new PrototypeObject("BuiltinFunction", funcPrototypeObject());
    return proto;
  }

  class FuncObject : public objects::DynObject
  {
  public:
    FuncObject(objects::DynObject* prototype_) : objects::DynObject(prototype_)
    {}
  };

  class BytecodeFuncObject : public FuncObject
  {
    verona::interpreter::Bytecode* body;

  public:
    BytecodeFuncObject(verona::interpreter::Bytecode* body_)
    : FuncObject(bytecodeFuncPrototypeObject()), body(body_)
    {}

    ~BytecodeFuncObject()
    {
      verona::interpreter::delete_bytecode(this->body);
      this->body = nullptr;
    }

    verona::interpreter::Bytecode* get_bytecode()
    {
      return this->body;
    }
  };

  class BuiltinFuncObject : public FuncObject
  {
    BuiltinFuncPtr func;

  public:
    BuiltinFuncObject(BuiltinFuncPtr func_)
    : FuncObject(builtinFuncPrototypeObject()), func(func_)
    {}

    BuiltinFuncPtr get_func()
    {
      return func;
    }
  };

  inline PrototypeObject* stringPrototypeObject()
  {
    static PrototypeObject* proto = new PrototypeObject("String");
    return proto;
  }

  class StringObject : public objects::DynObject
  {
    std::string value;

  public:
    StringObject(
      std::string value_,
      objects::Region* region = rt::objects::get_local_region())
    : objects::DynObject(stringPrototypeObject(), region), value(value_)
    {}

    std::string get_name() override
    {
      std::stringstream stream;
      stream << "\"" << value << "\"";
      return stream.str();
    }

    std::string as_key()
    {
      return value;
    }

    objects::DynObject* is_primitive() override
    {
      return this;
    }
  };

  inline StringObject* trueObject()
  {
    static StringObject* val =
      new StringObject("True", objects::immutable_region);
    return val;
  }

  inline StringObject* falseObject()
  {
    static StringObject* val =
      new StringObject("False", objects::immutable_region);
    return val;
  }

  // The prototype object for iterators
  inline PrototypeObject* keyIterPrototypeObject()
  {
    static PrototypeObject* proto = new PrototypeObject("KeyIterator");
    return proto;
  }

  class KeyIterObject : public objects::DynObject
  {
    std::map<std::string, objects::DynObject*>::iterator iter;
    std::map<std::string, objects::DynObject*>::iterator iter_end;

  public:
    KeyIterObject(std::map<std::string, objects::DynObject*>& fields)
    : objects::DynObject(keyIterPrototypeObject()),
      iter(fields.begin()),
      iter_end(fields.end())
    {}

    objects::DynObject* iter_next()
    {
      objects::DynObject* obj = nullptr;
      if (this->iter != this->iter_end)
      {
        obj = make_str(this->iter->first);
        this->iter++;
      }

      return obj;
    }

    std::string get_name() override
    {
      return "<iterator>";
    }

    objects::DynObject* is_primitive() override
    {
      return this;
    }
  };

  // The prototype object for cown
  inline PrototypeObject* cownPrototypeObject()
  {
    static PrototypeObject* proto = new PrototypeObject("Cown");
    return proto;
  }

  class CownObject : public objects::DynObject
  {
  private:
    static int s_id_counter;

    enum class Status
    {
      Pending,
      Released,
      Acquired
    };

    static std::string to_string(Status status)
    {
      switch (status)
      {
        case Status::Pending:
          return "Pending";
        case Status::Released:
          return "Released";
        case Status::Acquired:
          return "Acquired";
        default:
          return "Unknown";
      }
    }

    Status status;
    int id;
    core::Behaviour* owner;

  public:
    CownObject(
      objects::DynObject* obj, std::optional<std::string> name_ = std::nullopt)
    : objects::DynObject(cownPrototypeObject(), objects::cown_region)
    {
      id = s_id_counter++;

      status = Status::Pending;
      this->owner = Behaviour::get_active_behaviour().get();
      auto old = set("value", obj);
      assert(!old);

      // This is really wonky. The scheduler should actually know about this
      // new cown, but meh?
      if (this->status == Status::Pending)
      {
        this->change_rc(1);
        this->owner->signal_new_cown(this);
        //verona::interpreter::Scheduler::new_pending_cown(this, this->owner);
      }

      if (name_)
      {
        name = name_.value();
      }
      else
      {
        std::stringstream ss;
        ss << "<cown " << this->id << ">";
        name = ss.str();
      }
    }

    [[nodiscard]] DynObject* set(std::string name, DynObject* obj) override
    {
      assert_modifiable();

      if (obj && !obj->is_immutable() && !obj->is_cown())
      {
        auto region = objects::get_region(obj);
        // Potentiall error message
        std::stringstream ss;
        ss << "Object is neither immutable nor a cown, attempted to threat it "
              "as a bridge but..."
           << std::endl;
        if (region->bridge != obj)
        {
          ss << obj << " is not the bridge object of the region";
          ui::error(ss.str(), obj);
        }

        if (region->parent != nullptr)
        {
          ss << "A cown can only be created from a free region" << std::endl;
          ss << "| " << obj << " is currently a subregion of "
             << region->parent->bridge;
          ui::error(ss.str(), {this, "", obj});
        }

        region->cown = this;
      }

      DynObject* old = fields[name];
      fields[name] = obj;

      if (old && !old->is_immutable() && !old->is_cown())
      {
        auto old_reg = objects::get_region(old);
        assert(old_reg->cown == this);
        old_reg->cown = nullptr;
      }

      update_status();

      return old;
    }

    // A unique cown ID
    int get_id()
    {
      return this->id;
    }

    // TODO: This should really be split into `get_name()` just getting the name
    // and `get_info()` or the additional info text like lrc and status
    std::optional<std::string> get_additional_info() override
    {
      std::stringstream ss;
      ss << "status=" << to_string(status);
      if (status == Status::Pending || status == Status::Acquired)
      {
        assert(this->owner);
        ss << " (" << this->owner->get_name() << ")";
      }
      return ss.str();
    }

    objects::DynObject* is_primitive() override
    {
      return this;
    }

    bool is_opaque() override
    {
      switch (status)
      {
        // Acquired would usually also check the current thread ID
        // but this is single threaded
        case Status::Acquired:
        case Status::Pending:
          return Behaviour::get_active_behaviour().get() != this->owner;
        case Status::Released:
        default:
          return true;
      }
    }

    bool is_released()
    {
      return this->status == Status::Released;
    }

    /// This function updates the status of the cown. It mainly checks if a
    /// cown in the pending state can be released.
    void update_status()
    {
      if (status != Status::Pending)
      {
        return;
      }

      auto value = this->get("value").value();
      if (!value || value->is_immutable() || value->is_cown())
      {
        status = Status::Released;
        this->owner = nullptr;
        return;
      }

      auto region = objects::get_region(value);
      if (region->combined_lrc() == 0)
      {
        status = Status::Released;
        this->owner->signal_early_release(this);
        this->owner = nullptr;
      }
    }

    void aquire(Behaviour* behaviour)
    {
      // Who needs other safety checks than this?
      // This is so gonna bite me...
      assert(this->status == Status::Released);

      this->status = Status::Acquired;
      this->owner = behaviour;
    }

    void release(Behaviour* behaviour)
    {
      assert(this->owner == behaviour);
      this->status = Status::Released;
      this->owner = nullptr;
    }
    bool is_owner(Behaviour* behaviour)
    {
      return this->owner == behaviour;
    }
  };

  inline std::set<objects::DynObject*>* globals()
  {
    static std::set<objects::DynObject*>* globals =
      new std::set<objects::DynObject*>{
        objects::regionPrototypeObject(),
        framePrototypeObject(),
        funcPrototypeObject(),
        bytecodeFuncPrototypeObject(),
        builtinFuncPrototypeObject(),
        stringPrototypeObject(),
        keyIterPrototypeObject(),
        cownPrototypeObject(),
        trueObject(),
        falseObject(),
      };
    return globals;
  }

  inline std::set<objects::DynObject*>* global_prototypes()
  {
    static std::set<objects::DynObject*>* globals =
      new std::set<objects::DynObject*>{
        objects::regionPrototypeObject(),
        framePrototypeObject(),
        funcPrototypeObject(),
        bytecodeFuncPrototypeObject(),
        builtinFuncPrototypeObject(),
        stringPrototypeObject(),
        keyIterPrototypeObject(),
        cownPrototypeObject(),
      };
    return globals;
  }

  inline std::map<std::string, objects::DynObject*>* global_names()
  {
    static std::map<std::string, objects::DynObject*>* global_names =
      new std::map<std::string, objects::DynObject*>{
        {"True", trueObject()},
        {"False", falseObject()},
      };
    return global_names;
  }

  /// @brief Initilizes builtin functions and adds them to the global namespace.
  ///
  /// @param ui The UI to allow builtin functions to create output, when they're
  /// called.
  void init_builtins(ui::UI* ui, verona::interpreter::Scheduler* scheduler);
} // namespace rt::core
