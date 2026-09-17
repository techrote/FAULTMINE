#include "faultmine/crossover.hpp"
#include "faultmine/fault_catalogue.hpp"
#include "faultmine/image.hpp"
#include "faultmine/lineage.hpp"
#include "faultmine/mutation.hpp"
#include "faultmine/project.hpp"
#include "faultmine/session.hpp"
#include "faultmine/starter_operators.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
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

faultmine::core::Genome make_parent_a() {
    using namespace faultmine::core;
    Genome genome;
    genome.root_seed = RootSeed{0x1111222233334444ULL};
    auto row = make_operator(InstanceId{10U, 1U}, kFaultRowOffset);
    row.parameters.emplace("amount", ParameterValue{std::int64_t{2}});
    row.parameters.emplace("boundary", ParameterValue{std::string{"wrap"}});
    auto xors = make_operator(InstanceId{10U, 2U}, kFaultByteXor);
    xors.parameters.emplace("mask", ParameterValue{std::uint64_t{3U}});
    xors.parameters.emplace("channels", ParameterValue{std::string{"rb"}});
    genome.operators = {std::move(row), std::move(xors)};
    return genome;
}

faultmine::core::Genome make_parent_b() {
    using namespace faultmine::core;
    Genome genome;
    genome.root_seed = RootSeed{0x9999aaaabbbbccccULL};
    auto row = make_operator(InstanceId{20U, 1U}, kFaultRowOffset);
    row.parameters.emplace("amount", ParameterValue{std::int64_t{-5}});
    row.parameters.emplace("boundary", ParameterValue{std::string{"clamp"}});
    auto xors = make_operator(InstanceId{20U, 2U}, kFaultByteXor);
    xors.parameters.emplace("mask", ParameterValue{std::uint64_t{0x55U}});
    xors.parameters.emplace("channels", ParameterValue{std::string{"ga"}});
    auto channels = make_operator(InstanceId{20U, 3U}, kFaultChannelPermute);
    channels.parameters.emplace("order", ParameterValue{std::string{"argb"}});
    genome.operators = {std::move(row), std::move(xors), std::move(channels)};
    return genome;
}

faultmine::core::ImageBuffer make_source() {
    auto created = faultmine::core::make_rgba8_image(9U, 7U);
    if (!created.ok()) throw std::runtime_error(created.error->message);
    auto image = std::move(*created.image);
    for (std::size_t index = 0U; index < image.bytes.size(); ++index) {
        image.bytes[index] = static_cast<std::uint8_t>((index * 29U + 17U) & 0xffU);
    }
    return image;
}

void test_typed_crossover_determinism_and_order() {
    using namespace faultmine::core;
    const FaultRegistry registry = make_default_fault_registry();
    const Genome a = make_parent_a();
    const Genome b = make_parent_b();

    CrossoverRequest request;
    request.crossover_seed = RootSeed{0xabcdef0123456789ULL};
    request.parents = {CrossoverParent{a, {}}, CrossoverParent{b, {}}};
    const CrossoverResult first = crossover_genomes(registry.schema_registry(), request);
    const CrossoverResult second = crossover_genomes(registry.schema_registry(), request);
    expect(first.ok() && second.ok(), "same ordered parent set and seed produces crossover child");
    if (first.ok() && second.ok()) {
        expect_equal(
            serialize_canonical_genome(*first.genome),
            serialize_canonical_genome(*second.genome),
            "same crossover request reproduces byte-identical canonical genome");
        expect_equal(first.provenance.parent_genome_identities.size(), std::size_t{2U}, "crossover records both parent identities");
        expect_equal(first.genome->operators.front().instance_id, a.operators.front().instance_id, "first parent is the topology scaffold");
    }

    CrossoverRequest reversed;
    reversed.crossover_seed = request.crossover_seed;
    reversed.parents = {CrossoverParent{b, {}}, CrossoverParent{a, {}}};
    const CrossoverResult swapped = crossover_genomes(registry.schema_registry(), reversed);
    expect(swapped.ok(), "reversed parent order is valid");
    if (swapped.ok()) {
        expect_equal(swapped.genome->operators.front().instance_id, b.operators.front().instance_id, "parent order is explicitly semantic in policy v1");
    }
}

void test_crossover_locks_unmatched_and_invalid_parent() {
    using namespace faultmine::core;
    const FaultRegistry registry = make_default_fault_registry();
    const Genome a = make_parent_a();
    const Genome b = make_parent_b();

    CrossoverParent primary{a, {}};
    primary.locks.operators.push_back(a.operators[0].instance_id);
    primary.locks.parameters.push_back(MutationParameterLock{a.operators[1].instance_id, "mask"});

    bool found_unmatched = false;
    for (std::uint64_t seed = 1U; seed < 128U && !found_unmatched; ++seed) {
        CrossoverRequest request;
        request.crossover_seed = RootSeed{seed};
        request.parents = {primary, CrossoverParent{b, {}}};
        const CrossoverResult child = crossover_genomes(registry.schema_registry(), request);
        expect(child.ok(), "locked crossover candidate validates");
        if (!child.ok()) continue;
        expect_equal(child.genome->operators[0], a.operators[0], "whole-operator lock preserves exact primary anchor");
        expect_equal(
            child.genome->operators[1].parameters.at("mask"),
            a.operators[1].parameters.at("mask"),
            "parameter lock preserves exact protected gene");
        if (child.genome->operators.size() > a.operators.size()) {
            found_unmatched = true;
            const auto& inherited = child.genome->operators.back();
            expect(!(inherited.instance_id == b.operators.back().instance_id), "unmatched inherited operator receives a child-specific instance id");
            const CrossoverResult repeated = crossover_genomes(registry.schema_registry(), request);
            expect(repeated.ok(), "unmatched crossover repeats");
            if (repeated.ok()) {
                expect_equal(repeated.genome->operators.back().instance_id, inherited.instance_id, "new crossover instance id is deterministic");
            }
        }
    }
    expect(found_unmatched, "policy v1 deterministically exercises unmatched-operator inheritance");

    Genome invalid = a;
    invalid.operators.push_back(invalid.operators.front());
    CrossoverRequest bad;
    bad.parents = {CrossoverParent{a, {}}, CrossoverParent{invalid, {}}};
    expect(!crossover_genomes(registry.schema_registry(), bad).ok(), "invalid parent is rejected before crossover/rendering");
}

faultmine::app::SpecimenRecord root_record(
    const faultmine::core::Genome& genome,
    const std::string& source,
    const std::uint64_t ordinal) {
    faultmine::app::SpecimenRecord record;
    record.genome = genome;
    record.genome_identity = faultmine::core::genome_identity_hex(genome);
    record.source_identity = source;
    record.derivation = faultmine::app::make_manual_root_derivation();
    record.creation_ordinal = ordinal;
    return record;
}

void test_lineage_dag_duplicates_and_cycle_rejection() {
    using namespace faultmine;
    const core::FaultRegistry registry = core::make_default_fault_registry();
    const std::string source(64U, 'a');
    const core::Genome root = make_parent_a();

    app::LineageGraph graph;
    std::string error;
    expect(graph.reset_root(source, root, app::DerivationKind::manual_root, registry.schema_registry(), &error), "lineage root initializes");
    const std::string root_id = core::genome_identity_hex(root);

    core::MutationRequest mutation_request;
    mutation_request.mutation_seed = core::RootSeed{77U};
    mutation_request.descendant_index = 0U;
    mutation_request.radius = core::MutationRadius::medium;
    const core::MutationResult mutation = core::generate_descendant(root, registry.schema_registry(), mutation_request);
    expect(mutation.ok(), "mutation child for lineage succeeds");
    if (!mutation.ok()) return;

    app::SpecimenRecord child;
    child.genome = *mutation.genome;
    child.genome_identity = core::genome_identity_hex(child.genome);
    child.source_identity = source;
    child.derivation = app::make_mutation_derivation(mutation.provenance);
    child.favourite = true;
    bool added = false;
    expect(graph.retain(child, registry.schema_registry(), &added, &error) && added, "mutation child retained with parent edge");
    expect_equal(graph.parents(child.genome_identity), std::vector<std::string>{root_id}, "mutation parent edge is explicit");

    const std::size_t before_duplicate = graph.records().size();
    added = true;
    expect(graph.retain(child, registry.schema_registry(), &added, &error), "duplicate genome identity is handled deliberately");
    expect(!added, "duplicate genome collapses to existing lineage node");
    expect_equal(graph.records().size(), before_duplicate, "duplicate genome does not explode lineage graph");
    expect(graph.find(child.genome_identity)->favourite, "favourite state is retained on duplicate collapse");

    app::LineageState cyclic = graph.state();
    expect(cyclic.specimens.size() >= 2U, "cycle fixture has two nodes");
    if (cyclic.specimens.size() >= 2U) {
        cyclic.specimens[0].derivation.kind = app::DerivationKind::mutation;
        cyclic.specimens[0].derivation.parent_genome_identities = {cyclic.specimens[1].genome_identity};
        cyclic.specimens[0].derivation.policy_version = core::kMutationPolicyVersion;
        cyclic.specimens[0].derivation.seed = core::RootSeed{1U};
        cyclic.specimens[0].derivation.descendant_index = 0U;
        cyclic.specimens[0].derivation.mutation_radius = core::MutationRadius::low;
        app::LineageGraph rejected;
        expect(!rejected.load(cyclic, registry.schema_registry(), source, &error), "lineage DAG validation rejects parent cycles");
    }
}

std::string legacy_project_text(
    const faultmine::core::Genome& genome,
    const std::string& source_identity) {
    std::string genome_text = faultmine::core::serialize_canonical_genome(genome);
    if (!genome_text.empty() && genome_text.back() == '\n') genome_text.pop_back();
    return "{\"project_version\":1,\"source\":{\"path\":\"C:/legacy.png\",\"identity\":\"" + source_identity +
        "\"},\"genome\":" + genome_text +
        ",\"locks\":[],\"session\":{\"proxy_enabled\":true,\"proxy_max_width\":1280,\"proxy_max_height\":960,\"proxy_method_version\":1,\"selected_instance_id\":\"\"},"
        "\"ui\":{\"view_mode\":\"fit\",\"zoom_milli\":1000,\"pan_x_milli\":0,\"pan_y_milli\":0,\"show_before\":false}}\n";
}

void test_project_v1_migration_and_v2_lineage_persistence() {
    using namespace faultmine;
    const core::FaultRegistry registry = core::make_default_fault_registry();
    const core::Genome genome = make_parent_a();
    const std::string source(64U, 'b');
    const auto migrated = app::parse_project(legacy_project_text(genome, source), registry.schema_registry());
    expect(migrated.ok(), "pre-lineage project v1 migrates explicitly");
    if (!migrated.ok()) return;
    expect_equal(migrated.project->project_version, app::kProjectSchemaVersion, "v1 migration materializes current project version");
    expect_equal(migrated.project->lineage.specimens.size(), std::size_t{1U}, "v1 migration creates exactly one history-free root");
    if (!migrated.project->lineage.specimens.empty()) {
        const auto& root = migrated.project->lineage.specimens.front();
        expect_equal(root.derivation.kind, app::DerivationKind::migrated_project, "migrated root is labelled rather than inventing parents");
        expect(root.derivation.parent_genome_identities.empty(), "migrated root has no fabricated historical parent");
    }
    const std::string v2 = app::serialize_project_canonical(*migrated.project);
    expect(v2.find("\"project_version\":2") != std::string::npos, "migrated project serializes as schema v2");
    const auto reparsed = app::parse_project(v2, registry.schema_registry());
    expect(reparsed.ok(), "migrated schema v2 project reloads");
    if (reparsed.ok()) expect_equal(reparsed.project->lineage, migrated.project->lineage, "v2 lineage state round-trips exactly");
}

void test_session_mutate_favourite_breed_navigate_save_reload() {
    using namespace faultmine;
    core::ImageBuffer source = make_source();
    const std::string source_id = core::source_identity_hex(source);
    app::SessionModel session;
    std::string error;
    expect(session.set_source(source, source_id, L"C:\\lineage.png", &error), "session source initializes lineage root");
    const core::Genome parent = session.genome();
    const std::string parent_id = session.genome_identity();

    core::MutationRequest first_request;
    first_request.mutation_seed = core::RootSeed{0x500U};
    first_request.descendant_index = 0U;
    first_request.radius = core::MutationRadius::medium;
    core::MutationRequest second_request = first_request;
    second_request.descendant_index = 1U;
    const core::MutationResult first = core::generate_descendant(parent, session.registry().schema_registry(), first_request);
    const core::MutationResult second = core::generate_descendant(parent, session.registry().schema_registry(), second_request);
    expect(first.ok() && second.ok(), "two sibling descendants generated for breeding");
    if (!first.ok() || !second.ok()) return;

    expect(session.retain_mutation_specimen(*first.genome, first.provenance, true, &error), "first sibling retained as durable favourite");
    expect(session.retain_mutation_specimen(*second.genome, second.provenance, false, &error), "second sibling retained for crossover");
    expect(session.promote_mutation_specimen(*first.genome, first.provenance, true, &error), "mutation promotion records lineage without using undo history");
    expect(!session.can_undo(), "lineage promotion remains separate from manual edit undo/redo");

    const std::vector<core::Genome> parents{*first.genome, *second.genome};
    expect(session.breed_and_promote(parents, core::RootSeed{0xabcU}, &error), "two retained siblings breed and promote deterministically");
    const std::string crossover_id = session.genome_identity();
    const app::SpecimenRecord* crossover = session.lineage().find(crossover_id);
    expect(crossover != nullptr, "crossover child is retained in lineage");
    if (crossover != nullptr) {
        expect_equal(crossover->derivation.kind, app::DerivationKind::crossover, "active child reports crossover provenance");
        expect_equal(crossover->derivation.parent_genome_identities.size(), std::size_t{2U}, "crossover lineage records both ordered parents");
    }

    expect(session.activate_lineage_specimen(parent_id, &error), "lineage navigation returns to retained ancestor");
    expect_equal(session.genome_identity(), parent_id, "ancestor navigation changes active specimen independently of undo");
    expect(session.activate_lineage_specimen(crossover_id, &error), "lineage navigation returns to retained descendant");

    const auto document = session.make_project_document(&error);
    expect(document.has_value(), "lineage session emits project v2");
    if (!document.has_value()) return;
    const std::string text = app::serialize_project_canonical(*document);
    const auto parsed = app::parse_project(text, session.registry().schema_registry());
    expect(parsed.ok(), "lineage project v2 parses after save");
    if (!parsed.ok()) return;

    app::SessionModel restored;
    expect(restored.load_project_state(*parsed.project, source, L"D:\\moved\\lineage.png", &error), "lineage project reloads against moved identical source");
    expect_equal(restored.genome_identity(), crossover_id, "project reload restores active crossover specimen");
    const std::string first_id = core::genome_identity_hex(*first.genome);
    const app::SpecimenRecord* favourite = restored.lineage().find(first_id);
    expect(favourite != nullptr && favourite->favourite, "favourite survives project save/reload");
    expect_equal(restored.lineage().parents(crossover_id).size(), std::size_t{2U}, "crossover edges survive project reload");

    core::ImageBuffer changed = source;
    changed.bytes[0] ^= 1U;
    expect(!restored.load_project_state(*parsed.project, changed, L"D:\\changed.png", &error), "source identity mismatch remains enforced after lineage migration");
}

}  // namespace

int main() {
    try {
        test_typed_crossover_determinism_and_order();
        test_crossover_locks_unmatched_and_invalid_parent();
        test_lineage_dag_duplicates_and_cycle_rejection();
        test_project_v1_migration_and_v2_lineage_persistence();
        test_session_mutate_favourite_breed_navigate_save_reload();
    } catch (const std::exception& exception) {
        ++g_failures;
        std::cerr << "UNCAUGHT TEST EXCEPTION: " << exception.what() << '\n';
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " FM-010 crossover/lineage assertion(s) failed.\n";
        return 1;
    }
    std::cout << "FAULTMINE FM-010 crossover/lineage contracts passed.\n";
    return 0;
}
