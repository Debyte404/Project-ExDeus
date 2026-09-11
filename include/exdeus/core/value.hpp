#pragma once

// Lesson 2 exercise:
// Define the ValueType enum and Value class described in
// docs/book/02-values-and-types.md.

#include <string>
#include <variant>

namespace exdeus::core {

    enum class ValueType{
        Integer,
        Decimal,
        Text,
        Boolean,
    };

    class Value{
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
}