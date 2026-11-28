#include "../../../Modules/Template/matrix.hpp"
#include <chrono>
#include <cstdio>
#include <memory>

int main()
{
    constexpr ft_size_t ROWS = 200U;
    constexpr ft_size_t INNER = 240U;
    constexpr ft_size_t COLS = 256U;
    constexpr uint32_t ITERATIONS = 12U;
    ft_matrix<double> left;
    ft_matrix<double> right;
    if (left.initialize(ROWS, INNER) != FT_ERR_SUCCESS
        || right.initialize(INNER, COLS) != FT_ERR_SUCCESS)
        return (1);
    ft_size_t inner = 0U;
    while (inner < INNER)
    {
        ft_size_t row = 0U;
        while (row < ROWS)
        {
            left.at(row, inner) = 2.0;
            ++row;
        }
        ft_size_t column = 0U;
        while (column < COLS)
        {
            right.at(inner, column) = 3.0;
            ++column;
        }
        ++inner;
    }
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        const std::unique_ptr<ft_matrix<double>> product(left.multiply(right));
        if (!product || product->at(ROWS - 1U, COLS - 1U) != 6.0 * INNER)
            return (1);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    std::printf("matrix_product ms/op=%.4f\n",
        std::chrono::duration<double, std::milli>(elapsed).count() / ITERATIONS);
    return (0);
}
