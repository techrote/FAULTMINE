#pragma once

#include "faultmine/laboratory.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>

namespace faultmine::laboratory {

enum class WorkerSyntheticMode : std::uint32_t {
    none = 0U,
    success = 1U,
    decode_failure = 2U,
    crash = 3U,
    hang = 4U,
    oversized_payload = 5U,
    malformed_response = 6U,
    invalid_dimensions = 7U,
    invalid_stride = 8U,
    invalid_byte_count = 9U,
};

struct WorkerOptions {
    std::filesystem::path executable;
    std::uint32_t timeout_ms{5000U};
    std::uint64_t job_memory_limit_bytes{512ULL * 1024ULL * 1024ULL};
    WorkerSyntheticMode synthetic_mode{WorkerSyntheticMode::none};
    std::function<bool()> should_cancel;
};

struct WorkerResult {
    LaboratoryProvenance provenance;
    std::optional<MaterializedSource> materialized;
    std::optional<std::uint32_t> exit_code;
    std::string host_error;

    [[nodiscard]] bool ok() const noexcept {
        return materialized.has_value() && provenance.outcome == Outcome::success && host_error.empty();
    }
};

[[nodiscard]] WorkerResult run_decoder_worker(
    std::span<const std::uint8_t> mutated_encoded_bytes,
    std::string original_encoded_identity,
    std::string mutation_recipe,
    core::RootSeed seed,
    const WorkerOptions& options);

}  // namespace faultmine::laboratory
