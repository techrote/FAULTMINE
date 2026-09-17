#pragma once

#include "faultmine/editor.hpp"
#include "faultmine/mutation.hpp"
#include "faultmine/proxy.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace faultmine::app {

inline constexpr std::size_t kDefaultSpecimenPopulation = 8U;
inline constexpr std::size_t kMaximumSpecimenPopulation = 32U;

struct SpecimenTrayConfig {
    core::RootSeed mutation_seed{0x464d303039534545ULL};
    core::MutationRadius radius{core::MutationRadius::medium};
    std::size_t population_size{kDefaultSpecimenPopulation};
    core::ProxySpec thumbnail_proxy{320U, 240U, core::kProxyMethodVersion};
};

struct SpecimenTrayItem {
    core::DescendantProvenance provenance;
    core::Genome genome;
    std::optional<core::ImageBuffer> preview;
    std::string render_error;
    bool pinned{};
};

class SpecimenTrayModel {
public:
    [[nodiscard]] bool generate(
        const core::Genome& parent,
        const LockState& locks,
        const core::FaultRegistry& registry,
        SpecimenTrayConfig config,
        std::string* error = nullptr);

    // Render at most one pending specimen. generation_token lets asynchronous
    // callers discard obsolete posted work safely after a reroll.
    [[nodiscard]] bool render_next(
        const core::ImageBuffer& full_source,
        const std::string& source_identity,
        const core::FaultRegistry& registry,
        std::uint64_t generation_token,
        std::string* error = nullptr);

    void reset() noexcept;

    [[nodiscard]] const SpecimenTrayConfig& config() const noexcept;
    [[nodiscard]] const std::vector<SpecimenTrayItem>& items() const noexcept;
    [[nodiscard]] std::optional<std::size_t> selected_index() const noexcept;
    [[nodiscard]] const SpecimenTrayItem* selected_item() const noexcept;
    [[nodiscard]] bool select(std::size_t index) noexcept;
    [[nodiscard]] bool toggle_pin(std::size_t index) noexcept;

    [[nodiscard]] bool busy() const noexcept;
    [[nodiscard]] const std::string& error_text() const noexcept;
    [[nodiscard]] std::uint64_t generation_token() const noexcept;

    [[nodiscard]] static core::RootSeed reroll_seed(core::RootSeed current) noexcept;

private:
    [[nodiscard]] static core::MutationLocks make_mutation_locks(const LockState& locks);
    void refresh_busy() noexcept;

    SpecimenTrayConfig config_{};
    std::vector<SpecimenTrayItem> items_;
    std::optional<std::size_t> selected_index_;
    std::optional<core::ProxyImage> source_proxy_;
    std::string source_proxy_identity_;
    std::string error_text_;
    std::uint64_t generation_token_{};
    bool busy_{};
};

}  // namespace faultmine::app
