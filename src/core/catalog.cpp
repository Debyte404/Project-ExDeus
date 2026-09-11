// Lesson 4 implementation: named databases owning tables, plus selection.
#include "exdeus/core/catalog.hpp"

#include <utility>

namespace exdeus::core {

void Catalog::require_name(const std::string& name, const char* what) {
    if (name.empty()) {
        throw CatalogError(std::string(what) + " name must not be empty");
    }
}

const Database& Catalog::find_db(const std::string& name) const {
    auto it = databases_.find(name);
    if (it == databases_.end()) {
        throw CatalogError("unknown database: " + name);
    }
    return it->second;
}

Database& Catalog::find_db(const std::string& name) {
    auto it = databases_.find(name);
    if (it == databases_.end()) {
        throw CatalogError("unknown database: " + name);
    }
    return it->second;
}

void Catalog::create_database(const std::string& name) {
    require_name(name, "database");
    if (has_database(name)) {
        throw CatalogError("duplicate database: " + name);
    }
    databases_.emplace(name, Database{name, {}});
}

bool Catalog::has_database(const std::string& name) const {
    return databases_.find(name) != databases_.end();
}

void Catalog::drop_database(const std::string& name) {
    auto it = databases_.find(name);
    if (it == databases_.end()) {
        throw CatalogError("unknown database: " + name);
    }
    databases_.erase(it);
    if (has_selection_ && selected_ == name) {
        clear_selection();
    }
}

void Catalog::rename_database(const std::string& old_name, const std::string& new_name) {
    require_name(new_name, "database");
    auto it = databases_.find(old_name);
    if (it == databases_.end()) {
        throw CatalogError("unknown database: " + old_name);
    }
    if (old_name != new_name && has_database(new_name)) {
        throw CatalogError("duplicate database: " + new_name);
    }
    if (old_name == new_name) {
        return;
    }
    Database db = std::move(it->second);
    databases_.erase(it);
    db.name = new_name;
    databases_.emplace(new_name, std::move(db));
    if (has_selection_ && selected_ == old_name) {
        selected_ = new_name;
    }
}

std::vector<std::string> Catalog::list_databases() const {
    std::vector<std::string> names;
    for (const auto& [name, db] : databases_) {
        (void)db;
        names.push_back(name);
    }
    return names;
}

void Catalog::harness_database(const std::string& name) {
    find_db(name);  // throws on unknown names before touching selection
    selected_ = name;
    has_selection_ = true;
}

bool Catalog::has_selection() const {
    return has_selection_;
}

const std::string& Catalog::selected_database() const {
    if (!has_selection_) {
        throw CatalogError("no database selected: harness a database first");
    }
    return selected_;
}

void Catalog::clear_selection() {
    selected_.clear();
    has_selection_ = false;
}

void Catalog::create_table(const std::string& db, const std::string& table, Schema schema) {
    require_name(table, "table");
    Database& database = find_db(db);
    if (database.tables.find(table) != database.tables.end()) {
        throw CatalogError("duplicate table: " + table);
    }
    database.tables.emplace(table, Table(table, std::move(schema)));
}

bool Catalog::has_table(const std::string& db, const std::string& table) const {
    const Database& database = find_db(db);
    return database.tables.find(table) != database.tables.end();
}

void Catalog::drop_table(const std::string& db, const std::string& table) {
    Database& database = find_db(db);
    auto it = database.tables.find(table);
    if (it == database.tables.end()) {
        throw CatalogError("unknown table: " + table);
    }
    database.tables.erase(it);
}

void Catalog::rename_table(const std::string& db, const std::string& old_name, const std::string& new_name) {
    require_name(new_name, "table");
    Database& database = find_db(db);
    auto it = database.tables.find(old_name);
    if (it == database.tables.end()) {
        throw CatalogError("unknown table: " + old_name);
    }
    if (old_name != new_name && database.tables.find(new_name) != database.tables.end()) {
        throw CatalogError("duplicate table: " + new_name);
    }
    if (old_name == new_name) {
        return;
    }
    Table moved = std::move(it->second);
    database.tables.erase(it);
    // Rebuild under the new name: Table has no rename of its own name
    // field, so rows are re-inserted into a fresh table carrying the schema.
    Table renamed(new_name, moved.schema());
    for (const Row& row : moved.rows()) {
        renamed.insert(row);
    }
    database.tables.emplace(new_name, std::move(renamed));
}

std::vector<std::string> Catalog::list_tables(const std::string& db) const {
    const Database& database = find_db(db);
    std::vector<std::string> names;
    for (const auto& [name, table] : database.tables) {
        (void)table;
        names.push_back(name);
    }
    return names;
}

const Table& Catalog::table(const std::string& db, const std::string& table) const {
    const Database& database = find_db(db);
    auto it = database.tables.find(table);
    if (it == database.tables.end()) {
        throw CatalogError("unknown table: " + table);
    }
    return it->second;
}

Table& Catalog::mutable_table(const std::string& db, const std::string& table) {
    Database& database = find_db(db);
    auto it = database.tables.find(table);
    if (it == database.tables.end()) {
        throw CatalogError("unknown table: " + table);
    }
    return it->second;
}

}  // namespace exdeus::core
