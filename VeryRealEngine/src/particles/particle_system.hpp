// A minimal CPU-side particle system — step 7's "particle-emitting object"
// requirement (e.g. a coffee machine emitting steam).
//
// Scope decision: particles are rendered as small shrinking cubes (reusing
// the ordinary mesh/material/RenderItem pipeline every other object uses),
// not soft camera-facing billboards or a GPU-instanced sprite system. That
// keeps this a self-contained CPU simulation with zero new rendering
// infrastructure (no billboard shader, no per-instance vertex buffers) —
// real puffs of rising, fading geometry, just blocky ones. Billboards
// would look better and are a natural follow-up, not attempted here.
#pragma once

#include "../math/vre_math.hpp"
#include "../renderer/renderer.hpp"

#include <vector>

namespace vre
{

struct ParticleSystemDesc
{
    vec3 emitter_position;
    MeshHandle mesh = 0;         // typically a small cube (see cube_steam.obj)
    float spawn_interval = 0.15f; // seconds between new particles
    float lifetime = 2.2f;        // seconds a particle lives before despawning
    float start_scale = 0.14f;
    vec3 base_velocity{0.0f, 0.5f, 0.0f}; // upward drift, steam-like
    float velocity_jitter = 0.12f;        // random horizontal spread
    uint32_t random_seed = 1;
};

class ParticleSystem
{
    public:
        explicit ParticleSystem(const ParticleSystemDesc &desc);

        void update(float delta_seconds);
        void collect_render_items(std::vector<RenderItem> *out_items) const;

    private:
        struct Particle
        {
            vec3 position;
            vec3 velocity;
            float age = 0.0f;
        };

        ParticleSystemDesc _desc;
        std::vector<Particle> _particles;
        float _spawn_accumulator = 0.0f;
        uint32_t _rng_state;

        float next_random(); // [0,1), a tiny xorshift PRNG — no <random> needed
};

} // namespace vre
