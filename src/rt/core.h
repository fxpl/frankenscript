#include "../lang/interpreter.h"
#include "objects/prototype_object.h"
#include "objects/region.h"
#include "objects/region_object.h"
#include "rt.h"

#include <map>

namespace rt::core
{
  using PrototypeObject = objects::PrototypeObject;

  inline PrototypeObject* framePrototypeObject()
  {
    static PrototypeObject* proto = new PrototypeObject("Frame");
    return proto;
  }

  class FrameObject : public objects::DynObject,
                      public verona::interpreter::FrameObj
  {
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

      std::cout << "pushed " << value << " (" << info << ")" << std::endl;
      if (rc_add)
      {
        rt::add_reference(this, value);
      }
    }

    rt::objects::DynObject* stack_pop(char const* info)
    {
      stack_size -= 1;
      auto value = erase(stack_name(stack_size));
      std::cout << "poped " << value << " (" << value << ")" << std::endl;
      return value;
    }

    size_t get_stack_size()
    {
      return stack_size;
    }

    rt::objects::DynObject* stack_get(size_t index)
    {
      return get(stack_name(index));
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
    StringObject(std::string value_)
    : objects::DynObject(stringPrototypeObject()), value(value_)
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
    static StringObject* val = new StringObject("True");
    return val;
  }

  inline StringObject* falseObject()
  {
    static StringObject* val = new StringObject("False");
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

  public:
    CownObject(objects::DynObject* obj)
    : objects::DynObject(cownPrototypeObject(), objects::cown_region)
    {
      status = Status::Pending;
      auto region = objects::get_region(obj);

      if (!obj->is_immutable() && !obj->is_cown())
      {
        // TODO: Make sure we're parenting the region and add third state, like
        // pending
        
        // Potentiall error message
        std::stringstream ss;
        ss << "Object is neither immutable nor a cown, attempted to threat it as a bridge but..." << std::endl;
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
      }

      auto old = set("value", obj);
      assert(!old);

      // 1x LRC from the stack
      if (region->local_reference_count == 1)
      {
        status = Status::Released;
      }
    }

    std::string get_name() override
    {
      std::stringstream ss;
      ss << "<cown>" << std::endl;
      ss << "status=" << to_string(status);
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
          return false;
        case Status::Released:
        default:
          return true;
      }
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
  void init_builtins(ui::UI* ui);
} // namespace rt::core
