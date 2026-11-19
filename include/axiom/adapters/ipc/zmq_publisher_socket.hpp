#pragma once

#include "axiom/core/ipc_socket.hpp"

#include <zmq.hpp>

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>

namespace axiom::adapters {

struct WireHeader {
    core::SlotMetadata metadata{};
    core::DType dtype{core::DType::UInt8};
    std::uint8_t rank{0};
    std::array<std::size_t, 4> extents{0, 0, 0, 0};
};

class ZmqPublisherSocket final : public core::IIpcSocket {
public:
    ZmqPublisherSocket(zmq::context_t& ctx,
                       std::string endpoint,
                       std::string topic,
                       int sndhwm = 1000)
        : socket_(ctx, zmq::socket_type::pub),
          endpoint_(std::move(endpoint)),
          topic_(std::move(topic)) {
        socket_.set(zmq::sockopt::sndhwm, sndhwm);
        socket_.set(zmq::sockopt::linger, 0);
        socket_.bind(endpoint_);
    }

    [[nodiscard]] bool publish(const core::TensorView& tensor,
                               const core::SlotMetadata& meta) noexcept override {
        try {
            WireHeader header{};
            header.metadata = meta;
            header.dtype = tensor.dtype;
            header.rank = tensor.rank;
            header.extents = tensor.extents;

            // Part 1: Topic / Node ID
            if (!socket_.send(zmq::const_buffer(topic_.data(), topic_.size()),
                              zmq::send_flags::sndmore | zmq::send_flags::dontwait)) {
                return false;
            }

            // Part 2: Binary WireHeader (SlotMetadata + TensorView geometry)
            if (!socket_.send(zmq::const_buffer(&header, sizeof(header)),
                              zmq::send_flags::sndmore | zmq::send_flags::dontwait)) {
                return false;
            }

            // Part 3: Raw tensor payload bytes
            const auto payload_size = tensor.byte_size();
            const void* payload_ptr = (payload_size > 0) ? static_cast<const void*>(tensor.data) : nullptr;
            if (!socket_.send(zmq::const_buffer(payload_ptr, payload_size),
                              zmq::send_flags::dontwait)) {
                return false;
            }

            return true;
        } catch (...) {
            return false;
        }
    }

    [[nodiscard]] const std::string& endpoint() const noexcept { return endpoint_; }
    [[nodiscard]] const std::string& topic() const noexcept { return topic_; }

private:
    zmq::socket_t socket_;
    std::string endpoint_;
    std::string topic_;
};

} // namespace axiom::adapters