# Lesson 12: Keys and Constraints

## Goal

Give tables real integrity: one demonic primary key, explicit nullability,
value checks, and cross-table foreign keys.

- `beelzebub` marks the single primary key of a table (implies unique + not null)
- `not null` / `required` demand a value; bare `null` declares a nullable column
- `check above 0` (and `equals`, `below`, chained with `and`) rejects bad values
- `references parent(col)` ties a child column to a live parent key (restrict, no cascade)
- snapshots persist all of it (format v2, v1 files still load)

This lesson introduces the first cross-table enforcement: the engine, not
just the table, validates writes.

## Files for this lesson

- `include/exdeus/core/value.hpp`, `src/core/value.cpp` — `ValueType::Null`, `Value::null()`, `to_string` prints `null`.
- `include/exdeus/core/schema.hpp`, `src/core/schema.cpp` — `CheckOp`, `CheckConstraint`, `ForeignRef`; `Column` gains `is_primary`, `checks`, `references`; `Schema` rejects a second `beelzebub` and normalizes primary to unique + required.
- `src/core/table.cpp` — `validate_row` enforces NOT NULL, CHECK (all rules must pass, nulls skip), and unique-with-nulls-exempt (two nulls never conflict); `add_column` validates its default the same way.
- `include/exdeus/language/token.hpp`, `src/language/lexer.cpp` — `beelzebub`, `not`, `check`, `references` keywords; `null` lexes as `NullLit` carrying `Value::null()`.
- `include/exdeus/language/command.hpp`, `src/language/parser.cpp` — `ColumnDef` gains `required_flag`, `is_primary`, `checks`, `references`; `parse_column_def` accepts the flags in any order, each at most once; `null` after `not` means NOT NULL, bare `null` means nullable; `check <op> <lit> (and <op> <lit>)*`; `references parent(col)`; `expect_literal` accepts `null`.
- `src/core/engine.cpp` — outgoing FK check on insert/update (non-null FK must exist in the parent; dangling parent/column is an error, not a silent pass); incoming restrict on parent update/remove (no orphaning); `drop_table`/`rename_table` blocked while referenced; `add_column` default must satisfy its FK.
- `src/io/snapshot_store.cpp`, `src/io/csv_exporter.cpp` — format v2: `beelzebub`, `required`/`nullable`, `unique`, repeated `check <op> <value>`, `references <table> <column>`; bare `null` for null values; v1 files still load; CSV renders null as empty.
- `src/repl/main.cpp` — help text documents the new forge flags.
- New tests: 4 lexer, 21 parser, 30 interpreter, 10 snapshot checks (65 new, 418 total).

## Syntax

```text
forge table customers with
  id: integer beelzebub,
  name: text not null,
  nickname: text null;

forge table accounts with
  id: integer beelzebub,
  customer_id: integer not null references customers(id),
  balance: decimal not null check above 0 and below 1000000;

add accounts (101, 1, 500.0);   -- ok: parent 1 exists, balance in range
add accounts (102, 99, 10.0);   -- rejected: no customer 99
add accounts (103, 1, 0.0);     -- rejected: check above 0
add members (2, null, null);    -- ok only when those columns are null-marked
remove customers where id equals 1;  -- rejected while accounts point at 1
seek members where nickname equals null;  -- null equals null matches
```

## Line-by-line connections

`Value::null()` is `variant<monostate, ...>` — every existing `std::get<T>`
is type-based, so no index shifts break. `to_string` returns `"null"`;
`operator==` needs no change (same type + same payload still holds).

`Schema` is the single-primary gate: constructor and `add_column` both
count `is_primary` and throw `multiple beelzebub columns` past one. Primary
normalization lives here (not in the parser) so forge, snapshot load, and
`add_column` agree even if a caller forgets the implications.

`Table::validate_row` treats null first: required + null throws before any
type comparison; nullable nulls skip type, checks, and uniqueness. Non-null
values run type, then every CHECK (numeric promotion for equals, numeric-only
for above/below, same helpers as the interpreter's `where`), then uniqueness
with nulls on both sides exempt. Failed writes change nothing — the
validate-before-mutate rule from Lesson 3 is unchanged, only longer.

Foreign keys live in `Engine`, not `Table`, because a table cannot see its
siblings. `check_outgoing_fks` runs before every insert/update against the
harnessed database; `check_incoming_fks` runs before parent updates, deletes,
drops, and renames. There is no cascade and no deferral: restrict only, which
is the one policy a beginner can reason about. `beelzebub` needs no engine
code — it rides on the unique + required paths Table already owns.

The parser's `parse_column_def` loop is order-free: each flag sets a
seen-bit and duplicates/conflicts (`not null` + `null`, two `check`s, two
`references`) fail at parse time with the offending token's position. Two
`beelzebub`s in one `forge` parse fine and die in `Schema` — grammar accepts,
invariants reject, each layer owning its own rule.

Snapshots stay readable text: `column pay decimal required check above 0
references teams id` is still one whitespace-split line. `and` between checks
is ExdeusQL-only; on disk each rule repeats `check`, so the loader never
needs the `and` keyword. Version 2 writes the new flags; the loader accepts
1 and 2, and old files (always `required`, no new flags) load unchanged.

## Build and test

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

New reds this lesson caught while building: the aggregate `Column{...}`
initializers in tests needed no change (trailing members value-initialize),
but `CheckConstraint` has no default constructor (its `Value` member has no
default either) — both call sites build it as `CheckConstraint{op, literal}`
instead of default-then-assign. The snapshot header test expected v1 and now
expects v2. Green: 418 PASS (21 value, 27 table, 28 engine, 60 lexer, 125
parser, 125 interpreter, 32 snapshot + e2e), `ctest 1/1`.

## Completion checkpoint

Do not continue until you can explain:

- why `beelzebub` implies unique + required instead of being a fourth independent flag
- why two nulls in a unique column do not conflict
- why CHECK skips nulls instead of failing them
- why foreign keys are enforced in `Engine` rather than `Table`
- why restrict (no cascade) is the only sane week-1 delete policy
- why snapshots repeat `check` per rule instead of using `and`
