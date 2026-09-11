# Lesson 3: Schemas and Tables

## Goal

Store ordered rows whose values respect a declared schema:

- a column has a name, a declared `ValueType`, a required flag, and a unique flag
- a schema preserves column order and rejects duplicate names
- a row is values aligned with the schema by position
- inserts validate count, declared type, and uniqueness before mutating anything
- rows can be selected, updated, deleted, and reshaped by add/remove column

This lesson introduces a central engine idea: `Value` knows one typed cell,
`Schema` knows the shape of a table, and `Table` owns shaped rows plus the
invariants between them. Later layers — catalog, engine, snapshots, parser —
will all depend on this one.

## Files for this lesson

- `include/exdeus/core/schema.hpp` — `Column` metadata and the `Schema` class.
- `src/core/schema.cpp` — schema ordering, lookup, and duplicate-name checks.
- `include/exdeus/core/table.hpp` — `TableError`, the `Row` alias, the `Table` class.
- `src/core/table.cpp` — row validation, insert/update/delete/select, column reshape.
- `tests/table_tests.cpp` — `run_table_tests()`, the executable proof of the above.
- `tests/test_main.cpp` — registers `run_table_tests()` next to `run_value_tests()`.

## Design target

`schema.hpp` declares:

```cpp
struct Column {
    std::string name;
    ValueType type;
    bool required = true;
    bool unique = false;
};

class Schema {
public:
    Schema() = default;
    explicit Schema(std::vector<Column> columns);

    const std::vector<Column>& columns() const;
    size_t column_count() const;
    bool has_column(const std::string& name) const;
    size_t index_of(const std::string& name) const;
    const Column& column_at(size_t index) const;
    const Column& column(const std::string& name) const;
    void add_column(Column column);
    void remove_column(const std::string& name);

private:
    std::vector<Column> columns_;
};
```

`table.hpp` declares:

```cpp
class TableError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

using Row = std::vector<Value>;

class Table {
public:
    Table() = default;
    Table(std::string name, Schema schema);

    const std::string& name() const;
    const Schema& schema() const;
    size_t row_count() const;
    const Row& row_at(size_t index) const;
    const std::vector<Row>& rows() const;

    size_t insert(Row values);
    void update(size_t index, Row values);
    void remove(size_t index);
    std::vector<size_t> select(const std::string& column_name, const Value& value) const;
    void add_column(Column column, Value default_value);
    void remove_column(const std::string& name);

private:
    void validate_row(const Row& values, bool is_update, size_t ignore_index) const;

    std::string name_;
    Schema schema_;
    std::vector<Row> rows_;
};
```

## Line-by-line connections

`struct Column` is plain metadata, not behavior. `name` plus `type` is what
week 1 enforces; `required` and `unique` are stored now so week-2 null support
can attach without changing the shape. Week 1 has no null values, so every row
must still supply one value per column.

`Schema` owns `std::vector<Column> columns_` in declaration order. Order is the
contract: position `i` of every row belongs to `columns_[i]`. Both constructors
reject duplicate names with `TableError`, and `add_column` repeats that check,
so the invariant holds for the whole lifetime, not just at construction.

`index_of` is the single name-to-position function. `column()`, `select()`,
and `remove_column()` all go through it, so unknown names fail in exactly one
place with `unknown column: <name>`.

`TableError` derives from `std::runtime_error` so tests can catch one named
type for every validation failure: bad count, bad type, duplicate unique,
unknown column, bad row index.

`using Row = std::vector<Value>` says a row carries no names, only positions.
Names live in the schema. This keeps each row small and makes misalignment a
loud size mismatch instead of silent corruption.

`validate_row` runs before any mutation and checks in this order: value count
against `column_count()`, each value's `type()` against the declared column
type, then every `unique` column against all stored rows. Updates pass their
own index as `ignore_index`, so keeping your own key passes while copying
someone else's key fails. Because validation throws before `push_back` or
assignment, a failed command never partially changes the table.

`insert` returns the new row index, `update`/`remove` bounds-check first,
`select` compares with `Value::operator==` and returns matching indexes rather
than copies, so update/delete act on stable positions. `add_column` first
checks the default value's type against the new column, then extends the schema
and every stored row together; `remove_column` shrinks both together. Schema
and rows can never disagree about width.

## How the test executable is connected

`tests/table_tests.cpp` exposes `void run_table_tests()`, and
`tests/test_main.cpp` calls it after `run_value_tests()`. A tiny local `check`
prints `PASS` and exits nonzero on `FAIL`; `check_throws` expects a
`TableError`. The bank-shaped fixture (`id` integer unique, `name` text,
`balance` decimal, `active` boolean) proves, in order:

```text
schema keeps column order and index lookup
duplicate names rejected at construct and at add_column
insert returns index, stores values, select finds the row
short rows and wrong-typed values rejected, table unchanged
duplicate unique ids rejected, table unchanged
update replaces values; bad index and bad row throw, old row kept
delete compacts remaining rows; bad index throws
add_column extends schema and rows with the default value
remove_column shrinks schema and rows together
```

Every check uses only the public API: factories, `type()`, `operator==`,
schema lookups, and table operations. Nothing reaches into `type_`, `data_`,
`columns_`, or `rows_` directly.

## Build and test

Because this repository uses the Visual Studio generator, use the
configuration explicitly:

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

The red phase for this lesson was 16 unresolved externals (`LNK2019`) — every
`Schema`/`Table` member the new tests call. The green phase compiles
`schema.cpp` and `table.cpp`, links all three targets, and passes
`1/1 Test #1: exdeus_tests`. The direct runner (`build\Debug\exdeus_tests.exe`)
prints 21 value PASS lines plus 27 table PASS lines and both
`All ... tests passed.` footers.

## Completion checkpoint

Do not continue to Lesson 4 until you can explain:

- why a row stores positions while the schema stores names
- why validation must run fully before any `push_back` or assignment
- why the unique check skips the row being updated
- why `select` returns indexes instead of row copies
- why `add_column` and `remove_column` must change the schema and every row together
- why `Table` depends on `Schema` and `Value`, but neither may depend on `Table`
