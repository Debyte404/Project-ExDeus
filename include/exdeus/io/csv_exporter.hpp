#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>

#include "exdeus/core/engine.hpp"

namespace exdeus::io {

// Named failure for CSV export problems: unknown tables, missing
// selection, unwritable paths.
class CsvError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// One table of the harnessed database as RFC-style CSV: a header row of
// column names, one row per record, quoting plus quote-doubling for
// fields containing commas, quotes, or newlines.
class CsvExporter {
public:
    static void export_table(const exdeus::core::Engine& engine, const std::string& table,
                             const std::filesystem::path& path);
};

}  // namespace exdeus::io
