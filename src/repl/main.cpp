// Lesson 7: the `exdeus` interactive REPL. Reads ExdeusQL source until a
// `;` terminator, runs it through Lexer -> Parser -> Interpreter, prints
// the human-readable result, and stays alive after any command error.
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include "exdeus/language/lexer.hpp"
#include "exdeus/language/parser.hpp"
#include "exdeus/repl/highlight.hpp"
#include "exdeus/repl/interpreter.hpp"

namespace {

using exdeus::language::Command;
using exdeus::language::Position;
using exdeus::language::TokenKind;
using exdeus::repl::highlight;

// ANSI styles: pure decoration on output only, never on stored data.
// Every styled print ends with RESET so a color can never leak into
// the next line, a piped file, or the test runner's output.
constexpr const char* RESET = "\x1b[0m";
constexpr const char* BOLD = "\x1b[1m";
constexpr const char* DIM = "\x1b[2m";
constexpr const char* CYAN = "\x1b[36m";
constexpr const char* BRIGHT_CYAN = "\x1b[96m";
constexpr const char* GREEN = "\x1b[32m";
constexpr const char* BRIGHT_GREEN = "\x1b[92m";
constexpr const char* YELLOW = "\x1b[33m";
constexpr const char* RED = "\x1b[31m";
constexpr const char* BRIGHT_RED = "\x1b[91m";
constexpr const char* MAGENTA = "\x1b[35m";

void print_ok(const std::string& message) {
    std::cout << GREEN << "+ " << RESET << message << "\n";
}

void print_error(const std::string& message) {
    std::cout << BRIGHT_RED << "x Error: " << RESET << message << "\n";
}

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

void print_banner() {
    std::cout
        << "\x1b[0m""\n"
        << "  " "    " "\x1b[93m"":" "          " "\x1b[90m""::;;####;;#####;::" "          " "\x1b[93m"":" "\x1b[0m""\n"
        << "  " "    " "\x1b[93m""+:" "     " "\x1b[93m"",:#" "\x1b[90m""#;::," "          " "\x1b[90m"",::;##" "\x1b[93m"":," "     " "\x1b[93m"":+" "\x1b[0m""\n"
        << "  " "    " "\x1b[93m"";@;:,:;#:," "                    " "\x1b[97m""," "\x1b[93m"":##:,:;@;" "\x1b[0m""\n"
        << "  " "     " "\x1b[93m"":#+#;::,:" "\x1b[97m""::," "              " "\x1b[97m"",:::" "\x1b[93m"":::;###:" "\x1b[0m""\n"
        << "  " "     " "\x1b[93m"";;:;###::" "\x1b[97m"",,::," "          " "\x1b[97m"",::,,:" "\x1b[93m"":###;:;;" "\x1b[0m""\n"
        << "  " "   " "\x1b[93m"",+#," "   " "\x1b[93m"":##+" "\x1b[97m""+," " " "\x1b[97m"",::" "        " "\x1b[97m""::," " " "\x1b[97m"",++" "\x1b[93m""##:" "   " "\x1b[93m"",#+," "\x1b[0m""\n"
        << "  " "  " "\x1b[93m"":@:" "      " "\x1b[93m""+#," "\x1b[97m""+" "\x1b[91m""@#,,+:" " " "\x1b[91m"",,,," " " "\x1b[91m"":+,,;@+" "\x1b[97m""," "\x1b[93m""#+" "      " "\x1b[93m"":@:" "\x1b[0m""\n"
        << "  " " " "\x1b[93m"",@:" "       " "\x1b[93m"",@," " " "\x1b[91m""+@;;;##:,,:##;;;@+" " " "\x1b[97m""," "\x1b[93m""@," "       " "\x1b[93m"":@," "\x1b[0m""\n"
        << "  " " " "\x1b[93m""+#" "         " "\x1b[93m"";+" " " "\x1b[91m"":@#:" " " "\x1b[91m"";#:,::#;" " " "\x1b[91m"":#@;" " " "\x1b[97m""+" "\x1b[93m"";" "         " "\x1b[93m""#+" "\x1b[0m""\n"
        << "  " "\x1b[93m"":@" "           " "\x1b[93m""+" "\x1b[97m"";" " " "\x1b[91m""+:" "   " "\x1b[91m""#::::;," "  " "\x1b[91m"":+" " " "\x1b[91m"";" "\x1b[97m""+" "           " "\x1b[93m""@:" "\x1b[0m""\n"
        << "  " "\x1b[93m""+#" "           " "\x1b[31m"",#:;,:;" "\x1b[91m"":;;" "  " "\x1b[91m"";;;;" "\x1b[31m"":,;:#," "           " "\x1b[93m""#+" "\x1b[0m""\n"
        << "  " "\x1b[93m""@:" "            " "\x1b[31m"":+;,:#" "\x1b[97m""::;;:;::#" "\x1b[31m"":,;+:" "            " "\x1b[93m"":@" "\x1b[0m""\n"
        << "  " "\x1b[97m""@:" "          " "\x1b[31m"":;:#++" " " "\x1b[31m""," "   " "\x1b[97m""##" "   " "\x1b[97m""," " " "\x1b[31m""++#:;:" "          " "\x1b[97m"":@" "\x1b[0m""\n"
        << "  " "\x1b[97m""+#" "       " "\x1b[97m"",:" "\x1b[31m""##:,#;#,:" "\x1b[97m"":" " " "\x1b[97m"",::," " " "\x1b[97m""::" "\x1b[31m"",#;#,:##:" "\x1b[97m""," "       " "\x1b[97m""#+" "\x1b[0m""\n"
        << "  " "\x1b[97m"":@" "     " "\x1b[97m"",;#:" "     " "\x1b[31m"":;;:" "\x1b[97m""::" "    " "\x1b[97m"":::" "\x1b[31m"":;:" "     " "\x1b[31m"":" "\x1b[97m""#;," "     " "\x1b[97m""@:" "\x1b[0m""\n"
        << "  " " " "\x1b[97m""+#" " " "\x1b[97m"",:+@#:::::::::#," " " "\x1b[97m""," " " "\x1b[97m"",," " " "\x1b[97m""," " " "\x1b[97m"",#:::::::::#@+:," " " "\x1b[97m""#+" "\x1b[0m""\n"
        << "  " " " "\x1b[97m"",@:,:::::::::::::;" "  " "\x1b[97m"":" " " "\x1b[97m""::" " " "\x1b[97m"":" "  " "\x1b[97m"";:::::::::::::,:@," "\x1b[0m""\n"
        << "  " "  " "\x1b[97m"":@:" "             " "\x1b[97m""#;,;,::,;,;#" "             " "\x1b[97m"":@:" "\x1b[0m""\n"
        << "  " "   " "\x1b[97m"",+#" "            " "\x1b[97m"",+;;+;;+#;+," "            " "\x1b[97m""#+," "\x1b[0m""\n"
        << "  " "     " "\x1b[97m"";+:" "            " "\x1b[97m""#:@++@:#" "            " "\x1b[97m"":+;" "\x1b[0m""\n"
        << "  " "      " "\x1b[97m"",#+:," "         " "\x1b[97m""::+;;+::" "         " "\x1b[97m"",:+#," "\x1b[0m""\n"
        << "  " "        " "\x1b[90m"",;##:," "      " "\x1b[90m"",#:;;:#," "      " "\x1b[90m"",:##;," "\x1b[0m""\n"
        << "  " "           " "\x1b[90m"":;##;::," "  " "\x1b[90m"",:;;:," "  " "\x1b[90m"",::;##;:" "\x1b[0m""\n"
        << "  " "               " "\x1b[90m""::;###;#;;#;###;::" "\x1b[0m""\n"
        "\x1b[0m""\n"
        << "  " "\x1b[1;97m""E X D E U S   v0.1.0" "\x1b[0m""\n"
        << "  " "\x1b[90m""a tiny database engine, week 1" "\x1b[0m""\n"
        "\x1b[0m""\n"
        << "  " "\x1b[1;97m""Engine    " "\x1b[0m""in-memory tables, snapshots" "\x1b[0m""\n"
        << "  " "\x1b[1;97m""Language  " "\x1b[0m""ExdeusQL: lexer -> parser" "\x1b[0m""\n"
        << "  " "\x1b[1;97m""Book      " "\x1b[0m""docs/book, lessons 01-10 + 12" "\x1b[0m""\n"
        << "  " "\x1b[1;97m""REPL      " "\x1b[0m""`help` for commands, `quit`" "\x1b[0m""\n"
        << "\n";
}

void print_help() {
    // One color per job so the eye can scan: YELLOW examples, DIM notes,
    // MAGENTA comments (nothing else uses it), BRIGHT_CYAN REPL verbs.
    // Descriptions start at one column; long examples wrap with indent.
    std::cout << BOLD << "ExdeusQL" << RESET << " - groups, examples, notes:\n"
              << "\n"
              << BOLD << "  databases" << RESET << "\n"
              << "  " << YELLOW << "create db <name>;" << RESET
              << "                     make a database (does not select it)\n"
              << "  " << YELLOW << "harness <name>;" << RESET
              << "                       pick one for the commands below\n"
              << "\n"
              << BOLD << "  tables & rows" << RESET << "\n"
              << "  " << YELLOW << "forge table <t> with" << RESET << "\n"
              << "    " << YELLOW << "id: integer beelzebub, n: text not null;" << RESET << "\n"
              << "    " << DIM << "beelzebub = primary key; also unique, null," << RESET << "\n"
              << "    " << DIM << "check above 0, references parent(col)" << RESET << "\n"
              << "  " << YELLOW << "add <t> (1, \"Asha\");" << RESET
              << "                  insert one row\n"
              << "  " << YELLOW << "change <t> set a to 2 [where ...];" << RESET
              << "    change rows\n"
              << "  " << YELLOW << "remove <t> [where ...];" << RESET
              << "               delete rows\n"
              << "\n"
              << BOLD << "  queries" << RESET << "\n"
              << "  " << YELLOW << "seek <t> [show * | show a, b]" << RESET << "\n"
              << "          " << YELLOW << "[inner join <t2> on <t1.a> equals <t2.b>]" << RESET << "\n"
              << "          " << YELLOW << "[where a above 1] [order by a]" << RESET << "\n"
              << "          " << YELLOW << "[into <tmp>];" << RESET
              << "                 stash as a temp table\n"
              << "  " << YELLOW << "seek (seek <t> show a) show a;" << RESET
              << "        subquery in parens\n"
              << "  " << YELLOW << "sort <t> by <col> [asc | desc];" << RESET
              << "       sort rows, asc by default\n"
              << "\n"
              << BOLD << "  files" << RESET << "\n"
              << "  " << YELLOW << "save db to \"bank.exd\";" << RESET
              << "                snapshot the harnessed database\n"
              << "  " << YELLOW << "load db from \"bank.exd\";" << RESET
              << "              restore it (try it right now)\n"
              << "  " << YELLOW << "export <t> to \"t.csv\";" << RESET
              << "                dump one table as CSV\n"
              << "  " << YELLOW << "run \"examples/bank.exql\";" << RESET
              << "             execute a script file, line by line\n"
              << "    " << DIM << "paths are relative to launch dir; replies show the full path" << RESET << "\n"
              << "    " << DIM << "forward slashes (\"dir/f.exd\") - \\n and \\t are string escapes" << RESET << "\n"
              << "\n"
              << "  " << MAGENTA << "# like this, or -- like this" << RESET
              << "          comments, both run to end of line\n"
              << "\n"
              << "  REPL: " << BRIGHT_CYAN << "help" << RESET << "  "
              << BRIGHT_CYAN << "banner" << RESET << "  "
              << BRIGHT_CYAN << "quit" << RESET << "\n";
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
    auto print_row = [&](const std::vector<std::string>& cells, bool header) {
        for (size_t i = 0; i < cells.size(); ++i) {
            if (i > 0) {
                std::cout << DIM << " | " << RESET;
            }
            if (header) {
                std::cout << BRIGHT_CYAN << BOLD << cells[i] << RESET
                          << std::string(widths[i] - cells[i].size(), ' ');
            } else {
                std::cout << cells[i] << std::string(widths[i] - cells[i].size(), ' ');
            }
        }
        std::cout << "\n";
    };
    if (!rendered_heads.empty()) {
        print_row(rendered_heads, true);
        for (size_t i = 0; i < rendered_heads.size(); ++i) {
            if (i > 0) {
                std::cout << DIM << "-+-" << RESET;
            }
            std::cout << DIM << std::string(widths[i], '-') << RESET;
        }
        std::cout << "\n";
    }
    for (const auto& row : rendered_rows) {
        print_row(row, false);
    }
}

void print_result(const Result& result) {
    if (result.table.has_value()) {
        std::cout << BRIGHT_GREEN << BOLD << result.message << RESET << "\n";
        print_table(*result.table);
    } else {
        print_ok(result.message);
    }
}

// Failed command, replayed the way the lexer saw it: the offending line
// highlighted, a caret under the exact column, then the message. Multi-line
// input shows only the failing line, so the caret always lines up.
void print_error_with_source(const std::string& source, Position position,
                             const std::string& message) {
    std::string line;
    {
        std::istringstream lines(source);
        std::string current;
        size_t lineno = 0;
        line = source;
        while (std::getline(lines, current)) {
            ++lineno;
            if (lineno == position.line) {
                line = current;
                break;
            }
        }
    }
    std::cout << DIM << "  in: " << RESET << highlight(line) << "\n";
    std::cout << "      ";
    for (size_t i = 1; i < position.column; ++i) {
        std::cout << ' ';
    }
    std::cout << BRIGHT_RED << BOLD << "^" << RESET << "\n";
    print_error(message);
}

// Runs an .exql script file: every command executes in order through the
// same Lexer -> Parser -> Interpreter path as typed input, each result
// printed as it runs. One bad command prints its error and the script
// continues; the session always survives. Paths follow the save/load rule:
// relative to the launch directory, backslashes normalized, and the reply
// names the absolute location.
void run_script_file(Interpreter& db, const std::string& raw_path) {
    std::string path = raw_path;
    for (char& c : path) {
        if (c == '\\') {
            c = '/';
        }
    }
    std::error_code ec;
    std::string abs =
        std::filesystem::absolute(std::filesystem::path(path), ec).generic_string();
    if (ec) {
        abs = path;
    }
    std::ifstream in(std::filesystem::path(path), std::ios::binary);
    if (!in.is_open()) {
        print_error("cannot open script file '" + abs + "'");
        return;
    }
    std::ostringstream text;
    text << in.rdbuf();
    if (in.bad()) {
        print_error("cannot read script file '" + abs + "'");
        return;
    }
    try {
        Lexer lexer(text.str());
        Parser parser(lexer.tokenize());
        std::vector<Command> commands = parser.parse_all();
        for (const Command& command : commands) {
            try {
                print_result(db.execute(command));
            } catch (const std::exception& e) {
                print_error(e.what());
            }
        }
        print_ok("ran " + std::to_string(commands.size()) + " commands from " + abs);
    } catch (const LexerError& e) {
        print_error_with_source(text.str(), e.position(), e.what());
    } catch (const ParserError& e) {
        print_error_with_source(text.str(), e.position(), e.what());
    } catch (const std::exception& e) {
        print_error(e.what());
    }
}

// True when `source` is a `run "<path>";` (or `source`) line, in which case
// it executes the script file and the caller skips the normal path.
// Shape-checked on real lexer tokens, so spacing, case, and escapes all
// behave exactly like every other command.
bool try_run_file(Interpreter& db, const std::string& source) {
    std::vector<exdeus::language::Token> tokens;
    try {
        Lexer lexer(source);
        tokens = lexer.tokenize();
    } catch (const LexerError&) {
        return false;  // not a run line; the normal path reports the error
    }
    // run "<path>" ; <end> — exactly four tokens.
    if (tokens.size() != 4) {
        return false;
    }
    if (tokens[0].kind != TokenKind::Identifier) {
        return false;
    }
    std::string verb = lower(tokens[0].lexeme);
    if (verb != "run" && verb != "source") {
        return false;
    }
    if (tokens[1].kind != TokenKind::StringLit) {
        print_error("run needs a quoted path: run \"examples/bank.exql\";");
        return true;
    }
    if (tokens[2].kind != TokenKind::Semicolon) {
        return false;
    }
    run_script_file(db, std::get<std::string>(tokens[1].literal->data()));
    return true;
}

}  // namespace

int main() {
    Interpreter db;
    print_banner();

    std::string pending;
    while (true) {
        if (pending.empty()) {
            std::cout << BRIGHT_RED << BOLD << "exdeus" << RESET << DIM << "> " << RESET
                      << std::flush;
        } else {
            std::cout << DIM << "    ... " << RESET << std::flush;
        }
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
        if (pending.empty() && (word == "banner" || word == "banner;")) {
            print_banner();
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
        if (try_run_file(db, source)) {
            continue;  // script file handled (or its usage error printed)
        }
        try {
            Lexer lexer(source);
            Parser parser(lexer.tokenize());
            for (const Result& result : db.execute_all(parser.parse_all())) {
                print_result(result);
            }
        } catch (const LexerError& e) {
            print_error_with_source(source, e.position(), e.what());
        } catch (const ParserError& e) {
            print_error_with_source(source, e.position(), e.what());
        } catch (const std::exception& e) {
            // Engine, table, interpreter, and catalog errors all land here:
            // the session survives and the next command runs clean.
            print_error(e.what());
        }
    }
    std::cout << DIM << "Bye." << RESET << "\n";
    return 0;
}
