#pragma once

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "exdeus/core/value.hpp"

namespace exdeus::language {

// Comparison operators for `where` filters and CHECK constraints.
enum class FilterOp {
    Equals,  // equals / == / =
    Above,   // above / >
    Below,   // below / <
};

// Column declaration inside `forge table ... with ...`.
// Example: `id: integer beelzebub, name: text not null,
//            balance: decimal check above 0,
//            customer_id: integer references customers(id)`
struct CheckDef {
    FilterOp op;
    exdeus::core::Value literal;
};

struct RefDef {
    std::string table;
    std::string column;
};

struct ColumnDef {
    std::string name;
    exdeus::core::ValueType type;
    bool unique = false;
    bool required_flag = true;  // true = NOT NULL; `null` alone makes it false
    bool is_primary = false;    // beelzebub: single per table, implies unique + required
    std::vector<CheckDef> checks;
    std::optional<RefDef> references;
};

// One `where` condition. Compares a column against a literal
// (`balance above 10000`) or against another column in a JOIN
// (`customers.id equals accounts.customer_id`).
struct Condition {
    std::string column;         // left side: bare or table-qualified
    FilterOp op;
    bool right_is_column = false;
    std::string right_column;   // only when right_is_column
    exdeus::core::Value literal;  // only when !right_is_column
    // How this condition joins to the NEXT one. Ignored on the last.
    // True = `and`, false = `or`.
    bool and_with_next = true;
};

// `set <col> to <expr>` inside `change`: either a literal or
// `<col> plus|minus <literal>` arithmetic on the row's own value.
struct Assignment {
    std::string column;
    bool is_arithmetic = false;
    std::string source_column;  // only when is_arithmetic
    bool is_plus = true;        // plus vs minus, only when arithmetic
    exdeus::core::Value value;  // literal, or the arithmetic operand
};

// `show a, b` projection. Empty = all columns, and so does a lone `*`.
using Projection = std::vector<std::string>;

enum class SortDirection {
    None,
    Ascending,
    Descending,
};

// --- Week-1 command objects. The parser builds these; the Lesson-7
// interpreter executes them against Engine. No engine headers here. ---

struct CreateDb {
    std::string name;
};

struct HarnessDb {
    std::string name;
};

struct ForgeTable {
    std::string table;
    std::vector<ColumnDef> columns;
};

struct AddRow {
    std::string table;
    std::vector<exdeus::core::Value> values;
};

// Inner equi-join: `inner join <table> on <left> equals <right>`.
// Column sides must be qualified (`customers.id`); only Equals is allowed.
struct JoinClause {
    std::string table;
    std::string left;
    std::string right;
};

struct SeekRows {
    std::string table;  // empty when from_query is set
    // Parenthesized subquery as the FROM source:
    // `seek (seek customers show id) show id`. The inner answer is
    // materialized in memory; the outer query reads it like a table.
    std::shared_ptr<SeekRows> from_query;
    Projection show;
    std::optional<JoinClause> join;
    std::vector<Condition> where;
    std::string order_by;
    SortDirection direction = SortDirection::None;
    // `seek ... into <name>`: stash the answer as a session-temp table.
    std::optional<std::string> into_table;
};

struct ChangeRows {
    std::string table;
    std::vector<Assignment> set;
    std::vector<Condition> where;  // empty = all rows
};

struct RemoveRows {
    std::string table;
    std::vector<Condition> where;  // empty = all rows
};

struct SaveDb {
    std::string path;
};

struct LoadDb {
    std::string path;
};

struct ExportTable {
    std::string table;
    std::string path;
};

using Command = std::variant<CreateDb, HarnessDb, ForgeTable, AddRow, SeekRows,
                             ChangeRows, RemoveRows, SaveDb, LoadDb, ExportTable>;

}  // namespace exdeus::language
