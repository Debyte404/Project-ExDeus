# Exdeus

Exdeus is an educational custom database engine written in C++.

The project has two language and runtime boundaries:

- **Exdeus** — the engine, in-memory data model, local snapshots, and REPL executable.
- **ExdeusQL** — the custom language lexer, parser, and command model.

The first prototype is intentionally small but practical. It will support databases such as a starter bank or college database, with local readable snapshots rather than crash-safe durability. Durability, recovery, indexes, and transactions are planned for week 2.

## Learning path

Read the lessons in `docs/book` in order. The book tells you which file to edit, why that file exists, how it connects to earlier code, and how to verify your work.

The production C++ implementation is yours to write. The project scaffolding and tests provide the rails; the exercises provide the next piece of code to implement.

## Build on Windows PowerShell

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

## Try it

```powershell
Get-Content examples/bank.exql | .\build\Debug\exdeus.exe
Get-Content examples/college.exql | .\build\Debug\exdeus.exe
```

Two scripted databases (bank, college) run through the interactive REPL:
create, harness, forge, add, seek, change, remove, save, export. Inspect
`bank.exd` and `customers.csv` afterwards — both are readable text.

## Week-1 scope

The prototype promises typed values, explicit database selection, full
row lifecycle, filtered/projected/ordered queries, atomic snapshots,
CSV export, and a REPL that survives every command error. It does not
promise crash recovery, concurrency, transactions, or SQL compatibility —
see `docs/book/10-testing-and-week1-checkpoint.md` for the honest
inventory and the measurements to collect before week 2.
