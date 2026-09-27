#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "exdeus/language/lexer.hpp"
#include "exdeus/language/parser.hpp"
#include "exdeus/repl/interpreter.hpp"

namespace {

using exdeus::language::Command;
using exdeus::language::Lexer;
using exdeus::language::Parser;
using exdeus::repl::Interpreter;
using exdeus::repl::InterpreterError;
using exdeus::repl::Result;

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        std::exit(1);
    }
    std::cout << "PASS: " << message << "\n";
}

Command parse_one(const std::string& source) {
    Lexer lexer(source);
    Parser parser(lexer.tokenize());
    return parser.parse_one();
}

Result run(Interpreter& db, const std::string& source) {
    return db.execute(parse_one(source));
}

std::vector<Result> run_script(Interpreter& db, const std::string& source) {
    Lexer lexer(source);
    Parser parser(lexer.tokenize());
    return db.execute_all(parser.parse_all());
}

template <typename F>
void check_fails(F&& run_fn, const char* message) {
    try {
        run_fn();
    } catch (const std::exception& e) {
        std::cout << "PASS: " << message << " [" << e.what() << "]\n";
        return;
    }
    std::cerr << "FAIL: " << message << " (no error thrown)\n";
    std::exit(1);
}

template <typename F>
void check_not_ready(F&& run_fn, const char* message) {
    try {
        run_fn();
    } catch (const InterpreterError& e) {
        std::cout << "PASS: " << message << " [" << e.what() << "]\n";
        return;
    }
    std::cerr << "FAIL: " << message << " (no InterpreterError thrown)\n";
    std::exit(1);
}

void seed_bank(Interpreter& db) {
    run_script(db,
               "create db bank;"
               "harness bank;"
               "forge table customers with id: integer unique, name: text, balance: decimal;"
               "add customers (1, \"Asha\", 25000.50);"
               "add customers (2, \"Ravi\", 12000.00);"
               "add customers (3, \"Mira\", 8000.25);");
}

}  // namespace

void run_interpreter_tests() {
    using exdeus::core::Value;

    // 1. create/harness/forge/add flow through to a full-table seek.
    {
        Interpreter db;
        Result created = run(db, "create db bank;");
        check(created.message == "created database bank", "create reports the database name");
        check(!created.table.has_value(), "create carries no table");
        check(!db.engine().has_selection(), "create still does not select");
        Result harnessed = run(db, "harness bank;");
        check(harnessed.message == "harnessed database bank", "harness reports the database name");
        Result forged = run(db, "forge table customers with id: integer unique, name: text, balance: decimal;");
        check(forged.message == "forged table customers", "forge reports the table name");
        Result added = run(db, "add customers (1, \"Asha\", 25000.50);");
        check(added.message == "added row 0 to customers", "add reports the row index");
        run(db, "add customers (2, \"Ravi\", 12000.00);");
        Result found = run(db, "seek customers;");
        check(found.table.has_value(), "seek returns a table");
        check(found.table->columns.size() == 3, "seek returns all three columns");
        check(found.table->columns[0] == "id" && found.table->columns[1] == "name" &&
                  found.table->columns[2] == "balance",
              "seek keeps the schema column order");
        check(found.table->rows.size() == 2, "seek returns both rows");
        check(found.table->rows[0][1] == Value::text("Asha"), "first row keeps its name");
        check(found.table->rows[1][2] == Value::decimal(12000.00), "second row keeps its balance");
    }

    // 2. Projection, chained filters, and ordering shape the answer.
    {
        Interpreter db;
        seed_bank(db);
        Result shown = run(db, "seek customers show name;");
        check(shown.table->columns.size() == 1 && shown.table->columns[0] == "name",
              "show projects a single column");
        check(shown.table->rows.size() == 3 && shown.table->rows[0].size() == 1,
              "projected rows carry one value each");

        Result rich = run(db, "seek customers where balance above 10000 and name equals \"Ravi\";");
        check(rich.table->rows.size() == 1 && rich.table->rows[0][1] == Value::text("Ravi"),
              "and keeps only the row passing both filters");

        Result either =
            run(db, "seek customers where balance below 9000 or name equals \"Asha\";");
        check(either.table->rows.size() == 2, "or keeps rows passing either filter");

        Result ordered = run(db, "seek customers show name order by balance descending;");
        check(ordered.table->rows.size() == 3, "ordered seek keeps all rows");
        check(ordered.table->rows[0][0] == Value::text("Asha") &&
                  ordered.table->rows[2][0] == Value::text("Mira"),
              "descending puts the richest balance first");

        Result ascending = run(db, "seek customers show name order by balance ascending;");
        check(ascending.table->rows[0][0] == Value::text("Mira") &&
                  ascending.table->rows[2][0] == Value::text("Asha"),
              "ascending puts the poorest balance first");
    }

    // 3. Literal and arithmetic change rewrite exactly the matching rows.
    {
        Interpreter db;
        seed_bank(db);
        Result literal = run(db, "change customers set balance to 999.0 where name equals \"Ravi\";");
        check(literal.message == "changed 1 row in customers", "change reports the hit count");
        Result after = run(db, "seek customers show balance where name equals \"Ravi\";");
        check(after.table->rows[0][0] == Value::decimal(999.0), "literal set rewrites the balance");

        Result richer = run(db, "change customers set balance to balance plus 500 where id equals 1;");
        check(richer.message == "changed 1 row in customers", "arithmetic change reports its hit");
        Result grown = run(db, "seek customers show balance where id equals 1;");
        check(grown.table->rows[0][0] == Value::decimal(25500.50),
              "integer operand promotes onto the decimal balance");

        run(db, "change customers set balance to balance minus 0.50 where id equals 1;");
        Result shrunk = run(db, "seek customers show balance where id equals 1;");
        check(shrunk.table->rows[0][0] == Value::decimal(25500.00), "decimal minus shrinks the balance");

        Result untouched = run(db, "seek customers show balance where id equals 3;");
        check(untouched.table->rows[0][0] == Value::decimal(8000.25),
              "unmatched rows keep their values");
    }

    // 4. Integer arithmetic stays integer; filtered and bare remove work.
    {
        Interpreter db;
        run_script(db,
                   "create db arcade;"
                   "harness arcade;"
                   "forge table wallet with id: integer unique, coins: integer;"
                   "add wallet (1, 10);"
                   "add wallet (2, 20);");
        run(db, "change wallet set coins to coins plus 5 where id equals 1;");
        Result coins = run(db, "seek wallet show coins where id equals 1;");
        check(coins.table->rows[0][0] == Value::integer(15), "integer plus stays integer");

        Result removed = run(db, "remove wallet where id equals 2;");
        check(removed.message == "removed 1 row from wallet", "filtered remove reports its hit");
        check(db.engine().row_count("wallet") == 1, "filtered remove deletes one row");
        Result cleared = run(db, "remove wallet;");
        check(cleared.message == "removed 1 row from wallet", "bare remove reports the rest");
        check(db.engine().row_count("wallet") == 0, "bare remove clears the table");
    }

    // 5. execute_all runs a pasted script in order with one result per command.
    {
        Interpreter db;
        auto results = run_script(db,
                                  "create db bank; harness bank;"
                                  "forge table t with id: integer;"
                                  "add t (1); seek t;");
        check(results.size() == 5, "execute_all returns one result per command");
        check(results[4].table.has_value() && results[4].table->rows.size() == 1,
              "the trailing seek sees the earlier add");
    }

    // 6. Snapshots and export wait for Lesson 8; engine errors still surface.
    {
        Interpreter db;
        seed_bank(db);
        check_not_ready([&] { run(db, "save db to \"bank.exd\";"); }, "save waits for Lesson 8");
        check_not_ready([&] { run(db, "load db from \"bank.exd\";"); }, "load waits for Lesson 8");
        check_not_ready([&] { run(db, "export customers to \"customers.csv\";"); },
                        "export waits for Lesson 8");

        check_fails([&] { run(db, "seek missing;"); }, "seek of an unknown table fails");
        check_fails([&] { run(db, "seek customers where missing equals 1;"); },
                    "filter on an unknown column fails");
        check_fails([&] { run(db, "add customers (1, \"Duplicate\");"); },
                    "short rows fail against the schema");
        check_fails([&] { run(db, "add customers (1, \"Clone\", 5.0);"); },
                    "duplicate unique ids fail");

        Interpreter fresh;
        check_fails([&] { run(fresh, "forge table t with id: integer;"); },
                    "forge without a harness fails");
    }

    std::cout << "All interpreter tests passed.\n";
}
