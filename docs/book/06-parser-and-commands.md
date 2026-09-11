# Lesson 6: Parser and Commands

## Goal

Turn the Lesson-5 token stream into command objects the interpreter can execute:

- one recursive-descent function per command shape
- `create db` parses without selecting; `harness` parses as its own command
- table definitions, inserts, projections, chained filters, updates, deletes, save/load, export
- unexpected tokens fail with line, column, expected-vs-found, and a correction hint
- command objects depend only on `core::Value` — never on `Catalog`, `Table`, or `Engine`

This lesson completes the language front end. The parser knows grammar; the
Lesson-7 interpreter will know execution. Neither crosses the boundary.

## Files for this lesson

- `include/exdeus/language/command.hpp` — `ColumnDef`, `FilterOp`, `Condition`, `Assignment`, `Projection`, `SortDirection`, and the ten command structs in a `Command` variant.
- `include/exdeus/language/parser.hpp` — `ParserError`, the `Parser` class.
- `src/language/parser.cpp` — the recursive-descent implementation.
- `tests/parser_tests.cpp` — `run_parser_tests()`, the executable proof.
- `tests/test_main.cpp` — registers `run_parser_tests()`.

## Design target

`command.hpp` declares:

```cpp
struct ColumnDef {
    std::string name;
    exdeus::core::ValueType type;
    bool unique = false;
};

enum class FilterOp { Equals, Above, Below };

struct Condition {
    std::string column;
    FilterOp op;
    exdeus::core::Value literal;
    bool and_with_next = true;  // true = `and`, false = `or`
};

struct Assignment {
    std::string column;
    bool is_arithmetic = false;
    std::string source_column;
    bool is_plus = true;
    exdeus::core::Value value;
};

using Projection = std::vector<std::string>;

enum class SortDirection { None, Ascending, Descending };

struct CreateDb { std::string name; };
struct HarnessDb { std::string name; };
struct ForgeTable { std::string table; std::vector<ColumnDef> columns; };
struct AddRow { std::string table; std::vector<exdeus::core::Value> values; };
struct SeekRows {
    std::string table; Projection show;
    std::vector<Condition> where;
    std::string order_by; SortDirection direction = SortDirection::None;
};
struct ChangeRows {
    std::string table;
    std::vector<Assignment> set; std::vector<Condition> where;
};
struct RemoveRows { std::string table; std::vector<Condition> where; };
struct SaveDb { std::string path; };
struct LoadDb { std::string path; };
struct ExportTable { std::string table; std::string path; };

using Command = std::variant<CreateDb, HarnessDb, ForgeTable, AddRow, SeekRows,
                             ChangeRows, RemoveRows, SaveDb, LoadDb, ExportTable>;
```

`parser.hpp` declares:

```cpp
class ParserError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    Command parse_one();                 // exactly one `;`-terminated command
    std::vector<Command> parse_all();    // `;`-separated sequence
};
```

## Line-by-line connections

`Command` is a `std::variant` of ten plain structs. The interpreter will
`std::visit` it in Lesson 7; tests use `std::holds_alternative` /
`std::get`. No inheritance, no virtual dispatch — adding a command means
adding a struct plus one `parse_*` function.

`FilterOp` covers the three week-1 comparisons in both spellings:
`equals`/`==`/`=`, `above`/`>`, `below`/`<`. `Condition::and_with_next`
lives on the condition *before* the connector, defaulting to `and`, so a
single condition needs no connector at all.

`Assignment` covers both `set` forms: a literal (`set name to "Ravi"`) and
row arithmetic (`set balance to balance plus 500`). The arithmetic form
names its source column explicitly so the interpreter can read the row's own
value first — the grammar never assumes target and source are the same.

`Projection` is just column names; empty means all columns. `SeekRows`
bundles projection, chained `where`, and optional `order by` with direction
defaulting to `None` (no sorting) when no keyword follows the column.

Parsing is recursive descent: `parse_command` dispatches on the first token
to one function per shape (`parse_create`, `parse_forge`, `parse_seek`, ...),
each consuming exactly its grammar and returning its struct. `parse_all`
loops `parse_command` + `expect(Semicolon)` until `EndOfInput`;
`parse_one` does it once and rejects trailing tokens.

The cursor trio — `peek()`, `advance()`, `check()`/`match()` — mirrors the
lexer's style: `expect(kind, what)` consumes or throws. The constructor
appends a synthetic `EndOfInput` when the stream lacks one, so every error
path has a token to point at.

Errors report `line L, column C: expected <what>, found '<lexeme>'
(<kind>)` using the offending token's own position. Truncated input reports
`found end of input` at the end token's position. The `default` branch of
`parse_command` lists all ten verbs as the correction hint.

Two intentional asymmetries. `create` accepts `db` or `database` (same as
the lexer alias); `harness` takes a bare name because selection is its whole
job. `remove <table> [where ...]` deletes rows in week 1 — table drops stay
a REPL-level operation until the interpreter chapter decides their syntax.

The parser includes only `command.hpp` and `token.hpp`. It never includes
`engine.hpp`, `catalog.hpp`, or `table.hpp` — `core::Value` literals flow
through by value, which is the only core contact allowed.

## How the test executable is connected

`tests/parser_tests.cpp` exposes `void run_parser_tests()`, and
`tests/test_main.cpp` calls it after `run_lexer_tests()`. It lexes real
source text through `Lexer` first, so every test proves the lexer-to-parser
connection, not just the parser in isolation. It proves, in order:

```text
create db keeps the name; database aliases db; harness parses separately
forge keeps table name, all columns, types, and unique flags
add keeps table name and all four typed values in order
full seek: projection, two chained conditions, and/or links, order + direction
bare seek has no projection/filter/direction; or + symbol ops map
arithmetic change: source column, plus flag, operand, filter
multi-set literal change; change without where hits all rows
filtered and bare remove; save/load/export keep their paths
parse_all returns three commands in order with the right types
ten failure cases throw ParserError; errors carry line and column
```

The `as<T>` helper takes its `Command` by value and returns the struct by
value — an earlier version returned a reference into a temporary and read
garbage. `check_throws` prints the caught `ParserError::what()`, so the
output itself shows the position format working.

## Build and test

Because this repository uses the Visual Studio generator, use the
configuration explicitly:

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

The red phase for this lesson was an unresolved external for every `Parser`
member plus a missing `run_parser_tests` symbol. Two further reds were real
bugs the tests caught: the `as<T>` helper returned a dangling reference
into a temporary `Command`, and one failure-case input (`save db bank.exd;`)
tripped the lexer (`.` is not a token) before reaching the parser — it now
uses `save db bank;` so the parser owns the failure. The green phase passes
`1/1 Test #1: exdeus_tests`. The direct runner
(`build\Debug\exdeus_tests.exe`) prints 21 value, 27 table, 28 engine, 48
lexer, and 80 parser PASS lines plus all five footers.

## Completion checkpoint

Do not continue to Lesson 7 until you can explain:

- why commands are a variant of structs instead of a class hierarchy
- why the parser must never include engine or catalog headers
- why `create` and `harness` are two commands instead of one
- why `and_with_next` defaults to `and` and lives on the earlier condition
- why arithmetic `set` names its source column explicitly
- why `parse_one` rejects trailing tokens after the first `;`
