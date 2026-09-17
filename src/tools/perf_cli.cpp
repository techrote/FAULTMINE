#include "faultmine/batch.hpp"
#include "faultmine/fault_catalogue.hpp"
#include "faultmine/lineage.hpp"
#include "faultmine/project.hpp"
#include "faultmine/proxy.hpp"
#include "faultmine/session.hpp"
#include "faultmine/starter_operators.hpp"
#include "faultmine/version.hpp"
#include "render/d3d11/canvas_renderer.hpp"

#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

struct Measurement {
    double render_1080p_ms{};
    double proxy_ms{};
    double d3d_upload_present_ms{};
    double project_roundtrip_ms{};
    double temporal_frame8_ms{};
    double batch32_ms{};
    std::size_t estimated_pipeline_mib{};
    bool using_warp{};
};

[[nodiscard]] double elapsed_ms(const Clock::time_point start, const Clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

[[nodiscard]] faultmine::core::ImageBuffer make_pattern(const std::uint32_t width, const std::uint32_t height) {
    auto created = faultmine::core::make_rgba8_image(width, height);
    if (!created.ok()) {
        return {};
    }
    auto image = std::move(*created.image);
    for (std::uint32_t y = 0U; y < height; ++y) {
        for (std::uint32_t x = 0U; x < width; ++x) {
            const std::size_t offset = (static_cast<std::size_t>(y) * width + x) * 4U;
            image.bytes[offset + 0U] = static_cast<std::uint8_t>((x * 3U + y * 5U) & 0xffU);
            image.bytes[offset + 1U] = static_cast<std::uint8_t>((x * 11U + y * 7U) & 0xffU);
            image.bytes[offset + 2U] = static_cast<std::uint8_t>((x * 13U + y * 17U) & 0xffU);
            image.bytes[offset + 3U] = 255U;
        }
    }
    return image;
}

[[nodiscard]] faultmine::core::OperatorInstance make_operator(
    const std::uint64_t ordinal,
    std::string type,
    std::map<std::string, faultmine::core::ParameterValue, std::less<>> parameters) {
    faultmine::core::OperatorInstance instance;
    instance.instance_id = faultmine::core::InstanceId{0x4641554c544d494eULL, ordinal};
    instance.type_id = std::move(type);
    instance.type_version = 1U;
    instance.enabled = true;
    instance.parameters = std::move(parameters);
    return instance;
}

[[nodiscard]] faultmine::core::Genome make_representative_genome() {
    using faultmine::core::ParameterValue;
    faultmine::core::Genome genome;
    genome.root_seed = faultmine::core::RootSeed{0x123456789abcdef0ULL};
    genome.operators.push_back(make_operator(
        1U,
        faultmine::core::kFaultRowOffset,
        {{"amount", ParameterValue{std::int64_t{17}}}, {"boundary", ParameterValue{std::string{"wrap"}}}}));
    genome.operators.push_back(make_operator(
        2U,
        faultmine::core::kFaultByteXor,
        {{"channels", ParameterValue{std::string{"rgb"}}}, {"mask", ParameterValue{std::uint64_t{0x55U}}}}));
    genome.operators.push_back(make_operator(
        3U,
        faultmine::core::kFaultBitRotate,
        {{"amount", ParameterValue{std::uint64_t{3U}}}, {"channels", ParameterValue{std::string{"rgba"}}}}));
    genome.operators.push_back(make_operator(
        4U,
        faultmine::core::kFaultScanlineJitter,
        {{"boundary", ParameterValue{std::string{"wrap"}}}, {"max_shift", ParameterValue{std::uint64_t{12U}}}}));
    return genome;
}

[[nodiscard]] bool measure_d3d(
    const faultmine::core::ImageBuffer& image,
    double& milliseconds,
    bool& using_warp) {
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    const wchar_t* class_name = L"FAULTMINEPerfHiddenWindow";
    WNDCLASSW window_class{};
    window_class.lpfnWndProc = DefWindowProcW;
    window_class.hInstance = instance;
    window_class.lpszClassName = class_name;
    if (RegisterClassW(&window_class) == 0U && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    HWND window = CreateWindowExW(
        0U,
        class_name,
        L"FAULTMINE perf",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        640,
        480,
        nullptr,
        nullptr,
        instance,
        nullptr);
    if (window == nullptr) {
        return false;
    }

    faultmine::render::d3d11::CanvasRenderer renderer;
    if (renderer.initialize(window).has_value()) {
        DestroyWindow(window);
        return false;
    }

    constexpr std::size_t iterations = 8U;
    const auto start = Clock::now();
    for (std::size_t index = 0U; index < iterations; ++index) {
        if (renderer.upload_image(image).has_value() ||
            renderer.draw(faultmine::app::CanvasViewState{}).has_value()) {
            DestroyWindow(window);
            return false;
        }
    }
    const auto end = Clock::now();
    milliseconds = elapsed_ms(start, end) / static_cast<double>(iterations);
    using_warp = renderer.using_warp();
    DestroyWindow(window);
    return true;
}

[[nodiscard]] bool measure_all(Measurement& output, std::string& error) {
    const faultmine::core::FaultRegistry registry = faultmine::core::make_default_fault_registry();
    const faultmine::core::Genome genome = make_representative_genome();
    if (const auto validation = faultmine::core::validate_genome(genome, registry.schema_registry()); validation.has_value()) {
        error = "representative genome is invalid: " + validation->message;
        return false;
    }

    const faultmine::core::ImageBuffer full = make_pattern(1920U, 1080U);
    if (full.bytes.empty()) {
        error = "failed to allocate 1080p fixture";
        return false;
    }
    const std::string full_identity = faultmine::core::source_identity_hex(full);
    output.estimated_pipeline_mib = (full.bytes.size() * 3U) / (1024U * 1024U);

    std::vector<double> render_samples;
    for (std::size_t iteration = 0U; iteration < 3U; ++iteration) {
        const auto start = Clock::now();
        const auto rendered = faultmine::core::render_pipeline(full, genome, registry);
        const auto end = Clock::now();
        if (!rendered.ok()) {
            error = "canonical render probe failed";
            return false;
        }
        render_samples.push_back(elapsed_ms(start, end));
    }
    std::sort(render_samples.begin(), render_samples.end());
    output.render_1080p_ms = render_samples[1U];

    const auto proxy_start = Clock::now();
    for (std::size_t iteration = 0U; iteration < 12U; ++iteration) {
        const auto proxy = faultmine::core::make_nearest_proxy(
            full,
            full_identity,
            faultmine::core::ProxySpec{640U, 360U, faultmine::core::kProxyMethodVersion});
        if (!proxy.ok()) {
            error = "proxy throughput probe failed";
            return false;
        }
    }
    output.proxy_ms = elapsed_ms(proxy_start, Clock::now()) / 12.0;

    const auto d3d_proxy = faultmine::core::make_nearest_proxy(
        full,
        full_identity,
        faultmine::core::ProxySpec{1280U, 720U, faultmine::core::kProxyMethodVersion});
    if (!d3d_proxy.ok() || !measure_d3d(d3d_proxy.proxy->image, output.d3d_upload_present_ms, output.using_warp)) {
        error = "D3D11 upload/present probe failed";
        return false;
    }

    faultmine::app::ProjectDocument project;
    project.source.path_utf8 = "perf://synthetic";
    project.source.source_identity = full_identity;
    project.genome = genome;
    faultmine::app::SpecimenRecord root;
    root.genome_identity = faultmine::core::genome_identity_hex(genome);
    root.source_identity = full_identity;
    root.genome = genome;
    root.derivation = faultmine::app::make_manual_root_derivation();
    root.creation_ordinal = 0U;
    project.lineage.specimens.push_back(root);
    project.lineage.active_genome_identity = root.genome_identity;

    const auto persistence_start = Clock::now();
    for (std::size_t iteration = 0U; iteration < 100U; ++iteration) {
        const std::string serialized = faultmine::app::serialize_project_canonical(project);
        const auto parsed = faultmine::app::parse_project(serialized, registry.schema_registry());
        if (!parsed.ok()) {
            error = "project persistence probe failed";
            return false;
        }
    }
    output.project_roundtrip_ms = elapsed_ms(persistence_start, Clock::now()) / 100.0;

    faultmine::core::Genome temporal = genome;
    temporal.operators.push_back(make_operator(
        5U,
        "temporal.feedback-blend",
        {{"amount_256", faultmine::core::ParameterValue{std::uint64_t{192U}}}}));
    const auto temporal_start = Clock::now();
    const auto temporal_render = faultmine::core::render_pipeline_at_frame(full, temporal, registry, 8U);
    output.temporal_frame8_ms = elapsed_ms(temporal_start, Clock::now());
    if (!temporal_render.ok()) {
        error = "temporal frame-8 probe failed";
        return false;
    }

    const faultmine::core::ImageBuffer batch_source = make_pattern(320U, 180U);
    const std::string batch_source_identity = faultmine::core::source_identity_hex(batch_source);
    faultmine::core::BatchRequest batch_request;
    batch_request.mutation_seed = faultmine::core::RootSeed{0x0badf00d12345678ULL};
    batch_request.radius = faultmine::core::MutationRadius::medium;
    batch_request.index_begin = 0U;
    batch_request.index_end_exclusive = 32U;
    batch_request.select_count = 8U;
    batch_request.worker_count = std::max<std::size_t>(1U, std::min<std::size_t>(4U, std::thread::hardware_concurrency()));
    const auto batch_start = Clock::now();
    const auto batch_result = faultmine::core::run_batch(
        batch_source,
        batch_source_identity,
        genome,
        registry,
        batch_request);
    output.batch32_ms = elapsed_ms(batch_start, Clock::now());
    if (!batch_result.ok() || !batch_result.manifest.complete || batch_result.stats.completed != 32U) {
        error = "batch-32 throughput probe failed";
        return false;
    }

    return true;
}

}  // namespace

int wmain() {
    Measurement measurement;
    std::string error;
    if (!measure_all(measurement, error)) {
        std::cerr << "FAULTMINE-perf: " << error << '\n';
        return 2;
    }

    const bool within_budget =
        measurement.render_1080p_ms <= 2500.0 &&
        measurement.proxy_ms <= 250.0 &&
        measurement.d3d_upload_present_ms <= 250.0 &&
        measurement.project_roundtrip_ms <= 100.0 &&
        measurement.temporal_frame8_ms <= 15000.0 &&
        measurement.batch32_ms <= 15000.0 &&
        measurement.estimated_pipeline_mib <= 64U;

    std::cout
        << "{\"application_version\":\"" << faultmine::kApplicationVersion
        << "\",\"render_1080p_ms\":" << measurement.render_1080p_ms
        << ",\"proxy_640x360_ms\":" << measurement.proxy_ms
        << ",\"d3d_upload_present_1280x720_ms\":" << measurement.d3d_upload_present_ms
        << ",\"d3d_warp\":" << (measurement.using_warp ? "true" : "false")
        << ",\"project_roundtrip_ms\":" << measurement.project_roundtrip_ms
        << ",\"temporal_frame8_ms\":" << measurement.temporal_frame8_ms
        << ",\"batch32_ms\":" << measurement.batch32_ms
        << ",\"estimated_pipeline_mib\":" << measurement.estimated_pipeline_mib
        << ",\"within_release_budget\":" << (within_budget ? "true" : "false")
        << "}\n";

    return within_budget ? 0 : 3;
}
