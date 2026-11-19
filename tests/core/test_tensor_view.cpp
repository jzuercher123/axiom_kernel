#include "axiom/core/tensor_view.hpp"

#include <array>
#include <cstddef>
#include <gtest/gtest.h>

using namespace axiom::core;

TEST(TensorViewTest, EmptyAndInvalidStates) {
    TensorView empty_view{};
    EXPECT_TRUE(empty_view.empty());
    EXPECT_EQ(empty_view.element_count(), 0u);
    EXPECT_EQ(empty_view.byte_size(), 0u);

    std::array<std::byte, 16> raw{};
    auto invalid_rank = TensorView::make_contiguous(raw.data(), DType::UInt8, 5, {1, 1, 1, 1});
    EXPECT_TRUE(invalid_rank.empty());
}

TEST(TensorViewTest, Contiguous1DTo4DIndexingAndByteSize) {
    // 2D Optical Frame (4 rows x 8 cols of UInt16 = 64 bytes)
    std::array<std::byte, 64> buf2d{};
    auto view2d = TensorView::make_contiguous(buf2d.data(), DType::UInt16, 2, {4, 8, 0, 0});

    EXPECT_FALSE(view2d.empty());
    EXPECT_EQ(view2d.element_count(), 32u);
    EXPECT_EQ(view2d.byte_size(), 64u);
    EXPECT_EQ(view2d.strides[0], 8u);
    EXPECT_EQ(view2d.strides[1], 1u);
    EXPECT_EQ(view2d.linear_index(2, 5), 21u);

    // 4D Detector Hit-Map (2 x 3 x 4 x 5 of Float32 = 480 bytes)
    std::array<std::byte, 480> buf4d{};
    auto view4d = TensorView::make_contiguous(buf4d.data(), DType::Float32, 4, {2, 3, 4, 5});

    EXPECT_EQ(view4d.element_count(), 120u);
    EXPECT_EQ(view4d.byte_size(), 480u);
    EXPECT_EQ(view4d.strides[0], 60u);
    EXPECT_EQ(view4d.strides[1], 20u);
    EXPECT_EQ(view4d.strides[2], 5u);
    EXPECT_EQ(view4d.strides[3], 1u);
    EXPECT_EQ(view4d.linear_index(1, 2, 3, 4), 119u);
}