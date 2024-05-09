#pragma once

#include "objects.h"
#include "mermaid.h"

namespace api
{
  using namespace objects;

  class Reference;

  void set_output(std::string path)
  {
    objects::set_output(path);
  }

  class Object
  {
    friend class Reference;
    friend void mermaid(std::initializer_list<std::pair<std::string, Object&>> roots);

    DynObject* object;

    Object(DynObject* object) : object(object) {}

    void copy(DynObject* new_object)
    {
      auto old = object;
      if (new_object != nullptr)
        new_object->inc_rc();
      object = new_object;
      if (old != nullptr)
        old->dec_rc();
    }

    void move(DynObject* new_object)
    {
      auto old = object;
      object = new_object;
      if (old != nullptr)
        old->dec_rc();
    }

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
      copy(other.object);
      return *this;
    }

    Object& operator=(Object&& other)
    {
      move(other.object);
      return *this;
    }

    Object& operator=(std::nullptr_t)
    {
      move(nullptr);
      return *this;
    }

    Object& operator=(Reference& other);
    Object& operator=(Reference&& other);

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

    void copy(DynObject* new_object)
    {
      auto old = object;
      if (new_object != nullptr)
        new_object->inc_rc();
      object->set(key, new_object);
    }

    void move(DynObject*& new_object)
    {
      object->set(key, new_object);
      new_object = nullptr;
    }

    DynObject* get()
    {
      return object->get(key);
    }

    Reference(std::string name, DynObject* object) : key(name), object(object) {}
  public:

    Reference& operator=(Object& other)
    {
      copy(other.object);
      return *this;
    }

    Reference& operator=(Object&& other)
    {
      move(other.object);
      return *this;
    }

    Reference& operator=(Reference& other)
    {
      copy(other.get());
      return *this;
    }

    Reference& operator=(Reference&& other)
    {
      copy(other.get());
      return *this;
    }

    Reference& operator=(std::nullptr_t)
    {
      object->set(key, nullptr);
      return *this;
    }

    Reference operator[](std::string name)
    {
      return {name, object->get(key)};
    }
  };


  Reference Object::operator[](std::string name)
  {
    return Reference(name, object);
  }

  Object& Object::operator=(Reference& other)
  {
    copy(other.get());
    return *this;
  }

  Object& Object::operator=(Reference&& other)
  {
    copy(other.get());
    return *this;
  }

  void mermaid(std::initializer_list<std::pair<std::string, Object&>> roots)
  {
    std::vector<objects::Edge> edges;
    for (auto& root : roots)
    {
      edges.push_back({objects::DynObject::frame(), root.first, root.second.object});
    }
    objects::mermaid(edges);
    std::cout << "Press a key!" << std::endl;
    getchar();
  }

  template <typename F>
  void run(F&& f)
  {
    size_t initial_count = objects::DynObject::get_count();

    f();

    if (objects::DynObject::get_count() != initial_count)
    {
      std::cout << "Memory leak detected!" << std::endl;
      std::exit(1);
    }
  };
}