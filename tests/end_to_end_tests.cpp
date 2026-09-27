// Lesson 9: the week-1 acceptance test. Reads examples/bank.exql as data,
// replays it command by command through the real Lexer -> Parser ->
// Interpreter path, and checks every layer the script touches: create,
// harness, forge, add, seek, change, remove, save, load, export. Then
// loads the saved snapshot into a fresh engine and confirms the data.
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "exdeus/core/value.hpp"
#include "exdeus/io/csv_exporter.hpp"
#include "exdeus/io/snapshot_store.hpp"
#include "exdeus/language/lexer.hpp"
#include "exdeus/language/parser.hpp"
#include "exdeus/repl/interpreter.hpp"

namespace {

using exdeus::core::Value;
using exdeus::language::Command;
using exdeus::language::Lexer;
using exdeus::language::Parser;
using exdeus::repl::Interpreter;
using exdeus::repl::Result;

int file_counter = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        std::exit(1);
    }
    std::cout << "PASS: " << message << "\n";
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

// Temp paths with forward slashes so they survive ExdeusQL string
// literals and reruns without colliding.
std::string temp_path(const std::string& stem, const std::string& ext) {
    std::filesystem::path dir = std::filesystem::temp_directory_path() / "exdeus_e2e";
    std::filesystem::create_directories(dir);
    std::string path =
        (dir / (stem + "_" + std::to_string(++file_counter) + ext)).string();
    for (char& c : path) {
        if (c == '\\') {
            c = '/';
        }
    }
    return path;
}

// The bank script saves to fixed names; point it at temp files instead so
// the test never writes into the repo and reruns stay hermetic.
std::string redirect_outputs(const std::string& source, const std::string& snap,
                             const std::string& csv) {
    std::string out = source;
    const std::string save_from = "save db to \"bank.exd\";";
    const std::string save_to = "save db to \"" + snap + "\";";
    size_t pos = out.find(save_from);
    if (pos != std::string::npos) {
        out.replace(pos, save_from.size(), save_to);
    }
    const std::string export_from = "export customers to \"customers.csv\";";
    const std::string export_to = "export customers to \"" + csv + "\";";
    pos = out.find(export_from);
    if (pos != std::string::npos) {
        out.replace(pos, export_from.size(), export_to);
    }
    return out;
}

// Finds examples/bank.exql whether the binary runs from build/Debug,
// build/, or the repo root.
std::filesystem::path find_bank_script() {
    const char* candidates[] = {
        "../../examples/bank.exql",
        "../examples/bank.exql",
        "examples/bank.exql",
        "D:/ValueProjects/ExDeus/Project-ExDeus/examples/bank.exql",
    };
    for (const char* candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return std::filesystem::path(candidate);
        }
    }
    std::cerr << "FAIL: examples/bank.exql not found\n";
    std::exit(1);
}

}  // namespace

void run_end_to_end_tests() {
    std::string snap = temp_path("bank", ".exd");
    std::string csv = temp_path("customers", ".csv");

    // 1. The bank script is real data: read it, redirect its outputs, and
    // replay every command through the production path.
    std::string source = redirect_outputs(read_file(find_bank_script()), snap, csv);
    Lexer lexer(source);
    Parser parser(lexer.tokenize());
    std::vector<Command> commands = parser.parse_all();
    check(commands.size() == 20, "bank script parses to twenty commands");

    // 2. Replay through the interpreter; the script's own seeks carry the
    // mid-run expectations.
    Interpreter db;
    std::vector<Result> results = db.execute_all(commands);
    check(results.size() == 20, "every bank command executes");
    check(results[0].message == "created database bank", "script creates bank");
    check(results[1].message == "harnessed database bank", "script harnesses bank");
    check(results[2].message == "forged table customers", "script forges customers");
    check(results[6].message == "forged table accounts", "script forges accounts");
    check(results[10].message == "forged table transactions", "script forges transactions");

    // The first seek (index 14): only Asha holds savings above 10000.
    check(results[14].table.has_value(), "rich-savings seek returns a table");
    check(results[14].table->rows.size() == 1 &&
              results[14].table->rows[0][0] == Value::text("Asha"),
          "only Asha holds savings above 10000");
    check(results[15].message == "changed 2 rows in customers",
          "interest hits both savings rows");
    check(results[16].message == "removed 1 row from customers", "Mira is removed");
    check(results[17].table.has_value() && results[17].table->rows.size() == 2,
          "remaining seek shows Asha and Ravi");
    check(results[18].message == "saved database bank to " + snap, "script saves the snapshot");
    check(results[19].message == "exported table customers to " + csv, "script exports the csv");

    // 3. Fresh engine from the snapshot: the week-1 durability promise.
    {
        Interpreter restored;
        Lexer load_lexer("load db from \"" + snap + "\";");
        Parser load_parser(load_lexer.tokenize());
        Result loaded = restored.execute(load_parser.parse_one());
        check(loaded.message == "loaded database bank from " + snap, "snapshot reloads");
        check(restored.engine().row_count("customers") == 2, "restored customers keeps 2 rows");
        check(restored.engine().row_count("accounts") == 3, "restored accounts keeps 3 rows");
        check(restored.engine().row_count("transactions") == 3,
              "restored transactions keeps 3 rows");

        Lexer seek_lexer(
            "seek customers show name, balance where name equals \"Asha\";");
        Parser seek_parser(seek_lexer.tokenize());
        Result asha = restored.execute(seek_parser.parse_one());
        check(asha.table->rows.size() == 1 &&
                  asha.table->rows[0][1] == Value::decimal(25500.50),
              "Asha keeps her balance plus interest after reload");
    }

    // 4. The exported CSV on disk: header plus the two surviving rows.
    {
        std::string text = read_file(csv);
        check(text.rfind("id,name,account_type,balance,active\n", 0) == 0,
              "csv keeps the five-column header");
        check(text.find("1,Asha,savings,25500.5,true\n") != std::string::npos,
              "csv keeps Asha with interest");
        check(text.find("Mira") == std::string::npos, "csv drops the removed row");
    }

    std::cout << "All end-to-end tests passed.\n";
}
