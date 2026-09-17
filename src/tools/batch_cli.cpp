#include "faultmine/batch.hpp"
#include "faultmine/export.hpp"
#include "faultmine/fault_catalogue.hpp"
#include "faultmine/genome.hpp"
#include "faultmine/project.hpp"
#include "faultmine/session.hpp"
#include "faultmine/wic_io.hpp"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

std::atomic<bool> g_cancel_requested{false};

BOOL WINAPI console_control_handler(const DWORD type) {
    if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT) {
        g_cancel_requested.store(true, std::memory_order_relaxed);
        return TRUE;
    }
    return FALSE;
}

struct Options {
    std::filesystem::path source;
    std::filesystem::path genome;
    std::filesystem::path project;
    std::filesystem::path output;
    std::optional<faultmine::core::RootSeed> mutation_seed;
    faultmine::core::MutationRadius radius{faultmine::core::MutationRadius::medium};
    std::uint64_t begin{};
    std::optional<std::uint64_t> count;
    std::uint64_t frame{};
    faultmine::core::BatchRenderMode render_mode{faultmine::core::BatchRenderMode::canonical};
    faultmine::core::ProxySpec proxy{};
    std::uint64_t near_threshold{65535U};
    std::size_t select_count{16U};
    std::size_t threads{std::max<unsigned int>(1U, std::thread::hardware_concurrency())};
    bool resume{};
    bool selected_stills{};
    bool contact_sheet{true};
};

struct InputState {
    faultmine::core::ImageBuffer source;
    std::string source_identity;
    std::filesystem::path source_path;
    faultmine::core::Genome parent;
    faultmine::core::MutationLocks locks;
    std::optional<faultmine::app::ProjectDocument> project;
};

[[nodiscard]] std::string read_text(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return {};
    return std::string{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
}

[[nodiscard]] bool write_text_atomic(
    const std::filesystem::path& path,
    const std::string_view text,
    std::string& error) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        error = "cannot create output directory: " + ec.message();
        return false;
    }
    std::filesystem::path temporary = path;
    temporary += L".tmp";
    for (std::uint32_t suffix = 0U; std::filesystem::exists(temporary, ec) && suffix < 1000U; ++suffix) {
        temporary = path;
        temporary += L".tmp." + std::to_wstring(suffix + 1U);
    }
    std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
    if (!stream) {
        error = "cannot open temporary output";
        return false;
    }
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    stream.close();
    if (!stream) {
        std::filesystem::remove(temporary, ec);
        error = "failed writing temporary output";
        return false;
    }
    if (!MoveFileExW(
            temporary.c_str(), path.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const DWORD code = GetLastError();
        std::filesystem::remove(temporary, ec);
        error = "failed committing output (Win32 error " + std::to_string(code) + ')';
        return false;
    }
    return true;
}

[[nodiscard]] std::string narrow_ascii(const std::wstring_view text) {
    std::string result;
    result.reserve(text.size());
    for (const wchar_t c : text) {
        if (c < 0 || c > 127) return {};
        result.push_back(static_cast<char>(c));
    }
    return result;
}

[[nodiscard]] bool parse_u64(const std::wstring_view text, std::uint64_t& value) {
    const std::string narrow = narrow_ascii(text);
    if (narrow.empty()) return false;
    const auto parsed = std::from_chars(narrow.data(), narrow.data() + narrow.size(), value, 10);
    return parsed.ec == std::errc{} && parsed.ptr == narrow.data() + narrow.size();
}

[[nodiscard]] bool parse_size(const std::wstring_view text, std::size_t& value) {
    std::uint64_t parsed{};
    if (!parse_u64(text, parsed) || parsed > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) return false;
    value = static_cast<std::size_t>(parsed);
    return true;
}

void usage() {
    std::cerr
        << "FAULTMINE-batch - deterministic headless mutation/diversity miner\n\n"
        << "Input (choose one):\n"
        << "  --source <image> --genome <genome.json>\n"
        << "  --project <project.fmproj> [--source <relinked-image>]\n\n"
        << "Required mining arguments:\n"
        << "  --out <directory> --seed <16-hex> --count <N>\n\n"
        << "Options:\n"
        << "  --begin <N>                  first descendant index (default 0)\n"
        << "  --radius low|medium|high     mutation radius (default medium)\n"
        << "  --frame <N>                  explicit semantic frame (default 0)\n"
        << "  --render canonical|proxy     descriptor render mode (default canonical)\n"
        << "  --proxy-width <N> --proxy-height <N>\n"
        << "  --near-threshold <N>         inclusive descriptor L1 threshold\n"
        << "  --select <N>                 diversity-selected candidates (default 16)\n"
        << "  --threads <N>                worker count; semantics are thread-independent\n"
        << "  --resume                     verify/reuse matching candidate cache\n"
        << "  --selected-stills            export canonical selected stills + manifests\n"
        << "  --no-contact-sheet           skip canonical selected contact sheet\n\n"
        << "Ctrl+C writes a coherent partial manifest and returns a cancellation exit code.\n";
}

[[nodiscard]] bool parse_options(const int argc, wchar_t* argv[], Options& options, std::string& error) {
    for (int index = 1; index < argc; ++index) {
        const std::wstring_view arg{argv[index]};
        const auto value = [&](const char* name) -> std::optional<std::wstring_view> {
            if (index + 1 >= argc) {
                error = std::string{name} + " requires a value";
                return std::nullopt;
            }
            ++index;
            return std::wstring_view{argv[index]};
        };
        if (arg == L"--help" || arg == L"-h") return false;
        if (arg == L"--source") {
            const auto next = value("--source"); if (!next) return false; options.source = std::filesystem::path{*next};
        } else if (arg == L"--genome") {
            const auto next = value("--genome"); if (!next) return false; options.genome = std::filesystem::path{*next};
        } else if (arg == L"--project") {
            const auto next = value("--project"); if (!next) return false; options.project = std::filesystem::path{*next};
        } else if (arg == L"--out") {
            const auto next = value("--out"); if (!next) return false; options.output = std::filesystem::path{*next};
        } else if (arg == L"--seed") {
            const auto next = value("--seed"); if (!next) return false;
            const std::string seed_text = narrow_ascii(*next);
            options.mutation_seed = faultmine::core::RootSeed::parse(seed_text);
            if (!options.mutation_seed.has_value()) { error = "--seed must be exactly 16 hexadecimal digits"; return false; }
        } else if (arg == L"--radius") {
            const auto next = value("--radius"); if (!next) return false;
            const std::string radius = narrow_ascii(*next);
            if (radius == "low") options.radius = faultmine::core::MutationRadius::low;
            else if (radius == "medium") options.radius = faultmine::core::MutationRadius::medium;
            else if (radius == "high") options.radius = faultmine::core::MutationRadius::high;
            else { error = "--radius must be low, medium, or high"; return false; }
        } else if (arg == L"--begin") {
            const auto next = value("--begin"); if (!next || !parse_u64(*next, options.begin)) { error = "invalid --begin"; return false; }
        } else if (arg == L"--count") {
            const auto next = value("--count"); std::uint64_t count{};
            if (!next || !parse_u64(*next, count) || count == 0U) { error = "--count must be positive"; return false; }
            options.count = count;
        } else if (arg == L"--frame") {
            const auto next = value("--frame"); if (!next || !parse_u64(*next, options.frame)) { error = "invalid --frame"; return false; }
        } else if (arg == L"--render") {
            const auto next = value("--render"); if (!next) return false;
            const auto mode = faultmine::core::parse_batch_render_mode(narrow_ascii(*next));
            if (!mode.has_value()) { error = "--render must be canonical or proxy"; return false; }
            options.render_mode = *mode;
        } else if (arg == L"--proxy-width") {
            const auto next = value("--proxy-width"); std::uint64_t parsed{};
            if (!next || !parse_u64(*next, parsed) || parsed == 0U || parsed > std::numeric_limits<std::uint32_t>::max()) { error = "invalid --proxy-width"; return false; }
            options.proxy.max_width = static_cast<std::uint32_t>(parsed);
        } else if (arg == L"--proxy-height") {
            const auto next = value("--proxy-height"); std::uint64_t parsed{};
            if (!next || !parse_u64(*next, parsed) || parsed == 0U || parsed > std::numeric_limits<std::uint32_t>::max()) { error = "invalid --proxy-height"; return false; }
            options.proxy.max_height = static_cast<std::uint32_t>(parsed);
        } else if (arg == L"--near-threshold") {
            const auto next = value("--near-threshold"); if (!next || !parse_u64(*next, options.near_threshold)) { error = "invalid --near-threshold"; return false; }
        } else if (arg == L"--select") {
            const auto next = value("--select"); if (!next || !parse_size(*next, options.select_count)) { error = "invalid --select"; return false; }
        } else if (arg == L"--threads") {
            const auto next = value("--threads"); if (!next || !parse_size(*next, options.threads) || options.threads == 0U) { error = "--threads must be positive"; return false; }
        } else if (arg == L"--resume") {
            options.resume = true;
        } else if (arg == L"--selected-stills") {
            options.selected_stills = true;
        } else if (arg == L"--no-contact-sheet") {
            options.contact_sheet = false;
        } else {
            error = "unknown argument: " + narrow_ascii(arg);
            return false;
        }
    }
    if (options.output.empty()) { error = "--out is required"; return false; }
    if (!options.mutation_seed.has_value()) { error = "--seed is required"; return false; }
    if (!options.count.has_value()) { error = "--count is required"; return false; }
    const bool has_project = !options.project.empty();
    const bool has_genome = !options.genome.empty();
    if (has_project == has_genome) { error = "choose exactly one of --project or --genome"; return false; }
    if (has_genome && options.source.empty()) { error = "--source is required with --genome"; return false; }
    if (*options.count > std::numeric_limits<std::uint64_t>::max() - options.begin) { error = "batch index range overflows uint64"; return false; }
    return true;
}

[[nodiscard]] faultmine::core::MutationLocks mutation_locks_from_project(const faultmine::app::LockState& locks) {
    faultmine::core::MutationLocks result;
    result.operators = locks.operators;
    for (const auto& lock : locks.parameters) {
        result.parameters.push_back(faultmine::core::MutationParameterLock{lock.instance_id, lock.parameter});
    }
    return result;
}

[[nodiscard]] std::optional<InputState> load_input(
    const Options& options,
    const faultmine::core::FaultRegistry& registry,
    std::string& error) {
    InputState state;
    if (!options.project.empty()) {
        const std::string text = read_text(options.project);
        if (text.empty()) { error = "failed to read non-empty project"; return std::nullopt; }
        const auto parsed = faultmine::app::parse_project(text, registry.schema_registry());
        if (!parsed.ok()) {
            error = "project parse failed";
            if (parsed.error.has_value()) error += " at " + parsed.error->path + ": " + parsed.error->message;
            return std::nullopt;
        }
        state.project = *parsed.project;
        state.parent = parsed.project->genome;
        state.locks = mutation_locks_from_project(parsed.project->locks);
        state.source_identity = parsed.project->source.source_identity;
        if (parsed.project->source.laboratory.has_value()) {
            state.source = parsed.project->source.laboratory->image;
            state.source_path = options.source.empty()
                ? std::filesystem::path{parsed.project->source.path_utf8}
                : options.source;
            return state;
        }
        state.source_path = options.source.empty()
            ? std::filesystem::path{parsed.project->source.path_utf8}
            : options.source;
        const auto loaded = faultmine::io::load_wic_image(state.source_path);
        if (!loaded.ok()) {
            error = "project source load failed";
            if (loaded.error.has_value()) error += ": " + loaded.error->message;
            return std::nullopt;
        }
        if (loaded.source->source_identity != state.source_identity) {
            error = "project source identity mismatch; explicit relink must contain identical normalized pixels";
            return std::nullopt;
        }
        state.source = loaded.source->image;
        return state;
    }

    const std::string genome_text = read_text(options.genome);
    if (genome_text.empty()) { error = "failed to read non-empty genome"; return std::nullopt; }
    const auto parsed_genome = faultmine::core::parse_genome(genome_text, registry.schema_registry());
    if (!parsed_genome.ok()) {
        error = "genome parse failed";
        if (parsed_genome.error.has_value()) error += " at " + parsed_genome.error->path + ": " + parsed_genome.error->message;
        return std::nullopt;
    }
    const auto loaded = faultmine::io::load_wic_image(options.source);
    if (!loaded.ok()) {
        error = "source load failed";
        if (loaded.error.has_value()) error += ": " + loaded.error->message;
        return std::nullopt;
    }
    state.source = loaded.source->image;
    state.source_identity = loaded.source->source_identity;
    state.source_path = loaded.source->path;
    state.parent = *parsed_genome.genome;
    return state;
}

[[nodiscard]] bool manifest_matches_request(
    const faultmine::core::BatchManifest& manifest,
    const InputState& input,
    const faultmine::core::BatchRequest& request) {
    return manifest.source_identity == input.source_identity &&
        manifest.parent_genome_identity == faultmine::core::genome_identity_hex(input.parent) &&
        manifest.parent_genome == input.parent &&
        manifest.mutation_seed == request.mutation_seed &&
        manifest.radius == request.radius &&
        manifest.index_begin == request.index_begin &&
        manifest.index_end_exclusive == request.index_end_exclusive &&
        manifest.frame_index == request.frame_index &&
        manifest.render_mode == request.render_mode &&
        manifest.proxy_spec == request.proxy_spec &&
        manifest.near_duplicate_threshold == request.near_duplicate_threshold &&
        manifest.requested_selection_count == request.select_count;
}

[[nodiscard]] std::vector<faultmine::core::BatchCandidate> load_resume_candidates(
    const Options& options,
    const InputState& input,
    const faultmine::core::FaultRegistry& registry,
    const faultmine::core::BatchRequest& request,
    std::string& error) {
    std::vector<faultmine::core::BatchCandidate> candidates;
    if (!options.resume) return candidates;
    const std::filesystem::path manifest_path = options.output / L"batch.fmbatch.json";
    if (!std::filesystem::exists(manifest_path)) return candidates;
    const std::string text = read_text(manifest_path);
    const auto parsed = faultmine::core::parse_batch_manifest(text, registry.schema_registry());
    if (!parsed.ok()) {
        error = "resume manifest is invalid: " + parsed.error;
        return {};
    }
    if (!manifest_matches_request(*parsed.manifest, input, request)) {
        error = "resume manifest does not match the current source/genome/range/policy/render request";
        return {};
    }
    const std::filesystem::path cache_directory = options.output / L"candidates";
    for (auto candidate : parsed.manifest->candidates) {
        const std::filesystem::path png = cache_directory / faultmine::core::batch_candidate_filename(candidate.descendant_index);
        const auto loaded = faultmine::io::load_wic_image(png);
        if (!loaded.ok() || loaded.source->source_identity != candidate.pixel_identity) continue;
        candidate.rendered_image = std::move(loaded.source->image);
        candidates.push_back(std::move(candidate));
    }
    return candidates;
}

[[nodiscard]] bool save_candidate_cache(
    const Options& options,
    const faultmine::core::BatchRunResult& run,
    std::string& error) {
    const std::filesystem::path directory = options.output / L"candidates";
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec) { error = "cannot create candidate cache directory: " + ec.message(); return false; }
    for (const auto& candidate : run.manifest.candidates) {
        if (!candidate.rendered_image.has_value()) { error = "completed candidate is missing its render buffer"; return false; }
        const std::filesystem::path path = directory / faultmine::core::batch_candidate_filename(candidate.descendant_index);
        const std::filesystem::path temporary = path.wstring() + L".tmp.png";
        if (const auto write_error = faultmine::io::save_wic_png(*candidate.rendered_image, temporary); write_error.has_value()) {
            error = "candidate cache PNG write failed: " + write_error->message;
            return false;
        }
        if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            std::filesystem::remove(temporary, ec);
            error = "candidate cache commit failed with Win32 error " + std::to_string(GetLastError());
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool prepare_session(
    faultmine::app::SessionModel& session,
    const InputState& input,
    std::string& error) {
    if (input.project.has_value()) {
        return session.load_project_state(*input.project, input.source, input.source_path, &error);
    }
    if (!session.set_source(input.source, input.source_identity, input.source_path, &error)) return false;
    return session.promote_exploration_genome(input.parent, &error);
}

[[nodiscard]] bool write_importable_project(
    const Options& options,
    const InputState& input,
    const faultmine::core::BatchRunResult& run,
    std::string& error) {
    faultmine::app::SessionModel session;
    if (!prepare_session(session, input, error)) return false;
    std::string first_selected;
    for (const auto& candidate : run.manifest.candidates) {
        if (!candidate.selected) continue;
        if (!session.retain_mutation_specimen(candidate.genome, candidate.provenance, true, &error)) return false;
        if (first_selected.empty()) first_selected = candidate.genome_identity;
    }
    if (!first_selected.empty() && !session.activate_lineage_specimen(first_selected, &error)) return false;
    const auto project = session.make_project_document(&error);
    if (!project.has_value()) return false;
    const std::string text = faultmine::app::serialize_project_canonical(*project);
    if (text.empty()) { error = "failed to serialize GUI-importable batch project"; return false; }
    return write_text_atomic(options.output / L"batch.fmproj", text, error);
}

[[nodiscard]] bool export_selected_outputs(
    const Options& options,
    const InputState& input,
    const faultmine::core::BatchRunResult& run,
    std::string& error) {
    std::vector<faultmine::exporting::ContactSheetSpecimen> specimens;
    for (const auto& candidate : run.manifest.candidates) {
        if (!candidate.selected) continue;
        specimens.push_back(faultmine::exporting::ContactSheetSpecimen{
            candidate.descendant_index, candidate.genome, candidate.genome_identity});
    }
    if (specimens.empty()) return true;

    faultmine::app::SessionModel session;
    if (!prepare_session(session, input, error)) return false;
    for (const auto& candidate : run.manifest.candidates) {
        if (!candidate.selected) continue;
        if (!session.retain_mutation_specimen(candidate.genome, candidate.provenance, true, &error)) return false;
    }

    const auto collision = options.resume
        ? faultmine::exporting::CollisionPolicy::overwrite
        : faultmine::exporting::CollisionPolicy::fail_if_exists;
    if (options.contact_sheet) {
        faultmine::exporting::ContactSheetRequest request;
        request.destination = options.output / L"selected-contact-sheet.png";
        request.specimens = specimens;
        request.frame_index = options.frame;
        request.collision = collision;
        request.write_manifest = true;
        const auto exported = faultmine::exporting::export_contact_sheet(session, request);
        if (!exported.ok()) {
            error = "contact sheet export failed";
            if (exported.error.has_value()) error += ": " + exported.error->message;
            return false;
        }
    }

    if (options.selected_stills) {
        const std::filesystem::path directory = options.output / L"selected";
        std::error_code ec;
        std::filesystem::create_directories(directory, ec);
        if (ec) { error = "cannot create selected-still directory: " + ec.message(); return false; }
        for (const auto& candidate : run.manifest.candidates) {
            if (!candidate.selected) continue;
            if (!session.promote_mutation_specimen(candidate.genome, candidate.provenance, true, &error)) return false;
            faultmine::exporting::StillExportRequest request;
            request.destination = directory / faultmine::core::batch_candidate_filename(candidate.descendant_index);
            request.collision = collision;
            request.write_manifest = true;
            request.frame_index = options.frame;
            const auto exported = faultmine::exporting::export_still(session, request);
            if (!exported.ok()) {
                error = "selected still export failed";
                if (exported.error.has_value()) error += ": " + exported.error->message;
                return false;
            }
        }
    }
    return true;
}

}  // namespace

int wmain(const int argc, wchar_t* argv[]) {
    Options options;
    std::string error;
    if (!parse_options(argc, argv, options, error)) {
        if (!error.empty()) std::cerr << "Error: " << error << "\n\n";
        usage();
        return error.empty() ? 0 : 2;
    }
    SetConsoleCtrlHandler(console_control_handler, TRUE);

    faultmine::core::FaultRegistry registry;
    try {
        registry = faultmine::core::make_default_fault_registry();
    } catch (const std::exception& exception) {
        std::cerr << "Registry initialization failed: " << exception.what() << '\n';
        return 3;
    }
    auto input = load_input(options, registry, error);
    if (!input.has_value()) {
        std::cerr << "Input error: " << error << '\n';
        return 4;
    }

    faultmine::core::BatchRequest request;
    request.mutation_seed = *options.mutation_seed;
    request.radius = options.radius;
    request.locks = input->locks;
    request.index_begin = options.begin;
    request.index_end_exclusive = options.begin + *options.count;
    request.frame_index = options.frame;
    request.render_mode = options.render_mode;
    request.proxy_spec = options.proxy;
    request.near_duplicate_threshold = options.near_threshold;
    request.select_count = options.select_count;
    request.worker_count = options.threads;

    std::vector<faultmine::core::BatchCandidate> resume = load_resume_candidates(
        options, *input, registry, request, error);
    if (!error.empty()) {
        std::cerr << "Resume error: " << error << '\n';
        return 5;
    }

    const auto run = faultmine::core::run_batch(
        input->source,
        input->source_identity,
        input->parent,
        registry,
        request,
        resume,
        [] { return g_cancel_requested.load(std::memory_order_relaxed); });
    if (!run.ok()) {
        std::cerr << "Batch error: " << run.error << '\n';
        return 6;
    }

    if (!save_candidate_cache(options, run, error)) {
        std::cerr << "Output error: " << error << '\n';
        return 7;
    }
    const std::string manifest_text = faultmine::core::serialize_batch_manifest(run.manifest);
    if (!write_text_atomic(options.output / L"batch.fmbatch.json", manifest_text, error)) {
        std::cerr << "Manifest error: " << error << '\n';
        return 8;
    }
    if (!write_importable_project(options, *input, run, error)) {
        std::cerr << "Project output error: " << error << '\n';
        return 9;
    }
    if (!run.manifest.cancelled && !export_selected_outputs(options, *input, run, error)) {
        std::cerr << "Selected-output error: " << error << '\n';
        return 10;
    }

    const auto& stats = run.stats;
    std::cout << "FAULTMINE batch: " << (run.manifest.complete ? "complete" : (run.manifest.cancelled ? "cancelled" : "partial")) << '\n'
              << "requested=" << stats.requested << " completed=" << stats.completed
              << " reused=" << stats.reused << " failed=" << stats.failed << '\n'
              << "unique_genomes=" << stats.unique_genomes << " unique_pixels=" << stats.unique_pixels
              << " near_duplicate_groups=" << stats.near_duplicate_groups
              << " selected=" << stats.selected << '\n'
              << "manifest=" << (options.output / L"batch.fmbatch.json").string() << '\n'
              << "project=" << (options.output / L"batch.fmproj").string() << '\n'
              << "{\"requested\":" << stats.requested
              << ",\"completed\":" << stats.completed
              << ",\"reused\":" << stats.reused
              << ",\"failed\":" << stats.failed
              << ",\"unique_genomes\":" << stats.unique_genomes
              << ",\"unique_pixels\":" << stats.unique_pixels
              << ",\"near_duplicate_groups\":" << stats.near_duplicate_groups
              << ",\"selected\":" << stats.selected
              << ",\"complete\":" << (run.manifest.complete ? "true" : "false")
              << ",\"cancelled\":" << (run.manifest.cancelled ? "true" : "false") << "}\n";
    if (run.manifest.cancelled) return 11;
    return run.manifest.complete ? 0 : 12;
}
