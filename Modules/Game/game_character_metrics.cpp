#include "game_character.hpp"
#include "../Observability/observability_game_metrics.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"

void game_character::emit_game_metric(const char *event_name, const char *attribute,
    int64_t delta_value, int64_t total_value, const char *unit) const noexcept
{
    if (delta_value == 0)
        return ;
    const ft_game_observability_sample sample = {{event_name, "game_character", attribute},
        delta_value, total_value, unit, FT_ERR_SUCCESS, ft_nullptr, FT_TRUE};
    observability_game_metrics_record(sample);
    return ;
}
