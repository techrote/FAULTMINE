#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace faultmine::core {

using Sha256Digest = std::array<std::uint8_t, 32>;

[[nodiscard]] Sha256Digest sha256(std::span<const std::uint8_t> bytes);
[[nodiscard]] Sha256Digest sha256(std::string_view bytes);
[[nodiscard]] std::string sha256_hex(std::span<const std::uint8_t> bytes);
[[nodiscard]] std::string sha256_hex(std::string_view bytes);
[[nodiscard]] std::string digest_to_hex(const Sha256Digest& digest);

}  // namespace faultmine::core
