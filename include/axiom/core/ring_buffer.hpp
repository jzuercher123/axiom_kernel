#pragma once

#include "axiom/core/slot_metadata.hpp"
#include "axiom/core/tensor_view.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

namespace axiom::core {

enum class OverflowPolicy : std::uint8_t {
    DropNewest,
    OverwriteOldest
};

class RingBuffer {
public:
    struct WriteSlot {
        std::byte* buffer{nullptr};
        std::size_t capacity_bytes{0};
        SlotMetadata* metadata{nullptr};
    };

    struct ReadSlot {
        TensorView tensor{};
        SlotMetadata* metadata{nullptr};
    };

    RingBuffer(std::size_t capacity,
               std::size_t slot_size_bytes,
               OverflowPolicy policy = OverflowPolicy::DropNewest)
        : capacity_(capacity),
          slot_size_bytes_(slot_size_bytes),
          policy_(policy),
          arena_(capacity * slot_size_bytes),
          metadata_(capacity),
          views_(capacity) {
        if (capacity_ == 0 || slot_size_bytes_ == 0) {
            throw std::invalid_argument("RingBuffer capacity and slot_size_bytes must be > 0");
        }
    }

    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;
    RingBuffer(RingBuffer&&) = delete;
    RingBuffer& operator=(RingBuffer&&) = delete;

    // =========================================================================
    // Producer API (Hot Path - Zero Allocation, Non-Blocking)
    // =========================================================================
    [[nodiscard]] std::optional<WriteSlot> begin_write() noexcept {
        const std::uint64_t w = write_idx_.load(std::memory_order_relaxed);
        std::uint64_t rs = read_state_.load(std::memory_order_acquire);
        std::uint64_t r = rs >> 1u;

        if ((w - r) >= capacity_) {
            if (policy_ == OverflowPolicy::DropNewest) {
                overrun_count_.fetch_add(1, std::memory_order_relaxed);
                return std::nullopt;
            }

            // OverwriteOldest: only evict oldest slot if consumer is not actively reading it
            if ((rs & 1u) != 0u) {
                overrun_count_.fetch_add(1, std::memory_order_relaxed);
                return std::nullopt;
            }

            const std::uint64_t next_rs = (r + 1u) << 1u;
            if (!read_state_.compare_exchange_strong(
                    rs, next_rs, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                // Consumer claimed the slot concurrently; do not race or block
                overrun_count_.fetch_add(1, std::memory_order_relaxed);
                return std::nullopt;
            }
            overrun_count_.fetch_add(1, std::memory_order_relaxed);
        }

        const auto slot_idx = static_cast<std::size_t>(w % capacity_);
        metadata_[slot_idx].reset();
        views_[slot_idx] = TensorView{};

        return WriteSlot{
            arena_.data() + (slot_idx * slot_size_bytes_),
            slot_size_bytes_,
            &metadata_[slot_idx]
        };
    }

    void commit_write(const TensorView& view) noexcept {
        const std::uint64_t w = write_idx_.load(std::memory_order_relaxed);
        const auto slot_idx = static_cast<std::size_t>(w % capacity_);
        views_[slot_idx] = view;
        write_idx_.store(w + 1u, std::memory_order_release);
    }

    // =========================================================================
    // Consumer API (Hot Path - Zero Allocation, Non-Blocking)
    // =========================================================================
    [[nodiscard]] std::optional<ReadSlot> begin_read() noexcept {
        while (true) {
            std::uint64_t rs = read_state_.load(std::memory_order_relaxed);
            const std::uint64_t r = rs >> 1u;
            const std::uint64_t w = write_idx_.load(std::memory_order_acquire);

            if (r == w) {
                return std::nullopt;
            }

            // Mark slot as actively being read (bit 0 = 1)
            const std::uint64_t claimed = (r << 1u) | 1u;
            if (read_state_.compare_exchange_weak(
                    rs, claimed, std::memory_order_acquire, std::memory_order_relaxed)) {
                const auto slot_idx = static_cast<std::size_t>(r % capacity_);
                return ReadSlot{views_[slot_idx], &metadata_[slot_idx]};
            }
        }
    }

    void commit_read() noexcept {
        const std::uint64_t rs = read_state_.load(std::memory_order_relaxed);
        const std::uint64_t r = rs >> 1u;
        // Clear reading bit and advance read index
        read_state_.store((r + 1u) << 1u, std::memory_order_release);
    }

    // =========================================================================
    // Inspection & Telemetry
    // =========================================================================
    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }
    [[nodiscard]] std::size_t slot_size_bytes() const noexcept { return slot_size_bytes_; }
    [[nodiscard]] OverflowPolicy overflow_policy() const noexcept { return policy_; }

    [[nodiscard]] std::uint64_t overrun_count() const noexcept {
        return overrun_count_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] std::size_t size_approx() const noexcept {
        const std::uint64_t w = write_idx_.load(std::memory_order_acquire);
        const std::uint64_t r = read_state_.load(std::memory_order_acquire) >> 1u;
        return (w >= r) ? static_cast<std::size_t>(w - r) : 0;
    }

private:
    std::size_t capacity_;
    std::size_t slot_size_bytes_;
    OverflowPolicy policy_;

    // Pre-allocated once at startup; never resized on hot path
    std::vector<std::byte> arena_;
    std::vector<SlotMetadata> metadata_;
    std::vector<TensorView> views_;

    // Separated onto distinct 64-byte cache lines to prevent false sharing
    alignas(64) std::atomic<std::uint64_t> write_idx_{0};
    // Packed state: bits [63:1] = read_idx, bit [0] = consumer actively reading
    alignas(64) std::atomic<std::uint64_t> read_state_{0};
    alignas(64) std::atomic<std::uint64_t> overrun_count_{0};
};

} // namespace axiom::core