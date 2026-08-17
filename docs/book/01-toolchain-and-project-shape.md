# Lesson 1: The C++ Project Shape

## Goal

Make the empty Exdeus project configure, compile, and run. At this point we are not pretending to have a database. We are proving that the toolchain can compile several C++ translation units and link them into programs.

## Files in this lesson

- `CMakeLists.txt` describes how the project is built.
- `src/repl/main.cpp` contains the executable entry point.
- `tests/test_main.cpp` contains the first test executable entry point.
- `include/` will hold declarations that other files can include.
- `src/` will hold implementations.
- `tests/` will hold executable checks of behavior.

## Read the build file slowly

`cmake_minimum_required` prevents the project from silently using an ancient CMake behavior.

`project(Exdeus ... LANGUAGES CXX)` tells CMake that this is a C++ project.

`CMAKE_CXX_STANDARD 20` selects the language version. `CMAKE_CXX_EXTENSIONS OFF` asks for standard C++ instead of compiler-specific language extensions.

`add_library(exdeus_core ...)` creates a reusable target for the engine. The files are listed before they exist because the project plan reserves their responsibilities. Your first task is to create those files as you progress through the book; until then, configure may report missing source files.

`target_include_directories(... PUBLIC include)` tells consumers of the engine where public headers will live.

`add_executable(exdeus ...)` creates the command-line program.

`target_link_libraries` connects the executable to the engine library. This is the first important project connection: the REPL depends on the engine, while the engine must not depend on the REPL.

`add_executable(exdeus_tests ...)` creates a separate program for automated checks, and `add_test` registers it with CTest.

## Your first exercise

The current scaffold intentionally references future source files. Create the following empty implementation files so CMake can compile the project:

```text
src/core/value.cpp
src/core/schema.cpp
src/core/table.cpp
src/core/catalog.cpp
src/core/engine.cpp
src/language/lexer.cpp
src/language/parser.cpp
src/io/snapshot_store.cpp
src/io/csv_exporter.cpp
tests/value_tests.cpp
tests/lexer_tests.cpp
tests/parser_tests.cpp
tests/table_tests.cpp
tests/snapshot_tests.cpp
tests/end_to_end_tests.cpp
```

For now, each `.cpp` file may contain only a comment. This is scaffolding, not the implementation. You will replace the comments with real code as each lesson arrives.

Run:

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## What you should understand before continuing

- A header declares an interface; a source file defines behavior.
- CMake creates targets and connects files.
- The engine library is separate from the executable.
- Tests are another executable, not magic code inside the REPL.
- A successful build only proves structure, not correctness.
