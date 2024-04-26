/*****************************************************************************
 * This file contains a model implementation of region tracking.
 */

#include "api.h"

int main()
{
  using namespace api;
  
  auto a = Object::create("a");
  auto b = Object::create("b");
  {
    auto c = Object::create("c");
    auto d = Object::create("d");
    auto e = Object::create("e");
    auto f = Object::create("f");

    c["e"] = e;
    a["c"] = c;
    f["c"] = c;
    b["d"] = d;
    a["e"] = e;

    std::cout << "=============" << std::endl;
    e["emp"] = Object::create("emp");
    std::cout << "=============" << std::endl;
    e["self"] = e;
  }
//  a["b"] = b;

  a.print();

  b.print();
  b.create_region();
  a.print();
  
  a.create_region();

  return 0;
}