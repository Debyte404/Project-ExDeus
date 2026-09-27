// Lesson 8 implementation: one table as RFC-style CSV.
#include "exdeus/io/csv_exporter.hpp"

#include <charconv>
#include <fstream>
#include <limits>
#include <sstream>
#include <system_error>
#include <variant>

namespace exdeus::io {

namespace {

using exdeus::core::Engine;
using exdeus::core::Value;
using exdeus::core::ValueType;

std::string format_decimal(double value) {
    char buf[64];
    auto result = std::to_chars(buf, buf + sizeof(buf), value);
    if (result.ec == std::errc()) {
        return std::string(buf, result.ptr);
    }
    std::ostringstream out;
    out.precision(std::numeric_limits<double>::max_digits10);
    out << value;
    return out.str();
}

// CSV quoting: fields holding a comma, quote, or newline are wrapped in
// quotes with every inner quote doubled. Plain fields stay bare, so
// `Asha` never becomes `"Asha"`.
std::string csv_field(const Value& value) {
    if (value.type() == ValueType::Integer) {
        return std::to_string(std::get<long long>(value.data()));
    }
    if (value.type() == ValueType::Decimal) {
        return format_decimal(std::get<double>(value.data()));
    }
    if (value.type() == ValueType::Boolean) {
        return std::get<bool>(value.data()) ? "true" : "false";
    }
    const std::string& text = std::get<std::string>(value.data());
    if (text.find_first_of(",\"\n\r") == std::string::npos) {
        return text;
    }
    std::string out = "\"";
    for (char c : text) {
        if (c == '"') {
            out += "\"\"";
        } else {
            out += c;
        }
    }
    out += '"';
    return out;
}

}  // namespace

void CsvExporter::export_table(const Engine& engine, const std::string& table,
                               const std::filesystem::path& path) {
    const exdeus::core::Table* target = nullptr;
    try {
        target = &engine.table(table);
    } catch (const std::exception& e) {
        // One catch type for export callers: missing selection and unknown
        // tables both surface as CsvError.
        throw CsvError(e.what());
    }

    std::ostringstream out;
    bool first = true;
    for (const auto& column : target->schema().columns()) {
        if (!first) {
            out << ',';
        }
        out << column.name;
        first = false;
    }
    out << '\n';
    for (size_t i = 0; i < target->row_count(); ++i) {
        bool cell_first = true;
        for (const Value& value : target->row_at(i)) {
            if (!cell_first) {
                out << ',';
            }
            out << csv_field(value);
            cell_first = false;
        }
        out << '\n';
    }

    if (!path.parent_path().empty()) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            throw CsvError("cannot create export directory '" + path.string() +
                           "': " + ec.message());
        }
    }
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        throw CsvError("cannot write csv file '" + path.string() + "'");
    }
    file << out.str();
    file.flush();
    if (!file) {
        throw CsvError("cannot write csv file '" + path.string() + "'");
    }
}

}  // namespace exdeus::io
