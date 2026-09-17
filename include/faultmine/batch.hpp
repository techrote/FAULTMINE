#pragma once

#include "faultmine/image.hpp"
#include "faultmine/mutation.hpp"
#include "faultmine/pipeline.hpp"
#include "faultmine/proxy.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace faultmine::core {

inline constexpr std::uint32_t kBatchManifestVersion = 1U;
inline constexpr std::uint32_t kVisualDescriptorVersion = 1U;
inline constexpr std::size_t kVisualDescriptorDimensions = 24U;
inline constexpr std::uint64_t kNoRepresentativeIndex = ~std::uint64_t{0U};

enum class BatchRenderMode : std::uint8_t {
    canonical = 0U,
    proxy = 1U,
};

[[nodiscard]] std::string_view batch_render_mode_name(BatchRenderMode mode) noexcept;
[[nodiscard]] std::optional<BatchRenderMode> parse_batch_render_mode(std::string_view text) noexcept;

struct VisualDescriptor {
    std::uint32_t version{kVisualDescriptorVersion};
    std::vector<std::uint32_t> values;

    bool operator==(const VisualDescriptor&) const = default;
};

struct DescriptorResult {
    std::optional<VisualDescriptor> descriptor;
    std::string error;

    [[nodiscard]] bool ok() const noexcept {
        return descriptor.has_value() && error.empty();
    }
};

[[nodiscard]] DescriptorResult compute_visual_descriptor(const ImageBuffer& image);
[[nodiscard]] std::optional<std::uint64_t> visual_descriptor_distance(
    const VisualDescriptor& left,
    const VisualDescriptor& right) noexcept;

struct BatchCandidate {
    std::uint64_t descendant_index{};
    std::string candidate_identity;
    Genome genome;
    std::string genome_identity;
    std::string pixel_identity;
    VisualDescriptor descriptor;
    DescendantProvenance provenance;
    std::uint64_t genome_representative_index{kNoRepresentativeIndex};
    std::uint64_t pixel_representative_index{kNoRepresentativeIndex};
    std::uint64_t near_representative_index{kNoRepresentativeIndex};
    bool selected{};
    std::uint64_t novelty_distance{};
    std::optional<ImageBuffer> rendered_image;

    bool operator==(const BatchCandidate&) const = default;
};

struct BatchFailure {
    std::uint64_t descendant_index{};
    std::string message;

    bool operator==(const BatchFailure&) const = default;
};

struct BatchRequest {
    RootSeed mutation_seed{};
    MutationRadius radius{MutationRadius::medium};
    MutationLocks locks;
    std::uint64_t index_begin{};
    std::uint64_t index_end_exclusive{};
    std::uint64_t frame_index{};
    BatchRenderMode render_mode{BatchRenderMode::canonical};
    ProxySpec proxy_spec{};
    std::uint64_t near_duplicate_threshold{};
    std::size_t select_count{16U};
    std::size_t worker_count{1U};
};

struct BatchRunStats {
    std::size_t requested{};
    std::size_t completed{};
    std::size_t reused{};
    std::size_t failed{};
    std::size_t unique_genomes{};
    std::size_t unique_pixels{};
    std::size_t near_duplicate_groups{};
    std::size_t selected{};

    bool operator==(const BatchRunStats&) const = default;
};

struct BatchManifest {
    std::uint32_t manifest_version{kBatchManifestVersion};
    std::uint32_t descriptor_version{kVisualDescriptorVersion};
    std::string source_identity;
    std::string parent_genome_identity;
    Genome parent_genome;
    RootSeed mutation_seed{};
    MutationRadius radius{MutationRadius::medium};
    std::uint64_t index_begin{};
    std::uint64_t index_end_exclusive{};
    std::uint64_t frame_index{};
    BatchRenderMode render_mode{BatchRenderMode::canonical};
    ProxySpec proxy_spec{};
    std::uint64_t near_duplicate_threshold{};
    std::uint64_t requested_selection_count{};
    bool complete{};
    bool cancelled{};
    std::vector<BatchCandidate> candidates;
    std::vector<BatchFailure> failures;

    bool operator==(const BatchManifest&) const = default;
};

struct BatchRunResult {
    BatchManifest manifest;
    BatchRunStats stats;
    std::string error;

    [[nodiscard]] bool ok() const noexcept { return error.empty(); }
};

using BatchCancelCallback = std::function<bool()>;

// Candidate generation is independently addressed by descendant_index. Worker
// count changes throughput only. Resume records are accepted only after their
// address/genome/pixel/descriptor identities are revalidated.
[[nodiscard]] BatchRunResult run_batch(
    const ImageBuffer& source,
    std::string_view source_identity,
    const Genome& parent,
    const FaultRegistry& registry,
    const BatchRequest& request,
    const std::vector<BatchCandidate>& resume_candidates = {},
    BatchCancelCallback should_cancel = {});

[[nodiscard]] std::string batch_candidate_identity(
    std::string_view source_identity,
    std::string_view parent_genome_identity,
    RootSeed mutation_seed,
    MutationRadius radius,
    std::uint64_t descendant_index,
    std::uint64_t frame_index,
    BatchRenderMode render_mode,
    const ProxySpec& proxy_spec,
    std::string_view genome_identity);

[[nodiscard]] std::string batch_candidate_filename(std::uint64_t descendant_index);

[[nodiscard]] std::string serialize_batch_manifest(const BatchManifest& manifest);

struct BatchManifestParseResult {
    std::optional<BatchManifest> manifest;
    std::string error;

    [[nodiscard]] bool ok() const noexcept {
        return manifest.has_value() && error.empty();
    }
};

[[nodiscard]] BatchManifestParseResult parse_batch_manifest(
    std::string_view text,
    const OperatorRegistry& registry);

}  // namespace faultmine::core
