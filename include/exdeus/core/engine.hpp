#pragma once

#include <string>
#include <vector>

#include "exdeus/core/catalog.hpp"
#include "exdeus/core/schema.hpp"
#include "exdeus/core/table.hpp"
#include "exdeus/core/value.hpp"

namespace exdeus::core {

// Named failure for engine-facade operations (missing selection, bad names).
class EngineError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Engine is the week-2-stable command execution boundary: the parser and
// the future interpreter call this facade, never Catalog or Table directly.
// Table operations always run against the harnessed (selected) database.
class Engine {
public:
    Engine() = default;

    // Databases.
    void create_database(const std::string& name);
    void harness_database(const std::string& name);
    void drop_database(const std::string& name);
    void rename_database(const std::string& old_name, const std::string& new_name);
    std::vector<std::string> list_databases() const;
    std::string selected_database() const;
    bool has_selection() const;

    // Tables in the selected database.
    void create_table(const std::string& table, Schema schema);
    void drop_table(const std::string& table);
    void rename_table(const std::string& old_name, const std::string& new_name);
    std::vector<std::string> list_tables() const;
    const Table& table(const std::string& table) const;
    Schema table_schema(const std::string& table) const;

    // Rows in the selected database.
    size_t insert(const std::string& table, Row values);
    void update(const std::string& table, size_t index, Row values);
    void remove(const std::string& table, size_t index);
    std::vector<size_t> select(const std::string& table, const std::string& column, const Value& value) const;
    Row row_at(const std::string& table, size_t index) const;
    size_t row_count(const std::string& table) const;
    void add_column(const std::string& table, Column column, Value default_value);
    void remove_column(const std::string& table, const std::string& column);

private:
    const std::string& require_selection() const;

    Catalog catalog_;
};

}  // namespace exdeus::core
