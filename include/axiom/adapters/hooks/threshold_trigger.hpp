#pragma once

#include "axiom/core/boolean_hook.hpp"

#include <cstddef>
#include <cstdint>

namespace axiom::adapters {

    class ThresholdTrigger final : public core::IBooleanHook {
    public:
        explicit ThresholdTrigger(std::uint8_t min_intensity, std::size_t min_hit_count = 1) noexcept
            : min_intensity_(min_intensity), min_hit_count_(min_hit_count) {}

        [[nodiscard]] std::string_view name() const noexcept override {
            return "ThresholdTrigger";
        }

        [[nodiscard]] bool evaluate(const core::TensorView& tensor,
                                    core::SlotMetadata& meta) noexcept override {
            if (tensor.empty() || tensor.dtype != core::DType::UInt8) {
                return false;
            }

            const auto* bytes = reinterpret_cast<const std::uint8_t*>(tensor.data);
            const std::size_t count = tensor.element_count();
            std::size_t hits = 0;

            for (std::size_t i = 0; i < count; ++i) {
                if (bytes[i] >= min_intensity_) {
                    if (++hits >= min_hit_count_) {
                        meta.provenance_flags |= core::kProvenanceAnomaly;
                        return true;
                    }
                }
            }
            return false;
        }

    private:
        std::uint8_t min_intensity_;
        std::size_t min_hit_count_;
    };

} // namespace axiom::adapters