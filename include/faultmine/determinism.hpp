#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace faultmine::core {

inline constexpr std::uint32_t kEntropyContractVersion = 1;

struct RootSeed {
    std::uint64_t value{};

    [[nodiscard]] static std::optional<RootSeed> parse(std::string_view text) noexcept;
    [[nodiscard]] std::string to_string() const;

    bool operator==(const RootSeed&) const = default;
};

struct InstanceId {
    std::uint64_t high{};
    std::uint64_t low{};

    [[nodiscard]] static std::optional<InstanceId> parse(std::string_view text) noexcept;
    [[nodiscard]] std::string to_string() const;

    bool operator==(const InstanceId&) const = default;
};

[[nodiscard]] std::uint64_t mix64(std::uint64_t value) noexcept;
[[nodiscard]] std::uint64_t stable_tag_hash(std::string_view text) noexcept;

[[nodiscard]] std::uint64_t derive_stream_seed(
    RootSeed root_seed,
    InstanceId instance_id,
    std::string_view purpose_tag,
    std::span<const std::uint64_t> identity_words = {}) noexcept;

[[nodiscard]] InstanceId derive_instance_id(
    RootSeed root_seed,
    InstanceId parent_id,
    std::string_view purpose_tag,
    std::uint64_t ordinal) noexcept;

class DeterministicStream {
public:
    explicit DeterministicStream(std::uint64_t seed) noexcept;

    [[nodiscard]] std::uint64_t next_u64() noexcept;
    [[nodiscard]] std::uint64_t uniform_below(std::uint64_t upper_exclusive);
    [[nodiscard]] std::uint64_t uniform_closed(std::uint64_t minimum, std::uint64_t maximum);

private:
    std::uint64_t state_{};
};

[[nodiscard]] DeterministicStream make_named_stream(
    RootSeed root_seed,
    InstanceId instance_id,
    std::string_view purpose_tag,
    std::span<const std::uint64_t> identity_words = {}) noexcept;

}  // namespace faultmine::core
