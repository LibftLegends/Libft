#ifndef ANALYTICS_INSTRUMENTATION_HPP
# define ANALYTICS_INSTRUMENTATION_HPP

# include "analytics.hpp"

# if defined(LIBFT_ENABLE_ANALYTICS)
#  define FT_ANALYTICS_BEGIN_FRAME(session, frame_number) \
    analytics_begin_frame((session), (frame_number))
#  define FT_ANALYTICS_END_FRAME(session) \
    analytics_end_frame((session))
#  define FT_ANALYTICS_BEGIN_SCOPE(session, region_id) \
    analytics_begin_scope((session), (region_id))
#  define FT_ANALYTICS_END_SCOPE(session) \
    analytics_end_scope((session))
# else
#  define FT_ANALYTICS_BEGIN_FRAME(session, frame_number) FT_ERR_SUCCESS
#  define FT_ANALYTICS_END_FRAME(session) FT_ERR_SUCCESS
#  define FT_ANALYTICS_BEGIN_SCOPE(session, region_id) FT_ERR_SUCCESS
#  define FT_ANALYTICS_END_SCOPE(session) FT_ERR_SUCCESS
# endif

#endif
