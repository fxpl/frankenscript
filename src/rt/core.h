#include "objects/dyn_object.h"
#include "rt.h"

namespace rt::core
{

  class PrototypeObject : public objects::DynObject
  {
    std::string name;

  public:
    PrototypeObject(std::string name_, objects::DynObject* prototype = nullptr)
    : objects::DynObject(prototype, true), name(name_)
    {}

    std::string get_name()
    {
      std::stringstream stream;
      stream << "[" << name << "]";
      return stream.str();
    }
  };

  PrototypeObject framePrototypeObject{"Frame"};

  class FrameObject : public objects::DynObject
  {
    FrameObject() : objects::DynObject(&framePrototypeObject, true) {}

  public:
    FrameObject(objects::DynObject* parent_frame)
    : objects::DynObject(&framePrototypeObject)
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

  PrototypeObject funcPrototypeObject{"Function"};
  PrototypeObject bytecodeFuncPrototypeObject{
    "BytecodeFunction", &funcPrototypeObject};

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
    : FuncObject(&bytecodeFuncPrototypeObject), body(body_)
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

  PrototypeObject stringPrototypeObject{"String"};

  class StringObject : public objects::DynObject
  {
    std::string value;

  public:
    StringObject(std::string value_, bool global = false)
    : objects::DynObject(&stringPrototypeObject, global), value(value_)
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

  StringObject TrueObject{"True", true};
  StringObject FalseObject{"False", true};

  // The prototype object for iterators
  // TODO put some stuff in here?
  PrototypeObject keyIterPrototypeObject{"KeyIterator"};

  class KeyIterObject : public objects::DynObject
  {
    std::map<std::string, objects::DynObject*>::iterator iter;
    std::map<std::string, objects::DynObject*>::iterator iter_end;

  public:
    KeyIterObject(std::map<std::string, objects::DynObject*>& fields)
    : objects::DynObject(&keyIterPrototypeObject),
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
} // namespace rt::core
