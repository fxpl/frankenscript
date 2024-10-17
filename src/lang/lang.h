#pragma once

#include "bytecode.h"
#include "trieste/trieste.h"

using namespace trieste;

#define TAB_SIZE 4

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
inline const TokenDef Method{"method"};

inline const TokenDef Op{"op"};
inline const TokenDef Rhs{"rhs"};
inline const TokenDef Lhs{"lhs"};
inline const TokenDef Key{"key"};
inline const TokenDef Value{"value"};
inline const TokenDef Compile{"compile"};

namespace verona::wf
{
  inline const auto lv = Ident | Lookup;
  inline const auto rv = lv | Empty | Null | String | Create | Call | Method;
  inline const auto cmp_values = Ident | Lookup | Null;
  inline const auto key = Ident | Lookup | String;
  inline const auto operand = Lookup | Call | Method | Ident;

  inline const auto grouping = (Top <<= File) | (File <<= Body) |
    (Body <<= Block) |
    (Block <<=
     (Freeze | Region | Assign | If | For | Func | Return | ReturnValue | Call |
      Method)++) |
    (Assign <<= (Lhs >>= lv) * (Rhs >>= rv)) |
    (Lookup <<= (Op >>= operand) * (Rhs >>= key)) | (Region <<= Ident) |
    (Freeze <<= Ident) | (Create <<= Ident) | (If <<= Eq * Block * Block) |
    (For <<= (Key >>= Ident) * (Value >>= Ident) * (Op >>= lv) * Block) |
    (Eq <<= (Lhs >>= cmp_values) * (Rhs >>= cmp_values)) |
    (Func <<= Ident * Params * Body) | (Call <<= Ident * List) |
    (Method <<= Lookup * List) | (ReturnValue <<= rv) | (List <<= rv++) |
    (Params <<= Ident++);

  inline const trieste::wf::Wellformed bytecode = (Top <<= Body) |
    (Body <<=
     (LoadFrame | StoreFrame | LoadField | StoreField | Drop | Null |
      CreateObject | CreateRegion | FreezeObject | IterNext | Print | Eq | Neq |
      Jump | JumpFalse | Label | Call | Return | ReturnValue | ClearStack |
      Dup)++) |
    (CreateObject <<= (Dictionary | String | KeyIter | Proto | Func)) |
    (Func <<= Body) | (Label <<= Ident)[Ident];
}

inline const auto LV = T(Ident, Lookup);
inline const auto RV =
  T(Empty, Ident, Lookup, Null, String, Create, Call, Method);
inline const auto CMP_V = T(Ident, Lookup, Null);
inline const auto KEY = T(Ident, Lookup, String);
inline const auto OPERAND = T(Lookup, Call, Method, Ident);

// Parsing && AST construction
Parse parser();
PassDef grouping();

// Lowering && Bytecode
PassDef call_stmts();
PassDef flatten();
PassDef bytecode();

inline Node create_print(size_t line, std::string text)
{
  std::stringstream ss;
  ss << "Line " << line << ": " << text;
  return NodeDef::create(Print, ss.str());
}
inline Node create_print(Node from, std::string text)
{
  auto [line, col] = from->location().linecol();
  return create_print(line + 1, text);
}
inline Node create_print(Node from)
{
  return create_print(from, std::string(from->location().view()));
}
inline Node create_from(Token def, Node from)
{
  return NodeDef::create(def, from->location());
}
