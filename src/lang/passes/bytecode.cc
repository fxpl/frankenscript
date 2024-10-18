#include "../lang.h"

inline const trieste::TokenDef DestructiveRead{"destructive_read"};

PassDef bytecode()
{
  PassDef p{
    "bytecode",
    verona::wf::bytecode,
    dir::topdown,
    {
      T(File) << T(Body)[Body] >>
        [](auto& _) { return Body << (Compile << *_[Body]); },
      T(Compile) << (T(Body)[Body] << Any++[Block]) >>
        [](auto& _) {
          return create_from(Body, _(Body)) << (Compile << _[Block]);
        },

      T(Compile) << (Any[Lhs] * (Any * Any++)[Rhs]) >>
        [](auto& _) {
          return Seq << (Compile << _[Lhs]) << (Compile << _[Rhs]);
        },

      // The node doesn't require additional processing and should be copied
      T(Compile) << T(
        Null,
        Label,
        Print,
        Jump,
        JumpFalse,
        CreateObject,
        StoreFrame,
        LoadFrame,
        IterNext,
        StoreField,
        Return,
        ReturnValue,
        ClearStack)[Op] >>
        [](auto& _) -> Node { return _(Op); },

      T(Compile) << (T(Eq, Neq)[Op] << (Any[Lhs] * Any[Rhs])) >>
        [](auto& _) {
          return Seq << (Compile << _(Lhs)) << (Compile << _(Rhs))
                     << _(Op)->type();
        },

      T(Compile) << (T(Ident)[Ident]) >>
        [](auto& _) { return create_from(LoadFrame, _(Ident)); },

      T(Compile) << (T(Lookup)[Lookup] << (Any[Op] * Any[Key] * End)) >>
        [](auto& _) {
          return Seq << (Compile << _[Op]) << (Compile << _[Key])
                     << create_from(LoadField, _(Lookup));
        },

      T(Compile) << (T(Assign)[Op] << (T(Ident)[Ident] * Any[Rhs])) >>
        [](auto& _) {
          return Seq << (Compile << _[Rhs]) << create_from(StoreFrame, _(Ident))
                     << create_print(_(Op));
        },

      T(Compile)
          << (T(Assign)[Assign]
              << ((T(Lookup)[Lookup] << (Any[Op] * Any[Key] * End)) *
                  Any[Rhs])) >>
        [](auto& _) {
          return Seq << (Compile << _[Op]) << (Compile << _[Key])
                     << (Compile << _[Rhs])
                     << create_from(StoreField, _(Lookup))
                     << create_print(_(Assign));
        },

      T(Compile) << (T(Freeze)[Op] << T(Ident)[Ident]) >>
        [](auto& _) {
          return Seq << (Compile << _[Ident]) << FreezeObject
                     << create_print(_(Op));
        },

      T(Compile) << (T(Taint)[Op] << T(Ident)[Ident]) >>
        [](auto& _) {
          auto print_str = std::string(create_print(_(Op))->location().view());
          return Seq << (Compile << _[Ident]) << (Taint ^ print_str);
        },

      T(Compile) << (T(DestructiveRead) << T(Ident)[Ident]) >>
        [](auto& _) {
          // Read the value from the frame and set the frame value to null
          return Seq << (Compile << _(Ident)) << Null
                     << create_from(StoreFrame, _(Ident));
        },
      T(Compile) << (T(Cown)[Op] << T(Ident)[Ident]) >>
        [](auto& _) {
          return Seq << (Compile << (DestructiveRead << _(Ident)))
                     << (CreateObject << Cown);
        },

      T(Compile) << (T(Create)[Op] << T(Ident)[Ident]) >>
        [](auto& _) {
          return Seq << (Compile << _[Ident]) << (CreateObject << Proto);
        },

      T(Compile) << (T(Region)[Op] << T(Ident)[Ident]) >>
        [](auto& _) {
          return Seq << (Compile << _[Ident]) << CreateRegion
                     << create_print(_(Op));
        },

      T(Compile)
          << (T(Method)[Method]
              << ((T(Lookup)[Lookup] << (OPERAND[Op] * T(String)[Key])) *
                  (T(List) << Any++[List]) * End)) >>
        [](auto& _) {
          auto arg_ctn = _[List].size();
          return Seq
            // `self` lookup and first argument
            << (Compile << _(Op))
            << (Compile << _[List])
            // Duplicate self
            << (Dup ^ std::to_string(arg_ctn))
            // Fetch the function
            << (Compile << _(Key))
            << create_from(LoadField, _(Lookup))
            // function call, +1 for the self argument
            << (Call ^ std::to_string(arg_ctn + 1));
        },
      T(Compile)
          << (T(Call)[Call] << (KEY[Op] * (T(List) << Any++[List]) * End)) >>
        [](auto& _) {
          // The print is done by the called function
          return Seq << (Compile << _[List]) << (Compile << _(Op))
                     << (Call ^ std::to_string(_[List].size()));
        },

      T(Compile) << End >> [](auto&) -> Node { return {}; },

      T(Compile) << (T(Empty)) >>
        [](auto&) -> Node { return CreateObject << Dictionary; },
      T(Compile) << (T(String)[String]) >>
        [](auto& _) -> Node { return CreateObject << _(String); },

    }};
  return p;
}
