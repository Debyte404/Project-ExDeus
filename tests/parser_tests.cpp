#include <cstdlib>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

#include "exdeus/language/lexer.hpp"
#include "exdeus/language/parser.hpp"

namespace {

using exdeus::language::Command;
using exdeus::language::Lexer;
using exdeus::language::Parser;

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

std::vector<Command> parse_all(const std::string& source) {
    Lexer lexer(source);
    Parser parser(lexer.tokenize());
    return parser.parse_all();
}

template <typename T>
T as(Command command, const char* message) {
    if (!std::holds_alternative<T>(command)) {
        std::cerr << "FAIL: " << message << " (wrong command type)\n";
        std::exit(1);
    }
    std::cout << "PASS: " << message << "\n";
    return std::get<T>(command);
}

template <typename F>
void check_throws(F&& run, const char* message) {
    try {
        run();
    } catch (const exdeus::language::ParserError& e) {
        std::cout << "PASS: " << message << " [" << e.what() << "]\n";
        return;
    }
    std::cerr << "FAIL: " << message << " (no ParserError thrown)\n";
    std::exit(1);
}

}  // namespace

void run_parser_tests() {
    using namespace exdeus::language;
    using exdeus::core::Value;
    using exdeus::core::ValueType;

    // 1. create db parses without selecting; harness is a separate command.
    {
        const auto& create = as<CreateDb>(parse_one("create db bank;"), "create db parses");
        check(create.name == "bank", "create db keeps the name");
        const auto& alias = as<CreateDb>(parse_one("create database vault;"), "database aliases db");
        check(alias.name == "vault", "database alias keeps the name");
        const auto& harness = as<HarnessDb>(parse_one("harness bank;"), "harness parses separately");
        check(harness.name == "bank", "harness keeps the name");
    }

    // 2. forge table parses columns, types, and unique flags.
    {
        const auto& forge = as<ForgeTable>(
            parse_one("forge table customers with id: integer unique, name: text, balance: decimal, active: boolean;"),
            "forge table parses");
        check(forge.table == "customers", "forge keeps the table name");
        check(forge.columns.size() == 4, "forge keeps all four columns");
        check(forge.columns[0].name == "id", "first column keeps its name");
        check(forge.columns[0].type == ValueType::Integer, "id is integer");
        check(forge.columns[0].unique, "id is unique");
        check(!forge.columns[1].unique, "name is not unique");
        check(forge.columns[1].type == ValueType::Text, "name is text");
        check(forge.columns[2].type == ValueType::Decimal, "balance is decimal");
        check(forge.columns[3].type == ValueType::Boolean, "active is boolean");
    }

    // 3. add parses typed row values in order.
    {
        const auto& add = as<AddRow>(
            parse_one("add customers (1, \"Asha\", 25000.50, true);"), "add parses");
        check(add.table == "customers", "add keeps the table name");
        check(add.values.size() == 4, "add keeps all four values");
        check(add.values[0] == Value::integer(1), "first value is integer 1");
        check(add.values[1] == Value::text("Asha"), "second value is text Asha");
        check(add.values[2] == Value::decimal(25000.50), "third value is decimal");
        check(add.values[3] == Value::boolean(true), "fourth value is boolean");
    }

    // 4. seek parses projection, chained filters, and ordering.
    {
        const auto& seek = as<SeekRows>(
            parse_one("seek customers show name, balance where balance above 10000 and active equals true order by balance descending;"),
            "full seek parses");
        check(seek.table == "customers", "seek keeps the table name");
        check(seek.show.size() == 2 && seek.show[0] == "name" && seek.show[1] == "balance",
              "seek keeps the projection");
        check(seek.where.size() == 2, "seek keeps both conditions");
        check(seek.where[0].column == "balance", "first condition keeps its column");
        check(seek.where[0].op == FilterOp::Above, "above maps to Above");
        check(seek.where[0].literal == Value::integer(10000), "first literal is 10000");
        check(seek.where[0].and_with_next, "and chains the conditions");
        check(seek.where[1].op == FilterOp::Equals, "equals maps to Equals");
        check(seek.order_by == "balance", "order by keeps its column");
        check(seek.direction == SortDirection::Descending, "descending maps correctly");

        const auto& bare = as<SeekRows>(parse_one("seek customers;"), "bare seek parses");
        check(bare.show.empty() && bare.where.empty(), "bare seek has no projection or filter");
        check(bare.direction == SortDirection::None, "bare seek has no direction");

        const auto& ors = as<SeekRows>(
            parse_one("seek t where a below 5 or b == 6;"), "or + symbol ops parse");
        check(!ors.where[0].and_with_next, "or chains the conditions");
        check(ors.where[0].op == FilterOp::Below, "below maps to Below");
        check(ors.where[1].op == FilterOp::Equals, "== maps to Equals");
    }

    // 5. change parses literal and arithmetic assignments plus filters.
    {
        const auto& change = as<ChangeRows>(
            parse_one("change customers set balance to balance plus 500 where account_type equals \"savings\";"),
            "arithmetic change parses");
        check(change.table == "customers", "change keeps the table name");
        check(change.set.size() == 1, "change keeps one assignment");
        check(change.set[0].is_arithmetic, "plus form is arithmetic");
        check(change.set[0].source_column == "balance", "arithmetic reads its own column");
        check(change.set[0].is_plus, "plus maps correctly");
        check(change.set[0].value == Value::integer(500), "operand is 500");
        check(change.where.size() == 1, "change keeps its filter");

        const auto& literal = as<ChangeRows>(
            parse_one("change t set a to 1, b to \"x\";"), "multi-set parses");
        check(literal.set.size() == 2, "change keeps both assignments");
        check(!literal.set[0].is_arithmetic, "literal set is not arithmetic");
        check(literal.set[0].value == Value::integer(1), "first literal is 1");
        check(literal.where.empty(), "change without where hits all rows");
    }

    // 6. remove, save, load, export parse their targets.
    {
        const auto& filtered = as<RemoveRows>(
            parse_one("remove customers where id equals 1;"), "filtered remove parses");
        check(filtered.where.size() == 1, "remove keeps its filter");
        const auto& all = as<RemoveRows>(parse_one("remove customers;"), "bare remove parses");
        check(all.where.empty(), "bare remove hits all rows");

        const auto& save = as<SaveDb>(parse_one("save db to \"bank.exd\";"), "save parses");
        check(save.path == "bank.exd", "save keeps its path");
        const auto& load = as<LoadDb>(parse_one("load db from \"bank.exd\";"), "load parses");
        check(load.path == "bank.exd", "load keeps its path");
        const auto& export_cmd = as<ExportTable>(
            parse_one("export customers to \"customers.csv\";"), "export parses");
        check(export_cmd.table == "customers", "export keeps its table");
        check(export_cmd.path == "customers.csv", "export keeps its path");
    }

    // 7. Sequences parse in order; errors carry positions.
    {
        auto commands = parse_all("create db bank; harness bank; seek customers;");
        check(commands.size() == 3, "parse_all returns all three commands");
        check(std::holds_alternative<CreateDb>(commands[0]), "first command is CreateDb");
        check(std::holds_alternative<HarnessDb>(commands[1]), "second command is HarnessDb");
        check(std::holds_alternative<SeekRows>(commands[2]), "third command is SeekRows");

        check_throws([] { parse_one("create bank;"); }, "create without db fails");
        check_throws([] { parse_one("forge table t with id;"); }, "column without type fails");
        check_throws([] { parse_one("forge table t with id: money;"); }, "unknown type fails");
        check_throws([] { parse_one("add customers (1,);"); }, "trailing comma fails");
        check_throws([] { parse_one("seek customers where;"); }, "dangling where fails");
        check_throws([] { parse_one("change t set a 1;"); }, "set without to fails");
        check_throws([] { parse_one("save db bank;"); }, "save without to fails");
        check_throws([] { parse_one("fly away;"); }, "unknown command fails");
        check_throws([] { parse_one("create db bank"); }, "missing semicolon fails");
        check_throws([] { parse_one("seek customers; extra;"); }, "trailing command fails parse_one");

        bool positioned = false;
        try {
            parse_one("seek customers show ;");
        } catch (const ParserError& e) {
            positioned = std::string(e.what()).find("line 1, column") != std::string::npos;
        }
        check(positioned, "parser errors carry line and column");
    }

    std::cout << "All parser tests passed.\n";
}
