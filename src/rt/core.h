#include "objects/prototype_object.h"
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

  class FrameObject : public objects::DynObject
  {
    FrameObject() : objects::DynObject(framePrototypeObject()) {}

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
  public:
    CownObject(objects::DynObject* region)
    : objects::DynObject(cownPrototypeObject(), objects::cown_region)
    {
      // FIXME: Add once regions are reified
      // assert(
      //   region->get_prototype() == regionPrototype() &&
      //   "Cowns can only store regions");
      //
      // FIXME: Also check that the region has a LRC == 1, with 1
      // being the reference passed into this constructor

      // this->set would fail, since this is a cown
      this->fields["value"] = region;
    }

    std::string get_name() override
    {
      return "<cown>";
    }

    objects::DynObject* is_primitive() override
    {
      return this;
    }

    bool is_opaque() override
    {
      // For now there is no mechanism to aquire the cown, it'll therefore
      // always be opaque.
      return true;
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
