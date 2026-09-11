#pragma once

#include <optional>
#include <string>
#include <vector>

#include "exdeus/core/value.hpp"

namespace exdeus::language {

// 1-based source position. Every token carries one so parser errors can
// point at the exact line and column.
struct Position {
    size_t line = 1;
    size_t column = 1;
};

enum class TokenKind {
    // Keywords (matched case-insensitively, lexeme keeps source spelling).
    KwCreate,
    KwHarness,
    KwForge,
    KwTable,
    KwDb,
    KwDatabase,
    KwAdd,
    KwSeek,
    KwShow,
    KwWhere,
    KwChange,
    KwSet,
    KwRemove,
    KwSave,
    KwLoad,
    KwExport,
    KwFrom,
    KwTo,
    KwWith,
    KwOrder,
    KwBy,
    KwAscending,
    KwDescending,
    KwAnd,
    KwOr,
    KwEquals,
    KwAbove,
    KwBelow,
    KwPlus,
    KwMinus,
    KwUnique,
    KwRequired,
    KwInteger,
    KwDecimal,
    KwText,
    KwBoolean,
    // Literals.
    IntegerLit,
    DecimalLit,
    StringLit,
    BoolLit,
    Identifier,
    // Punctuation.
    Semicolon,
    Comma,
    Colon,
    LParen,
    RParen,
    LBrace,
    RBrace,
    // Operators.
    Assign,      // =
    Equal,       // ==
    NotEqual,    // !=
    Less,        // <
    Greater,     // >
    LessEqual,   // <=
    GreaterEqual,  // >=
    EndOfInput,
};

const char* to_string(TokenKind kind);

struct Token {
    TokenKind kind;
    std::string lexeme;
    Position position;
    // Set only for the four literal kinds; reused core::Value so the
    // parser can hand values straight to the engine without conversion.
    std::optional<exdeus::core::Value> literal;
};

}  // namespace exdeus::language
