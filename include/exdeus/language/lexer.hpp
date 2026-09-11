#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include "exdeus/language/token.hpp"

namespace exdeus::language {

// Named failure: unexpected character or unterminated string, always with
// a 1-based line and column.
class LexerError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Single-pass scanner: consumed left to right with one char of lookahead.
// Keywords match case-insensitively; identifiers and strings keep spelling.
class Lexer {
public:
    explicit Lexer(std::string source);

    std::vector<Token> tokenize();

private:
    char peek() const;
    char peek_next() const;
    char advance();
    bool at_end() const;
    bool match(char expected);
    void skip_whitespace();
    [[noreturn]] void fail(const std::string& message) const;

    Token make_token(TokenKind kind, const std::string& lexeme, Position start) const;
    void scan_string();
    void scan_number();
    void scan_word();

    static bool is_alpha(char c);
    static bool is_digit(char c);
    static bool is_alnum(char c);

    std::string source_;
    std::vector<Token> tokens_;
    size_t cursor_ = 0;
    size_t line_ = 1;
    size_t column_ = 1;
};

}  // namespace exdeus::language
