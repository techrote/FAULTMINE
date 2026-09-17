#include "faultmine/batch.hpp"

#include "faultmine/sha256.hpp"
#include "json.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace faultmine::core {
namespace {

inline constexpr std::uint64_t kMaximumBatchCandidates = 1'000'000U;

[[nodiscard]] bool parse_u64_text(const std::string_view text, std::uint64_t& value) noexcept {
    if (text.empty() || text.front() == '-') return false;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value, 10);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

[[nodiscard]] const json::Value* field(const json::Value& object, const std::string_view name) noexcept {
    if (object.type != json::ValueType::object) return nullptr;
    for (const auto& pair : object.object) {
        if (pair.first == name) return &pair.second;
    }
    return nullptr;
}

[[nodiscard]] bool exact_fields(
    const json::Value& object,
    const std::initializer_list<std::string_view> expected) noexcept {
    if (object.type != json::ValueType::object || object.object.size() != expected.size()) return false;
    for (const auto& pair : object.object) {
        if (std::find(expected.begin(), expected.end(), pair.first) == expected.end()) return false;
    }
    return true;
}

[[nodiscard]] bool read_u64(const json::Value& object, const std::string_view name, std::uint64_t& out) noexcept {
    const json::Value* value = field(object, name);
    return value != nullptr && value->type == json::ValueType::number && parse_u64_text(value->text, out);
}

[[nodiscard]] bool read_string(const json::Value& object, const std::string_view name, std::string& out) {
    const json::Value* value = field(object, name);
    if (value == nullptr || value->type != json::ValueType::string) return false;
    out = value->text;
    return true;
}

[[nodiscard]] bool read_bool(const json::Value& object, const std::string_view name, bool& out) noexcept {
    const json::Value* value = field(object, name);
    if (value == nullptr || value->type != json::ValueType::boolean) return false;
    out = value->boolean;
    return true;
}

[[nodiscard]] std::optional<MutationRadius> parse_radius(const std::string_view text) noexcept {
    if (text == "low") return MutationRadius::low;
    if (text == "medium") return MutationRadius::medium;
    if (text == "high") return MutationRadius::high;
    return std::nullopt;
}

[[nodiscard]] bool is_hex_identity(const std::string_view text) noexcept {
    if (text.size() != 64U) return false;
    return std::all_of(text.begin(), text.end(), [](const char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}

[[nodiscard]] std::string without_final_lf(std::string text) {
    if (!text.empty() && text.back() == '\n') text.pop_back();
    return text;
}

[[nodiscard]] std::uint32_t normalized_fraction(
    const std::uint64_t numerator,
    const std::uint64_t denominator) noexcept {
    if (denominator == 0U) return 0U;
    return static_cast<std::uint32_t>((numerator * 65535U + denominator / 2U) / denominator);
}

[[nodiscard]] std::uint8_t luminance(const std::uint8_t* pixel) noexcept {
    return static_cast<std::uint8_t>(
        (54U * static_cast<unsigned int>(pixel[0]) +
         183U * static_cast<unsigned int>(pixel[1]) +
         19U * static_cast<unsigned int>(pixel[2]) + 128U) >> 8U);
}

struct CandidateBuildResult {
    std::optional<BatchCandidate> candidate;
    std::string error;
};

[[nodiscard]] CandidateBuildResult build_candidate(
    const ImageBuffer& render_source,
    const std::string_view source_identity,
    const Genome& parent,
    const FaultRegistry& registry,
    const BatchRequest& request,
    const std::uint64_t descendant_index) {
    MutationRequest mutation;
    mutation.mutation_seed = request.mutation_seed;
    mutation.descendant_index = descendant_index;
    mutation.radius = request.radius;
    mutation.locks = request.locks;
    MutationResult child = generate_descendant(parent, registry.schema_registry(), mutation);
    if (!child.ok()) {
        std::string message = "mutation failed";
        if (child.error.has_value()) message += ": " + child.error->message;
        return {std::nullopt, std::move(message)};
    }

    const RenderResult rendered = render_pipeline_at_frame(
        render_source, *child.genome, registry, request.frame_index);
    if (!rendered.ok()) {
        std::string message = "canonical render failed";
        if (rendered.error.has_value()) message += ": " + rendered.error->message;
        return {std::nullopt, std::move(message)};
    }
    DescriptorResult descriptor = compute_visual_descriptor(*rendered.image);
    if (!descriptor.ok()) return {std::nullopt, "descriptor failed: " + descriptor.error};

    BatchCandidate candidate;
    candidate.descendant_index = descendant_index;
    candidate.genome = std::move(*child.genome);
    candidate.genome_identity = genome_identity_hex(candidate.genome);
    candidate.pixel_identity = source_identity_hex(*rendered.image);
    candidate.descriptor = std::move(*descriptor.descriptor);
    candidate.provenance = std::move(child.provenance);
    candidate.genome_representative_index = descendant_index;
    candidate.pixel_representative_index = descendant_index;
    candidate.near_representative_index = descendant_index;
    candidate.rendered_image = std::move(*rendered.image);
    candidate.candidate_identity = batch_candidate_identity(
        source_identity,
        candidate.provenance.parent_genome_identity,
        request.mutation_seed,
        request.radius,
        descendant_index,
        request.frame_index,
        request.render_mode,
        request.proxy_spec,
        candidate.genome_identity);
    return {std::move(candidate), {}};
}

[[nodiscard]] bool valid_resume_candidate(
    const BatchCandidate& cached,
    const ImageBuffer& render_source,
    const std::string_view source_identity,
    const Genome& parent,
    const FaultRegistry& registry,
    const BatchRequest& request) {
    if (cached.descendant_index < request.index_begin ||
        cached.descendant_index >= request.index_end_exclusive ||
        !cached.rendered_image.has_value()) return false;

    MutationRequest mutation;
    mutation.mutation_seed = request.mutation_seed;
    mutation.descendant_index = cached.descendant_index;
    mutation.radius = request.radius;
    mutation.locks = request.locks;
    const MutationResult regenerated = generate_descendant(parent, registry.schema_registry(), mutation);
    if (!regenerated.ok()) return false;
    const std::string regenerated_identity = genome_identity_hex(*regenerated.genome);
    if (cached.genome_identity != regenerated_identity || cached.genome != *regenerated.genome) return false;
    if (cached.provenance.parent_genome_identity != regenerated.provenance.parent_genome_identity ||
        cached.provenance.mutation_seed != request.mutation_seed ||
        cached.provenance.descendant_index != cached.descendant_index ||
        cached.provenance.radius != request.radius ||
        cached.provenance.mutation_policy_version != kMutationPolicyVersion) return false;

    const auto image_error = validate_canonical_image(*cached.rendered_image);
    if (image_error.has_value()) return false;
    if (cached.rendered_image->width != render_source.width ||
        cached.rendered_image->height != render_source.height ||
        source_identity_hex(*cached.rendered_image) != cached.pixel_identity) return false;
    const DescriptorResult descriptor = compute_visual_descriptor(*cached.rendered_image);
    if (!descriptor.ok() || *descriptor.descriptor != cached.descriptor) return false;
    return cached.candidate_identity == batch_candidate_identity(
        source_identity,
        regenerated.provenance.parent_genome_identity,
        request.mutation_seed,
        request.radius,
        cached.descendant_index,
        request.frame_index,
        request.render_mode,
        request.proxy_spec,
        cached.genome_identity);
}

void classify_and_select(BatchRunResult& result) {
    auto& candidates = result.manifest.candidates;
    std::map<std::string, std::uint64_t, std::less<>> genome_first;
    std::map<std::string, std::uint64_t, std::less<>> pixel_first;
    for (BatchCandidate& candidate : candidates) {
        const auto genome_inserted = genome_first.emplace(candidate.genome_identity, candidate.descendant_index);
        candidate.genome_representative_index = genome_inserted.first->second;
        const auto pixel_inserted = pixel_first.emplace(candidate.pixel_identity, candidate.descendant_index);
        candidate.pixel_representative_index = pixel_inserted.first->second;
        candidate.near_representative_index = candidate.descendant_index;
        candidate.selected = false;
        candidate.novelty_distance = 0U;
    }
    result.stats.unique_genomes = genome_first.size();
    result.stats.unique_pixels = pixel_first.size();

    std::vector<std::size_t> near_representatives;
    std::map<std::uint64_t, std::size_t> near_group_sizes;
    for (std::size_t index = 0U; index < candidates.size(); ++index) {
        BatchCandidate& candidate = candidates[index];
        if (candidate.pixel_representative_index != candidate.descendant_index) {
            candidate.near_representative_index = candidate.pixel_representative_index;
            continue;
        }
        bool grouped = false;
        for (const std::size_t representative_index : near_representatives) {
            const auto distance = visual_descriptor_distance(
                candidate.descriptor, candidates[representative_index].descriptor);
            if (distance.has_value() && *distance <= result.manifest.near_duplicate_threshold) {
                candidate.near_representative_index = candidates[representative_index].descendant_index;
                ++near_group_sizes[candidate.near_representative_index];
                grouped = true;
                break;
            }
        }
        if (!grouped) {
            candidate.near_representative_index = candidate.descendant_index;
            near_representatives.push_back(index);
            near_group_sizes.emplace(candidate.descendant_index, 1U);
        }
    }
    result.stats.near_duplicate_groups = static_cast<std::size_t>(std::count_if(
        near_group_sizes.begin(), near_group_sizes.end(),
        [](const auto& entry) { return entry.second > 1U; }));

    const std::size_t wanted = std::min<std::size_t>(
        static_cast<std::size_t>(result.manifest.requested_selection_count),
        near_representatives.size());
    if (wanted == 0U) return;

    std::vector<std::size_t> selected;
    selected.reserve(wanted);
    selected.push_back(near_representatives.front());
    candidates[near_representatives.front()].selected = true;
    candidates[near_representatives.front()].novelty_distance = 0U;

    while (selected.size() < wanted) {
        std::optional<std::size_t> best;
        std::uint64_t best_distance{};
        for (const std::size_t candidate_index : near_representatives) {
            if (candidates[candidate_index].selected) continue;
            std::uint64_t minimum = std::numeric_limits<std::uint64_t>::max();
            for (const std::size_t selected_index : selected) {
                const auto distance = visual_descriptor_distance(
                    candidates[candidate_index].descriptor,
                    candidates[selected_index].descriptor);
                if (!distance.has_value()) {
                    minimum = 0U;
                    break;
                }
                minimum = std::min(minimum, *distance);
            }
            if (!best.has_value() || minimum > best_distance ||
                (minimum == best_distance &&
                 candidates[candidate_index].descendant_index < candidates[*best].descendant_index) ||
                (minimum == best_distance &&
                 candidates[candidate_index].descendant_index == candidates[*best].descendant_index &&
                 candidates[candidate_index].genome_identity < candidates[*best].genome_identity)) {
                best = candidate_index;
                best_distance = minimum;
            }
        }
        if (!best.has_value()) break;
        candidates[*best].selected = true;
        candidates[*best].novelty_distance = best_distance;
        selected.push_back(*best);
    }
    result.stats.selected = selected.size();
}

[[nodiscard]] BatchManifestParseResult parse_fail(std::string message) {
    return {std::nullopt, std::move(message)};
}

}  // namespace

std::string_view batch_render_mode_name(const BatchRenderMode mode) noexcept {
    switch (mode) {
        case BatchRenderMode::canonical: return "canonical";
        case BatchRenderMode::proxy: return "proxy";
    }
    return "canonical";
}

std::optional<BatchRenderMode> parse_batch_render_mode(const std::string_view text) noexcept {
    if (text == "canonical") return BatchRenderMode::canonical;
    if (text == "proxy") return BatchRenderMode::proxy;
    return std::nullopt;
}

DescriptorResult compute_visual_descriptor(const ImageBuffer& image) {
    if (const auto error = validate_canonical_image(image); error.has_value()) {
        return {std::nullopt, error->message};
    }
    const std::uint64_t pixel_count =
        static_cast<std::uint64_t>(image.width) * static_cast<std::uint64_t>(image.height);
    if (pixel_count == 0U) return {std::nullopt, "descriptor requires at least one pixel"};

    std::array<std::uint64_t, 4U> histogram{};
    std::array<std::uint64_t, 16U> cell_sum{};
    std::array<std::uint64_t, 16U> cell_count{};
    std::vector<std::uint64_t> row_sum(image.height, 0U);
    std::uint64_t high_luminance{};
    std::uint64_t horizontal_edge_sum{};
    std::uint64_t horizontal_edges{};
    std::uint64_t vertical_edge_sum{};
    std::uint64_t vertical_edges{};

    for (std::uint32_t y = 0U; y < image.height; ++y) {
        for (std::uint32_t x = 0U; x < image.width; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * image.width + x) * 4U;
            const std::uint8_t lum = luminance(image.bytes.data() + offset);
            ++histogram[lum >> 6U];
            if (lum >= 128U) ++high_luminance;
            row_sum[y] += lum;
            const std::size_t cell_x =
                std::min<std::size_t>(3U, static_cast<std::size_t>(x) * 4U / image.width);
            const std::size_t cell_y =
                std::min<std::size_t>(3U, static_cast<std::size_t>(y) * 4U / image.height);
            const std::size_t cell = cell_y * 4U + cell_x;
            cell_sum[cell] += lum;
            ++cell_count[cell];

            if (x > 0U) {
                const std::size_t previous = offset - 4U;
                const int delta = static_cast<int>(lum) - static_cast<int>(luminance(image.bytes.data() + previous));
                horizontal_edge_sum += static_cast<std::uint64_t>(delta < 0 ? -delta : delta);
                ++horizontal_edges;
            }
            if (y > 0U) {
                const std::size_t previous = offset - static_cast<std::size_t>(image.width) * 4U;
                const int delta = static_cast<int>(lum) - static_cast<int>(luminance(image.bytes.data() + previous));
                vertical_edge_sum += static_cast<std::uint64_t>(delta < 0 ? -delta : delta);
                ++vertical_edges;
            }
        }
    }

    VisualDescriptor descriptor;
    descriptor.values.reserve(kVisualDescriptorDimensions);
    for (const std::uint64_t count : histogram) {
        descriptor.values.push_back(normalized_fraction(count, pixel_count));
    }
    for (std::size_t cell = 0U; cell < cell_sum.size(); ++cell) {
        if (cell_count[cell] == 0U) {
            descriptor.values.push_back(0U);
        } else {
            const std::uint64_t rounded =
                (cell_sum[cell] + cell_count[cell] / 2U) / cell_count[cell];
            descriptor.values.push_back(static_cast<std::uint32_t>(rounded * 257U));
        }
    }
    const auto normalized_edge = [](const std::uint64_t sum, const std::uint64_t count) {
        if (count == 0U) return std::uint32_t{0U};
        const std::uint64_t rounded = (sum + count / 2U) / count;
        return static_cast<std::uint32_t>(rounded * 257U);
    };
    descriptor.values.push_back(normalized_edge(horizontal_edge_sum, horizontal_edges));
    descriptor.values.push_back(normalized_edge(vertical_edge_sum, vertical_edges));
    descriptor.values.push_back(normalized_fraction(high_luminance, pixel_count));

    std::uint64_t equal_adjacent_rows{};
    for (std::uint32_t y = 1U; y < image.height; ++y) {
        const std::uint64_t previous_mean =
            (row_sum[y - 1U] + image.width / 2U) / image.width;
        const std::uint64_t current_mean =
            (row_sum[y] + image.width / 2U) / image.width;
        if (previous_mean == current_mean) ++equal_adjacent_rows;
    }
    descriptor.values.push_back(normalized_fraction(
        equal_adjacent_rows,
        image.height > 1U ? static_cast<std::uint64_t>(image.height - 1U) : 0U));
    if (descriptor.values.size() != kVisualDescriptorDimensions) {
        return {std::nullopt, "internal descriptor dimension mismatch"};
    }
    return {std::move(descriptor), {}};
}

std::optional<std::uint64_t> visual_descriptor_distance(
    const VisualDescriptor& left,
    const VisualDescriptor& right) noexcept {
    if (left.version != kVisualDescriptorVersion || right.version != kVisualDescriptorVersion ||
        left.values.size() != kVisualDescriptorDimensions ||
        right.values.size() != kVisualDescriptorDimensions) return std::nullopt;
    std::uint64_t distance{};
    for (std::size_t index = 0U; index < kVisualDescriptorDimensions; ++index) {
        const std::uint32_t a = left.values[index];
        const std::uint32_t b = right.values[index];
        distance += a >= b ? static_cast<std::uint64_t>(a - b) : static_cast<std::uint64_t>(b - a);
    }
    return distance;
}

std::string batch_candidate_identity(
    const std::string_view source_identity,
    const std::string_view parent_genome_identity,
    const RootSeed mutation_seed,
    const MutationRadius radius,
    const std::uint64_t descendant_index,
    const std::uint64_t frame_index,
    const BatchRenderMode render_mode,
    const ProxySpec& proxy_spec,
    const std::string_view genome_identity) {
    std::ostringstream stream;
    stream << "FAULTMINE-BATCH-CANDIDATE-v1\n"
           << source_identity << '\n'
           << parent_genome_identity << '\n'
           << mutation_seed.to_string() << '\n'
           << mutation_radius_name(radius) << '\n'
           << descendant_index << '\n'
           << frame_index << '\n'
           << batch_render_mode_name(render_mode) << '\n'
           << proxy_spec.method_version << ':' << proxy_spec.max_width << ':' << proxy_spec.max_height << '\n'
           << genome_identity << '\n';
    return sha256_hex(stream.str());
}

std::string batch_candidate_filename(const std::uint64_t descendant_index) {
    std::ostringstream stream;
    stream << "candidate_" << std::setw(20) << std::setfill('0') << descendant_index << ".png";
    return stream.str();
}

BatchRunResult run_batch(
    const ImageBuffer& source,
    const std::string_view source_identity,
    const Genome& parent,
    const FaultRegistry& registry,
    const BatchRequest& request,
    const std::vector<BatchCandidate>& resume_candidates,
    BatchCancelCallback should_cancel) {
    BatchRunResult result;
    result.manifest.source_identity = std::string{source_identity};
    result.manifest.parent_genome_identity = genome_identity_hex(parent);
    result.manifest.parent_genome = parent;
    result.manifest.mutation_seed = request.mutation_seed;
    result.manifest.radius = request.radius;
    result.manifest.index_begin = request.index_begin;
    result.manifest.index_end_exclusive = request.index_end_exclusive;
    result.manifest.frame_index = request.frame_index;
    result.manifest.render_mode = request.render_mode;
    result.manifest.proxy_spec = request.proxy_spec;
    result.manifest.near_duplicate_threshold = request.near_duplicate_threshold;
    result.manifest.requested_selection_count = request.select_count;

    if (const auto image_error = validate_canonical_image(source); image_error.has_value()) {
        result.error = "invalid source: " + image_error->message;
        return result;
    }
    if (source_identity_hex(source) != source_identity) {
        result.error = "supplied source identity does not match canonical source pixels";
        return result;
    }
    if (const auto genome_error = validate_genome(parent, registry.schema_registry()); genome_error.has_value()) {
        result.error = "invalid parent genome at " + genome_error->path + ": " + genome_error->message;
        return result;
    }
    if (request.index_end_exclusive < request.index_begin) {
        result.error = "batch index range is reversed";
        return result;
    }
    const std::uint64_t requested_u64 = request.index_end_exclusive - request.index_begin;
    if (requested_u64 > kMaximumBatchCandidates ||
        requested_u64 > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        result.error = "batch candidate count exceeds the bounded v1 limit";
        return result;
    }
    result.stats.requested = static_cast<std::size_t>(requested_u64);
    if (request.worker_count == 0U) {
        result.error = "worker count must be positive";
        return result;
    }

    ImageBuffer render_source = source;
    if (request.render_mode == BatchRenderMode::proxy) {
        const ProxyResult proxy = make_nearest_proxy(source, std::string{source_identity}, request.proxy_spec);
        if (!proxy.ok()) {
            result.error = "proxy preprocessing failed: " + proxy.error->message;
            return result;
        }
        render_source = proxy.proxy->image;
    }

    std::map<std::uint64_t, const BatchCandidate*> resume_by_index;
    for (const BatchCandidate& candidate : resume_candidates) {
        resume_by_index.emplace(candidate.descendant_index, &candidate);
    }

    std::vector<std::optional<BatchCandidate>> slots(result.stats.requested);
    std::vector<std::optional<std::string>> failures(result.stats.requested);
    std::vector<std::size_t> pending;
    pending.reserve(result.stats.requested);
    for (std::size_t offset = 0U; offset < result.stats.requested; ++offset) {
        const std::uint64_t index = request.index_begin + static_cast<std::uint64_t>(offset);
        const auto cached = resume_by_index.find(index);
        if (cached != resume_by_index.end() && valid_resume_candidate(
                *cached->second, render_source, source_identity, parent, registry, request)) {
            slots[offset] = *cached->second;
            ++result.stats.reused;
        } else {
            pending.push_back(offset);
        }
    }

    std::atomic<std::size_t> next{0U};
    std::atomic<bool> cancelled{false};
    std::mutex cancel_mutex;
    const auto worker = [&]() {
        while (!cancelled.load(std::memory_order_relaxed)) {
            const std::size_t pending_index = next.fetch_add(1U, std::memory_order_relaxed);
            if (pending_index >= pending.size()) return;
            if (should_cancel) {
                std::scoped_lock lock(cancel_mutex);
                if (should_cancel()) {
                    cancelled.store(true, std::memory_order_relaxed);
                    return;
                }
            }
            const std::size_t offset = pending[pending_index];
            const std::uint64_t index = request.index_begin + static_cast<std::uint64_t>(offset);
            CandidateBuildResult built = build_candidate(
                render_source, source_identity, parent, registry, request, index);
            if (built.candidate.has_value()) slots[offset] = std::move(*built.candidate);
            else failures[offset] = std::move(built.error);
        }
    };

    const std::size_t thread_count = std::max<std::size_t>(
        1U, std::min<std::size_t>(request.worker_count, std::max<std::size_t>(1U, pending.size())));
    std::vector<std::thread> threads;
    threads.reserve(thread_count);
    for (std::size_t index = 0U; index < thread_count; ++index) threads.emplace_back(worker);
    for (std::thread& thread : threads) thread.join();

    result.manifest.cancelled = cancelled.load(std::memory_order_relaxed);
    for (std::size_t offset = 0U; offset < slots.size(); ++offset) {
        if (slots[offset].has_value()) {
            result.manifest.candidates.push_back(std::move(*slots[offset]));
        } else if (failures[offset].has_value()) {
            result.manifest.failures.push_back(BatchFailure{
                request.index_begin + static_cast<std::uint64_t>(offset),
                std::move(*failures[offset])});
        }
    }
    result.stats.completed = result.manifest.candidates.size();
    result.stats.failed = result.manifest.failures.size();
    result.manifest.complete = !result.manifest.cancelled &&
        result.stats.failed == 0U && result.stats.completed == result.stats.requested;
    classify_and_select(result);
    return result;
}

std::string serialize_batch_manifest(const BatchManifest& manifest) {
    std::ostringstream out;
    out << "{\"manifest_version\":" << manifest.manifest_version
        << ",\"descriptor_version\":" << manifest.descriptor_version
        << ",\"mutation_policy_version\":" << kMutationPolicyVersion
        << ",\"source_identity\":\"" << json::escape_string(manifest.source_identity) << '"'
        << ",\"parent_genome_identity\":\"" << json::escape_string(manifest.parent_genome_identity) << '"'
        << ",\"parent_genome\":\"" << json::escape_string(without_final_lf(serialize_canonical_genome(manifest.parent_genome))) << '"'
        << ",\"mutation_seed\":\"" << manifest.mutation_seed.to_string() << '"'
        << ",\"radius\":\"" << mutation_radius_name(manifest.radius) << '"'
        << ",\"index_begin\":" << manifest.index_begin
        << ",\"index_end_exclusive\":" << manifest.index_end_exclusive
        << ",\"frame_index\":" << manifest.frame_index
        << ",\"render_mode\":\"" << batch_render_mode_name(manifest.render_mode) << '"'
        << ",\"proxy\":{\"method_version\":" << manifest.proxy_spec.method_version
        << ",\"max_width\":" << manifest.proxy_spec.max_width
        << ",\"max_height\":" << manifest.proxy_spec.max_height << '}'
        << ",\"near_duplicate_threshold\":" << manifest.near_duplicate_threshold
        << ",\"requested_selection_count\":" << manifest.requested_selection_count
        << ",\"complete\":" << (manifest.complete ? "true" : "false")
        << ",\"cancelled\":" << (manifest.cancelled ? "true" : "false")
        << ",\"candidates\":[";
    for (std::size_t index = 0U; index < manifest.candidates.size(); ++index) {
        if (index != 0U) out << ',';
        const BatchCandidate& candidate = manifest.candidates[index];
        out << "{\"descendant_index\":" << candidate.descendant_index
            << ",\"candidate_identity\":\"" << json::escape_string(candidate.candidate_identity) << '"'
            << ",\"genome_identity\":\"" << json::escape_string(candidate.genome_identity) << '"'
            << ",\"canonical_genome\":\"" << json::escape_string(without_final_lf(serialize_canonical_genome(candidate.genome))) << '"'
            << ",\"pixel_identity\":\"" << json::escape_string(candidate.pixel_identity) << '"'
            << ",\"descriptor\":[";
        for (std::size_t value = 0U; value < candidate.descriptor.values.size(); ++value) {
            if (value != 0U) out << ',';
            out << candidate.descriptor.values[value];
        }
        out << "]"
            << ",\"genome_representative_index\":" << candidate.genome_representative_index
            << ",\"pixel_representative_index\":" << candidate.pixel_representative_index
            << ",\"near_representative_index\":" << candidate.near_representative_index
            << ",\"selected\":" << (candidate.selected ? "true" : "false")
            << ",\"novelty_distance\":" << candidate.novelty_distance << '}';
    }
    out << "],\"failures\":[";
    for (std::size_t index = 0U; index < manifest.failures.size(); ++index) {
        if (index != 0U) out << ',';
        out << "{\"descendant_index\":" << manifest.failures[index].descendant_index
            << ",\"message\":\"" << json::escape_string(manifest.failures[index].message) << "\"}";
    }
    out << "]}\n";
    return out.str();
}

BatchManifestParseResult parse_batch_manifest(
    const std::string_view text,
    const OperatorRegistry& registry) {
    const json::ParseResult parsed = json::parse(text);
    if (!parsed.value.has_value()) {
        return parse_fail(parsed.error.has_value() ? parsed.error->message : "invalid JSON");
    }
    const json::Value& root = *parsed.value;
    if (!exact_fields(root, {
            "manifest_version", "descriptor_version", "mutation_policy_version",
            "source_identity", "parent_genome_identity", "parent_genome", "mutation_seed",
            "radius", "index_begin", "index_end_exclusive", "frame_index", "render_mode",
            "proxy", "near_duplicate_threshold", "requested_selection_count", "complete",
            "cancelled", "candidates", "failures"})) {
        return parse_fail("batch manifest has missing or unexpected top-level fields");
    }

    BatchManifest manifest;
    std::uint64_t number{};
    if (!read_u64(root, "manifest_version", number) || number != kBatchManifestVersion) {
        return parse_fail("unsupported batch manifest version");
    }
    if (!read_u64(root, "descriptor_version", number) || number != kVisualDescriptorVersion) {
        return parse_fail("unsupported descriptor version");
    }
    if (!read_u64(root, "mutation_policy_version", number) || number != kMutationPolicyVersion) {
        return parse_fail("unsupported mutation policy version");
    }
    if (!read_string(root, "source_identity", manifest.source_identity) || !is_hex_identity(manifest.source_identity)) {
        return parse_fail("invalid source identity");
    }
    if (!read_string(root, "parent_genome_identity", manifest.parent_genome_identity) ||
        !is_hex_identity(manifest.parent_genome_identity)) return parse_fail("invalid parent genome identity");
    std::string parent_text;
    if (!read_string(root, "parent_genome", parent_text)) return parse_fail("parent_genome must be canonical JSON text");
    parent_text.push_back('\n');
    const GenomeParseResult parent = parse_genome(parent_text, registry);
    if (!parent.ok()) return parse_fail("invalid parent genome in batch manifest");
    manifest.parent_genome = *parent.genome;
    if (genome_identity_hex(manifest.parent_genome) != manifest.parent_genome_identity) {
        return parse_fail("parent genome identity mismatch");
    }
    std::string seed_text;
    if (!read_string(root, "mutation_seed", seed_text)) return parse_fail("invalid mutation seed");
    const auto seed = RootSeed::parse(seed_text);
    if (!seed.has_value()) return parse_fail("invalid mutation seed");
    manifest.mutation_seed = *seed;
    std::string radius_text;
    if (!read_string(root, "radius", radius_text)) return parse_fail("invalid radius");
    const auto radius = parse_radius(radius_text);
    if (!radius.has_value()) return parse_fail("invalid radius");
    manifest.radius = *radius;
    if (!read_u64(root, "index_begin", manifest.index_begin) ||
        !read_u64(root, "index_end_exclusive", manifest.index_end_exclusive) ||
        manifest.index_end_exclusive < manifest.index_begin) return parse_fail("invalid batch index range");
    if (!read_u64(root, "frame_index", manifest.frame_index)) return parse_fail("invalid frame index");
    std::string mode_text;
    if (!read_string(root, "render_mode", mode_text)) return parse_fail("invalid render mode");
    const auto mode = parse_batch_render_mode(mode_text);
    if (!mode.has_value()) return parse_fail("invalid render mode");
    manifest.render_mode = *mode;

    const json::Value* proxy = field(root, "proxy");
    if (proxy == nullptr || !exact_fields(*proxy, {"method_version", "max_width", "max_height"})) {
        return parse_fail("invalid proxy object");
    }
    std::uint64_t method{}, width{}, height{};
    if (!read_u64(*proxy, "method_version", method) || method != kProxyMethodVersion ||
        !read_u64(*proxy, "max_width", width) || width == 0U || width > std::numeric_limits<std::uint32_t>::max() ||
        !read_u64(*proxy, "max_height", height) || height == 0U || height > std::numeric_limits<std::uint32_t>::max()) {
        return parse_fail("invalid proxy specification");
    }
    manifest.proxy_spec.method_version = static_cast<std::uint32_t>(method);
    manifest.proxy_spec.max_width = static_cast<std::uint32_t>(width);
    manifest.proxy_spec.max_height = static_cast<std::uint32_t>(height);
    if (!read_u64(root, "near_duplicate_threshold", manifest.near_duplicate_threshold) ||
        !read_u64(root, "requested_selection_count", manifest.requested_selection_count) ||
        !read_bool(root, "complete", manifest.complete) || !read_bool(root, "cancelled", manifest.cancelled)) {
        return parse_fail("invalid batch manifest scalar fields");
    }

    const json::Value* candidates = field(root, "candidates");
    if (candidates == nullptr || candidates->type != json::ValueType::array) return parse_fail("candidates must be an array");
    std::set<std::uint64_t> seen_indices;
    std::uint64_t previous_index{};
    bool first_candidate = true;
    for (const json::Value& value : candidates->array) {
        if (!exact_fields(value, {
                "descendant_index", "candidate_identity", "genome_identity", "canonical_genome",
                "pixel_identity", "descriptor", "genome_representative_index",
                "pixel_representative_index", "near_representative_index", "selected",
                "novelty_distance"})) return parse_fail("candidate has missing or unexpected fields");
        BatchCandidate candidate;
        if (!read_u64(value, "descendant_index", candidate.descendant_index) ||
            candidate.descendant_index < manifest.index_begin || candidate.descendant_index >= manifest.index_end_exclusive ||
            !seen_indices.emplace(candidate.descendant_index).second ||
            (!first_candidate && candidate.descendant_index <= previous_index)) {
            return parse_fail("candidate descendant indices must be unique, in-range and increasing");
        }
        first_candidate = false;
        previous_index = candidate.descendant_index;
        if (!read_string(value, "candidate_identity", candidate.candidate_identity) || !is_hex_identity(candidate.candidate_identity) ||
            !read_string(value, "genome_identity", candidate.genome_identity) || !is_hex_identity(candidate.genome_identity) ||
            !read_string(value, "pixel_identity", candidate.pixel_identity) || !is_hex_identity(candidate.pixel_identity)) {
            return parse_fail("candidate identity is invalid");
        }
        std::string genome_text;
        if (!read_string(value, "canonical_genome", genome_text)) return parse_fail("candidate canonical genome is missing");
        genome_text.push_back('\n');
        const GenomeParseResult genome = parse_genome(genome_text, registry);
        if (!genome.ok()) return parse_fail("candidate canonical genome is invalid");
        candidate.genome = *genome.genome;
        if (genome_identity_hex(candidate.genome) != candidate.genome_identity) return parse_fail("candidate genome identity mismatch");
        const json::Value* descriptor = field(value, "descriptor");
        if (descriptor == nullptr || descriptor->type != json::ValueType::array ||
            descriptor->array.size() != kVisualDescriptorDimensions) return parse_fail("candidate descriptor has wrong dimension");
        candidate.descriptor.version = kVisualDescriptorVersion;
        for (const json::Value& component : descriptor->array) {
            if (component.type != json::ValueType::number || !parse_u64_text(component.text, number) || number > 65535U) {
                return parse_fail("candidate descriptor component is invalid");
            }
            candidate.descriptor.values.push_back(static_cast<std::uint32_t>(number));
        }
        if (!read_u64(value, "genome_representative_index", candidate.genome_representative_index) ||
            !read_u64(value, "pixel_representative_index", candidate.pixel_representative_index) ||
            !read_u64(value, "near_representative_index", candidate.near_representative_index) ||
            !read_bool(value, "selected", candidate.selected) ||
            !read_u64(value, "novelty_distance", candidate.novelty_distance)) return parse_fail("candidate classification fields are invalid");
        candidate.provenance.mutation_policy_version = kMutationPolicyVersion;
        candidate.provenance.parent_genome_identity = manifest.parent_genome_identity;
        candidate.provenance.mutation_seed = manifest.mutation_seed;
        candidate.provenance.descendant_index = candidate.descendant_index;
        candidate.provenance.radius = manifest.radius;
        const std::string expected_candidate_identity = batch_candidate_identity(
            manifest.source_identity,
            manifest.parent_genome_identity,
            manifest.mutation_seed,
            manifest.radius,
            candidate.descendant_index,
            manifest.frame_index,
            manifest.render_mode,
            manifest.proxy_spec,
            candidate.genome_identity);
        if (candidate.candidate_identity != expected_candidate_identity) return parse_fail("candidate address identity mismatch");
        manifest.candidates.push_back(std::move(candidate));
    }

    const json::Value* failures = field(root, "failures");
    if (failures == nullptr || failures->type != json::ValueType::array) return parse_fail("failures must be an array");
    std::set<std::uint64_t> failed_indices;
    for (const json::Value& value : failures->array) {
        if (!exact_fields(value, {"descendant_index", "message"})) return parse_fail("failure has unexpected fields");
        BatchFailure failure;
        if (!read_u64(value, "descendant_index", failure.descendant_index) ||
            failure.descendant_index < manifest.index_begin || failure.descendant_index >= manifest.index_end_exclusive ||
            seen_indices.contains(failure.descendant_index) || !failed_indices.emplace(failure.descendant_index).second ||
            !read_string(value, "message", failure.message) || failure.message.empty()) return parse_fail("invalid failure record");
        manifest.failures.push_back(std::move(failure));
    }
    return {std::move(manifest), {}};
}

}  // namespace faultmine::core
