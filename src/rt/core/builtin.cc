#include "../core.h"
#include "../rt.h"

namespace rt::core
{
  void init_builtins(ui::UI* ui)
  {
    add_builtin("dummy", [](auto frame, auto stack, auto args) {
      assert(args == 1);
      std::cout << "===============================" << std::endl;
      std::cout << "Dummy was called with " << stack->back() << std::endl;
      std::cout << "===============================" << std::endl;
      remove_reference(frame, stack->back());
      stack->pop_back();
      return std::nullopt;
    });
  }
}