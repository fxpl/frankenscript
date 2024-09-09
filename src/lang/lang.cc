#include "bytecode.h"
#include "trieste/driver.h"
#include "trieste/trieste.h"

using namespace trieste;

inline const TokenDef Ident{"ident", flag::print};
inline const TokenDef Assign{"assign"};
inline const TokenDef Empty{"empty"};
inline const TokenDef Drop{"drop"};
inline const TokenDef Freeze{"freeze"};
inline const TokenDef Region{"region"};
inline const TokenDef Lookup{"lookup", flag::print};

inline const TokenDef Rhs{"rhs"};
inline const TokenDef Lhs{"lhs"};

namespace verona::wf {
using namespace trieste::wf;

inline const auto parse_tokens = Region | Ident | Lookup | Empty | Freeze | Drop | Null;

inline const auto parser = (Top <<= File) | (File <<= (Group | Assign)++) |
                           (Assign <<= Group++) | (Group <<= parse_tokens++);

inline const auto lv = Ident | Lookup;
inline const auto rv = lv | Empty | Null;

inline const auto grouping =
    (Top <<= File) | (File <<= (Freeze | Region | Assign)++) |
    (Assign <<= (Lhs >>= lv) * (Rhs >>= rv)) | (Lookup <<= rv) |
    (Region <<= Ident) | (Freeze <<= Ident);
} // namespace verona::wf

trieste::Parse parser() {
  Parse p{depth::file, verona::wf::parser};

  p("start",
    {
        "[[:blank:]]+" >> [](auto &) {},
        "(?:#[^\\n]*)?\\n" >> [](auto &m) { m.term({Assign}); },
        "drop" >> [](auto &m) { m.add(Drop); },
        "freeze" >> [](auto &m) { m.add(Freeze); },
        "region" >> [](auto &m) { m.add(Region); },
        "[[:alpha:]]+" >> [](auto &m) { m.add(Ident); },
        "\\[\"([[:alpha:]]+)\"\\]" >> [](auto &m) { m.add(Lookup, 1); },
        "=" >> [](auto &m) { m.seq(Assign); },
        "{}" >> [](auto &m) { m.add(Empty); },
    });

  p.done([](auto &m) { m.term({Assign}); });

  return p;
}

auto LV = T(Ident, Lookup);
auto RV = T(Empty, Ident, Lookup, Null);

PassDef grouping() {
  PassDef p{
      "grouping",
      verona::wf::grouping,
      dir::bottomup,
      {
          In(Group) * LV[Lhs] * T(Lookup)[Lookup] >>
              [](auto &_) { return _(Lookup) << _[Lhs]; },

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

          T(Assign) << ((T(Group) << LV[Lhs] * End) *
                        (T(Group) << RV[Rhs] * End) * End) >>
              [](auto &_) { return Assign << _[Lhs] << _[Rhs]; },
      }};

  return p;
}

inline const TokenDef Compile{"compile"};

namespace verona::wf {
using namespace trieste::wf;
inline const trieste::wf::Wellformed bytecode =
    empty | (Top <<= (LoadFrame | StoreFrame | LoadField | StoreField | Drop | Null |
                      CreateObject | CreateRegion | FreezeObject | Print)++);
} // namespace verona::wf

Node create_from(Token def, Node from) {
  return NodeDef::create(def, from->location());
}

std::pair<PassDef, std::shared_ptr<Node>> bytecode() {
  std::shared_ptr<Node> result = std::make_shared<Node>();

  PassDef p{"bytecode",
            verona::wf::bytecode,
            dir::topdown,
            {
                T(File)[File] >>
                    [](auto &_) {
                      return Seq << (Compile << *_[File])
                                 << create_from(Print, _(File)->back());
                    },

                T(Compile) << (Any[Lhs] * (Any * Any++)[Rhs]) >>
                    [](auto &_) {
                      return Seq << (Compile << _[Lhs])
                                 << create_from(Print, _(Lhs))
                                 << (Compile << _[Rhs]);
                    },

                T(Compile) << T(Null) >> [](auto &) ->Node { return Null; },

                T(Compile) << (T(Ident)[Ident]) >>
                    [](auto &_) { return create_from(LoadFrame, _(Ident)); },

                T(Compile) << (T(Lookup)[Lookup] << Any[Rhs]) >>
                    [](auto &_) {
                      return Seq << (Compile << _[Rhs])
                                 << create_from(LoadField, _(Lookup));
                    },

                T(Compile) << (T(Assign) << (T(Ident)[Ident] * Any[Rhs])) >>
                    [](auto &_) {
                      return Seq << (Compile << _[Rhs])
                                 << create_from(StoreFrame, _(Ident));
                    },

                T(Compile) << (T(Assign) << ((T(Lookup)[Lookup] << Any[Lhs]) *
                                             Any[Rhs])) >>
                    [](auto &_) {
                      return Seq << (Compile << _[Lhs]) << (Compile << _[Rhs])
                                 << create_from(StoreField, _(Lookup));
                    },

                T(Compile) << (T(Freeze) << T(Ident)[Ident]) >>
                    [](auto &_) {
                      return Seq << (Compile << _[Ident]) << FreezeObject;
                    },

                T(Compile) << (T(Region) << T(Ident)[Ident]) >>
                    [](auto &_) {
                      return Seq << (Compile << _[Ident]) << CreateRegion;
                    },

                T(Compile) << (T(Empty)) >>
                    [](auto &) -> Node { return CreateObject; },

            }};
  p.post(trieste::Top, [result](Node n) {
    *result = n;
    return 0;
  });

  return {p, result};
}

struct CLIOptions : trieste::Options
{
  bool iterative = false;

  void configure(CLI::App &app)
  {
    app.add_flag("-i,--interactive", iterative, "Run the interpreter iteratively");
  }
};

namespace verona::interpreter {
void run(trieste::Node node, bool iterative);
}

void load_trieste(int argc, char **argv) {
  CLIOptions options;
  auto [bytecodepass, result] = bytecode();
  trieste::Reader reader{"verona_dyn", {grouping(), bytecodepass}, parser()};
  trieste::Driver driver{reader, &options};
  driver.run(argc, argv);
  verona::interpreter::run(*result, options.iterative);
  // return *result;
}