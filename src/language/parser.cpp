// Lesson 6 implementation: recursive-descent ExdeusQL parser.
#include "exdeus/language/parser.hpp"

#include <utility>
#include <variant>

namespace exdeus::language {

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {
    if (tokens_.empty() ||
        tokens_.back().kind != TokenKind::EndOfInput) {
        tokens_.push_back(
            Token{TokenKind::EndOfInput, "", Position{}, std::nullopt});
    }
}

const Token& Parser::peek() const {
    return tokens_[cursor_];
}

const Token& Parser::advance() {
    if (!at_end()) {
        ++cursor_;
    }
    return tokens_[cursor_ - 1];
}

bool Parser::at_end() const {
    return peek().kind == TokenKind::EndOfInput;
}

bool Parser::check(TokenKind kind) const {
    return peek().kind == kind;
}

bool Parser::match(TokenKind kind) {
    if (check(kind)) {
        advance();
        return true;
    }
    return false;
}

const Token& Parser::expect(TokenKind kind, const std::string& what) {
    if (!check(kind)) {
        if (at_end()) {
            fail_at_end(what);
        }
        fail(peek(), what);
    }
    return advance();
}

[[noreturn]] void Parser::fail(const Token& token, const std::string& expected) const {
    throw ParserError("line " + std::to_string(token.position.line) + ", column " +
                      std::to_string(token.position.column) + ": expected " + expected +
                      ", found '" + token.lexeme + "' (" + to_string(token.kind) + ")");
}

[[noreturn]] void Parser::fail_at_end(const std::string& expected) const {
    const Token& end = peek();
    throw ParserError("line " + std::to_string(end.position.line) + ", column " +
                      std::to_string(end.position.column) + ": expected " + expected +
                      ", found end of input");
}

std::string Parser::expect_name(const std::string& what) {
    return expect(TokenKind::Identifier, what).lexeme;
}

exdeus::core::Value Parser::expect_literal(const std::string& what) {
    const Token& token = peek();
    switch (token.kind) {
        case TokenKind::IntegerLit:
        case TokenKind::DecimalLit:
        case TokenKind::StringLit:
        case TokenKind::BoolLit:
            advance();
            return *token.literal;
        default:
            break;
    }
    if (at_end()) {
        fail_at_end(what);
    }
    fail(token, what);
}

std::string Parser::expect_string(const std::string& what) {
    const Token& token = expect(TokenKind::StringLit, what);
    return *std::get_if<std::string>(&token.literal->data());
}

Command Parser::parse_one() {
    Command command = parse_command();
    expect(TokenKind::Semicolon, "';'");
    if (!at_end()) {
        fail(peek(), "end of input after ';'");
    }
    return command;
}

std::vector<Command> Parser::parse_all() {
    std::vector<Command> commands;
    while (!at_end()) {
        commands.push_back(parse_command());
        expect(TokenKind::Semicolon, "';'");
    }
    return commands;
}

Command Parser::parse_command() {
    if (at_end()) {
        fail_at_end("a command");
    }
    switch (peek().kind) {
        case TokenKind::KwCreate: return parse_create();
        case TokenKind::KwHarness: return parse_harness();
        case TokenKind::KwForge: return parse_forge();
        case TokenKind::KwAdd: return parse_add();
        case TokenKind::KwSeek: return parse_seek();
        case TokenKind::KwChange: return parse_change();
        case TokenKind::KwRemove: return parse_remove();
        case TokenKind::KwSave: return parse_save();
        case TokenKind::KwLoad: return parse_load();
        case TokenKind::KwExport: return parse_export();
        default: break;
    }
    fail(peek(), "a command (create, harness, forge, add, seek, change, remove, save, load, export)");
}

CreateDb Parser::parse_create() {
    expect(TokenKind::KwCreate, "'create'");
    if (check(TokenKind::KwDb) || check(TokenKind::KwDatabase)) {
        advance();
    } else {
        fail(peek(), "'db' or 'database'");
    }
    return CreateDb{expect_name("a database name")};
}

HarnessDb Parser::parse_harness() {
    expect(TokenKind::KwHarness, "'harness'");
    return HarnessDb{expect_name("a database name")};
}

ForgeTable Parser::parse_forge() {
    expect(TokenKind::KwForge, "'forge'");
    expect(TokenKind::KwTable, "'table'");
    ForgeTable command{expect_name("a table name"), {}};
    expect(TokenKind::KwWith, "'with'");
    command.columns.push_back(parse_column_def());
    while (match(TokenKind::Comma)) {
        command.columns.push_back(parse_column_def());
    }
    return command;
}

ColumnDef Parser::parse_column_def() {
    ColumnDef def{expect_name("a column name"), exdeus::core::ValueType::Text, false};
    expect(TokenKind::Colon, "':'");
    const Token& type = peek();
    switch (type.kind) {
        case TokenKind::KwInteger: def.type = exdeus::core::ValueType::Integer; break;
        case TokenKind::KwDecimal: def.type = exdeus::core::ValueType::Decimal; break;
        case TokenKind::KwText: def.type = exdeus::core::ValueType::Text; break;
        case TokenKind::KwBoolean: def.type = exdeus::core::ValueType::Boolean; break;
        default:
            fail(type, "a column type (integer, decimal, text, boolean)");
    }
    advance();
    // Optional `required` (accepted, always true in week 1) then `unique`.
    if (check(TokenKind::KwRequired)) {
        advance();
    }
    if (check(TokenKind::KwUnique)) {
        advance();
        def.unique = true;
    }
    return def;
}

AddRow Parser::parse_add() {
    expect(TokenKind::KwAdd, "'add'");
    AddRow command{expect_name("a table name"), {}};
    expect(TokenKind::LParen, "'('");
    command.values.push_back(expect_literal("a value"));
    while (match(TokenKind::Comma)) {
        command.values.push_back(expect_literal("a value"));
    }
    expect(TokenKind::RParen, "')'");
    return command;
}

SeekRows Parser::parse_seek() {
    expect(TokenKind::KwSeek, "'seek'");
    SeekRows command{expect_name("a table name"), {}, {}, "", SortDirection::None};
    if (check(TokenKind::KwShow)) {
        advance();
        command.show = parse_projection();
    }
    if (check(TokenKind::KwWhere)) {
        advance();
        command.where = parse_conditions();
    }
    if (check(TokenKind::KwOrder)) {
        advance();
        expect(TokenKind::KwBy, "'by'");
        command.order_by = expect_name("a column name");
        if (check(TokenKind::KwAscending)) {
            advance();
            command.direction = SortDirection::Ascending;
        } else if (check(TokenKind::KwDescending)) {
            advance();
            command.direction = SortDirection::Descending;
        }
    }
    return command;
}

Projection Parser::parse_projection() {
    Projection columns{expect_name("a column name")};
    while (match(TokenKind::Comma)) {
        columns.push_back(expect_name("a column name"));
    }
    return columns;
}

std::vector<Condition> Parser::parse_conditions() {
    std::vector<Condition> conditions{parse_condition()};
    while (check(TokenKind::KwAnd) || check(TokenKind::KwOr)) {
        bool is_and = peek().kind == TokenKind::KwAnd;
        advance();
        conditions.back().and_with_next = is_and;
        conditions.push_back(parse_condition());
    }
    return conditions;
}

Condition Parser::parse_condition() {
    Condition condition{expect_name("a column name"), FilterOp::Equals, exdeus::core::Value::boolean(false), true};
    const Token& op = peek();
    switch (op.kind) {
        case TokenKind::KwEquals:
        case TokenKind::Equal:
        case TokenKind::Assign:
            condition.op = FilterOp::Equals;
            break;
        case TokenKind::KwAbove:
        case TokenKind::Greater:
            condition.op = FilterOp::Above;
            break;
        case TokenKind::KwBelow:
        case TokenKind::Less:
            condition.op = FilterOp::Below;
            break;
        default:
            fail(op, "a comparison (equals, above, below, =, ==, >, <)");
    }
    advance();
    condition.literal = expect_literal("a comparison value");
    return condition;
}

ChangeRows Parser::parse_change() {
    expect(TokenKind::KwChange, "'change'");
    ChangeRows command{expect_name("a table name"), {}, {}};
    expect(TokenKind::KwSet, "'set'");
    command.set = parse_assignments();
    if (check(TokenKind::KwWhere)) {
        advance();
        command.where = parse_conditions();
    }
    return command;
}

std::vector<Assignment> Parser::parse_assignments() {
    std::vector<Assignment> items{parse_assignment()};
    while (match(TokenKind::Comma)) {
        items.push_back(parse_assignment());
    }
    return items;
}

Assignment Parser::parse_assignment() {
    Assignment item{expect_name("a column name"), false, "", true,
                    exdeus::core::Value::boolean(false)};
    expect(TokenKind::KwTo, "'to'");
    // Arithmetic form: `set balance to balance plus 500`.
    if (check(TokenKind::Identifier)) {
        const Token& source = advance();
        if (check(TokenKind::KwPlus) || check(TokenKind::KwMinus)) {
            item.is_arithmetic = true;
            item.source_column = source.lexeme;
            item.is_plus = peek().kind == TokenKind::KwPlus;
            advance();
            item.value = expect_literal("an arithmetic operand");
            return item;
        }
        fail(peek(), "'plus' or 'minus' after '" + source.lexeme + "'");
    }
    item.value = expect_literal("a value");
    return item;
}

RemoveRows Parser::parse_remove() {
    expect(TokenKind::KwRemove, "'remove'");
    RemoveRows command{expect_name("a table name"), {}};
    if (check(TokenKind::KwWhere)) {
        advance();
        command.where = parse_conditions();
    }
    return command;
}

SaveDb Parser::parse_save() {
    expect(TokenKind::KwSave, "'save'");
    if (check(TokenKind::KwDb) || check(TokenKind::KwDatabase)) {
        advance();
    } else {
        fail(peek(), "'db' or 'database'");
    }
    expect(TokenKind::KwTo, "'to'");
    return SaveDb{expect_string("a file path string")};
}

LoadDb Parser::parse_load() {
    expect(TokenKind::KwLoad, "'load'");
    if (check(TokenKind::KwDb) || check(TokenKind::KwDatabase)) {
        advance();
    } else {
        fail(peek(), "'db' or 'database'");
    }
    expect(TokenKind::KwFrom, "'from'");
    return LoadDb{expect_string("a file path string")};
}

ExportTable Parser::parse_export() {
    expect(TokenKind::KwExport, "'export'");
    ExportTable command{expect_name("a table name"), ""};
    expect(TokenKind::KwTo, "'to'");
    command.path = expect_string("a file path string");
    return command;
}

}  // namespace exdeus::language
