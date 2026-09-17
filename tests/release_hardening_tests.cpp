#include "faultmine/batch.hpp"
#include "faultmine/export.hpp"
#include "faultmine/fault_catalogue.hpp"
#include "faultmine/genome.hpp"
#include "faultmine/image.hpp"
#include "faultmine/laboratory_worker.hpp"
#include "faultmine/lineage.hpp"
#include "faultmine/mutation.hpp"
#include "faultmine/project.hpp"
#include "faultmine/version.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>

namespace {

int failures = 0;

void expect(const bool condition, const std::string_view message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void test_release_versions() {
    expect(faultmine::kApplicationVersion == "1.0.0", "application version must be v1.0.0");
    expect(faultmine::exporting::kApplicationVersion == faultmine::kApplicationVersion,
        "export provenance must use the shared application version");
    expect(faultmine::core::kEngineContractVersion == 1U, "engine contract remains v1");
    expect(faultmine::core::kGenomeSchemaVersion == 1U, "genome schema remains v1");
    expect(faultmine::app::kProjectSchemaVersion == 2U, "project schema remains v2");
    expect(faultmine::exporting::kExportManifestSchemaVersion == 1U, "export manifest remains v1");
    expect(faultmine::core::kMutationPolicyVersion == 1U, "mutation policy remains v1");
    expect(faultmine::core::kBatchManifestVersion == 1U, "batch manifest remains v1");
    expect(faultmine::core::kVisualDescriptorVersion == 1U, "descriptor remains v1");
}

void test_image_resource_limit() {
    constexpr std::uint32_t width = 16384U;
    constexpr std::uint32_t exact_height = 8192U;
    constexpr std::uint32_t too_tall = 8193U;
    const auto exact = faultmine::core::canonical_rgba8_byte_size(width, exact_height);
    expect(exact.has_value(), "512 MiB canonical image boundary must remain representable");
    if (exact.has_value()) {
        expect(*exact == faultmine::core::kMaxCanonicalImageBytes,
            "canonical image boundary must equal the documented 512 MiB limit");
    }
    expect(!faultmine::core::canonical_rgba8_byte_size(width, too_tall).has_value(),
        "canonical image sizing must reject allocations above 512 MiB");
    const auto rejected = faultmine::core::make_rgba8_image(width, too_tall);
    expect(!rejected.ok(), "oversized canonical image allocation must fail before allocation");
    expect(rejected.error.has_value() && rejected.error->code == faultmine::core::ImageErrorCode::resource_limit,
        "oversized canonical image must report an actionable resource-limit error");
}

void test_structured_input_limits() {
    const auto registry = faultmine::core::make_default_fault_registry();

    std::string malicious;
    malicious.append(70U, '[');
    malicious += '0';
    malicious.append(70U, ']');
    const auto nested = faultmine::core::parse_genome(malicious, registry.schema_registry());
    expect(!nested.ok(), "excessively nested structured input must be rejected");
    expect(nested.error.has_value() && nested.error->code == faultmine::core::GenomeErrorCode::syntax_error,
        "nesting rejection must surface as a structured parse error");

    std::string oversized =
        "{\"schema_version\":1,\"engine_contract_version\":1,\"root_seed\":\"0000000000000000\",\"operators\":[";
    for (std::size_t index = 0U; index < faultmine::core::kMaximumGenomeOperators + 1U; ++index) {
        if (index != 0U) oversized += ',';
        oversized += "{}";
    }
    oversized += "]}";
    const auto parsed = faultmine::core::parse_genome(oversized, registry.schema_registry());
    expect(!parsed.ok(), "oversized genome JSON must be rejected before per-operator parsing");
    expect(parsed.error.has_value() && parsed.error->code == faultmine::core::GenomeErrorCode::resource_limit,
        "oversized genome JSON must report the resource-limit error class");

    faultmine::core::Genome programmatic;
    programmatic.operators.resize(faultmine::core::kMaximumGenomeOperators + 1U);
    const auto validation = faultmine::core::validate_genome(programmatic, registry.schema_registry());
    expect(validation.has_value() && validation->code == faultmine::core::GenomeErrorCode::resource_limit,
        "programmatic genomes must obey the same topology resource ceiling as parsed genomes");
}

void test_lineage_resource_limit() {
    const auto registry = faultmine::core::make_default_fault_registry();
    faultmine::app::LineageState oversized;
    oversized.specimens.resize(faultmine::app::kMaximumLineageSpecimens + 1U);
    oversized.active_genome_identity = std::string(64U, '0');
    faultmine::app::LineageGraph graph;
    std::string error;
    expect(!graph.load(
        std::move(oversized),
        registry.schema_registry(),
        std::string(64U, '0'),
        &error),
        "oversized retained lineage must be rejected before graph traversal");
    expect(error.find("4096") != std::string::npos,
        "lineage resource-limit error must state the v1 retained-specimen ceiling");
}

void test_resource_contract_alignment() {
    expect(faultmine::core::kMaximumGenomeOperators == 64U,
        "v1 genome topology resource ceiling must be explicit");
    expect(faultmine::core::kMaximumMutationOperators == faultmine::core::kMaximumGenomeOperators,
        "mutation topology limit must match the genome resource contract");
    expect(faultmine::app::kMaximumLineageSpecimens == 4096U,
        "v1 retained-lineage ceiling must be explicit");
    const faultmine::laboratory::WorkerOptions worker;
    expect(worker.timeout_ms == 5000U, "worker default deadline remains explicit");
    expect(worker.job_memory_limit_bytes == 512ULL * 1024ULL * 1024ULL,
        "worker Job Object memory ceiling remains explicit");
}

}  // namespace

int main() {
    test_release_versions();
    test_image_resource_limit();
    test_structured_input_limits();
    test_lineage_resource_limit();
    test_resource_contract_alignment();

    if (failures != 0) {
        std::cerr << failures << " release-hardening assertion(s) failed.\n";
        return 1;
    }
    std::cout << "FAULTMINE v1 release-hardening contracts passed.\n";
    return 0;
}
