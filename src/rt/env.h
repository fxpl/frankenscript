#include "objects/dyn_object.h"
#include "rt.h"

namespace rt::env {

// The prototype object for functions
// TODO put some stuff in here?
objects::DynObject framePrototypeObject{nullptr, true};

class FrameObject : public objects::DynObject {
  FrameObject() : objects::DynObject(&framePrototypeObject, true) {}
public:
  FrameObject(objects::DynObject* parent_frame) : objects::DynObject(&framePrototypeObject) {
    if (parent_frame) {
      auto old_value = this->set(objects::ParentField, parent_frame);
      objects::add_reference(this, parent_frame);
      assert(!old_value);
    }
  }

  static FrameObject* create_first_stack() {
    return new FrameObject();
  }
};

// The prototype object for functions
// TODO put some stuff in here?
objects::DynObject funcPrototypeObject{nullptr, true};
// The prototype object for bytecode functions
// TODO put some stuff in here?
objects::DynObject bytecodeFuncPrototypeObject{&funcPrototypeObject, true};

class FuncObject : public objects::DynObject {
public:
  FuncObject(objects::DynObject* prototype_, bool global = false) : objects::DynObject(prototype_, global) {}
};

class BytecodeFuncObject : public FuncObject {
  verona::interpreter::Bytecode *body;
public:
  BytecodeFuncObject(verona::interpreter::Bytecode *body_) : FuncObject(&bytecodeFuncPrototypeObject), body(body_) {}
  ~BytecodeFuncObject() {
    verona::interpreter::delete_bytecode(this->body);
    this->body = nullptr;
  }

  verona::interpreter::Bytecode* get_bytecode() {
    return this->body;
  }
};


// The prototype object for strings
// TODO put some stuff in here?
objects::DynObject stringPrototypeObject{nullptr, true};

class StringObject : public objects::DynObject {
  std::string value;

public:
  StringObject(std::string value_)
    : objects::DynObject(&stringPrototypeObject), value(value_) {}

  std::string get_name() {
    return value;
  }

  std::string as_key() {
    return value;
  }

  objects::DynObject* is_primitive() {
    return this;
  }
};

// The prototype object for iterators
// TODO put some stuff in here?
objects::DynObject keyIterPrototypeObject{nullptr, true};

class KeyIterObject : public objects::DynObject {
    std::map<std::string, objects::DynObject *>::iterator iter;
    std::map<std::string, objects::DynObject *>::iterator iter_end;

  public:
    KeyIterObject(std::map<std::string, objects::DynObject *> &fields)
      : objects::DynObject(&keyIterPrototypeObject), iter(fields.begin()), iter_end(fields.end()) {}

    objects::DynObject* iter_next() {
      objects::DynObject *obj = nullptr;
      if (this->iter != this->iter_end) {
        obj = objects::make_str(this->iter->first);
        this->iter++;
      }

      return obj;
    }

    std::string get_name() {
      return "&lt;iterator&gt;";
    }

    objects::DynObject* is_primitive() {
      return this;
    }
};
} // namespace rt::env
