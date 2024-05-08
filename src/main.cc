/*****************************************************************************
 * This file contains a model implementation of region tracking.
 */

#include <stdio.h>

#include "api.h"

int main()
{
  using namespace api;
  
  api::set_output("mermaid.md");

  auto a = Object::create("a");
  auto b = Object::create("b");
  auto c = Object::create("c");
  {
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

    e["emp"] = nullptr;
  }

  b.freeze();
  mermaid({{"a", a}, {"b", b}, {"c", c}});
  c.create_region();
  mermaid({{"a", a}, {"b", b}, {"c", c} });
  a["w"] = b;
  b = b["d"];
  a["eff"] = b;

  auto g = Object::create("g");
  a["g"] = g;


  g["x"] = Object::create("dd");

  Object dd;
  dd = g["x"];
  dd["i"] = b;
  auto h = Object::create("h");
  dd["h"] = h;
  dd.create_region();
  mermaid({{"a", a}, {"b", b}, {"c", c}, {"g", g}, {"dd", dd}, {"h", h}});

  dd["g"] = g;

  mermaid({{"a", a}, {"b", b}, {"c", c}, {"g", g}, {"dd", dd}, {"h", h}});

  a["g"] = nullptr;

  mermaid({{"a", a}, {"b", b}, {"c", c}, {"g", g}, {"dd", dd}, {"h", h}});

  return 0;
}