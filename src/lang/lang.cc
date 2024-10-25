#include "lang.h"

#include "interpreter.h"
#include "trieste/driver.h"

#include <limits>
#include <optional>

using namespace trieste;

std::pair<PassDef, std::shared_ptr<std::optional<Node>>> extract_bytecode_pass()
{
  auto result = std::make_shared<std::optional<Node>>();
  PassDef p{"interpreter", verona::wf::bytecode, dir::topdown | dir::once, {}};
  p.post(trieste::Top, [result](Node n) {
    *result = n->at(0);
    return 0;
  });

  return {p, result};
}

namespace verona::interpreter
{
  void start(trieste::Node main_body, int step_counter);
}

struct CLIOptions : trieste::Options
{
  int step_counter = std::numeric_limits<int>::max();

  void configure(CLI::App& app)
  {
    app.add_flag(
      "-i,--interactive",
      [&](auto) { step_counter = 0; },
      "Run the interpreter iteratively");
    app.add_option(
      "-s,--step",
      step_counter,
      "Step n instructions before entering interactive mode");
  }
};

int load_trieste(int argc, char** argv)
{
  CLIOptions options;
  auto [extract_bytecode, result] = extract_bytecode_pass();
  trieste::Reader reader{
    "verona_dyn",
    {grouping(), call_stmts(), flatten(), bytecode(), extract_bytecode},
    parser()};
  trieste::Driver driver{reader, &options};
  auto build_res = driver.run(argc, argv);

  if (build_res == 0 && result->has_value())
  {
    verona::interpreter::start(result->value(), options.step_counter);
  }
  return build_res;
}
