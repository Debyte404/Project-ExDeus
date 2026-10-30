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
    KwAsc,
    KwDesc,
    KwSort,
    KwAnd,
    KwOr,
    KwEquals,
    KwAbove,
    KwBelow,
    KwPlus,
    KwMinus,
    KwUnique,
    KwRequired,
    KwBeelzebub,
    KwNot,
    KwCheck,
    KwReferences,
    KwInteger,
    KwDecimal,
    KwText,
    KwBoolean,
    KwInner,
    KwJoin,
    KwOn,
    KwInto,
    // Literals.
    IntegerLit,
    DecimalLit,
    StringLit,
    BoolLit,
    NullLit,
    Identifier,
    // Punctuation.
    Semicolon,
    Comma,
    Colon,
    Dot,         // . (qualified names: customers.id)
    Star,        // * (show-all projection)
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
    // End of the token in source (exclusive): the position right after its
    // last character. Lets the highlighter slice exact source spans, so
    // whitespace, string escapes, and comments survive verbatim.
    Position end = Position{1, 1};
    // Set only for the four literal kinds; reused core::Value so the
    // parser can hand values straight to the engine without conversion.
    std::optional<exdeus::core::Value> literal = std::nullopt;
};

}  // namespace exdeus::language
