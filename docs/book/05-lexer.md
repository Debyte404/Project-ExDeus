# Lesson 5: ExdeusQL Tokens and Lexer

## Goal

Turn ExdeusQL source text into a token stream the parser can consume:

- every keyword of the week-1 vocabulary, matched case-insensitively
- integer, decimal, quoted-string, and boolean literals carrying typed `Value`s
- identifiers, punctuation (`; , : ( ) { }`), and operators (`= == != < <= > >=`)
- a 1-based line/column position on every token
- `--` line comments skipped; bad characters and unterminated strings fail loudly

This lesson introduces the language boundary: the lexer knows spellings, the
parser will know grammar, and neither knows about `Catalog` or `Table`.

## Files for this lesson

- `include/exdeus/language/token.hpp` — `Position`, `TokenKind`, `Token`.
- `include/exdeus/language/lexer.hpp` — `LexerError`, the `Lexer` class.
- `src/language/lexer.cpp` — the single-pass scanner.
- `tests/lexer_tests.cpp` — `run_lexer_tests()`, the executable proof.
- `tests/test_main.cpp` — registers `run_lexer_tests()`.

## Design target

`token.hpp` declares:

```cpp
struct Position {
    size_t line = 1;
    size_t column = 1;
};

enum class TokenKind {
    KwCreate, KwHarness, KwForge, KwTable, KwDb, KwDatabase,
    KwAdd, KwSeek, KwShow, KwWhere, KwChange, KwSet,
    KwRemove, KwSave, KwLoad, KwExport, KwFrom, KwTo,
    KwWith, KwOrder, KwBy, KwAscending, KwDescending,
    KwAnd, KwOr, KwEquals, KwAbove, KwBelow,
    KwPlus, KwMinus, KwUnique, KwRequired,
    KwInteger, KwDecimal, KwText, KwBoolean,
    IntegerLit, DecimalLit, StringLit, BoolLit, Identifier,
    Semicolon, Comma, Colon, LParen, RParen, LBrace, RBrace,
    Assign, Equal, NotEqual, Less, Greater, LessEqual, GreaterEqual,
    EndOfInput,
};

struct Token {
    TokenKind kind;
    std::string lexeme;
    Position position;
    std::optional<exdeus::core::Value> literal;
};
```

`lexer.hpp` declares:

```cpp
class LexerError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class Lexer {
public:
    explicit Lexer(std::string source);
    std::vector<Token> tokenize();
};
```

## Line-by-line connections

`Position` is 1-based: the first character of the file is line 1, column 1.
Every `Token` carries one, so the Lesson-6 parser can report `unexpected
token at line L, column C` without tracking source itself.

`TokenKind` covers the canonical week-1 vocabulary from the design spec:
`create db`, `harness`, `forge table`, `add`, `seek`, `show`, `where`,
`change`, `set`, `remove`, `save db`, `load db`, `export` — plus the
constraint words (`unique`, `required`), type words (`integer`, `decimal`,
`text`, `boolean`), filter words (`equals`, `above`, `below`, `and`, `or`),
direction words (`ascending`, `descending`, `order by`), and the arithmetic
words (`plus`, `minus`) reserved for `set` expressions.

`Token::literal` is set only for the four literal kinds and reuses
`core::Value` — the parser hands these straight to `Engine::insert`
without conversion. Keywords and punctuation leave it empty. This is the
one intentional dependency from `language` to `core`, and it flows
value-only: the lexer never includes `catalog.hpp` or `table.hpp`.

`keywords()` is a function-local static map from lowercase word to
`TokenKind`, so it is built once. `scan_word` lowercases a copy for lookup
but stores the source spelling in `lexeme` — `HARNESS Bank;` lexes as
`KwHarness` + `Identifier("Bank")`. `true`/`false` are entries in the same
map producing `BoolLit` with the boolean already packed.

`to_string(TokenKind)` gives every kind a human name (`";"`, `"integer
literal"`, `"end of input"`), which the parser's error messages reuse.

Scanning is one left-to-right pass with one character of lookahead.
`advance()` bumps `column_`, resetting to 1 on `\n`. Single-char tokens
record their `start` before consuming; `scan_string`/`scan_number`/
`scan_word` correct for the already-consumed first character with
`column_ - 1`. `=`/`!`/`<`/`>` use `match()` to decide between the one-
and two-character operators; a lone `!` fails.

`scan_string` decodes `\n`, `\t`, `\"`, `\\` and throws with the opening
quote's position on unterminated input. `scan_number` accepts digits with
exactly one optional `.fraction` — `25000.50` is a decimal, `25000.` stays
an integer `25000` plus punctuation. `skip_whitespace` also eats `--`
comments to end of line.

Error positions point at the offending character: the `default` branch
throws with the pre-`advance()` `start`, not the post-advance cursor, so
`bank@` reports column 15 (the `@`), not 16.

## How the test executable is connected

`tests/lexer_tests.cpp` exposes `void run_lexer_tests()`, and
`tests/test_main.cpp` calls it after `run_engine_tests()`. It proves, in order:

```text
create db bank; lexes to keywords + identifier + semicolon + EndOfInput
first token at line 1 column 1; identifier position tracks columns
keywords match any case and keep source spelling; database aliases db
forge header lexes names, colons, types, unique, commas
int/decimal/string/bool literals carry typed Values into literal
seek/show/where/above/order/by/descending lex as keywords
string escapes decode; = == != < <= > >= lex as distinct operators
comment lines are skipped; tokens after them land on the right line
bad character error carries line and column; unterminated string throws; lone ! throws
```

Every check inspects only the returned `std::vector<Token>`: kinds,
lexemes, positions, and the `literal` payload. Nothing reaches into the
scanner's cursor or line state.

## Build and test

Because this repository uses the Visual Studio generator, use the
configuration explicitly:

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

The red phase for this lesson was an unresolved external for every `Lexer`
member plus a missing `run_lexer_tests` symbol. Two further reds were
position bugs the tests caught: word/number scanners recorded the
post-advance column, and the bad-character error reported the cursor after
the offending char. The green phase passes `1/1 Test #1: exdeus_tests`.
The direct runner (`build\Debug\exdeus_tests.exe`) prints 21 value, 27
table, 28 engine, and 48 lexer PASS lines plus all four footers.

## Completion checkpoint

Do not continue to Lesson 6 until you can explain:

- why keywords match case-insensitively but lexemes keep source spelling
- why the literal payload reuses `core::Value` instead of raw strings
- why every token needs a position even though the lexer never reports errors by token
- why `!` alone throws but `!=` is a valid token
- why the bad-character error must use the pre-advance position
- why the lexer must never include `catalog.hpp` or `engine.hpp`
