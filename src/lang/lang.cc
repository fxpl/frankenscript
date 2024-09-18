#include "bytecode.h"
#include "trieste/driver.h"
#include "trieste/trieste.h"
#include <optional>

#define TAB_SIZE 4

using namespace trieste;

inline const TokenDef Ident{"ident", trieste::flag::print};
inline const TokenDef Assign{"assign"};
inline const TokenDef If{"if"};
inline const TokenDef Else{"else"};
inline const TokenDef Eq{"=="};
inline const TokenDef Block{"block"};
inline const TokenDef Empty{"empty"};
inline const TokenDef Drop{"drop"};
inline const TokenDef Freeze{"freeze"};
inline const TokenDef Region{"region"};
inline const TokenDef Lookup{"lookup", flag::print};

inline const TokenDef Op{"op"};
inline const TokenDef Rhs{"rhs"};
inline const TokenDef Lhs{"lhs"};

namespace verona::wf {
using namespace trieste::wf;

inline const auto parse_tokens = Region | Ident | Lookup | Empty | Freeze | Drop | Null;
inline const auto parse_groups = Group | Assign | If | Else | Block;

inline const auto parser =
  (Top <<= File)
  | (File <<= parse_groups++)
  | (Assign <<= Group++)
  | (If <<= Group * Eq * Group)
  | (Else <<= Group * Group)
  | (Group <<= (parse_tokens | Block)++)
  | (Block <<= (parse_tokens | parse_groups)++)
  | (Eq <<= Group * Group)
  ;

inline const auto lv = Ident | Lookup;
inline const auto rv = lv | Empty | Null;
inline const auto cmp_values = Ident | Lookup | Null;

inline const auto grouping =
    (Top <<= File)
    | (File <<= Block)
    | (Block <<= (Freeze | Region | Assign | If)++)
    | (Assign <<= (Lhs >>= lv) * (Rhs >>= rv))
    | (Lookup <<= rv)
    | (Region <<= Ident)
    | (Freeze <<= Ident)
    | (If <<= Eq * Block * Block)
    | (Eq <<= (Lhs >>= cmp_values) * (Rhs >>= cmp_values))
    ;
} // namespace verona::wf

struct Indent {
  size_t indent;
  Token toc;
};

trieste::Parse parser() {
  Parse p{depth::file, verona::wf::parser};

  auto indent = std::make_shared<std::vector<Indent>>();
  indent->push_back({0, File});

  auto update_indent = [indent](detail::Make& m, size_t this_indent) {
    m.term({Assign});

    if (this_indent > indent->back().indent) {
      m.error("unexpected indention");
    }

    while (this_indent < indent->back().indent) {
      m.term({Block, Group, indent->back().toc});
      indent->pop_back();
    }
  };

  p("start",
    {
        // The file always has to start with zero indention
        "\\A(    )+" >> [](auto &m) {
          m.error("unexpected indention");
        },
        // Empty lines should be ignored
        "(?m)\\n$" >> [](auto &) {},

        // Indention
        "\\n((    )*)" >> [update_indent](auto &m) {
          auto this_indent = m.match(1).len / TAB_SIZE;
          update_indent(m, this_indent);
        },
        "\\n(\\t*)" >> [update_indent](auto &m) {
          auto this_indent = m.match(1).len;
          update_indent(m, this_indent);
        },
        "[[:blank:]]+" >> [](auto &) {},

        // Line comment
        "(?:#[^\\n]*)" >> [](auto &) {},

        "if" >> [](auto &m) { 
          m.seq(If);
        },
        "else" >> [](auto &m) { 
          m.seq(Else);
        },
        ":" >> [indent](auto &m) { 
          // Exit conditionals expressions.
          m.term({Eq});

          Token toc = Empty;
          if (m.in(If)) {
            toc = If;
          } else if (m.in(Else)) {
            toc = Else;
          } else {
            m.error("unexpected colon");
            return;
          }
          assert(toc != Empty);

          auto current_indent = indent->back().indent;
          auto next_indent = current_indent + 1;
          indent->push_back({next_indent, toc});

          if (m.in(Group)) {
            m.pop(Group);
          }

          m.push(Block);
        },
        "drop" >> [](auto &m) { m.add(Drop); },
        "freeze" >> [](auto &m) { m.add(Freeze); },
        "region" >> [](auto &m) { m.add(Region); },
        "None" >> [](auto &m) { m.add(Null); },
        "[[:alpha:]]+" >> [](auto &m) { m.add(Ident); },
        "\\[\"([[:alpha:]]+)\"\\]" >> [](auto &m) { m.add(Lookup, 1); },
        "\\.([[:alpha:]]+)" >> [](auto &m) { m.add(Lookup, 1); },
        "==" >> [](auto &m) { m.seq(Eq); },
        "=" >> [](auto &m) { m.seq(Assign); },
        "{}" >> [](auto &m) { m.add(Empty); },
    });

  p.done([update_indent](auto &m) {
    update_indent(m, 0);
  });

  return p;
}

auto LV = T(Ident, Lookup);
auto RV = T(Empty, Ident, Lookup, Null);
auto CMP_V = T(Ident, Lookup, Null);

PassDef grouping() {
  PassDef p{
      "grouping",
      verona::wf::grouping,
      dir::bottomup,
      {
          // Normalize so that all expressions are inside blocks.
          T(File) << (--T(Block) * Any++[File]) >>
            [](auto& _) { return  File << (Block << _[File]); },

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
              return _(If) << _(Block);
            },
      }};

  return p;
}
namespace verona::wf {
using namespace trieste::wf;
inline const trieste::wf::Wellformed flatten =
    (Top <<= File)
    | (File <<= (Freeze | Region | Assign | Eq | Label | Jump | JumpFalse | Print)++)
    | (Assign <<= (Lhs >>= lv) * (Rhs >>= rv))
    | (Lookup <<= rv)
    | (Region <<= Ident)
    | (Freeze <<= Ident)
    | (Eq <<= (Lhs >>= cmp_values) * (Rhs >>= cmp_values))
    | (Label <<= Ident)[Ident];
    ;
} // namespace verona::wf


int g_jump_label_counter = 0;

std::string new_jump_label()
{
  g_jump_label_counter += 1;
  return "label_"+std::to_string(g_jump_label_counter);
}

PassDef flatten() {
  return {
      "flatten",
      verona::wf::flatten,
      dir::bottomup,
      {
        In(File) * (T(Block) << Any++[Block]) >>
          [](auto &_) {
            return Seq << _[Block];
        },
        T(If)[If] << (T(Eq)[Op] * (T(Block) << Any++[Lhs]) * (T(Block) << Any++[Rhs])) >>
          [](auto &_) {
            auto else_label = new_jump_label();
            auto join_label = new_jump_label();

            auto if_str = _(If)->location().view();
            auto if_head = std::string(if_str.substr(0, if_str.find(":") + 1));

            return Seq << _(Op)
                        << (JumpFalse ^ else_label)
                        // Then block
                        << (Print ^ (if_head + " (True)"))
                        << _[Lhs]
                        << (Jump ^ join_label)
                        // Else block
                        << ((Label ^ "else:") << (Ident ^ else_label))
                        << (Print ^ (if_head + " (False)"))
                        << _[Rhs]
                        // Join
                        << (Label << (Ident ^ join_label))
                        ;
        },
      }
  };
}

inline const TokenDef Compile{"compile"};
inline const TokenDef Prelude{"prelude"};

namespace verona::wf {
using namespace trieste::wf;
inline const trieste::wf::Wellformed bytecode =
    empty | (Top <<= (LoadFrame | StoreFrame | LoadField | StoreField | Drop | Null |
                      CreateObject | CreateRegion | FreezeObject | Print | Cmp | Jump |
                      JumpFalse | Label)++)
          | (Label <<= Ident)[Ident];
} // namespace verona::wf

Node create_from(Token def, Node from) {
  return NodeDef::create(def, from->location());
}


std::pair<PassDef, std::shared_ptr<std::optional<Node>>> bytecode() {
  auto result = std::make_shared<std::optional<Node>>();

  PassDef p{"bytecode",
            verona::wf::bytecode,
            dir::topdown,
            {
                T(File)[File] >>
                    [](auto &_) {
                      return Seq << Prelude
                                 << (Compile << *_[File]);
                    },
                
                T(Prelude) >>
                    [](auto &) {
                      return Seq
                        << CreateObject
                        << (StoreFrame ^ "True")
                        << (LoadFrame ^ "True")
                        << FreezeObject
                        << CreateObject
                        << (StoreFrame ^ "False")
                        << (LoadFrame ^ "False")
                        << FreezeObject
                        << (Print ^ "prelude");
                    },

                T(Compile) << (Any[Lhs] * (Any * Any++)[Rhs]) >>
                    [](auto &_) {
                      return Seq << (Compile << _[Lhs])
                                 
                                 << (Compile << _[Rhs]);
                    },

                // The node doesn't require additional processing and should be copied
                T(Compile) << T(Null, Label, Print, Jump, JumpFalse)[Op] >> [](auto &_) -> Node { return _(Op); },

                T(Compile) << (T(Eq)[Eq] << (Any[Lhs] * Any[Rhs])) >>
                    [](auto &_) {
                      return Seq << (Compile << _(Lhs))
                                 << (Compile << _(Rhs))
                                 << Cmp;
                    },

                T(Compile) << (T(Ident)[Ident]) >>
                    [](auto &_) { return create_from(LoadFrame, _(Ident)); },

                T(Compile) << (T(Lookup)[Lookup] << Any[Rhs]) >>
                    [](auto &_) {
                      return Seq << (Compile << _[Rhs])
                                 << create_from(LoadField, _(Lookup));
                    },

                T(Compile) << (T(Assign)[Op] << (T(Ident)[Ident] * Any[Rhs])) >>
                    [](auto &_) {
                      return Seq << (Compile << _[Rhs])
                                 << create_from(StoreFrame, _(Ident))
                                 << create_from(Print, _(Op));
                    },

                T(Compile) << (T(Assign)[Op] << ((T(Lookup)[Lookup] << Any[Lhs]) *
                                             Any[Rhs])) >>
                    [](auto &_) {
                      return Seq << (Compile << _[Lhs]) << (Compile << _[Rhs])
                                 << create_from(StoreField, _(Lookup))
                                 << create_from(Print, _(Op));
                    },

                T(Compile) << (T(Freeze)[Op] << T(Ident)[Ident]) >>
                    [](auto &_) {
                      return Seq << (Compile << _[Ident])
                                 << FreezeObject
                                 << create_from(Print, _(Op));
                    },

                T(Compile) << (T(Region)[Op] << T(Ident)[Ident]) >>
                    [](auto &_) {
                      return Seq << (Compile << _[Ident])
                                 << CreateRegion
                                 << create_from(Print, _(Op));
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

int load_trieste(int argc, char **argv) {
  CLIOptions options;
  auto [bytecodepass, result] = bytecode();
  trieste::Reader reader{"verona_dyn", {grouping(), flatten(), bytecodepass}, parser()};
  trieste::Driver driver{reader, &options};
  auto build_res = driver.run(argc, argv);

  if (build_res == 0 && result->has_value()) {
    verona::interpreter::run(result->value(), options.iterative);
  }
  return build_res;
}
