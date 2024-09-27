#pragma once

#include <vector>
#include <optional>

namespace objects {
    struct UI;
    class DynObject;
}

namespace verona::interpreter {
    struct Bytecode;
}

namespace verona::interpreter {
/// @brief This executes the the given function body with the given stack.
/// The frame will be pushed and poped by the function callee. It's recommendable
/// to have a print at the start and end of the function body.
///
/// The stack holds the arguments in order of their definition. They have to
/// be popped of in reverse order.
std::optional<objects::DynObject *> run_body(Bytecode *body, std::vector<objects::DynObject *> &stack, objects::UI* ui);
} // namespace verona::interpreter
