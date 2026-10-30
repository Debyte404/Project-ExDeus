// Lexer-exact ExdeusQL highlighting. Token spans come from the real
// lexer (start/end positions), so what you see is what the parser sees.
#include "exdeus/repl/highlight.hpp"

#include <string>
#include <vector>

#include "exdeus/language/lexer.hpp"
#include "exdeus/language/token.hpp"

namespace exdeus::repl {

namespace {

using exdeus::language::Lexer;
using exdeus::language::Position;
using exdeus::language::Token;
using exdeus::language::TokenKind;

constexpr const char* RESET = "\x1b[0m";
constexpr const char* CYAN = "\x1b[36m";
constexpr const char* YELLOW = "\x1b[33m";
constexpr const char* MAGENTA = "\x1b[35m";
constexpr const char* DIM = "\x1b[2m";

const char* color_for(TokenKind kind) {
    switch (kind) {
        case TokenKind::KwCreate:
        case TokenKind::KwHarness:
        case TokenKind::KwForge:
        case TokenKind::KwTable:
        case TokenKind::KwDb:
        case TokenKind::KwDatabase:
        case TokenKind::KwAdd:
        case TokenKind::KwSeek:
        case TokenKind::KwShow:
        case TokenKind::KwWhere:
        case TokenKind::KwChange:
        case TokenKind::KwSet:
        case TokenKind::KwRemove:
        case TokenKind::KwSave:
        case TokenKind::KwLoad:
        case TokenKind::KwExport:
        case TokenKind::KwFrom:
        case TokenKind::KwTo:
        case TokenKind::KwWith:
        case TokenKind::KwOrder:
        case TokenKind::KwBy:
        case TokenKind::KwAscending:
        case TokenKind::KwDescending:
        case TokenKind::KwAsc:
        case TokenKind::KwDesc:
        case TokenKind::KwSort:
        case TokenKind::KwAlter:
        case TokenKind::KwDefault:
        case TokenKind::KwAnd:
        case TokenKind::KwOr:
        case TokenKind::KwEquals:
        case TokenKind::KwAbove:
        case TokenKind::KwBelow:
        case TokenKind::KwPlus:
        case TokenKind::KwMinus:
        case TokenKind::KwUnique:
        case TokenKind::KwRequired:
        case TokenKind::KwBeelzebub:
        case TokenKind::KwNot:
        case TokenKind::KwCheck:
        case TokenKind::KwReferences:
        case TokenKind::KwInteger:
        case TokenKind::KwDecimal:
        case TokenKind::KwText:
        case TokenKind::KwBoolean:
        case TokenKind::KwInner:
        case TokenKind::KwJoin:
        case TokenKind::KwOn:
        case TokenKind::KwInto:
            return CYAN;
        case TokenKind::StringLit:
            return YELLOW;
        case TokenKind::IntegerLit:
        case TokenKind::DecimalLit:
        case TokenKind::BoolLit:
        case TokenKind::NullLit:
            return MAGENTA;
        default:
            return nullptr;  // names, punctuation, operators: plain
    }
}

// Byte offset of a 1-based (line, column). Lines split on '\n' only;
// columns count bytes, so ASCII-only ExdeusQL maps 1:1.
size_t to_offset(const std::vector<size_t>& line_starts, const std::string& source,
                 Position pos) {
    if (pos.line < 1 || pos.line > line_starts.size()) {
        return source.size();
    }
    size_t line_end = (pos.line < line_starts.size())
                          ? line_starts[pos.line]  // start of next line == end of this
                          : source.size() + 1;     // +1 so col-1 clamps to size
    size_t off = line_starts[pos.line - 1] + (pos.column - 1);
    if (off > line_end - 1) {
        off = line_end - 1;
    }
    return off;
}

// Emit a gap (whitespace and comments only — strings are always tokens).
// A `--` or `#` starts a comment running to the end of its line.
void emit_gap(std::string& out, const std::string& source, size_t begin, size_t end) {
    size_t i = begin;
    while (i < end) {
        size_t line_end = source.find('\n', i);
        if (line_end == std::string::npos || line_end > end) {
            line_end = end;
        }
        std::string line = source.substr(i, line_end - i);
        size_t comment = std::string::npos;
        size_t dash = line.find("--");
        size_t hash = line.find('#');
        if (dash != std::string::npos) {
            comment = dash;
        }
        if (hash != std::string::npos && (comment == std::string::npos || hash < comment)) {
            comment = hash;
        }
        if (comment == std::string::npos) {
            out += line;
        } else {
            out += line.substr(0, comment);
            out += DIM;
            out += line.substr(comment);
            out += RESET;
        }
        if (line_end < end) {
            out += '\n';
            i = line_end + 1;
        } else {
            i = line_end;
        }
    }
}

}  // namespace

std::string highlight(const std::string& source) {
    std::vector<Token> tokens;
    try {
        Lexer lexer(source);
        tokens = lexer.tokenize();
    } catch (const std::exception&) {
        return source;  // never break error reporting
    }

    std::vector<size_t> line_starts{0};
    for (size_t i = 0; i < source.size(); ++i) {
        if (source[i] == '\n') {
            line_starts.push_back(i + 1);
        }
    }

    std::string out;
    size_t cursor = 0;
    for (const Token& token : tokens) {
        if (token.kind == TokenKind::EndOfInput) {
            break;
        }
        size_t begin = to_offset(line_starts, source, token.position);
        size_t end = to_offset(line_starts, source, token.end);
        if (end < begin) {
            end = begin;
        }
        if (begin < cursor) {
            begin = cursor;  // never rewind on a stale span
        }
        if (end < cursor) {
            continue;
        }
        emit_gap(out, source, cursor, begin);
        const char* color = color_for(token.kind);
        std::string span = source.substr(begin, end - begin);
        if (color) {
            out += color;
            out += span;
            out += RESET;
        } else {
            out += span;
        }
        cursor = end;
    }
    emit_gap(out, source, cursor, source.size());
    return out;
}

}  // namespace exdeus::repl
