#pragma once

#include "trieste/ast.h"

inline const trieste::TokenDef LoadFrame{"load_frame", trieste::flag::print};
inline const trieste::TokenDef StoreFrame{"store_frame", trieste::flag::print};
inline const trieste::TokenDef LoadField{"load_field"};
inline const trieste::TokenDef StoreField{"store_field"};
inline const trieste::TokenDef CreateObject{"create_object"};
inline const trieste::TokenDef Proto{"prototype"};
inline const trieste::TokenDef Dictionary{"dictionary"};
inline const trieste::TokenDef String{"string", trieste::flag::print};
inline const trieste::TokenDef KeyIter{"key_iter"};
inline const trieste::TokenDef Func{"func"};
inline const trieste::TokenDef List{"list"};
inline const trieste::TokenDef Params{"params"};
inline const trieste::TokenDef Body{"body", trieste::flag::symtab};
inline const trieste::TokenDef Return{"return"};
inline const trieste::TokenDef ReturnValue{"return_value"};

inline const trieste::TokenDef CreateRegion{"create_region"};
inline const trieste::TokenDef FreezeObject{"freeze_object"};
inline const trieste::TokenDef Null{"null"};
inline const trieste::TokenDef Label{"label"};
inline const trieste::TokenDef Eq{"=="};
inline const trieste::TokenDef Neq{"!="};
/// Stack: <arg_0>::<arg_1>::<arg_2>::<func_obj>
/// For `function(a, b, c)` the stack would be: `a, b, c, function`
inline const trieste::TokenDef Call{"call"};
/// This clears any potentual values from the current stack.
inline const trieste::TokenDef ClearStack{"clear_stack"};
/// Unconditional Jump
inline const trieste::TokenDef Jump{"jump", trieste::flag::print};
/// Jump if the current stack frame is `False`
inline const trieste::TokenDef JumpFalse{"jump_false", trieste::flag::print};
inline const trieste::TokenDef Print("print", trieste::flag::print);
inline const trieste::TokenDef IterNext("iter_next");

