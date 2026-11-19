#pragma once

#include "axiom/core/ring_buffer.hpp"
#include "axiom/core/tensor_view.hpp"

#include <cstdint>
#include <string_view>

namespace axiom::core {

    class ISensorSource {
    public:
        virtual ~ISensorSource() = default;

        [[nodiscard]] virtual std::string_view name() const noexcept = 0;
        [[nodiscard]] virtual std::uint32_t sensor_id() const noexcept = 0;

        // Writes raw sensor bytes directly into slot.buffer and describes them via out_view.
        // Returns false if no frame is available or the source has reached EOF.
        [[nodiscard]] virtual bool capture(RingBuffer::WriteSlot& slot,
                                           TensorView& out_view) noexcept = 0;
    };

} // namespace axiom::cores