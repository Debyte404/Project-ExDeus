// Lesson 7 implementation: executes parsed commands against the Engine.
#include "exdeus/repl/interpreter.hpp"

#include <algorithm>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

#include "exdeus/io/csv_exporter.hpp"
#include "exdeus/io/snapshot_store.hpp"

namespace exdeus::repl {

namespace {

using exdeus::core::Column;
using exdeus::core::CheckConstraint;
using exdeus::core::CheckOp;
using exdeus::core::Engine;
using exdeus::core::ForeignRef;
using exdeus::core::Row;
using exdeus::core::Schema;
using exdeus::core::Table;
using exdeus::core::Value;
using exdeus::core::ValueType;
using exdeus::language::Assignment;
using exdeus::language::Condition;
using exdeus::language::FilterOp;

bool is_number(const Value& value) {
    return value.type() == ValueType::Integer || value.type() == ValueType::Decimal;
}

double as_double(const Value& value) {
    if (value.type() == ValueType::Integer) {
        return static_cast<double>(std::get<long long>(value.data()));
    }
    return std::get<double>(value.data());
}

// Equality with int/decimal promotion: `where balance equals 25000`
// matches a decimal column. Non-numeric pairs use exact Value equality.
bool values_equal(const Value& left, const Value& right) {
    if (is_number(left) && is_number(right)) {
        return as_double(left) == as_double(right);
    }
    return left == right;
}

// Ordering is numeric-only: text/boolean have no above/below meaning.
bool value_above(const Value& left, const Value& right) {
    if (is_number(left) && is_number(right)) {
        return as_double(left) > as_double(right);
    }
    throw InterpreterError("above/below needs numeric values, found '" + left.to_string() +
                           "' and '" + right.to_string() + "'");
}

bool eval_condition(const Value& cell, const Condition& condition) {
    switch (condition.op) {
        case FilterOp::Equals:
            return values_equal(cell, condition.literal);
        case FilterOp::Above:
            return value_above(cell, condition.literal);
        case FilterOp::Below:
            return value_above(condition.literal, cell);
    }
    throw InterpreterError("unknown filter operator");
}

// Build a column-resolution map: bare name -> index, qualified name
// (table.column) -> index. In a JOIN, bare names prefer the left table
// and only map if the right table does not already claim them.
std::map<std::string, size_t> build_columns(
    const std::string& left_table, const Schema& left_schema,
    const std::string* right_table, const Schema* right_schema) {
    std::map<std::string, size_t> columns;
    size_t next_index = 0;
    for (const Column& c : left_schema.columns()) {
        columns[c.name] = next_index;
        columns[left_table + "." + c.name] = next_index;
        ++next_index;
    }
    if (right_schema) {
        for (const Column& c : right_schema->columns()) {
            if (columns.find(c.name) == columns.end()) {
                columns[c.name] = next_index;
            }
            columns[*right_table + "." + c.name] = next_index;
            ++next_index;
        }
    }
    return columns;
}

size_t resolve_column(const std::map<std::string, size_t>& columns, const std::string& ref) {
    auto it = columns.find(ref);
    if (it == columns.end()) {
        throw InterpreterError("unknown column: " + ref);
    }
    return it->second;
}

// Evaluate one condition against a row through the column map.
// Supports literal comparisons and column-to-column equality.
bool eval_on_row(const Row& row, const std::map<std::string, size_t>& columns,
                 const Condition& condition) {
    size_t idx = resolve_column(columns, condition.column);
    if (condition.right_is_column) {
        return values_equal(row[idx], row[resolve_column(columns, condition.right_column)]);
    }
    return eval_condition(row[idx], condition);
}

// Left-associative and/or fold: conditions[i].and_with_next joins
// conditions[i] to conditions[i + 1]. Empty means no filter: match all.
bool row_matches(const Row& row, const std::map<std::string, size_t>& columns,
                 const std::vector<Condition>& where) {
    if (where.empty()) {
        return true;
    }
    bool result = eval_on_row(row, columns, where[0]);
    for (size_t i = 0; i + 1 < where.size(); ++i) {
        bool next = eval_on_row(row, columns, where[i + 1]);
        result = where[i].and_with_next ? (result && next) : (result || next);
    }
    return result;
}

std::string plural(size_t count, const char* word) {
    return std::to_string(count) + " " + word + (count == 1 ? "" : "s");
}

// Strip a `table.` qualifier: `customers.name` -> `name`. Bare names pass
// through unchanged.
std::string bare_name(const std::string& ref) {
    size_t dot = ref.rfind('.');
    return (dot == std::string::npos) ? ref : ref.substr(dot + 1);
}

// Build a session-temp-shaped Table from an answer: bare display names,
// exact column types, everything nullable, no checks or keys. Duplicate
// bare names (e.g. a join's two `id` columns) fail loudly so the caller
// narrows the projection instead of guessing.
Table materialize_temp(const std::string& name, const ResultTable& answer,
                       const std::vector<ValueType>& types) {
    if (types.size() != answer.columns.size()) {
        throw InterpreterError("internal: answer type/column count mismatch");
    }
    std::vector<Column> cols;
    for (size_t i = 0; i < answer.columns.size(); ++i) {
        std::string bare = bare_name(answer.columns[i]);
        for (const Column& existing : cols) {
            if (existing.name == bare) {
                throw InterpreterError("column '" + bare +
                                       "' is ambiguous: narrow the projection");
            }
        }
        cols.push_back(Column{bare, types[i], false, false, false, {}, std::nullopt});
    }
    Table staged(name, Schema(std::move(cols)));
    for (const std::vector<Value>& vals : answer.rows) {
        staged.insert(vals);
    }
    return staged;
}

// File paths arrive from ExdeusQL string literals. On Windows users type
// backslashes (`snapshots\bank.exd`), so normalize them to forward slashes
// before touching the filesystem — backslash is not a valid filename
// character on Windows, so this never renames a real file. Note: `\n` and
// `\t` are decoded as escapes by the lexer before we see them, so paths
// containing those sequences still need forward slashes (see help text).
std::string normalize_separators(const std::string& path) {
    std::string out = path;
    for (char& c : out) {
        if (c == '\\') {
            c = '/';
        }
    }
    return out;
}

// Absolute location for result messages, so the user always sees WHERE the
// file went regardless of the process working directory. generic_string
// keeps forward slashes on every platform.
std::string display_path(const std::string& path) {
    std::error_code ec;
    std::string abs =
        std::filesystem::absolute(std::filesystem::path(path), ec).generic_string();
    if (ec) {
        return path;
    }
    return abs;
}

}  // namespace

exdeus::core::Engine& Interpreter::engine() {
    return engine_;
}

const exdeus::core::Engine& Interpreter::engine() const {
    return engine_;
}

Result Interpreter::execute(const exdeus::language::Command& command) {
    return std::visit(
        [this](const auto& active) -> Result {
            using T = std::decay_t<decltype(active)>;
            if constexpr (std::is_same_v<T, exdeus::language::CreateDb>) {
                return run_create(active);
            } else if constexpr (std::is_same_v<T, exdeus::language::HarnessDb>) {
                return run_harness(active);
            } else if constexpr (std::is_same_v<T, exdeus::language::ForgeTable>) {
                return run_forge(active);
            } else if constexpr (std::is_same_v<T, exdeus::language::AddRow>) {
                return run_add(active);
            } else if constexpr (std::is_same_v<T, exdeus::language::SeekRows>) {
                return run_seek(active);
            } else if constexpr (std::is_same_v<T, exdeus::language::ChangeRows>) {
                return run_change(active);
            } else if constexpr (std::is_same_v<T, exdeus::language::RemoveRows>) {
                return run_remove(active);
            } else if constexpr (std::is_same_v<T, exdeus::language::SaveDb>) {
                return run_save(active);
            } else if constexpr (std::is_same_v<T, exdeus::language::LoadDb>) {
                return run_load(active);
            } else if constexpr (std::is_same_v<T, exdeus::language::ExportTable>) {
                return run_export(active);
            }
        },
        command);
}

std::vector<Result> Interpreter::execute_all(
    const std::vector<exdeus::language::Command>& commands) {
    std::vector<Result> results;
    for (const auto& command : commands) {
        results.push_back(execute(command));
    }
    return results;
}

Result Interpreter::run_create(const exdeus::language::CreateDb& command) {
    engine_.create_database(command.name);
    return Result{"created database " + command.name, std::nullopt};
}

Result Interpreter::run_harness(const exdeus::language::HarnessDb& command) {
    engine_.harness_database(command.name);
    return Result{"harnessed database " + command.name, std::nullopt};
}

Result Interpreter::run_forge(const exdeus::language::ForgeTable& command) {
    std::vector<Column> columns;
    for (const auto& def : command.columns) {
        Column column;
        column.name = def.name;
        column.type = def.type;
        column.required = def.required_flag;
        column.unique = def.unique;
        column.is_primary = def.is_primary;
        for (const auto& check : def.checks) {
            CheckOp mapped = CheckOp::Equals;
            switch (check.op) {
                case exdeus::language::FilterOp::Equals:
                    mapped = CheckOp::Equals;
                    break;
                case exdeus::language::FilterOp::Above:
                    mapped = CheckOp::Above;
                    break;
                case exdeus::language::FilterOp::Below:
                    mapped = CheckOp::Below;
                    break;
            }
            column.checks.push_back(CheckConstraint{mapped, check.literal});
        }
        if (def.references) {
            column.references =
                ForeignRef{def.references->table, def.references->column};
        }
        columns.push_back(std::move(column));
    }
    engine_.create_table(command.table, Schema(std::move(columns)));
    return Result{"forged table " + command.table, std::nullopt};
}

Result Interpreter::run_add(const exdeus::language::AddRow& command) {
    size_t index = engine_.insert(command.table, command.values);
    return Result{"added row " + std::to_string(index) + " to " + command.table, std::nullopt};
}

Result Interpreter::run_seek(const exdeus::language::SeekRows& command) {
    return run_seek_depth(command, 0).result;
}

SeekOutcome Interpreter::run_seek_depth(const exdeus::language::SeekRows& command,
                                        int depth) {
    if (depth > 8) {
        throw InterpreterError("subqueries nested too deep (max 8)");
    }
    // Resolve the FROM source: a catalog table, or a parenthesized
    // subquery materialized in memory for this query only.
    const Table* source = nullptr;
    std::unique_ptr<Table> owned;
    std::string source_name;
    if (command.from_query) {
        SeekOutcome inner = run_seek_depth(*command.from_query, depth + 1);
        owned = std::make_unique<Table>(
            materialize_temp("(subquery)", *inner.result.table, inner.col_types));
        source = owned.get();
        source_name = "(subquery)";
    } else {
        source = &engine_.table(command.table);
        source_name = command.table;
    }
    return run_seek_against(command, *source, source_name);
}

SeekOutcome Interpreter::run_seek_against(const exdeus::language::SeekRows& command,
                                          const Table& source,
                                          const std::string& source_name) {
    const Table& left_table = source;
    const Schema& left_schema = left_table.schema();

    const Table* right_table = nullptr;
    const Schema* right_schema = nullptr;
    if (command.join) {
        right_table = &engine_.table(command.join->table);
        right_schema = &right_table->schema();
    }

    // Column map resolves bare and qualified names into composite-row indexes.
    std::map<std::string, size_t> columns =
        build_columns(source_name, left_schema,
                      command.join ? &command.join->table : nullptr, right_schema);

    // Validate projection and ordering columns before touching rows.
    std::vector<std::string> display_names;
    std::vector<size_t> display_indexes;
    if (command.show.empty()) {
        // `show *` or no show: every column, schema order.
        // Qualified when joined so `id` vs `id` stays unambiguous.
        if (right_schema) {
            for (const Column& c : left_schema.columns()) {
                std::string ref = source_name + "." + c.name;
                display_names.push_back(ref);
                display_indexes.push_back(resolve_column(columns, ref));
            }
            for (const Column& c : right_schema->columns()) {
                std::string ref = command.join->table + "." + c.name;
                display_names.push_back(ref);
                display_indexes.push_back(resolve_column(columns, ref));
            }
        } else {
            for (const Column& c : left_schema.columns()) {
                display_names.push_back(c.name);
                display_indexes.push_back(resolve_column(columns, c.name));
            }
        }
    } else {
        for (const std::string& ref : command.show) {
            display_names.push_back(ref);
            display_indexes.push_back(resolve_column(columns, ref));
        }
    }
    size_t order_key = 0;
    bool has_order = !command.order_by.empty() &&
                     command.direction != exdeus::language::SortDirection::None;
    if (has_order) {
        order_key = resolve_column(columns, command.order_by);
    }

    // Generate candidates: single rows, or composite (left x right) rows.
    std::vector<Row> candidates;
    if (!right_table) {
        for (size_t i = 0; i < left_table.row_count(); ++i) {
            candidates.push_back(left_table.row_at(i));
        }
    } else {
        for (size_t l = 0; l < left_table.row_count(); ++l) {
            for (size_t r = 0; r < right_table->row_count(); ++r) {
                Row composite = left_table.row_at(l);
                composite.insert(composite.end(), right_table->row_at(r).begin(),
                                 right_table->row_at(r).end());
                candidates.push_back(std::move(composite));
            }
        }
    }

    // Filter: the join key is just another condition, evaluated first.
    std::vector<Row> matched;
    for (Row& row : candidates) {
        if (command.join) {
            size_t l_idx = resolve_column(columns, command.join->left);
            size_t r_idx = resolve_column(columns, command.join->right);
            if (!values_equal(row[l_idx], row[r_idx])) {
                continue;
            }
        }
        if (!row_matches(row, columns, command.where)) {
            continue;
        }
        matched.push_back(std::move(row));
    }

    if (has_order) {
        bool ascending = command.direction == exdeus::language::SortDirection::Ascending;
        std::stable_sort(matched.begin(), matched.end(), [&](const Row& a, const Row& b) {
            const Value& left = a[order_key];
            const Value& right = b[order_key];
            if (is_number(left) && is_number(right)) {
                return ascending ? as_double(left) < as_double(right)
                                 : as_double(left) > as_double(right);
            }
            if (left.type() != right.type()) {
                throw InterpreterError("cannot order mixed-type column: " + command.order_by);
            }
            return ascending ? left.to_string() < right.to_string()
                             : left.to_string() > right.to_string();
        });
    }

    ResultTable answer;
    answer.columns = display_names;
    for (Row& row : matched) {
        std::vector<Value> projected;
        for (size_t idx : display_indexes) {
            projected.push_back(std::move(row[idx]));
        }
        answer.rows.push_back(std::move(projected));
    }

    // Display column types come from the source schemas, so `into` tables
    // and outer subqueries keep exact types even on zero rows.
    std::vector<ValueType> col_types;
    size_t left_count = left_schema.column_count();
    for (size_t idx : display_indexes) {
        if (idx < left_count) {
            col_types.push_back(left_schema.column_at(idx).type);
        } else {
            col_types.push_back(right_schema->column_at(idx - left_count).type);
        }
    }

    std::string label = source_name;
    if (command.join) {
        label += " join " + command.join->table;
    }
    if (command.into_table) {
        Table stashed = materialize_temp(*command.into_table, answer, col_types);
        engine_.create_temp_table(*command.into_table, stashed.schema());
        for (const std::vector<Value>& vals : answer.rows) {
            engine_.insert(*command.into_table, vals);
        }
        label += " into " + *command.into_table;
    }
    Result result{"seek " + label + ": " + plural(answer.rows.size(), "row"),
                  std::move(answer)};
    return SeekOutcome{std::move(result), std::move(col_types)};
}

Result Interpreter::run_change(const exdeus::language::ChangeRows& command) {
    const Table& table = engine_.table(command.table);
    const Schema& schema = table.schema();
    std::map<std::string, size_t> columns =
        build_columns(command.table, schema, nullptr, nullptr);

    // Validate every named column up front so a bad `set` changes nothing.
    for (const Assignment& item : command.set) {
        resolve_column(columns, item.column);
        if (item.is_arithmetic) {
            resolve_column(columns, item.source_column);
        }
    }
    for (const Condition& condition : command.where) {
        resolve_column(columns, condition.column);
        if (condition.right_is_column) {
            resolve_column(columns, condition.right_column);
        }
    }

    std::vector<size_t> hits;
    for (size_t i = 0; i < table.row_count(); ++i) {
        if (row_matches(table.row_at(i), columns, command.where)) {
            hits.push_back(i);
        }
    }

    for (size_t index : hits) {
        Row rewritten = engine_.row_at(command.table, index);
        for (const Assignment& item : command.set) {
            size_t target = resolve_column(columns, item.column);
            if (!item.is_arithmetic) {
                rewritten[target] = item.value;
                continue;
            }
            const Value& source = rewritten[resolve_column(columns, item.source_column)];
            if (!is_number(source) || !is_number(item.value)) {
                throw InterpreterError("plus/minus needs numeric values for column: " +
                                       item.column);
            }
            bool keep_integer = source.type() == ValueType::Integer &&
                                item.value.type() == ValueType::Integer;
            double outcome = item.is_plus ? as_double(source) + as_double(item.value)
                                          : as_double(source) - as_double(item.value);
            rewritten[target] = keep_integer ? Value::integer(static_cast<long long>(outcome))
                                             : Value::decimal(outcome);
        }
        engine_.update(command.table, index, std::move(rewritten));
    }
    return Result{"changed " + plural(hits.size(), "row") + " in " + command.table, std::nullopt};
}

Result Interpreter::run_remove(const exdeus::language::RemoveRows& command) {
    const Table& table = engine_.table(command.table);
    const Schema& schema = table.schema();
    std::map<std::string, size_t> columns =
        build_columns(command.table, schema, nullptr, nullptr);
    for (const Condition& condition : command.where) {
        resolve_column(columns, condition.column);
        if (condition.right_is_column) {
            resolve_column(columns, condition.right_column);
        }
    }

    std::vector<size_t> hits;
    for (size_t i = 0; i < table.row_count(); ++i) {
        if (row_matches(table.row_at(i), columns, command.where)) {
            hits.push_back(i);
        }
    }
    // Delete from the back so earlier indexes stay valid.
    for (auto it = hits.rbegin(); it != hits.rend(); ++it) {
        engine_.remove(command.table, *it);
    }
    return Result{"removed " + plural(hits.size(), "row") + " from " + command.table, std::nullopt};
}

Result Interpreter::run_save(const exdeus::language::SaveDb& command) {
    // Save the harnessed database: `save db` without a harness has no
    // database to name, so it fails instead of guessing one.
    if (!engine_.has_selection()) {
        throw InterpreterError("no database selected: harness a database first");
    }
    try {
        std::string disk_path = normalize_separators(command.path);
        exdeus::io::SnapshotStore::save(engine_, engine_.selected_database(), disk_path);
        return Result{"saved database " + engine_.selected_database() + " to " +
                          display_path(disk_path),
                      std::nullopt};
    } catch (const std::exception& e) {
        throw InterpreterError(e.what());
    }
}

Result Interpreter::run_load(const exdeus::language::LoadDb& command) {
    try {
        std::string disk_path = normalize_separators(command.path);
        std::string name = exdeus::io::SnapshotStore::load(engine_, disk_path);
        return Result{"loaded database " + name + " from " + display_path(disk_path),
                      std::nullopt};
    } catch (const std::exception& e) {
        throw InterpreterError(e.what());
    }
}

Result Interpreter::run_export(const exdeus::language::ExportTable& command) {
    try {
        std::string disk_path = normalize_separators(command.path);
        exdeus::io::CsvExporter::export_table(engine_, command.table, disk_path);
        return Result{"exported table " + command.table + " to " + display_path(disk_path),
                      std::nullopt};
    } catch (const std::exception& e) {
        throw InterpreterError(e.what());
    }
}

}  // namespace exdeus::repl
