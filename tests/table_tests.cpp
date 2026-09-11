#include <cstdlib>
#include <iostream>
#include <string>

#include "exdeus/core/schema.hpp"
#include "exdeus/core/table.hpp"
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
    } catch (const exdeus::core::TableError&) {
        std::cout << "PASS: " << message << "\n";
        return;
    }
    std::cerr << "FAIL: " << message << " (no TableError thrown)\n";
    std::exit(1);
}

exdeus::core::Schema bank_schema() {
    using exdeus::core::Column;
    using exdeus::core::ValueType;
    return exdeus::core::Schema({
        Column{"id", ValueType::Integer, true, true},
        Column{"name", ValueType::Text, true, false},
        Column{"balance", ValueType::Decimal, true, false},
        Column{"active", ValueType::Boolean, true, false},
    });
}

exdeus::core::Row bank_row(long long id, const std::string& name, double balance, bool active) {
    return {
        exdeus::core::Value::integer(id),
        exdeus::core::Value::text(name),
        exdeus::core::Value::decimal(balance),
        exdeus::core::Value::boolean(active),
    };
}

}  // namespace

void run_table_tests() {
    using exdeus::core::Column;
    using exdeus::core::Schema;
    using exdeus::core::Table;
    using exdeus::core::Value;
    using exdeus::core::ValueType;

    // 1. Schema preserves column order and looks up by name.
    {
        Schema schema = bank_schema();
        check(schema.column_count() == 4, "schema keeps four columns");
        check(schema.column_at(0).name == "id", "schema preserves column order");
        check(schema.index_of("balance") == 2, "schema finds column index by name");
        check(schema.column("name").type == ValueType::Text, "schema exposes column type");
    }

    // 2. Schema rejects duplicate names on construct and on add.
    {
        check_throws(
            [] {
                Schema({
                    Column{"id", ValueType::Integer},
                    Column{"id", ValueType::Text},
                });
            },
            "duplicate column names are rejected");
        Schema schema = bank_schema();
        check_throws(
            [&] { schema.add_column(Column{"id", ValueType::Integer}); },
            "add_column rejects a duplicate name");
    }

    // 3. Valid insert stores the row and select finds it.
    {
        Table customers("customers", bank_schema());
        size_t index = customers.insert(bank_row(1, "Asha", 25000.0, true));
        check(index == 0, "insert returns the new row index");
        check(customers.row_count() == 1, "insert grows row count");
        check(customers.row_at(0)[1] == Value::text("Asha"), "inserted row keeps values");
        auto found = customers.select("name", Value::text("Asha"));
        check(found.size() == 1 && found[0] == 0, "select finds the inserted row");
    }

    // 4. Insert validates count and declared type; failures change nothing.
    {
        Table customers("customers", bank_schema());
        check_throws([&] { customers.insert({Value::integer(1)}); },
                     "short row is rejected");
        check_throws(
            [&] {
                customers.insert({
                    Value::integer(1),
                    Value::integer(2),
                    Value::decimal(10.0),
                    Value::boolean(true),
                });
            },
            "wrong column type is rejected");
        check(customers.row_count() == 0, "failed insert leaves the table empty");
    }

    // 5. Unique columns reject duplicates.
    {
        Table customers("customers", bank_schema());
        customers.insert(bank_row(1, "Asha", 25000.0, true));
        check_throws([&] { customers.insert(bank_row(1, "Ravi", 9000.0, true)); },
                     "duplicate unique id is rejected");
        check(customers.row_count() == 1, "duplicate insert changes nothing");
    }

    // 6. Update replaces the row; bad indexes and bad rows throw.
    {
        Table customers("customers", bank_schema());
        customers.insert(bank_row(1, "Asha", 25000.0, true));
        customers.update(0, bank_row(1, "Asha", 26000.0, false));
        check(customers.row_at(0)[2] == Value::decimal(26000.0), "update replaces values");
        check_throws([&] { customers.update(3, bank_row(2, "Ravi", 1.0, true)); },
                     "update with bad index throws");
        check_throws([&] { customers.update(0, {Value::integer(9)}); },
                     "update with bad row throws");
        check(customers.row_at(0)[2] == Value::decimal(26000.0),
              "failed update keeps the old row");
    }

    // 7. Delete removes the row and compacts the rest.
    {
        Table customers("customers", bank_schema());
        customers.insert(bank_row(1, "Asha", 25000.0, true));
        customers.insert(bank_row(2, "Ravi", 9000.0, true));
        customers.remove(0);
        check(customers.row_count() == 1, "delete shrinks row count");
        check(customers.row_at(0)[0] == Value::integer(2), "delete compacts remaining rows");
        check_throws([&] { customers.remove(5); }, "delete with bad index throws");
    }

    // 8. Add/remove column keeps rows aligned with the schema.
    {
        Table customers("customers", bank_schema());
        customers.insert(bank_row(1, "Asha", 25000.0, true));
        customers.add_column(Column{"city", ValueType::Text}, Value::text("Pune"));
        check(customers.schema().column_count() == 5, "add_column grows the schema");
        check(customers.row_at(0).size() == 5, "add_column extends stored rows");
        check(customers.row_at(0)[4] == Value::text("Pune"),
              "existing rows get the default value");
        customers.remove_column("city");
        check(customers.schema().column_count() == 4, "remove_column shrinks the schema");
        check(customers.row_at(0).size() == 4, "remove_column shrinks stored rows");
    }

    std::cout << "All table tests passed.\n";
}
