# Lesson 9: Bank Walkthrough

## Goal

Prove every layer composes by running two real databases end to end:

- a bank (customers, accounts, transactions) and a college (students,
  courses, enrollments) written as plain `.exql` scripts
- each script exercises create, harness, forge, add, seek, change, remove,
  save, and export through the exact binary the user types into
- the acceptance test replays the bank script file as data through the
  production Lexer → Parser → Interpreter path, then reloads the snapshot
  in a fresh engine and checks the CSV on disk

No new production code. This lesson is composition: earlier lessons were
proved in isolation; this one proves they work together.

## Files for this lesson

- `examples/bank.exql` — the starter bank script (20 commands).
- `examples/college.exql` — the college script (24 commands).
- `tests/end_to_end_tests.cpp` — `run_end_to_end_tests()`, the executable proof.
- `tests/test_main.cpp` — registers `run_end_to_end_tests()`.

## Design target

The bank script opens three tables and tells one story:

```text
create db bank;  harness bank;
forge table customers with id: integer unique, name: text,
  account_type: text, balance: decimal, active: boolean;
add customers (1, "Asha", "savings", 25000.50, true);
add customers (2, "Ravi", "current", 12000.00, true);
add customers (3, "Mira", "savings", 8000.25, false);
forge table accounts with id, customer_id, number, opened;
add accounts (101, 1, "SAV-1001", "2026-01-15");  -- plus 102, 103
forge table transactions with id, account_id, kind, amount;
add transactions (1001, 101, "deposit", 5000.00);  -- plus 1002, 1003
seek customers show name, balance
  where account_type equals "savings" and balance above 10000
  order by balance descending;
change customers set balance to balance plus 500
  where account_type equals "savings";
remove customers where name equals "Mira";
seek customers show name, balance order by balance descending;
save db to "bank.exd";
export customers to "customers.csv";
```

The college script mirrors the shape with different data: students with
years, courses with credits, enrollments with grades; a grade-bump
`change`, a withdrawal `remove` that really deletes one row, and the same
save/export close.

Comment lines (`-- ...`) and blank lines ride along in the file. The REPL
accumulates them into the pending buffer and the lexer skips them, so
scripts stay readable without affecting parsing. Blocks may span lines —
a command fires only when its `;` arrives outside a string.

## Line-by-line connections

The scripts are data, not code: they run through the shipped `exdeus`
binary by piping the file into stdin:

```powershell
Get-Content examples/bank.exql | .\build\Debug\exdeus.exe
```

The acceptance test does the same thing programmatically. It reads
`examples/bank.exql` from disk (searching the usual build-tree relative
paths plus the repo absolute path), redirects the two fixed output names
to temp files, and runs `parse_all` + `execute_all` — the identical calls
the REPL makes. That ordering matters: the test proves the script file,
not a copy of it. If someone edits the script, the test follows.

Result indexes pin the story: command 14 (the rich-savings seek) returns
only Asha; 15 changes 2 savings rows; 16 removes Mira; 17 shows the two
survivors; 18 saves; 19 exports. Counting is 0-based over the 20 parsed
commands — the `;` inside the header comment is comment text, not a
command, which is exactly why the test counts parsed commands instead of
semicolons.

The reload phase is the week-1 durability promise in miniature: a fresh
`Interpreter` loads the snapshot and checks all three tables plus Asha's
post-interest balance (`25500.50`). The CSV phase reads the exported file
as text: five-column header, Asha with interest, no Mira. Temp paths use
forward slashes so they parse inside ExdeusQL string literals, and a
per-file counter keeps reruns hermetic.

One scripting lesson earned its place: Mira's college withdrawal first
removed 0 rows because the draft gave her no enrollments. A `remove` that
matches nothing succeeding silently is correct engine behavior — but a
demo that deletes nothing demonstrates nothing. The script now enrolls
her (id 3005) before withdrawing her, so the story removes exactly 1 row.

## How the test executable is connected

`tests/end_to_end_tests.cpp` exposes `void run_end_to_end_tests()`, and
`tests/test_main.cpp` calls it last. It proves, in order:

```text
bank script parses to twenty commands; every command executes
create/harness/forge messages for bank, customers, accounts, transactions
rich-savings seek returns only Asha
interest changes 2 rows; Mira removed; survivors seek shows 2
save and export report the temp paths
fresh engine reloads: 2 customers, 3 accounts, 3 transactions
Asha keeps 25500.50 after reload
csv keeps the header, Asha with interest, and drops Mira
```

## Build and test

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Both scripts were proven by piping them into the real binary before the
test encoded any expectation — bank exits 0 with `bank.exd` (774 bytes)
and `customers.csv`; college exits 0 with `college.exd` and
`enrollments.csv`. The green suite prints 21 value, 27 table, 28 engine,
48 lexer, 80 parser, 47 interpreter, 34 snapshot, and 22 end-to-end PASS
lines plus all eight footers (307 total).

## Completion checkpoint

Do not continue to Lesson 10 until you can explain:

- why the test reads the script file instead of embedding its commands
- why result indexes are 0-based over parsed commands, not semicolons
- why temp paths use forward slashes inside ExdeusQL strings
- why the reload phase uses a fresh Interpreter instead of the same one
- why Mira needed an enrollment before her withdrawal meant anything
