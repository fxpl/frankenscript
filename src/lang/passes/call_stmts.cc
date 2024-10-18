#include "../lang.h"

namespace verona::wf
{
  using namespace trieste::wf;

  inline const auto call_stmts = grouping |
    (Block <<=
     (Freeze | Taint | Region | Assign | If | For | Func | Return |
      ReturnValue | Call | Method | ClearStack | Print)++);
}

PassDef call_stmts()
{
  return {
    "call_stmts",
    verona::wf::call_stmts,
    dir::bottomup | dir::once,
    {
      In(Block) * T(Call, Method)[Call] >>
        [](auto& _) {
          return Seq << _(Call) << ClearStack << create_print(_(Call));
        },
    }};
}
