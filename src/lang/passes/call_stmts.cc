#include "../lang.h"

namespace verona::wf
{
  using namespace trieste::wf;

  inline const auto call_stmts = grouping |
    (Block <<=
     (Assign | If | For | While | Func | Return | ReturnValue | Call | Method |
      ClearStack | Print)++);
}

PassDef call_stmts()
{
  return {
    "call_stmts",
    verona::wf::call_stmts,
    dir::bottomup | dir::once,
    {
      In(Block) * (T(Call)[Call] << T(Ident)[Ident]) >>
        [](auto& _) {
          if (_(Ident)->location().view() == "spawn_behavior")
          {
            return Seq << _(Call) << create_print(_(Call), "Spawning Behavior");
          }
          else
          {
            return Seq << _(Call) << ClearStack << create_print(_(Call));
          }
        },
      In(Block) * T(Method)[Method] >>
        [](auto& _) {
          return Seq << _(Method) << ClearStack << create_print(_(Method));
        },
    }};
}
