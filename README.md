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
cmake --build build
ctest --test-dir build --output-on-failure
```

At the first checkpoint, the executable is only a project bootstrap. The database behavior arrives through the following lessons.
