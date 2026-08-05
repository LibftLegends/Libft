#include "cma_internal.hpp"

Page *page_list = nullptr;
uint64_t g_cma_page_generation = 0;
Block *g_cma_free_bins[CMA_FREE_BIN_COUNT] = {nullptr};
ft_size_t    g_cma_alloc_limit = 0;
ft_bool    g_cma_alloc_logging = FT_FALSE;
ft_size_t    g_cma_allocation_count = 0;
ft_size_t    g_cma_free_count = 0;
ft_size_t    g_cma_current_bytes = 0;
ft_size_t    g_cma_peak_bytes = 0;
int64_t    g_cma_metadata_access_depth = 0;
