#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "exdeus/core/engine.hpp"
#include "exdeus/core/value.hpp"
#include "exdeus/io/csv_exporter.hpp"
#include "exdeus/io/snapshot_store.hpp"

namespace {

using exdeus::core::Column;
using exdeus::core::Engine;
using exdeus::core::Schema;
using exdeus::core::Value;
using exdeus::core::ValueType;
using exdeus::io::CsvExporter;
using exdeus::io::SnapshotStore;

int file_counter = 0;

std::filesystem::path temp_path(const std::string& stem, const std::string& ext) {
    std::filesystem::path dir = std::filesystem::temp_directory_path() / "exdeus_tests";
    std::filesystem::create_directories(dir);
    return dir / (stem + "_" + std::to_string(++file_counter) + ext);
}

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        std::exit(1);
    }
    std::cout << "PASS: " << message << "\n";
}

template <typename F>
void check_throws_snapshot(F&& run_fn, const char* message) {
    try {
        run_fn();
    } catch (const exdeus::io::SnapshotError& e) {
        std::cout << "PASS: " << message << " [" << e.what() << "]\n";
        return;
    }
    std::cerr << "FAIL: " << message << " (no SnapshotError thrown)\n";
    std::exit(1);
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

void write_file(const std::filesystem::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
}

Schema customer_schema() {
    return Schema({
        Column{"id", ValueType::Integer, true, true},
        Column{"name", ValueType::Text, true, false},
        Column{"balance", ValueType::Decimal, true, false},
        Column{"active", ValueType::Boolean, true, false},
    });
}

Engine seeded_bank() {
    Engine engine;
    engine.create_database("bank");
    engine.harness_database("bank");
    engine.create_table("customers", customer_schema());
    engine.insert("customers",
                  {Value::integer(1), Value::text("Asha"), Value::decimal(25000.50),
                   Value::boolean(true)});
    engine.insert("customers",
                  {Value::integer(2), Value::text("O\"Neil, Jr.\nJr"), Value::decimal(12000.00),
                   Value::boolean(false)});
    return engine;
}

void check_bank(const Engine& engine, const char* message) {
    check(engine.has_selection(), message);
    check(engine.selected_database() == "bank", "loaded database selects bank");
    check(engine.row_count("customers") == 2, "loaded customers keeps both rows");
    check(engine.row_at("customers", 0)[1] == Value::text("Asha"), "first row keeps its name");
    check(engine.row_at("customers", 1)[1] == Value::text("O\"Neil, Jr.\nJr"),
          "escaped comma, quote, and newline survive the round trip");
    check(engine.row_at("customers", 1)[3] == Value::boolean(false),
          "boolean false survives the round trip");
    // Copy the schema: table_schema() returns by value, so binding its
    // columns() to a reference would dangle into a dead temporary.
    const auto schema = engine.table_schema("customers");
    const auto& columns = schema.columns();
    check(columns.size() == 4 && columns[0].unique && !columns[1].unique,
          "unique flags survive the round trip");
}

}  // namespace

void run_snapshot_tests() {
    // 1. Save writes a versioned readable snapshot; load rebuilds it.
    {
        Engine engine = seeded_bank();
        auto path = temp_path("bank", ".exd");
        SnapshotStore::save(engine, "bank", path);
        std::string text = read_file(path);
        check(text.rfind("EXDEUS_SNAPSHOT 1\n", 0) == 0, "snapshot starts with the version header");
        check(text.find("database bank\n") != std::string::npos, "snapshot names the database");
        check(text.find("O\\\"Neil") != std::string::npos, "snapshot escapes the quote");

        Engine fresh;
        std::string name = SnapshotStore::load(fresh, path);
        check(name == "bank", "load returns the database name");
        check_bank(fresh, "loaded engine has bank selected");

        // Replace semantics: loading the same snapshot again restores it
        // instead of throwing duplicate.
        std::string again = SnapshotStore::load(fresh, path);
        check(again == "bank", "reload returns the database name");
        check_bank(fresh, "reloaded engine still matches");
    }

    // 2. Save replaces atomically: no temp file lingers, failures keep the old file.
    {
        Engine engine = seeded_bank();
        auto path = temp_path("atomic", ".exd");
        SnapshotStore::save(engine, "bank", path);
        std::string before = read_file(path);
        check(!std::filesystem::exists(std::filesystem::path(path.string() + ".tmp")),
              "no temp file lingers after save");
        check_throws_snapshot([&] { SnapshotStore::save(engine, "missing", path); },
                              "save of an unknown database fails");
        check(read_file(path) == before, "failed save keeps the old snapshot intact");
    }

    // 3. Malformed snapshots fail with useful errors and change nothing.
    {
        auto good_path = temp_path("good", ".exd");
        SnapshotStore::save(seeded_bank(), "bank", good_path);
        std::string good = read_file(good_path);

        auto bad_version = temp_path("badver", ".exd");
        write_file(bad_version, "EXDEUS_SNAPSHOT 99\ndatabase bank\n");
        Engine probe;
        check_throws_snapshot([&] { SnapshotStore::load(probe, bad_version); },
                              "unknown snapshot version fails");
        check(probe.list_databases().empty(), "failed load creates no database");

        auto bad_header = temp_path("badhead", ".exd");
        write_file(bad_header, "hello\n");
        check_throws_snapshot([&] { SnapshotStore::load(probe, bad_header); },
                              "missing version header fails");

        auto truncated = temp_path("trunc", ".exd");
        write_file(truncated, good.substr(0, good.size() / 2));
        check_throws_snapshot([&] { SnapshotStore::load(probe, truncated); },
                              "truncated snapshot fails");

        auto bad_escape = temp_path("badesc", ".exd");
        write_file(bad_escape, good + "oops \\q\n");
        check_throws_snapshot([&] { SnapshotStore::load(probe, bad_escape); },
                              "bad escape fails");

        auto dup_table = temp_path("duptab", ".exd");
        std::string doubled = good + good.substr(good.find("table customers"));
        write_file(dup_table, doubled);
        check_throws_snapshot([&] { SnapshotStore::load(probe, dup_table); },
                              "duplicate table in snapshot fails");

        auto missing = temp_path("nofile", ".exd");
        check_throws_snapshot([&] { SnapshotStore::load(probe, missing); },
                              "missing snapshot file fails");
        check(probe.list_databases().empty(), "no failed load leaks a database");
    }

    // 4. CSV export writes a header plus escaped rows.
    {
        Engine engine = seeded_bank();
        auto path = temp_path("customers", ".csv");
        CsvExporter::export_table(engine, "customers", path);
        std::string text = read_file(path);
        check(text.rfind("id,name,balance,active\n", 0) == 0, "csv starts with the header row");
        check(text.find("1,Asha,25000.5,true\n") != std::string::npos,
              "plain row needs no quoting");
        check(text.find("2,\"O\"\"Neil, Jr.\nJr\",12000,false\n") != std::string::npos,
              "comma, quote, and newline force quoting with doubled quotes");

        Engine bare;
        bare.create_database("bank");
        bare.harness_database("bank");
        check_throws_snapshot(
            [&] {
                try {
                    CsvExporter::export_table(bare, "customers", path);
                } catch (const exdeus::io::CsvError& e) {
                    throw exdeus::io::SnapshotError(e.what());
                }
            },
            "csv export of an unknown table fails");
    }

    std::cout << "All snapshot tests passed.\n";
}
