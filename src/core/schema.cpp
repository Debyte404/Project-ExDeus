// Lesson 3 implementation: ordered column metadata with unique names.
#include "exdeus/core/schema.hpp"

#include <utility>

#include "exdeus/core/table.hpp"

namespace exdeus::core {

Schema::Schema(std::vector<Column> columns) : columns_(std::move(columns)) {
    for (size_t i = 0; i < columns_.size(); ++i) {
        for (size_t j = i + 1; j < columns_.size(); ++j) {
            if (columns_[i].name == columns_[j].name) {
                throw TableError("duplicate column name: " + columns_[i].name);
            }
        }
    }
}

const std::vector<Column>& Schema::columns() const {
    return columns_;
}

size_t Schema::column_count() const {
    return columns_.size();
}

bool Schema::has_column(const std::string& name) const {
    for (const Column& column : columns_) {
        if (column.name == name) {
            return true;
        }
    }
    return false;
}

size_t Schema::index_of(const std::string& name) const {
    for (size_t i = 0; i < columns_.size(); ++i) {
        if (columns_[i].name == name) {
            return i;
        }
    }
    throw TableError("unknown column: " + name);
}

const Column& Schema::column_at(size_t index) const {
    if (index >= columns_.size()) {
        throw TableError("column index out of range");
    }
    return columns_[index];
}

const Column& Schema::column(const std::string& name) const {
    return columns_[index_of(name)];
}

void Schema::add_column(Column column) {
    if (has_column(column.name)) {
        throw TableError("duplicate column name: " + column.name);
    }
    columns_.push_back(std::move(column));
}

void Schema::remove_column(const std::string& name) {
    columns_.erase(columns_.begin() + static_cast<long long>(index_of(name)));
}

}  // namespace exdeus::core
