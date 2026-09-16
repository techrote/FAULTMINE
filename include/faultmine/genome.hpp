#pragma once

#include "faultmine/determinism.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace faultmine::core {

inline constexpr std::uint32_t kGenomeSchemaVersion = 1;
inline constexpr std::uint32_t kEngineContractVersion = 1;

enum class ParameterKind {
    boolean,
    signed_integer,
    unsigned_integer,
    text,
};

using ParameterValue = std::variant<bool, std::int64_t, std::uint64_t, std::string>;

enum class MutationDomain {
    opaque,
    toggle,
    signed_range,
    unsigned_range,
    choice,
    bitmask,
};

struct MutationMetadata {
    bool mutable_gene{true};
    std::uint32_t policy_version{1};
    MutationDomain domain{MutationDomain::opaque};
    std::int64_t signed_min{};
    std::int64_t signed_max{};
    std::int64_t signed_step{1};
    std::uint64_t unsigned_min{};
    std::uint64_t unsigned_max{};
    std::uint64_t unsigned_step{1};
    std::vector<std::string> choices;

    bool operator==(const MutationMetadata&) const = default;
};

struct ParameterDescriptor {
    std::string name;
    ParameterKind kind{ParameterKind::signed_integer};
    bool required{true};
    MutationMetadata mutation{};

    bool operator==(const ParameterDescriptor&) const = default;
};

struct OperatorDescriptor {
    std::string type_id;
    std::uint32_t minimum_supported_version{1};
    std::uint32_t current_version{1};
    std::vector<ParameterDescriptor> parameters;

    bool operator==(const OperatorDescriptor&) const = default;
};

class OperatorRegistry {
public:
    [[nodiscard]] bool register_operator(OperatorDescriptor descriptor, std::string* error = nullptr);
    [[nodiscard]] const OperatorDescriptor* find(std::string_view type_id) const noexcept;

private:
    std::vector<OperatorDescriptor> descriptors_;
};

struct OperatorInstance {
    InstanceId instance_id{};
    std::string type_id;
    std::uint32_t type_version{1};
    bool enabled{true};
    std::map<std::string, ParameterValue, std::less<>> parameters;

    bool operator==(const OperatorInstance&) const = default;
};

struct Genome {
    std::uint32_t schema_version{kGenomeSchemaVersion};
    std::uint32_t engine_contract_version{kEngineContractVersion};
    RootSeed root_seed{};
    std::vector<OperatorInstance> operators;

    bool operator==(const Genome&) const = default;
};

enum class GenomeErrorCode {
    syntax_error,
    duplicate_field,
    missing_field,
    unexpected_field,
    wrong_type,
    invalid_value,
    numeric_overflow,
    unsupported_version,
    unknown_operator,
    duplicate_instance_id,
    invalid_parameter,
};

struct GenomeError {
    GenomeErrorCode code{GenomeErrorCode::invalid_value};
    std::string path;
    std::string message;
    std::optional<std::size_t> byte_offset;
};

struct GenomeParseResult {
    std::optional<Genome> genome;
    std::optional<GenomeError> error;

    [[nodiscard]] bool ok() const noexcept {
        return genome.has_value() && !error.has_value();
    }
};

[[nodiscard]] ParameterKind parameter_kind(const ParameterValue& value) noexcept;
[[nodiscard]] std::optional<GenomeError> validate_genome(
    const Genome& genome,
    const OperatorRegistry& registry);

[[nodiscard]] std::string serialize_canonical_genome(const Genome& genome);
[[nodiscard]] GenomeParseResult parse_genome(
    std::string_view text,
    const OperatorRegistry& registry);
[[nodiscard]] std::string genome_identity_hex(const Genome& genome);

}  // namespace faultmine::core
