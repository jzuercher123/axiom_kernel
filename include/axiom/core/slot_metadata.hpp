#pragma once

#include <array>
#include <cstdint>

namespace axiom::core {

    enum ProvenanceFlags : std::uint32_t {
        kProvenanceNone        = 0u,
        kProvenanceHasDigest   = 1u << 0u,
        kProvenanceSoftwareSig = 1u << 1u,
        kProvenanceHardwareSig = 1u << 2u,
        kProvenanceAnomaly     = 1u << 3u,
    };

    struct SlotMetadata {
        std::uint64_t sequence_id{0};
        std::uint64_t monotonic_ns{0};
        std::uint64_t utc_epoch_ns{0};
        std::uint32_t sensor_id{0};
        std::uint32_t provenance_flags{kProvenanceNone};
        double anomaly_score{0.0};
        std::array<std::uint8_t, 32> sha256_digest{};
        std::array<std::uint8_t, 64> signature{};

        constexpr void reset() noexcept {
            sequence_id = 0;
            monotonic_ns = 0;
            utc_epoch_ns = 0;
            sensor_id = 0;
            provenance_flags = kProvenanceNone;
            anomaly_score = 0.0;
            sha256_digest.fill(0);
            signature.fill(0);
        }
    };

} // namespace axiom::core