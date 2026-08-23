CMA_TARGET         := CustomMemoryAllocator.a
CMA_DEBUG_TARGET   := CustomMemoryAllocator_debug.a

CMA_SOURCES := cma_backend.cpp \
        cma_arena.cpp \
        cma_malloc.cpp \
        cma_free.cpp \
        cma_bzero_and_free.cpp \
        cma_free_checked.cpp \
        cma_realloc.cpp \
        cma_aligned_alloc.cpp \
        cma_alloc_size.cpp \
        cma_free_double.cpp \
        cma_globals.cpp \
        cma_allocator_lock.cpp \
        cma_metadata.cpp \
        cma_debug.cpp \
        cma_utils.cpp \
        cma_global_overloads.cpp \
        cma_set_alloc_limit.cpp \
        cma_set_thread_safety.cpp

CMA_HEADERS := CMA.hpp \
           cma_internal.hpp
