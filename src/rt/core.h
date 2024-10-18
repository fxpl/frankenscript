#include "objects/dyn_object.h"
#include "rt.h"

namespace rt::core
{
  class PrototypeObject : public objects::DynObject
  {
    std::string name;

  public:
    PrototypeObject(std::string name_, objects::DynObject* prototype = nullptr)
    : objects::DynObject(prototype), name(name_)
    {}

    std::string get_name()
    {
      std::stringstream stream;
      stream << "[" << name << "]";
      return stream.str();
    }
  };

  inline PrototypeObject* framePrototypeObject()
  {
    static PrototypeObject* proto = new PrototypeObject("Frame");
    return proto;
  }

  class FrameObject : public objects::DynObject
  {
    FrameObject() : objects::DynObject(framePrototypeObject(), true) {}

  public:
    FrameObject(objects::DynObject* parent_frame)
    : objects::DynObject(framePrototypeObject())
    {
      if (parent_frame)
      {
        auto old_value = this->set(objects::ParentField, parent_frame);
        add_reference(this, parent_frame);
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

  class FuncObject : public objects::DynObject
  {
  public:
    FuncObject(objects::DynObject* prototype_, bool global = false)
    : objects::DynObject(prototype_, global)
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

    std::string get_name()
    {
      std::stringstream stream;
      stream << "\"" << value << "\"";
      return stream.str();
    }

    std::string as_key()
    {
      return value;
    }

    objects::DynObject* is_primitive()
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

    std::string get_name()
    {
      return "<iterator>";
    }

    objects::DynObject* is_primitive()
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
    // For now always false, but might be needed later if we want to simulate
    // concurrency.
    bool aquired = false;

  public:
    CownObject(objects::DynObject* region)
    : objects::DynObject(cownPrototypeObject()),
    {
      // FIXME: Add once regions are reified
      // assert(
      //   region->get_prototype() == regionPrototype() &&
      //   "Cowns can only store regions");
      //
      // FIXME: Also check that the region has a LRC == 1, with 1
      // being the reference passed into this constructor
      this->set("region", region);
    }

    bool is_aquired()
    {
      return aquired;
    }

    std::string get_name()
    {
      return "<cown>";
    }

    objects::DynObject* is_primitive()
    {
      return this;
    }

    bool is_cown() override
    {
      return false;
    }
  };

  inline std::set<objects::DynObject*>* globals()
  {
    static std::set<objects::DynObject*>* globals =
      new std::set<objects::DynObject*>{
        framePrototypeObject(),
        funcPrototypeObject(),
        bytecodeFuncPrototypeObject(),
        stringPrototypeObject(),
        keyIterPrototypeObject(),
        trueObject(),
        falseObject(),
        cownPrototypeObject(),
      };
    return globals;
  }
} // namespace rt::core
