#include <cstdlib>
#include <filesystem>
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

int command_file_counter = 0;

// Forward slashes keep Windows temp paths simple inside ExdeusQL string
// literals (no escape decoding to think about).
std::string temp_command_path(const std::string& stem, const std::string& ext) {
    std::filesystem::path dir = std::filesystem::temp_directory_path() / "exdeus_cmd";
    std::filesystem::create_directories(dir);
    std::string path = (dir / (stem + "_" + std::to_string(++command_file_counter) + ext)).string();
    for (char& c : path) {
        if (c == '\\') {
            c = '/';
        }
    }
    return path;
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

    // 6. Save/load/export run for real; engine errors still surface.
    {
        Interpreter db;
        seed_bank(db);
        auto snap = temp_command_path("bank", ".exd");
        Result saved = run(db, "save db to \"" + snap + "\";");
        check(saved.message == "saved database bank to " + snap, "save reports db and path");
        run(db, "remove customers where id equals 3;");
        check(db.engine().row_count("customers") == 2, "remove shrinks before reload");
        Result loaded = run(db, "load db from \"" + snap + "\";");
        check(loaded.message == "loaded database bank from " + snap, "load reports db and path");
        check(db.engine().row_count("customers") == 3, "load restores the removed row");

        auto csv = temp_command_path("customers", ".csv");
        Result exported = run(db, "export customers to \"" + csv + "\";");
        check(exported.message == "exported table customers to " + csv,
              "export reports table and path");
        check(std::filesystem::exists(csv), "export writes the csv file");

        check_fails([&] { run(db, "export missing to \"" + csv + "\";"); },
                    "export of an unknown table fails");
        check_fails([&] { run(db, "load db from \"no-such-file.exd\";"); },
                    "load of a missing file fails");

        Interpreter unharnessed;
        check_fails([&] { run(unharnessed, "save db to \"" + snap + "\";"); },
                    "save without a harness fails");
        check_fails([&] { run(unharnessed, "forge table t with id: integer;"); },
                    "forge without a harness fails");

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

    // 7. show * expands to all columns; qualified names work everywhere.
    {
        Interpreter db;
        seed_bank(db);
        Result star = run(db, "seek customers show *;");
        check(star.table->columns.size() == 3, "show * returns all three columns");
        check(star.table->columns[0] == "id" && star.table->columns[1] == "name" &&
                  star.table->columns[2] == "balance",
              "show * keeps schema order");

        Result qualified = run(db, "seek customers show customers.name where customers.id equals 1;");
        check(qualified.table->columns[0] == "customers.name",
              "qualified show keeps the prefix");
        check(qualified.table->rows.size() == 1 &&
                  qualified.table->rows[0][0] == Value::text("Asha"),
              "qualified filter resolves correctly");

        check_fails([&] { run(db, "seek customers show missing;"); },
                    "show on an unknown column fails");
    }

    // 8. inner join combines two tables on a qualified key.
    {
        Interpreter db;
        run_script(db,
                   "create db bank; harness bank;"
                   "forge table customers with id: integer unique, name: text, balance: decimal;"
                   "add customers (1, \"Asha\", 25000.50);"
                   "add customers (2, \"Ravi\", 12000.00);"
                   "add customers (3, \"Mira\", 8000.25);"
                   "forge table accounts with id: integer unique, customer_id: integer, number: text;"
                   "add accounts (101, 1, \"SAV-1001\");"
                   "add accounts (102, 2, \"CUR-2001\");"
                   "add accounts (103, 3, \"SAV-1003\");"
                   "add accounts (104, 1, \"SAV-1002\");");

        Result joined = run(db,
                            "seek customers inner join accounts on customers.id equals accounts.customer_id;");
        check(joined.table.has_value(), "join returns a table");
        check(joined.table->columns.size() == 6,
              "join show * returns all six columns (3 + 3)");
        check(joined.table->columns[0] == "customers.id" &&
                  joined.table->columns[3] == "accounts.id",
              "join columns are qualified");
        // Asha has two accounts (101, 104); Ravi one (102); Mira one (103).
        check(joined.table->rows.size() == 4, "join produces four matched rows");

        Result filtered = run(db,
                              "seek customers inner join accounts on customers.id equals accounts.customer_id "
                              "show customers.name, accounts.number where customers.name equals \"Asha\";");
        check(filtered.table->rows.size() == 2, "join + where keeps only Asha's accounts");
        check(filtered.table->rows[0][1] == Value::text("SAV-1001") &&
                  filtered.table->rows[1][1] == Value::text("SAV-1002"),
              "joined rows carry the right account numbers");

        Result ordered = run(db,
                             "seek customers inner join accounts on customers.id equals accounts.customer_id "
                             "show accounts.number order by accounts.number descending;");
        check(ordered.table->rows[0][0] == Value::text("SAV-1003"),
              "join orders by a right-table column");

        check_fails([&] { run(db, "seek customers inner join missing on customers.id equals missing.customer_id;"); },
                    "join to an unknown table fails");
        check_fails([&] { run(db, "seek customers inner join accounts on customers.nope equals accounts.customer_id;"); },
                    "join on an unknown column fails");
    }

    // 9. Column-to-column comparison works outside joins too.
    {
        Interpreter db;
        run_script(db,
                   "create db shop; harness shop;"
                   "forge table items with id: integer unique, price: decimal, limit: decimal;"
                   "add items (1, 100.0, 100.0);"
                   "add items (2, 200.0, 150.0);");
        Result eq = run(db, "seek items where price equals limit;");
        check(eq.table->rows.size() == 1 && eq.table->rows[0][0] == Value::integer(1),
              "column-to-column equals finds matching rows");
        check_fails([&] { run(db, "seek items where price above limit;"); },
                    "column-to-column with above fails");
    }

    // 10. beelzebub primary key: unique, required, single per table.
    {
        Interpreter db;
        run_script(db, "create db shop; harness shop;"
                       "forge table items with id: integer beelzebub, name: text not null;");
        run(db, "add items (1, \"Hammer\");");
        check_fails([&] { run(db, "add items (1, \"Nails\");"); },
                    "duplicate beelzebub rejected");
        check_fails([&] { run(db, "add items (null, \"Ghost\");"); },
                    "null beelzebub rejected");
        check_fails([&] {
            run(db, "forge table bad with a: integer beelzebub, b: integer beelzebub;");
        }, "second beelzebub in one table rejected");
        check_fails([&] { run(db, "add items (2, null);"); },
                    "null into not null name rejected");
        Result hit = run(db, "seek items where id equals 1;");
        check(hit.table->rows.size() == 1, "beelzebub row is retrievable");
    }

    // 11. Nullable columns accept nulls; unique lets nulls repeat.
    {
        Interpreter db;
        run_script(db, "create db shop; harness shop;"
                       "forge table members with id: integer beelzebub, nickname: text null, code: integer unique null;");
        run(db, "add members (1, null, null);");
        run(db, "add members (2, null, null);");
        check(db.engine().row_count("members") == 2, "two null nicknames coexist");
        Result found = run(db, "seek members where nickname equals null;");
        check(found.table->rows.size() == 2, "null equals null matches");
        check_fails([&] { run(db, "add members (null, \"x\", 5);"); },
                    "null primary rejected even when other nulls exist");
    }

    // 12. CHECK constraints reject bad values on insert and update.
    {
        Interpreter db;
        run_script(db, "create db bank; harness bank;"
                       "forge table accounts with id: integer beelzebub, balance: decimal not null check above 0 and below 1000000;");
        run(db, "add accounts (1, 500.0);");
        check_fails([&] { run(db, "add accounts (2, 0.0);"); },
                    "zero balance violates above 0");
        check_fails([&] { run(db, "add accounts (2, 2000000.0);"); },
                    "huge balance violates below 1000000");
        check_fails([&] { run(db, "change accounts set balance to 0.0 where id equals 1;"); },
                    "update to violating value rejected");
        Result kept = run(db, "seek accounts where id equals 1;");
        check(kept.table->rows[0][1] == Value::decimal(500.0),
              "failed update keeps the old balance");
    }

    // 13. FOREIGN KEY: insert needs a live parent; parent delete/update restricted.
    {
        Interpreter db;
        run_script(db, "create db bank; harness bank;"
                       "forge table customers with id: integer beelzebub, name: text not null;"
                       "forge table accounts with id: integer beelzebub, customer_id: integer not null references customers(id), number: text;"
                       "add customers (1, \"Asha\");"
                       "add customers (2, \"Ravi\");"
                       "add accounts (101, 1, \"SAV-1001\");");
        check_fails([&] { run(db, "add accounts (102, 99, \"ORPHAN\");"); },
                    "orphan child rejected");
        check_fails([&] { run(db, "remove customers where id equals 1;"); },
                    "parent delete restricted while referenced");
        run(db, "remove accounts where id equals 101;");
        run(db, "remove customers where id equals 1;");
        check(db.engine().row_count("customers") == 1, "parent deletes after child goes");
        run_script(db, "add customers (3, \"Mira\"); add accounts (103, 3, \"SAV-1003\");");
        check_fails([&] { run(db, "change customers set id to 30 where id equals 3;"); },
                    "parent key update restricted while referenced");
        check_fails([&] { run(db, "change accounts set customer_id to 99 where id equals 103;"); },
                    "child update to orphan rejected");
        check_fails([&] { db.engine().drop_table("customers"); },
                    "drop of referenced parent rejected");
    }

    // 14. Nullable FK: null means no parent, always allowed.
    {
        Interpreter db;
        run_script(db, "create db hr; harness hr;"
                       "forge table teams with id: integer beelzebub, name: text;"
                       "forge table staff with id: integer beelzebub, team_id: integer null references teams(id), name: text;"
                       "add teams (1, \"Core\");"
                       "add staff (10, null, \"Solo\");"
                       "add staff (11, 1, \"Joined\");");
        check(db.engine().row_count("staff") == 2, "null-FK row and live-FK row coexist");
        check_fails([&] { run(db, "add staff (12, 99, \"Lost\");"); },
                    "non-null dangling FK still rejected");
        check_fails([&] { run(db, "remove teams where id equals 1;"); },
                    "referenced parent restricted even with a null-FK sibling");
        run(db, "remove staff where id equals 11;");
        run(db, "remove teams where id equals 1;");
        check(db.engine().row_count("teams") == 0, "parent deletes after referencing child goes");
    }

    // 15. Relative paths resolve against the process cwd; messages echo the
    // absolute location; backslashes normalize to forward slashes.
    {
        Interpreter db;
        run_script(db, "create db rel; harness rel;"
                       "forge table t with id: integer beelzebub, name: text;"
                       "add t (1, \"Pwd\");");
        // Forward-slash relative save: subdir auto-created under the cwd.
        Result saved = run(db, "save db to \"exd_rel_check/r1.exd\";");
        std::string abs_dir =
            std::filesystem::absolute("exd_rel_check").generic_string();
        check(saved.message == "saved database rel to " + abs_dir + "/r1.exd",
              "save message echoes the absolute location");
        check(std::filesystem::exists("exd_rel_check/r1.exd"),
              "relative save lands under the cwd");

        // Backslash save: doubled in source, single after lexing, normalized on disk.
        Result saved_bs = run(db, "save db to \"exd_rel_check\\\\r2.exd\";");
        check(saved_bs.message == "saved database rel to " + abs_dir + "/r2.exd",
              "backslash save normalizes and echoes absolute");
        check(std::filesystem::exists("exd_rel_check/r2.exd"),
              "backslash save lands in the same subdir");

        // Load the database back from a relative path in this cwd.
        Interpreter fresh;
        Result loaded = run(fresh, "load db from \"exd_rel_check/r1.exd\";");
        check(loaded.message == "loaded database rel from " + abs_dir + "/r1.exd",
              "load message echoes the absolute location");
        check(fresh.engine().row_count("t") == 1, "relative load restores the row");

        // Export to a relative path too.
        Result exported = run(fresh, "export t to \"exd_rel_check/t.csv\";");
        check(exported.message == "exported table t to " + abs_dir + "/t.csv",
              "export message echoes the absolute location");
        check(std::filesystem::exists("exd_rel_check/t.csv"),
              "relative export lands under the cwd");

        std::error_code ec;
        std::filesystem::remove_all("exd_rel_check", ec);
        check(!ec, "relative test dir cleans up");
    }

    std::cout << "All interpreter tests passed.\n";
}
