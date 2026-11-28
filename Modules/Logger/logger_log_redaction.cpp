#include "logger_internal.hpp"
#include "../Basic/basic.hpp"
#include "../Basic/limits.hpp"
#include <new>
#include "../Template/vector.hpp"
#include "../Errno/errno.hpp"

struct s_log_redaction_snapshot
{
    std::atomic<ft_size_t> references{1U};
    ft_vector<s_redaction_rule> rules;
};

struct s_log_redaction_state
{
    std::atomic<s_log_redaction_snapshot *> snapshot{nullptr};

    ~s_log_redaction_state()
    {
        delete this->snapshot.load(std::memory_order_relaxed);
        return ;
    }
};

static s_log_redaction_state g_redaction_state;
static pthread_mutex_t g_redaction_mutex = PTHREAD_MUTEX_INITIALIZER;

void logger_release_redaction_snapshot(s_log_redaction_snapshot *snapshot)
{
    if (snapshot != ft_nullptr
        && snapshot->references.fetch_sub(1U, std::memory_order_acq_rel) == 1U)
        delete snapshot;
    return ;
}

int32_t s_redaction_rule::initialize(const s_redaction_rule &other) noexcept
{
    int32_t error_code;

    error_code = this->pattern.initialize(other.pattern);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->replacement.initialize(other.replacement);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (FT_ERR_SUCCESS);
}

int32_t s_redaction_rule::destroy() noexcept
{
    int32_t first_error;
    int32_t error_code;

    first_error = this->pattern.destroy();
    error_code = this->replacement.destroy();
    if (first_error == FT_ERR_SUCCESS && error_code != FT_ERR_SUCCESS)
        first_error = error_code;
    return (first_error);
}

static int32_t logger_apply_redaction_rule(ft_string &text,
    const s_redaction_rule &rule)
{
    if (!text.is_initialised() || !rule.pattern.is_initialised()
        || !rule.replacement.is_initialised())
        return (FT_ERR_NOT_INITIALISED);
    const ft_size_t pattern_size = rule.pattern.size();
    if (pattern_size == 0U)
        return (FT_ERR_SUCCESS);
    const char *pattern = rule.pattern.c_str();
    const char *cursor = text.c_str();
    const char *end = cursor + text.size();
    const char *match = static_cast<const char *>(ft_memmem(cursor,
        static_cast<ft_size_t>(end - cursor), pattern, pattern_size));
    if (match == ft_nullptr)
        return (FT_ERR_SUCCESS);
    ft_string result;
    int32_t error = result.initialize();
    if (error != FT_ERR_SUCCESS)
        return (error);
    while (match != ft_nullptr)
    {
        error = result.append(cursor, static_cast<ft_size_t>(match - cursor));
        if (error == FT_ERR_SUCCESS)
            error = result.append(rule.replacement);
        if (error != FT_ERR_SUCCESS)
            return (error);
        cursor = match + pattern_size;
        match = static_cast<const char *>(ft_memmem(cursor,
            static_cast<ft_size_t>(end - cursor), pattern, pattern_size));
    }
    error = result.append(cursor, static_cast<ft_size_t>(end - cursor));
    if (error != FT_ERR_SUCCESS)
        return (error);
    return (text.move(result));
}

int32_t ft_log_add_redaction_with_replacement(const char *pattern,
    const char *replacement)
{
    if (pattern == ft_nullptr || pattern[0] == '\0')
        return (FT_ERR_INVALID_ARGUMENT);
    if (replacement == ft_nullptr)
        replacement = "[REDACTED]";
    s_redaction_rule rule;
    int32_t error = rule.pattern.initialize(pattern);
    if (error == FT_ERR_SUCCESS)
        error = rule.replacement.initialize(replacement);
    if (error != FT_ERR_SUCCESS)
        return (error);

    auto *updated = new (std::nothrow) s_log_redaction_snapshot;
    if (updated == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    if (pthread_mutex_lock(&g_redaction_mutex) != 0)
    {
        delete updated;
        return (FT_ERR_INTERNAL);
    }
    s_log_redaction_snapshot *previous = g_redaction_state.snapshot.load(std::memory_order_relaxed);
    if (previous == ft_nullptr)
        error = updated->rules.initialize();
    else
        error = updated->rules.initialize(previous->rules);
    if (error == FT_ERR_SUCCESS)
        error = updated->rules.push_back(rule);
    if (error == FT_ERR_SUCCESS)
        g_redaction_state.snapshot.store(updated, std::memory_order_release);
    (void)pthread_mutex_unlock(&g_redaction_mutex);

    if (error == FT_ERR_SUCCESS)
        logger_release_redaction_snapshot(previous);
    else
        delete updated;
    return (error);
}

int32_t ft_log_add_redaction(const char *pattern)
{
    return (ft_log_add_redaction_with_replacement(pattern, "[REDACTED]"));
}

void ft_log_clear_redactions()
{
    if (pthread_mutex_lock(&g_redaction_mutex) != 0)
        return ;
    s_log_redaction_snapshot *previous = g_redaction_state.snapshot.exchange(
        nullptr, std::memory_order_release);
    (void)pthread_mutex_unlock(&g_redaction_mutex);
    logger_release_redaction_snapshot(previous);
    return ;
}

int32_t logger_acquire_redaction_snapshot(s_log_redaction_snapshot *&snapshot)
{
    snapshot = ft_nullptr;
    // The fast check never dereferences the pointer. The mutex protects retention.
    if (g_redaction_state.snapshot.load(std::memory_order_acquire) != ft_nullptr)
    {
        if (pthread_mutex_lock(&g_redaction_mutex) != 0)
            return (FT_ERR_INTERNAL);
        snapshot = g_redaction_state.snapshot.load(std::memory_order_relaxed);
        if (snapshot != ft_nullptr)
            snapshot->references.fetch_add(1U, std::memory_order_relaxed);
        (void)pthread_mutex_unlock(&g_redaction_mutex);
    }
    return (FT_ERR_SUCCESS);
}

int32_t logger_redact_log_value(const s_log_redaction_snapshot *snapshot,
    const char *&value, ft_string &storage)
{
    if (snapshot == ft_nullptr || value == ft_nullptr)
        return (FT_ERR_SUCCESS);
    const ft_size_t length = ft_strlen_size_t(value);
    const ft_size_t count = snapshot->rules.size();
    ft_size_t index = 0U;
    while (index < count)
    {
        const s_redaction_rule &rule = snapshot->rules[index];
        if (ft_memmem(value, length, rule.pattern.c_str(), rule.pattern.size()) != ft_nullptr)
        {
            int32_t error = storage.initialize(value);
            while (error == FT_ERR_SUCCESS && index < count)
            {
                error = logger_apply_redaction_rule(storage, snapshot->rules[index]);
                index += 1U;
            }
            if (error == FT_ERR_SUCCESS)
                value = storage.c_str();
            return (error);
        }
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

int32_t logger_prepare_redacted_context(ft_string &text, ft_string &context)
{
    if (text.is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    s_log_redaction_snapshot *snapshot;
    int32_t error = logger_acquire_redaction_snapshot(snapshot);
    if (error != FT_ERR_SUCCESS)
        return (error);
    if (snapshot != ft_nullptr)
        error = logger_apply_redactions(text, snapshot->rules);
    if (error == FT_ERR_SUCCESS)
        error = context.initialize();
    if (error == FT_ERR_SUCCESS)
        error = logger_context_format_flat(context);
    if (error == FT_ERR_SUCCESS && snapshot != ft_nullptr && context.size() != 0U)
        error = logger_apply_redactions(context, snapshot->rules);
    logger_release_redaction_snapshot(snapshot);
    return (error);
}

int32_t logger_apply_redactions(ft_string &text,
    const ft_vector<s_redaction_rule> &rules)
{
    ft_size_t rule_count;
    ft_size_t entry_index;
    int32_t error_code_value;

    if (!rules.is_initialised())
        return (FT_ERR_NOT_INITIALISED);
    if (!text.is_initialised())
        return (FT_ERR_NOT_INITIALISED);
    rule_count = rules.size();
    error_code_value = rules.get_error();
    if (error_code_value != FT_ERR_SUCCESS)
        return (error_code_value);
    entry_index = 0;
    while (entry_index < rule_count)
    {
        int32_t             apply_error;

        const s_redaction_rule &source_rule = rules[entry_index];
        error_code_value = rules.get_error();
        if (error_code_value != FT_ERR_SUCCESS)
            return (error_code_value);
        apply_error = logger_apply_redaction_rule(text, source_rule);
        if (apply_error != FT_ERR_SUCCESS)
            return (apply_error);
        entry_index++;
    }
    error_code_value = FT_ERR_SUCCESS;
    return (error_code_value);
}
