#include "axiom/core/ring_buffer.hpp"

#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <thread>

using namespace axiom::core;

TEST(RingBufferTest, DropNewestBackpressure) {
    RingBuffer rb(2, 16, OverflowPolicy::DropNewest);

    for (std::uint64_t i = 0; i < 2; ++i) {
        auto slot = rb.begin_write();
        ASSERT_TRUE(slot.has_value());
        slot->metadata->sequence_id = i;
        rb.commit_write(TensorView::make_contiguous(slot->buffer, DType::UInt8, 1, {16, 0, 0, 0}));
    }

    // Third write must drop without blocking
    auto dropped = rb.begin_write();
    EXPECT_FALSE(dropped.has_value());
    EXPECT_EQ(rb.overrun_count(), 1u);

    // Oldest two frames must remain intact
    auto r0 = rb.begin_read();
    ASSERT_TRUE(r0.has_value());
    EXPECT_EQ(r0->metadata->sequence_id, 0u);
    rb.commit_read();

    auto r1 = rb.begin_read();
    ASSERT_TRUE(r1.has_value());
    EXPECT_EQ(r1->metadata->sequence_id, 1u);
    rb.commit_read();
}

TEST(RingBufferTest, OverwriteOldestBackpressure) {
    RingBuffer rb(2, 16, OverflowPolicy::OverwriteOldest);

    for (std::uint64_t i = 0; i < 3; ++i) {
        auto slot = rb.begin_write();
        ASSERT_TRUE(slot.has_value());
        slot->metadata->sequence_id = i;
        rb.commit_write(TensorView::make_contiguous(slot->buffer, DType::UInt8, 1, {16, 0, 0, 0}));
    }

    EXPECT_EQ(rb.overrun_count(), 1u);

    // Frame 0 was evicted; frames 1 and 2 should be read
    auto r1 = rb.begin_read();
    ASSERT_TRUE(r1.has_value());
    EXPECT_EQ(r1->metadata->sequence_id, 1u);
    rb.commit_read();

    auto r2 = rb.begin_read();
    ASSERT_TRUE(r2.has_value());
    EXPECT_EQ(r2->metadata->sequence_id, 2u);
    rb.commit_read();
}

TEST(RingBufferTest, ConcurrentSpscIntegrity) {
    constexpr std::uint64_t kTotalFrames = 10000;
    RingBuffer rb(64, sizeof(std::uint64_t), OverflowPolicy::DropNewest);

    std::thread producer([&]() {
        for (std::uint64_t seq = 0; seq < kTotalFrames; ) {
            auto slot = rb.begin_write();
            if (!slot.has_value()) {
                std::this_thread::yield();
                continue;
            }
            slot->metadata->sequence_id = seq;
            std::memcpy(slot->buffer, &seq, sizeof(seq));
            rb.commit_write(TensorView::make_contiguous(
                slot->buffer, DType::UInt8, 1, {sizeof(std::uint64_t), 0, 0, 0}));
            ++seq;
        }
    });

    std::uint64_t received = 0;
    std::thread consumer([&]() {
        while (received < kTotalFrames) {
            auto slot = rb.begin_read();
            if (!slot.has_value()) {
                std::this_thread::yield();
                continue;
            }
            std::uint64_t payload = 0;
            std::memcpy(&payload, slot->tensor.data, sizeof(payload));
            EXPECT_EQ(slot->metadata->sequence_id, received);
            EXPECT_EQ(payload, received);
            rb.commit_read();
            ++received;
        }
    });

    producer.join();
    consumer.join();
    EXPECT_EQ(received, kTotalFrames);
}