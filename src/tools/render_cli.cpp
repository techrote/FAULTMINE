#include "faultmine/genome.hpp"
#include "faultmine/image.hpp"
#include "faultmine/pipeline.hpp"
#include "faultmine/starter_operators.hpp"
#include "faultmine/wic_io.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

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

}  // namespace

int wmain(const int argc, wchar_t* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: FAULTMINE-render <input-image> <genome.json> <output.png>\n";
        return 2;
    }

    const std::filesystem::path input_path{argv[1]};
    const std::filesystem::path genome_path{argv[2]};
    const std::filesystem::path output_path{argv[3]};

    const std::string genome_text = read_binary_text(genome_path);
    if (genome_text.empty()) {
        std::cerr << "Failed to read non-empty genome file.\n";
        return 3;
    }

    faultmine::core::FaultRegistry registry;
    try {
        registry = faultmine::core::make_starter_fault_registry();
    } catch (const std::exception& exception) {
        std::cerr << "Failed to initialize starter fault registry: " << exception.what() << '\n';
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

    const auto rendered = faultmine::core::render_pipeline(
        loaded.source->image,
        *parsed.genome,
        registry);
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

    if (const auto error = faultmine::io::save_wic_png(*rendered.image, output_path); error.has_value()) {
        std::cerr << "PNG export failed during " << error->operation << ": " << error->message << '\n';
        return 8;
    }

    std::cout << "source_identity=" << loaded.source->source_identity << '\n';
    std::cout << "output_identity=" << faultmine::core::source_identity_hex(*rendered.image) << '\n';
    return 0;
}
