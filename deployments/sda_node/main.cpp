#include "axiom/adapters/hooks/bayesian_kl_gate.hpp"
#include "axiom/adapters/hooks/crypto_provenance_hook.hpp"
#include "axiom/adapters/hooks/openssl_backend.hpp"
#include "axiom/adapters/ipc/zmq_publisher_socket.hpp"
#include "axiom/adapters/sensors/synthetic_sensor.hpp"
#include "axiom/core/pipeline.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

using namespace axiom::core;
using namespace axiom::adapters;

int main() {
    const std::string endpoint = "tcp://127.0.0.1:5555";
    const std::string topic = "sda.node_01";
    constexpr int total_frames = 50;

    std::cout << "[sda_node] Booting Axiom Kernel reference node...\n";
    std::cout << "[sda_node] Publishing verified anomalies to " << endpoint
              << " (topic: " << topic << ")\n";

    // 1. Sensor & Lock-Free RingBuffer (Layer 0 + Layer 1)
    SyntheticSensorConfig cfg{};
    cfg.width = 64;
    cfg.height = 64;
    cfg.anomaly_interval = 10; // 1 anomaly every 10 frames

    SyntheticSensor sensor(cfg);
    RingBuffer ring_buffer(16, cfg.width * cfg.height, OverflowPolicy::DropNewest);
    Pipeline pipeline(sensor, ring_buffer);

    // 2. Hook Chain: Bayesian KL Anomaly Gate -> SHA-256 + ECDSA P-256 Provenance
    BayesianKlGate kl_gate(/*threshold=*/0.05, cfg.bg_mean, cfg.bg_stddev);
    OpenSslBackend crypto_backend;
    CryptoProvenanceHook crypto_hook(crypto_backend);

    pipeline.add_hook(&kl_gate);
    pipeline.add_hook(&crypto_hook);

    // 3. Egress: 3-Part ZeroMQ Publisher over TCP
    zmq::context_t zmq_ctx(1);
    ZmqPublisherSocket zmq_pub(zmq_ctx, endpoint, topic);
    pipeline.add_sink(&zmq_pub);

    // 4. Deterministic Run Loop
    for (int i = 0; i < total_frames; ++i) {
        if (!pipeline.produce_once()) {
            std::cerr << "[sda_node] Warning: RingBuffer overflow on frame " << i << "\n";
        }
        pipeline.consume_once();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    const auto published = pipeline.metrics().frames_published.load();
    std::cout << "[sda_node] Run complete. Frames processed: " << total_frames
              << " | Anomalies signed & published: " << published << "\n";

    return 0;
}