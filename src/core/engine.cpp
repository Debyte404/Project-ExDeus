// Lesson 4 implementation: safe facade over the catalog.
#include "exdeus/core/engine.hpp"

#include <utility>
#include <variant>

namespace exdeus::core {

namespace {

// Numeric promotion for FK matching: int 1 matches decimal 1.0.
// Null never matches (nullable FKs skip instead of comparing).
bool fk_values_equal(const Value& left, const Value& right) {
    if (left.type() == ValueType::Null || right.type() == ValueType::Null) {
        return false;
    }
    bool left_num =
        left.type() == ValueType::Integer || left.type() == ValueType::Decimal;
    bool right_num =
        right.type() == ValueType::Integer || right.type() == ValueType::Decimal;
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

// Enforce outgoing FKs on a candidate child row: every non-null FK value
// must exist in the parent table. Parent/column absence is also an error
// (a dangling references clause), not a silent pass.
void check_outgoing_fks(Catalog& catalog, const std::string& db,
                        const std::string& child_table, const Row& row) {
    const Table& child = catalog.table(db, child_table);
    const Schema& schema = child.schema();
    for (size_t i = 0; i < schema.column_count(); ++i) {
        const Column& column = schema.column_at(i);
        if (!column.references) {
            continue;
        }
        const Value& value = row[i];
        if (value.type() == ValueType::Null && !column.required) {
            continue;  // nullable FK: null means "no parent", always allowed
        }
        const std::string& parent_table = column.references->table;
        const std::string& parent_column = column.references->column;
        if (!catalog.has_table(db, parent_table)) {
            throw EngineError("foreign key violation: " + child_table + "." + column.name +
                              " references missing table " + parent_table);
        }
        const Table& parent = catalog.table(db, parent_table);
        size_t parent_idx;
        try {
            parent_idx = parent.schema().index_of(parent_column);
        } catch (const TableError&) {
            throw EngineError("foreign key violation: " + child_table + "." + column.name +
                              " references missing column " + parent_table + "." +
                              parent_column);
        }
        bool found = false;
        for (size_t r = 0; r < parent.row_count(); ++r) {
            if (fk_values_equal(parent.row_at(r)[parent_idx], value)) {
                found = true;
                break;
            }
        }
        if (!found) {
            throw EngineError("foreign key violation: " + child_table + "." + column.name +
                              " value " + value.to_string() + " not found in " +
                              parent_table + "." + parent_column);
        }
    }
}

// Enforce incoming FKs when a parent key changes or disappears: no child
// row may be left pointing at a value that no longer exists.
void check_incoming_fks(Catalog& catalog, const std::string& db,
                        const std::string& parent_table, const std::string& parent_column,
                        const Value& old_value) {
    if (old_value.type() == ValueType::Null) {
        return;  // nulls are never referenced (children skip them)
    }
    for (const std::string& child_name : catalog.list_tables(db)) {
        const Table& child = catalog.table(db, child_name);
        const Schema& schema = child.schema();
        for (size_t i = 0; i < schema.column_count(); ++i) {
            const Column& column = schema.column_at(i);
            if (!column.references) {
                continue;
            }
            if (column.references->table != parent_table ||
                column.references->column != parent_column) {
                continue;
            }
            for (size_t r = 0; r < child.row_count(); ++r) {
                if (fk_values_equal(child.row_at(r)[i], old_value)) {
                    throw EngineError("foreign key violation: " + child_name + "." +
                                      column.name + " still references " + parent_table +
                                      "." + parent_column + " value " + old_value.to_string());
                }
            }
        }
    }
}

}  // namespace

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
        // Restrict: a referenced parent cannot be dropped while children point at it.
        for (const std::string& child_name : catalog_.list_tables(db)) {
            if (child_name == table) {
                continue;
            }
            const Table& child = catalog_.table(db, child_name);
            for (const Column& column : child.schema().columns()) {
                if (column.references && column.references->table == table) {
                    throw EngineError("foreign key violation: cannot drop table " + table +
                                      " while " + child_name + "." + column.name +
                                      " references it");
                }
            }
        }
        catalog_.drop_table(db, table);
    } catch (const CatalogError& e) {
        throw EngineError(e.what());
    }
}

void Engine::rename_table(const std::string& old_name, const std::string& new_name) {
    const std::string& db = require_selection();
    try {
        // Renaming a referenced parent would dangle children's references.
        for (const std::string& child_name : catalog_.list_tables(db)) {
            if (child_name == old_name) {
                continue;
            }
            const Table& child = catalog_.table(db, child_name);
            for (const Column& column : child.schema().columns()) {
                if (column.references && column.references->table == old_name) {
                    throw EngineError("foreign key violation: cannot rename table " + old_name +
                                      " while " + child_name + "." + column.name +
                                      " references it");
                }
            }
        }
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
        check_outgoing_fks(catalog_, db, table, values);
        return catalog_.mutable_table(db, table).insert(std::move(values));
    } catch (const CatalogError& e) {
        throw EngineError(e.what());
    }
}

void Engine::update(const std::string& table, size_t index, Row values) {
    const std::string& db = require_selection();
    try {
        // Child side first: the new row must still point at live parents.
        check_outgoing_fks(catalog_, db, table, values);
        // Parent side: changing a referenced key must not orphan children.
        const Table& current = catalog_.table(db, table);
        Row old_row = current.row_at(index);  // bounds-check before FK work
        const Schema& schema = current.schema();
        for (size_t i = 0; i < schema.column_count() && i < values.size() &&
                            i < old_row.size();
             ++i) {
            if (!fk_values_equal(old_row[i], values[i])) {
                check_incoming_fks(catalog_, db, table, schema.column_at(i).name, old_row[i]);
            }
        }
        catalog_.mutable_table(db, table).update(index, std::move(values));
    } catch (const CatalogError& e) {
        throw EngineError(e.what());
    }
}

void Engine::remove(const std::string& table, size_t index) {
    const std::string& db = require_selection();
    try {
        // Restrict: a referenced parent row cannot disappear while children point at it.
        const Table& current = catalog_.table(db, table);
        Row old_row = current.row_at(index);
        const Schema& schema = current.schema();
        for (size_t i = 0; i < schema.column_count() && i < old_row.size(); ++i) {
            check_incoming_fks(catalog_, db, table, schema.column_at(i).name, old_row[i]);
        }
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
        // A new FK column's default must already satisfy the constraint,
        // since every existing row receives it.
        if (column.references) {
            if (default_value.type() == ValueType::Null && !column.required) {
                // nullable default: allowed without a parent lookup
            } else {
                Row probe;
                probe.push_back(std::move(default_value));
                // Build a probe using current schema width is unnecessary:
                // check the single default against the parent directly.
                const std::string& parent_table = column.references->table;
                const std::string& parent_column = column.references->column;
                if (!catalog_.has_table(db, parent_table)) {
                    throw EngineError("foreign key violation: " + table + "." + column.name +
                                      " references missing table " + parent_table);
                }
                const Table& parent = catalog_.table(db, parent_table);
                size_t parent_idx;
                try {
                    parent_idx = parent.schema().index_of(parent_column);
                } catch (const TableError&) {
                    throw EngineError("foreign key violation: " + table + "." + column.name +
                                      " references missing column " + parent_table + "." +
                                      parent_column);
                }
                bool found = false;
                for (size_t r = 0; r < parent.row_count(); ++r) {
                    if (fk_values_equal(parent.row_at(r)[parent_idx], probe[0])) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    throw EngineError("foreign key violation: " + table + "." + column.name +
                                      " default value " + probe[0].to_string() + " not found in " +
                                      parent_table + "." + parent_column);
                }
                default_value = std::move(probe[0]);
            }
        }
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
