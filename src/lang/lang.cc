#include "bytecode.h"
#include "trieste/driver.h"
#include "trieste/trieste.h"
#include <optional>

#define TAB_SIZE 4

using namespace trieste;

inline const TokenDef Ident{"ident", trieste::flag::print};
inline const TokenDef Assign{"assign"};
inline const TokenDef For{"for"};
inline const TokenDef If{"if"};
inline const TokenDef Else{"else"};
inline const TokenDef Block{"block"};
inline const TokenDef Empty{"empty"};
inline const TokenDef Drop{"drop"};
inline const TokenDef Freeze{"freeze"};
inline const TokenDef Region{"region"};
inline const TokenDef Lookup{"lookup"};

inline const TokenDef Op{"op"};
inline const TokenDef Rhs{"rhs"};
inline const TokenDef Lhs{"lhs"};
inline const TokenDef Key{"key"};

namespace verona::wf {
using namespace trieste::wf;

inline const auto parse_tokens = Region | Ident | Lookup | Empty | Freeze | Drop | Null | String;
inline const auto parse_groups = Group | Assign | If | Else | Block | For;

inline const auto parser =
  (Top <<= File)
  | (File <<= parse_groups++)
  | (Assign <<= Group++)
  | (If <<= Group * Eq * Group)
  | (Else <<= Group * Group)
  | (Group <<= (parse_tokens | Block)++)
  | (Block <<= (parse_tokens | parse_groups)++)
  | (Eq <<= Group * Group)
  | (Lookup <<= Group)
  | (For <<= Group * Group * Group * Group)
  ;

inline const auto lv = Ident | Lookup;
inline const auto rv = lv | Empty | Null | String;
inline const auto cmp_values = Ident | Lookup | Null;
inline const auto key = Ident | Lookup | String;

inline const auto grouping =
    (Top <<= File)
    | (File <<= Block)
    | (Block <<= (Freeze | Region | Assign | If | For)++)
    | (Assign <<= (Lhs >>= lv) * (Rhs >>= rv))
    | (Lookup <<= (Lhs >>= lv) * (Rhs >>= key))
    | (Region <<= Ident)
    | (Freeze <<= Ident)
    | (If <<= Eq * Block * Block)
    | (For <<= Ident * (Op >>= lv) * Block)
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

        "for" >> [](auto &m) { 
          m.seq(For);
        },
        "in" >> [](auto &m) { 
          // In should always be in a group, from the entry identifier.
          m.pop(Group);
        },

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
          } else if (m.in(For)) {
            toc = For;
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
        "\\[" >> [](auto &m) {
          m.push(Lookup);
        },
        "\\]" >> [](auto &m) {
          m.term({Lookup});
        },
        "\\.([[:alpha:]]+)" >> [](auto &m) {
          m.push(Lookup);
          m.add(String, 1);
          m.term({Lookup});
        },
        "\"([^\\n\"]+)\"" >> [](auto &m) { m.add(String, 1); },
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
auto RV = T(Empty, Ident, Lookup, Null, String);
auto CMP_V = T(Ident, Lookup, Null);
auto KEY = T(Ident, Lookup, String);

Node create_from(Token def, Node from) {
  return NodeDef::create(def, from->location());
}

PassDef grouping() {
  PassDef p{
      "grouping",
      verona::wf::grouping,
      dir::bottomup,
      {
          // Normalize so that all expressions are inside blocks.
          T(File) << (--T(Block) * Any++[File]) >>
            [](auto& _) { return  File << (Block << _[File]); },

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
              // This adds an empty else block, if no else was written
              return _(If) << Block;
            },

          T(For)[For] << (
              (T(Group)) *
              (T(Group) << (T(Ident)[Ident] * End)) *
              (T(Group) << (LV[Op] * End)) *
              (T(Group) << (T(Block)[Block] * End)) *
              End) >>
            [](auto &_) {
              return create_from(For, _(For))
                << _(Ident)
                << _(Op)
                << _(Block);
            },
      }};

  return p;
}
namespace verona::wf {
using namespace trieste::wf;
inline const trieste::wf::Wellformed flatten =
    (Top <<= File)
    | (File <<= (Freeze | Region | Assign | Eq | Neq | Label | Jump | JumpFalse |
                Print | StoreFrame | LoadFrame | CreateObject | Ident | IterNext |
                StoreField | Lookup | String)++)
    | (CreateObject <<= (KeyIter | String | Dictionary))
    | (Assign <<= (Lhs >>= lv) * (Rhs >>= rv))
    | (Lookup <<= (Lhs >>= lv) * (Rhs >>= key))
    | (Region <<= Ident)
    | (Freeze <<= Ident)
    | (Eq <<= (Lhs >>= cmp_values) * (Rhs >>= cmp_values))
    | (Neq <<= (Lhs >>= cmp_values) * (Rhs >>= cmp_values))
    | (Label <<= Ident)[Ident];
    ;
} // namespace verona::wf

int g_jump_label_counter = 0;
std::string new_jump_label()
{
  g_jump_label_counter += 1;
  return "label_"+std::to_string(g_jump_label_counter);
}

int g_iter_name = 0;
std::string new_iter_name()
{
  g_iter_name += 1;
  return "_iter_"+std::to_string(g_iter_name);
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
        T(For)[For] << (T(Ident)[Ident] * LV[Op] * (T(Block) << Any++[Block]) * End) >>
          [](auto &_) {
            auto it_name = new_iter_name();

            auto start_label = new_jump_label();
            auto break_label = new_jump_label();

            auto for_str = _(For)->location().view();
            auto for_head = std::string(for_str.substr(0, for_str.find(":") + 1));

            auto entry_name = _(Ident)->location();

            return Seq
              // Prelude
              << clone(_(Op))
              << (CreateObject << KeyIter)
              << (StoreFrame ^ it_name)
              << (Ident ^ it_name)
              << (String ^ "source")
              << _(Op)
              << (StoreField)
              << (Print ^ ("create " + it_name))
              << (CreateObject << Dictionary)
              << (StoreFrame ^ entry_name)
              << ((Label ^ "start:") << (Ident ^ start_label))
              // entry.key = iter++
              << (Ident ^ entry_name)
              << (String ^ "key")
              << (Ident ^ it_name)
              << (IterNext)
              << (StoreField)
              // While (entry.key != null)
              << (Neq
                  << (Lookup
                      << (Ident ^ entry_name)
                      << (String ^ "key"))
                  << Null)
              << (JumpFalse ^ break_label)
              // entry.value = it.source.key
              << (Ident ^ entry_name)
              << (String ^ "value")
              << (Lookup
                  << (Lookup
                      << (Ident ^ it_name)
                      << (String ^ "source"))
                  << (Lookup
                      << (Ident ^ entry_name)
                      << (String ^ "key")))
              << (StoreField)
              << (Print ^ (for_head + " (Next)"))
              // Block
              << _[Block]
              // Final cleanup
              << (Jump ^ start_label)
              << ((Label ^ "break:") << (Ident ^ break_label))
              << (Print ^ (for_head + " (Break)"))
              << ((Assign ^ ("drop " + std::string(entry_name.view()))) << (Ident ^ entry_name) << Null)
              << ((Assign ^ ("drop " + it_name)) << (Ident ^ it_name) << Null);
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
                      CreateObject | CreateRegion | FreezeObject | IterNext | Print |
                      Eq | Neq | Jump | JumpFalse | Label)++)
          | (CreateObject <<= (Dictionary | String | KeyIter))
          | (Label <<= Ident)[Ident];
} // namespace verona::wf

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
                        << (CreateObject << (String ^ "True"))
                        << (StoreFrame ^ "True")
                        << (LoadFrame ^ "True")
                        << FreezeObject
                        << (CreateObject << (String ^ "False"))
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
                T(Compile) << T(
                  Null, Label, Print, Jump, JumpFalse, CreateObject,
                  StoreFrame, LoadFrame, IterNext, StoreField)[Op] >>
                    [](auto &_) -> Node { return _(Op); },

                T(Compile) << (T(Eq, Neq)[Op] << (Any[Lhs] * Any[Rhs])) >>
                    [](auto &_) {
                      return Seq << (Compile << _(Lhs))
                                 << (Compile << _(Rhs))
                                 << _(Op)->type();
                    },

                T(Compile) << (T(Ident)[Ident]) >>
                    [](auto &_) { return create_from(LoadFrame, _(Ident)); },

                T(Compile) << (T(Lookup)[Lookup] << (Any[Op] * Any[Key] * End)) >>
                    [](auto &_) {
                      return Seq << (Compile << _[Op])
                                 << (Compile << _[Key])
                                 << create_from(LoadField, _(Lookup));
                    },

                T(Compile) << (T(Assign)[Op] << (T(Ident)[Ident] * Any[Rhs])) >>
                    [](auto &_) {
                      return Seq << (Compile << _[Rhs])
                                 << create_from(StoreFrame, _(Ident))
                                 << create_from(Print, _(Op));
                    },

                T(Compile) << (T(Assign)[Assign] << ((
                    T(Lookup)[Lookup] << (Any[Op] * Any[Key] * End)) *
                    Any[Rhs])) >>
                    [](auto &_) {
                      return Seq << (Compile << _[Op])
                                 << (Compile << _[Key])
                                 << (Compile << _[Rhs])
                                 << create_from(StoreField, _(Lookup))
                                 << create_from(Print, _(Assign));
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
                    [](auto &) -> Node { return CreateObject << Dictionary; },
                T(Compile) << (T(String)[String]) >>
                    [](auto &_) -> Node { return CreateObject << _(String); },

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
