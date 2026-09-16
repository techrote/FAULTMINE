#include "faultmine/determinism.hpp"

#include <array>
#include <limits>
#include <stdexcept>

namespace faultmine::core {
namespace {

constexpr std::uint64_t kSplitMixIncrement = 0x9e3779b97f4a7c15ULL;
constexpr std::uint64_t kTupleDomain = 0x4641554c544d494eULL;  // "FAULTMIN"
constexpr std::uint64_t kFnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;
constexpr std::uint64_t kInstanceHighSalt = 0x49445f48495f3031ULL;  // "ID_HI_01"
constexpr std::uint64_t kInstanceLowSalt = 0x49445f4c4f5f3031ULL;   // "ID_LO_01"

[[nodiscard]] int hex_digit_value(const char character) noexcept {
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

[[nodiscard]] std::optional<std::uint64_t> parse_hex_64(const std::string_view text) noexcept {
    if (text.size() != 16U) {
        return std::nullopt;
    }

    std::uint64_t value = 0;
    for (const char character : text) {
        const int digit = hex_digit_value(character);
        if (digit < 0) {
            return std::nullopt;
        }
        value = (value << 4U) | static_cast<std::uint64_t>(digit);
    }
    return value;
}

[[nodiscard]] std::string format_hex_64(std::uint64_t value) {
    constexpr char kHexDigits[] = "0123456789abcdef";
    std::string result(16U, '0');
    for (std::size_t index = 0; index < result.size(); ++index) {
        const std::size_t shift = (result.size() - index - 1U) * 4U;
        result[index] = kHexDigits[(value >> shift) & 0x0fU];
    }
    return result;
}

}  // namespace

std::optional<RootSeed> RootSeed::parse(const std::string_view text) noexcept {
    const auto value = parse_hex_64(text);
    if (!value.has_value()) {
        return std::nullopt;
    }
    return RootSeed{*value};
}

std::string RootSeed::to_string() const {
    return format_hex_64(value);
}

std::optional<InstanceId> InstanceId::parse(const std::string_view text) noexcept {
    if (text.size() != 32U) {
        return std::nullopt;
    }

    const auto high_value = parse_hex_64(text.substr(0U, 16U));
    const auto low_value = parse_hex_64(text.substr(16U, 16U));
    if (!high_value.has_value() || !low_value.has_value()) {
        return std::nullopt;
    }
    return InstanceId{*high_value, *low_value};
}

std::string InstanceId::to_string() const {
    return format_hex_64(high) + format_hex_64(low);
}

std::uint64_t mix64(std::uint64_t value) noexcept {
    value ^= value >> 30U;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27U;
    value *= 0x94d049bb133111ebULL;
    value ^= value >> 31U;
    return value;
}

std::uint64_t stable_tag_hash(const std::string_view text) noexcept {
    std::uint64_t hash = kFnvOffset;
    for (const char character : text) {
        hash ^= static_cast<std::uint64_t>(static_cast<unsigned char>(character));
        hash *= kFnvPrime;
    }
    return hash;
}

std::uint64_t derive_stream_seed(
    const RootSeed root_seed,
    const InstanceId instance_id,
    const std::string_view purpose_tag,
    const std::span<const std::uint64_t> identity_words) noexcept {
    std::uint64_t value = mix64(root_seed.value ^ kTupleDomain);
    value = mix64(value ^ instance_id.high);
    value = mix64(value ^ instance_id.low);
    value = mix64(value ^ stable_tag_hash(purpose_tag));
    for (const std::uint64_t word : identity_words) {
        value = mix64(value ^ word);
    }
    return value;
}

InstanceId derive_instance_id(
    const RootSeed root_seed,
    const InstanceId parent_id,
    const std::string_view purpose_tag,
    const std::uint64_t ordinal) noexcept {
    const std::array<std::uint64_t, 2> high_words{ordinal, kInstanceHighSalt};
    const std::array<std::uint64_t, 2> low_words{ordinal, kInstanceLowSalt};
    return InstanceId{
        derive_stream_seed(root_seed, parent_id, purpose_tag, high_words),
        derive_stream_seed(root_seed, parent_id, purpose_tag, low_words)};
}

DeterministicStream::DeterministicStream(const std::uint64_t seed) noexcept : state_(seed) {}

std::uint64_t DeterministicStream::next_u64() noexcept {
    state_ += kSplitMixIncrement;
    return mix64(state_);
}

std::uint64_t DeterministicStream::uniform_below(const std::uint64_t upper_exclusive) {
    if (upper_exclusive == 0U) {
        throw std::invalid_argument("uniform_below requires a non-zero upper bound");
    }

    const std::uint64_t threshold = (std::uint64_t{0} - upper_exclusive) % upper_exclusive;
    while (true) {
        const std::uint64_t candidate = next_u64();
        if (candidate >= threshold) {
            return candidate % upper_exclusive;
        }
    }
}

std::uint64_t DeterministicStream::uniform_closed(
    const std::uint64_t minimum,
    const std::uint64_t maximum) {
    if (minimum > maximum) {
        throw std::invalid_argument("uniform_closed requires minimum <= maximum");
    }

    if (minimum == 0U && maximum == std::numeric_limits<std::uint64_t>::max()) {
        return next_u64();
    }

    const std::uint64_t width = (maximum - minimum) + 1U;
    return minimum + uniform_below(width);
}

DeterministicStream make_named_stream(
    const RootSeed root_seed,
    const InstanceId instance_id,
    const std::string_view purpose_tag,
    const std::span<const std::uint64_t> identity_words) noexcept {
    return DeterministicStream{derive_stream_seed(root_seed, instance_id, purpose_tag, identity_words)};
}

}  // namespace faultmine::core
