#include "faultmine/fault_catalogue.hpp"
#include "faultmine/image.hpp"
#include "faultmine/mutation.hpp"
#include "faultmine/pipeline.hpp"
#include "faultmine/session.hpp"
#include "faultmine/specimen_tray.hpp"
#include "faultmine/starter_operators.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

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

faultmine::core::OperatorInstance make_operator(
    const faultmine::core::InstanceId id,
    const char* type) {
    faultmine::core::OperatorInstance instance;
    instance.instance_id = id;
    instance.type_id = type;
    instance.type_version = 1U;
    instance.enabled = true;
    return instance;
}

faultmine::core::Genome make_parent() {
    using namespace faultmine::core;
    Genome genome;
    genome.root_seed = RootSeed{0x123456789abcdef0ULL};

    auto row = make_operator(InstanceId{1U, 1U}, kFaultRowOffset);
    row.parameters.emplace("amount", ParameterValue{std::int64_t{3}});
    row.parameters.emplace("boundary", ParameterValue{std::string{"wrap"}});

    auto channels = make_operator(InstanceId{1U, 2U}, kFaultChannelPermute);
    channels.parameters.emplace("order", ParameterValue{std::string{"bgra"}});

    auto xors = make_operator(InstanceId{1U, 3U}, kFaultByteXor);
    xors.parameters.emplace("mask", ParameterValue{std::uint64_t{7U}});
    xors.parameters.emplace("channels", ParameterValue{std::string{"rb"}});

    auto jitter = make_operator(InstanceId{1U, 4U}, kFaultScanlineJitter);
    jitter.parameters.emplace("max_shift", ParameterValue{std::uint64_t{2U}});
    jitter.parameters.emplace("boundary", ParameterValue{std::string{"wrap"}});

    genome.operators = {std::move(row), std::move(channels), std::move(xors), std::move(jitter)};
    return genome;
}

faultmine::core::ImageBuffer make_pattern(const std::uint32_t width, const std::uint32_t height) {
    auto created = faultmine::core::make_rgba8_image(width, height);
    if (!created.ok()) throw std::runtime_error(created.error->message);
    faultmine::core::ImageBuffer image = std::move(*created.image);
    for (std::size_t index = 0U; index < image.bytes.size(); ++index) {
        image.bytes[index] = static_cast<std::uint8_t>((index * 41U + 13U) & 0xffU);
    }
    return image;
}

const faultmine::core::OperatorInstance* find_instance(
    const faultmine::core::Genome& genome,
    const faultmine::core::InstanceId id) {
    const auto found = std::find_if(
        genome.operators.begin(), genome.operators.end(),
        [id](const faultmine::core::OperatorInstance& instance) { return instance.instance_id == id; });
    return found == genome.operators.end() ? nullptr : &*found;
}

std::optional<std::size_t> find_index(
    const faultmine::core::Genome& genome,
    const faultmine::core::InstanceId id) {
    for (std::size_t index = 0U; index < genome.operators.size(); ++index) {
        if (genome.operators[index].instance_id == id) return index;
    }
    return std::nullopt;
}

faultmine::core::MutationResult child(
    const faultmine::core::Genome& parent,
    const faultmine::core::OperatorRegistry& registry,
    const std::uint64_t index,
    const faultmine::core::MutationRadius radius,
    const faultmine::core::MutationLocks& locks = {},
    const std::uint64_t seed = 0xfeedfacecafebeefULL) {
    faultmine::core::MutationRequest request;
    request.mutation_seed = faultmine::core::RootSeed{seed};
    request.descendant_index = index;
    request.radius = radius;
    request.locks = locks;
    return faultmine::core::generate_descendant(parent, registry, request);
}

void test_catalogue_metadata_complete() {
    const auto registry = faultmine::core::make_default_fault_registry();
    const auto error = faultmine::core::validate_mutation_descriptors(registry.schema_registry());
    expect(!error.has_value(), "default fault catalogue has complete mutation metadata");

    faultmine::core::OperatorRegistry malformed;
    faultmine::core::OperatorDescriptor descriptor;
    descriptor.type_id = "test.bad";
    descriptor.parameters.push_back(faultmine::core::ParameterDescriptor{
        "x", faultmine::core::ParameterKind::signed_integer, true, faultmine::core::MutationMetadata{}});
    std::string registration_error;
    expect(malformed.register_operator(std::move(descriptor), &registration_error), "synthetic malformed schema registers structurally");
    expect(
        faultmine::core::validate_mutation_descriptors(malformed).has_value(),
        "mutation layer detects opaque/incomplete mutable descriptor metadata");
}

void test_same_request_and_index_independence() {
    using namespace faultmine::core;
    const FaultRegistry registry = make_default_fault_registry();
    const Genome parent = make_parent();

    const MutationResult first = child(parent, registry.schema_registry(), 7U, MutationRadius::medium);
    const MutationResult second = child(parent, registry.schema_registry(), 7U, MutationRadius::medium);
    expect(first.ok() && second.ok(), "same descendant request succeeds");
    if (first.ok() && second.ok()) {
        expect_equal(
            serialize_canonical_genome(*first.genome),
            serialize_canonical_genome(*second.genome),
            "same parent/seed/radius/index reproduces byte-identical canonical genome");
    }

    for (std::uint64_t index = 0U; index < 7U; ++index) {
        (void)child(parent, registry.schema_registry(), index, MutationRadius::medium);
    }
    const MutationResult after_lower = child(parent, registry.schema_registry(), 7U, MutationRadius::medium);
    expect(first.ok() && after_lower.ok(), "descendant remains generatable after lower indices");
    if (first.ok() && after_lower.ok()) {
        expect_equal(
            serialize_canonical_genome(*first.genome),
            serialize_canonical_genome(*after_lower.genome),
            "descendant N is independent of generation of lower indices");
    }

    std::map<std::uint64_t, std::string> serial;
    for (std::uint64_t index = 0U; index < 12U; ++index) {
        const MutationResult generated = child(parent, registry.schema_registry(), index, MutationRadius::high);
        expect(generated.ok(), "serial high-radius descendant succeeds");
        if (generated.ok()) serial.emplace(index, serialize_canonical_genome(*generated.genome));
    }
    const std::vector<std::uint64_t> shuffled{9U, 1U, 11U, 0U, 7U, 3U, 10U, 2U, 8U, 5U, 4U, 6U};
    for (const std::uint64_t index : shuffled) {
        const MutationResult generated = child(parent, registry.schema_registry(), index, MutationRadius::high);
        expect(generated.ok(), "shuffled high-radius descendant succeeds");
        if (generated.ok()) {
            expect_equal(
                serialize_canonical_genome(*generated.genome), serial[index],
                "shuffled/parallel-equivalent generation order does not change descendant");
        }
    }
}

void test_locks_and_documented_noop() {
    using namespace faultmine::core;
    const FaultRegistry registry = make_default_fault_registry();
    const Genome parent = make_parent();

    MutationLocks locks;
    locks.operators.push_back(parent.operators[0].instance_id);
    locks.parameters.push_back(MutationParameterLock{parent.operators[2].instance_id, "mask"});
    for (std::uint64_t index = 0U; index < 96U; ++index) {
        const MutationResult generated = child(parent, registry.schema_registry(), index, MutationRadius::high, locks);
        expect(generated.ok(), "locked high-radius descendant succeeds");
        if (!generated.ok()) continue;
        const OperatorInstance* locked_operator = find_instance(*generated.genome, parent.operators[0].instance_id);
        expect(locked_operator != nullptr, "whole-operator lock prevents deletion/substitution");
        if (locked_operator != nullptr) {
            expect_equal(locked_operator->parameters, parent.operators[0].parameters, "whole-operator lock preserves every gene");
            expect_equal(
                find_index(*generated.genome, parent.operators[0].instance_id),
                find_index(parent, parent.operators[0].instance_id),
                "whole-operator lock prevents topology reorder across the locked instance");
        }
        const OperatorInstance* parameter_owner = find_instance(*generated.genome, parent.operators[2].instance_id);
        expect(parameter_owner != nullptr, "parameter-locked operator is not deleted/substituted");
        if (parameter_owner != nullptr) {
            expect_equal(
                parameter_owner->parameters.at("mask"), parent.operators[2].parameters.at("mask"),
                "parameter lock preserves exact gene value");
        }
    }

    MutationLocks all_locked;
    for (const OperatorInstance& instance : parent.operators) all_locked.operators.push_back(instance.instance_id);
    const MutationResult no_op = child(parent, registry.schema_registry(), 3U, MutationRadius::high, all_locked);
    expect(no_op.ok(), "fully locked parent returns a valid descendant result");
    if (no_op.ok()) {
        expect(!no_op.changed, "fully locked parent is an intentional no-op");
        expect(!no_op.no_change_reason.empty(), "intentional no-op carries documented reason");
        expect_equal(*no_op.genome, parent, "intentional no-op preserves parent exactly");
    }
}

void test_low_radius_locality() {
    using namespace faultmine::core;
    const FaultRegistry registry = make_default_fault_registry();
    const Genome parent = make_parent();

    for (std::uint64_t index = 0U; index < 80U; ++index) {
        const MutationResult generated = child(parent, registry.schema_registry(), index, MutationRadius::low);
        expect(generated.ok(), "low-radius descendant succeeds");
        if (!generated.ok()) continue;
        expect(!generated.topology_changed, "low radius preserves topology");
        expect_equal(generated.genome->operators.size(), parent.operators.size(), "low radius preserves stack size");

        std::size_t changed_parameters = 0U;
        for (std::size_t op = 0U; op < parent.operators.size(); ++op) {
            const OperatorInstance& before = parent.operators[op];
            const OperatorInstance& after = generated.genome->operators[op];
            expect_equal(after.instance_id, before.instance_id, "low radius preserves instance identity/order");
            const OperatorDescriptor* descriptor = registry.schema_registry().find(before.type_id);
            expect(descriptor != nullptr, "low locality test resolves descriptor");
            if (descriptor == nullptr) continue;
            for (const ParameterDescriptor& parameter : descriptor->parameters) {
                const auto before_value = before.parameters.find(parameter.name);
                const auto after_value = after.parameters.find(parameter.name);
                if (before_value == before.parameters.end() || after_value == after.parameters.end() ||
                    before_value->second == after_value->second) {
                    continue;
                }
                ++changed_parameters;
                if (parameter.mutation.domain == MutationDomain::signed_range) {
                    const auto a = std::get<std::int64_t>(before_value->second);
                    const auto b = std::get<std::int64_t>(after_value->second);
                    const std::uint64_t distance = a > b
                        ? static_cast<std::uint64_t>(a - b)
                        : static_cast<std::uint64_t>(b - a);
                    expect_equal(distance, static_cast<std::uint64_t>(parameter.mutation.signed_step), "low signed mutation moves one descriptor step");
                } else if (parameter.mutation.domain == MutationDomain::unsigned_range) {
                    const auto a = std::get<std::uint64_t>(before_value->second);
                    const auto b = std::get<std::uint64_t>(after_value->second);
                    const std::uint64_t distance = a > b ? a - b : b - a;
                    expect_equal(distance, parameter.mutation.unsigned_step, "low unsigned mutation moves one descriptor step");
                } else if (parameter.mutation.domain == MutationDomain::bitmask) {
                    const auto a = std::get<std::uint64_t>(before_value->second);
                    const auto b = std::get<std::uint64_t>(after_value->second);
                    expect_equal(std::popcount(a ^ b), 1, "low bitmask mutation flips exactly one bit");
                } else if (parameter.mutation.domain == MutationDomain::choice) {
                    const std::string& a = std::get<std::string>(before_value->second);
                    const std::string& b = std::get<std::string>(after_value->second);
                    const auto ia = std::find(parameter.mutation.choices.begin(), parameter.mutation.choices.end(), a);
                    const auto ib = std::find(parameter.mutation.choices.begin(), parameter.mutation.choices.end(), b);
                    expect(ia != parameter.mutation.choices.end() && ib != parameter.mutation.choices.end(), "choice values remain declared");
                    if (ia != parameter.mutation.choices.end() && ib != parameter.mutation.choices.end()) {
                        const auto da = std::distance(parameter.mutation.choices.begin(), ia);
                        const auto db = std::distance(parameter.mutation.choices.begin(), ib);
                        expect(da - db == 1 || db - da == 1, "low choice mutation moves to adjacent declared alternative");
                    }
                }
            }
        }
        expect_equal(changed_parameters, std::size_t{1U}, "low radius mutates one local gene");
    }
}

void test_high_topology_valid_and_ids_stable() {
    using namespace faultmine::core;
    const FaultRegistry registry = make_default_fault_registry();
    const Genome parent = make_parent();
    const ImageBuffer source = make_pattern(7U, 5U);
    bool saw_insert = false;
    bool saw_substitute = false;

    for (std::uint64_t index = 0U; index < 256U; ++index) {
        const MutationResult generated = child(parent, registry.schema_registry(), index, MutationRadius::high);
        expect(generated.ok(), "high-radius descendant succeeds");
        if (!generated.ok()) continue;
        expect(!validate_genome(*generated.genome, registry.schema_registry()).has_value(), "high topology output is registry-valid");
        if (index < 64U) {
            const RenderResult rendered = render_pipeline(source, *generated.genome, registry);
            expect(rendered.ok(), "representative high-radius descendants execute as valid pipelines");
        }

        if (generated.genome->operators.size() == parent.operators.size() + 1U) saw_insert = true;
        if (generated.genome->operators.size() == parent.operators.size()) {
            for (const OperatorInstance& instance : generated.genome->operators) {
                if (find_instance(parent, instance.instance_id) == nullptr) {
                    saw_substitute = true;
                    break;
                }
            }
        }

        if (generated.topology_changed) {
            const MutationResult repeated = child(parent, registry.schema_registry(), index, MutationRadius::high);
            expect(repeated.ok(), "topology descendant repeats");
            if (repeated.ok()) {
                expect_equal(
                    serialize_canonical_genome(*generated.genome),
                    serialize_canonical_genome(*repeated.genome),
                    "inserted/substituted stable IDs are deterministic for same request");
            }
        }
    }
    expect(saw_insert, "high-radius search exercises deterministic insertion");
    expect(saw_substitute, "high-radius search exercises deterministic substitution with a new stable ID");
}

void test_explicit_seed_changes_population() {
    using namespace faultmine::core;
    const FaultRegistry registry = make_default_fault_registry();
    const Genome parent = make_parent();
    std::vector<std::string> first;
    std::vector<std::string> second;
    for (std::uint64_t index = 0U; index < 8U; ++index) {
        const MutationResult a = child(parent, registry.schema_registry(), index, MutationRadius::medium, {}, 1U);
        const MutationResult b = child(parent, registry.schema_registry(), index, MutationRadius::medium, {}, 2U);
        expect(a.ok() && b.ok(), "explicit-seed descendants generate");
        if (a.ok()) first.push_back(serialize_canonical_genome(*a.genome));
        if (b.ok()) second.push_back(serialize_canonical_genome(*b.genome));
    }
    expect(first != second, "changing explicit mutation seed deterministically changes population");
    for (std::uint64_t index = 0U; index < first.size(); ++index) {
        const MutationResult repeated = child(parent, registry.schema_registry(), index, MutationRadius::medium, {}, 1U);
        expect(repeated.ok(), "seed-one population repeats");
        if (repeated.ok()) expect_equal(serialize_canonical_genome(*repeated.genome), first[index], "seed-one population is reproducible");
    }
}

void test_tray_proxy_promotion_and_history_boundary() {
    using namespace faultmine;
    app::SessionModel session;
    core::ImageBuffer source = make_pattern(18U, 10U);
    const std::string identity = core::source_identity_hex(source);
    std::string error;
    expect(session.set_source(source, identity, L"tray.png", &error), "tray test loads canonical source");
    expect(session.nudge_parameter(0U, "amount", 1, false).ok(), "manual edit succeeds before tray generation");
    const std::string parent_identity = session.genome_identity();
    const bool undo_before = session.can_undo();

    app::SpecimenTrayModel tray;
    app::SpecimenTrayConfig config;
    config.population_size = 4U;
    config.thumbnail_proxy = core::ProxySpec{6U, 6U, core::kProxyMethodVersion};
    expect(tray.generate(session.genome(), session.locks(), session.registry(), config, &error), "tray generation succeeds");
    expect_equal(session.genome_identity(), parent_identity, "tray generation does not mutate active parent");
    expect_equal(session.can_undo(), undo_before, "tray generation does not enter manual edit history");

    const std::uint64_t token = tray.generation_token();
    while (tray.busy()) {
        expect(
            tray.render_next(source, identity, session.registry(), token, &error),
            "incremental tray render advances one specimen");
    }
    expect(!tray.items().empty() && tray.items().front().preview.has_value(), "tray produces deterministic proxy thumbnail");
    if (tray.items().empty() || !tray.items().front().preview.has_value()) return;

    const auto proxy = core::make_nearest_proxy(source, identity, config.thumbnail_proxy);
    expect(proxy.ok(), "direct thumbnail proxy succeeds");
    if (proxy.ok()) {
        const core::ImageBuffer& proxy_input = proxy.proxy->is_proxy ? proxy.proxy->image : source;
        const core::RenderResult direct_proxy = core::render_pipeline(proxy_input, tray.items().front().genome, session.registry());
        expect(direct_proxy.ok(), "direct child proxy render succeeds");
        if (direct_proxy.ok()) {
            expect_equal(
                core::source_identity_hex(*direct_proxy.image),
                core::source_identity_hex(*tray.items().front().preview),
                "thumbnail pixels come from exact generated child genome");
        }
    }

    const core::Genome promoted = tray.items().front().genome;
    expect(session.promote_exploration_genome(promoted, &error), "selected specimen promotes after full canonical validation");
    expect_equal(session.genome(), promoted, "promotion adopts generated child genome, not thumbnail pixels");
    const auto full = session.render_full(&error);
    const core::RenderResult direct_full = core::render_pipeline(source, promoted, session.registry());
    expect(full.has_value() && direct_full.ok(), "promoted child has full canonical output");
    if (full.has_value() && direct_full.ok()) {
        expect_equal(
            core::source_identity_hex(*full),
            core::source_identity_hex(*direct_full.image),
            "promoted full render equals direct canonical child render");
    }
}

void test_pin_survives_reroll_and_obsolete_jobs_are_safe() {
    using namespace faultmine;
    app::SessionModel session;
    core::ImageBuffer source = make_pattern(9U, 6U);
    const std::string identity = core::source_identity_hex(source);
    std::string error;
    expect(session.set_source(source, identity, L"pins.png", &error), "pin test source loads");

    app::SpecimenTrayModel tray;
    app::SpecimenTrayConfig config;
    config.population_size = 4U;
    expect(tray.generate(session.genome(), session.locks(), session.registry(), config, &error), "first pin generation succeeds");
    const std::uint64_t obsolete = tray.generation_token();
    expect(tray.toggle_pin(0U), "first specimen pins");
    const std::string pinned_identity = core::genome_identity_hex(tray.items().front().genome);

    config.mutation_seed = app::SpecimenTrayModel::reroll_seed(config.mutation_seed);
    expect(tray.generate(session.genome(), session.locks(), session.registry(), config, &error), "reroll generation succeeds");
    expect(!tray.items().empty() && tray.items().front().pinned, "pinned specimen survives reroll");
    if (!tray.items().empty()) {
        expect_equal(core::genome_identity_hex(tray.items().front().genome), pinned_identity, "pin keeps exact old child genome");
    }
    expect(
        !tray.render_next(source, identity, session.registry(), obsolete, &error),
        "obsolete generation token cannot mutate rerolled tray");
}

}  // namespace

int main() {
    test_catalogue_metadata_complete();
    test_same_request_and_index_independence();
    test_locks_and_documented_noop();
    test_low_radius_locality();
    test_high_topology_valid_and_ids_stable();
    test_explicit_seed_changes_population();
    test_tray_proxy_promotion_and_history_boundary();
    test_pin_survives_reroll_and_obsolete_jobs_are_safe();

    if (g_failures != 0) {
        std::cerr << g_failures << " mutation/tray contract test(s) failed\n";
        return 1;
    }
    std::cout << "FM-009 deterministic mutation/tray contracts passed\n";
    return 0;
}
