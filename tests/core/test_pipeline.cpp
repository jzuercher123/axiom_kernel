#include "axiom/core/pipeline.hpp"

#include <atomic>
#include <cstdint>
#include <gtest/gtest.h>

using namespace axiom::core;

namespace {

class MockSensor final : public ISensorSource {
public:
    explicit MockSensor(std::uint64_t max_frames) : max_frames_(max_frames) {}

    [[nodiscard]] std::string_view name() const noexcept override { return "MockSensor"; }
    [[nodiscard]] std::uint32_t sensor_id() const noexcept override { return 42u; }

    [[nodiscard]] bool capture(RingBuffer::WriteSlot& slot, TensorView& out_view) noexcept override {
        const auto current = produced_.fetch_add(1, std::memory_order_relaxed);
        if (current >= max_frames_) {
            return false;
        }
        slot.buffer[0] = static_cast<std::byte>(current % 2u); // Alternating 0 and 1
        out_view = TensorView::make_contiguous(slot.buffer, DType::UInt8, 1, {1, 0, 0, 0});
        return true;
    }

private:
    std::uint64_t max_frames_;
    std::atomic<std::uint64_t> produced_{0};
};

class ParityGateHook final : public IBooleanHook {
public:
    [[nodiscard]] std::string_view name() const noexcept override { return "ParityGateHook"; }

    [[nodiscard]] bool evaluate(const TensorView& tensor, SlotMetadata& meta) noexcept override {
        ++calls;
        const bool pass = (static_cast<std::uint8_t>(tensor.data[0]) == 1u);
        if (pass) {
            meta.anomaly_score = 1.0;
        }
        return pass;
    }

    std::uint64_t calls{0};
};

class CountingHook final : public IBooleanHook {
public:
    [[nodiscard]] std::string_view name() const noexcept override { return "CountingHook"; }

    [[nodiscard]] bool evaluate(const TensorView&, SlotMetadata&) noexcept override {
        ++calls;
        return true;
    }

    std::uint64_t calls{0};
};

class CountingSink final : public IIpcSocket {
public:
    [[nodiscard]] bool publish(const TensorView&, const SlotMetadata& meta) noexcept override {
        EXPECT_DOUBLE_EQ(meta.anomaly_score, 1.0);
        ++published;
        return true;
    }

    std::uint64_t published{0};
};

} // namespace

TEST(PipelineTest, ShortCircuitHookChainingAndMetrics) {
    MockSensor sensor(10);
    RingBuffer rb(16, 16, OverflowPolicy::DropNewest);
    Pipeline pipeline(sensor, rb);

    ParityGateHook gate;
    CountingHook downstream_hook;
    CountingSink sink;

    pipeline.add_hook(&gate);
    pipeline.add_hook(&downstream_hook);
    pipeline.add_sink(&sink);

    for (int i = 0; i < 10; ++i) {
        ASSERT_TRUE(pipeline.produce_once());
        ASSERT_TRUE(pipeline.consume_once());
    }

    // 10 frames ingested: 5 even (dropped), 5 odd (passed)
    EXPECT_EQ(gate.calls, 10u);
    EXPECT_EQ(downstream_hook.calls, 5u); // Short-circuited on the 5 dropped frames
    EXPECT_EQ(sink.published, 5u);

    EXPECT_EQ(pipeline.metrics().frames_ingested.load(), 10u);
    EXPECT_EQ(pipeline.metrics().frames_gated_out.load(), 5u);
    EXPECT_EQ(pipeline.metrics().frames_published.load(), 5u);
    EXPECT_EQ(pipeline.metrics().frames_dropped_overrun.load(), 0u);
}