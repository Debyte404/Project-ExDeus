#pragma once

#include <string>
#include <variant>
#include <vector>

#include "exdeus/core/value.hpp"

namespace exdeus::language {

// Column declaration inside `forge table ... with name: type [unique]`.
struct ColumnDef {
    std::string name;
    exdeus::core::ValueType type;
    bool unique = false;
};

// Comparison operators for `where` filters.
enum class FilterOp {
    Equals,  // equals / == / =
    Above,   // above / >
    Below,   // below / <
};

// One `where` condition: `<column> <op> <literal>`, chained with and/or.
struct Condition {
    std::string column;
    FilterOp op;
    exdeus::core::Value literal;
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

// `show a, b` projection. Empty = all columns.
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

struct SeekRows {
    std::string table;
    Projection show;
    std::vector<Condition> where;
    std::string order_by;
    SortDirection direction = SortDirection::None;
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
