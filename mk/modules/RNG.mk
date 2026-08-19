RNG_TARGET := RNG.a
RNG_DEBUG_TARGET := RNG_debug.a

RNG_SOURCES := RNG_dice_roll.cpp RNG_engine.cpp RNG_random_int.cpp RNG_random_float.cpp \
       RNG_random_vector.cpp \
       RNG_random_normal.cpp RNG_random_exponential.cpp RNG_random_poisson.cpp \
       RNG_random_binomial.cpp RNG_random_geometric.cpp RNG_random_gamma.cpp \
       RNG_random_beta.cpp RNG_random_chi_squared.cpp RNG_distribution_functions.cpp \
       RNG_random_seed.cpp RNG_secure_bytes.cpp RNG_secure_wrappers.cpp \
       RNG_seed_value.cpp \
       RNG_stream.cpp \
       RNG_stream_split.cpp RNG_uuid.cpp

RNG_HEADERS := rng.hpp rng_internal.hpp deck.hpp loot_table.hpp rng_stream.hpp
