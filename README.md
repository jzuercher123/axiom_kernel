# Axiom Kernel (`v0.1.0`)

A minimal, zero-dependency C++20 data-plane kernel for scientific instrumentation and edge-to-AI metrology.

## Architecture

Axiom Kernel separates hot-path numerical processing from domain-specific schemas using an Onion Architecture:

* **Layer 0 (`axiom_core`):** Header-only, zero external dependencies. Lock-free SPSC `RingBuffer`, non-owning 1D–4D `TensorView`, `SlotMetadata`, `IBooleanHook`, `ISensorSource`, and `IIpcSocket`.
* **Layer 1 (`axiom_adapters`):** Optional, feature-flagged adapters:
    * `BayesianKlGate` & `ThresholdTrigger` (information-theoretic edge gating)
    * `CryptoProvenanceHook` + `OpenSslBackend` (SHA-256 + ECDSA P-256 chain of custody)
    * `ZmqPublisherSocket` (3-part ZeroMQ egress: topic, binary `WireHeader`, raw tensor bytes)
    * `SyntheticSensor` (deterministic Gaussian + point-source anomaly generator)
* **Layer 2 (`deployments/sda_node`):** Reference Space Domain Awareness (SDA) executable paired with a declarative `config/fleet.yaml` manifest and a Python subscriber (`tools/agent_subscriber.py`).

## Build & Test

```bash
cmake -B build \
  -DAXIOM_BUILD_TESTS=ON \
  -DAXIOM_ENABLE_OPENSSL=ON \
  -DAXIOM_ENABLE_ZMQ=ON \
  -DAXIOM_BUILD_SDA_NODE=ON
cmake --build build
ctest --test-dir build --output-on-failure