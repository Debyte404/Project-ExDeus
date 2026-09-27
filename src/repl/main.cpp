// Lesson 7: the `exdeus` interactive REPL. Reads ExdeusQL source until a
// `;` terminator, runs it through Lexer -> Parser -> Interpreter, prints
// the human-readable result, and stays alive after any command error.
#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>
#include <vector>

#include "exdeus/language/lexer.hpp"
#include "exdeus/language/parser.hpp"
#include "exdeus/repl/interpreter.hpp"

namespace {

using exdeus::language::Lexer;
using exdeus::language::LexerError;
using exdeus::language::Parser;
using exdeus::language::ParserError;
using exdeus::repl::Interpreter;
using exdeus::repl::Result;
using exdeus::repl::ResultTable;

std::string trim(const std::string& text) {
    size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin]))) {
        ++begin;
    }
    size_t end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }
    return text.substr(begin, end - begin);
}

std::string lower(std::string text) {
    for (char& c : text) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

// True when `text` holds a `;` outside a quoted string, honouring the
// lexer's `\"` and `\\` escapes. A `;` inside `"a;b"` is data, not the end
// of the command, so the REPL must keep reading.
bool has_terminator(const std::string& text) {
    bool in_string = false;
    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (in_string) {
            if (c == '\\' && i + 1 < text.size()) {
                ++i;  // skip the escaped character
            } else if (c == '"') {
                in_string = false;
            }
            continue;
        }
        if (c == '"') {
            in_string = true;
        } else if (c == ';') {
            return true;
        }
    }
    return false;
}

void print_help() {
    std::cout << "ExdeusQL commands (end every command with `;`):\n"
              << "  create db <name>;                 create a database\n"
              << "  harness <name>;                   select a database\n"
              << "  forge table <t> with a: integer, b: text;   create a table\n"
              << "  add <t> (1, \"Asha\");             insert a row\n"
              << "  seek <t> [show a, b] [where a above 1] [order by a];\n"
              << "  change <t> set a to 2 [where ...];\n"
              << "  remove <t> [where ...];\n"
              << "  save db to \"f.exd\";  load db from \"f.exd\";   save/load a snapshot\n"
              << "  export <t> to \"f.csv\";            export a table as CSV\n"
              << "REPL commands: help, quit\n";
}

void print_table(const ResultTable& table) {
    std::vector<std::string> rendered_heads = table.columns;
    std::vector<std::vector<std::string>> rendered_rows;
    for (const auto& row : table.rows) {
        std::vector<std::string> rendered;
        for (const auto& value : row) {
            rendered.push_back(value.to_string());
        }
        rendered_rows.push_back(rendered);
    }
    std::vector<size_t> widths(rendered_heads.size(), 0);
    for (size_t i = 0; i < rendered_heads.size(); ++i) {
        widths[i] = rendered_heads[i].size();
    }
    for (const auto& row : rendered_rows) {
        for (size_t i = 0; i < row.size(); ++i) {
            widths[i] = std::max(widths[i], row[i].size());
        }
    }
    auto print_row = [&](const std::vector<std::string>& cells) {
        for (size_t i = 0; i < cells.size(); ++i) {
            if (i > 0) {
                std::cout << " | ";
            }
            std::cout << cells[i] << std::string(widths[i] - cells[i].size(), ' ');
        }
        std::cout << "\n";
    };
    if (!rendered_heads.empty()) {
        print_row(rendered_heads);
        for (size_t i = 0; i < rendered_heads.size(); ++i) {
            if (i > 0) {
                std::cout << "-+-";
            }
            std::cout << std::string(widths[i], '-');
        }
        std::cout << "\n";
    }
    for (const auto& row : rendered_rows) {
        print_row(row);
    }
}

void print_result(const Result& result) {
    std::cout << result.message << "\n";
    if (result.table.has_value()) {
        print_table(*result.table);
    }
}

}  // namespace

int main() {
    Interpreter db;
    std::cout << "Exdeus REPL. Type `help` for commands, `quit` to leave.\n";

    std::string pending;
    while (true) {
        std::cout << (pending.empty() ? "exdeus> " : "    ... ") << std::flush;
        std::string line;
        if (!std::getline(std::cin, line)) {
            // EOF (Ctrl+Z / Ctrl+D): leave quietly on a fresh line.
            std::cout << "\n";
            break;
        }
        if (pending.empty() && trim(line).empty()) {
            continue;  // blank line at a fresh prompt: reprompt, no continuation
        }
        std::string command = trim(line);
        std::string word = lower(command);
        // REPL-only commands never reach the lexer; anything else
        // accumulates until the `;` terminator arrives.
        if (pending.empty() && (word == "quit" || word == "exit" || word == "quit;" ||
                                word == "exit;")) {
            break;
        }
        if (pending.empty() && (word == "help" || word == "help;")) {
            print_help();
            continue;
        }
        if (!pending.empty()) {
            pending += "\n";
        }
        pending += line;
        if (!has_terminator(pending)) {
            continue;  // multiline block: keep reading
        }
        std::string source = std::move(pending);
        pending.clear();
        try {
            Lexer lexer(source);
            Parser parser(lexer.tokenize());
            for (const Result& result : db.execute_all(parser.parse_all())) {
                print_result(result);
            }
        } catch (const LexerError& e) {
            std::cout << "Error: " << e.what() << "\n";
        } catch (const ParserError& e) {
            std::cout << "Error: " << e.what() << "\n";
        } catch (const std::exception& e) {
            // Engine, table, interpreter, and catalog errors all land here:
            // the session survives and the next command runs clean.
            std::cout << "Error: " << e.what() << "\n";
        }
    }
    std::cout << "Bye.\n";
    return 0;
}
