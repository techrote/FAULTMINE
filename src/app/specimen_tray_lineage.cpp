#include "faultmine/specimen_tray.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace faultmine::app {

bool SpecimenTrayModel::toggle_crossover_parent(const std::size_t index) noexcept {
    if (index >= items_.size()) return false;
    const std::uint32_t old_rank = items_[index].crossover_rank;
    if (old_rank != 0U) {
        items_[index].crossover_rank = 0U;
        for (SpecimenTrayItem& item : items_) {
            if (item.crossover_rank > old_rank) --item.crossover_rank;
        }
        return true;
    }

    std::uint32_t maximum = 0U;
    for (const SpecimenTrayItem& item : items_) maximum = std::max(maximum, item.crossover_rank);
    items_[index].crossover_rank = maximum + 1U;
    return true;
}

std::vector<std::size_t> SpecimenTrayModel::crossover_parent_indices() const {
    std::vector<std::pair<std::uint32_t, std::size_t>> ranked;
    for (std::size_t index = 0U; index < items_.size(); ++index) {
        if (items_[index].crossover_rank != 0U) {
            ranked.emplace_back(items_[index].crossover_rank, index);
        }
    }
    std::sort(ranked.begin(), ranked.end());
    std::vector<std::size_t> output;
    output.reserve(ranked.size());
    for (const auto& entry : ranked) output.push_back(entry.second);
    return output;
}

void SpecimenTrayModel::clear_crossover_selection() noexcept {
    for (SpecimenTrayItem& item : items_) item.crossover_rank = 0U;
}

}  // namespace faultmine::app
