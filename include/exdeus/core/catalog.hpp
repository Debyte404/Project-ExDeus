#pragma once

#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "exdeus/core/table.hpp"

namespace exdeus::core {

// Named failure for catalog operations: unknown/duplicate databases and
// tables, empty names, and missing database selection.
class CatalogError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// One named database: owns its tables by name. Tables live in a
// std::map so list_tables() is deterministic (alphabetical).
struct Database {
    std::string name;
    std::map<std::string, Table> tables;
};

// Catalog owns every database and the current selection.
// create db never selects; harness_database() selects explicitly.
class Catalog {
public:
    Catalog() = default;

    void create_database(const std::string& name);
    bool has_database(const std::string& name) const;
    void drop_database(const std::string& name);
    void rename_database(const std::string& old_name, const std::string& new_name);
    std::vector<std::string> list_databases() const;

    // Canonical ExdeusQL selection: `harness bank;`
    void harness_database(const std::string& name);
    bool has_selection() const;
    const std::string& selected_database() const;
    void clear_selection();

    void create_table(const std::string& db, const std::string& table, Schema schema);
    bool has_table(const std::string& db, const std::string& table) const;
    void drop_table(const std::string& db, const std::string& table);
    void rename_table(const std::string& db, const std::string& old_name, const std::string& new_name);
    std::vector<std::string> list_tables(const std::string& db) const;
    const Table& table(const std::string& db, const std::string& table) const;
    Table& mutable_table(const std::string& db, const std::string& table);

private:
    const Database& find_db(const std::string& name) const;
    Database& find_db(const std::string& name);
    static void require_name(const std::string& name, const char* what);

    std::map<std::string, Database> databases_;
    std::string selected_;
    bool has_selection_ = false;
};

}  // namespace exdeus::core
