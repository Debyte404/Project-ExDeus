// Lesson 7 implementation: executes parsed commands against the Engine.
#include "exdeus/repl/interpreter.hpp"

#include <algorithm>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace exdeus::repl {

namespace {

using exdeus::core::Column;
using exdeus::core::Engine;
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

// Left-associative and/or fold: conditions[i].and_with_next joins
// conditions[i] to conditions[i + 1]. Empty means no filter: match all.
bool row_matches(const Row& row, const Schema& schema, const std::vector<Condition>& where) {
    if (where.empty()) {
        return true;
    }
    bool result = eval_condition(row[schema.index_of(where[0].column)], where[0]);
    for (size_t i = 0; i + 1 < where.size(); ++i) {
        bool next = eval_condition(row[schema.index_of(where[i + 1].column)], where[i + 1]);
        result = where[i].and_with_next ? (result && next) : (result || next);
    }
    return result;
}

std::string plural(size_t count, const char* word) {
    return std::to_string(count) + (count == 1 ? " " : " ") + word + (count == 1 ? "" : "s");
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
                throw InterpreterError("save db waits for the Lesson-8 snapshot store");
            } else if constexpr (std::is_same_v<T, exdeus::language::LoadDb>) {
                throw InterpreterError("load db waits for the Lesson-8 snapshot store");
            } else if constexpr (std::is_same_v<T, exdeus::language::ExportTable>) {
                throw InterpreterError("export waits for the Lesson-8 CSV exporter");
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
        columns.push_back(Column{def.name, def.type, true, def.unique});
    }
    engine_.create_table(command.table, Schema(std::move(columns)));
    return Result{"forged table " + command.table, std::nullopt};
}

Result Interpreter::run_add(const exdeus::language::AddRow& command) {
    size_t index = engine_.insert(command.table, command.values);
    return Result{"added row " + std::to_string(index) + " to " + command.table, std::nullopt};
}

Result Interpreter::run_seek(const exdeus::language::SeekRows& command) {
    const Table& table = engine_.table(command.table);
    const Schema& schema = table.schema();

    // Projection: empty `show` means every column in schema order.
    // Unknown projection or ordering columns fail before touching rows.
    std::vector<std::string> shown = command.show;
    if (shown.empty()) {
        for (const Column& column : schema.columns()) {
            shown.push_back(column.name);
        }
    }
    for (const std::string& name : shown) {
        if (!schema.has_column(name)) {
            throw InterpreterError("unknown column: " + name);
        }
    }
    if (!command.order_by.empty() && !schema.has_column(command.order_by)) {
        throw InterpreterError("unknown column: " + command.order_by);
    }

    std::vector<size_t> hits;
    for (size_t i = 0; i < table.row_count(); ++i) {
        if (row_matches(table.row_at(i), schema, command.where)) {
            hits.push_back(i);
        }
    }

    if (!command.order_by.empty() &&
        command.direction != exdeus::language::SortDirection::None) {
        size_t key = schema.index_of(command.order_by);
        bool ascending = command.direction == exdeus::language::SortDirection::Ascending;
        std::stable_sort(hits.begin(), hits.end(), [&](size_t a, size_t b) {
            const Value& left = table.row_at(a)[key];
            const Value& right = table.row_at(b)[key];
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
    answer.columns = shown;
    for (size_t index : hits) {
        const Row& row = table.row_at(index);
        std::vector<Value> projected;
        for (const std::string& name : shown) {
            projected.push_back(row[schema.index_of(name)]);
        }
        answer.rows.push_back(std::move(projected));
    }
    return Result{"seek " + command.table + ": " + plural(hits.size(), "row"), std::move(answer)};
}

Result Interpreter::run_change(const exdeus::language::ChangeRows& command) {
    const Table& table = engine_.table(command.table);
    const Schema& schema = table.schema();

    // Validate every named column up front so a bad `set` changes nothing.
    for (const Assignment& item : command.set) {
        if (!schema.has_column(item.column)) {
            throw InterpreterError("unknown column: " + item.column);
        }
        if (item.is_arithmetic && !schema.has_column(item.source_column)) {
            throw InterpreterError("unknown column: " + item.source_column);
        }
    }
    for (const Condition& condition : command.where) {
        if (!schema.has_column(condition.column)) {
            throw InterpreterError("unknown column: " + condition.column);
        }
    }

    std::vector<size_t> hits;
    for (size_t i = 0; i < table.row_count(); ++i) {
        if (row_matches(table.row_at(i), schema, command.where)) {
            hits.push_back(i);
        }
    }

    for (size_t index : hits) {
        Row rewritten = engine_.row_at(command.table, index);
        for (const Assignment& item : command.set) {
            size_t target = schema.index_of(item.column);
            if (!item.is_arithmetic) {
                rewritten[target] = item.value;
                continue;
            }
            const Value& source = rewritten[schema.index_of(item.source_column)];
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
    for (const Condition& condition : command.where) {
        if (!schema.has_column(condition.column)) {
            throw InterpreterError("unknown column: " + condition.column);
        }
    }

    std::vector<size_t> hits;
    for (size_t i = 0; i < table.row_count(); ++i) {
        if (row_matches(table.row_at(i), schema, command.where)) {
            hits.push_back(i);
        }
    }
    // Delete from the back so earlier indexes stay valid.
    for (auto it = hits.rbegin(); it != hits.rend(); ++it) {
        engine_.remove(command.table, *it);
    }
    return Result{"removed " + plural(hits.size(), "row") + " from " + command.table, std::nullopt};
}

}  // namespace exdeus::repl
