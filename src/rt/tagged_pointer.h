#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

namespace utils
{
  template <typename T>
  class TaggedPointer
  {
    uintptr_t ptr;

    constexpr TaggedPointer(uintptr_t ptr) : ptr(ptr) {}

  public:
    TaggedPointer(T* ptr) : ptr(reinterpret_cast<uintptr_t>(ptr)) {}
    constexpr TaggedPointer(nullptr_t) : ptr(0) {}

    TaggedPointer(T* ptr, uintptr_t tag) : ptr(reinterpret_cast<uintptr_t>(ptr) | tag)
    { assert(tag < 4); }

    constexpr TaggedPointer(nullptr_t, uintptr_t tag) : ptr(tag) { assert(tag < 4); }

    bool operator==(TaggedPointer other) const { return ptr == other.ptr; }
    bool operator!=(TaggedPointer other) const { return ptr != other.ptr; }

    void set_tag(uintptr_t tag)
    {
      assert(tag < 4);
      ptr = (ptr & ~0x3) | tag;
    }

    void add_tag(uintptr_t tag)
    {
      assert(tag < 4);
      ptr |= tag;
    }

    void remove_tag(uintptr_t tag)
    {
      assert(tag < 4);
      ptr &= ~tag;
    }

    T* get_ptr() const
    {
      return reinterpret_cast<T*>(ptr & ~0x3);
    }

    operator T*() const { return get_ptr(); }

    T& operator *() const { return *get_ptr(); }

    uintptr_t get_tag() const
    {
      return ptr & 0x3;
    }
  };
}