# Lesson 7: Interpreter and REPL

## Goal

Execute parsed command objects against the engine and talk to a human:

- one visitor function per command shape, calling only the `Engine` facade
- `seek` returns a message plus a table: projection, chained filters,
  numeric ordering, all applied in that order
- `change` rewrites exactly the matching rows: literal `set` or
  `plus`/`minus` arithmetic with int/decimal promotion
- `remove` deletes matches back-to-front so indexes stay valid
- `save`/`load`/`export` parse fine but wait for Lesson 8 with a clear error
- the `exdeus` REPL reads until `;`, prints results, and survives every error
- `help` and `quit` are REPL-only commands that never reach the lexer

This lesson completes the execution boundary. The parser knows grammar,
the interpreter knows meaning, the engine knows storage. None crosses.

## Files for this lesson

- `include/exdeus/repl/interpreter.hpp` — `InterpreterError`, `Result`,
  `ResultTable`, the `Interpreter` class.
- `src/repl/interpreter.cpp` — the `std::visit` dispatch plus seven runners.
- `src/repl/main.cpp` — the `exdeus` read-until-`;` loop with help/quit.
- `tests/interpreter_tests.cpp` — `run_interpreter_tests()`, the executable proof.
- `tests/test_main.cpp` — registers `run_interpreter_tests()`.
- `CMakeLists.txt` — adds `src/repl/interpreter.cpp` to the library and
  `tests/interpreter_tests.cpp` to the test target.

## Design target

`interpreter.hpp` declares:

```cpp
class InterpreterError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct ResultTable {
    std::vector<std::string> columns;
    std::vector<std::vector<exdeus::core::Value>> rows;
};

struct Result {
    std::string message;
    std::optional<ResultTable> table;
};

class Interpreter {
public:
    Result execute(const exdeus::language::Command& command);
    std::vector<Result> execute_all(const std::vector<exdeus::language::Command>& commands);
    exdeus::core::Engine& engine();
    const exdeus::core::Engine& engine() const;
};
```

Private runners mirror the seven executable commands: `run_create`,
`run_harness`, `run_forge`, `run_add`, `run_seek`, `run_change`,
`run_remove`. Save/load/export need no runner — the visitor throws
`InterpreterError` for them directly.

## Line-by-line connections

`execute` is a single `std::visit` over the ten-way `Command` variant.
Each arm dispatches to its runner by exact type (`if constexpr` on
`std::decay_t`), so adding a command means adding a struct, a parser
function, and one arm — the compiler enforces exhaustiveness. The
interpreter includes `engine.hpp` and `command.hpp` only. It never
includes `lexer.hpp`, `parser.hpp`, `catalog.hpp`, or `table.hpp`:
strings go in one side as commands, messages come out the other.

`Result` separates the one-line message from the optional table. Every
command returns a message (`created database bank`, `changed 1 row in
customers`); only `seek` fills the table. The REPL prints the message
then the table, so tests assert on the struct without scraping output.

Helpers stay anonymous-namespace free functions. `is_number` /
`as_double` give one promotion rule used by filters, arithmetic, and
ordering alike: integer operands decode to double for comparison, and
mixed int/decimal pairs compare by value. `values_equal` applies that
promotion first and falls back to exact `Value::operator==` for
text/boolean. `value_above` throws on non-numeric input — text has no
above/below meaning, and failing loudly beats guessing. `row_matches`
folds the `and_with_next` chain left to right; empty `where` matches all.

`run_forge` maps each `ColumnDef` to a `Column{name, type, required=true,
unique}`. Week 1 keeps every column required; the parser's optional
`required` keyword is accepted and ignored, so the grammar is ready when
week-2 nulls arrive. `run_add` hands `command.values` straight to
`Engine::insert` — parser-owned `Value`s, zero conversion.

`run_seek` applies projection, then filtering, then ordering. Projection
defaults to every schema column in schema order; unknown projection or
`order by` names throw before any row is touched. Filtering collects
matching indexes via `row_matches`. Ordering `stable_sort`s those indexes
on the key column: numeric keys compare by value, same-type text/boolean
keys compare by rendered string, mixed-type keys throw. Stability keeps
ties in insertion order.

`run_change` validates every `set` target, arithmetic source, and filter
column up front — a bad name changes nothing. It snapshots matches, then
rewrites each row from `engine().row_at` and commits via
`engine().update`, so `Table` re-validates types and uniqueness per row.
Arithmetic reads the row's own current value (`source_column`), requires
both sides numeric, and keeps integer results integer only when both
sides are integer; any decimal touch promotes to decimal. `to 500`
against a decimal balance therefore stores `25500.50`, not `25500`.

`run_remove` validates filter columns, snapshots matches, and deletes
from the back. Deleting index 2 before index 0 keeps earlier indexes
valid — deleting front-to-back would shift every later row.

`execute_all` loops `execute` in order and returns one `Result` per
command. The REPL and the script test share this path, so pasted scripts
behave exactly like typed commands.

`main.cpp` owns the three remaining concerns: reading, printing, and
surviving. It accumulates lines into `pending` until a `;` appears
outside a quoted string (`has_terminator` honours `\"` and `\\`, so
`add t (1, "x;y")` does not fire early), then runs one
Lexer → Parser → `execute_all` pass. `help`, `quit`/`exit` are checked on
the trimmed lowercase line before accumulation and never reach the
lexer. The `catch` ladder is ordered `LexerError`, `ParserError`,
`std::exception`: engine, table, catalog, and interpreter errors all land
in the last arm, print `Error: ...`, and the loop reprompts. EOF
(Ctrl+Z / Ctrl+D) breaks cleanly; blank lines at a fresh prompt reprompt
instead of entering continuation.

## How the test executable is connected

`tests/interpreter_tests.cpp` exposes `void run_interpreter_tests()`, and
`tests/test_main.cpp` calls it after `run_parser_tests()`. Every test
goes through the real `Lexer` + `Parser` first, so the suite proves the
full text-to-result path, not the interpreter in isolation. It proves,
in order:

```text
create/harness/forge/add flow; bare seek returns schema-ordered columns and rows
show projects one column; and/or chains filter correctly
ascending and descending order the balances
literal set rewrites one row; plus promotes int onto decimal; minus shrinks it
unmatched rows keep their values
integer arithmetic stays integer; filtered remove deletes one; bare remove clears
execute_all returns five results in order; trailing seek sees the earlier add
save/load/export throw InterpreterError naming Lesson 8
unknown tables, columns, short rows, duplicate uniques, and unharnessed forge all fail
```

The `run` helper parses one `;`-terminated string and executes it; the
`run_script` helper parses a pasted multi-command script through
`parse_all`. `check_not_ready` requires `InterpreterError` specifically
for the three parked commands; `check_fails` accepts any `std::exception`
for engine errors.

## Build and test

Because this repository uses the Visual Studio generator, use the
configuration explicitly:

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

The red phase for this lesson was `Cannot find source file:
src/repl/interpreter.cpp` at configure time — the test file and CMake
entries existed before the implementation did. The green phase compiles
the new translation unit with zero warnings, links all targets, and
passes `1/1 Test #1: exdeus_tests`. The direct runner
(`build\Debug\exdeus_tests.exe`) prints 21 value, 27 table, 28 engine, 48
lexer, 80 parser, and 40 interpreter PASS lines plus all six footers
(244 total).

The REPL is exercised by piping scripted sessions:

```powershell
'type commands here' | .\build\Debug\exdeus.exe
```

A bank session (create, harness, forge, two adds, seeks, change, remove)
prints messages plus aligned tables; an error session proves `help`,
multiline `forge`, quoted `;` inside strings, surviving engine/lexer/
parser errors, and the Lesson-8 parking messages — ending in `Bye.`
either way.

## Completion checkpoint

Do not continue to Lesson 8 until you can explain:

- why the interpreter calls `Engine` and never `Catalog` or `Table`
- why `execute` is a `std::visit` instead of a virtual method hierarchy
- why `seek` projects, then filters, then orders — in that order
- why integer-plus-integer stays integer but any decimal touch promotes
- why `remove` deletes from the back of the match list
- why `save`/`load`/`export` throw instead of silently succeeding
- why `help` and `quit` never reach the lexer
