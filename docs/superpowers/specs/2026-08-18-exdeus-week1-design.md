# Exdeus Week 1 Prototype Design

## Goal

Build a demonstrable local database prototype in C++ with a custom language called ExdeusQL and a REPL executable called `exdeus`. The prototype must be capable of modeling small practical databases such as a starter bank, college, or inventory database.

This is an educational project. The implementation will be intentionally explicit, modular, and explained line by line in `docs/book`. The user writes the C++ implementation; the guide provides the file map, exercises, explanations, tests, and checkpoints.

## Scope boundary

### Week 1 includes

- A C++ executable named `exdeus`.
- An interactive REPL controlled through the terminal.
- A lexer that turns ExdeusQL text into tokens.
- A parser that turns tokens into command objects.
- An interpreter that executes command objects.
- In-memory database, table, schema, column, and row structures.
- Databases and tables can be created, selected, listed, renamed, and removed.
- Integer, decimal, text, and boolean column types.
- Required values and unique columns.
- Adding, reading, updating, and removing rows.
- Filtering with comparisons and `and`/`or`.
- Adding and removing columns.
- A readable text snapshot format for save/load.
- CSV export for a selected table.
- Tests for each layer and at least one end-to-end bank database scenario.
- A line-by-line C++ learning book accompanying each implementation step.

### Week 1 explicitly excludes

- Crash-safe write-ahead logging.
- Transactions and concurrent clients.
- Page layout, binary files, indexes, query planning, and a network server.
- Full SQL compatibility.
- User-configurable syntax aliases.

These are week-2 expansion points, not hidden promises of the prototype.

## Product names and boundaries

`exdeus` is the database engine and executable name. It owns data structures, validation, execution, and local snapshot I/O.

`exdeusQL` is the language layer. It owns tokenization, grammar, syntax errors, and conversion of source text into engine commands. The language syntax is developer-controlled. Users can write ExdeusQL programs, but they do not redefine keywords.

The REPL is the first client. It reads one command or command block, sends it to ExdeusQL, executes the result through Exdeus, and prints a human-readable result.

## Language direction

The language uses verbs that describe database actions without copying SQL keywords directly:

```text
harness bank

forge table customers with
  id: integer unique,
  name: text,
  account_type: text,
  balance: decimal,
  active: boolean

add customers (1, "Asha", "savings", 25000.00, true)

seek customers
  show name, balance
  where balance above 10000
  order by balance descending

change customers
  set balance to balance plus 500
  where account_type equals "savings"

save database to "bank.exd"
load database from "bank.exd"
export customers to "customers.csv"
```

The first grammar will be small and strict. Parser errors will report the unexpected token, its location, and a short correction hint where possible.

## Architecture

```text
REPL
  -> Input reader
  -> exdeusQL lexer
  -> exdeusQL parser
  -> command model
  -> interpreter
  -> engine catalog and table operations
  -> snapshot/CSV services
```

The parser does not directly mutate tables. The interpreter does not parse strings. The engine does not know about keyword spellings. This separation is required so week 2 can add a catalog, persistence, and indexes without rewriting the language.

## Proposed teaching-oriented file map

The exact build-system files may vary after the implementation plan, but the design uses these responsibilities:

```text
include/exdeus/core/       Public engine model types and interfaces
include/exdeus/language/   Tokens, AST/command types, lexer/parser interfaces
include/exdeus/io/         Snapshot and export interfaces
include/exdeus/repl/       REPL-facing interfaces
src/core/                  Engine implementations
src/language/              Lexer, parser, and command implementations
src/io/                    Text snapshot and CSV implementations
src/repl/                  CLI loop and formatting
tests/                     Focused unit and end-to-end tests
examples/                  Bank and college ExdeusQL scripts
docs/book/                 Lessons, exercises, diagrams, and checkpoints
```

Each book chapter will identify:

1. The file to create or edit.
2. The exact concept being introduced.
3. The code connections to earlier files.
4. A small exercise for the user to complete.
5. The command used to build or test it.
6. A checkpoint explaining what should now be understood.

The agent may create file names, test harnesses, examples, and explanations, but the user will write the production C++ bodies. When code is needed, the guide will provide a small target, not silently complete the entire project.

## In-memory model and local snapshots

The engine will use memory as its active working set. A database remains available between commands during one `exdeus` session.

The week-1 snapshot is readable text with an explicit version header, database name, table declarations, column metadata, and escaped row values. Snapshot loading reconstructs the in-memory model. Saving writes a complete replacement snapshot through a temporary path and then renames it; this is a safe file-update technique, but it is not yet a complete power-loss recovery protocol.

The storage interface will be separated from table operations so week 2 can add checksums, backups, journaling, and recovery without changing command semantics.

## Validation and error behavior

- Unknown databases, tables, and columns produce named errors.
- Duplicate table names are rejected.
- Row value count must match the schema.
- Values are checked against their declared type.
- Unique columns reject duplicate non-null values.
- Invalid commands do not partially mutate the database.
- Lexer and parser errors include line and column positions.
- The REPL remains alive after a command error.

## Testing strategy

Tests will be written before each production behavior is implemented:

- lexer token tests
- parser command tests
- schema and row validation tests
- table mutation tests
- filtering and projection tests
- snapshot round-trip tests
- CSV export tests
- REPL/interpreter end-to-end test using the bank example

The final week-1 acceptance test creates a bank database, creates customers and accounts tables, inserts records, queries balances, changes a balance, removes a record, saves the database, loads it into a fresh engine, and confirms the data remains available.

## Week 2 extension seams

The following interfaces must remain stable enough for later work:

- `DatabaseEngine` command execution boundary.
- `Catalog` ownership of databases, tables, and schema metadata.
- `SnapshotStore` persistence boundary.
- `Table` row access and mutation boundary.
- `Value` type representation.
- `Command` model produced by the parser.

Week 2 can then introduce a real catalog, file-backed pages, write-ahead logging, backup rotation, checksums, indexes, transactions, and a richer query planner in separate milestones.

## Definition of done

Week 1 is complete when a new learner can follow the book from an empty checkout, write the C++ implementation in the named files, build `exdeus`, run the REPL, execute the bank example, inspect the generated `.exd` text snapshot, and pass the automated test suite.
