// Lesson 2 implementation: typed database values.
#include "exdeus/core/value.hpp"

#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace exdeus::core {

Value::Value(ValueType type, Data data) : type_(type), data_(std::move(data)) {}

Value Value::integer(long long value) {
    return Value(ValueType::Integer, Data(value));
}

Value Value::decimal(double value) {
    return Value(ValueType::Decimal, Data(value));
}

Value Value::text(std::string value) {
    return Value(ValueType::Text, Data(std::move(value)));
}

Value Value::boolean(bool value) {
    return Value(ValueType::Boolean, Data(value));
}

ValueType Value::type() const {
    return type_;
}

const Value::Data& Value::data() const {
    return data_;
}

std::string Value::to_string() const {
    return std::visit(
        [](const auto& active) -> std::string {
            using T = std::decay_t<decltype(active)>;
            if constexpr (std::is_same_v<T, std::string>) {
                return active;
            } else if constexpr (std::is_same_v<T, bool>) {
                return active ? "true" : "false";
            } else {
                return std::to_string(active);
            }
        },
        data_);
}

bool Value::operator==(const Value& other) const {
    return type_ == other.type_ && data_ == other.data_;
}

}  // namespace exdeus::core
