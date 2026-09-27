#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>

#include "exdeus/core/engine.hpp"

namespace exdeus::io {

// Named failure for snapshot problems: missing files, bad versions,
// malformed lines, type mismatches, duplicate names. Always thrown
// before the engine is touched, so a failed load changes nothing.
class SnapshotError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Versioned readable text snapshots (`.exd`) of one database.
// Save serializes the named database; load rebuilds it into an engine,
// harnesses it, and returns its name. Both work through the public
// Engine facade only — no Catalog or Table access.
class SnapshotStore {
public:
    static void save(const exdeus::core::Engine& engine, const std::string& database,
                     const std::filesystem::path& path);
    static std::string load(exdeus::core::Engine& engine, const std::filesystem::path& path);
};

}  // namespace exdeus::io
