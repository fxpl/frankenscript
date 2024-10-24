#include "../lang.h"

inline const TokenDef Rest{"rest"};

PassDef grouping()
{
  PassDef p{
    "grouping",
    verona::wf::grouping,
    dir::bottomup,
    {
      // Normalize so that all expressions are inside bodies and blocks.
      T(File) << (--T(Body) * Any++[File]) >>
        [](auto& _) { return File << (Body << (Block << _[File])); },

      In(Group) * OPERAND[Op] * (T(Lookup)[Lookup] << (T(Group) << KEY[Rhs])) >>
        [](auto& _) { return Lookup << _(Op) << _(Rhs); },

      T(Group) << ((T(Drop)[Drop] << End) * LV[Lhs] * End) >>
        [](auto& _) { return Assign << _(Lhs) << Null; },
      T(Group) << ((T(Take)[Take] << End) * LV[Lhs] * End) >>
        [](auto& _) { return create_from(Take, _(Take)) << _(Lhs); },

      // function(arg, arg)
      --In(Func) *
          (T(Group)[Group] << (T(Ident)[Ident]) *
             (T(Parens)[Parens] << (~T(List)[List] * End)) * Any++[Rest]) >>
        [](auto& _) {
          auto list = _(List);
          if (!list)
          {
            list = create_from(List, _(Parens));
          }

          auto call = create_from(Call, _(Group)) << _(Ident) << list;
          // If there are more nodes to process we want to keep the "Group"
          if (_[Rest].size() == 0)
          {
            return call;
          }
          else
          {
            return Group << call << _[Rest];
          }
        },
      --In(Method) *
          (T(Group)[Group]
           << ((T(Lookup)[Lookup]) * (T(Parens)[Parens] << (~T(List)[List])) *
               End)) >>
        [](auto& _) {
          auto list = _(List);
          if (!list)
          {
            list = create_from(List, _(Parens));
          }

          return create_from(Method, _(Group)) << _(Lookup) << list;
        },

      T(Assign)
          << ((T(Group) << LV[Lhs] * End) *
              ((T(Group) << (RV[Rhs] * End)) / (RV[Rhs] * End)) * End) >>
        [](auto& _) { return Assign << _[Lhs] << _[Rhs]; },

      COND[Op] << (Any[Lhs] * (T(Group) << CMP_V[Rhs] * End) * End) >>
        [](auto& _) {
          return create_from(_(Op)->type(), _(Op)) << _[Lhs] << _[Rhs];
        },
      COND[Op] << ((T(Group) << CMP_V[Lhs] * End) * Any[Rhs] * End) >>
        [](auto& _) {
          return create_from(_(Op)->type(), _(Op)) << _[Lhs] << _[Rhs];
        },

      (T(If) << (T(Group) * COND[Op] * (T(Group) << T(Block)[Block]))) >>
        [](auto& _) { return If << _(Op) << _(Block); },
      (T(Else) << (T(Group) * (T(Group) << T(Block)[Block]))) >>
        [](auto& _) { return Else << _(Block); },
      (T(If)[If] << (COND * T(Block) * End)) * (T(Else) << T(Block)[Block]) >>
        [](auto& _) { return _(If) << _(Block); },
      (T(If)[If] << (COND * T(Block) * End)) * (--T(Else)) >>
        [](auto& _) {
          // This adds an empty else block, if no else was written
          return _(If) << Block;
        },

      In(List) * (T(Group) << (RV[Rhs] * End)) >>
        [](auto& _) { return _(Rhs); },

      T(For)[For]
          << ((T(Group)) *
              (T(List) << (T(Ident)[Key] * T(Ident)[Value] * End)) *
              (T(Group) << (LV[Op] * End)) *
              (T(Group) << (T(Block)[Block] * End)) * End) >>
        [](auto& _) {
          // This function only has a block for now, as a lowering pass still
          // needs to do some modifications until it's a "full" body.
          return create_from(For, _(For))
            << _(Key) << _(Value) << _(Op) << _(Block);
        },
      (T(While) << (T(Group) * COND[Op] * (T(Group) << T(Block)[Block]))) >>
        [](auto& _) { return While << _(Op) << _(Block); },

      T(Func)[Func]
          << ((T(Group) << End) *
              (T(Group)
               << ((T(Ident)[Ident]) *
                   (T(Parens)[Parens] << (~(T(List) << T(Ident)++[List]))) *
                   End)) *
              (T(Group) << T(Block)[Block]) * End) >>
        [](auto& _) {
          return create_from(Func, _(Func))
            << _(Ident) << (create_from(Params, _(Parens)) << _[List])
            << (Body << _(Block));
        },
      // Normalize parenthesis with a single node to also have a list token
      T(Parens)[Parens] << ((T(Group) << (RV[Rhs] * End)) / (RV[Rhs] * End)) >>
        [](auto& _) {
          return create_from(Parens, _(Parens))
            << (create_from(List, _(Parens)) << _(Rhs));
        },

      T(Return)[Return] << ((T(Group) << End) * End) >>
        [](auto& _) { return create_from(Return, _(Return)); },
      T(Return)[Return]
          << ((T(Group) << End) * (T(Group) << (RV[Rhs] * End)) * End) >>
        [](auto& _) { return create_from(ReturnValue, _(Return)) << _(Rhs); },
      T(Return)[Return] << ((T(Group) << End) * (RV[Rhs] * End)) >>
        [](auto& _) { return create_from(ReturnValue, _(Return)) << _(Rhs); },

    }};

  return p;
}
