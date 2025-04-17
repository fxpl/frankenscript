# FrankenScript Dynamic Language experiment

This repository contains a toy programming language called "FrankenScript" with Python-like syntax and Python/JavaScript-inspired semantics.
The purpose of FrankenScript is to explore the design space for adding a region-based ownership system inspired by [Reggio](https://doi.org/10.1145/3622846) and a concurrency
model inspired by [behaviour-oriented concurrency](https://doi.org/10.1145/3622852) to dynamic programming languages to unlock parallelism in a way that is free from data-races
(and also deadlocks, if the full behaviour-oriented concurrency model is adopted). FrankenScript is implemented using [Trieste](https://doi.org/10.1145/3687997.3695647).
To this end, FrankenScript programs generate a file called `mermaid.md` with a Mermaid diagram per line in the source program showing the object and region
graph of the program at that program point. 

This is a legend for the diagrams that FrankenScript generates:

![](./docs/frankenscript-legend.png)

## Pre-requisites

This project is C++20 based and uses CMake as the build system.  We also recommend installing Ninja to speed up the build process.

```base
sudo apt-get install cmake ninja-build clang-15
```

## Build

Once you have the pre-requisites installed, you can build the project by running the following commands:

```bash
mkdir build
cd build
cmake -G Ninja ..
ninja
```

and run the tests with:

```bash
ctest
```

## Run

The project can be run by

```bash
./build/frankenscript build foo.frank
```

where `foo.fs` is a FrankenScript program. This generates a file `mermaid.md` that contains the Mermaid representation of the heap after each step of the program.

You can run in interactive mode by running:

```bash
./build/frankenscript build --interactive foo.frank
```

Which will keep overwritting the `mermaid.md` file with the new heap state after each step.

## Lungfish

FrankenScript has been submitted as an PLDI25 artifact to explain the *Lungfish*
Ownership Model implemented here.

A critical part of Lungfish is the write-barrier shown in Figure 6 of the Paper. FrankenScript implements
these functions in `src/rt/objects/region.cc`. The important functions are:

* `add_reference(source, target)`: This adds a new reference from `source` to `target`.
* `add_to_region(region, target, source)`: This adds `target` and all reachable nodes to `region` if possible.
* `remove_reference(source, old_target)`: This removes a reference from `source` to `old_target`
* `move_reference(old_src, new_src, target)`: This is the `writeBarrier()` function, which adds a new reference
   from `new_src` to `target` and removes the reference from `old_src` to `target`.

The interpreter, implemented in `src/lang/interpreter.cc`, calls these functions via the public API of the
runtime (`rt::add_reference`, `rt::remove_reference`, `rt::move_reference`). The `add_to_region()` method is
never called directly by the interpreter.

A good example for the write-barrier is the `StoreField` bytecode implementation:

https://github.com/fxpl/frankenscript/blob/30e431c7ba8022f29fb4927913976bbf18356789/src/lang/interpreter.cc#L303-L318
