#include "faultmine/pipeline.hpp"

#include <utility>

namespace faultmine::core {

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

RenderResult render_pipeline(
    const ImageBuffer& source,
    const Genome& genome,
    const FaultRegistry& registry) {
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

    ImageBuffer current = source;
    for (std::size_t index = 0; index < genome.operators.size(); ++index) {
        const OperatorInstance& instance = genome.operators[index];
        if (!instance.enabled) {
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

        ImageBuffer output;
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
        if (const auto image_error = validate_canonical_image(output); image_error.has_value()) {
            return RenderResult{
                std::nullopt,
                PipelineError{
                    PipelineErrorCode::execution_failed,
                    index,
                    instance.type_id,
                    "operator returned an invalid canonical image: " + image_error->message}};
        }
        if (output.width != source.width || output.height != source.height || output.format != source.format) {
            return RenderResult{
                std::nullopt,
                PipelineError{
                    PipelineErrorCode::execution_failed,
                    index,
                    instance.type_id,
                    "FM-003 starter operators must preserve canonical image dimensions and format"}};
        }
        current = std::move(output);
    }

    return RenderResult{std::move(current), std::nullopt};
}

}  // namespace faultmine::core
