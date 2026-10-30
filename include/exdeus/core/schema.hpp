#pragma once

#include <optional>
#include <string>
#include <vector>

#include "exdeus/core/value.hpp"

namespace exdeus::core {

// Check operators for column CHECK constraints. Mirrors the ExdeusQL
// filter words: equals / above / below (and ==, >, < in the parser).
enum class CheckOp {
    Equals,
    Above,
    Below,
};

// One CHECK rule on a column: `balance above 0`.
// All checks on a column must pass (AND). Nulls skip checks.
struct CheckConstraint {
    CheckOp op;
    Value literal;
};

// Foreign key target: `references parent_table(parent_column)`.
struct ForeignRef {
    std::string table;
    std::string column;
};

// Column metadata: one declared field of a table.
// `required` = NOT NULL. `unique` = no duplicates (nulls exempt).
// `is_primary` = beelzebub: single per table, implies unique + required.
// `checks` = CHECK rules. `references` = optional foreign key.
struct Column {
    std::string name;
    ValueType type;
    bool required = true;
    bool unique = false;
    bool is_primary = false;
    std::vector<CheckConstraint> checks;
    std::optional<ForeignRef> references;
};

// Schema owns the ordered column list plus the duplicate-name and
// single-primary invariants. Table depends on Schema; Schema depends
// only on Value/ValueType.
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
