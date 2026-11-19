#pragma once

#include "axiom/core/slot_metadata.hpp"
#include "axiom/core/tensor_view.hpp"

#include <string_view>

namespace axiom::core {

    class IBooleanHook {
    public:
        virtual ~IBooleanHook() = default;

        [[nodiscard]] virtual std::string_view name() const noexcept = 0;

        [[nodiscard]] virtual bool evaluate(const TensorView& tensor,
                                            SlotMetadata& meta) noexcept = 0;
    };

} // namespace axiom::core