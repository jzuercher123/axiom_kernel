#include "axiom/adapters/hooks/bayesian_kl_gate.hpp"
#include "axiom/adapters/hooks/threshold_trigger.hpp"
#include "axiom/adapters/ipc/null_socket.hpp"
#include "axiom/adapters/sensors/synthetic_sensor.hpp"
#include "axiom/core/pipeline.hpp"

#include <gtest/gtest.h>

using namespace axiom::core;
using namespace axiom::adapters;

TEST(BayesianKlGateTest, DropsBackgroundNoiseAndForwardsAnomalies) {
    SyntheticSensorConfig cfg{};
    cfg.width = 64;
    cfg.height = 64;
    cfg.bg_mean = 20.0;
    cfg.bg_stddev = 4.0;
    cfg.anomaly_interval = 5; // Frames 4, 9, 14, 19 (1-indexed: 5, 10, 15, 20)
    cfg.anomaly_intensity = 240;
    cfg.anomaly_patch_size = 4;

    SyntheticSensor sensor(cfg);
    RingBuffer rb(16, cfg.width * cfg.height, OverflowPolicy::DropNewest);
    Pipeline pipeline(sensor, rb);

    // Background noise yields very low D_KL (~0.005); a 4x4 bright patch spikes D_KL well above 0.05
    BayesianKlGate kl_gate(/*threshold=*/0.05, cfg.bg_mean, cfg.bg_stddev);
    NullSocket sink;

    pipeline.add_hook(&kl_gate);
    pipeline.add_sink(&sink);

    constexpr int kTotalFrames = 20;
    for (int i = 0; i < kTotalFrames; ++i) {
        ASSERT_TRUE(pipeline.produce_once());
        ASSERT_TRUE(pipeline.consume_once());
    }

    EXPECT_EQ(pipeline.metrics().frames_ingested.load(), 20u);
    EXPECT_EQ(pipeline.metrics().frames_gated_out.load(), 16u);
    EXPECT_EQ(pipeline.metrics().frames_published.load(), 4u);
    EXPECT_EQ(sink.published_count(), 4u);
}

TEST(ThresholdTriggerTest, FastAmplitudeGateChainedWithKlGate) {
    SyntheticSensorConfig cfg{};
    cfg.width = 32;
    cfg.height = 32;
    cfg.anomaly_interval = 4; // 3 anomalies in 12 frames
    cfg.anomaly_intensity = 250;
    cfg.anomaly_patch_size = 3; // 9 bright pixels

    SyntheticSensor sensor(cfg);
    RingBuffer rb(16, cfg.width * cfg.height, OverflowPolicy::DropNewest);
    Pipeline pipeline(sensor, rb);

    ThresholdTrigger fast_gate(/*min_intensity=*/200, /*min_hit_count=*/5);
    BayesianKlGate kl_gate(/*threshold=*/0.05, cfg.bg_mean, cfg.bg_stddev);
    NullSocket sink;

    pipeline.add_hook(&fast_gate);
    pipeline.add_hook(&kl_gate);
    pipeline.add_sink(&sink);

    for (int i = 0; i < 12; ++i) {
        ASSERT_TRUE(pipeline.produce_once());
        ASSERT_TRUE(pipeline.consume_once());
    }

    EXPECT_EQ(pipeline.metrics().frames_ingested.load(), 12u);
    EXPECT_EQ(pipeline.metrics().frames_gated_out.load(), 9u);
    EXPECT_EQ(pipeline.metrics().frames_published.load(), 3u);
    EXPECT_EQ(sink.published_count(), 3u);
}