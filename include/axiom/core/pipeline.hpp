#pragma once

#include "axiom/core/boolean_hook.hpp"
#include "axiom/core/ipc_socket.hpp"
#include "axiom/core/ring_buffer.hpp"
#include "axiom/core/sensor_source.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

namespace axiom::core {

struct PipelineMetrics {
    std::atomic<std::uint64_t> frames_ingested{0};
    std::atomic<std::uint64_t> frames_dropped_overrun{0};
    std::atomic<std::uint64_t> frames_gated_out{0};
    std::atomic<std::uint64_t> frames_published{0};

    void reset() noexcept {
        frames_ingested.store(0, std::memory_order_relaxed);
        frames_dropped_overrun.store(0, std::memory_order_relaxed);
        frames_gated_out.store(0, std::memory_order_relaxed);
        frames_published.store(0, std::memory_order_relaxed);
    }
};

class Pipeline {
public:
    Pipeline(ISensorSource& source, RingBuffer& buffer) noexcept
        : source_(source), buffer_(buffer) {}

    ~Pipeline() {
        stop();
    }

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    // Register hooks and sinks during startup (never call while running)
    void add_hook(IBooleanHook* hook) {
        if (hook != nullptr) {
            hooks_.push_back(hook);
        }
    }

    void add_sink(IIpcSocket* sink) {
        if (sink != nullptr) {
            sinks_.push_back(sink);
        }
    }

    // =========================================================================
    // Single-Step Hot-Path Operations (Zero Allocation)
    // =========================================================================
    [[nodiscard]] bool produce_once() noexcept {
        auto maybe_slot = buffer_.begin_write();
        if (!maybe_slot.has_value()) {
            metrics_.frames_dropped_overrun.fetch_add(1, std::memory_order_relaxed);
            return false;
        }

        auto& slot = *maybe_slot;
        slot.metadata->sequence_id = next_sequence_id_++;
        slot.metadata->sensor_id = source_.sensor_id();
        slot.metadata->monotonic_ns = now_monotonic_ns();
        slot.metadata->utc_epoch_ns = now_utc_ns();

        TensorView view{};
        if (!source_.capture(slot, view)) {
            return false;
        }

        buffer_.commit_write(view);
        metrics_.frames_ingested.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    [[nodiscard]] bool consume_once() noexcept {
        auto maybe_slot = buffer_.begin_read();
        if (!maybe_slot.has_value()) {
            return false;
        }

        auto& slot = *maybe_slot;
        bool passed = true;

        for (IBooleanHook* hook : hooks_) {
            if (!hook->evaluate(slot.tensor, *slot.metadata)) {
                passed = false;
                break; // Short-circuit remaining hooks
            }
        }

        if (passed) {
            for (IIpcSocket* sink : sinks_) {
                (void)sink->publish(slot.tensor, *slot.metadata);
            }
            metrics_.frames_published.fetch_add(1, std::memory_order_relaxed);
        } else {
            metrics_.frames_gated_out.fetch_add(1, std::memory_order_relaxed);
        }

        buffer_.commit_read();
        return true;
    }

    // =========================================================================
    // Threaded Execution Lifecycle
    // =========================================================================
    void start() {
        bool expected = false;
        if (!running_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            return;
        }

        producer_thread_ = std::thread([this]() {
            while (running_.load(std::memory_order_acquire)) {
                if (!produce_once()) {
                    std::this_thread::yield();
                }
            }
        });

        consumer_thread_ = std::thread([this]() {
            while (running_.load(std::memory_order_acquire)) {
                if (!consume_once()) {
                    std::this_thread::yield();
                }
            }
            // Drain any remaining buffered slots on cooperative shutdown
            while (consume_once()) {}
            for (IIpcSocket* sink : sinks_) {
                sink->flush();
            }
        });
    }

    void stop() {
        bool expected = true;
        if (!running_.compare_exchange_strong(expected, false, std::memory_order_acq_rel)) {
            return;
        }

        if (producer_thread_.joinable()) {
            producer_thread_.join();
        }
        if (consumer_thread_.joinable()) {
            consumer_thread_.join();
        }
    }

    [[nodiscard]] const PipelineMetrics& metrics() const noexcept {
        return metrics_;
    }

private:
    [[nodiscard]] static std::uint64_t now_monotonic_ns() noexcept {
        const auto now = std::chrono::steady_clock::now().time_since_epoch();
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
    }

    [[nodiscard]] static std::uint64_t now_utc_ns() noexcept {
        const auto now = std::chrono::system_clock::now().time_since_epoch();
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
    }

    ISensorSource& source_;
    RingBuffer& buffer_;
    std::vector<IBooleanHook*> hooks_;
    std::vector<IIpcSocket*> sinks_;
    PipelineMetrics metrics_{};

    std::uint64_t next_sequence_id_{0};
    std::atomic<bool> running_{false};
    std::thread producer_thread_;
    std::thread consumer_thread_;
};

} // namespace axiom::core