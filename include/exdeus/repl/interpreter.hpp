#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "exdeus/core/engine.hpp"
#include "exdeus/core/value.hpp"
#include "exdeus/language/command.hpp"

namespace exdeus::repl {

// Named failure for command-vs-engine mismatches: unknown columns,
// type errors, snapshot commands parked for Lesson 8.
class InterpreterError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Tabular answer for `seek`: the columns shown plus one row of Values
// per matching database row. Other commands leave it empty.
struct ResultTable {
    std::vector<std::string> columns;
    std::vector<std::vector<exdeus::core::Value>> rows;
};

// Human-readable outcome of one executed command: a one-line message
// plus an optional table for `seek`.
struct Result {
    std::string message;
    std::optional<ResultTable> table;
};

// Interpreter executes parsed Command objects against an Engine.
// It owns grammar-free execution only: no Lexer, no Parser inside.
// SaveDb/LoadDb/ExportTable are accepted and parked with InterpreterError
// until the Lesson-8 snapshot and CSV services exist.
class Interpreter {
public:
    Interpreter() = default;

    // Execute a single parsed command, returning its message (and a table for seek).
    Result execute(const exdeus::language::Command& command);

    // Execute a `;`-separated script in order; one Result per Command.
    std::vector<Result> execute_all(const std::vector<exdeus::language::Command>& commands);

    exdeus::core::Engine& engine();
    const exdeus::core::Engine& engine() const;

private:
    Result run_create(const exdeus::language::CreateDb& command);
    Result run_harness(const exdeus::language::HarnessDb& command);
    Result run_forge(const exdeus::language::ForgeTable& command);
    Result run_add(const exdeus::language::AddRow& command);
    Result run_seek(const exdeus::language::SeekRows& command);
    Result run_change(const exdeus::language::ChangeRows& command);
    Result run_remove(const exdeus::language::RemoveRows& command);

    exdeus::core::Engine engine_;
};
}  // namespace exdeus::repl
