#pragma once

#include "faultmine/genome.hpp"
#include "faultmine/image.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace faultmine::core {

enum class PipelineErrorCode {
    invalid_source,
    invalid_genome,
    unknown_executor,
    invalid_parameter,
    execution_failed,
};

struct PipelineError {
    PipelineErrorCode code{PipelineErrorCode::execution_failed};
    std::size_t operator_index{};
    std::string operator_type;
    std::string message;
};

struct RenderResult {
    std::optional<ImageBuffer> image;
    std::optional<PipelineError> error;

    [[nodiscard]] bool ok() const noexcept {
        return image.has_value() && !error.has_value();
    }
};

// Explicit state retained for one temporal operator between canonical ticks.
// v1 feedback operators use the previous operator output as their state.
struct OperatorTemporalState {
    std::optional<ImageBuffer> previous_output;
};

using OperatorExecutor = std::optional<std::string> (*)(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    RootSeed root_seed);

using TemporalOperatorExecutor = std::optional<std::string> (*)(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    RootSeed root_seed,
    std::uint64_t frame_index,
    const OperatorTemporalState* previous_state,
    OperatorTemporalState& next_state);

class FaultRegistry {
public:
    [[nodiscard]] bool register_operator(
        OperatorDescriptor descriptor,
        OperatorExecutor executor,
        std::string* error = nullptr);
    [[nodiscard]] bool register_temporal_operator(
        OperatorDescriptor descriptor,
        TemporalOperatorExecutor executor,
        std::string* error = nullptr);

    [[nodiscard]] OperatorRegistry& schema_registry() noexcept;
    [[nodiscard]] const OperatorRegistry& schema_registry() const noexcept;
    [[nodiscard]] OperatorExecutor find_executor(std::string_view type_id) const noexcept;
    [[nodiscard]] TemporalOperatorExecutor find_temporal_executor(std::string_view type_id) const noexcept;

private:
    struct ExecutionEntry {
        std::string type_id;
        OperatorExecutor executor{};
    };
    struct TemporalExecutionEntry {
        std::string type_id;
        TemporalOperatorExecutor executor{};
    };

    OperatorRegistry schemas_;
    std::vector<ExecutionEntry> executions_;
    std::vector<TemporalExecutionEntry> temporal_executions_;
};

[[nodiscard]] RenderResult render_pipeline(
    const ImageBuffer& source,
    const Genome& genome,
    const FaultRegistry& registry);

// Canonical temporal entry point. Frame N is rendered by deterministic replay
// from the defined frame-0 state. Checkpoint acceleration may be added later
// only if byte-equivalent to this reference implementation.
[[nodiscard]] RenderResult render_pipeline_at_frame(
    const ImageBuffer& source,
    const Genome& genome,
    const FaultRegistry& registry,
    std::uint64_t frame_index);

}  // namespace faultmine::core
