// Lesson 5 implementation: single-pass ExdeusQL scanner.
#include "exdeus/language/lexer.hpp"

#include <cctype>
#include <unordered_map>

#include "exdeus/core/value.hpp"

namespace exdeus::language {

namespace {

const std::unordered_map<std::string, TokenKind>& keywords() {
    static const std::unordered_map<std::string, TokenKind> table = {
        {"create", TokenKind::KwCreate},   {"harness", TokenKind::KwHarness},
        {"forge", TokenKind::KwForge},     {"table", TokenKind::KwTable},
        {"db", TokenKind::KwDb},           {"database", TokenKind::KwDatabase},
        {"add", TokenKind::KwAdd},         {"seek", TokenKind::KwSeek},
        {"show", TokenKind::KwShow},       {"where", TokenKind::KwWhere},
        {"change", TokenKind::KwChange},   {"set", TokenKind::KwSet},
        {"remove", TokenKind::KwRemove},   {"save", TokenKind::KwSave},
        {"load", TokenKind::KwLoad},       {"export", TokenKind::KwExport},
        {"from", TokenKind::KwFrom},       {"to", TokenKind::KwTo},
        {"with", TokenKind::KwWith},       {"order", TokenKind::KwOrder},
        {"by", TokenKind::KwBy},           {"ascending", TokenKind::KwAscending},
        {"descending", TokenKind::KwDescending},
        {"and", TokenKind::KwAnd},         {"or", TokenKind::KwOr},
        {"equals", TokenKind::KwEquals},   {"above", TokenKind::KwAbove},
        {"below", TokenKind::KwBelow},     {"plus", TokenKind::KwPlus},
        {"minus", TokenKind::KwMinus},     {"unique", TokenKind::KwUnique},
        {"required", TokenKind::KwRequired},
        {"integer", TokenKind::KwInteger}, {"decimal", TokenKind::KwDecimal},
        {"text", TokenKind::KwText},       {"boolean", TokenKind::KwBoolean},
        {"true", TokenKind::BoolLit},      {"false", TokenKind::BoolLit},
    };
    return table;
}

std::string lower_copy(const std::string& text) {
    std::string out = text;
    for (char& c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

}  // namespace

const char* to_string(TokenKind kind) {
    switch (kind) {
        case TokenKind::KwCreate: return "create";
        case TokenKind::KwHarness: return "harness";
        case TokenKind::KwForge: return "forge";
        case TokenKind::KwTable: return "table";
        case TokenKind::KwDb: return "db";
        case TokenKind::KwDatabase: return "database";
        case TokenKind::KwAdd: return "add";
        case TokenKind::KwSeek: return "seek";
        case TokenKind::KwShow: return "show";
        case TokenKind::KwWhere: return "where";
        case TokenKind::KwChange: return "change";
        case TokenKind::KwSet: return "set";
        case TokenKind::KwRemove: return "remove";
        case TokenKind::KwSave: return "save";
        case TokenKind::KwLoad: return "load";
        case TokenKind::KwExport: return "export";
        case TokenKind::KwFrom: return "from";
        case TokenKind::KwTo: return "to";
        case TokenKind::KwWith: return "with";
        case TokenKind::KwOrder: return "order";
        case TokenKind::KwBy: return "by";
        case TokenKind::KwAscending: return "ascending";
        case TokenKind::KwDescending: return "descending";
        case TokenKind::KwAnd: return "and";
        case TokenKind::KwOr: return "or";
        case TokenKind::KwEquals: return "equals";
        case TokenKind::KwAbove: return "above";
        case TokenKind::KwBelow: return "below";
        case TokenKind::KwPlus: return "plus";
        case TokenKind::KwMinus: return "minus";
        case TokenKind::KwUnique: return "unique";
        case TokenKind::KwRequired: return "required";
        case TokenKind::KwInteger: return "integer";
        case TokenKind::KwDecimal: return "decimal";
        case TokenKind::KwText: return "text";
        case TokenKind::KwBoolean: return "boolean";
        case TokenKind::IntegerLit: return "integer literal";
        case TokenKind::DecimalLit: return "decimal literal";
        case TokenKind::StringLit: return "string literal";
        case TokenKind::BoolLit: return "boolean literal";
        case TokenKind::Identifier: return "identifier";
        case TokenKind::Semicolon: return ";";
        case TokenKind::Comma: return ",";
        case TokenKind::Colon: return ":";
        case TokenKind::LParen: return "(";
        case TokenKind::RParen: return ")";
        case TokenKind::LBrace: return "{";
        case TokenKind::RBrace: return "}";
        case TokenKind::Assign: return "=";
        case TokenKind::Equal: return "==";
        case TokenKind::NotEqual: return "!=";
        case TokenKind::Less: return "<";
        case TokenKind::Greater: return ">";
        case TokenKind::LessEqual: return "<=";
        case TokenKind::GreaterEqual: return ">=";
        case TokenKind::EndOfInput: return "end of input";
    }
    return "?";
}

Lexer::Lexer(std::string source) : source_(std::move(source)) {}

char Lexer::peek() const {
    return at_end() ? '\0' : source_[cursor_];
}

char Lexer::peek_next() const {
    return (cursor_ + 1 >= source_.size()) ? '\0' : source_[cursor_ + 1];
}

char Lexer::advance() {
    char c = source_[cursor_++];
    if (c == '\n') {
        ++line_;
        column_ = 1;
    } else {
        ++column_;
    }
    return c;
}

bool Lexer::at_end() const {
    return cursor_ >= source_.size();
}

bool Lexer::match(char expected) {
    if (at_end() || source_[cursor_] != expected) {
        return false;
    }
    advance();
    return true;
}

void Lexer::skip_whitespace() {
    while (!at_end()) {
        char c = peek();
        if (c == ' ' || c == '\r' || c == '\t' || c == '\n') {
            advance();
        } else if (c == '-' && peek_next() == '-') {
            // Line comment to end of line.
            while (!at_end() && peek() != '\n') {
                advance();
            }
        } else {
            break;
        }
    }
}

[[noreturn]] void Lexer::fail(const std::string& message) const {
    throw LexerError("line " + std::to_string(line_) + ", column " +
                     std::to_string(column_) + ": " + message);
}

Token Lexer::make_token(TokenKind kind, const std::string& lexeme, Position start) const {
    return Token{kind, lexeme, start, std::nullopt};
}

void Lexer::scan_string() {
    Position start{line_, column_ - 1};  // column already past the quote
    std::string value;
    while (!at_end() && peek() != '"') {
        char c = advance();
        if (c == '\\' && !at_end()) {
            char esc = advance();
            switch (esc) {
                case 'n': value.push_back('\n'); break;
                case 't': value.push_back('\t'); break;
                case '"': value.push_back('"'); break;
                case '\\': value.push_back('\\'); break;
                default: value.push_back(esc); break;
            }
        } else {
            value.push_back(c);
        }
    }
    if (at_end()) {
        throw LexerError("line " + std::to_string(start.line) + ", column " +
                         std::to_string(start.column) + ": unterminated string");
    }
    advance();  // closing quote
    Token token = make_token(TokenKind::StringLit, value, start);
    token.literal = exdeus::core::Value::text(value);
    tokens_.push_back(std::move(token));
}

void Lexer::scan_number() {
    Position start{line_, column_ - 1};  // first digit already consumed
    size_t begin = cursor_ - 1;  // first digit already consumed
    bool is_decimal = false;
    while (is_digit(peek())) {
        advance();
    }
    if (peek() == '.' && is_digit(peek_next())) {
        is_decimal = true;
        advance();  // the dot
        while (is_digit(peek())) {
            advance();
        }
    }
    std::string lexeme = source_.substr(begin, cursor_ - begin);
    Token token = make_token(
        is_decimal ? TokenKind::DecimalLit : TokenKind::IntegerLit, lexeme, start);
    if (is_decimal) {
        token.literal = exdeus::core::Value::decimal(std::stod(lexeme));
    } else {
        token.literal = exdeus::core::Value::integer(std::stoll(lexeme));
    }
    tokens_.push_back(std::move(token));
}

void Lexer::scan_word() {
    Position start{line_, column_ - 1};  // first char already consumed
    size_t begin = cursor_ - 1;  // first char already consumed
    while (is_alnum(peek())) {
        advance();
    }
    std::string word = source_.substr(begin, cursor_ - begin);
    auto it = keywords().find(lower_copy(word));
    if (it == keywords().end()) {
        tokens_.push_back(make_token(TokenKind::Identifier, word, start));
        return;
    }
    Token token = make_token(it->second, word, start);
    if (it->second == TokenKind::BoolLit) {
        token.literal = exdeus::core::Value::boolean(lower_copy(word) == "true");
    }
    tokens_.push_back(std::move(token));
}

bool Lexer::is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool Lexer::is_digit(char c) {
    return c >= '0' && c <= '9';
}

bool Lexer::is_alnum(char c) {
    return is_alpha(c) || is_digit(c);
}

std::vector<Token> Lexer::tokenize() {
    tokens_.clear();
    cursor_ = 0;
    line_ = 1;
    column_ = 1;
    while (!at_end()) {
        skip_whitespace();
        if (at_end()) {
            break;
        }
        Position start{line_, column_};
        char c = advance();
        switch (c) {
            case ';': tokens_.push_back(make_token(TokenKind::Semicolon, ";", start)); break;
            case ',': tokens_.push_back(make_token(TokenKind::Comma, ",", start)); break;
            case ':': tokens_.push_back(make_token(TokenKind::Colon, ":", start)); break;
            case '(': tokens_.push_back(make_token(TokenKind::LParen, "(", start)); break;
            case ')': tokens_.push_back(make_token(TokenKind::RParen, ")", start)); break;
            case '{': tokens_.push_back(make_token(TokenKind::LBrace, "{", start)); break;
            case '}': tokens_.push_back(make_token(TokenKind::RBrace, "}", start)); break;
            case '=':
                if (match('=')) {
                    tokens_.push_back(make_token(TokenKind::Equal, "==", start));
                } else {
                    tokens_.push_back(make_token(TokenKind::Assign, "=", start));
                }
                break;
            case '!':
                if (match('=')) {
                    tokens_.push_back(make_token(TokenKind::NotEqual, "!=", start));
                } else {
                    fail("unexpected character '!'");
                }
                break;
            case '<':
                if (match('=')) {
                    tokens_.push_back(make_token(TokenKind::LessEqual, "<=", start));
                } else {
                    tokens_.push_back(make_token(TokenKind::Less, "<", start));
                }
                break;
            case '>':
                if (match('=')) {
                    tokens_.push_back(make_token(TokenKind::GreaterEqual, ">=", start));
                } else {
                    tokens_.push_back(make_token(TokenKind::Greater, ">", start));
                }
                break;
            case '"':
                scan_string();
                break;
            default:
                if (is_digit(c)) {
                    scan_number();
                } else if (is_alpha(c)) {
                    scan_word();
                } else {
                    throw LexerError("line " + std::to_string(start.line) + ", column " +
                                     std::to_string(start.column) + ": unexpected character '" +
                                     c + "'");
                }
                break;
        }
    }
    tokens_.push_back(make_token(TokenKind::EndOfInput, "", Position{line_, column_}));
    return tokens_;
}

}  // namespace exdeus::language
