#include "../test_internal.hpp"
#include "../../Modules/Template/matrix.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <memory>

FT_TEST(test_ft_matrix_rectangular_arithmetic_and_self_add)
{
    ft_matrix<int32_t> left;
    ft_matrix<int32_t> right;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, left.initialize(2U, 3U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, right.initialize(3U, 2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, left.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, right.enable_thread_safety());
    ft_size_t index = 0;
    while (index < 6U)
    {
        left.at(index / 3U, index % 3U) = static_cast<int32_t>(index + 1U);
        right.at(index / 2U, index % 2U) = static_cast<int32_t>(index + 7U);
        ++index;
    }
    const std::unique_ptr<ft_matrix<int32_t>> product(left.multiply(right));
    FT_ASSERT(product.get() != ft_nullptr);
    FT_ASSERT_EQ(2U, product->rows());
    FT_ASSERT_EQ(2U, product->cols());
    FT_ASSERT_EQ(58, product->at(0U, 0U));
    FT_ASSERT_EQ(64, product->at(0U, 1U));
    FT_ASSERT_EQ(139, product->at(1U, 0U));
    FT_ASSERT_EQ(154, product->at(1U, 1U));
    const std::unique_ptr<ft_matrix<int32_t>> doubled(left.add(left));
    FT_ASSERT(doubled.get() != ft_nullptr);
    FT_ASSERT_EQ(12, doubled->at(1U, 2U));
    const std::unique_ptr<ft_matrix<int32_t>> transpose(left.transpose());
    FT_ASSERT(transpose.get() != ft_nullptr);
    FT_ASSERT_EQ(3U, transpose->rows());
    FT_ASSERT_EQ(2U, transpose->cols());
    FT_ASSERT_EQ(6, transpose->at(2U, 1U));
    const std::unique_ptr<ft_matrix<int32_t>> mismatch(left.add(right));
    FT_ASSERT(mismatch.get() == ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, left.get_error());
    return (1);
}

FT_TEST(test_ft_matrix_zero_width_product_and_overflowed_dimensions)
{
    ft_matrix<double> left;
    ft_matrix<double> right;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, left.initialize(2U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, right.initialize(0U, 2U));
    const std::unique_ptr<ft_matrix<double>> product(left.multiply(right));
    FT_ASSERT(product.get() != ft_nullptr);
    FT_ASSERT_EQ(2U, product->rows());
    FT_ASSERT_EQ(2U, product->cols());
    FT_ASSERT_EQ(0.0, product->at(1U, 1U));

    const ft_size_t overflowed_row_count = FT_SYSTEM_SIZE_MAX / 2U + 1U;
    FT_ASSERT_EQ(FT_FALSE, left.init(overflowed_row_count, 2U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, left.get_error());
    FT_ASSERT_EQ(2U, left.rows());
    FT_ASSERT_EQ(0U, left.cols());
    ft_matrix<double> oversized;
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, oversized.initialize(FT_SYSTEM_SIZE_MAX / sizeof(double) + 1U, 1U));
    return (1);
}
