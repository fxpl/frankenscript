/*****************************************************************************
 * This file contains a model implementation of region tracking.
 */

#include <stdio.h>
#include <termios.h>
#include <unistd.h>

#include "api.h"

void load_trieste(int argc, char **argv);

void test()
{
  using namespace api;
  

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

  a["self"] = a;

  mermaid({{"a", a}, {"b", b}, {"c", c}, {"g", g}, {"dd", dd}, {"h", h}});

  a["c"]["sub"] = g;

  mermaid({{"a", a}, {"b", b}, {"c", c}, {"g", g}, {"dd", dd}, {"h", h}});

  auto new_obj = Object::create("new");
  new_obj.create_region();

  mermaid({{"a", a}, {"b", b}, {"c", c}, {"g", g}, {"dd", dd}, {"h", h}, {"new", new_obj}});

  a["c"]["sub"] = nullptr;

  mermaid({{"a", a}, {"b", b}, {"c", c}, {"g", g}, {"dd", dd}, {"h", h}, {"new", new_obj}});

  new_obj["sub"] = g;

  mermaid({{"a", a}, {"b", b}, {"c", c}, {"g", g}, {"dd", dd}, {"h", h}, {"new", new_obj}});

  new_obj["h"] = h;

  dd = nullptr;
  g = nullptr;
  mermaid({{"a", a}, {"b", b}, {"c", c}, {"g", g}, {"dd", dd}, {"h", h}, {"new", new_obj}});

  h = new_obj["h"];

  mermaid({{"a", a}, {"b", b}, {"c", c}, {"g", g}, {"dd", dd}, {"h", h}, {"new", new_obj}});

  new_obj["self"] = new_obj;

  mermaid({{"a", a}, {"b", b}, {"c", c}, {"g", g}, {"dd", dd}, {"h", h}, {"new", new_obj}});

  a.create_region();

  mermaid({{"a", a}, {"b", b}, {"c", c}, {"g", g}, {"dd", dd}, {"h", h}, {"new", new_obj}});

  a["self"] = nullptr;
  a = nullptr;
  mermaid({{"a", a}, {"b", b}, {"c", c}, {"g", g}, {"dd", dd}, {"h", h}, {"new", new_obj}});

  c = nullptr;

  mermaid({{"a", a}, {"b", b}, {"c", c}, {"g", g}, {"dd", dd}, {"h", h}, {"new", new_obj}});

  b = nullptr;

  mermaid({{"a", a}, {"b", b}, {"c", c}, {"g", g}, {"dd", dd}, {"h", h}, {"new", new_obj}});

  new_obj = nullptr;

  mermaid({{"a", a}, {"b", b}, {"c", c}, {"g", g}, {"dd", dd}, {"h", h}, {"new", new_obj}});
}


bool read_up_down() {
  while (true) {
    char c = getchar();
    while (c == 27) {
      c = getchar();
      if (c == 91) {
        c = getchar();
        if (c == 65) {
          return true;
        } else if (c == 66) {
          return false;
        }
      }
    }
  }
}

void ui() {
  // Written by Copilot
  struct termios old_tio, new_tio;
  tcgetattr(STDIN_FILENO, &old_tio); // Get terminal settings for stdin
  new_tio = old_tio; // Keep the old settings to restore them later
  new_tio.c_lflag &= ~ICANON & ~ECHO; // Disable canonical mode (buffered I/O)
  tcsetattr(STDIN_FILENO, TCSANOW,
            &new_tio); // Set the new settings immediately

  for (int i = 0; i < 10; i++) {
    if (read_up_down()) {
      std::cout << "Up" << std::endl;
    } else {
      std::cout << "Down" << std::endl;
    }
  }

  // Restore original settings before exiting
  tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
}




int main(int argc, char **argv)
{
//  api::run(test);

  load_trieste(argc, argv);
  return 0;
}