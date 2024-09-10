#pragma once

#include "trieste/ast.h"

inline const trieste::TokenDef LoadFrame{"load_frame", trieste::flag::print};
inline const trieste::TokenDef StoreFrame{"store_frame", trieste::flag::print};
inline const trieste::TokenDef LoadField{"load_field", trieste::flag::print};
inline const trieste::TokenDef StoreField{"store_field", trieste::flag::print};
inline const trieste::TokenDef CreateObject{"create_object"};
inline const trieste::TokenDef CreateRegion{"create_region"};
inline const trieste::TokenDef FreezeObject{"freeze_object"};
inline const trieste::TokenDef Null{"null"};
inline const trieste::TokenDef Label{"label"};
inline const trieste::TokenDef Cmp{"cmp"};
/// Unconditional Jump
inline const trieste::TokenDef Jump{"jump", trieste::flag::print};
/// Jump if the current stack frame is `False`
inline const trieste::TokenDef JumpFalse{"jump_false", trieste::flag::print};
inline const trieste::TokenDef Print("print", trieste::flag::print);


