#include <cstdlib>
#include <iostream>
#include <string>
#include <variant>

#include "exdeus/core/value.hpp"

namespace {

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        std::exit(1);
    }
    std::cout << "PASS: " << message << "\n";
}

}  // namespace

void run_value_tests() {
    using exdeus::core::Value;
    using exdeus::core::ValueType;

    // 1. Integer factory produces Integer type and preserves data.
    {
        Value v = Value::integer(7);
        check(v.type() == ValueType::Integer, "integer factory produces ValueType::Integer");
        check(std::get<long long>(v.data()) == 7, "integer factory preserves data");
    }

    // 2. Decimal factory produces Decimal type and preserves data.
    {
        Value v = Value::decimal(1.5);
        check(v.type() == ValueType::Decimal, "decimal factory produces ValueType::Decimal");
        check(std::get<double>(v.data()) == 1.5, "decimal factory preserves data");
    }

    // 3. Text factory preserves the original text.
    {
        Value v = Value::text("hello");
        check(v.type() == ValueType::Text, "text factory produces ValueType::Text");
        check(std::get<std::string>(v.data()) == "hello", "text factory preserves data");
    }

    // 4. Boolean factory preserves true and false.
    {
        Value t = Value::boolean(true);
        Value f = Value::boolean(false);
        check(t.type() == ValueType::Boolean, "boolean factory produces ValueType::Boolean");
        check(std::get<bool>(t.data()) == true, "boolean factory preserves true");
        check(std::get<bool>(f.data()) == false, "boolean factory preserves false");
    }

    // 5. Equal values compare equal.
    {
        check(Value::integer(7) == Value::integer(7), "equal integers compare equal");
        check(Value::text("a") == Value::text("a"), "equal texts compare equal");
        check(Value::boolean(true) == Value::boolean(true), "equal booleans compare equal");
    }

    // 6. Different values compare unequal.
    {
        check(!(Value::integer(7) == Value::integer(8)), "different integer data compares unequal");
        check(!(Value::integer(7) == Value::decimal(7.0)), "different type compares unequal");
        check(!(Value::text("a") == Value::text("b")), "different text compares unequal");
        check(!(Value::boolean(true) == Value::boolean(false)), "different boolean compares unequal");
    }

    // 7. to_string produces readable output.
    {
        check(Value::integer(7).to_string() == std::to_string(7), "integer to_string is readable");
        check(Value::decimal(1.5).to_string() == std::to_string(1.5), "decimal to_string is readable");
        check(Value::text("hello").to_string() == "hello", "text to_string returns itself");
        check(Value::boolean(true).to_string() == "true", "boolean true formats as true");
        check(Value::boolean(false).to_string() == "false", "boolean false formats as false");
    }

    std::cout << "All value tests passed.\n";
}
