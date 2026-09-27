// Lesson 8 implementation: versioned readable snapshots with atomic save.
#include "exdeus/io/snapshot_store.hpp"

#include <charconv>
#include <fstream>
#include <limits>
#include <sstream>
#include <system_error>
#include <variant>

namespace exdeus::io {

namespace {

using exdeus::core::Column;
using exdeus::core::Engine;
using exdeus::core::Row;
using exdeus::core::Schema;
using exdeus::core::Value;
using exdeus::core::ValueType;

constexpr int kVersion = 1;

// One whitespace-separated field of a snapshot line. Quoted fields arrive
// decoded (`\"` -> `"`, `\n` -> newline); bare fields arrive raw. Row
// parsing needs the distinction: text must be quoted, numbers must be bare.
struct Field {
    std::string text;
    bool quoted = false;
};

[[noreturn]] void fail(size_t lineno, const std::string& message) {
    throw SnapshotError("snapshot line " + std::to_string(lineno) + ": " + message);
}

std::string type_word(ValueType type) {
    switch (type) {
        case ValueType::Integer:
            return "integer";
        case ValueType::Decimal:
            return "decimal";
        case ValueType::Text:
            return "text";
        case ValueType::Boolean:
            return "boolean";
    }
    throw SnapshotError("unknown column type");
}

ValueType parse_type(const std::string& word, size_t lineno) {
    if (word == "integer") {
        return ValueType::Integer;
    }
    if (word == "decimal") {
        return ValueType::Decimal;
    }
    if (word == "text") {
        return ValueType::Text;
    }
    if (word == "boolean") {
        return ValueType::Boolean;
    }
    fail(lineno, "unknown column type '" + word + "'");
}

// Shortest round-trip decimal: `to_chars` prints `25000.5`, never
// `25000.500000`, and `stod` reads back the exact same double.
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

// Text escaping keeps the format line-based: quotes, backslashes, and
// control characters become `\"`, `\\`, `\n`, `\t`, `\r`, so a value
// holding a real newline still occupies exactly one physical line.
std::string quote_text(const std::string& value) {
    std::string out = "\"";
    for (char c : value) {
        switch (c) {
            case '\\':
                out += "\\\\";
                break;
            case '"':
                out += "\\\"";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\t':
                out += "\\t";
                break;
            case '\r':
                out += "\\r";
                break;
            default:
                out += c;
                break;
        }
    }
    out += '"';
    return out;
}

std::string format_value(const Value& value) {
    switch (value.type()) {
        case ValueType::Integer:
            return std::to_string(std::get<long long>(value.data()));
        case ValueType::Decimal:
            return format_decimal(std::get<double>(value.data()));
        case ValueType::Text:
            return quote_text(std::get<std::string>(value.data()));
        case ValueType::Boolean:
            return std::get<bool>(value.data()) ? "true" : "false";
    }
    throw SnapshotError("unknown value type");
}

std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::string current;
    for (char c : text) {
        if (c == '\n') {
            lines.push_back(current);
            current.clear();
        } else if (c != '\r') {
            current += c;
        }
    }
    lines.push_back(current);
    return lines;
}

// Splits one snapshot line on whitespace, honouring quoted sections.
// Throws on unterminated quotes and unknown `\x` escapes — both mean a
// corrupted file, and the error names the exact line.
std::vector<Field> split_fields(const std::string& line, size_t lineno) {
    std::vector<Field> fields;
    size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
            ++i;
        }
        if (i >= line.size()) {
            break;
        }
        if (line[i] == '"') {
            Field field;
            field.quoted = true;
            ++i;
            bool closed = false;
            while (i < line.size()) {
                char c = line[i];
                if (c == '"') {
                    closed = true;
                    ++i;
                    break;
                }
                if (c == '\\') {
                    ++i;
                    if (i >= line.size()) {
                        break;
                    }
                    switch (line[i]) {
                        case '\\':
                            field.text += '\\';
                            break;
                        case '"':
                            field.text += '"';
                            break;
                        case 'n':
                            field.text += '\n';
                            break;
                        case 't':
                            field.text += '\t';
                            break;
                        case 'r':
                            field.text += '\r';
                            break;
                        default:
                            fail(lineno,
                                 std::string("bad escape '\\") + line[i] + "' in snapshot");
                    }
                    ++i;
                    continue;
                }
                field.text += c;
                ++i;
            }
            if (!closed) {
                fail(lineno, "unterminated quoted string in snapshot");
            }
            fields.push_back(std::move(field));
            continue;
        }
        Field field;
        while (i < line.size() && line[i] != ' ' && line[i] != '\t') {
            field.text += line[i];
            ++i;
        }
        fields.push_back(std::move(field));
    }
    return fields;
}

long long parse_integer(const std::string& token, size_t lineno) {
    try {
        size_t used = 0;
        long long value = std::stoll(token, &used);
        if (used != token.size()) {
            fail(lineno, "expected integer, found '" + token + "'");
        }
        return value;
    } catch (const std::invalid_argument&) {
        fail(lineno, "expected integer, found '" + token + "'");
    } catch (const std::out_of_range&) {
        fail(lineno, "integer out of range: '" + token + "'");
    }
}

double parse_decimal(const std::string& token, size_t lineno) {
    try {
        size_t used = 0;
        double value = std::stod(token, &used);
        if (used != token.size()) {
            fail(lineno, "expected decimal, found '" + token + "'");
        }
        return value;
    } catch (const std::invalid_argument&) {
        fail(lineno, "expected decimal, found '" + token + "'");
    } catch (const std::out_of_range&) {
        fail(lineno, "decimal out of range: '" + token + "'");
    }
}

Value parse_field(const Column& column, const Field& field, size_t lineno) {
    switch (column.type) {
        case ValueType::Integer:
            if (field.quoted) {
                fail(lineno, "expected integer for column '" + column.name + "', found quoted text");
            }
            return Value::integer(parse_integer(field.text, lineno));
        case ValueType::Decimal:
            if (field.quoted) {
                fail(lineno, "expected decimal for column '" + column.name + "', found quoted text");
            }
            return Value::decimal(parse_decimal(field.text, lineno));
        case ValueType::Text:
            if (!field.quoted) {
                fail(lineno, "expected quoted text for column '" + column.name + "', found '" +
                                 field.text + "'");
            }
            return Value::text(field.text);
        case ValueType::Boolean:
            if (field.quoted) {
                fail(lineno, "expected boolean for column '" + column.name + "', found quoted text");
            }
            if (field.text == "true") {
                return Value::boolean(true);
            }
            if (field.text == "false") {
                return Value::boolean(false);
            }
            fail(lineno, "expected boolean for column '" + column.name + "', found '" +
                             field.text + "'");
    }
    fail(lineno, "unknown column type");
}

// Fully parsed snapshot: parse errors throw before any engine is touched,
// so a failed load changes nothing.
struct ParsedTable {
    std::string name;
    Schema schema;
    std::vector<Row> rows;
};

struct ParsedDb {
    std::string name;
    std::vector<ParsedTable> tables;
};

Schema parse_schema(const std::vector<std::string>& lines, size_t& pos, size_t count,
                    size_t header_lineno) {
    std::vector<Column> columns;
    for (size_t c = 0; c < count; ++c) {
        if (pos >= lines.size()) {
            fail(header_lineno, "table declares columns but the snapshot ends");
        }
        size_t lineno = pos + 1;
        std::vector<Field> fields = split_fields(lines[pos++], lineno);
        if (fields.empty()) {
            --c;  // blank lines never count as columns
            continue;
        }
        if (fields[0].text != "column" || fields.size() < 3) {
            fail(lineno, "expected 'column <name> <type> [required] [unique]'");
        }
        if (fields[1].quoted || fields[2].quoted) {
            fail(lineno, "column names and types must not be quoted");
        }
        Column column{fields[1].text, parse_type(fields[2].text, lineno), true, false};
        if (column.name.empty()) {
            fail(lineno, "column name must not be empty");
        }
        for (size_t f = 3; f < fields.size(); ++f) {
            if (fields[f].text == "required") {
                column.required = true;
            } else if (fields[f].text == "unique") {
                column.unique = true;
            } else {
                fail(lineno, "unknown column flag '" + fields[f].text + "'");
            }
        }
        columns.push_back(std::move(column));
    }
    try {
        return Schema(std::move(columns));
    } catch (const std::exception& e) {
        fail(header_lineno, e.what());
    }
}

Row parse_row(const Schema& schema, const std::vector<Field>& fields, size_t lineno) {
    if (fields.size() != schema.column_count()) {
        fail(lineno, "row has " + std::to_string(fields.size()) + " values, schema has " +
                         std::to_string(schema.column_count()));
    }
    Row row;
    for (size_t i = 0; i < fields.size(); ++i) {
        row.push_back(parse_field(schema.column_at(i), fields[i], lineno));
    }
    return row;
}

void check_unique(const ParsedTable& table, const Row& row, size_t lineno) {
    for (size_t i = 0; i < table.schema.column_count(); ++i) {
        if (!table.schema.column_at(i).unique) {
            continue;
        }
        for (const Row& existing : table.rows) {
            if (existing[i] == row[i]) {
                fail(lineno, "duplicate value in unique column '" +
                                 table.schema.column_at(i).name + "'");
            }
        }
    }
}

ParsedDb parse_snapshot(const std::string& text) {
    std::vector<std::string> lines = split_lines(text);
    if (lines.empty()) {
        throw SnapshotError("snapshot is empty: missing version header");
    }
    const std::string header = "EXDEUS_SNAPSHOT ";
    if (lines[0].rfind(header, 0) != 0) {
        throw SnapshotError("missing snapshot version header, found '" + lines[0] + "'");
    }
    if (lines[0].substr(header.size()) != std::to_string(kVersion)) {
        throw SnapshotError("unsupported snapshot version: '" +
                            lines[0].substr(header.size()) + "'");
    }

    ParsedDb db;
    size_t pos = 1;
    // Skip blank lines between sections; every content line carries its
    // 1-based number into errors.
    auto next_content = [&]() -> bool {
        while (pos < lines.size() &&
               split_fields(lines[pos], pos + 1).empty()) {
            ++pos;
        }
        return pos < lines.size();
    };

    if (!next_content()) {
        throw SnapshotError("snapshot names no database");
    }
    {
        size_t lineno = pos + 1;
        std::vector<Field> fields = split_fields(lines[pos++], lineno);
        if (fields.size() != 2 || fields[0].text != "database" || fields[1].quoted ||
            fields[1].text.empty()) {
            fail(lineno, "expected 'database <name>'");
        }
        db.name = fields[1].text;
    }

    while (next_content()) {
        size_t lineno = pos + 1;
        std::vector<Field> fields = split_fields(lines[pos], lineno);
        if (fields[0].text == "end") {
            if (fields.size() != 3 || fields[1].text != "database" || fields[2].text != db.name) {
                fail(lineno, "expected 'end database " + db.name + "'");
            }
            ++pos;
            if (next_content()) {
                fail(pos + 1, "unexpected content after end of snapshot");
            }
            if (db.tables.empty()) {
                fail(lineno, "database '" + db.name + "' holds no tables");
            }
            return db;
        }
        if (fields[0].text != "table" || fields.size() != 3 || fields[1].quoted) {
            fail(lineno, "expected 'table <name> <column-count>' or 'end database " + db.name +
                             "'");
        }
        for (const ParsedTable& existing : db.tables) {
            if (existing.name == fields[1].text) {
                fail(lineno, "duplicate table '" + fields[1].text + "'");
            }
        }
        ParsedTable table;
        table.name = fields[1].text;
        if (table.name.empty()) {
            fail(lineno, "table name must not be empty");
        }
        size_t count = static_cast<size_t>(parse_integer(fields[2].text, lineno));
        if (count == 0) {
            fail(lineno, "table '" + table.name + "' declares no columns");
        }
        if (fields[2].quoted) {
            fail(lineno, "table column count must not be quoted");
        }
        size_t header_lineno = lineno;
        ++pos;
        table.schema = parse_schema(lines, pos, count, header_lineno);
        while (pos < lines.size()) {
            size_t row_lineno = pos + 1;
            std::vector<Field> row_fields = split_fields(lines[pos], row_lineno);
            if (row_fields.empty()) {
                ++pos;
                continue;
            }
            if (row_fields[0].text != "row") {
                break;  // next table or the end marker
            }
            row_fields.erase(row_fields.begin());
            Row row = parse_row(table.schema, row_fields, row_lineno);
            check_unique(table, row, row_lineno);
            table.rows.push_back(std::move(row));
            ++pos;
        }
        db.tables.push_back(std::move(table));
    }
    fail(lines.size(), "snapshot ends before 'end database " + db.name + "'");
}

}  // namespace

void SnapshotStore::save(const Engine& engine, const std::string& database,
                         const std::filesystem::path& path) {
    bool known = false;
    for (const std::string& name : engine.list_databases()) {
        if (name == database) {
            known = true;
            break;
        }
    }
    if (!known) {
        throw SnapshotError("unknown database: " + database);
    }

    std::ostringstream out;
    out << "EXDEUS_SNAPSHOT " << kVersion << "\n";
    out << "database " << database << "\n";
    for (const std::string& table_name : engine.list_tables()) {
        const Schema schema = engine.table_schema(table_name);
        out << "table " << table_name << " " << schema.column_count() << "\n";
        for (const Column& column : schema.columns()) {
            out << "column " << column.name << " " << type_word(column.type);
            if (column.required) {
                out << " required";
            }
            if (column.unique) {
                out << " unique";
            }
            out << "\n";
        }
        for (size_t i = 0; i < engine.row_count(table_name); ++i) {
            out << "row";
            for (const Value& value : engine.row_at(table_name, i)) {
                out << " " << format_value(value);
            }
            out << "\n";
        }
    }
    out << "end database " << database << "\n";

    // Atomic replace: the reader never sees a half-written snapshot.
    // A crash between write and rename leaves `<name>.tmp`, never a
    // truncated `<name>`.
    if (!path.parent_path().empty()) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            throw SnapshotError("cannot create snapshot directory '" + path.string() +
                                "': " + ec.message());
        }
    }
    std::filesystem::path tmp(path.string() + ".tmp");
    {
        std::ofstream file(tmp, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            throw SnapshotError("cannot write snapshot file '" + tmp.string() + "'");
        }
        file << out.str();
        file.flush();
        if (!file) {
            throw SnapshotError("cannot write snapshot file '" + tmp.string() + "'");
        }
    }
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        throw SnapshotError("cannot replace snapshot file '" + path.string() +
                            "': " + ec.message());
    }
}

std::string SnapshotStore::load(Engine& engine, const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw SnapshotError("cannot open snapshot file '" + path.string() + "'");
    }
    std::ostringstream text;
    text << file.rdbuf();
    if (file.bad()) {
        throw SnapshotError("cannot read snapshot file '" + path.string() + "'");
    }

    // Parse fully before touching the engine: malformed input throws here
    // with the engine exactly as it was.
    ParsedDb db = parse_snapshot(text.str());

    // Replace semantics: loading over an existing database of the same
    // name restores the snapshot. Drop first, then rebuild — the parse
    // above already validated everything, so the apply below cannot fail
    // on content (the catch is a safety net, not a plan).
    bool had_existing = false;
    for (const std::string& name : engine.list_databases()) {
        if (name == db.name) {
            had_existing = true;
            break;
        }
    }
    if (had_existing) {
        try {
            engine.drop_database(db.name);
        } catch (const std::exception& e) {
            throw SnapshotError(e.what());
        }
    }
    try {
        engine.create_database(db.name);
        engine.harness_database(db.name);
        for (const ParsedTable& table : db.tables) {
            engine.create_table(table.name, table.schema);
            for (const Row& row : table.rows) {
                engine.insert(table.name, row);
            }
        }
    } catch (...) {
        // Best-effort rollback: a failed apply leaves no half-built database.
        try {
            engine.drop_database(db.name);
        } catch (...) {
        }
        throw;
    }
    return db.name;
}

}  // namespace exdeus::io
