# Verona Dynamic Language experiment


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
./build/verona_dyn build foo.vpy
```

where `foo.vpy` is a Verona dynamic language program. This generates a file `mermaid.md` that contains the Mermaid representation of the heap after each step of the program.

You can run in interactive mode by running:

```bash
./build/verona_dyn build --interactive foo.vpy
```

Which will keep overwritting the `mermaid.md` file with the new heap state after each step.