#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace faultmine::core::json {

enum class ValueType {
    null_value,
    boolean,
    number,
    string,
    array,
    object,
};

struct Value {
    ValueType type{ValueType::null_value};
    bool boolean{};
    std::string text;
    std::vector<Value> array;
    std::vector<std::pair<std::string, Value>> object;
};

enum class ParseErrorKind {
    syntax,
    duplicate_key,
};

struct ParseError {
    ParseErrorKind kind{ParseErrorKind::syntax};
    std::size_t offset{};
    std::string message;
};

struct ParseResult {
    std::optional<Value> value;
    std::optional<ParseError> error;
};

[[nodiscard]] ParseResult parse(std::string_view input);
[[nodiscard]] bool is_valid_utf8(std::string_view text) noexcept;
[[nodiscard]] std::string escape_string(std::string_view text);

}  // namespace faultmine::core::json
