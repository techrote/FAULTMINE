#include "faultmine/image.hpp"
#include "faultmine/proxy.hpp"
#include "faultmine/session.hpp"
#include "faultmine/starter_operators.hpp"

#include <cstdint>
#include <filesystem>
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

faultmine::core::ImageBuffer make_pattern(const std::uint32_t width, const std::uint32_t height) {
    auto created = faultmine::core::make_rgba8_image(width, height);
    if (!created.ok()) {
        throw std::runtime_error(created.error->message);
    }
    faultmine::core::ImageBuffer image = std::move(*created.image);
    for (std::size_t index = 0U; index < image.bytes.size(); ++index) {
        image.bytes[index] = static_cast<std::uint8_t>((index * 29U + 17U) & 0xffU);
    }
    return image;
}

void test_proxy_contract() {
    using namespace faultmine::core;
    const ImageBuffer source = make_pattern(8U, 4U);
    const std::string identity = source_identity_hex(source);
    const ProxySpec spec{4U, 4U, kProxyMethodVersion};

    const ProxyResult first = make_nearest_proxy(source, identity, spec);
    const ProxyResult second = make_nearest_proxy(source, identity, spec);
    expect(first.ok() && second.ok(), "deterministic proxy generation succeeds");
    if (!first.ok() || !second.ok()) {
        return;
    }
    expect(first.proxy->is_proxy, "oversized source is marked proxy");
    expect_equal(first.proxy->image.width, std::uint32_t{4U}, "proxy width is bounded");
    expect_equal(first.proxy->image.height, std::uint32_t{2U}, "proxy height preserves aspect ratio");
    expect_equal(first.proxy->cache_key, second.proxy->cache_key, "proxy cache key is stable");
    expect_equal(first.proxy->image.bytes, second.proxy->image.bytes, "proxy pixels are deterministic");

    const std::size_t target_pixel_1 = 4U;
    const std::size_t source_pixel_2 = 2U * 4U;
    for (std::size_t channel = 0U; channel < 4U; ++channel) {
        expect_equal(
            first.proxy->image.bytes[target_pixel_1 + channel],
            source.bytes[source_pixel_2 + channel],
            "nearest proxy maps deterministic source coordinate");
    }

    ProxySpec changed = spec;
    changed.max_width = 5U;
    expect(proxy_cache_key(identity, changed) != first.proxy->cache_key, "proxy spec participates in cache identity");
}

void test_view_events_do_not_render() {
    using namespace faultmine;
    app::SessionModel session;
    core::ImageBuffer source = make_pattern(5U, 3U);
    const std::string identity = core::source_identity_hex(source);
    std::string error;
    expect(session.set_source(source, identity, L"view.png", &error), "session accepts canonical source");
    expect(session.ensure_preview(&error), "initial preview render succeeds");
    const std::uint64_t generation = session.render_generation();
    const std::string genome_identity = session.genome_identity();
    const std::string result_identity = core::source_identity_hex(*session.preview_result());

    session.set_fit_view();
    session.zoom_by(1.2);
    session.pan_by(17.0, -9.0);
    session.toggle_before();
    session.toggle_before();

    expect_equal(session.render_generation(), generation, "view events do not execute canonical pipeline");
    expect(!session.preview_dirty(), "view events do not dirty semantic preview");
    expect_equal(session.genome_identity(), genome_identity, "view events do not mutate genome");
    expect_equal(core::source_identity_hex(*session.preview_result()), result_identity, "view events do not mutate cached canonical result");
}

void test_session_matches_direct_pipeline() {
    using namespace faultmine;
    app::SessionModel session;
    session.set_proxy_enabled(false);
    core::ImageBuffer source = make_pattern(6U, 4U);
    const std::string identity = core::source_identity_hex(source);
    std::string error;
    expect(session.set_source(source, identity, L"direct.png", &error), "session accepts direct source");
    expect(session.ensure_preview(&error), "session preview succeeds");

    const core::FaultRegistry registry = core::make_starter_fault_registry();
    const core::RenderResult direct = core::render_pipeline(source, session.genome(), registry);
    expect(direct.ok(), "direct starter pipeline succeeds");
    if (direct.ok() && session.preview_result() != nullptr) {
        expect_equal(
            core::source_identity_hex(*session.preview_result()),
            core::source_identity_hex(*direct.image),
            "session preview calls same canonical CPU pipeline");
    }
}

void test_proxy_preview_and_full_export_boundary() {
    using namespace faultmine;
    app::SessionModel session;
    session.set_proxy_spec(core::ProxySpec{4U, 4U, core::kProxyMethodVersion});
    core::ImageBuffer source = make_pattern(8U, 4U);
    const std::string identity = core::source_identity_hex(source);
    std::string error;
    expect(session.set_source(source, identity, L"large.png", &error), "large source enters session");
    expect(session.ensure_preview(&error), "proxy preview render succeeds");
    const app::PreviewState proxy_state = session.preview_state();
    expect(proxy_state.is_proxy, "large interactive preview is explicitly proxy");
    expect_equal(proxy_state.width, std::uint32_t{4U}, "proxy preview width reported");
    expect_equal(proxy_state.height, std::uint32_t{2U}, "proxy preview height reported");

    const auto full = session.render_full(&error);
    expect(full.has_value(), "full canonical render succeeds while proxy preview is active");
    if (full.has_value()) {
        expect_equal(full->width, std::uint32_t{8U}, "export/full render keeps original width");
        expect_equal(full->height, std::uint32_t{4U}, "export/full render keeps original height");
        const core::RenderResult direct = core::render_pipeline(
            source,
            session.genome(),
            core::make_starter_fault_registry());
        expect(direct.ok(), "direct full render succeeds");
        if (direct.ok()) {
            expect_equal(
                core::source_identity_hex(*full),
                core::source_identity_hex(*direct.image),
                "full export render ignores proxy preprocessing");
        }
    }

    session.set_proxy_enabled(false);
    expect(session.preview_dirty(), "disabling proxy dirties preview only");
    expect(session.ensure_preview(&error), "full interactive preview succeeds");
    const app::PreviewState full_state = session.preview_state();
    expect(!full_state.is_proxy, "full preview is explicitly non-proxy");
    expect_equal(full_state.width, std::uint32_t{8U}, "full preview reports source width");
}

void test_semantic_controls_and_source_switching() {
    using namespace faultmine;
    app::SessionModel session;
    session.set_proxy_enabled(false);
    core::ImageBuffer first = make_pattern(5U, 3U);
    std::string first_identity = core::source_identity_hex(first);
    std::string error;
    expect(session.set_source(first, first_identity, L"first.png", &error), "first source loads");
    expect(session.ensure_preview(&error), "first source renders");
    const std::uint64_t initial_generation = session.render_generation();
    const std::string initial_genome = session.genome_identity();

    session.adjust_row_offset(1);
    expect(session.preview_dirty(), "fault parameter edit dirties semantic preview");
    expect(session.ensure_preview(&error), "edited fault preview renders");
    expect_equal(session.render_generation(), initial_generation + 1U, "semantic edit advances render generation once");
    expect(session.genome_identity() != initial_genome, "fault parameter edit changes genome identity");

    const std::string before_seed = session.genome().root_seed.to_string();
    session.reroll_seed();
    expect(session.genome().root_seed.to_string() != before_seed, "seed reroll changes explicit root seed");
    expect(session.ensure_preview(&error), "rerolled seed preview renders");

    core::ImageBuffer second = make_pattern(3U, 7U);
    second.bytes[0] ^= 0x5aU;
    const std::string second_identity = core::source_identity_hex(second);
    expect(session.set_source(second, second_identity, L"second.png", &error), "second source replaces first source");
    expect(session.preview_result() == nullptr, "source switch clears stale cached result");
    expect(session.ensure_preview(&error), "second source renders");
    expect_equal(session.preview_result()->width, std::uint32_t{3U}, "new preview uses second source width");
    expect_equal(session.preview_result()->height, std::uint32_t{7U}, "new preview uses second source height");
    expect_equal(session.source_identity(), second_identity, "source switch replaces provenance identity");
}

}  // namespace

int main() {
    try {
        test_proxy_contract();
        test_view_events_do_not_render();
        test_session_matches_direct_pipeline();
        test_proxy_preview_and_full_export_boundary();
        test_semantic_controls_and_source_switching();
    } catch (const std::exception& exception) {
        ++g_failures;
        std::cerr << "UNCAUGHT TEST EXCEPTION: " << exception.what() << '\n';
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " session/proxy assertion(s) failed.\n";
        return 1;
    }
    std::cout << "FAULTMINE session/proxy contracts passed.\n";
    return 0;
}
