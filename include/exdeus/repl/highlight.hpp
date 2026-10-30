#pragma once

#include <string>

namespace exdeus::repl {

// Lexer-exact syntax highlighting for ExdeusQL source. Colors real token
// spans sliced from the original text, so whitespace, string escapes, and
// comments survive verbatim:
//
//   keywords  cyan        `seek`, `where`, `beelzebub`, ...
//   strings   yellow      `"Asha"`
//   numbers + booleans + null  magenta  `25000.50`, `true`, `null`
//   comments  dim         `-- ...` and `# ...` to end of line
//   names, punctuation, operators  plain
//
// All output bytes stay <= 124 (legacy console rule); every styled span
// ends with RESET so color never leaks. On lexer failure the source is
// returned uncolored — highlighting must never break error reporting.
std::string highlight(const std::string& source);

}  // namespace exdeus::repl
