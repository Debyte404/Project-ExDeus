# Lesson 2: Values and Types

## Goal

Represent the four values that week 1 needs:

- integer
- decimal
- text
- boolean

This lesson introduces a central database idea: a table column has a declared type, and every value inserted into that column must respect it.

## Files for this lesson

- `include/exdeus/core/value.hpp` — declarations visible to the rest of the engine.
- `src/core/value.cpp` — definitions of behavior declared in the header.
- `tests/value_tests.cpp` — tests that describe the behavior before you implement it.

The header currently contains only an exercise marker. You are going to write the first real production declarations.

## Design target

Write the following design in `include/exdeus/core/value.hpp`, then implement it in `src/core/value.cpp`:

```cpp
#pragma once

#include <string>
#include <variant>

namespace exdeus::core {

enum class ValueType {
    Integer,
    Decimal,
    Text,
    Boolean,
};

class Value {
public:
    using Data = std::variant<long long, double, std::string, bool>;

    static Value integer(long long value);
    static Value decimal(double value);
    static Value text(std::string value);
    static Value boolean(bool value);

    ValueType type() const;
    const Data& data() const;
    std::string to_string() const;

    bool operator==(const Value& other) const;

private:
    Value(ValueType type, Data data);

    ValueType type_;
    Data data_;
};

} // namespace exdeus::core
```

Do not paste this blindly. Read what each part means, type it yourself, and ask the compiler to reveal mistakes.

## Line-by-line connections

`#pragma once` prevents a header from being included more than once in one translation unit. This is a compiler instruction, not a C++ object.

`#include <string>` gives us `std::string`. The header needs it because `Value` publicly names strings.

`#include <variant>` gives us `std::variant`, a type-safe union. A `Value` stores exactly one alternative from `long long`, `double`, `std::string`, or `bool`.

`namespace exdeus::core` prevents names such as `Value` from colliding with another library. The namespace also mirrors the folder responsibility: this is core engine code, not parser code.

`enum class ValueType` is a closed list of types. `enum class` is scoped, so the spelling is `ValueType::Integer` instead of a loose global `Integer`.

`class Value` owns one typed database value. The class is intentionally small because later classes—columns, rows, tables, snapshots, and the parser—will all depend on it.

`using Data = ...` gives the long variant type a readable name. This is a type alias, not a new runtime object.

The four static factory functions make construction readable. `Value::integer(7)` communicates intent better than exposing how the variant is initialized.

The `const` after `type()`, `data()`, and `to_string()` promises that reading a value does not modify it. This will matter when a table validates many rows without accidentally changing them.

The private constructor forces callers through the named factories. The implementation owns the invariant that `ValueType::Integer` must be paired with a `long long`, and so on.

## Your implementation tasks

Complete these in order:

1. Add the includes, namespace, enum, class, alias, declarations, and private fields to `value.hpp`.
2. Define the private constructor in `value.cpp`.
3. Define the four factory functions.
4. Define `type()` and `data()`.
5. Define `to_string()` using `std::visit` or an equivalent type-safe approach.
6. Define equality.

Do not add null values, dates, automatic conversions, or arithmetic yet. Those are separate design decisions.

## Test-first exercise

Before writing the definitions, add tests to `tests/value_tests.cpp` that prove:

```text
an integer factory produces ValueType::Integer
a decimal factory produces ValueType::Decimal
a text factory preserves the original text
a boolean factory preserves true and false
equal values compare equal
different values compare unequal
to_string produces readable output
```

You may use the existing test executable as a temporary place to call these checks, or improve the test runner with a tiny `EXPECT` helper. Keep the helper project-owned and transparent; do not add a testing library yet.

## Build and test

Because this repository uses the Visual Studio generator, use the configuration explicitly:

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Your first run after adding tests should fail because the behavior is not implemented. That is the red phase. Implement the smallest missing behavior, rebuild, and run the tests again.

## Completion checkpoint

Do not continue to Lesson 3 until you can explain:

- why `Value` is a class instead of four unrelated functions
- why `std::variant` is safer than a `void*`
- why the type tag and variant alternative must agree
- why the table will depend on `Value`, but `Value` must not depend on `Table`
- how a test failure led you to the next implementation step
