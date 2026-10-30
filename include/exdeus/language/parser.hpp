#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include "exdeus/language/command.hpp"
#include "exdeus/language/token.hpp"

namespace exdeus::language {

// Named failure: unexpected token or truncated input, always with the
// 1-based line and column of the offending token. Carries the position
// structurally (for the REPL caret) as well as inside the message.
class ParserError : public std::runtime_error {
public:
    ParserError(const std::string& message, Position position)
        : std::runtime_error(message), position_(position) {}
    Position position() const { return position_; }

private:
    Position position_{1, 1};
};

// Recursive-descent parser: one grammar function per command shape.
// Consumes a Lexer token stream, produces Command objects, never touches
// Engine/Catalog/Table. Grammar functions mirror the ExdeusQL forms:
//
//   create db <name>;
//   harness <name>;
//   forge table <name> with <col>: <type> [beelzebub] [unique]
//                [required | not null | null] [check <op> <lit> (and <op> <lit>)*]
//                [references <table>(<col>)], ... ;
//   add <table> (<lit>, ...);
//   seek <table> [show <col>, ... | show *]
//                [inner join <table> on <q.col> equals <q.col>]
//                [where <cond> (and|or <cond>)*]
//                [order by <col> [ascending|descending]]
//                [into <table>];
//   The <table> may be a parenthesized subquery instead:
//   seek (seek customers show id) show id;
//   `sort <table> by <col> [asc|desc]` — sugar over a bare ordered seek.
//   `alter table <t> add <coldef> [default <lit>]` — new column, existing
//   rows take the default (or null when nullable).
//   `alter table <t> remove <col>` — drop a column and its values.
//   `into` stashes the answer as a session-temp table (never snapshotted).
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
    // A column reference: bare (`balance`) or table-qualified (`customers.id`).
    std::string expect_column(const std::string& what);

    Command parse_command();
    CreateDb parse_create();
    HarnessDb parse_harness();
    ForgeTable parse_forge();
    AddRow parse_add();
    SeekRows parse_seek();
    // Subquery body shared by top-level seeks and parenthesized subqueries.
    // `depth` caps nesting so `(((...` cannot blow the call stack.
    SeekRows parse_seek_rest(int depth);
    // `sort <table> by <col> [asc|desc]` — sugar over a bare seek that only
    // orders. Desugared here so the interpreter sees one code path.
    SeekRows parse_sort();
    // `alter table <t> add <coldef> [default <lit>]` and
    // `alter table <t> remove <col>` — schema evolution.
    Command parse_alter();
    ChangeRows parse_change();
    RemoveRows parse_remove();
    SaveDb parse_save();
    LoadDb parse_load();
    ExportTable parse_export();

    Projection parse_projection();
    std::optional<JoinClause> parse_join();
    std::vector<Condition> parse_conditions();
    Condition parse_condition();
    CheckDef parse_check_item();
    std::vector<Assignment> parse_assignments();
    Assignment parse_assignment();
    ColumnDef parse_column_def();

    std::vector<Token> tokens_;
    size_t cursor_ = 0;
};

}  // namespace exdeus::language
