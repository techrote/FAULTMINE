#include "faultmine/crossover.hpp"
#include "faultmine/fault_catalogue.hpp"
#include "faultmine/image.hpp"
#include "faultmine/lineage.hpp"
#include "faultmine/mutation.hpp"
#include "faultmine/project.hpp"
#include "faultmine/session.hpp"
#include "faultmine/starter_operators.hpp"

#include <algorithm>
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

faultmine::core::OperatorInstance make_row(
    const faultmine::core::InstanceId id,
    const std::int64_t amount) {
    using namespace faultmine::core;
    OperatorInstance row;
    row.instance_id = id;
    row.type_id = kFaultRowOffset;
    row.type_version = 1U;
    row.enabled = true;
    row.parameters.emplace("amount", ParameterValue{amount});
    row.parameters.emplace("boundary", ParameterValue{std::string{"wrap"}});
    return row;
}

faultmine::core::OperatorInstance make_xor(const faultmine::core::InstanceId id) {
    using namespace faultmine::core;
    OperatorInstance xors;
    xors.instance_id = id;
    xors.type_id = kFaultByteXor;
    xors.type_version = 1U;
    xors.enabled = true;
    xors.parameters.emplace("mask", ParameterValue{std::uint64_t{7U}});
    xors.parameters.emplace("channels", ParameterValue{std::string{"rb"}});
    return xors;
}

faultmine::core::Genome make_parent(
    const std::uint64_t seed,
    const faultmine::core::InstanceId row_id,
    const std::int64_t amount,
    const bool with_unmatched) {
    faultmine::core::Genome genome;
    genome.root_seed = faultmine::core::RootSeed{seed};
    genome.operators.push_back(make_row(row_id, amount));
    if (with_unmatched) genome.operators.push_back(make_xor(faultmine::core::InstanceId{9U, 9U}));
    return genome;
}

faultmine::core::ImageBuffer make_source() {
    auto created = faultmine::core::make_rgba8_image(8U, 6U);
    if (!created.ok()) throw std::runtime_error(created.error->message);
    auto image = std::move(*created.image);
    for (std::size_t index = 0U; index < image.bytes.size(); ++index) {
        image.bytes[index] = static_cast<std::uint8_t>((index * 29U + 11U) & 0xffU);
    }
    return image;
}

faultmine::core::MutationLocks mutation_locks(const faultmine::app::LockState& locks) {
    faultmine::core::MutationLocks converted;
    converted.operators = locks.operators;
    for (const auto& lock : locks.parameters) {
        converted.parameters.push_back(faultmine::core::MutationParameterLock{lock.instance_id, lock.parameter});
    }
    return converted;
}

void test_crossover_contract() {
    using namespace faultmine;
    const core::FaultRegistry registry = core::make_default_fault_registry();
    const core::Genome a = make_parent(1U, core::InstanceId{1U, 1U}, 1, false);
    const core::Genome b = make_parent(2U, core::InstanceId{2U, 2U}, 5, true);

    core::CrossoverParent pa{a, {}};
    pa.locks.parameters.push_back(core::CrossoverParameterLock{a.operators[0].instance_id, "amount"});
    core::CrossoverParent pb{b, {}};
    pb.locks.operators.push_back(b.operators[1].instance_id);

    core::CrossoverRequest request;
    request.crossover_seed = core::RootSeed{0x1010101010101010ULL};
    const auto first = core::crossover_genomes({pa, pb}, registry.schema_registry(), request);
    const auto reversed = core::crossover_genomes({pb, pa}, registry.schema_registry(), request);
    expect(first.ok() && reversed.ok(), "typed compatible parents cross successfully");
    if (!first.ok() || !reversed.ok()) return;

    expect_equal(core::serialize_canonical_genome(*first.genome), core::serialize_canonical_genome(*reversed.genome),
        "parent selection order is normalized by canonical identity");
    expect_equal(first.provenance.parent_genome_identities, reversed.provenance.parent_genome_identities,
        "parent identities are stable provenance inputs");
    expect(!core::validate_genome(*first.genome, registry.schema_registry()).has_value(),
        "crossover child is registry-valid before rendering");
    expect_equal(first.genome->operators.size(), std::size_t{2U},
        "protected unmatched operator is conservatively inherited");
    expect_equal(std::get<std::int64_t>(first.genome->operators[0].parameters.at("amount")), std::int64_t{1},
        "protected aligned gene survives crossover");
    expect(first.genome->operators[0].instance_id != a.operators[0].instance_id &&
        first.genome->operators[0].instance_id != b.operators[0].instance_id,
        "independently identified aligned operators receive deterministic synthesized identity");

    const auto repeated = core::crossover_genomes({pa, pb}, registry.schema_registry(), request);
    expect(repeated.ok(), "same crossover request repeats");
    if (repeated.ok()) {
        expect_equal(core::serialize_canonical_genome(*repeated.genome), core::serialize_canonical_genome(*first.genome),
            "same parents/policy/seed reproduce byte-identical child genome");
    }

    core::CrossoverParent ca{a, {}};
    core::CrossoverParent cb{b, {}};
    ca.locks.parameters.push_back(core::CrossoverParameterLock{a.operators[0].instance_id, "amount"});
    cb.locks.parameters.push_back(core::CrossoverParameterLock{b.operators[0].instance_id, "amount"});
    const auto conflict = core::crossover_genomes({ca, cb}, registry.schema_registry(), request);
    expect(!conflict.ok() && conflict.error.has_value() && conflict.error->code == core::CrossoverErrorCode::protected_conflict,
        "conflicting protected genes fail explicitly rather than violating a lock");

    core::Genome invalid = a;
    invalid.operators[0].type_id = "fault.not-registered";
    expect(!core::crossover_genomes({core::CrossoverParent{invalid, {}}, pb}, registry.schema_registry(), request).ok(),
        "invalid candidate is rejected before rendering");
}

void test_lineage_graph_contract() {
    using namespace faultmine;
    const core::FaultRegistry registry = core::make_default_fault_registry();
    const std::string source_identity(64U, 'a');
    const core::Genome root = make_parent(1U, core::InstanceId{1U, 1U}, 1, false);
    const core::Genome child = make_parent(3U, core::InstanceId{1U, 1U}, 2, false);
    const core::Genome grandchild = make_parent(4U, core::InstanceId{1U, 1U}, 3, false);

    app::LineageGraph graph;
    std::string error;
    expect(graph.reset_root(source_identity, root, {}, app::DerivationKind::manual_root, &error), "lineage root initializes");
    const std::string root_id = core::genome_identity_hex(root);
    const std::string child_id = core::genome_identity_hex(child);
    const std::string grandchild_id = core::genome_identity_hex(grandchild);

    app::SpecimenDerivation mutation;
    mutation.kind = app::DerivationKind::mutation;
    mutation.policy_version = core::kMutationPolicyVersion;
    mutation.seed = core::RootSeed{7U};
    mutation.descendant_index = 2U;
    mutation.mutation_radius = "medium";
    mutation.parent_specimen_ids = {root_id};
    expect(graph.retain(source_identity, child, {}, mutation, true, registry.schema_registry(), &error),
        "mutation child retains explicit provenance");
    expect(graph.retain(source_identity, child, {}, mutation, false, registry.schema_registry(), &error),
        "duplicate canonical specimen merges safely");
    expect_equal(graph.state().specimens.size(), std::size_t{2U}, "duplicate identity does not explode graph");
    expect(graph.find(child_id) != nullptr && graph.find(child_id)->favourite,
        "duplicate merge does not erase favourite state");

    app::SpecimenDerivation crossover;
    crossover.kind = app::DerivationKind::crossover;
    crossover.policy_version = core::kCrossoverPolicyVersion;
    crossover.seed = core::RootSeed{9U};
    crossover.mutation_radius = "none";
    crossover.parent_specimen_ids = {root_id, child_id};
    expect(graph.retain(source_identity, grandchild, {}, crossover, false, registry.schema_registry(), &error),
        "multi-parent crossover specimen is retained");
    expect_equal(graph.parents_of(grandchild_id).size(), std::size_t{2U}, "both crossover parents remain inspectable");
    const auto root_children = graph.children_of(root_id);
    expect(std::find(root_children.begin(), root_children.end(), grandchild_id) != root_children.end(),
        "descendant navigation is distinct from edit history");

    app::SpecimenDerivation cycle;
    cycle.kind = app::DerivationKind::crossover;
    cycle.policy_version = core::kCrossoverPolicyVersion;
    cycle.seed = core::RootSeed{10U};
    cycle.mutation_radius = "none";
    cycle.parent_specimen_ids = {child_id, grandchild_id};
    expect(!graph.retain(source_identity, root, {}, cycle, false, registry.schema_registry(), &error),
        "cycle-forming provenance is rejected");
    expect(!app::LineageGraph::validate_state(graph.state(), registry.schema_registry(), source_identity).has_value(),
        "lineage remains a valid DAG after rejected cycle");
}

void test_project_v2_and_v1_migration() {
    using namespace faultmine;
    const core::FaultRegistry registry = core::make_default_fault_registry();
    const core::Genome root = make_parent(1U, core::InstanceId{1U, 1U}, 1, false);
    const core::Genome child = make_parent(3U, core::InstanceId{1U, 1U}, 2, false);
    const std::string source_identity(64U, 'b');

    app::LineageGraph graph;
    std::string error;
    expect(graph.reset_root(source_identity, root, {}, app::DerivationKind::manual_root, &error), "project lineage root initializes");
    app::SpecimenDerivation mutation;
    mutation.kind = app::DerivationKind::mutation;
    mutation.policy_version = core::kMutationPolicyVersion;
    mutation.seed = core::RootSeed{0x55U};
    mutation.descendant_index = 4U;
    mutation.mutation_radius = "low";
    mutation.parent_specimen_ids = {core::genome_identity_hex(root)};
    expect(graph.retain(source_identity, child, {}, mutation, true, registry.schema_registry(), &error), "project child retained");
    expect(graph.set_active(core::genome_identity_hex(child)), "project child activated");

    app::ProjectDocument project;
    project.source.path_utf8 = "C:/art/source.png";
    project.source.source_identity = source_identity;
    project.genome = child;
    project.lineage = graph.state();
    const std::string canonical = app::serialize_project_canonical(project);
    const auto parsed = app::parse_project(canonical, registry.schema_registry());
    expect(parsed.ok(), "project v2 with lineage parses");
    if (parsed.ok()) {
        expect_equal(app::serialize_project_canonical(*parsed.project), canonical, "project v2 round-trip is canonical");
        expect_equal(parsed.project->lineage.active_specimen_id, core::genome_identity_hex(child), "active specimen survives reload");
        expect(std::any_of(parsed.project->lineage.specimens.begin(), parsed.project->lineage.specimens.end(),
            [](const app::SpecimenRecord& record) { return record.favourite; }), "favourite survives reload");
    }

    std::string genome_text = core::serialize_canonical_genome(root);
    if (!genome_text.empty() && genome_text.back() == '\n') genome_text.pop_back();
    const std::string legacy =
        "{\"project_version\":1,\"source\":{\"path\":\"C:/old.png\",\"identity\":\"" + source_identity +
        "\"},\"genome\":" + genome_text +
        ",\"locks\":[],\"session\":{\"proxy_enabled\":true,\"proxy_max_width\":1280,\"proxy_max_height\":960,\"proxy_method_version\":1,\"selected_instance_id\":\"\"},"
        "\"ui\":{\"view_mode\":\"fit\",\"zoom_milli\":1000,\"pan_x_milli\":0,\"pan_y_milli\":0,\"show_before\":false}}\n";
    const auto migrated = app::parse_project(legacy, registry.schema_registry());
    expect(migrated.ok(), "pre-lineage project v1 migrates explicitly");
    if (migrated.ok()) {
        expect_equal(migrated.project->lineage.specimens.size(), std::size_t{1U}, "migration creates one truthful root");
        const auto& derivation = migrated.project->lineage.specimens.front().derivations.front();
        expect(derivation.kind == app::DerivationKind::legacy_project_root, "migration records legacy-root provenance");
        expect(derivation.parent_specimen_ids.empty(), "migration invents no historical parents");
    }
}

void test_session_breed_navigation_and_reload() {
    using namespace faultmine;
    core::ImageBuffer source = make_source();
    const std::string identity = core::source_identity_hex(source);
    app::SessionModel session;
    std::string error;
    expect(session.set_source(source, identity, L"C:\\art\\source.png", &error), "session accepts canonical source");

    core::MutationRequest request;
    request.mutation_seed = core::RootSeed{0xabcddcba01234567ULL};
    request.radius = core::MutationRadius::medium;
    request.locks = mutation_locks(session.locks());
    request.descendant_index = 0U;
    const auto first = core::generate_descendant(session.genome(), session.registry().schema_registry(), request);
    request.descendant_index = 1U;
    const auto second = core::generate_descendant(session.genome(), session.registry().schema_registry(), request);
    expect(first.ok() && second.ok(), "two deterministic mutation descendants are available for breeding");
    if (!first.ok() || !second.ok()) return;

    expect(session.set_mutation_specimen_favourite(*first.genome, first.provenance, true, &error),
        "favourite action durably retains descendant");
    bool selected = false;
    expect(session.toggle_crossover_parent(*first.genome, first.provenance, &selected, &error) && selected,
        "first parent is selected");
    expect(session.toggle_crossover_parent(*second.genome, second.provenance, &selected, &error) && selected,
        "second parent is selected");
    expect_equal(session.crossover_parent_selection().size(), std::size_t{2U}, "multi-parent selection is explicit");
    expect(session.breed_selected(core::RootSeed{0x777788889999aaaaULL}, &error), "selected specimens breed deterministically");
    const std::string child_id = session.genome_identity();
    expect_equal(session.lineage().parents_of(child_id).size(), std::size_t{2U}, "bred specimen records both parents");
    expect(!session.can_undo(), "lineage promotion is not manual edit undo history");
    expect(session.navigate_lineage_parent(&error), "ancestry navigation succeeds");
    expect(session.navigate_lineage_child(&error), "descendant navigation succeeds independently of undo");
    expect_equal(session.genome_identity(), child_id, "descendant navigation returns to bred specimen");
    expect(session.active_provenance_summary().find("crossover") != std::string::npos,
        "active provenance inspection exposes crossover metadata");

    const auto document = session.make_project_document(&error);
    expect(document.has_value(), "evolutionary session emits project document");
    if (!document.has_value()) return;
    const auto parsed = app::parse_project(app::serialize_project_canonical(*document), session.registry().schema_registry());
    expect(parsed.ok(), "saved evolutionary project parses");
    if (!parsed.ok()) return;

    app::SessionModel restored;
    expect(restored.load_project_state(*parsed.project, source, L"D:\\moved\\source.png", &error),
        "same canonical source reloads evolutionary project");
    expect_equal(restored.genome_identity(), child_id, "reload restores same active specimen");
    expect_equal(restored.lineage().parents_of(child_id).size(), std::size_t{2U}, "reload preserves crossover edges");
    expect_equal(restored.lineage().favourites().size(), std::size_t{1U}, "reload preserves favourite");

    core::ImageBuffer changed = source;
    changed.bytes[0] ^= 1U;
    expect(!restored.load_project_state(*parsed.project, changed, L"D:\\moved\\changed.png", &error),
        "source identity mismatch remains detected after lineage persistence");
}

}  // namespace

int main() {
    try {
        test_crossover_contract();
        test_lineage_graph_contract();
        test_project_v2_and_v1_migration();
        test_session_breed_navigation_and_reload();
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
