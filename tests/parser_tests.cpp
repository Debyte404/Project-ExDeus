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

    // 2b. forge parses beelzebub, not null, null, check, and references.
    {
        const auto& pk = as<ForgeTable>(
            parse_one("forge table t with id: integer beelzebub;"), "beelzebub parses");
        check(pk.columns[0].is_primary, "beelzebub sets primary");
        check(pk.columns[0].unique, "beelzebub implies unique");
        check(pk.columns[0].required_flag, "beelzebub implies required");

        const auto& nn = as<ForgeTable>(
            parse_one("forge table t with name: text not null;"), "not null parses");
        check(nn.columns[0].required_flag, "not null stays required");

        const auto& nul = as<ForgeTable>(
            parse_one("forge table t with nickname: text null;"), "bare null parses");
        check(!nul.columns[0].required_flag, "bare null makes nullable");

        const auto& chk = as<ForgeTable>(
            parse_one("forge table t with balance: decimal check above 0 and below 1000000;"),
            "check with and parses");
        check(chk.columns[0].checks.size() == 2, "both checks are kept");
        check(chk.columns[0].checks[0].op == FilterOp::Above, "first check is above");
        check(chk.columns[0].checks[0].literal == Value::integer(0), "check bound is 0");
        check(chk.columns[0].checks[1].op == FilterOp::Below, "second check is below");

        const auto& fk = as<ForgeTable>(
            parse_one("forge table a with customer_id: integer references customers(id);"),
            "references parses");
        check(fk.columns[0].references.has_value(), "references is present");
        check(fk.columns[0].references->table == "customers", "references keeps parent table");
        check(fk.columns[0].references->column == "id", "references keeps parent column");

        const auto& combo = as<ForgeTable>(
            parse_one("forge table a with id: integer beelzebub, bal: decimal not null check above 0, cid: integer references customers(id);"),
            "combined constraints parse");
        check(combo.columns.size() == 3, "all three columns kept");
        check(combo.columns[1].required_flag && combo.columns[1].checks.size() == 1,
              "not null plus check coexist");

        const auto& two_pk = as<ForgeTable>(
            parse_one("forge table t with a: integer beelzebub, b: integer beelzebub;"),
            "two beelzebubs parse");
        check(two_pk.columns[0].is_primary && two_pk.columns[1].is_primary,
              "both columns carry primary (Schema rejects this at forge time)");
        check_throws([] { parse_one("forge table t with a: integer not null null;"); },
                     "not null plus null fails");
        check_throws([] { parse_one("forge table t with a: integer check;"); },
                     "dangling check fails");
        check_throws([] { parse_one("forge table t with a: integer references customers;"); },
                     "references without parens fails");
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

    // 8. show * parses to an empty projection (all columns).
    {
        const auto& star = as<SeekRows>(parse_one("seek customers show *;"), "show * parses");
        check(star.show.empty(), "show * stores an empty projection");

        const auto& bare = as<SeekRows>(parse_one("seek customers;"), "bare seek still works");
        check(bare.show.empty() && !bare.join.has_value(), "bare seek has no join either");
    }

    // 9. inner join ... on parses the JoinClause.
    {
        const auto& join = as<SeekRows>(
            parse_one("seek customers inner join accounts on customers.id equals accounts.customer_id;"),
            "inner join parses");
        check(join.join.has_value(), "join clause is present");
        check(join.join->table == "accounts", "join keeps the right table name");
        check(join.join->left == "customers.id", "join left side is qualified");
        check(join.join->right == "accounts.customer_id", "join right side is qualified");

        const auto& short_join = as<SeekRows>(
            parse_one("seek customers join accounts on customers.id equals accounts.customer_id;"),
            "join without inner parses");
        check(short_join.join.has_value(), "bare join also produces a clause");

        check_throws([] { parse_one("seek customers inner join accounts on customers.id above accounts.customer_id;"); },
                     "join on non-equals operator fails");
        check_throws([] { parse_one("seek customers inner join accounts on id equals customer_id;"); },
                     "join on bare column fails");
    }

    // 10. Qualified columns and column-to-column conditions parse everywhere.
    {
        const auto& seek = as<SeekRows>(
            parse_one("seek customers show customers.name, customers.balance where customers.id equals 1 order by customers.balance descending;"),
            "qualified columns parse in seek");
        check(seek.show.size() == 2 && seek.show[0] == "customers.name",
              "qualified show keeps the prefix");
        check(seek.where[0].column == "customers.id", "qualified where column keeps the prefix");
        check(seek.order_by == "customers.balance", "qualified order by keeps the prefix");

        const auto& col_cmp = as<SeekRows>(
            parse_one("seek accounts where balance equals limit;"),
            "column-to-column condition parses");
        check(col_cmp.where[0].right_is_column, "column-to-column sets the flag");
        check(col_cmp.where[0].right_column == "limit", "right column keeps its name");

        check_throws([] { parse_one("seek accounts where balance above limit;"); },
                     "column-to-column with above fails");
    }

    // 11. `into` parses at the end; subqueries parse in parens.
    {
        const auto& into = as<SeekRows>(
            parse_one("seek customers show name, balance into rich;"), "into parses");
        check(into.into_table.has_value() && *into.into_table == "rich",
              "into keeps the temp table name");
        check(into.show.size() == 2, "show survives alongside into");

        const auto& bare_into = as<SeekRows>(parse_one("seek customers into rich;"),
                                             "bare into parses");
        check(bare_into.show.empty() && bare_into.into_table.has_value(),
              "into works without show");

        const auto& sub = as<SeekRows>(
            parse_one("seek (seek customers show name) show name;"), "subquery parses");
        check(sub.from_query != nullptr, "subquery is present");
        check(sub.from_query->table == "customers", "inner keeps its table");
        check(sub.from_query->show.size() == 1, "inner keeps its projection");
        check(sub.table.empty(), "outer has no direct table");

        const auto& nested = as<SeekRows>(
            parse_one("seek (seek (seek t show a) show a) show a;"), "nested subquery parses");
        check(nested.from_query->from_query != nullptr, "two levels nest");

        check_throws([] { parse_one("seek customers into;"); }, "into without a name fails");
        check_throws([] { parse_one("seek (seek t show a;"); }, "unclosed paren fails");
        check_throws([] { parse_one("seek (customers);"); }, "parens need seek inside");
    }

    // 12. `sort` desugars to an ordered seek; bare `order by` means ascending.
    {
        const auto& sort = as<SeekRows>(parse_one("sort customers by balance;"), "sort parses");
        check(sort.table == "customers", "sort keeps the table");
        check(sort.order_by == "balance", "sort keeps the column");
        check(sort.direction == SortDirection::Ascending, "sort defaults to ascending");

        const auto& down = as<SeekRows>(
            parse_one("sort customers by balance desc;"), "sort desc parses");
        check(down.direction == SortDirection::Descending, "desc means descending");

        const auto& up = as<SeekRows>(
            parse_one("sort customers by balance asc;"), "sort asc parses");
        check(up.direction == SortDirection::Ascending, "asc means ascending");

        const auto& full = as<SeekRows>(
            parse_one("sort customers by customers.balance descending into rich;"),
            "long forms and into work in sort");
        check(full.direction == SortDirection::Descending, "descending still works");
        check(full.order_by == "customers.balance", "qualified order-by survives in sort");
        check(full.into_table.has_value(), "sort supports into");

        const auto& bare = as<SeekRows>(
            parse_one("seek customers show name order by name;"), "bare order by parses");
        check(bare.direction == SortDirection::Ascending, "bare order by means ascending");

        check_throws([] { parse_one("sort customers balance;"); }, "sort without by fails");
        check_throws([] { parse_one("sort customers;"); }, "sort without column fails");
    }

    // 13. `alter table` parses add (with optional default) and remove.
    {
        const auto& add = as<AlterAdd>(
            parse_one("alter table customers add nickname: text null;"), "alter add parses");
        check(add.table == "customers", "alter add keeps the table");
        check(add.column.name == "nickname", "alter add keeps the column");
        check(!add.column.required_flag, "nullable marker survives");
        check(!add.has_default, "no default clause means null fill");

        const auto& add_def = as<AlterAdd>(
            parse_one("alter table t add age: integer not null default 0;"),
            "alter add with default parses");
        check(add_def.has_default, "default clause is recorded");
        check(add_def.default_value == Value::integer(0), "default value is kept");

        const auto& add_full = as<AlterAdd>(
            parse_one("alter table t add id2: integer beelzebub;"),
            "alter add accepts beelzebub");
        check(add_full.column.is_primary, "beelzebub survives alter add");

        const auto& rem = as<AlterRemove>(
            parse_one("alter table customers remove nickname;"), "alter remove parses");
        check(rem.table == "customers", "alter remove keeps the table");
        check(rem.column == "nickname", "alter remove keeps the column");

        check_throws([] { parse_one("alter table t;"); }, "alter without verb fails");
        check_throws([] { parse_one("alter table t drop c;"); }, "alter drop is not a verb");
        check_throws([] { parse_one("alter table t add c: integer default;"); },
                     "dangling default fails");
    }

    std::cout << "All parser tests passed.\n";
}
