#include "faultmine/fault_catalogue.hpp"
#include "faultmine/genome.hpp"
#include "faultmine/image.hpp"
#include "faultmine/pipeline.hpp"
#include "faultmine/wic_io.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>

namespace {

[[nodiscard]] std::string read_binary_text(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return {};
    }
    return std::string{
        std::istreambuf_iterator<char>{stream},
        std::istreambuf_iterator<char>{}};
}

[[nodiscard]] bool parse_frame(const std::wstring_view text, std::uint64_t& frame) noexcept {
    if (text.empty()) {
        return false;
    }
    std::uint64_t parsed = 0U;
    for (const wchar_t digit : text) {
        if (digit < L'0' || digit > L'9') {
            return false;
        }
        const std::uint64_t value = static_cast<std::uint64_t>(digit - L'0');
        if (parsed > (std::numeric_limits<std::uint64_t>::max() - value) / 10U) {
            return false;
        }
        parsed = parsed * 10U + value;
    }
    frame = parsed;
    return true;
}

void usage() {
    std::cerr
        << "Usage:\n"
        << "  FAULTMINE-render [--frame N] <input-image> <genome.json> <output.png>\n"
        << "  FAULTMINE-render --verify [--frame N] <input-image> <genome.json> <expected-image>\n"
        << "Verification compares decoded canonical RGBA8 pixels and does not change the expected image.\n";
}

}  // namespace

int wmain(const int argc, wchar_t* argv[]) {
    int first_path = 1;
    bool verify = false;
    bool explicit_frame = false;
    std::uint64_t frame = 0U;

    if (first_path < argc && std::wstring_view{argv[first_path]} == L"--verify") {
        verify = true;
        ++first_path;
    }
    if (first_path < argc && std::wstring_view{argv[first_path]} == L"--frame") {
        if (first_path + 1 >= argc ||
            !parse_frame(std::wstring_view{argv[first_path + 1]}, frame)) {
            usage();
            return 2;
        }
        explicit_frame = true;
        first_path += 2;
    }
    if (argc != first_path + 3) {
        usage();
        return 2;
    }

    const std::filesystem::path input_path{argv[first_path]};
    const std::filesystem::path genome_path{argv[first_path + 1]};
    const std::filesystem::path output_path{argv[first_path + 2]};

    const std::string genome_text = read_binary_text(genome_path);
    if (genome_text.empty()) {
        std::cerr << "Failed to read non-empty genome file.\n";
        return 3;
    }

    faultmine::core::FaultRegistry registry;
    try {
        registry = faultmine::core::make_default_fault_registry();
    } catch (const std::exception& exception) {
        std::cerr << "Failed to initialize fault registry: " << exception.what() << '\n';
        return 4;
    }

    const auto parsed = faultmine::core::parse_genome(genome_text, registry.schema_registry());
    if (!parsed.ok()) {
        std::cerr << "Genome parse/validation failed";
        if (parsed.error.has_value()) {
            std::cerr << " at " << parsed.error->path << ": " << parsed.error->message;
        }
        std::cerr << '\n';
        return 5;
    }

    const auto loaded = faultmine::io::load_wic_image(input_path);
    if (!loaded.ok()) {
        std::cerr << "WIC source load failed";
        if (loaded.error.has_value()) {
            std::cerr << " during " << loaded.error->operation << ": " << loaded.error->message;
        }
        std::cerr << '\n';
        return 6;
    }

    const auto rendered = explicit_frame
        ? faultmine::core::render_pipeline_at_frame(
              loaded.source->image, *parsed.genome, registry, frame)
        : faultmine::core::render_pipeline(
              loaded.source->image, *parsed.genome, registry);
    if (!rendered.ok()) {
        std::cerr << "Canonical render failed";
        if (rendered.error.has_value()) {
            std::cerr << " at operator " << rendered.error->operator_index;
            if (!rendered.error->operator_type.empty()) {
                std::cerr << " (" << rendered.error->operator_type << ')';
            }
            std::cerr << ": " << rendered.error->message;
        }
        std::cerr << '\n';
        return 7;
    }

    const std::string actual_identity = faultmine::core::source_identity_hex(*rendered.image);

    if (verify) {
        const auto expected = faultmine::io::load_wic_image(output_path);
        if (!expected.ok()) {
            std::cerr << "WIC expected-image load failed";
            if (expected.error.has_value()) {
                std::cerr << " during " << expected.error->operation << ": " << expected.error->message;
            }
            std::cerr << '\n';
            return 9;
        }

        const auto& actual = *rendered.image;
        const auto& reference = expected.source->image;
        if (actual.width != reference.width ||
            actual.height != reference.height ||
            actual.row_stride != reference.row_stride ||
            actual.bytes != reference.bytes) {
            std::cerr << "Canonical verification mismatch at frame " << frame
                      << ": expected_identity=" << expected.source->source_identity
                      << " actual_identity=" << actual_identity << '\n';
            return 10;
        }
        std::cout << "source_identity=" << loaded.source->source_identity << '\n';
        std::cout << "output_identity=" << actual_identity << '\n';
        std::cout << "frame=" << frame << '\n';
        std::cout << "verification=passed\n";
        return 0;
    }

    if (const auto error = faultmine::io::save_wic_png(*rendered.image, output_path); error.has_value()) {
        std::cerr << "PNG export failed during " << error->operation << ": " << error->message << '\n';
        return 8;
    }

    std::cout << "source_identity=" << loaded.source->source_identity << '\n';
    std::cout << "output_identity=" << actual_identity << '\n';
    if (explicit_frame) {
        std::cout << "frame=" << frame << '\n';
    }
    return 0;
}
