#include "../lang.h"

namespace verona::wf
{
  using namespace trieste::wf;
  inline const trieste::wf::Wellformed flatten = (Top <<= File) |
    (File <<= Body) |
    (Body <<=
     (Freeze | Region | Assign | Eq | Neq | Label | Jump | JumpFalse | Print |
      StoreFrame | LoadFrame | CreateObject | Ident | IterNext | Create |
      StoreField | Lookup | String | Call | Method | Return | ReturnValue |
      ClearStack)++) |
    (CreateObject <<= (KeyIter | String | Dictionary | Func)) |
    (Func <<= Compile) | (Compile <<= Body) | (Create <<= Ident) |
    (Assign <<= (Lhs >>= lv) * (Rhs >>= rv)) |
    (Lookup <<= (Op >>= operand) * (Rhs >>= key)) | (Region <<= Ident) |
    (Freeze <<= Ident) | (Call <<= Ident * List) | (Method <<= Lookup * List) |
    (List <<= rv++) | (Params <<= Ident++) |
    (Eq <<= (Lhs >>= cmp_values) * (Rhs >>= cmp_values)) |
    (Neq <<= (Lhs >>= cmp_values) * (Rhs >>= cmp_values)) |
    (Label <<= Ident)[Ident];
  ;
} // namespace verona::wf

int g_jump_label_counter = 0;
std::string new_jump_label()
{
  g_jump_label_counter += 1;
  return "label_" + std::to_string(g_jump_label_counter);
}

int g_iter_name = 0;
std::string new_iter_name()
{
  g_iter_name += 1;
  return "_iter_" + std::to_string(g_iter_name);
}

std::string expr_header(Node expr)
{
  auto expr_str = expr->location().view();
  return std::string(expr_str.substr(0, expr_str.find(":") + 1));
}

PassDef flatten()
{
  return {
    "flatten",
    verona::wf::flatten,
    dir::bottomup,
    {
      In(Body) * (T(Block) << Any++[Block]) >>
        [](auto& _) { return Seq << _[Block]; },
      T(If)[If]
          << (COND[Op] * (T(Block) << Any++[Lhs]) * (T(Block) << Any++[Rhs])) >>
        [](auto& _) {
          auto else_label = new_jump_label();
          auto join_label = new_jump_label();

          auto if_head = expr_header(_(If));

          return Seq << _(Op)
                     << (JumpFalse ^ else_label)
                     // Then block
                     << create_print(_(If), if_head + " (True)") << _[Lhs]
                     << (Jump ^ join_label)
                     // Else block
                     << ((Label ^ "else:") << (Ident ^ else_label))
                     << create_print(_(If), if_head + " (False)")
                     << _[Rhs]
                     // Join
                     << (Label << (Ident ^ join_label));
        },
      T(For)[For]
          << (T(Ident)[Key] * T(Ident)[Value] * LV[Op] *
              (T(Block) << Any++[Block]) * End) >>
        [](auto& _) {
          auto it_name = new_iter_name();

          auto start_label = new_jump_label();
          auto break_label = new_jump_label();

          auto for_head = expr_header(_(For));

          return Seq
            // Prelude
            << clone(_(Op)) << (CreateObject << KeyIter)
            << (StoreFrame ^ it_name) << (Ident ^ it_name)
            << (String ^ "source") << _(Op) << (StoreField)
            << create_print(_(For), "create " + it_name)
            << ((Label ^ "start:") << (Ident ^ start_label))
            // key = iter++
            << (Ident ^ it_name) << (IterNext)
            << create_from(StoreFrame, _(Key))
            // While (key != null)
            << (Neq << create_from(Ident, _(Key)) << Null)
            << (JumpFalse ^ break_label)
            // value = it.source.key
            << (Lookup << (Lookup << (Ident ^ it_name) << (String ^ "source"))
                       << create_from(Ident, _(Key)))
            << create_from(StoreFrame, _(Value))
            << create_print(_(For), for_head + " (Next)")
            // Block
            << _[Block]
            // Final cleanup
            << (Jump ^ start_label)
            << ((Label ^ "break:") << (Ident ^ break_label))
            << create_print(_(For), for_head + " (Break)")
            << ((Assign ^ ("drop " + std::string(_(Value)->location().view())))
                << create_from(Ident, _(Value)) << Null)
            << ((Assign ^ ("drop " + it_name)) << (Ident ^ it_name) << Null);
        },

      T(Func)[Func]
          << (T(Ident)[Ident] * T(Params)[Params] *
              (T(Body)[Body] << Any++[Block]) * End) >>
        [](auto& _) {
          auto func_head = expr_header(_(Func));

          // Function setup
          Node body = Body;
          Node args = _(Params);
          for (auto it = args->begin(); it != args->end(); it++)
          {
            body << create_from(StoreFrame, *it);
          }
          body << create_print(_(Func), func_head + " (Enter)");

          // Function body
          auto block = _[Block];
          for (auto stmt : block)
          {
            if (stmt == ReturnValue)
            {
              body
                << (create_from(Assign, stmt)
                    << (Ident ^ "return") << stmt->at(0));
              body << (LoadFrame ^ "return");
              body << create_from(ReturnValue, stmt);
            }
            else if (stmt == Return)
            {
              body << create_print(stmt);
              body << stmt;
            }
            else
            {
              body << stmt;
            }
          }
          body << create_print(_(Func), func_head + " (Exit)");

          // Function cleanup
          return Seq << (CreateObject << (Func << (Compile << body)))
                     << (StoreFrame ^ _(Ident))
                     << create_print(_(Func), func_head);
        },
    }};
}
