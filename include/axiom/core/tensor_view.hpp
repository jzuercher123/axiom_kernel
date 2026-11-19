#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace axiom::core {

enum class DType : std::uint8_t {
    UInt8,
    UInt16,
    Int16,
    Int32,
    Float32,
    Float64
};

[[nodiscard]] constexpr std::size_t dtype_size(DType dtype) noexcept {
    switch (dtype) {
        case DType::UInt8:   return 1;
        case DType::UInt16:  return 2;
        case DType::Int16:   return 2;
        case DType::Int32:   return 4;
        case DType::Float32: return 4;
        case DType::Float64: return 8;
    }
    return 0;
}

struct TensorView {
    const std::byte* data{nullptr};
    std::array<std::size_t, 4> extents{0, 0, 0, 0}; // Up to 4D without heap allocation
    std::array<std::size_t, 4> strides{0, 0, 0, 0}; // Element strides per dimension
    std::uint8_t rank{0};
    DType dtype{DType::UInt8};

    [[nodiscard]] constexpr bool empty() const noexcept {
        return data == nullptr || rank == 0 || rank > 4;
    }

    [[nodiscard]] constexpr std::size_t element_count() const noexcept {
        if (empty()) {
            return 0;
        }
        std::size_t count = 1;
        for (std::uint8_t i = 0; i < rank; ++i) {
            count *= extents[i];
        }
        return count;
    }

    [[nodiscard]] constexpr std::size_t byte_size() const noexcept {
        return element_count() * dtype_size(dtype);
    }

    // Construct a row-major contiguous 1D-4D TensorView without heap allocation
    [[nodiscard]] static constexpr TensorView make_contiguous(
        const std::byte* ptr,
        DType type,
        std::uint8_t num_dims,
        std::array<std::size_t, 4> shape) noexcept {
        TensorView view{};
        if (ptr == nullptr || num_dims == 0 || num_dims > 4) {
            return view;
        }
        view.data = ptr;
        view.dtype = type;
        view.rank = num_dims;
        view.extents = shape;

        std::size_t stride = 1;
        for (std::uint8_t i = num_dims; i > 0; --i) {
            const auto idx = static_cast<std::size_t>(i - 1);
            view.strides[idx] = stride;
            stride *= shape[idx];
        }
        return view;
    }

    // Zero-copy element offset calculation (in elements)
    [[nodiscard]] constexpr std::size_t linear_index(
        std::size_t d0,
        std::size_t d1 = 0,
        std::size_t d2 = 0,
        std::size_t d3 = 0) const noexcept {
        return d0 * strides[0] + d1 * strides[1] + d2 * strides[2] + d3 * strides[3];
    }
};

} // namespace axiom::core