# Lesson 4: Catalog and Engine

## Goal

Own named databases, each owning named tables, behind one safe facade:

- a catalog creates, lists, renames, and drops databases
- `create db` never selects; `harness <db>` selects explicitly
- table operations always run against the harnessed database
- an `Engine` facade exposes every operation without leaking catalog internals
- missing selection, unknown names, and duplicates fail loudly and change nothing

This lesson introduces the week-2-stable boundary: the future parser and
interpreter call `Engine`, never `Catalog` or `Table` directly.

## Files for this lesson

- `include/exdeus/core/catalog.hpp` — `CatalogError`, `Database`, `Catalog`.
- `src/core/catalog.cpp` — database/table ownership, selection, renames.
- `include/exdeus/core/engine.hpp` — `EngineError`, the `Engine` facade.
- `src/core/engine.cpp` — selection guard plus error translation.
- `tests/engine_tests.cpp` — `run_engine_tests()`, the executable proof.
- `tests/test_main.cpp` — registers `run_engine_tests()`.
- `CMakeLists.txt` — adds `tests/engine_tests.cpp` to the test target.

## Design target

`catalog.hpp` declares:

```cpp
class CatalogError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct Database {
    std::string name;
    std::map<std::string, Table> tables;
};

class Catalog {
public:
    void create_database(const std::string& name);
    bool has_database(const std::string& name) const;
    void drop_database(const std::string& name);
    void rename_database(const std::string& old_name, const std::string& new_name);
    std::vector<std::string> list_databases() const;

    void harness_database(const std::string& name);
    bool has_selection() const;
    const std::string& selected_database() const;
    void clear_selection();

    void create_table(const std::string& db, const std::string& table, Schema schema);
    bool has_table(const std::string& db, const std::string& table) const;
    void drop_table(const std::string& db, const std::string& table);
    void rename_table(const std::string& db, const std::string& old_name, const std::string& new_name);
    std::vector<std::string> list_tables(const std::string& db) const;
    const Table& table(const std::string& db, const std::string& table) const;
    Table& mutable_table(const std::string& db, const std::string& table);
};
```

`engine.hpp` declares:

```cpp
class EngineError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class Engine {
public:
    void create_database(const std::string& name);
    void harness_database(const std::string& name);
    void drop_database(const std::string& name);
    void rename_database(const std::string& old_name, const std::string& new_name);
    std::vector<std::string> list_databases() const;
    std::string selected_database() const;
    bool has_selection() const;

    void create_table(const std::string& table, Schema schema);
    void drop_table(const std::string& table);
    void rename_table(const std::string& old_name, const std::string& new_name);
    std::vector<std::string> list_tables() const;
    const Table& table(const std::string& table) const;
    Schema table_schema(const std::string& table) const;

    size_t insert(const std::string& table, Row values);
    void update(const std::string& table, size_t index, Row values);
    void remove(const std::string& table, size_t index);
    std::vector<size_t> select(const std::string& table, const std::string& column, const Value& value) const;
    Row row_at(const std::string& table, size_t index) const;
    size_t row_count(const std::string& table) const;
    void add_column(const std::string& table, Column column, Value default_value);
    void remove_column(const std::string& table, const std::string& column);
};
```

## Line-by-line connections

`Database` is a plain struct: a name plus `std::map<std::string, Table>`.
`std::map` (not `unordered_map`) keeps `list_tables()` alphabetical, so
listings are deterministic in tests and in the future REPL.

`Catalog` owns `std::map<std::string, Database> databases_` plus the
selection pair (`selected_`, `has_selection_`). Selection is a name, not a
pointer, so dropping a database can clear it without dangling anything.

`find_db` is the single unknown-database check — const and non-const
overloads — throwing `unknown database: <name>`. Every table operation goes
through it, so unknown databases fail in exactly one place.

`create_database` never touches the selection. This is the
`create db bank;` vs `harness bank;` contract from the design spec: creating
is not selecting. Only `harness_database` sets the selection, and it looks
the name up first so a failed harness leaves the old selection intact.

`drop_database` clears the selection when the selected database dies;
`rename_database` follows it to the new name. Renaming to the same name is
a no-op; renaming onto an existing name throws `duplicate database:`.

`table()` returns a const reference for reads; `mutable_table()` returns a
mutable reference for insert/update/remove. The parser and interpreter will
never see either — they call `Engine`, which is why this split exists.

`rename_table` rebuilds under the new name: `Table` has no rename of its own
`name_` field, so rows are re-inserted into a fresh `Table(new_name,
moved.schema())`. Schema and rows stay aligned because `insert`
re-validates.

`Engine::require_selection()` is the single missing-selection guard,
throwing `no database selected: harness a database first`. Every
table/row operation calls it first, so no table command can run without a
harness. `Engine` translates `CatalogError` into `EngineError` at the
boundary, keeping one catch type for the future interpreter.

## How the test executable is connected

`tests/engine_tests.cpp` exposes `void run_engine_tests()`, and
`tests/test_main.cpp` calls it after `run_table_tests()`. The bank-shaped
fixture (`id` integer unique, `name` text, `balance` decimal) proves, in order:

```text
create db leaves nothing selected; table op without harness throws
harness of unknown db throws; harness selects and reports the name
duplicate/unknown/rename-onto-existing db names all throw
list_databases reports both names in order
rename follows the selection; drop clears it
full lifecycle: create, duplicate reject, insert, select, update, rename keeps rows, remove, drop, second drop throws
add_column grows schema and rows; remove_column shrinks both
```

Every check uses only the public `Engine` API. Nothing reaches into
`Catalog`, `databases_`, or `rows_` directly.

## Build and test

Because this repository uses the Visual Studio generator, use the
configuration explicitly:

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

The red phase for this lesson was missing `catalog.hpp`/`engine.hpp`
plus unresolved externals for every `Catalog`/`Engine` member the new tests
call. The green phase compiles both translation units, links all targets,
and passes `1/1 Test #1: exdeus_tests`. The direct runner
(`build\Debug\exdeus_tests.exe`) prints 21 value PASS lines, 27 table PASS
lines, 28 engine PASS lines, and all three `All ... tests passed.` footers.

## Completion checkpoint

Do not continue to Lesson 5 until you can explain:

- why `create db` must not select the database
- why selection is a name plus a flag instead of a pointer
- why `find_db` has const and non-const overloads
- why `rename_table` rebuilds instead of assigning
- why the future parser must call `Engine` and never `Catalog`
- why `Engine` translates `CatalogError` into `EngineError`
