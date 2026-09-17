#include "faultmine/pipeline.hpp"

#include <utility>
#include <vector>

namespace faultmine::core {
namespace {

[[nodiscard]] std::optional<PipelineError> validate_output(
    const ImageBuffer& output,
    const ImageBuffer& source,
    const std::size_t index,
    const OperatorInstance& instance) {
    if (const auto image_error = validate_canonical_image(output); image_error.has_value()) {
        return PipelineError{
            PipelineErrorCode::execution_failed,
            index,
            instance.type_id,
            "operator returned an invalid canonical image: " + image_error->message};
    }
    if (output.width != source.width || output.height != source.height || output.format != source.format) {
        return PipelineError{
            PipelineErrorCode::execution_failed,
            index,
            instance.type_id,
            "FM-003 starter operators must preserve canonical image dimensions and format"};
    }
    return std::nullopt;
}

}  // namespace

bool FaultRegistry::register_operator(
    OperatorDescriptor descriptor,
    const OperatorExecutor executor,
    std::string* error) {
    if (executor == nullptr) {
        if (error != nullptr) {
            *error = "operator executor must not be null";
        }
        return false;
    }

    const std::string type_id = descriptor.type_id;
    if (!schemas_.register_operator(std::move(descriptor), error)) {
        return false;
    }
    executions_.push_back(ExecutionEntry{type_id, executor});
    return true;
}

bool FaultRegistry::register_temporal_operator(
    OperatorDescriptor descriptor,
    const TemporalOperatorExecutor executor,
    std::string* error) {
    if (executor == nullptr) {
        if (error != nullptr) {
            *error = "temporal operator executor must not be null";
        }
        return false;
    }

    const std::string type_id = descriptor.type_id;
    if (!schemas_.register_operator(std::move(descriptor), error)) {
        return false;
    }
    temporal_executions_.push_back(TemporalExecutionEntry{type_id, executor});
    return true;
}

OperatorRegistry& FaultRegistry::schema_registry() noexcept {
    return schemas_;
}

const OperatorRegistry& FaultRegistry::schema_registry() const noexcept {
    return schemas_;
}

OperatorExecutor FaultRegistry::find_executor(const std::string_view type_id) const noexcept {
    for (const ExecutionEntry& entry : executions_) {
        if (entry.type_id == type_id) {
            return entry.executor;
        }
    }
    return nullptr;
}

TemporalOperatorExecutor FaultRegistry::find_temporal_executor(const std::string_view type_id) const noexcept {
    for (const TemporalExecutionEntry& entry : temporal_executions_) {
        if (entry.type_id == type_id) {
            return entry.executor;
        }
    }
    return nullptr;
}

RenderResult render_pipeline(
    const ImageBuffer& source,
    const Genome& genome,
    const FaultRegistry& registry) {
    return render_pipeline_at_frame(source, genome, registry, 0U);
}

RenderResult render_pipeline_at_frame(
    const ImageBuffer& source,
    const Genome& genome,
    const FaultRegistry& registry,
    const std::uint64_t frame_index) {
    if (const auto source_error = validate_canonical_image(source); source_error.has_value()) {
        return RenderResult{
            std::nullopt,
            PipelineError{PipelineErrorCode::invalid_source, 0U, {}, source_error->message}};
    }

    if (const auto genome_error = validate_genome(genome, registry.schema_registry()); genome_error.has_value()) {
        return RenderResult{
            std::nullopt,
            PipelineError{
                PipelineErrorCode::invalid_genome,
                0U,
                {},
                genome_error->path + ": " + genome_error->message}};
    }

    std::vector<std::optional<OperatorTemporalState>> temporal_state(genome.operators.size());

    for (std::uint64_t frame = 0U;; ++frame) {
        ImageBuffer current = source;

        for (std::size_t index = 0U; index < genome.operators.size(); ++index) {
            const OperatorInstance& instance = genome.operators[index];
            if (!instance.enabled) {
                continue;
            }

            ImageBuffer output;
            if (const TemporalOperatorExecutor temporal = registry.find_temporal_executor(instance.type_id);
                temporal != nullptr) {
                OperatorTemporalState next_state;
                const OperatorTemporalState* previous =
                    temporal_state[index].has_value() ? &*temporal_state[index] : nullptr;
                const auto execution_error = temporal(
                    current,
                    output,
                    instance,
                    genome.root_seed,
                    frame,
                    previous,
                    next_state);
                if (execution_error.has_value()) {
                    return RenderResult{
                        std::nullopt,
                        PipelineError{
                            PipelineErrorCode::invalid_parameter,
                            index,
                            instance.type_id,
                            *execution_error}};
                }
                if (const auto output_error = validate_output(output, source, index, instance);
                    output_error.has_value()) {
                    return RenderResult{std::nullopt, output_error};
                }
                temporal_state[index] = std::move(next_state);
                current = std::move(output);
                continue;
            }

            const OperatorExecutor executor = registry.find_executor(instance.type_id);
            if (executor == nullptr) {
                return RenderResult{
                    std::nullopt,
                    PipelineError{
                        PipelineErrorCode::unknown_executor,
                        index,
                        instance.type_id,
                        "validated operator has no registered executor"}};
            }

            const auto execution_error = executor(current, output, instance, genome.root_seed);
            if (execution_error.has_value()) {
                return RenderResult{
                    std::nullopt,
                    PipelineError{
                        PipelineErrorCode::invalid_parameter,
                        index,
                        instance.type_id,
                        *execution_error}};
            }
            if (const auto output_error = validate_output(output, source, index, instance);
                output_error.has_value()) {
                return RenderResult{std::nullopt, output_error};
            }
            current = std::move(output);
        }

        if (frame == frame_index) {
            return RenderResult{std::move(current), std::nullopt};
        }
    }
}

}  // namespace faultmine::core
