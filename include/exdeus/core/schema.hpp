#pragma once

#include <string>
#include <vector>

#include "exdeus/core/value.hpp"

namespace exdeus::core {

// Column metadata: one declared field of a table.
// `required` and `unique` are stored now so week-2 null support
// can attach without changing this struct. Week 1 has no null
// values, so every inserted row must still supply a value per column.
struct Column {
    std::string name;
    ValueType type;
    bool required = true;
    bool unique = false;
};

// Schema owns the ordered column list and the duplicate-name invariant.
// Table depends on Schema; Schema depends only on Value/ValueType.
class Schema {
public:
    Schema() = default;
    explicit Schema(std::vector<Column> columns);

    const std::vector<Column>& columns() const;
    size_t column_count() const;
    bool has_column(const std::string& name) const;
    size_t index_of(const std::string& name) const;
    const Column& column_at(size_t index) const;
    const Column& column(const std::string& name) const;
    void add_column(Column column);
    void remove_column(const std::string& name);

private:
    std::vector<Column> columns_;
};

}  // namespace exdeus::core
