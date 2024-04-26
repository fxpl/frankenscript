#pragma once

#include "objects.h"

namespace api
{
  using namespace objects;

  class Reference;

  class Object
  {
    friend class Reference;

    DynObject* object;

    Object(DynObject* object) : object(object) {}

  public:
    Object() : object(nullptr) {}

    Object(Object& other) : object(other.object)
    {
      if (object != nullptr)
        object->inc_rc();
    }

    Object(Object&& other) : object(other.object)
    {
      other.object = nullptr;
    }

    Object& operator=(Object& other)
    {
      auto old = object;
      if (other.object != nullptr)
        object->inc_rc();
      object = other.object;
      if (old != nullptr)
        old->dec_rc();
      return *this;
    }

    Object& operator=(Object&& other)
    {
      auto old = object;
      object = other.object;
      other.object = nullptr;
      if (old != nullptr)
        old->dec_rc();
      return *this;
    }

    static Object create(std::string name)
    {
      return {new DynObject(name)};
    }

    Reference operator[](std::string name);

    ~Object()
    {
      if (object != nullptr)
        object->dec_rc();
    }

    void freeze()
    {
      object->freeze();
    }

    void print()
    {
      object->print();
    }

    void create_region()
    {
      object->create_region();
    }
  };

  class Reference
  {
    friend class Object;
    std::string key;
    DynObject* object;

    Reference(std::string name, DynObject* object) : key(name), object(object) {}
  public:

    operator DynObject*() const {
      auto result = object->get(key);
      if (result != nullptr)
        result->inc_rc();
      return result;    
    }

    Reference& operator=(Object& other)
    {
      if (other.object != nullptr)
        other.object->inc_rc();
      object->set(key, other.object);
      return *this;
    }

    Reference& operator=(Object&& other)
    {
      object->set(key, other.object);
      other.object = nullptr;
      return *this;
    }
  };


  Reference Object::operator[](std::string name)
  {
    return Reference(name, object);
  }
}