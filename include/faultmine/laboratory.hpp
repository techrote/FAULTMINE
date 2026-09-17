#pragma once

#include "faultmine/determinism.hpp"
#include "faultmine/image.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace faultmine::laboratory {

inline constexpr std::uint32_t kLaboratoryProtocolVersion = 1U;
inline constexpr std::size_t kMaxLaboratoryEncodedBytes = 64U * 1024U * 1024U;
inline constexpr std::size_t kMaxLaboratoryPixelBytes = 256U * 1024U * 1024U;
inline constexpr std::size_t kMaxLaboratoryDiagnosticBytes = 4096U;

enum class Outcome {
    success,
    decode_failure,
    worker_crash,
    timeout,
    cancelled,
    rejected_response,
};

[[nodiscard]] std::string_view outcome_name(Outcome outcome) noexcept;
[[nodiscard]] std::optional<Outcome> parse_outcome(std::string_view text) noexcept;

enum class ByteMutationKind {
    bit_flip,
    xor_range,
    duplicate_range,
    drop_range,
};

struct ByteMutationOperation {
    ByteMutationKind kind{ByteMutationKind::bit_flip};
    std::uint64_t offset{};
    std::uint64_t length{};
    std::uint64_t value{};
    std::uint64_t count{1U};

    bool operator==(const ByteMutationOperation&) const = default;
};

struct ByteMutationPlan {
    core::RootSeed seed{};
    std::uint64_t protected_prefix{};
    std::vector<ByteMutationOperation> operations;

    bool operator==(const ByteMutationPlan&) const = default;
};

struct ByteMutationResult {
    std::vector<std::uint8_t> bytes;
    std::string original_identity;
    std::string mutated_identity;
    std::string canonical_recipe;
    std::string error;

    [[nodiscard]] bool ok() const noexcept { return error.empty(); }
};

[[nodiscard]] std::string serialize_byte_mutation_plan(const ByteMutationPlan& plan);
[[nodiscard]] ByteMutationResult mutate_encoded_bytes(
    std::span<const std::uint8_t> input,
    const ByteMutationPlan& plan);

enum class RawFormat {
    gray8,
    rgb8,
    rgba8,
    bgra8,
};

enum class RawBoundaryPolicy {
    drop,
    wrap,
    fill,
};

struct RawBinarySpec {
    std::uint32_t width{};
    std::uint32_t height{}; // zero derives a finite height from the available bytes
    std::uint64_t offset{};
    std::uint64_t stride{}; // zero means width * bytes_per_pixel
    RawFormat format{RawFormat::rgba8};
    RawBoundaryPolicy boundary{RawBoundaryPolicy::drop};
    std::uint8_t fill_byte{};

    bool operator==(const RawBinarySpec&) const = default;
};

struct RawInterpretResult {
    std::optional<core::ImageBuffer> image;
    std::string error;

    [[nodiscard]] bool ok() const noexcept { return image.has_value() && error.empty(); }
};

[[nodiscard]] RawInterpretResult interpret_binary_as_image(
    std::span<const std::uint8_t> bytes,
    const RawBinarySpec& spec);

struct LaboratoryProvenance {
    std::string encoded_input_identity;
    std::string mutated_input_identity;
    std::string mutation_recipe;
    std::string root_seed;
    std::uint32_t worker_protocol_version{kLaboratoryProtocolVersion};
    std::string worker_application_version;
    std::string os_metadata;
    std::string decoder_metadata;
    Outcome outcome{Outcome::decode_failure};
    std::string diagnostic;
    std::optional<std::string> materialized_source_identity;

    bool operator==(const LaboratoryProvenance&) const = default;
};

struct MaterializedSource {
    core::ImageBuffer image;
    LaboratoryProvenance provenance;

    bool operator==(const MaterializedSource&) const = default;
};

struct LaboratoryParseResult {
    std::optional<LaboratoryProvenance> provenance;
    std::string error;

    [[nodiscard]] bool ok() const noexcept { return provenance.has_value() && error.empty(); }
};

struct MaterializedParseResult {
    std::optional<MaterializedSource> source;
    std::string error;

    [[nodiscard]] bool ok() const noexcept { return source.has_value() && error.empty(); }
};

[[nodiscard]] std::string serialize_laboratory_provenance(const LaboratoryProvenance& provenance);
[[nodiscard]] LaboratoryParseResult parse_laboratory_provenance(std::string_view text);
[[nodiscard]] std::string serialize_materialized_source(const MaterializedSource& source);
[[nodiscard]] MaterializedParseResult parse_materialized_source(std::string_view text);
[[nodiscard]] std::optional<std::string> validate_materialized_source(const MaterializedSource& source);

}  // namespace faultmine::laboratory
