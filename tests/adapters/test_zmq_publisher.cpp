#include "axiom/adapters/hooks/bayesian_kl_gate.hpp"
#include "axiom/adapters/ipc/zmq_publisher_socket.hpp"
#include "axiom/adapters/sensors/synthetic_sensor.hpp"
#include "axiom/core/pipeline.hpp"

#include <cstring>
#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace axiom::core;
using namespace axiom::adapters;

TEST(ZmqPublisherSocketTest, PublishesThreePartAnomalyMessages) {
    zmq::context_t ctx(1);
    const std::string endpoint = "inproc://axiom_test_pub";
    const std::string topic = "sda.node_01";

    ZmqPublisherSocket pub(ctx, endpoint, topic);

    zmq::socket_t sub(ctx, zmq::socket_type::sub);
    sub.set(zmq::sockopt::subscribe, topic);
    sub.set(zmq::sockopt::rcvtimeo, 500);
    sub.connect(endpoint);

    SyntheticSensorConfig cfg{};
    cfg.width = 32;
    cfg.height = 32;
    cfg.anomaly_interval = 5; // 2 anomalies in 10 frames

    SyntheticSensor sensor(cfg);
    RingBuffer rb(8, cfg.width * cfg.height, OverflowPolicy::DropNewest);
    Pipeline pipeline(sensor, rb);

    BayesianKlGate kl_gate(/*threshold=*/0.05, cfg.bg_mean, cfg.bg_stddev);
    pipeline.add_hook(&kl_gate);
    pipeline.add_sink(&pub);

    for (int i = 0; i < 10; ++i) {
        ASSERT_TRUE(pipeline.produce_once());
        ASSERT_TRUE(pipeline.consume_once());
    }

    ASSERT_EQ(pipeline.metrics().frames_published.load(), 2u);

    for (int msg_idx = 0; msg_idx < 2; ++msg_idx) {
        zmq::message_t part1_topic;
        zmq::message_t part2_header;
        zmq::message_t part3_payload;

        ASSERT_TRUE(sub.recv(part1_topic, zmq::recv_flags::none).has_value());
        ASSERT_TRUE(sub.recv(part2_header, zmq::recv_flags::none).has_value());
        ASSERT_TRUE(sub.recv(part3_payload, zmq::recv_flags::none).has_value());

        EXPECT_EQ(part1_topic.to_string(), topic);
        ASSERT_EQ(part2_header.size(), sizeof(WireHeader));

        WireHeader hdr{};
        std::memcpy(&hdr, part2_header.data(), sizeof(WireHeader));

        EXPECT_EQ(hdr.rank, 2u);
        EXPECT_EQ(hdr.dtype, DType::UInt8);
        EXPECT_EQ(hdr.extents[0], 32u);
        EXPECT_EQ(hdr.extents[1], 32u);
        EXPECT_GT(hdr.metadata.anomaly_score, 0.05);
        EXPECT_EQ(part3_payload.size(), 32u * 32u);
    }
}