#include "bytecode.h"
#include "trieste/driver.h"
#include "trieste/trieste.h"
#include "interpreter.h"
#include <optional>

#define TAB_SIZE 4

using namespace trieste;

inline const TokenDef Ident{"ident", trieste::flag::print};
inline const TokenDef Assign{"assign"};
inline const TokenDef Create{"create"};
inline const TokenDef For{"for"};
inline const TokenDef If{"if"};
inline const TokenDef Else{"else"};
inline const TokenDef Block{"block"};
inline const TokenDef Empty{"empty"};
inline const TokenDef Drop{"drop"};
inline const TokenDef Freeze{"freeze"};
inline const TokenDef Region{"region"};
inline const TokenDef Lookup{"lookup"};
inline const TokenDef Parens{"parens"};
inline const TokenDef Return{"return"};

inline const TokenDef Op{"op"};
inline const TokenDef Rhs{"rhs"};
inline const TokenDef Lhs{"lhs"};
inline const TokenDef Key{"key"};
inline const TokenDef Value{"value"};

namespace verona::wf {
using namespace trieste::wf;

inline const auto parse_tokens = Region | Ident | Lookup | Empty | Freeze | Drop | Null | String | Create | Parens;
inline const auto parse_groups = Group | Assign | If | Else | Block | For | Func | List | Return;

inline const auto parser =
  (Top <<= File)
  | (File <<= parse_groups++)
  | (Assign <<= Group++)
  | (If <<= Group * Eq * Group)
  | (Else <<= Group * Group)
  | (Group <<= (parse_tokens | Block | List)++)
  | (Block <<= (parse_tokens | parse_groups)++)
  | (Eq <<= Group * Group)
  | (Lookup <<= Group)
  | (For <<= Group * List * Group * Group)
  | (List <<= Group++)
  | (Parens <<= (Group | List)++)
  | (Func <<= Group * Group * Group)
  | (Return <<= Group * Group)
  ;

inline const auto lv = Ident | Lookup;
inline const auto rv = lv | Empty | Null | String | Create | Call;
inline const auto cmp_values = Ident | Lookup | Null;
inline const auto key = Ident | Lookup | String;

inline const auto grouping =
    (Top <<= File)
    | (File <<= Body)
    | (Body <<= Block)
    | (Block <<= (Freeze | Region | Assign | If | For | Func | Return | Call)++)
    | (Assign <<= (Lhs >>= lv) * (Rhs >>= rv))
    | (Lookup <<= (Lhs >>= lv) * (Rhs >>= key))
    | (Region <<= Ident)
    | (Freeze <<= Ident)
    | (Create <<= Ident)
    | (If <<= Eq * Block * Block)
    | (For <<= (Key >>= Ident) * (Value >>= Ident) * (Op >>= lv) * Block)
    | (Eq <<= (Lhs >>= cmp_values) * (Rhs >>= cmp_values))
    | (Func <<= Ident * Params * Body)
    | (Call <<= (Op >>= key) * List)
    | (Return <<= rv)
    | (List <<= rv++)
    | (Params <<= Ident++)
    ;
inline const auto stmt_prints =
  grouping
  | (Block <<= (Freeze | Region | Assign | If | For | Func | Return | Call | ClearStack | Print)++);
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
    m.term({Assign, Return});

    if (this_indent > indent->back().indent) {
      std::cout << "this_indent " << this_indent << std::endl;
      std::cout << "indent->back().indent " << indent->back().indent << std::endl;
      m.error("unexpected indention");
    }

    while (this_indent < indent->back().indent) {
      m.term({Block, Group, indent->back().toc});
      indent->pop_back();
    }

    if (this_indent != indent->back().indent) {
      m.error("unexpected indention");
    }
  };

  p("start",
    {
        // Empty lines should be ignored
        "[ \\t]*\\r?\\n" >> [](auto &) {},

        // Line comment
        "(?:#[^\\n]*)" >> [](auto &) {},

        // Indention
        " +" >> [update_indent](auto &m) {
          if (m.match().len % TAB_SIZE != 0) {
            m.error("unexpected indention");
          }
          auto this_indent = m.match().len / TAB_SIZE;
          update_indent(m, this_indent);
          m.mode("contents");
        },

        "\\t*" >> [update_indent](auto &m) {
          auto this_indent = m.match().len;
          update_indent(m, this_indent);
          m.mode("contents");
        },
    });

  p(
    "contents",
    {
        // Indentation 
        "\\r?\\n" >> [](auto &m) {
          m.mode("start");
        },

        "[[:blank:]]+" >> [](auto &) {},

        // Line comment
        "(?:#[^\\n\\r]*)" >> [](auto &) {},

        "def" >> [](auto &m) {
          m.seq(Func);
        },
        "\\(" >> [](auto &m) {
          m.push(Parens);
        },
        "\\)" >> [](auto &m) {
          m.term({List, Parens});
          m.extend(Parens);
        },
        "return" >> [](auto &m) {
          m.seq(Return);
        },

        "for" >> [](auto &m) { 
          m.seq(For);
        },
        "in" >> [](auto &m) { 
          // In should always be in a list from the identifiers.
          m.term({List});
        },
        "," >> [](auto &m) { 
          m.seq(List);
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
          } else if (m.in(Func)) {
            toc = Func;
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
        "create" >> [](auto &m) { m.add(Create); },
        "freeze" >> [](auto &m) { m.add(Freeze); },
        "region" >> [](auto &m) { m.add(Region); },
        "None" >> [](auto &m) { m.add(Null); },
        "[0-9A-Za-z_]+" >> [](auto &m) { m.add(Ident); },
        "\\[" >> [](auto &m) {
          m.push(Lookup);
        },
        "\\]" >> [](auto &m) {
          m.term({Lookup});
        },
        "\\.([0-9A-Za-z_]+)" >> [](auto &m) {
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
auto RV = T(Empty, Ident, Lookup, Null, String, Create, Call);
auto CMP_V = T(Ident, Lookup, Null);
auto KEY = T(Ident, Lookup, String);

Node create_print(size_t line, std::string text) {
  std::stringstream ss;
  ss << "Line " << line << ": " << text;
  return NodeDef::create(Print, ss.str());
}
Node create_print(Node from, std::string text) {
  auto [line, col] = from->location().linecol();
  return create_print(line + 1, text);
}
Node create_print(Node from) {
  return create_print(from, std::string(from->location().view()));
}
Node create_from(Token def, Node from) {
  return NodeDef::create(def, from->location());
}
std::string expr_header(Node expr) {
  auto expr_str = expr->location().view();
  return std::string(expr_str.substr(0, expr_str.find(":") + 1));
}

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
              (T(Group) << (RV[Rhs] * End)) *
              End) >>
              [](auto &_) {
                return create_from(Return, _(Return)) << _(Rhs);
            },

      }};


  return p;
}

PassDef stmt_prints() {
  PassDef p{
      "stmt_prints",
      verona::wf::stmt_prints,
      dir::bottomup | dir::once,
      {
        In(Block) * T(Call)[Call] >>
          [](auto &_) {
            return Seq
              << _(Call)
              << ClearStack
              << create_print(_(Call));
          },
      }
  };

  return p;
}

Node return_ident() {
  return Ident ^ "return";
}

inline const TokenDef Compile{"compile"};

namespace verona::wf {
using namespace trieste::wf;
inline const trieste::wf::Wellformed flatten =
    (Top <<= File)
    | (File <<= Body)
    | (Body <<= (Freeze | Region | Assign | Eq | Neq | Label | Jump | JumpFalse |
                Print | StoreFrame | LoadFrame | CreateObject | Ident | IterNext |
                Create | StoreField | Lookup | String | Call | PushFrame | PopFrame |
                ClearStack)++)
    | (CreateObject <<= (KeyIter | String | Dictionary | Func))
    | (Func <<= Compile)
    | (Compile <<= Body)
    | (Create <<= Ident)
    | (Assign <<= (Lhs >>= lv) * (Rhs >>= rv))
    | (Lookup <<= (Lhs >>= lv) * (Rhs >>= key))
    | (Region <<= Ident)
    | (Freeze <<= Ident)
    | (Call <<= (Op >>= key) * List)
    | (List <<= rv++)
    | (Params <<= Ident++)
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
        In(Body) * (T(Block) << Any++[Block]) >>
          [](auto &_) {
            return Seq << _[Block];
        },
        T(If)[If] << (T(Eq)[Op] * (T(Block) << Any++[Lhs]) * (T(Block) << Any++[Rhs])) >>
          [](auto &_) {
            auto else_label = new_jump_label();
            auto join_label = new_jump_label();

            auto if_head = expr_header(_(If));

            return Seq << _(Op)
                        << (JumpFalse ^ else_label)
                        // Then block
                        << create_print(_(If), if_head + " (True)")
                        << _[Lhs]
                        << (Jump ^ join_label)
                        // Else block
                        << ((Label ^ "else:") << (Ident ^ else_label))
                        << create_print(_(If), if_head + " (False)")
                        << _[Rhs]
                        // Join
                        << (Label << (Ident ^ join_label))
                        ;
        },
        T(For)[For] << (
            T(Ident)[Key] *
            T(Ident)[Value] *
            LV[Op] *
            (T(Block) << Any++[Block]) *
            End) >>
          [](auto &_) {
            auto it_name = new_iter_name();

            auto start_label = new_jump_label();
            auto break_label = new_jump_label();

            auto for_head = expr_header(_(For));

            return Seq
              // Prelude
              << clone(_(Op))
              << (CreateObject << KeyIter)
              << (StoreFrame ^ it_name)
              << (Ident ^ it_name)
              << (String ^ "source")
              << _(Op)
              << (StoreField)
              << create_print(_(For), "create " + it_name)
              << ((Label ^ "start:") << (Ident ^ start_label))
              // key = iter++
              << (Ident ^ it_name)
              << (IterNext)
              << create_from(StoreFrame, _(Key))
              // While (key != null)
              << (Neq
                  << create_from(Ident, _(Key))
                  << Null)
              << (JumpFalse ^ break_label)
              // value = it.source.key
              << (Lookup
                  << (Lookup
                      << (Ident ^ it_name)
                      << (String ^ "source"))
                  << create_from(Ident, _(Key)))
              << create_from(StoreFrame, _(Value))
              << create_print(_(For), for_head + " (Next)")
              // Block
              << _[Block]
              // Final cleanup
              << (Jump ^ start_label)
              << ((Label ^ "break:") << (Ident ^ break_label))
              << create_print(_(For), for_head + " (Break)")
              << ((Assign ^ ("drop " + std::string(_(Value)->location().view()))) << create_from(Ident, _(Value)) << Null)
              << ((Assign ^ ("drop " + it_name)) << (Ident ^ it_name) << Null);
          },

        T(Func)[Func] << (
          T(Ident)[Ident] *
          T(Params)[Params] *
          (T(Body)[Body] << Any++[Block])*
          End) >>
          [](auto &_) {
            auto return_label = new_jump_label();
            auto func_head = expr_header(_(Func));

            // Function setup
            Node body = Body;
            body << PushFrame;
            Node args = _(Params);
            for (auto it = args->cbegin(); it != args->cend(); it++) {
              body << create_from(StoreFrame, *it);
            }
            body << create_print(_(Func), func_head + " (Enter)");

            // Function body
            auto block = _[Block];
            for (auto stmt : block) {
              if (stmt == Return) {
                body << (create_from(Assign, stmt) << return_ident() << stmt->at(0));
                body << (Jump ^ return_label);
              } else {
                body << stmt;
              }
            }

            // Function cleanup
            body << ((Label ^ "return:") << (Ident ^ return_label));
            body << create_print(_(Func), func_head + " (Exit)");
            body << PopFrame;

            return Seq
              << (CreateObject << (Func << (Compile << body)))
              << (StoreFrame ^ _(Ident))
              << create_print(_(Func), func_head);
          },
      }
  };
}

inline const TokenDef Prelude{"prelude"};
inline const TokenDef Postlude{"postlude"};

namespace verona::wf {
using namespace trieste::wf;
inline const trieste::wf::Wellformed bytecode =
    empty | (Body <<= (LoadFrame | StoreFrame | LoadField | StoreField | Drop | Null |
                      CreateObject | CreateRegion | FreezeObject | IterNext | Print |
                      Eq | Neq | Jump | JumpFalse | Label | Call | PushFrame | PopFrame |
                      ClearStack)++)
          | (CreateObject <<= (Dictionary | String | KeyIter | Proto | Func))
          | (Top <<= Body)
          | (Func <<= Body)
          | (Label <<= Ident)[Ident];
} // namespace verona::wf

PassDef bytecode() {
  PassDef p{"bytecode",
            verona::wf::bytecode,
            dir::topdown,
            {
                T(File) << T(Body)[Body] >>
                    [](auto &_) {
                      return Body
                        << Prelude
                        << (Compile << *_[Body])
                        << Postlude;
                    },
                T(Compile) << (T(Body)[Body] << Any++[Block]) >>
                    [](auto &_) {
                      return create_from(Body, _(Body)) << (Compile << _[Block]);
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
                        << create_print(0, "prelude");
                    },
                T(Postlude) >>
                    [](auto &) {
                      return Seq
                        << Null
                        << (StoreFrame ^ "True")
                        << Null
                        << (StoreFrame ^ "False")
                        << create_print(0, "postlude");
                    },

                T(Compile) << (Any[Lhs] * (Any * Any++)[Rhs]) >>
                    [](auto &_) {
                      return Seq << (Compile << _[Lhs])
                                 << (Compile << _[Rhs]);
                    },

                // The node doesn't require additional processing and should be copied
                T(Compile) << T(
                  Null, Label, Print, Jump, JumpFalse, CreateObject,
                  StoreFrame, LoadFrame, IterNext, StoreField, PushFrame,
                  PopFrame, ClearStack)[Op] >>
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
                                 << create_print(_(Op));
                    },

                T(Compile) << (T(Assign)[Assign] << ((
                    T(Lookup)[Lookup] << (Any[Op] * Any[Key] * End)) *
                    Any[Rhs])) >>
                    [](auto &_) {
                      return Seq << (Compile << _[Op])
                                 << (Compile << _[Key])
                                 << (Compile << _[Rhs])
                                 << create_from(StoreField, _(Lookup))
                                 << create_print(_(Assign));
                    },

                T(Compile) << (T(Freeze)[Op] << T(Ident)[Ident]) >>
                    [](auto &_) {
                      return Seq << (Compile << _[Ident])
                                 << FreezeObject
                                 << create_print(_(Op));
                    },

                T(Compile) << (T(Create)[Op] << T(Ident)[Ident]) >>
                    [](auto &_) {
                      return Seq << (Compile << _[Ident])
                                 << (CreateObject << Proto);
                    },

                T(Compile) << (T(Region)[Op] << T(Ident)[Ident]) >>
                    [](auto &_) {
                      return Seq << (Compile << _[Ident])
                                 << CreateRegion
                                 << create_print(_(Op));
                    },
                
                T(Compile) << (
                  T(Call)[Call] << (
                    KEY[Op] *
                    (T(List) << Any++[List]) *
                    End)) >>
                    [](auto &_) {
                      // The print is done by the called function
                      return Seq
                        << (Compile << _[List])
                        << (Compile << _(Op))
                        << (Call ^ std::to_string(_[List].size()));
                    },

                T(Compile) << End >>
                [](auto &) -> Node { return {}; },

                T(Compile) << (T(Empty)) >>
                    [](auto &) -> Node { return CreateObject << Dictionary; },
                T(Compile) << (T(String)[String]) >>
                    [](auto &_) -> Node { return CreateObject << _(String); },

            }};
  return p;
}

std::pair<PassDef, std::shared_ptr<std::optional<Node>>> extract_bytecode_pass() {
  auto result = std::make_shared<std::optional<Node>>();
  PassDef p{
    "interpreter",
    verona::wf::bytecode,
    dir::topdown | dir::once,
    {}
  };
  p.post(trieste::Top, [result](Node n) {
    *result = n->at(0);
    return 0;
  });

  return {p, result};
}

namespace verona::interpreter {
  void start(trieste::Node main_body, bool interactive);
}

struct CLIOptions : trieste::Options
{
  bool iterative = false;

  void configure(CLI::App &app)
  {
    app.add_flag("-i,--interactive", iterative, "Run the interpreter iteratively");
  }
};

int load_trieste(int argc, char **argv) {
  CLIOptions options;
  auto [extract_bytecode, result] = extract_bytecode_pass();
  trieste::Reader reader{"verona_dyn", {grouping(), stmt_prints(), flatten(), bytecode(), extract_bytecode}, parser()};
  trieste::Driver driver{reader, &options};
  auto build_res = driver.run(argc, argv);

  if (build_res == 0 && result->has_value()) {
    verona::interpreter::start(result->value(), options.iterative);
  }
  return build_res;
}
