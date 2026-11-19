#pragma once

#include "axiom/core/sensor_source.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>

namespace axiom::adapters {

struct SyntheticSensorConfig {
    std::uint32_t sensor_id{1};
    std::size_t width{64};
    std::size_t height{64};
    double bg_mean{20.0};
    double bg_stddev{4.0};
    std::uint64_t anomaly_interval{5}; // Inject anomaly every Nth frame (0 = never)
    std::uint8_t anomaly_intensity{240};
    std::size_t anomaly_patch_size{4}; // 4x4 bright point source
    std::uint32_t seed{1337};
};

class SyntheticSensor final : public core::ISensorSource {
public:
    explicit SyntheticSensor(SyntheticSensorConfig cfg = {})
        : cfg_(cfg), rng_(cfg.seed), dist_(cfg.bg_mean, cfg.bg_stddev) {}

    [[nodiscard]] std::string_view name() const noexcept override {
        return "SyntheticSensor";
    }

    [[nodiscard]] std::uint32_t sensor_id() const noexcept override {
        return cfg_.sensor_id;
    }

    [[nodiscard]] bool capture(core::RingBuffer::WriteSlot& slot,
                               core::TensorView& out_view) noexcept override {
        const std::size_t total_pixels = cfg_.width * cfg_.height;
        if (slot.capacity_bytes < total_pixels) {
            return false;
        }

        auto* pixels = reinterpret_cast<std::uint8_t*>(slot.buffer);
        for (std::size_t i = 0; i < total_pixels; ++i) {
            const double sample = dist_(rng_);
            const double clamped = std::clamp(sample, 0.0, 255.0);
            pixels[i] = static_cast<std::uint8_t>(clamped);
        }

        // Inject synthetic point-source anomaly on scheduled frames
        const std::uint64_t frame_idx = frame_count_++;
        if (cfg_.anomaly_interval > 0 && ((frame_idx + 1u) % cfg_.anomaly_interval == 0u)) {
            const std::size_t start_y = cfg_.height / 2;
            const std::size_t start_x = cfg_.width / 2;
            for (std::size_t dy = 0; dy < cfg_.anomaly_patch_size && (start_y + dy) < cfg_.height; ++dy) {
                for (std::size_t dx = 0; dx < cfg_.anomaly_patch_size && (start_x + dx) < cfg_.width; ++dx) {
                    pixels[(start_y + dy) * cfg_.width + (start_x + dx)] = cfg_.anomaly_intensity;
                }
            }
        }

        out_view = core::TensorView::make_contiguous(
            slot.buffer,
            core::DType::UInt8,
            2,
            {cfg_.height, cfg_.width, 0, 0}
        );
        return true;
    }

private:
    SyntheticSensorConfig cfg_;
    std::mt19937 rng_;
    std::normal_distribution<double> dist_;
    std::uint64_t frame_count_{0};
};

} // namespace axiom::adapters