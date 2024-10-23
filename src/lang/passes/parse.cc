#include "../lang.h"

namespace verona::wf
{
  using namespace trieste::wf;

  inline const auto parse_tokens = Region | Ident | Lookup | Empty | Freeze |
    Drop | Take | Null | String | Create | Parens;
  inline const auto parse_groups =
    Group | Assign | If | Else | Block | For | Func | List | Return;

  inline const auto parser = (Top <<= File) | (File <<= parse_groups++) |
    (Assign <<= Group++) | (If <<= Group * (Op >>= Cond) * Group) |
    (Else <<= Group * Group) | (Group <<= (parse_tokens | Block | List)++) |
    (Block <<= (parse_tokens | parse_groups)++) | (Eq <<= Group * Group) |
    (Neq <<= Group * Group) | (Lookup <<= Group) |
    (For <<= Group * List * Group * Group) | (List <<= Group++) |
    (Parens <<= (Group | List)++) | (Func <<= Group * Group * Group) |
    (Return <<= Group++);
}

struct Indent
{
  size_t indent;
  Token toc;
};

trieste::Parse parser()
{
  Parse p{depth::file, verona::wf::parser};

  auto indent = std::make_shared<std::vector<Indent>>();
  indent->push_back({0, File});

  auto update_indent = [indent](detail::Make& m, size_t this_indent) {
    m.term({Assign, Return});

    if (this_indent > indent->back().indent)
    {
      std::cout << "this_indent " << this_indent << std::endl;
      std::cout << "indent->back().indent " << indent->back().indent
                << std::endl;
      m.error("unexpected indention");
    }

    while (this_indent < indent->back().indent)
    {
      m.term({Block, Group, indent->back().toc});
      indent->pop_back();
    }

    if (this_indent != indent->back().indent)
    {
      m.error("unexpected indention");
    }
  };

  p("start",
    {
      // Empty lines should be ignored
      "[ \\t]*\\r?\\n" >> [](auto&) {},

      // Line comment
      "(?:#[^\\n]*)" >> [](auto&) {},

      // Indention
      " +" >>
        [update_indent](auto& m) {
          if (m.match().len % TAB_SIZE != 0)
          {
            m.error("unexpected indention");
          }
          auto this_indent = m.match().len / TAB_SIZE;
          update_indent(m, this_indent);
          m.mode("contents");
        },

      "\\t*" >>
        [update_indent](auto& m) {
          auto this_indent = m.match().len;
          update_indent(m, this_indent);
          m.mode("contents");
        },
    });

  p("contents",
    {
      // Indentation
      "\\r?\\n" >> [](auto& m) { m.mode("start"); },

      "[[:blank:]]+" >> [](auto&) {},

      // Line comment
      "(?:#[^\\n\\r]*)" >> [](auto&) {},

      "def\\b" >> [](auto& m) { m.seq(Func); },
      "\\(" >> [](auto& m) { m.push(Parens); },
      "\\)" >>
        [](auto& m) {
          m.term({List, Parens});
          m.extend(Parens);
        },
      "return\\b" >> [](auto& m) { m.seq(Return); },

      "for\\b" >> [](auto& m) { m.seq(For); },
      "in\\b" >>
        [](auto& m) {
          // In should always be in a list from the identifiers.
          m.term({List});
        },
      "," >> [](auto& m) { m.seq(List); },

      "if\\b" >>
        [](auto& m) {
          m.term();
          m.seq(If);
        },
      "else\\b" >> [](auto& m) { m.seq(Else); },
      ":" >>
        [indent](auto& m) {
          // Exit conditionals expressions.
          m.term({Eq, Neq});

          Token toc = Empty;
          if (m.in(If))
          {
            toc = If;
          }
          else if (m.in(Else))
          {
            toc = Else;
          }
          else if (m.in(For))
          {
            toc = For;
          }
          else if (m.in(Func))
          {
            toc = Func;
          }
          else
          {
            m.error("unexpected colon");
            return;
          }
          assert(toc != Empty);

          auto current_indent = indent->back().indent;
          auto next_indent = current_indent + 1;
          indent->push_back({next_indent, toc});

          if (m.in(Group))
          {
            m.pop(Group);
          }

          m.push(Block);
        },
      "drop\\b" >> [](auto& m) { m.add(Drop); },
      "take\\b" >> [](auto& m) { m.add(Take); },
      "create\\b" >> [](auto& m) { m.add(Create); },
      "freeze\\b" >> [](auto& m) { m.add(Freeze); },
      "region\\b" >> [](auto& m) { m.add(Region); },
      "None\\b" >> [](auto& m) { m.add(Null); },
      "[0-9A-Za-z_]+" >> [](auto& m) { m.add(Ident); },
      "\\[" >> [](auto& m) { m.push(Lookup); },
      "\\]" >> [](auto& m) { m.term({Lookup}); },
      "\\.([0-9A-Za-z_]+)" >>
        [](auto& m) {
          m.push(Lookup);
          m.add(String, 1);
          m.term({Lookup});
        },
      "\"([^\\n\"]+)\"" >> [](auto& m) { m.add(String, 1); },
      "==" >> [](auto& m) { m.seq(Eq); },
      "!=" >> [](auto& m) { m.seq(Neq); },
      "=" >> [](auto& m) { m.seq(Assign); },
      "{}" >> [](auto& m) { m.add(Empty); },
    });

  p.done([update_indent](auto& m) { update_indent(m, 0); });

  return p;
}
