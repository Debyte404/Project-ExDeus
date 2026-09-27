# Lesson 10: Testing and Week-1 Checkpoint

## Goal

Close week 1 with an honest inventory: what the prototype promises, what
it cannot, and how the suite proves the difference.

## What week 1 can promise

- Four typed values (integer, decimal, text, boolean) with strict
  per-column checking and unique-column rejection.
- Named databases with explicit `harness` selection — creating never
  selects, and every table command without a harness fails loudly.
- Tables with ordered schemas, duplicate-name rejection, and full row
  lifecycle: insert, update, remove, add/remove column.
- An ExdeusQL front end (lexer + recursive-descent parser) producing
  engine-independent command objects, with line/column errors and
  correction hints.
- Filtered, projected, ordered `seek`; literal and arithmetic `change`
  with int/decimal promotion; back-to-front `remove`.
- A versioned readable snapshot with atomic save (temp + rename) and
  two-phase load (parse all, then rebuild), plus CSV export with quoting.
- A REPL that reads until `;`, prints aligned tables, and survives every
  lexer, parser, engine, and io error.
- Two scripted example databases replayed byte-for-byte by the acceptance
  test, including save → fresh-engine reload → CSV verification.

## What week 1 cannot promise

- Crash safety. Atomic rename protects one `save` call from torn reads;
  a power cut mid-rename, a torn temp write, or a corrupted disk are not
  recovered. There is no write-ahead log, no checksum, no backup rotation.
- Concurrency. One engine, one thread, no locking. Two REPLs on one
  snapshot file overwrite each other; last rename wins, silently.
- Transactions. Each command commits immediately. A five-command script
  that fails on command four keeps the first three — there is no rollback.
- Scale. Every table is a `std::vector` of rows; every filter is a full
  scan; ordering is a sort of the match set. Thousands of rows are fine;
  millions are not the design point.
- SQL compatibility. ExdeusQL is its own small grammar: three comparisons,
  `and`/`or` chains, one `order by` key, no joins, no aggregation, no
  subqueries, no views.

## The suite as proof

Eight suites, 307 checks, one binary, zero external frameworks:

| Suite | Checks | Owns |
| --- | --- | --- |
| value | 21 | variant types, equality, formatting |
| table | 27 | validation, mutation, reshape |
| engine | 28 | selection contract, facade errors |
| lexer | 48 | vocabulary, literals, positions |
| parser | 80 | grammar, commands, failure cases |
| interpreter | 47 | execution, filters, arithmetic, save/load/export wiring |
| snapshot | 34 | format, atomicity, malformed input, CSV |
| end-to-end | 22 | bank script replay, reload, CSV on disk |

Each suite registers one `run_*` function in `tests/test_main.cpp` and
prints its own `All ... tests passed.` footer. A red suite names its
layer; a passing run proves the layers compose.

## The week-1 gate

From an empty checkout, a new learner can:

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
Get-Content examples/bank.exql | .\build\Debug\exdeus.exe
```

Then open `bank.exd`, read the snapshot format from Lesson 8, and trace
one line of it — say `row 1 "Asha" 25000.5` — back through the parser
(`AddRow`), the interpreter (`run_add`), the engine (`insert`), and the
table (`validate_row`). Explaining that path in their own words is the
real checkpoint; the green suite is the evidence it is explainable.

## Measurements to collect before week 2

- Snapshot size vs row count for the bank script (today: 774 bytes for
  2 + 3 + 3 rows) — the baseline write-amplification number.
- Full-scan seek latency at 1k / 10k / 100k rows — the number that
  justifies (or postpones) indexes.
- Save latency vs database size — the number that justifies journaling
  over full rewrites.
- REPL memory after repeated load cycles — the number that finds leaks
  before pages and caches arrive.

## Completion checkpoint

Week 1 is done when you can explain, without opening the code:

- why creating a database does not select it
- why the parser must never include engine headers
- why `remove` deletes from the back of its match list
- why save writes temp-plus-rename and load parses before touching
- which week-1 promise breaks first under two writers, and why
