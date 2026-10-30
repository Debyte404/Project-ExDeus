#include <cstdlib>
#include <iostream>
#include <string>

#include "exdeus/repl/highlight.hpp"

namespace {

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        std::exit(1);
    }
    std::cout << "PASS: " << message << "\n";
}

}  // namespace

void run_highlight_tests() {
    using exdeus::repl::highlight;

    // 1. Keywords go cyan, names and punctuation stay plain.
    {
        std::string out = highlight("seek t;");
        check(out == "\x1b[36mseek\x1b[0m t;", "keyword cyan, rest verbatim");
    }

    // 2. Strings yellow, numbers magenta.
    {
        std::string out = highlight("add t (1, \"Asha\", 2.5, true, null);");
        check(out == "\x1b[36madd\x1b[0m t (\x1b[35m1\x1b[0m, \x1b[33m\"Asha\"\x1b[0m, "
                      "\x1b[35m2.5\x1b[0m, \x1b[35mtrue\x1b[0m, \x1b[35mnull\x1b[0m);",
              "literals colored by kind");
    }

    // 3. Comments dim to end of line, both styles.
    {
        std::string out = highlight("seek t; -- pick it");
        check(out == "\x1b[36mseek\x1b[0m t; \x1b[2m-- pick it\x1b[0m",
              "dash comment dims to end of line");

        std::string hash = highlight("# top\nseek t;");
        check(hash == "\x1b[2m# top\x1b[0m\n\x1b[36mseek\x1b[0m t;",
              "hash comment dims, next line still highlights");
    }

    // 4. String escapes slice exactly: hashes inside strings are data.
    {
        std::string out = highlight("add t (\"a#b \\\"q\\\" \\\\ \");");
        check(out == "\x1b[36madd\x1b[0m t (\x1b[33m\"a#b \\\"q\\\" \\\\ \"\x1b[0m);",
              "escaped string spans exactly");
    }

    // 5. Case-insensitive keywords still highlight; unknown input passes through.
    {
        std::string out = highlight("HARNESS bank;");
        check(out == "\x1b[36mHARNESS\x1b[0m bank;", "upper-case keyword highlights");

        std::string bad = highlight("seek @;");
        check(bad == "seek @;", "lexer failure returns source uncolored");
    }

    // 6. Multi-line blocks keep every line aligned.
    {
        std::string out = highlight("seek t\n  show a\n  where b above 1;");
        check(out == "\x1b[36mseek\x1b[0m t\n  \x1b[36mshow\x1b[0m a\n  "
                      "\x1b[36mwhere\x1b[0m b \x1b[36mabove\x1b[0m \x1b[35m1\x1b[0m;",
              "multi-line seek highlights per line");
    }

    std::cout << "All highlight tests passed.\n";
}
