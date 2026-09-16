#include "faultmine/core.hpp"
#include "faultmine/determinism.hpp"
#include "faultmine/genome.hpp"
#include "faultmine/sha256.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

int g_failures = 0;

void expect(const bool condition, const std::string_view message) {
    if (!condition) {
        ++g_failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

template <typename Left, typename Right>
void expect_equal(const Left& left, const Right& right, const std::string_view message) {
    expect(left == right, message);
}

faultmine::core::OperatorRegistry make_registry() {
    using namespace faultmine::core;
    OperatorRegistry registry;
    std::string error;

    OperatorDescriptor offset;
    offset.type_id = "test.offset";
    offset.minimum_supported_version = 1;
    offset.current_version = 1;
    offset.parameters = {
        ParameterDescriptor{"amount", ParameterKind::signed_integer, true, MutationMetadata{}},
        ParameterDescriptor{"label", ParameterKind::text, true, MutationMetadata{}}};
    if (!registry.register_operator(std::move(offset), &error)) {
        throw std::runtime_error("failed to register test.offset: " + error);
    }

    OperatorDescriptor mask;
    mask.type_id = "test.mask";
    mask.minimum_supported_version = 1;
    mask.current_version = 1;
    mask.parameters = {
        ParameterDescriptor{"mask", ParameterKind::unsigned_integer, true, MutationMetadata{}}};
    if (!registry.register_operator(std::move(mask), &error)) {
        throw std::runtime_error("failed to register test.mask: " + error);
    }
    return registry;
}

faultmine::core::Genome make_genome() {
    using namespace faultmine::core;
    Genome genome;
    genome.root_seed = RootSeed{0x0123456789abcdefULL};

    OperatorInstance offset;
    offset.instance_id = InstanceId{0x0011223344556677ULL, 0x8899aabbccddeeffULL};
    offset.type_id = "test.offset";
    offset.type_version = 1;
    offset.enabled = true;
    offset.parameters.emplace("amount", ParameterValue{std::int64_t{-7}});
    offset.parameters.emplace("label", ParameterValue{std::string{"A\nB"}});

    OperatorInstance mask;
    mask.instance_id = InstanceId{0xfedcba9876543210ULL, 0x0123456789abcdefULL};
    mask.type_id = "test.mask";
    mask.type_version = 1;
    mask.enabled = false;
    mask.parameters.emplace("mask", ParameterValue{std::uint64_t{0xffffffffffffffffULL}});

    genome.operators.push_back(std::move(offset));
    genome.operators.push_back(std::move(mask));
    return genome;
}

std::string canonical_fixture() {
    return
        "{\"schema_version\":1,\"engine_contract_version\":1,\"root_seed\":\"0123456789abcdef\",\"operators\":["
        "{\"instance_id\":\"00112233445566778899aabbccddeeff\",\"type_id\":\"test.offset\",\"type_version\":1,\"enabled\":true,\"parameters\":{"
        "\"amount\":{\"kind\":\"i64\",\"value\":-7},\"label\":{\"kind\":\"string\",\"value\":\"A\\nB\"}}},"
        "{\"instance_id\":\"fedcba98765432100123456789abcdef\",\"type_id\":\"test.mask\",\"type_version\":1,\"enabled\":false,\"parameters\":{"
        "\"mask\":{\"kind\":\"u64\",\"value\":18446744073709551615}}}]}\n";
}

void expect_parse_error(
    const std::string_view text,
    const faultmine::core::OperatorRegistry& registry,
    const faultmine::core::GenomeErrorCode expected,
    const std::string_view message) {
    const auto result = faultmine::core::parse_genome(text, registry);
    expect(!result.ok(), message);
    expect(result.error.has_value(), "parse failure must include structured error");
    if (result.error.has_value()) {
        expect_equal(result.error->code, expected, message);
        expect(!result.error->path.empty(), "structured error must include a path");
    }
}

std::string replace_once(std::string input, const std::string_view before, const std::string_view after) {
    const std::size_t position = input.find(before);
    if (position == std::string::npos) {
        throw std::runtime_error("test fixture replacement target not found");
    }
    input.replace(position, before.size(), after);
    return input;
}

void test_bootstrap_contract() {
    expect_equal(faultmine::core::product_name(), std::string_view{"FAULTMINE"}, "product name remains stable");
    expect(faultmine::core::bootstrap_contract_version() >= 1U, "bootstrap contract remains available");
}

void test_seed_and_instance_ids() {
    using namespace faultmine::core;
    const auto seed = RootSeed::parse("0123456789ABCDEF");
    expect(seed.has_value(), "root seed accepts exact fixed-width hexadecimal text");
    if (seed.has_value()) {
        expect_equal(seed->value, 0x0123456789abcdefULL, "root seed numeric parse");
        expect_equal(seed->to_string(), std::string{"0123456789abcdef"}, "root seed canonical lowercase format");
    }
    expect(!RootSeed::parse("1234").has_value(), "root seed rejects wrong width");
    expect(!RootSeed::parse("0123456789abcdeg").has_value(), "root seed rejects non-hex input");

    const auto instance = InstanceId::parse("00112233445566778899AABBCCDDEEFF");
    expect(instance.has_value(), "instance ID accepts exact 128-bit hexadecimal text");
    if (instance.has_value()) {
        expect_equal(instance->high, 0x0011223344556677ULL, "instance ID high word");
        expect_equal(instance->low, 0x8899aabbccddeeffULL, "instance ID low word");
        expect_equal(instance->to_string(), std::string{"00112233445566778899aabbccddeeff"}, "instance ID canonical lowercase format");
    }
    expect(!InstanceId::parse("0011").has_value(), "instance ID rejects wrong width");
}

void test_entropy_known_answers() {
    using namespace faultmine::core;
    expect_equal(mix64(0U), std::uint64_t{0x0000000000000000ULL}, "mix64 zero vector");
    expect_equal(mix64(1U), std::uint64_t{0x5692161d100b05e5ULL}, "mix64 one vector");
    expect_equal(stable_tag_hash("scanline"), std::uint64_t{0xf162a4628b7e1992ULL}, "FNV-1a purpose tag vector");

    const RootSeed root{0x0123456789abcdefULL};
    const InstanceId instance{0x0011223344556677ULL, 0x8899aabbccddeeffULL};
    const std::array<std::uint64_t, 2> identity{7U, 11U};
    const std::uint64_t derived = derive_stream_seed(root, instance, "scanline", identity);
    expect_equal(derived, std::uint64_t{0xe878b6e8cc098808ULL}, "named stream seed vector");

    auto stream = make_named_stream(root, instance, "scanline", identity);
    constexpr std::array<std::uint64_t, 5> expected{
        0x90c745c6e92dd49dULL,
        0x1be54033185387d8ULL,
        0x242ab2c766145b60ULL,
        0x33477970e976f62fULL,
        0x70a847fcd8f86b2dULL};
    for (const std::uint64_t value : expected) {
        expect_equal(stream.next_u64(), value, "SplitMix64 stream known-answer vector");
    }

    const InstanceId child = derive_instance_id(root, instance, "child-instance", 42U);
    expect_equal(child.to_string(), std::string{"b42a435c13b5b6269cba56a7fd72f5e6"}, "deterministic child instance ID vector");
}

void test_bounded_mapping_and_stream_independence() {
    using namespace faultmine::core;
    const RootSeed root{0x0123456789abcdefULL};
    const InstanceId instance{0x0011223344556677ULL, 0x8899aabbccddeeffULL};
    const std::array<std::uint64_t, 2> identity{7U, 11U};
    auto bounded = make_named_stream(root, instance, "scanline", identity);
    constexpr std::array<std::uint64_t, 10> expected{7U, 4U, 4U, 9U, 9U, 1U, 5U, 8U, 9U, 7U};
    for (const std::uint64_t value : expected) {
        expect_equal(bounded.uniform_below(10U), value, "rejection-sampled bounded mapping vector");
    }
    expect_equal(bounded.uniform_below(1U), std::uint64_t{0U}, "bounded mapping handles unit range");

    bool threw_zero = false;
    try {
        static_cast<void>(bounded.uniform_below(0U));
    } catch (const std::invalid_argument&) {
        threw_zero = true;
    }
    expect(threw_zero, "bounded mapping rejects zero upper bound");

    auto unrelated_a = make_named_stream(root, instance, "unrelated-a");
    auto unrelated_b = make_named_stream(root, instance, "unrelated-b");
    const std::uint64_t b_before = unrelated_b.next_u64();
    for (int index = 0; index < 100; ++index) {
        static_cast<void>(unrelated_a.next_u64());
    }
    auto unrelated_b_again = make_named_stream(root, instance, "unrelated-b");
    expect_equal(unrelated_b_again.next_u64(), b_before, "unrelated stream consumption cannot perturb a named stream");
}

void test_sha256() {
    using namespace faultmine::core;
    expect_equal(
        sha256_hex(""),
        std::string{"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
        "SHA-256 empty vector");
    expect_equal(
        sha256_hex("abc"),
        std::string{"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
        "SHA-256 abc vector");
}

void test_registry_contract() {
    using namespace faultmine::core;
    OperatorRegistry registry;
    std::string error;
    OperatorDescriptor descriptor;
    descriptor.type_id = "test.one";
    descriptor.parameters = {ParameterDescriptor{"value", ParameterKind::signed_integer, true, MutationMetadata{}}};
    expect(registry.register_operator(descriptor, &error), "registry accepts valid descriptor");
    expect(registry.find("test.one") != nullptr, "registry resolves registered descriptor");
    expect(!registry.register_operator(descriptor, &error), "registry rejects duplicate type ID");

    OperatorDescriptor invalid;
    invalid.type_id = "test.invalid";
    invalid.minimum_supported_version = 2;
    invalid.current_version = 1;
    expect(!registry.register_operator(std::move(invalid), &error), "registry rejects invalid version ranges");
}

void test_canonical_genome_round_trip() {
    using namespace faultmine::core;
    const OperatorRegistry registry = make_registry();
    const Genome genome = make_genome();
    expect(!validate_genome(genome, registry).has_value(), "representative genome validates");

    const std::string expected = canonical_fixture();
    const std::string serialized = serialize_canonical_genome(genome);
    expect_equal(serialized, expected, "canonical genome bytes are exact");
    expect_equal(
        genome_identity_hex(genome),
        std::string{"c2b815d1b94e4485a9a3d7f58152f53546809d2a2e2775b30aa9d0e0bb818e92"},
        "canonical genome SHA-256 identity vector");

    const GenomeParseResult parsed = parse_genome(serialized, registry);
    expect(parsed.ok(), "canonical genome parses successfully");
    if (parsed.genome.has_value()) {
        expect_equal(*parsed.genome, genome, "serialize/parse preserves semantic genome state");
        expect_equal(serialize_canonical_genome(*parsed.genome), expected, "parse/serialize returns canonical bytes");
    }
}

void test_reorder_identity() {
    using namespace faultmine::core;
    Genome original = make_genome();
    Genome reordered = original;
    std::swap(reordered.operators[0], reordered.operators[1]);
    expect_equal(reordered.operators[1].instance_id, original.operators[0].instance_id, "reorder preserves first operator instance ID");
    expect_equal(reordered.operators[0].instance_id, original.operators[1].instance_id, "reorder preserves second operator instance ID");
    expect(genome_identity_hex(reordered) != genome_identity_hex(original), "operator order changes canonical genome identity");
}

void test_parse_failures() {
    using namespace faultmine::core;
    const OperatorRegistry registry = make_registry();
    const std::string canonical = canonical_fixture();

    expect_parse_error("{", registry, GenomeErrorCode::syntax_error, "malformed JSON is rejected");
    expect_parse_error(
        "{\"schema_version\":1,\"schema_version\":1}",
        registry,
        GenomeErrorCode::duplicate_field,
        "duplicate fields are rejected");
    expect_parse_error(
        "{\"schema_version\":1,\"engine_contract_version\":1,\"root_seed\":\"0123456789abcdef\"}",
        registry,
        GenomeErrorCode::missing_field,
        "missing required field is rejected");
    expect_parse_error(
        "{\"schema_version\":1,\"engine_contract_version\":1,\"root_seed\":5,\"operators\":[]}",
        registry,
        GenomeErrorCode::wrong_type,
        "wrong root seed type is rejected");
    expect_parse_error(
        "{\"schema_version\":4294967296,\"engine_contract_version\":1,\"root_seed\":\"0123456789abcdef\",\"operators\":[]}",
        registry,
        GenomeErrorCode::numeric_overflow,
        "uint32 overflow is rejected");
    expect_parse_error(
        "{\"schema_version\":2,\"engine_contract_version\":1,\"root_seed\":\"0123456789abcdef\",\"operators\":[]}",
        registry,
        GenomeErrorCode::unsupported_version,
        "future schema version is rejected");
    expect_parse_error(
        "{\"schema_version\":1,\"engine_contract_version\":1,\"root_seed\":\"0123456789abcdef\",\"operators\":[],\"future\":true}",
        registry,
        GenomeErrorCode::unexpected_field,
        "unknown required semantic state is not silently dropped");

    expect_parse_error(
        replace_once(canonical, "test.offset", "test.unknown"),
        registry,
        GenomeErrorCode::unknown_operator,
        "unknown operator type is rejected");
    expect_parse_error(
        replace_once(canonical, "\"type_version\":1", "\"type_version\":2"),
        registry,
        GenomeErrorCode::unsupported_version,
        "future operator version is rejected");
    expect_parse_error(
        replace_once(canonical, "18446744073709551615", "18446744073709551616"),
        registry,
        GenomeErrorCode::numeric_overflow,
        "uint64 parameter overflow is rejected");
    expect_parse_error(
        replace_once(canonical, "fedcba98765432100123456789abcdef", "00112233445566778899aabbccddeeff"),
        registry,
        GenomeErrorCode::duplicate_instance_id,
        "duplicate operator instance IDs are rejected");
    expect_parse_error(
        replace_once(canonical, "\"amount\":{\"kind\":\"i64\",\"value\":-7}", "\"amount\":{\"kind\":\"u64\",\"value\":7}"),
        registry,
        GenomeErrorCode::invalid_parameter,
        "parameter kind mismatch is rejected");
}

}  // namespace

int main() {
    try {
        test_bootstrap_contract();
        test_seed_and_instance_ids();
        test_entropy_known_answers();
        test_bounded_mapping_and_stream_independence();
        test_sha256();
        test_registry_contract();
        test_canonical_genome_round_trip();
        test_reorder_identity();
        test_parse_failures();
    } catch (const std::exception& exception) {
        ++g_failures;
        std::cerr << "UNCAUGHT TEST EXCEPTION: " << exception.what() << '\n';
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " test assertion(s) failed.\n";
        return 1;
    }
    std::cout << "FAULTMINE deterministic core contracts passed.\n";
    return 0;
}
