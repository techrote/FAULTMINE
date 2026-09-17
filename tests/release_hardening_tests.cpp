#include "faultmine/batch.hpp"
#include "faultmine/export.hpp"
#include "faultmine/fault_catalogue.hpp"
#include "faultmine/genome.hpp"
#include "faultmine/image.hpp"
#include "faultmine/laboratory_worker.hpp"
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

void test_structured_input_depth_limit() {
    std::string malicious;
    malicious.append(70U, '[');
    malicious += '0';
    malicious.append(70U, ']');
    const auto registry = faultmine::core::make_default_fault_registry();
    const auto parsed = faultmine::core::parse_genome(malicious, registry.schema_registry());
    expect(!parsed.ok(), "excessively nested structured input must be rejected");
    expect(parsed.error.has_value() && parsed.error->code == faultmine::core::GenomeErrorCode::syntax_error,
        "nesting rejection must surface as a structured parse error");
}

void test_resource_contract_alignment() {
    expect(faultmine::core::kMaximumGenomeOperators == 64U,
        "v1 genome topology resource ceiling must be explicit");
    expect(faultmine::core::kMaximumMutationOperators == faultmine::core::kMaximumGenomeOperators,
        "mutation topology limit must match the genome resource contract");
    const faultmine::laboratory::WorkerOptions worker;
    expect(worker.timeout_ms == 5000U, "worker default deadline remains explicit");
    expect(worker.job_memory_limit_bytes == 512ULL * 1024ULL * 1024ULL,
        "worker Job Object memory ceiling remains explicit");
}

}  // namespace

int main() {
    test_release_versions();
    test_image_resource_limit();
    test_structured_input_depth_limit();
    test_resource_contract_alignment();

    if (failures != 0) {
        std::cerr << failures << " release-hardening assertion(s) failed.\n";
        return 1;
    }
    std::cout << "FAULTMINE v1 release-hardening contracts passed.\n";
    return 0;
}
