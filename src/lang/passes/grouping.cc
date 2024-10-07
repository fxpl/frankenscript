#include "../lang.h"

PassDef grouping() {
  PassDef p{
      "grouping",
      verona::wf::grouping,
      dir::bottomup,
      {
          // Normalize so that all expressions are inside bodies and blocks.
          T(File) << (--T(Body) * Any++[File]) >>
            [](auto& _) { return  File << (Body << (Block << _[File])); },

          In(Group) * LV[Lhs] * (T(Lookup)[Lookup] << (T(Group) << KEY[Rhs])) >>
              [](auto &_) { return Lookup << _[Lhs] << _(Rhs); },

          T(Group) << ((T(Region)[Region] << End) * T(Ident)[Ident] * End) >>
              [](auto &_) {
                _(Region)->extend(_(Ident)->location());
                return _(Region) << _(Ident);
              },

          T(Group) << ((T(Freeze)[Freeze] << End) * T(Ident)[Ident] * End) >>
              [](auto &_) {
                _(Freeze)->extend(_(Ident)->location());
                return _(Freeze) << _(Ident);
              },

          T(Group) << ((T(Drop)[Drop] << End) * LV[Lhs] * End) >>
              [](auto &_) {
                return Assign << _(Lhs) << Null;
              },
          // function(arg, arg)
          --In(Func) * (
            T(Group)[Group] << (
              (T(Ident)[Ident]) *
              (T(Parens)[Parens] << (~T(List)[List])) *
              End)) >>
            [](auto &_) {
                auto list = _(List);
                if (!list) {
                  list = create_from(List, _(Parens));
                }

                return create_from(Call, _(Group))
                  << _(Ident)
                  << list;
            },

          T(Group) << ((T(Create)[Create] << End) * T(Ident)[Ident] * End) >>
              [](auto &_) {
                _(Create)->extend(_(Ident)->location());
                return Group << (_(Create) << _(Ident));
              },

          T(Assign) << ((T(Group) << LV[Lhs] * End) *
                        ((T(Group) << (RV[Rhs] * End)) / (RV[Rhs] * End)) * End) >>
              [](auto &_) { return Assign << _[Lhs] << _[Rhs]; },
          T(Eq) << ((T(Group) << CMP_V[Lhs] * End) *
                        (T(Group) << CMP_V[Rhs] * End) * End) >>
              [](auto &_) { return Eq << _[Lhs] << _[Rhs]; },

          (T(If) << (T(Group) * T(Eq)[Eq] * (T(Group) << T(Block)[Block]))) >>
            [](auto &_) {
              return If << _(Eq) << _(Block);
            },
          (T(Else) << (T(Group) * (T(Group) << T(Block)[Block]))) >>
            [](auto &_) {
              return Else << _(Block);
            },
          (T(If)[If] << (T(Eq) * T(Block) * End)) * (T(Else) << T(Block)[Block]) >>
            [](auto &_) {
              return _(If) << _(Block);
            },
          (T(If)[If] << (T(Eq) * T(Block) * End)) * (--T(Else)) >>
            [](auto &_) {
              // This adds an empty else block, if no else was written
              return _(If) << Block;
            },

          In(List) * (T(Group) << (RV[Rhs] * End)) >> [](auto &_) {
            return _(Rhs);
          },

          T(For)[For] << (
              (T(Group)) *
              (T(List) << (T(Ident)[Key] * T(Ident)[Value] * End)) *
              (T(Group) << (LV[Op] * End)) *
              (T(Group) << (T(Block)[Block] * End)) *
              End) >>
            [](auto &_) {
              // This function only has a block for now, as a lowering pass still needs to
              // do some modifications until it's a "full" body.
              return create_from(For, _(For))
                << _(Key)
                << _(Value)
                << _(Op)
                << _(Block);
            },
          
          T(Func)[Func] << (
            (T(Group) << End) *
            (T(Group) << (
              (T(Ident)[Ident]) *
              (T(Parens)[Parens] << (~(T(List) << T(Ident)++[List]))) *
              End)) *
            (T(Group) << T(Block)[Block]) *
            End) >>
            [](auto &_) {
              return create_from(Func, _(Func))
                << _(Ident)
                << (create_from(Params, _(Parens)) << _[List])
                << (Body << _(Block));
            },

            T(Return)[Return] << (
              (T(Group) << End) *
              End) >>
              [](auto &_) {
                return create_from(Return, _(Return));
            },
            T(Return)[Return] << (
              (T(Group) << End) *
              (T(Group) << (RV[Rhs] * End)) *
              End) >>
              [](auto &_) {
                return create_from(ReturnValue, _(Return)) << _(Rhs);
            },

      }};


  return p;
}
