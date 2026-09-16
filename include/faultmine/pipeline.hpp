#pragma once

#include "faultmine/genome.hpp"
#include "faultmine/image.hpp"

#include <cstddef>
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

using OperatorExecutor = std::optional<std::string> (*)(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    RootSeed root_seed);

class FaultRegistry {
public:
    [[nodiscard]] bool register_operator(
        OperatorDescriptor descriptor,
        OperatorExecutor executor,
        std::string* error = nullptr);

    [[nodiscard]] const OperatorRegistry& schema_registry() const noexcept;
    [[nodiscard]] OperatorExecutor find_executor(std::string_view type_id) const noexcept;

private:
    struct ExecutionEntry {
        std::string type_id;
        OperatorExecutor executor{};
    };

    OperatorRegistry schemas_;
    std::vector<ExecutionEntry> executions_;
};

[[nodiscard]] RenderResult render_pipeline(
    const ImageBuffer& source,
    const Genome& genome,
    const FaultRegistry& registry);

}  // namespace faultmine::core
