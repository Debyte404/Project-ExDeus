// Lesson 3 implementation: validated in-memory rows aligned to a Schema.
#include "exdeus/core/table.hpp"

#include <utility>
#include <variant>

namespace exdeus::core {

namespace {

// Numeric promotion for CHECK equals: int 25000 matches decimal 25000.0.
bool check_values_equal(const Value& left, const Value& right) {
    bool left_num = left.type() == ValueType::Integer || left.type() == ValueType::Decimal;
    bool right_num = right.type() == ValueType::Integer || right.type() == ValueType::Decimal;
    if (left_num && right_num) {
        auto as_double = [](const Value& v) -> double {
            if (v.type() == ValueType::Integer) {
                return static_cast<double>(std::get<long long>(v.data()));
            }
            return std::get<double>(v.data());
        };
        return as_double(left) == as_double(right);
    }
    return left == right;
}

bool check_value_above(const Value& cell, const Value& bound) {
    bool cell_num =
        cell.type() == ValueType::Integer || cell.type() == ValueType::Decimal;
    bool bound_num =
        bound.type() == ValueType::Integer || bound.type() == ValueType::Decimal;
    if (!cell_num || !bound_num) {
        throw TableError("check above/below needs numeric values");
    }
    auto as_double = [](const Value& v) -> double {
        if (v.type() == ValueType::Integer) {
            return static_cast<double>(std::get<long long>(v.data()));
        }
        return std::get<double>(v.data());
    };
    return as_double(cell) > as_double(bound);
}

}  // namespace

Table::Table(std::string name, Schema schema)
    : name_(std::move(name)), schema_(std::move(schema)) {}

const std::string& Table::name() const {
    return name_;
}

const Schema& Table::schema() const {
    return schema_;
}

size_t Table::row_count() const {
    return rows_.size();
}

const Row& Table::row_at(size_t index) const {
    if (index >= rows_.size()) {
        throw TableError("row index out of range");
    }
    return rows_[index];
}

const std::vector<Row>& Table::rows() const {
    return rows_;
}

size_t Table::insert(Row values) {
    validate_row(values, false, rows_.size());
    rows_.push_back(std::move(values));
    return rows_.size() - 1;
}

void Table::update(size_t index, Row values) {
    if (index >= rows_.size()) {
        throw TableError("row index out of range");
    }
    validate_row(values, true, index);
    rows_[index] = std::move(values);
}

void Table::remove(size_t index) {
    if (index >= rows_.size()) {
        throw TableError("row index out of range");
    }
    rows_.erase(rows_.begin() + static_cast<long long>(index));
}

std::vector<size_t> Table::select(const std::string& column_name, const Value& value) const {
    size_t column_index = schema_.index_of(column_name);
    std::vector<size_t> found;
    for (size_t i = 0; i < rows_.size(); ++i) {
        if (rows_[i][column_index] == value) {
            found.push_back(i);
        }
    }
    return found;
}

void Table::add_column(Column column, Value default_value) {
    bool default_is_null = default_value.type() == ValueType::Null;
    if (default_is_null) {
        if (column.required) {
            throw TableError("null default for not null column: " + column.name);
        }
    } else {
        if (default_value.type() != column.type) {
            throw TableError("default value type does not match column: " + column.name);
        }
        for (const CheckConstraint& check : column.checks) {
            bool ok = false;
            switch (check.op) {
                case CheckOp::Equals:
                    ok = check_values_equal(default_value, check.literal);
                    break;
                case CheckOp::Above:
                    ok = check_value_above(default_value, check.literal);
                    break;
                case CheckOp::Below:
                    ok = check_value_above(check.literal, default_value);
                    break;
            }
            if (!ok) {
                throw TableError("check violation on column: " + column.name);
            }
        }
    }
    schema_.add_column(column);
    for (Row& row : rows_) {
        row.push_back(default_value);
    }
}

void Table::remove_column(const std::string& name) {
    size_t column_index = schema_.index_of(name);
    schema_.remove_column(name);
    for (Row& row : rows_) {
        row.erase(row.begin() + static_cast<long long>(column_index));
    }
}

void Table::validate_row(const Row& values, bool is_update, size_t ignore_index) const {
    if (values.size() != schema_.column_count()) {
        throw TableError("row value count does not match schema");
    }
    for (size_t i = 0; i < values.size(); ++i) {
        const Column& column = schema_.column_at(i);
        bool is_null = values[i].type() == ValueType::Null;
        if (is_null) {
            // NOT NULL rejects nulls; nullable columns skip type and checks.
            if (column.required) {
                throw TableError("null value in not null column: " + column.name);
            }
            continue;
        }
        if (values[i].type() != column.type) {
            throw TableError("value type does not match column: " + column.name);
        }
        // CHECK rules: every rule on the column must pass (AND).
        for (const CheckConstraint& check : column.checks) {
            bool ok = false;
            switch (check.op) {
                case CheckOp::Equals:
                    ok = check_values_equal(values[i], check.literal);
                    break;
                case CheckOp::Above:
                    ok = check_value_above(values[i], check.literal);
                    break;
                case CheckOp::Below:
                    ok = check_value_above(check.literal, values[i]);
                    break;
            }
            if (!ok) {
                throw TableError("check violation on column: " + column.name);
            }
        }
    }
    // Uniqueness is checked against every stored row except the row
    // being updated, so an update that keeps its own key still passes
    // while any other duplicate is rejected. Nulls are exempt: two
    // nulls in a unique column do not conflict (SQL semantics).
    for (size_t i = 0; i < schema_.column_count(); ++i) {
        if (!schema_.column_at(i).unique) {
            continue;
        }
        if (values[i].type() == ValueType::Null) {
            continue;
        }
        for (size_t r = 0; r < rows_.size(); ++r) {
            if (is_update && r == ignore_index) {
                continue;
            }
            if (rows_[r][i].type() == ValueType::Null) {
                continue;
            }
            if (check_values_equal(rows_[r][i], values[i])) {
                throw TableError("duplicate value in unique column: " + schema_.column_at(i).name);
            }
        }
    }
}

}  // namespace exdeus::core
