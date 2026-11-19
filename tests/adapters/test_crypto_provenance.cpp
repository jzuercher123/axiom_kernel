#include "axiom/adapters/hooks/bayesian_kl_gate.hpp"
#include "axiom/adapters/hooks/crypto_provenance_hook.hpp"
#include "axiom/adapters/hooks/openssl_backend.hpp"
#include "axiom/adapters/sensors/synthetic_sensor.hpp"
#include "axiom/core/pipeline.hpp"

#include <array>
#include <gtest/gtest.h>
#include <vector>

using namespace axiom::core;
using namespace axiom::adapters;

namespace {

class VerifyingSink final : public IIpcSocket {
public:
    explicit VerifyingSink(const OpenSslBackend& backend) : backend_(backend) {}

    [[nodiscard]] bool publish(const TensorView& tensor,
                               const SlotMetadata& meta) noexcept override {
        EXPECT_NE(meta.provenance_flags & kProvenanceHasDigest, 0u);
        EXPECT_NE(meta.provenance_flags & kProvenanceSoftwareSig, 0u);
        EXPECT_NE(meta.provenance_flags & kProvenanceAnomaly, 0u);

        // 1. Verify original signature passes
        EXPECT_TRUE(backend_.verify_digest(meta.sha256_digest, meta.signature));

        // 2. Flip 1 bit in the payload and verify tamper detection catches it
        std::vector<std::byte> tampered(tensor.data, tensor.data + tensor.byte_size());
        tampered[0] ^= std::byte{0x01};

        std::array<std::uint8_t, 32> tampered_digest{};
        auto& mutable_backend = const_cast<OpenSslBackend&>(backend_);
        EXPECT_TRUE(mutable_backend.compute_digest(
            tampered.data(), tampered.size(),
            meta.sequence_id, meta.monotonic_ns, tampered_digest));

        EXPECT_NE(tampered_digest, meta.sha256_digest);
        EXPECT_FALSE(backend_.verify_digest(tampered_digest, meta.signature));

        ++verified_frames;
        return true;
    }

    std::uint64_t verified_frames{0};

private:
    const OpenSslBackend& backend_;
};

} // namespace

TEST(CryptoProvenanceTest, SignsAnomaliesAndDetectsSingleBitTamper) {
    SyntheticSensorConfig cfg{};
    cfg.width = 64;
    cfg.height = 64;
    cfg.anomaly_interval = 5; // 4 anomalies in 20 frames

    SyntheticSensor sensor(cfg);
    RingBuffer rb(16, cfg.width * cfg.height, OverflowPolicy::DropNewest);
    Pipeline pipeline(sensor, rb);

    BayesianKlGate kl_gate(/*threshold=*/0.05, cfg.bg_mean, cfg.bg_stddev);
    OpenSslBackend crypto_backend;
    CryptoProvenanceHook crypto_hook(crypto_backend);
    VerifyingSink sink(crypto_backend);

    // Gate first, then sign triggered anomalies
    pipeline.add_hook(&kl_gate);
    pipeline.add_hook(&crypto_hook);
    pipeline.add_sink(&sink);

    for (int i = 0; i < 20; ++i) {
        ASSERT_TRUE(pipeline.produce_once());
        ASSERT_TRUE(pipeline.consume_once());
    }

    EXPECT_EQ(pipeline.metrics().frames_published.load(), 4u);
    EXPECT_EQ(sink.verified_frames, 4u);
}