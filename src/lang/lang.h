#pragma once

#include "bytecode.h"
#include "trieste/trieste.h"

using namespace trieste;

#define TAB_SIZE 4

inline const TokenDef Ident{"ident", trieste::flag::print};
inline const TokenDef Assign{"assign"};
inline const TokenDef For{"for"};
inline const TokenDef While{"while"};
inline const TokenDef If{"if"};
inline const TokenDef Else{"else"};
inline const TokenDef Block{"block"};
inline const TokenDef Empty{"empty"};
inline const TokenDef Drop{"drop"};
inline const TokenDef Move{"move"};
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
  inline const auto cond = Eq | Neq;
  inline const auto lv = Ident | Lookup;
  inline const auto rv =
    lv | Empty | Null | String | Call | Method | Move | cond;
  inline const auto cmp_values = Ident | Lookup | Null | Call | Method;
  inline const auto key = Ident | Lookup | String;
  inline const auto operand = Lookup | Call | Method | Ident;

  inline const auto grouping = (Top <<= File) | (File <<= Body) |
    (Body <<= Block) |
    (Block <<=
     (Assign | If | For | While | Func | Return | ReturnValue | Call |
      Method)++) |
    (Assign <<= (Lhs >>= lv) * (Rhs >>= rv)) | (Move <<= (Lhs >>= lv)) |
    (Lookup <<= (Op >>= operand) * (Rhs >>= key)) |
    (If <<= (Op >>= cond) * Block * Block) | (While <<= (Op >>= cond) * Block) |
    (While <<= (Op >>= cond) * Block) |
    (For <<= (Key >>= Ident) * (Value >>= Ident) * (Op >>= lv) * Block) |
    (Eq <<= (Lhs >>= cmp_values) * (Rhs >>= cmp_values)) |
    (Neq <<= (Lhs >>= cmp_values) * (Rhs >>= cmp_values)) |
    (Func <<= Ident * Params * Body) | (Call <<= Ident * List) |
    (Method <<= Lookup * List) | (ReturnValue <<= rv) | (List <<= rv++) |
    (Params <<= Ident++);

  inline const trieste::wf::Wellformed bytecode = (Top <<= Body) |
    (Body <<=
     (LoadFrame | LoadGlobal | StoreFrame | SwapFrame | LoadField | StoreField |
      SwapField | Drop | Null | CreateObject | IterNext | Print | Eq | Neq |
      Jump | JumpFalse | Label | Call | Return | ReturnValue | ClearStack |
      Dup)++) |
    (CreateObject <<= (Dictionary | String | KeyIter | Func)) |
    (Func <<= Body) | (Label <<= Ident)[Ident];
}

inline const auto COND = T(Eq, Neq);
inline const auto LV = T(Ident, Lookup);
inline const auto RV =
  T(Empty, Ident, Lookup, Null, String, Call, Method, Move, Eq, Neq);
inline const auto CMP_V = T(Ident, Lookup, Null, Call, Method);
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
