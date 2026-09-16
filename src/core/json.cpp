#include "json.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

namespace faultmine::core::json {
namespace {

[[nodiscard]] int hex_value(const char character) noexcept {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return 10 + (character - 'a');
    }
    if (character >= 'A' && character <= 'F') {
        return 10 + (character - 'A');
    }
    return -1;
}

void append_utf8(std::string& output, const std::uint32_t code_point) {
    if (code_point <= 0x7fU) {
        output.push_back(static_cast<char>(code_point));
    } else if (code_point <= 0x7ffU) {
        output.push_back(static_cast<char>(0xc0U | (code_point >> 6U)));
        output.push_back(static_cast<char>(0x80U | (code_point & 0x3fU)));
    } else if (code_point <= 0xffffU) {
        output.push_back(static_cast<char>(0xe0U | (code_point >> 12U)));
        output.push_back(static_cast<char>(0x80U | ((code_point >> 6U) & 0x3fU)));
        output.push_back(static_cast<char>(0x80U | (code_point & 0x3fU)));
    } else {
        output.push_back(static_cast<char>(0xf0U | (code_point >> 18U)));
        output.push_back(static_cast<char>(0x80U | ((code_point >> 12U) & 0x3fU)));
        output.push_back(static_cast<char>(0x80U | ((code_point >> 6U) & 0x3fU)));
        output.push_back(static_cast<char>(0x80U | (code_point & 0x3fU)));
    }
}

class Parser {
public:
    explicit Parser(const std::string_view input) : input_(input) {}

    [[nodiscard]] ParseResult run() {
        skip_whitespace();
        auto value = parse_value(0U);
        if (!value.has_value()) {
            return ParseResult{std::nullopt, error_};
        }
        skip_whitespace();
        if (position_ != input_.size()) {
            fail(ParseErrorKind::syntax, "unexpected trailing content");
            return ParseResult{std::nullopt, error_};
        }
        return ParseResult{std::move(value), std::nullopt};
    }

private:
    [[nodiscard]] std::optional<Value> parse_value(const std::size_t depth) {
        if (depth > 64U) {
            fail(ParseErrorKind::syntax, "JSON nesting exceeds 64 levels");
            return std::nullopt;
        }
        if (position_ >= input_.size()) {
            fail(ParseErrorKind::syntax, "unexpected end of input");
            return std::nullopt;
        }

        const char character = input_[position_];
        if (character == '{') {
            return parse_object(depth + 1U);
        }
        if (character == '[') {
            return parse_array(depth + 1U);
        }
        if (character == '"') {
            auto text = parse_string();
            if (!text.has_value()) {
                return std::nullopt;
            }
            Value value;
            value.type = ValueType::string;
            value.text = std::move(*text);
            return value;
        }
        if (character == 't') {
            return parse_literal("true", ValueType::boolean, true);
        }
        if (character == 'f') {
            return parse_literal("false", ValueType::boolean, false);
        }
        if (character == 'n') {
            return parse_literal("null", ValueType::null_value, false);
        }
        if (character == '-' || (character >= '0' && character <= '9')) {
            return parse_number();
        }

        fail(ParseErrorKind::syntax, "unexpected token");
        return std::nullopt;
    }

    [[nodiscard]] std::optional<Value> parse_object(const std::size_t depth) {
        ++position_;
        skip_whitespace();

        Value value;
        value.type = ValueType::object;
        if (consume('}')) {
            return value;
        }

        while (true) {
            if (position_ >= input_.size() || input_[position_] != '"') {
                fail(ParseErrorKind::syntax, "object key must be a string");
                return std::nullopt;
            }
            const std::size_t key_offset = position_;
            auto key = parse_string();
            if (!key.has_value()) {
                return std::nullopt;
            }
            const bool duplicate = std::any_of(
                value.object.begin(),
                value.object.end(),
                [&key](const auto& member) { return member.first == *key; });
            if (duplicate) {
                position_ = key_offset;
                fail(ParseErrorKind::duplicate_key, "duplicate object key");
                return std::nullopt;
            }

            skip_whitespace();
            if (!consume(':')) {
                fail(ParseErrorKind::syntax, "expected ':' after object key");
                return std::nullopt;
            }
            skip_whitespace();
            auto member_value = parse_value(depth);
            if (!member_value.has_value()) {
                return std::nullopt;
            }
            value.object.emplace_back(std::move(*key), std::move(*member_value));

            skip_whitespace();
            if (consume('}')) {
                return value;
            }
            if (!consume(',')) {
                fail(ParseErrorKind::syntax, "expected ',' or '}' in object");
                return std::nullopt;
            }
            skip_whitespace();
        }
    }

    [[nodiscard]] std::optional<Value> parse_array(const std::size_t depth) {
        ++position_;
        skip_whitespace();

        Value value;
        value.type = ValueType::array;
        if (consume(']')) {
            return value;
        }

        while (true) {
            auto element = parse_value(depth);
            if (!element.has_value()) {
                return std::nullopt;
            }
            value.array.push_back(std::move(*element));

            skip_whitespace();
            if (consume(']')) {
                return value;
            }
            if (!consume(',')) {
                fail(ParseErrorKind::syntax, "expected ',' or ']' in array");
                return std::nullopt;
            }
            skip_whitespace();
        }
    }

    [[nodiscard]] std::optional<Value> parse_literal(
        const std::string_view literal,
        const ValueType type,
        const bool boolean_value) {
        if (input_.substr(position_, literal.size()) != literal) {
            fail(ParseErrorKind::syntax, "invalid literal");
            return std::nullopt;
        }
        position_ += literal.size();
        Value value;
        value.type = type;
        value.boolean = boolean_value;
        return value;
    }

    [[nodiscard]] std::optional<Value> parse_number() {
        const std::size_t start = position_;
        if (consume('-') && position_ >= input_.size()) {
            fail(ParseErrorKind::syntax, "incomplete number");
            return std::nullopt;
        }

        if (consume('0')) {
            if (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') {
                fail(ParseErrorKind::syntax, "leading zeros are not permitted");
                return std::nullopt;
            }
        } else {
            if (position_ >= input_.size() || input_[position_] < '1' || input_[position_] > '9') {
                fail(ParseErrorKind::syntax, "invalid number");
                return std::nullopt;
            }
            while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') {
                ++position_;
            }
        }

        if (consume('.')) {
            if (position_ >= input_.size() || input_[position_] < '0' || input_[position_] > '9') {
                fail(ParseErrorKind::syntax, "fraction requires at least one digit");
                return std::nullopt;
            }
            while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') {
                ++position_;
            }
        }

        if (position_ < input_.size() && (input_[position_] == 'e' || input_[position_] == 'E')) {
            ++position_;
            if (position_ < input_.size() && (input_[position_] == '+' || input_[position_] == '-')) {
                ++position_;
            }
            if (position_ >= input_.size() || input_[position_] < '0' || input_[position_] > '9') {
                fail(ParseErrorKind::syntax, "exponent requires at least one digit");
                return std::nullopt;
            }
            while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') {
                ++position_;
            }
        }

        Value value;
        value.type = ValueType::number;
        value.text = std::string{input_.substr(start, position_ - start)};
        return value;
    }

    [[nodiscard]] std::optional<std::string> parse_string() {
        if (!consume('"')) {
            fail(ParseErrorKind::syntax, "expected string");
            return std::nullopt;
        }

        std::string result;
        while (position_ < input_.size()) {
            const unsigned char byte = static_cast<unsigned char>(input_[position_++]);
            if (byte == static_cast<unsigned char>('"')) {
                if (!is_valid_utf8(result)) {
                    fail(ParseErrorKind::syntax, "string contains invalid UTF-8");
                    return std::nullopt;
                }
                return result;
            }
            if (byte < 0x20U) {
                fail(ParseErrorKind::syntax, "unescaped control character in string");
                return std::nullopt;
            }
            if (byte != static_cast<unsigned char>('\\')) {
                result.push_back(static_cast<char>(byte));
                continue;
            }

            if (position_ >= input_.size()) {
                fail(ParseErrorKind::syntax, "incomplete string escape");
                return std::nullopt;
            }
            const char escaped = input_[position_++];
            switch (escaped) {
                case '"': result.push_back('"'); break;
                case '\\': result.push_back('\\'); break;
                case '/': result.push_back('/'); break;
                case 'b': result.push_back('\b'); break;
                case 'f': result.push_back('\f'); break;
                case 'n': result.push_back('\n'); break;
                case 'r': result.push_back('\r'); break;
                case 't': result.push_back('\t'); break;
                case 'u': {
                    auto code_unit = parse_hex_quad();
                    if (!code_unit.has_value()) {
                        return std::nullopt;
                    }
                    std::uint32_t code_point = *code_unit;
                    if (code_point >= 0xd800U && code_point <= 0xdbffU) {
                        if (position_ + 2U > input_.size() || input_[position_] != '\\' || input_[position_ + 1U] != 'u') {
                            fail(ParseErrorKind::syntax, "high surrogate must be followed by a low surrogate");
                            return std::nullopt;
                        }
                        position_ += 2U;
                        auto low_surrogate = parse_hex_quad();
                        if (!low_surrogate.has_value() || *low_surrogate < 0xdc00U || *low_surrogate > 0xdfffU) {
                            fail(ParseErrorKind::syntax, "invalid low surrogate");
                            return std::nullopt;
                        }
                        code_point = 0x10000U + ((code_point - 0xd800U) << 10U) + (*low_surrogate - 0xdc00U);
                    } else if (code_point >= 0xdc00U && code_point <= 0xdfffU) {
                        fail(ParseErrorKind::syntax, "unexpected low surrogate");
                        return std::nullopt;
                    }
                    append_utf8(result, code_point);
                    break;
                }
                default:
                    fail(ParseErrorKind::syntax, "unsupported string escape");
                    return std::nullopt;
            }
        }

        fail(ParseErrorKind::syntax, "unterminated string");
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::uint32_t> parse_hex_quad() {
        if (position_ + 4U > input_.size()) {
            fail(ParseErrorKind::syntax, "incomplete Unicode escape");
            return std::nullopt;
        }

        std::uint32_t value = 0;
        for (std::size_t index = 0; index < 4U; ++index) {
            const int digit = hex_value(input_[position_ + index]);
            if (digit < 0) {
                fail(ParseErrorKind::syntax, "invalid Unicode escape");
                return std::nullopt;
            }
            value = (value << 4U) | static_cast<std::uint32_t>(digit);
        }
        position_ += 4U;
        return value;
    }

    void skip_whitespace() noexcept {
        while (position_ < input_.size()) {
            const char character = input_[position_];
            if (character != ' ' && character != '\t' && character != '\r' && character != '\n') {
                return;
            }
            ++position_;
        }
    }

    [[nodiscard]] bool consume(const char expected) noexcept {
        if (position_ < input_.size() && input_[position_] == expected) {
            ++position_;
            return true;
        }
        return false;
    }

    void fail(const ParseErrorKind kind, const std::string_view message) {
        if (!error_.has_value()) {
            error_ = ParseError{kind, position_, std::string{message}};
        }
    }

    std::string_view input_;
    std::size_t position_{};
    std::optional<ParseError> error_;
};

}  // namespace

ParseResult parse(const std::string_view input) {
    return Parser{input}.run();
}

bool is_valid_utf8(const std::string_view text) noexcept {
    std::size_t index = 0;
    while (index < text.size()) {
        const auto first = static_cast<unsigned char>(text[index]);
        if (first <= 0x7fU) {
            ++index;
            continue;
        }

        std::size_t length = 0;
        if (first >= 0xc2U && first <= 0xdfU) {
            length = 2U;
        } else if (first >= 0xe0U && first <= 0xefU) {
            length = 3U;
        } else if (first >= 0xf0U && first <= 0xf4U) {
            length = 4U;
        } else {
            return false;
        }
        if (index + length > text.size()) {
            return false;
        }

        const auto second = static_cast<unsigned char>(text[index + 1U]);
        if ((second & 0xc0U) != 0x80U) {
            return false;
        }
        if (length == 3U) {
            if (first == 0xe0U && second < 0xa0U) {
                return false;
            }
            if (first == 0xedU && second > 0x9fU) {
                return false;
            }
        }
        if (length == 4U) {
            if (first == 0xf0U && second < 0x90U) {
                return false;
            }
            if (first == 0xf4U && second > 0x8fU) {
                return false;
            }
        }

        for (std::size_t continuation = 2U; continuation < length; ++continuation) {
            const auto byte = static_cast<unsigned char>(text[index + continuation]);
            if ((byte & 0xc0U) != 0x80U) {
                return false;
            }
        }
        index += length;
    }
    return true;
}

std::string escape_string(const std::string_view text) {
    constexpr char kHexDigits[] = "0123456789abcdef";
    std::string result;
    result.reserve(text.size());
    for (const char character : text) {
        const auto byte = static_cast<unsigned char>(character);
        switch (character) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (byte < 0x20U) {
                    result += "\\u00";
                    result.push_back(kHexDigits[(byte >> 4U) & 0x0fU]);
                    result.push_back(kHexDigits[byte & 0x0fU]);
                } else {
                    result.push_back(character);
                }
                break;
        }
    }
    return result;
}

}  // namespace faultmine::core::json
