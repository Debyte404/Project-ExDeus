#include <cstdlib>
#include <iostream>
#include <string>

#include "exdeus/core/engine.hpp"
#include "exdeus/core/schema.hpp"
#include "exdeus/core/value.hpp"

namespace {

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        std::exit(1);
    }
    std::cout << "PASS: " << message << "\n";
}

template <typename F>
void check_throws(F&& run, const char* message) {
    try {
        run();
    } catch (const exdeus::core::EngineError&) {
        std::cout << "PASS: " << message << "\n";
        return;
    }
    std::cerr << "FAIL: " << message << " (no EngineError thrown)\n";
    std::exit(1);
}

exdeus::core::Schema bank_schema() {
    using exdeus::core::Column;
    using exdeus::core::ValueType;
    return exdeus::core::Schema({
        Column{"id", ValueType::Integer, true, true},
        Column{"name", ValueType::Text, true, false},
        Column{"balance", ValueType::Decimal, true, false},
    });
}

}  // namespace

void run_engine_tests() {
    using exdeus::core::Engine;
    using exdeus::core::Value;

    // 1. create db does not select; table ops need a harness first.
    {
        Engine engine;
        engine.create_database("bank");
        check(!engine.has_selection(), "create db leaves nothing selected");
        check_throws([&] { engine.create_table("customers", bank_schema()); },
                     "table op without harness throws");
        check_throws([&] { engine.harness_database("missing"); },
                     "harness of unknown db throws");
        engine.harness_database("bank");
        check(engine.has_selection(), "harness selects the database");
        check(engine.selected_database() == "bank", "selected db name is reported");
    }

    // 2. Duplicate and unknown database names are rejected.
    {
        Engine engine;
        engine.create_database("bank");
        check_throws([&] { engine.create_database("bank"); },
                     "duplicate db is rejected");
        check_throws([&] { engine.drop_database("missing"); },
                     "drop of unknown db throws");
        check_throws([&] { engine.rename_database("missing", "other"); },
                     "rename of unknown db throws");
        engine.create_database("other");
        check_throws([&] { engine.rename_database("bank", "other"); },
                     "rename onto existing db throws");
        auto dbs = engine.list_databases();
        check(dbs.size() == 2 && dbs[0] == "bank" && dbs[1] == "other",
              "list_databases reports both names in order");
    }

    // 3. Rename follows the selection; drop clears it.
    {
        Engine engine;
        engine.create_database("bank");
        engine.harness_database("bank");
        engine.rename_database("bank", "vault");
        check(engine.selected_database() == "vault", "rename follows the selection");
        engine.drop_database("vault");
        check(!engine.has_selection(), "drop of selected db clears selection");
        check_throws([&] { engine.list_tables(); },
                     "table op after drop throws without selection");
    }

    // 4. Full table lifecycle through the harness.
    {
        Engine engine;
        engine.create_database("bank");
        engine.harness_database("bank");
        engine.create_table("customers", bank_schema());
        check_throws([&] { engine.create_table("customers", bank_schema()); },
                     "duplicate table is rejected");
        check_throws([&] { engine.table("missing"); },
                     "unknown table lookup throws");
        size_t index = engine.insert("customers", {
            Value::integer(1), Value::text("Asha"), Value::decimal(25000.0)});
        check(index == 0, "engine insert returns the row index");
        check(engine.row_count("customers") == 1, "engine tracks row count");
        auto found = engine.select("customers", "name", Value::text("Asha"));
        check(found.size() == 1 && found[0] == 0, "engine select finds the row");
        engine.update("customers", 0, {
            Value::integer(1), Value::text("Asha"), Value::decimal(26000.0)});
        check(engine.row_at("customers", 0)[2] == Value::decimal(26000.0),
              "engine update replaces values");
        engine.rename_table("customers", "clients");
        check(engine.list_tables().size() == 1 && engine.list_tables()[0] == "clients",
              "rename_table swaps the name");
        check(engine.table("clients").name() == "clients",
              "renamed table reports the new name");
        check(engine.row_count("clients") == 1, "renamed table keeps its rows");
        engine.remove("clients", 0);
        check(engine.row_count("clients") == 0, "engine remove deletes the row");
        engine.drop_table("clients");
        check(engine.list_tables().empty(), "drop_table empties the listing");
        check_throws([&] { engine.drop_table("clients"); },
                     "second drop of the same table throws");
    }

    // 5. Column reshape through the engine keeps schema and rows aligned.
    {
        Engine engine;
        engine.create_database("bank");
        engine.harness_database("bank");
        engine.create_table("customers", bank_schema());
        engine.insert("customers", {
            Value::integer(1), Value::text("Asha"), Value::decimal(25000.0)});
        engine.add_column("customers", {"active", exdeus::core::ValueType::Boolean},
                          Value::boolean(true));
        check(engine.table_schema("customers").column_count() == 4,
              "add_column grows the schema through the engine");
        check(engine.row_at("customers", 0).size() == 4,
              "existing rows gain the default value");
        engine.remove_column("customers", "active");
        check(engine.table_schema("customers").column_count() == 3,
              "remove_column shrinks the schema again");
    }

    std::cout << "All engine tests passed.\n";
}
