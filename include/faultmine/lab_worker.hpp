#pragma once

#include "faultmine/image.hpp"
#include "faultmine/laboratory.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>

namespace faultmine::laboratory {

inline constexpr std::uint32_t kLabWorkerProtocolVersion = 1U;
inline constexpr std::uint64_t kLabWorkerMaximumEncodedBytes = 64ULL * 1024ULL * 1024ULL;
inline constexpr std::uint64_t kLabWorkerMaximumPixelBytes = 256ULL * 1024ULL * 1024ULL;
inline constexpr std::uint32_t kLabWorkerMaximumDiagnosticBytes = 16U * 1024U;

enum class WorkerMode : std::uint32_t {
    decode_wic = 0U,
    synthetic_success = 1U,
    synthetic_decode_failure = 2U,
    synthetic_crash = 3U,
    synthetic_hang = 4U,
    synthetic_oversized_metadata = 5U,
    synthetic_malformed_header = 6U,
    synthetic_invalid_dimensions = 7U,
    synthetic_invalid_stride = 8U,
    synthetic_invalid_byte_count = 9U,
};

struct WorkerRequest {
    std::filesystem::path worker_executable;
    std::span<const std::uint8_t> encoded_bytes;
    WorkerMode mode{WorkerMode::decode_wic};
    std::uint32_t timeout_ms{5000U};
    std::uint64_t process_memory_limit_bytes{256ULL * 1024ULL * 1024ULL};
    std::function<bool()> should_cancel;
};

struct WorkerResult {
    core::LaboratoryOutcome outcome{core::LaboratoryOutcome::rejected_response};
    std::optional<core::ImageBuffer> image;
    std::string diagnostic;
    std::string decoder_identifier;
    std::string os_build;
    std::uint32_t exit_code{};

    [[nodiscard]] bool success() const noexcept {
        return outcome == core::LaboratoryOutcome::success && image.has_value();
    }
};

[[nodiscard]] WorkerResult run_lab_worker(const WorkerRequest& request);

}  // namespace faultmine::laboratory
