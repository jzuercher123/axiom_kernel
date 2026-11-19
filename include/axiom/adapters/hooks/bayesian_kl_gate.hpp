#pragma once

#include "axiom/core/boolean_hook.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace axiom::adapters {

class BayesianKlGate final : public core::IBooleanHook {
public:
    explicit BayesianKlGate(double threshold,
                            double bg_mean = 20.0,
                            double bg_stddev = 4.0,
                            double epsilon = 1e-9) noexcept
        : threshold_(threshold), epsilon_(epsilon) {
        calibrate_gaussian_background(bg_mean, bg_stddev);
    }

    [[nodiscard]] std::string_view name() const noexcept override {
        return "BayesianKlGate";
    }

    // Calibrate reference distribution Q from a known Gaussian background
    void calibrate_gaussian_background(double mean, double stddev) noexcept {
        double sum = 0.0;
        const double inv_2var = 1.0 / (2.0 * stddev * stddev);
        for (std::size_t i = 0; i < 256; ++i) {
            const double diff = static_cast<double>(i) - mean;
            q_dist_[i] = std::exp(-(diff * diff) * inv_2var) + epsilon_;
            sum += q_dist_[i];
        }
        for (double& q : q_dist_) {
            q /= sum;
        }
    }

    [[nodiscard]] bool evaluate(const core::TensorView& tensor,
                                core::SlotMetadata& meta) noexcept override {
        if (tensor.empty() || tensor.dtype != core::DType::UInt8) {
            meta.anomaly_score = 0.0;
            return false;
        }

        const std::size_t count = tensor.element_count();
        if (count == 0) {
            meta.anomaly_score = 0.0;
            return false;
        }

        std::array<std::uint32_t, 256> hist{};
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(tensor.data);
        for (std::size_t i = 0; i < count; ++i) {
            ++hist[bytes[i]];
        }

        const double inv_count = 1.0 / static_cast<double>(count);
        double d_kl = 0.0;

        for (std::size_t i = 0; i < 256; ++i) {
            if (hist[i] == 0) {
                continue;
            }
            const double p_i = static_cast<double>(hist[i]) * inv_count;
            d_kl += p_i * std::log((p_i + epsilon_) / (q_dist_[i] + epsilon_));
        }

        meta.anomaly_score = d_kl;
        if (d_kl >= threshold_) {
            meta.provenance_flags |= core::kProvenanceAnomaly;
            return true;
        }
        return false;
    }

private:
    double threshold_;
    double epsilon_;
    std::array<double, 256> q_dist_{};
};

} // namespace axiom::adapters