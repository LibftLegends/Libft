#include "../../../Modules/YAML/yaml.hpp"
#include <chrono>
#include <cstdio>

int main()
{
    constexpr ft_size_t LINE_COUNT = 512;
    constexpr ft_size_t PAYLOAD_LENGTH = 512;
    constexpr ft_size_t ITERATIONS = 4;
    ft_string input;
    ft_string payload;
    ft_size_t index = 0;

    if (input.initialize() != FT_ERR_SUCCESS
        || payload.initialize(PAYLOAD_LENGTH, 'x') != FT_ERR_SUCCESS)
        return (1);
    while (index < LINE_COUNT)
    {
        if (input.append("  ") != FT_ERR_SUCCESS
            || input.append(payload) != FT_ERR_SUCCESS
            || input.append(" \r\n") != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }

    const auto start = std::chrono::steady_clock::now();
    index = 0;
    while (index < ITERATIONS)
    {
        ft_vector<ft_string> lines;
        if (yaml_split_lines(input, lines) != FT_ERR_SUCCESS || lines.size() != LINE_COUNT)
            return (1);
        ft_size_t line_index = 0;
        while (line_index < LINE_COUNT)
        {
            ft_string &line = lines[line_index];
            yaml_trim(line);
            if (line.size() != PAYLOAD_LENGTH || line.c_str()[0] != 'x'
                || line.c_str()[PAYLOAD_LENGTH - 1] != 'x')
                return (1);
            ++line_index;
        }
        ++index;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    std::printf("read=%.3f us\n",
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS);
    return (0);
}
