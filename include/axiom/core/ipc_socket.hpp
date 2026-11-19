#pragma once

#include "axiom/core/slot_metadata.hpp"
#include "axiom/core/tensor_view.hpp"

namespace axiom::core {

    class IIpcSocket {
    public:
        virtual ~IIpcSocket() = default;

        [[nodiscard]] virtual bool publish(const TensorView& tensor,
                                           const SlotMetadata& meta) noexcept = 0;

        virtual void flush() noexcept {}
    };

} // namespace axiom::core