// Lesson 4 implementation: safe facade over the catalog.
#include "exdeus/core/engine.hpp"

#include <utility>

namespace exdeus::core {

const std::string& Engine::require_selection() const {
    if (!catalog_.has_selection()) {
        throw EngineError("no database selected: harness a database first");
    }
    return catalog_.selected_database();
}

void Engine::create_database(const std::string& name) {
    try {
        catalog_.create_database(name);
    } catch (const CatalogError& e) {
        throw EngineError(e.what());
    }
}

void Engine::harness_database(const std::string& name) {
    try {
        catalog_.harness_database(name);
    } catch (const CatalogError& e) {
        throw EngineError(e.what());
    }
}

void Engine::drop_database(const std::string& name) {
    try {
        catalog_.drop_database(name);
    } catch (const CatalogError& e) {
        throw EngineError(e.what());
    }
}

void Engine::rename_database(const std::string& old_name, const std::string& new_name) {
    try {
        catalog_.rename_database(old_name, new_name);
    } catch (const CatalogError& e) {
        throw EngineError(e.what());
    }
}

std::vector<std::string> Engine::list_databases() const {
    return catalog_.list_databases();
}

std::string Engine::selected_database() const {
    try {
        return catalog_.selected_database();
    } catch (const CatalogError& e) {
        throw EngineError(e.what());
    }
}

bool Engine::has_selection() const {
    return catalog_.has_selection();
}

void Engine::create_table(const std::string& table, Schema schema) {
    const std::string& db = require_selection();
    try {
        catalog_.create_table(db, table, std::move(schema));
    } catch (const CatalogError& e) {
        throw EngineError(e.what());
    }
}

void Engine::drop_table(const std::string& table) {
    const std::string& db = require_selection();
    try {
        catalog_.drop_table(db, table);
    } catch (const CatalogError& e) {
        throw EngineError(e.what());
    }
}

void Engine::rename_table(const std::string& old_name, const std::string& new_name) {
    const std::string& db = require_selection();
    try {
        catalog_.rename_table(db, old_name, new_name);
    } catch (const CatalogError& e) {
        throw EngineError(e.what());
    }
}

std::vector<std::string> Engine::list_tables() const {
    const std::string& db = require_selection();
    try {
        return catalog_.list_tables(db);
    } catch (const CatalogError& e) {
        throw EngineError(e.what());
    }
}

const Table& Engine::table(const std::string& table) const {
    const std::string& db = require_selection();
    try {
        return catalog_.table(db, table);
    } catch (const CatalogError& e) {
        throw EngineError(e.what());
    }
}

Schema Engine::table_schema(const std::string& table) const {
    return this->table(table).schema();
}

size_t Engine::insert(const std::string& table, Row values) {
    const std::string& db = require_selection();
    try {
        return catalog_.mutable_table(db, table).insert(std::move(values));
    } catch (const CatalogError& e) {
        throw EngineError(e.what());
    }
}

void Engine::update(const std::string& table, size_t index, Row values) {
    const std::string& db = require_selection();
    try {
        catalog_.mutable_table(db, table).update(index, std::move(values));
    } catch (const CatalogError& e) {
        throw EngineError(e.what());
    }
}

void Engine::remove(const std::string& table, size_t index) {
    const std::string& db = require_selection();
    try {
        catalog_.mutable_table(db, table).remove(index);
    } catch (const CatalogError& e) {
        throw EngineError(e.what());
    }
}

std::vector<size_t> Engine::select(
    const std::string& table, const std::string& column, const Value& value) const {
    return this->table(table).select(column, value);
}

Row Engine::row_at(const std::string& table, size_t index) const {
    return this->table(table).row_at(index);
}

size_t Engine::row_count(const std::string& table) const {
    return this->table(table).row_count();
}

void Engine::add_column(const std::string& table, Column column, Value default_value) {
    const std::string& db = require_selection();
    try {
        catalog_.mutable_table(db, table).add_column(std::move(column), std::move(default_value));
    } catch (const CatalogError& e) {
        throw EngineError(e.what());
    }
}

void Engine::remove_column(const std::string& table, const std::string& column) {
    const std::string& db = require_selection();
    try {
        catalog_.mutable_table(db, table).remove_column(column);
    } catch (const CatalogError& e) {
        throw EngineError(e.what());
    }
}

}  // namespace exdeus::core
