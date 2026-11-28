#include "../../../Modules/Logger/logger_internal.hpp"
#include <chrono>
#include <cstdio>
#include <string>

static int32_t benchmark_redaction(const char *label, const std::string &input,
    const ft_vector<s_redaction_rule> &rules)
{
    constexpr uint32_t ITERATIONS = 512U;
    ft_string text;
    if (text.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint32_t checksum = 0U;
    uint32_t index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        if (text.assign(input.data(), input.size()) != FT_ERR_SUCCESS
            || logger_apply_redactions(text, rules) != FT_ERR_SUCCESS)
            return (1);
        checksum += static_cast<uint32_t>(text.size());
        checksum += static_cast<uint8_t>(text.c_str()[128]);
        index += 1U;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("%s: %.2f ns/op, checksum %u\n",
        label, elapsed / ITERATIONS, checksum);
    return (0);
}

int32_t main()
{
    ft_vector<s_redaction_rule> rules;
    s_redaction_rule rule;
    if (rules.initialize() != FT_ERR_SUCCESS
        || rule.pattern.initialize("secret") != FT_ERR_SUCCESS
        || rule.replacement.initialize("[hidden]") != FT_ERR_SUCCESS
        || rules.push_back(rule) != FT_ERR_SUCCESS)
        return (1);
    const std::string ordinary(512U, 'a');
    const std::string matched = ordinary.substr(0U, 128U) + "secret"
        + ordinary.substr(128U, 256U) + "secret" + ordinary.substr(384U);
    return (benchmark_redaction("no redaction match", ordinary, rules)
        || benchmark_redaction("two redaction matches", matched, rules));
}
