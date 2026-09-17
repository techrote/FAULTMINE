#include "faultmine/colour_operators.hpp"
#include "faultmine/editor.hpp"
#include "faultmine/image.hpp"
#include "faultmine/memory_addressing_operators.hpp"
#include "faultmine/project.hpp"
#include "faultmine/representation_bit_operators.hpp"
#include "faultmine/session.hpp"

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

faultmine::core::ImageBuffer make_source(const std::uint32_t width = 5U, const std::uint32_t height = 3U) {
    auto created = faultmine::core::make_rgba8_image(width, height);
    if (!created.ok()) {
        throw std::runtime_error(created.error->message);
    }
    auto image = std::move(*created.image);
    for (std::size_t index = 0U; index < image.bytes.size(); ++index) {
        image.bytes[index] = static_cast<std::uint8_t>((index * 31U + 7U) & 0xffU);
    }
    return image;
}

void test_descriptor_driven_editing() {
    using namespace faultmine;
    app::EditorModel editor;
    expect(editor.operator_descriptors().size() > 20U, "full default catalogue is enumerable by the generic editor");

    auto result = editor.add_operator(core::kFaultAddressOffset, 0U);
    expect(result.ok(), "generic add creates FM-005 operator from descriptor metadata");
    expect_equal(editor.genome().operators.size(), std::size_t{1U}, "one operator added");
    result = editor.set_parameter_from_text(0U, "offset_pixels", "0x2");
    expect(result.ok(), "signed exact entry accepts hexadecimal");
    expect_equal(std::get<std::int64_t>(editor.genome().operators[0].parameters.at("offset_pixels")), std::int64_t{2}, "exact entry updates typed genome value");
    result = editor.set_parameter_from_text(0U, "offset_pixels", "999999999");
    expect(!result.ok(), "descriptor range rejects hostile exact entry");

    result = editor.add_operator(core::kFaultBitplaneSwap, 1U);
    expect(result.ok(), "generic add creates FM-006 operator");
    result = editor.set_parameter_from_text(1U, "channels", "purple");
    expect(!result.ok(), "choice metadata rejects invalid enum text");

    result = editor.add_operator(core::kColourQuantize, 2U);
    expect(result.ok(), "generic add creates FM-007 operator");
    result = editor.set_parameter_from_text(2U, "levels", "16");
    expect(result.ok(), "generic unsigned parameter edit succeeds");
    result = editor.set_parameter_from_text(2U, "levels", "1");
    expect(!result.ok(), "quantisation lower bound is enforced by descriptor metadata");
}

void test_duplicate_reorder_history_and_ids() {
    using namespace faultmine;
    app::EditorModel editor;
    expect(editor.add_operator(core::kFaultAddressOffset, 0U).ok(), "base operator added");
    const core::InstanceId original = editor.genome().operators[0].instance_id;
    expect(editor.duplicate_operator(0U).ok(), "operator duplicates");
    const core::InstanceId duplicate = editor.genome().operators[1].instance_id;
    expect(!(duplicate == original), "duplicate receives a distinct stable instance id");
    expect(editor.undo(), "duplicate undo succeeds");
    expect_equal(editor.genome().operators.size(), std::size_t{1U}, "undo restores pre-duplicate topology");
    expect(editor.redo(), "duplicate redo succeeds");
    expect_equal(editor.genome().operators[1].instance_id, duplicate, "redo restores the exact duplicate id rather than regenerating it");
    expect(editor.move_operator(1U, 0U).ok(), "reorder succeeds");
    expect_equal(editor.genome().operators[0].instance_id, duplicate, "reorder preserves instance identity");

    expect(editor.set_parameter_from_text(0U, "offset_pixels", "1").ok(), "parameter edit succeeds");
    const std::string before_nudge = editor.genome_identity();
    expect(editor.nudge_parameter(0U, "offset_pixels", 1, false, true).ok(), "first coalesced nudge succeeds");
    expect(editor.nudge_parameter(0U, "offset_pixels", 1, false, true).ok(), "second coalesced nudge succeeds");
    editor.end_coalesced_edit();
    expect(editor.undo(), "coalesced nudge group has one undo boundary");
    expect_equal(editor.genome_identity(), before_nudge, "one undo removes the whole coalesced nudge group");
}

void test_locks_are_noncanonical() {
    using namespace faultmine;
    app::EditorModel editor;
    expect(editor.add_operator(core::kFaultAddressOffset, 0U).ok(), "operator added for lock test");
    expect(editor.set_parameter_from_text(0U, "offset_pixels", "1").ok(), "operator parameter set");
    const std::string identity = editor.genome_identity();
    const core::ImageBuffer source = make_source();
    const auto before = core::render_pipeline(source, editor.genome(), editor.registry());
    expect(before.ok(), "pre-lock canonical render succeeds");

    expect(editor.toggle_operator_lock(0U).ok(), "whole-operator mutation lock toggles");
    expect(editor.toggle_parameter_lock(0U, "offset_pixels").ok(), "gene mutation lock toggles");
    expect(editor.operator_locked(0U), "operator lock reported");
    expect(editor.parameter_locked(0U, "offset_pixels"), "gene lock reported");
    expect_equal(editor.genome_identity(), identity, "locks do not change canonical genome identity");
    const auto after = core::render_pipeline(source, editor.genome(), editor.registry());
    expect(after.ok(), "post-lock canonical render succeeds");
    if (before.ok() && after.ok()) {
        expect_equal(core::source_identity_hex(*before.image), core::source_identity_hex(*after.image), "locks do not change canonical output bytes");
    }
    expect(editor.set_parameter_from_text(0U, "offset_pixels", "2").ok(), "mutation lock does not block deliberate manual edit");
}

void test_project_roundtrip_and_source_classification() {
    using namespace faultmine;
    app::EditorModel editor;
    expect(editor.add_operator(core::kColourPaletteNearest, 0U).ok(), "palette operator added with canonical default asset");
    expect(editor.toggle_parameter_lock(0U, "palette").ok(), "palette gene lock set");

    app::ProjectDocument project;
    project.source.path_utf8 = "C:/art/source.png";
    project.source.source_identity = std::string(64U, 'a');
    project.genome = editor.genome();
    project.locks = editor.locks();
    project.session.proxy_enabled = false;
    project.session.selected_instance_id = project.genome.operators[0].instance_id.to_string();
    project.ui.mode = "custom";
    project.ui.zoom_milli = 2250;
    project.ui.pan_x_milli = -12500;
    project.ui.pan_y_milli = 3000;
    project.ui.show_before = true;

    const std::string canonical = app::serialize_project_canonical(project);
    const auto parsed = app::parse_project(canonical, editor.registry().schema_registry());
    expect(parsed.ok(), "canonical project parses");
    if (parsed.ok()) {
        expect_equal(app::serialize_project_canonical(*parsed.project), canonical, "project parse/serialize is exactly canonical");
        expect_equal(parsed.project->locks, project.locks, "lock state survives project reload");
        expect_equal(
            std::get<std::string>(parsed.project->genome.operators[0].parameters.at("palette")),
            std::get<std::string>(project.genome.operators[0].parameters.at("palette")),
            "embedded FM-007 palette asset survives project persistence");
    }

    std::string future = canonical;
    const std::string token = "\"project_version\":1";
    const std::size_t position = future.find(token);
    expect(position != std::string::npos, "project version token located");
    if (position != std::string::npos) {
        future.replace(position, token.size(), "\"project_version\":2");
        expect(!app::parse_project(future, editor.registry().schema_registry()).ok(), "future project version is rejected clearly");
    }

    expect_equal(app::assess_source_reference(project.source.source_identity, std::nullopt), app::SourceReferenceStatus::missing, "missing source classified separately");
    expect_equal(app::assess_source_reference(project.source.source_identity, std::optional<std::string>{project.source.source_identity}), app::SourceReferenceStatus::identical, "moved identical source is relink-compatible");
    expect_equal(app::assess_source_reference(project.source.source_identity, std::optional<std::string>{std::string(64U, 'b')}), app::SourceReferenceStatus::changed, "changed same-path source is rejected as provenance mismatch");
}

void test_session_project_reproduction() {
    using namespace faultmine;
    core::ImageBuffer source = make_source(7U, 5U);
    const std::string identity = core::source_identity_hex(source);

    app::SessionModel first;
    std::string error;
    expect(first.set_source(source, identity, L"C:\\work\\original.png", &error), "first session accepts source");
    expect(first.add_operator(core::kFaultAddressOffset, first.genome().operators.size()).ok(), "structural operator added through session");
    const std::size_t added = first.genome().operators.size() - 1U;
    expect(first.set_parameter_from_text(added, "offset_pixels", "3").ok(), "session generic parameter edit succeeds");
    expect(first.toggle_parameter_lock(added, "offset_pixels").ok(), "session lock set");
    first.set_selected_operator(added);
    first.set_proxy_enabled(false);
    const auto before = first.render_full(&error);
    expect(before.has_value(), "first session full render succeeds");

    const auto document = first.make_project_document(&error);
    expect(document.has_value(), "session emits project document");
    if (!document.has_value()) {
        return;
    }
    const std::string text = app::serialize_project_canonical(*document);
    const auto parsed = app::parse_project(text, first.registry().schema_registry());
    expect(parsed.ok(), "saved session project parses");
    if (!parsed.ok()) {
        return;
    }

    app::SessionModel restored;
    expect(restored.load_project_state(*parsed.project, source, L"D:\\moved\\original.png", &error), "moved identical source relinks without changing provenance");
    const auto after = restored.render_full(&error);
    expect(after.has_value(), "restored project full render succeeds");
    if (before.has_value() && after.has_value()) {
        expect_equal(core::source_identity_hex(*before), core::source_identity_hex(*after), "project reload reproduces canonical output exactly");
    }
    expect(restored.parameter_locked(added, "offset_pixels"), "project reload restores gene lock");
    expect_equal(restored.selected_operator(), std::optional<std::size_t>{added}, "project reload restores selected operator by stable id");

    core::ImageBuffer changed = source;
    changed.bytes[0] ^= 1U;
    expect(!restored.load_project_state(*parsed.project, changed, L"D:\\moved\\changed.png", &error), "changed-content relink is rejected");
}

}  // namespace

int main() {
    try {
        test_descriptor_driven_editing();
        test_duplicate_reorder_history_and_ids();
        test_locks_are_noncanonical();
        test_project_roundtrip_and_source_classification();
        test_session_project_reproduction();
    } catch (const std::exception& exception) {
        ++g_failures;
        std::cerr << "UNCAUGHT TEST EXCEPTION: " << exception.what() << '\n';
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " FM-008 editor/project assertion(s) failed.\n";
        return 1;
    }
    std::cout << "FAULTMINE FM-008 editor/project contracts passed.\n";
    return 0;
}
