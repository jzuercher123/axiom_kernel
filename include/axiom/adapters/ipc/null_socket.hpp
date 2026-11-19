#pragma once

#include "axiom/core/ipc_socket.hpp"

#include <atomic>
#include <cstdint>

namespace axiom::adapters {

    class NullSocket final : public core::IIpcSocket {
    public:
        [[nodiscard]] bool publish(const core::TensorView&,
                                   const core::SlotMetadata&) noexcept override {
            published_count_.fetch_add(1, std::memory_order_relaxed);
            return true;
        }

        [[nodiscard]] std::uint64_t published_count() const noexcept {
            return published_count_.load(std::memory_order_relaxed);
        }

    private:
        std::atomic<std::uint64_t> published_count_{0};
    };

} // namespace axiom::adapters