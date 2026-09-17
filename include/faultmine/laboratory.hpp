#pragma once

#include "faultmine/determinism.hpp"
#include "faultmine/image.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace faultmine::core {

inline constexpr std::uint32_t kLaboratoryProvenanceVersion = 1U;
inline constexpr std::uint32_t kEncodedMutationPolicyVersion = 1U;
inline constexpr std::uint32_t kRawBinaryPolicyVersion = 1U;

struct EncodedMutationPlan {
    std::uint64_t protected_prefix_bytes{};
    std::uint64_t random_bit_flips{};
    std::uint64_t xor_offset{};
    std::uint64_t xor_length{};
    std::uint8_t xor_mask{};
    std::uint64_t duplicate_offset{};
    std::uint64_t duplicate_length{};
    std::uint64_t drop_offset{};
    std::uint64_t drop_length{};
    std::uint64_t maximum_output_bytes{64ULL * 1024ULL * 1024ULL};

    bool operator==(const EncodedMutationPlan&) const = default;
};

struct EncodedMutationResult {
    std::vector<std::uint8_t> bytes;
    std::string original_identity;
    std::string mutated_identity;
};

[[nodiscard]] std::optional<EncodedMutationResult> mutate_encoded_bytes(
    std::span<const std::uint8_t> input,
    const EncodedMutationPlan& plan,
    RootSeed seed,
    std::string* error = nullptr);

[[nodiscard]] std::string encoded_mutation_plan_text(const EncodedMutationPlan& plan);

// Arbitrary binary data is interpreted in the canonical core. It does not use
// the external-decoder worker merely because the bytes are unusual.
enum class RawBinaryBoundary {
    wrap,
    fill,
    drop,
};

struct RawBinarySpec {
    std::uint32_t width{256U};
    std::uint32_t height{};  // zero derives a bounded height from the input/stride
    std::uint64_t offset{};
    std::uint64_t stride{};  // zero means tightly packed width * bytes_per_pixel
    std::uint32_t bytes_per_pixel{1U};  // 1=gray, 2=RG, 3=RGB, 4=RGBA
    RawBinaryBoundary boundary{RawBinaryBoundary::wrap};
    std::uint8_t fill_byte{};
    std::uint32_t maximum_dimension{16384U};

    bool operator==(const RawBinarySpec&) const = default;
};

struct RawBinaryResult {
    ImageBuffer image;
    std::string source_identity;
};

[[nodiscard]] std::optional<RawBinaryResult> interpret_raw_binary(
    std::span<const std::uint8_t> input,
    const RawBinarySpec& spec,
    std::string* error = nullptr);

[[nodiscard]] std::string_view raw_binary_boundary_name(RawBinaryBoundary boundary) noexcept;

// External-decoder results are explicitly non-canonical until their validated
// normalized pixels are frozen/materialized into this structure.
enum class LaboratoryOutcome {
    success,
    decode_failure,
    worker_crash,
    timeout,
    cancelled,
    rejected_response,
};

[[nodiscard]] std::string_view laboratory_outcome_name(LaboratoryOutcome outcome) noexcept;
[[nodiscard]] std::optional<LaboratoryOutcome> parse_laboratory_outcome(std::string_view text) noexcept;

struct LaboratoryProvenance {
    std::uint32_t provenance_version{kLaboratoryProvenanceVersion};
    std::string original_encoded_identity;
    std::string mutated_encoded_identity;
    std::string mutation_seed;
    std::string mutation_parameters;
    std::uint32_t worker_protocol_version{1U};
    std::string worker_application_version;
    std::string decoder_identifier;
    std::string os_build;
    LaboratoryOutcome outcome{LaboratoryOutcome::success};
    std::string diagnostic;
    std::string materialized_source_identity;

    bool operator==(const LaboratoryProvenance&) const = default;
};

struct MaterializedLaboratorySource {
    ImageBuffer image;
    LaboratoryProvenance provenance;

    bool operator==(const MaterializedLaboratorySource&) const = default;
};

[[nodiscard]] bool validate_laboratory_provenance(
    const LaboratoryProvenance& provenance,
    std::string* error = nullptr);
[[nodiscard]] bool validate_materialized_laboratory_source(
    const MaterializedLaboratorySource& source,
    std::string* error = nullptr);

[[nodiscard]] std::string serialize_laboratory_provenance_json(const LaboratoryProvenance& provenance);
[[nodiscard]] std::optional<LaboratoryProvenance> parse_laboratory_provenance_json(
    std::string_view text,
    std::string* error = nullptr);

[[nodiscard]] std::string bytes_to_hex(std::span<const std::uint8_t> bytes);
[[nodiscard]] std::optional<std::vector<std::uint8_t>> bytes_from_hex(
    std::string_view text,
    std::uint64_t maximum_bytes,
    std::string* error = nullptr);

}  // namespace faultmine::core
