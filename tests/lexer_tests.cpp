#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <variant>

#include "exdeus/language/lexer.hpp"

namespace {

using exdeus::language::Lexer;
using exdeus::language::Token;
using exdeus::language::TokenKind;

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        std::exit(1);
    }
    std::cout << "PASS: " << message << "\n";
}

std::vector<Token> lex(const std::string& source) {
    Lexer lexer(source);
    return lexer.tokenize();
}

const Token& at(const std::vector<Token>& tokens, size_t i) {
    if (i >= tokens.size()) {
        std::cerr << "FAIL: token index out of range\n";
        std::exit(1);
    }
    return tokens[i];
}

}  // namespace

void run_lexer_tests() {
    using exdeus::core::ValueType;

    // 1. Database commands lex to keywords + identifier + semicolon.
    {
        auto tokens = lex("create db bank;");
        check(at(tokens, 0).kind == TokenKind::KwCreate, "create lexes as keyword");
        check(at(tokens, 1).kind == TokenKind::KwDb, "db lexes as keyword");
        check(at(tokens, 2).kind == TokenKind::Identifier, "db name lexes as identifier");
        check(at(tokens, 2).lexeme == "bank", "identifier keeps its spelling");
        check(at(tokens, 3).kind == TokenKind::Semicolon, "terminator lexes as semicolon");
        check(at(tokens, 4).kind == TokenKind::EndOfInput, "stream ends with EndOfInput");
        check(at(tokens, 0).position.line == 1 && at(tokens, 0).position.column == 1,
              "first token starts at line 1 column 1");
        check(at(tokens, 2).position.column == 11, "identifier position tracks columns");
    }

    // 2. Keywords match case-insensitively; database alias works.
    {
        auto tokens = lex("HARNESS Bank;");
        check(at(tokens, 0).kind == TokenKind::KwHarness, "harness matches any case");
        check(at(tokens, 0).lexeme == "HARNESS", "keyword keeps source spelling");
        auto alias = lex("create database bank;");
        check(at(alias, 1).kind == TokenKind::KwDatabase, "database is an alias of db");
    }

    // 3. forge table header lexes names, colons, types, constraints.
    {
        auto tokens = lex("forge table customers with id: integer unique, name: text");
        check(at(tokens, 0).kind == TokenKind::KwForge, "forge lexes as keyword");
        check(at(tokens, 1).kind == TokenKind::KwTable, "table lexes as keyword");
        check(at(tokens, 3).kind == TokenKind::KwWith, "with lexes as keyword");
        check(at(tokens, 5).kind == TokenKind::Colon, "colon separates name and type");
        check(at(tokens, 6).kind == TokenKind::KwInteger, "integer lexes as type keyword");
        check(at(tokens, 7).kind == TokenKind::KwUnique, "unique lexes as keyword");
        check(at(tokens, 8).kind == TokenKind::Comma, "comma separates columns");
        check(at(tokens, 11).kind == TokenKind::KwText, "text lexes as type keyword");
    }

    // 4. Literals carry typed values the parser can hand to the engine.
    {
        auto tokens = lex("add customers (1, 25000.50, \"Asha\", true, false)");
        check(at(tokens, 3).kind == TokenKind::IntegerLit, "int literal has its kind");
        check(at(tokens, 3).literal->type() == ValueType::Integer,
              "int literal carries an Integer value");
        check(at(tokens, 5).kind == TokenKind::DecimalLit, "decimal literal has its kind");
        check(at(tokens, 5).literal->type() == ValueType::Decimal,
              "decimal literal carries a Decimal value");
        check(at(tokens, 7).kind == TokenKind::StringLit, "string literal has its kind");
        check(at(tokens, 7).lexeme == "Asha", "string lexeme drops the quotes");
        check(at(tokens, 9).kind == TokenKind::BoolLit, "true lexes as bool literal");
        check(std::get<bool>(at(tokens, 9).literal->data()) == true,
              "true literal carries boolean true");
        check(std::get<bool>(at(tokens, 11).literal->data()) == false,
              "false literal carries boolean false");
    }

    // 5. Escapes inside strings; query words and operators.
    {
        auto tokens = lex("seek customers show name where balance above 10000 order by balance descending");
        check(at(tokens, 0).kind == TokenKind::KwSeek, "seek lexes as keyword");
        check(at(tokens, 2).kind == TokenKind::KwShow, "show lexes as keyword");
        check(at(tokens, 4).kind == TokenKind::KwWhere, "where lexes as keyword");
        check(at(tokens, 6).kind == TokenKind::KwAbove, "above lexes as keyword");
        check(at(tokens, 8).kind == TokenKind::KwOrder, "order lexes as keyword");
        check(at(tokens, 9).kind == TokenKind::KwBy, "by lexes as keyword");
        check(at(tokens, 11).kind == TokenKind::KwDescending, "descending lexes as keyword");

        auto escaped = lex("\"a\\\"b\\\\c\\n\"");
        check(at(escaped, 0).lexeme == "a\"b\\c\n", "string escapes are decoded");

        auto ops = lex("= == != < <= > >=");
        check(at(ops, 0).kind == TokenKind::Assign, "= is Assign");
        check(at(ops, 1).kind == TokenKind::Equal, "== is Equal");
        check(at(ops, 2).kind == TokenKind::NotEqual, "!= is NotEqual");
        check(at(ops, 3).kind == TokenKind::Less, "< is Less");
        check(at(ops, 4).kind == TokenKind::LessEqual, "<= is LessEqual");
        check(at(ops, 5).kind == TokenKind::Greater, "> is Greater");
        check(at(ops, 6).kind == TokenKind::GreaterEqual, ">= is GreaterEqual");
    }

    // 6. Positions span lines; comments are skipped; errors point at source.
    {
        auto tokens = lex("create db bank;\n-- pick it\nharness bank;");
        check(at(tokens, 4).kind == TokenKind::KwHarness, "comment line is skipped");
        check(at(tokens, 4).position.line == 3 && at(tokens, 4).position.column == 1,
              "token after comment lands on line 3");

        bool bad_char = false;
        try {
            lex("create db bank@;");
        } catch (const exdeus::language::LexerError& e) {
            bad_char = std::string(e.what()).find("line 1, column 15") != std::string::npos;
        }
        check(bad_char, "bad character error carries line and column");

        bool unterminated = false;
        try {
            lex("\"oops");
        } catch (const exdeus::language::LexerError&) {
            unterminated = true;
        }
        check(unterminated, "unterminated string throws LexerError");

        bool bang = false;
        try {
            lex("!");
        } catch (const exdeus::language::LexerError&) {
            bang = true;
        }
        check(bang, "lone ! throws LexerError");
    }

    std::cout << "All lexer tests passed.\n";
}
