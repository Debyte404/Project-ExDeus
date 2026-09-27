# Lesson 8: Snapshots and CSV

## Goal

Persist one database as readable text and export one table as CSV:

- a versioned `.exd` snapshot: header, database name, table declarations,
  column metadata, escaped rows, end marker
- save writes through a temp file plus atomic rename, so readers never see
  a half-written snapshot
- load parses fully before touching the engine, then rebuilds through the
  public `Engine` facade; malformed input fails with a line number and
  changes nothing
- loading over an existing database of the same name restores the snapshot
- CSV export writes a header row plus RFC-style quoting for commas, quotes,
  and newlines
- the interpreter's `save`/`load`/`export` run for real; the REPL help no
  longer calls them future work

This is persistence, not crash recovery. A torn write inside one `save`
call is handled; a power cut mid-rename, concurrent writers, and partial
replication are week-2 problems.

## Files for this lesson

- `include/exdeus/io/snapshot_store.hpp` — `SnapshotError`, `SnapshotStore`.
- `src/io/snapshot_store.cpp` — the format writer, parser, and atomic save.
- `include/exdeus/io/csv_exporter.hpp` — `CsvError`, `CsvExporter`.
- `src/io/csv_exporter.cpp` — the header-plus-quoting writer.
- `tests/snapshot_tests.cpp` — `run_snapshot_tests()`, the executable proof.
- `tests/test_main.cpp` — registers `run_snapshot_tests()`.
- `include/exdeus/repl/interpreter.hpp` — three new private runners.
- `src/repl/interpreter.cpp` — `run_save`, `run_load`, `run_export`.
- `src/repl/main.cpp` — help text drops the Lesson-8 parking note.
- `tests/interpreter_tests.cpp` — section 6 now round-trips for real.

## Design target

```cpp
class SnapshotError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class SnapshotStore {
public:
    static void save(const exdeus::core::Engine& engine, const std::string& database,
                     const std::filesystem::path& path);
    static std::string load(exdeus::core::Engine& engine, const std::filesystem::path& path);
};

class CsvError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class CsvExporter {
public:
    static void export_table(const exdeus::core::Engine& engine, const std::string& table,
                             const std::filesystem::path& path);
};
```

Both classes are stateless static-method services. `save` reads through
`list_databases` / `list_tables` / `table_schema` / `row_at`; `load`
writes through `create_database` / `harness_database` / `create_table` /
`insert`. Neither includes `catalog.hpp` or `table.hpp`, so the engine
boundary from Lesson 4 holds for persistence too.

## Line-by-line connections

The snapshot format is one record per line, fields split on whitespace
with double-quote sections honoured:

```text
EXDEUS_SNAPSHOT 1
database bank
table customers 3
column id integer required unique
column name text required
column balance decimal required
row 1 "Asha" 25000.5
row 2 "Ravi, Jr." 12000
end database bank
```

The version header lets week 2 reject files it cannot read instead of
misreading them. `table <name> <column-count>` states the count up front
so the parser knows exactly how many `column` lines to consume. Every
`column` line carries `required`/`unique` flags explicitly — even
`required`, which week 1 always sets — so week-2 nulls parse old files
unchanged. Rows are positional values aligned to the schema, like `Table`
itself.

Text escaping keeps the format line-based. `quote_text` encodes `"`, `\`,
newline, tab, and carriage return as `\"`, `\\`, `\n`, `\t`, `\r`, so a
value holding a real newline still occupies exactly one physical line and
the loader can blame failures by line number. The parser's `Field`
struct records whether each field arrived quoted: text must be quoted,
numbers and booleans must be bare. That strictness turns `row 1 Asha
25000.5` (missing quotes) into a line-numbered error instead of silent
acceptance — the writer always quotes text, so bare text means a corrupt
or hand-edited file.

Decimals use shortest round-trip formatting: `std::to_chars` prints
`25000.5`, never `25000.500000`, and `stod` reads back the exact double.
`Value::to_string` is display formatting (fixed six decimals); snapshot
and CSV need exactness, so both writers carry their own `format_decimal`
with a `max_digits10` fallback.

Save is atomic by construction: serialize the whole database into a
string, write it to `<name>.tmp`, flush, check the stream, then
`std::filesystem::rename` over the target. Readers of `<name>` see the
complete old file or the complete new file, never a prefix. A failed save
(an unknown database, an unwritable path) throws before any rename, so
the old snapshot stays intact. A crash between write and rename leaves a
lingering `.tmp` — harmless, and visibly not the snapshot.

Load is two-phase: `parse_snapshot` builds a `ParsedDb` (name, tables,
schemas, rows) with every content error throwing `SnapshotError("snapshot
line N: ...")`, and only then does the engine phase apply it. Phase one
validates the version, the `database` line, each `table` header with a
positive column count, each `column` line with a known type and known
flags, each row's arity and per-column literal shape, in-file uniqueness,
and the closing `end database <name>` with nothing after it. Phase two
drops an existing same-named database (replace semantics — restoring a
save into its own session is the main flow), creates, harnesses, rebuilds
tables, and re-inserts rows through the facade, so `Table` re-validates
everything a second time. A `catch (...)` around phase two drops the
half-built database as a safety net; with parsing done first it cannot
trigger on content.

`split_fields` is the one shared primitive: blank lines yield zero fields
and are skipped, unterminated quotes and `\q`-style escapes throw with
the line number. `parse_integer`/`parse_decimal` use `stoll`/`stod` with
full-consumption checks (`used != size` fails), so `12x` never parses as
`12`.

CSV is deliberately simpler: a header row of column names, one line per
record, `csv_field` quoting only fields containing `,`, `"`, or newline
— with inner quotes doubled. `Asha` stays bare; `O"Neil, Jr.\nJr` becomes
`"O""Neil, Jr.\nJr"`. Newlines inside a quoted CSV field are real
newlines (RFC-style), which is why the CSV writer does not use the
snapshot's `\n` escaping — different format, different rule, both tested.
Missing selection and unknown tables surface as `CsvError`; the
interpreter translates those into `InterpreterError` so the REPL keeps
its single catch arm.

In the interpreter, `run_save` uses the harnessed database as the name —
`save db` without a harness fails instead of guessing. `run_load` returns
the name the store reports. Both plus `run_export` catch `std::exception`
and rethrow as `InterpreterError`, keeping the Lesson-7 error contract:
one catch type for the REPL, sessions survive every failure.

## How the test executable is connected

`tests/snapshot_tests.cpp` exposes `void run_snapshot_tests()`, and
`tests/test_main.cpp` calls it after `run_interpreter_tests()`. The
`seeded_bank` fixture holds commas, quotes, and newlines inside one name
(`O"Neil, Jr.\nJr`) plus a `false` boolean and a unique flag, so every
round trip proves the escaping it needs. It proves, in order:

```text
save writes the version header, the database name, and escaped quotes
load returns the name, selects the database, keeps both rows and the flags
reload over the same name restores instead of throwing duplicate
no .tmp lingers; failed save of an unknown db keeps the old file intact
bad version, missing header, truncated, bad escape, duplicate table, missing file all throw SnapshotError
no failed load leaks a database
csv starts with the header, leaves plain rows bare, quotes comma/quote/newline with doubled quotes
csv of an unknown table fails
```

All file paths go through the temp directory with a per-file counter, so
reruns never collide and the repo tree stays clean.

`tests/interpreter_tests.cpp` section 6 changed character in this lesson:
the three `check_not_ready` parking checks became a live round trip
(save → remove → load restores the row; export writes the file), plus
export-of-unknown and load-of-missing failures and save-without-harness.
Temp paths use forward slashes so they parse cleanly inside ExdeusQL
string literals.

## Build and test

Because this repository uses the Visual Studio generator, use the
configuration explicitly:

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

The red phase for this lesson was three linker unresolved-externals
(`SnapshotStore::save`, `SnapshotStore::load`,
`CsvExporter::export_table`) — headers and tests existed before bodies.
One real bug surfaced on the way to green: a test bound
`engine.table_schema(...).columns()` to a `const auto&`, dangling into
the by-value temporary (the same trap Lesson 6's `as<T>` helper fixed).
The green phase compiles both translation units with zero warnings and
passes `1/1 Test #1: exdeus_tests`. The direct runner
(`build\Debug\exdeus_tests.exe`) prints 21 value, 27 table, 28 engine, 48
lexer, 80 parser, 47 interpreter, and 34 snapshot PASS lines plus all
seven footers (285 total).

The REPL proves the wiring with a scripted session: create, harness,
forge, two adds (one name holding a comma), save, remove, seek shows one
row, load, seek shows both rows back, export, quit. The `.exd` file reads
as the format above; the `.csv` quotes only the comma-holding field.

## Completion checkpoint

Do not continue to Lesson 9 until you can explain:

- why save writes to `<name>.tmp` and renames instead of writing direct
- why load parses everything before creating anything
- why text must be quoted and numbers must be bare in snapshot rows
- why loading over a same-named database replaces instead of throwing
- why decimals use `to_chars` instead of `Value::to_string`
- why CSV newlines stay real while snapshot newlines become `\n`
- why this is persistence but not crash recovery
