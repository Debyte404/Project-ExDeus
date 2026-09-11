// Lesson 3 implementation: validated in-memory rows aligned to a Schema.
#include "exdeus/core/table.hpp"

#include <utility>

namespace exdeus::core {

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
    if (default_value.type() != column.type) {
        throw TableError("default value type does not match column: " + column.name);
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
        if (values[i].type() != schema_.column_at(i).type) {
            throw TableError("value type does not match column: " + schema_.column_at(i).name);
        }
    }
    // Uniqueness is checked against every stored row except the row
    // being updated, so an update that keeps its own key still passes
    // while any other duplicate is rejected.
    for (size_t i = 0; i < schema_.column_count(); ++i) {
        if (!schema_.column_at(i).unique) {
            continue;
        }
        for (size_t r = 0; r < rows_.size(); ++r) {
            if (is_update && r == ignore_index) {
                continue;
            }
            if (rows_[r][i] == values[i]) {
                throw TableError("duplicate value in unique column: " + schema_.column_at(i).name);
            }
        }
    }
}

}  // namespace exdeus::core
