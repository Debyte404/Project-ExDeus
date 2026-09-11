#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include "exdeus/language/command.hpp"
#include "exdeus/language/token.hpp"

namespace exdeus::language {

// Named failure: unexpected token or truncated input, always with the
// 1-based line and column of the offending token.
class ParserError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Recursive-descent parser: one grammar function per command shape.
// Consumes a Lexer token stream, produces Command objects, never touches
// Engine/Catalog/Table. Grammar functions mirror the ExdeusQL forms:
//
//   create db <name>;
//   harness <name>;
//   forge table <name> with <col>: <type> [unique], ... ;
//   add <table> (<lit>, ...);
//   seek <table> [show <col>, ...] [where <cond> (and|or <cond>)*]
//                [order by <col> [ascending|descending]];
//   change <table> set <col> to <expr>, ... [where ...];
//   remove <table> [where ...];   ("remove <t> where ..." deletes rows;
//                                 table drops arrive in Lesson 7 via REPL)
//   save db to "<path>";  load db from "<path>";
//   export <table> to "<path>";
class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    // Parse exactly one command; trailing tokens after `;` are an error.
    Command parse_one();
    // Parse a `;`-separated sequence (used by the REPL and scripts).
    std::vector<Command> parse_all();

private:
    const Token& peek() const;
    const Token& advance();
    bool at_end() const;
    bool check(TokenKind kind) const;
    bool match(TokenKind kind);
    const Token& expect(TokenKind kind, const std::string& what);
    [[noreturn]] void fail(const Token& token, const std::string& expected) const;
    [[noreturn]] void fail_at_end(const std::string& expected) const;

    std::string expect_name(const std::string& what);
    exdeus::core::Value expect_literal(const std::string& what);
    std::string expect_string(const std::string& what);

    Command parse_command();
    CreateDb parse_create();
    HarnessDb parse_harness();
    ForgeTable parse_forge();
    AddRow parse_add();
    SeekRows parse_seek();
    ChangeRows parse_change();
    RemoveRows parse_remove();
    SaveDb parse_save();
    LoadDb parse_load();
    ExportTable parse_export();

    Projection parse_projection();
    std::vector<Condition> parse_conditions();
    Condition parse_condition();
    std::vector<Assignment> parse_assignments();
    Assignment parse_assignment();
    ColumnDef parse_column_def();

    std::vector<Token> tokens_;
    size_t cursor_ = 0;
};

}  // namespace exdeus::language
