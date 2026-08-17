# Exdeus Week 1 Build Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. The user writes the production C++ implementation; the guide and scaffolding must support that learning workflow.

**Goal:** Create a complete, testable week-1 Exdeus prototype and an accompanying C++ book that lets the user implement the production code independently.

**Architecture:** Build from the outside-in through a vertical slice: command-line entry point, lexer, parser, command model, in-memory engine, readable snapshot store, CSV export, and end-to-end examples. Keep language parsing separate from engine mutation so ExdeusQL can evolve independently in week 2.

**Tech Stack:** C++20, CMake, standard library only for week 1, CTest, and a small project-owned test runner with no external framework.

---

## Teaching contract

The agent may create directories, CMake files, headers containing interfaces and guided exercise markers, test harnesses, examples, and book explanations. The user writes the production C++ function bodies. Each lesson must contain:

1. The exact file to open.
2. The learning objective.
3. The preceding file or concept it connects to.
4. A small failing test or exercise.
5. Hints without silently supplying the full solution.
6. The exact build/test command.
7. A completion checklist.

The final project should never require the learner to understand every subsystem at once. Each milestone must compile before the next one begins.

## Planned repository layout

```text
CMakeLists.txt
README.md
include/exdeus/core/value.hpp
include/exdeus/core/schema.hpp
include/exdeus/core/table.hpp
include/exdeus/core/catalog.hpp
include/exdeus/core/engine.hpp
include/exdeus/language/token.hpp
include/exdeus/language/lexer.hpp
include/exdeus/language/command.hpp
include/exdeus/language/parser.hpp
include/exdeus/io/snapshot_store.hpp
include/exdeus/io/csv_exporter.hpp
src/core/value.cpp
src/core/schema.cpp
src/core/table.cpp
src/core/catalog.cpp
src/core/engine.cpp
src/language/lexer.cpp
src/language/parser.cpp
src/io/snapshot_store.cpp
src/io/csv_exporter.cpp
src/repl/main.cpp
tests/test_main.cpp
tests/value_tests.cpp
tests/lexer_tests.cpp
tests/parser_tests.cpp
tests/table_tests.cpp
tests/snapshot_tests.cpp
tests/end_to_end_tests.cpp
examples/bank.exql
examples/college.exql
docs/book/00-roadmap.md
docs/book/01-toolchain-and-project-shape.md
docs/book/02-values-and-types.md
docs/book/03-schemas-and-tables.md
docs/book/04-catalog-and-engine.md
docs/book/05-lexer.md
docs/book/06-parser-and-commands.md
docs/book/07-interpreter-and-repl.md
docs/book/08-snapshots-and-csv.md
docs/book/09-bank-walkthrough.md
docs/book/10-testing-and-week1-checkpoint.md
docs/book/11-week2-extension-map.md
```

## Task 1: Establish the build and learning environment

**Files:**
- Create: `CMakeLists.txt`
- Create: `README.md`
- Create: `tests/test_main.cpp`
- Create: `docs/book/00-roadmap.md`
- Create: `docs/book/01-toolchain-and-project-shape.md`

- [ ] Write the CMake project declaration for C++20, an `exdeus_core` library target, the `exdeus` executable target, and a `exdeus_tests` executable target.
- [ ] Configure CTest so `ctest --test-dir build --output-on-failure` runs the test executable.
- [ ] Make the initial test runner return success with a visible `no tests registered` message.
- [ ] Document how to configure, build, run the REPL, and run tests on Windows PowerShell.
- [ ] Explain translation units, headers, linking, namespaces, and the include/src split in the book.
- [ ] Verify the empty project builds before adding database behavior.

Expected command:

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Task 2: Implement values and types

**Files:**
- Create: `include/exdeus/core/value.hpp`
- Create: `src/core/value.cpp`
- Create: `tests/value_tests.cpp`
- Create: `docs/book/02-values-and-types.md`

- [ ] Define `ValueType` with `Integer`, `Decimal`, `Text`, `Boolean`.
- [ ] Define a `Value` type using `std::variant` and conversion/access helpers.
- [ ] Define explicit type-checking and readable formatting behavior.
- [ ] Write failing tests for each value type, equality, and type mismatch.
- [ ] Implement only enough behavior to pass those tests.
- [ ] Explain enums, `std::variant`, references, `const`, exceptions/results, and value semantics line by line.

## Task 3: Build schemas and tables

**Files:**
- Create: `include/exdeus/core/schema.hpp`
- Create: `src/core/schema.cpp`
- Create: `include/exdeus/core/table.hpp`
- Create: `src/core/table.cpp`
- Create: `tests/table_tests.cpp`
- Create: `docs/book/03-schemas-and-tables.md`

- [ ] Define column metadata: name, type, required flag, and unique flag.
- [ ] Define a schema that preserves column order and rejects duplicate names.
- [ ] Define a row as values aligned with a schema.
- [ ] Implement insert validation for count, type, required values, and uniqueness.
- [ ] Implement row selection, update, delete, add-column, and remove-column operations.
- [ ] Write tests before each operation and observe the expected failures.
- [ ] Teach ownership, `std::vector`, `std::unordered_map`, iterators, and mutation boundaries.

## Task 4: Add database catalog and engine boundary

**Files:**
- Create: `include/exdeus/core/catalog.hpp`
- Create: `src/core/catalog.cpp`
- Create: `include/exdeus/core/engine.hpp`
- Create: `src/core/engine.cpp`
- Create: `docs/book/04-catalog-and-engine.md`

- [ ] Define a catalog that owns named databases and tables.
- [ ] Implement create, list, rename, remove, and harness/select behavior.
- [ ] Define an engine façade that exposes safe operations without leaking catalog internals.
- [ ] Reject commands that require a selected database when none is active.
- [ ] Write tests for `create db bank;`, `harness bank;`, missing objects, and duplicate names.
- [ ] Explain class composition, interfaces, invariants, and why the parser must not mutate the catalog directly.

## Task 5: Define ExdeusQL tokens and lexer

**Files:**
- Create: `include/exdeus/language/token.hpp`
- Create: `include/exdeus/language/lexer.hpp`
- Create: `src/language/lexer.cpp`
- Create: `tests/lexer_tests.cpp`
- Create: `docs/book/05-lexer.md`

- [ ] Define token kinds for keywords, identifiers, literals, punctuation, operators, and end-of-input.
- [ ] Include source positions in every token.
- [ ] Support semicolon terminators, braces, parentheses, commas, colons, quoted text, numbers, and booleans.
- [ ] Use the canonical vocabulary: `create db`, `harness`, `forge table`, `add`, `seek`, `show`, `where`, `change`, `set`, `remove`, `save db`, `load db`, and `export`.
- [ ] Write lexer tests for valid input, quoted text, numeric values, and invalid characters.
- [ ] Teach character-by-character scanning, lookahead, string escaping, and error positions.

## Task 6: Parse commands without executing them

**Files:**
- Create: `include/exdeus/language/command.hpp`
- Create: `include/exdeus/language/parser.hpp`
- Create: `src/language/parser.cpp`
- Create: `tests/parser_tests.cpp`
- Create: `docs/book/06-parser-and-commands.md`

- [ ] Define command structures for database, table, row, query, update, delete, snapshot, and export operations.
- [ ] Parse `create db bank;` without selecting the database.
- [ ] Parse `harness bank;` as a separate selection command.
- [ ] Parse table definitions, inserts, projections, filters, updates, deletes, save/load, and export.
- [ ] Report unexpected tokens with line and column information.
- [ ] Keep command objects independent of engine classes.
- [ ] Teach recursive-descent parsing, grammar functions, ownership of parsed data, and error recovery boundaries.

## Task 7: Interpret commands and run the REPL

**Files:**
- Modify: `include/exdeus/core/engine.hpp`
- Modify: `src/core/engine.cpp`
- Create: `src/repl/main.cpp`
- Create: `docs/book/07-interpreter-and-repl.md`

- [ ] Add an interpreter that accepts parsed command objects and calls the engine façade.
- [ ] Implement human-readable result objects for success, rows, and errors.
- [ ] Read commands until semicolon termination while preserving multiline table/query blocks.
- [ ] Keep the REPL alive after lexer, parser, or engine errors.
- [ ] Add help and quit commands as REPL-only commands.
- [ ] Teach control flow, exception boundaries, streams, dependency direction, and the difference between parsing and execution.

## Task 8: Add readable snapshots and CSV export

**Files:**
- Create: `include/exdeus/io/snapshot_store.hpp`
- Create: `src/io/snapshot_store.cpp`
- Create: `include/exdeus/io/csv_exporter.hpp`
- Create: `src/io/csv_exporter.cpp`
- Create: `tests/snapshot_tests.cpp`
- Create: `docs/book/08-snapshots-and-csv.md`

- [ ] Define a versioned, readable `.exd` snapshot format.
- [ ] Serialize database names, table schemas, and escaped row values.
- [ ] Load snapshots into a fresh engine and reject malformed input with useful errors.
- [ ] Write through a temporary file before replacing the requested snapshot path.
- [ ] Export a selected table with a header row and CSV escaping.
- [ ] Test save/load round trips and text containing quotes, commas, and newlines.
- [ ] Explain streams, filesystem paths, serialization, escaping, temporary files, and why this is persistence but not crash recovery.

## Task 9: Complete practical examples

**Files:**
- Create: `examples/bank.exql`
- Create: `examples/college.exql`
- Create: `docs/book/09-bank-walkthrough.md`
- Create: `docs/book/10-testing-and-week1-checkpoint.md`
- Modify: `README.md`
- Create: `tests/end_to_end_tests.cpp`

- [ ] Build a bank example with customers, accounts, and transactions.
- [ ] Build a college example with students, courses, and enrollments.
- [ ] Execute the bank script through the same lexer, parser, interpreter, and engine used by the REPL.
- [ ] Save the bank database, load it into a fresh engine, and verify selected rows.
- [ ] Add the final acceptance test covering create, harness, forge, add, seek, change, remove, save, load, and export.
- [ ] Document exactly what the week-1 prototype can and cannot promise.

## Task 10: Document the week-2 upgrade path

**Files:**
- Create: `docs/book/11-week2-extension-map.md`

- [ ] Explain how the catalog can become file-backed.
- [ ] Explain page storage, write-ahead logging, checksums, checkpoints, backups, and recovery as separate lessons.
- [ ] Explain where indexes and transactions attach without rewriting ExdeusQL.
- [ ] Record known week-1 limitations and measurements to collect before adding complexity.

## Verification gates

After each task:

```powershell
cmake --build build
ctest --test-dir build --output-on-failure
```

Before declaring week 1 complete:

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
build\Debug\exdeus.exe
```

The user should be able to type the bank example manually, inspect the resulting `.exd` file, and explain the path from source text to changed table rows.
