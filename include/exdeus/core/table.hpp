#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include "exdeus/core/schema.hpp"
#include "exdeus/core/value.hpp"

namespace exdeus::core {

// Named validation failure. Thrown before any mutation so invalid
// commands never partially change the table.
class TableError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// A row is values aligned with the schema by position:
// values[i] belongs to schema().columns()[i].
using Row = std::vector<Value>;

// Table owns the schema plus ordered rows. Insert validates count,
// declared type, required presence, and uniqueness. Selection helpers
// return row indexes so update/delete act on stable positions.
class Table {
public:
    Table() = default;
    Table(std::string name, Schema schema);

    const std::string& name() const;
    const Schema& schema() const;
    size_t row_count() const;
    const Row& row_at(size_t index) const;
    const std::vector<Row>& rows() const;

    // Returns the new row index.
    size_t insert(Row values);
    void update(size_t index, Row values);
    void remove(size_t index);
    std::vector<size_t> select(const std::string& column_name, const Value& value) const;
    void add_column(Column column, Value default_value);
    void remove_column(const std::string& name);

private:
    void validate_row(const Row& values, bool is_update, size_t ignore_index) const;

    std::string name_;
    Schema schema_;
    std::vector<Row> rows_;
};

}  // namespace exdeus::core
