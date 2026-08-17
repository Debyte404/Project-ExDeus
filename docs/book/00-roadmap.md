# Exdeus Book Roadmap

This book teaches C++ by building a small database engine from the ground up. You already know basic programming and data structures; the new challenge is learning how many small C++ files cooperate inside one project.

## How to use each lesson

For every chapter:

1. Read the goal before opening the code.
2. Inspect the files named in the chapter.
3. Write the smallest piece requested by the exercise.
4. Build immediately.
5. Run the focused test.
6. Explain the data flow in your own words before continuing.

Do not jump ahead to the parser or storage format. The project is ordered so each new idea has a previous idea to stand on.

## Milestones

| Lessons | Result | Main C++ ideas |
| --- | --- | --- |
| 01 | Buildable project shell | CMake, headers, source files, linking |
| 02 | Typed values | enums, `std::variant`, references, const correctness |
| 03 | Tables and rows | classes, vectors, maps, invariants |
| 04 | Database engine | composition, ownership, interfaces |
| 05 | Lexer | strings, lookahead, token positions |
| 06 | Parser | recursive descent, command objects |
| 07 | REPL | streams, control flow, interpreter boundary |
| 08 | Local snapshots | filesystem, serialization, escaping |
| 09 | Practical examples | composing all layers |
| 10 | Week-1 checkpoint | testing, limits, explanation |
| 11 | Week-2 map | durability and upgrade seams |

## Rule for this project

The database must be understandable before it is clever. A simple design that you can explain is more valuable than an advanced design that you cannot debug.
