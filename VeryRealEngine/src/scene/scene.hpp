/**
 * @file scene.hpp
 * @brief Scene graph, implemented as a genuine entity-component system:
 * each scene node is an ecs::Entity carrying a handful of small components
 * (Transform, Mesh, Visibility, ...) rather than one monolithic SceneNode
 * struct — see src/ecs/registry.hpp for the generic storage this plugs
 * into. collect_render_items() below is a "system" in the ECS sense: it
 * operates on whatever entities happen to have a Transform + Mesh, not on
 * a hardcoded node type.
 *
 * Loaded from a JSON scene file (step 3 of the roadmap in
 * ../../verdict.md). Hiding a node hides its entire subtree by
 * construction — visibility is combined down the parent chain in
 * collect_render_items(), so flipping one parent's Visibility component is
 * enough to hide it and everything under it, which is the exact scenario
 * the subject calls out for evaluation.
 */
#pragma once

#include "../ecs/registry.hpp"
#include "../math/vre_math.hpp"
#include "../renderer/renderer.hpp"
#include "../physics/physics_world.hpp"

#include <map>
#include <string>
#include <vector>

namespace vre
{

/// Sentinel MeshHandle meaning "no mesh" — an organizational or physics-only entity.
constexpr MeshHandle kNoMesh = static_cast<MeshHandle>(-1);

// --- Components --------------------------------------------------------
// Plain data, no behavior — every bit of scene logic below (transform
// composition, parent-chain visibility, render-list collection) lives in
// Scene's methods, operating generically over entities that have the
// components each one needs. That split (dumb components, systems that
// query for combinations of them) is what makes this an ECS rather than
// SceneNode with extra steps.

/// An entity's human-readable, lookup-by-string name (from the JSON scene file's "name" field).
struct NameComponent
{
    std::string name; ///< Unique name used for lookups (set_visible(), get_node_position(), ...).
};

/// Marks an entity as a child of another entity, for hierarchical transforms/visibility.
struct ParentComponent
{
    ecs::Entity parent = ecs::kInvalidEntity; ///< The parent entity.
};

/// An entity's local (parent-relative) transform, as authored in the scene file plus any runtime spin.
struct TransformComponent
{
    vec3 position;
    vec3 rotation; ///< Static base rotation, radians (from the scene file).
    vec3 scale{1.0f, 1.0f, 1.0f};
    vec3 spin;     ///< Continuous rotation speed, radians/second (optional, default 0).
    vec3 spin_accumulated;
};

/**
 * @brief Cached world-space transform.
 *
 * Populated every frame by collect_render_items()'s transform pass, for
 * anything that then needs "where is this entity in the world" without
 * re-walking the parent chain itself (get_node_position() below reuses it).
 */
struct WorldTransformComponent
{
    mat4 value = mat4::identity();
};

/// Marks an entity as having a renderable mesh.
struct MeshComponent
{
    MeshHandle mesh = kNoMesh;
};

/// An entity's own visibility flag (combined with its ancestors' — see collect_render_items()).
struct VisibilityComponent
{
    bool visible = true; ///< Editable at runtime; hides this entity AND its subtree.
};

/**
 * @brief Marks an entity as physics-driven.
 *
 * Presence of this component is what "has a rigid body" means — entities
 * without physics simply never get one added, rather than carrying a
 * sentinel "no body" value.
 */
struct PhysicsBodyComponent
{
    BodyHandle body = kInvalidBody;
};

/**
 * @brief Owns the scene's ECS registry and provides the JSON loader plus
 * the "systems" (transform composition, render-item collection, physics
 * sync) that operate over it.
 */
class Scene
{
    public:
        /**
         * @brief Parses `path` and loads every referenced mesh through `renderer`.
         *
         * Meshes are cached by path inside the renderer, so two nodes
         * pointing at the same .obj only upload it once.
         *
         * Nodes must appear after their parent in the file's "objects"
         * array (a scene authoring convention, not a technical limit of
         * the format) — world transforms are computed in file order.
         *
         * A node with an optional "physics" object gets a rigid body
         * created in `physics_world` (ignored if physics_world is
         * nullptr). Physics-driven nodes must be root nodes: the physics
         * simulation works in world space, but a node with a parent would
         * have its physics position re-interpreted as a *local* offset
         * from the parent, which is not what a rigid body means.
         * @param path Filesystem path to the JSON scene file.
         * @param renderer Renderer used to load referenced meshes.
         * @param physics_world If non-null, rigid bodies are created here for
         * nodes with a "physics" field.
         * @return true on success.
         */
        bool load(const char *path, Renderer *renderer, PhysicsWorld *physics_world = nullptr);

        /// Advances per-entity spin accumulators by `delta_seconds`.
        void update(float delta_seconds);

        /**
         * @brief Overwrites each physics-driven entity's position with its
         * rigid body's current position.
         *
         * Call once per frame after stepping the PhysicsWorld this scene
         * was loaded with, before collect_render_items().
         */
        void sync_from_physics(const PhysicsWorld &physics_world);

        /**
         * @brief The render-collection system.
         *
         * Recomputes WorldTransformComponent for every entity in load order
         * (parents before children, see load()'s doc comment above), then
         * emits a RenderItem for every entity whose whole ancestor chain
         * (including itself) is visible and that carries a MeshComponent.
         * @param out_items Render items for this frame are appended here.
         */
        void collect_render_items(std::vector<RenderItem> *out_items) const;

        /// Sets an entity's own visibility by name.
        /// @return false if no entity has that name.
        bool set_visible(const std::string &name, bool visible);
        /// Flips an entity's own visibility by name.
        /// @return false if no entity has that name.
        bool toggle_visible(const std::string &name);
        /// @return This entity's own visibility flag (not combined with ancestors), or false if not found.
        bool is_visible(const std::string &name) const;

        /**
         * @brief Sets an entity's local rotation at runtime.
         *
         * Used by the house demo's interactions (door hinge animation).
         * Overrides whatever static value the JSON scene file set; there's
         * no way back to the JSON value short of reloading the scene,
         * which is fine for a demo that only ever moves forward from here.
         * @return false if no entity has that name.
         */
        bool set_node_rotation(const std::string &name, const vec3 &euler_radians);
        /**
         * @brief Looks up an entity's current world-space position by name.
         *
         * Used for the house demo's proximity checks (light switch/door).
         * @param name Entity name to look up.
         * @param out_position Receives the world-space position on success.
         * @return false if no entity has that name.
         */
        bool get_node_position(const std::string &name, vec3 *out_position) const;

        /**
         * Parsed from an optional top-level "lights" array and "ambient"
         * field. get_lights()[0], if present, is treated as the shadow
         * caster by Renderer::draw_frame() and must be a Directional light.
         */
        const std::vector<Light> &get_lights() const { return _lights; }
        /// @return The scene's flat ambient intensity term.
        float get_ambient() const { return _ambient; }

        /// @return Number of lights parsed from the scene file.
        size_t get_light_count() const { return _lights.size(); }
        /**
         * @brief Runtime light control for the demo's light switch.
         *
         * Addressed by index into get_lights() (scene lights have no name
         * in the JSON format; the demo just needs to know which index it
         * authored the switchable light at).
         * @return false if `index` is out of range.
         */
        bool set_light_intensity(size_t index, float intensity);

    private:
        ecs::Registry _registry;
        std::map<std::string, ecs::Entity> _name_to_entity;
        /**
         * Load order == parent-before-child order (enforced in load()) —
         * every system below that needs to walk the hierarchy top-down
         * (collect_render_items) iterates entities in this order rather
         * than however the registry's internal hash maps happen to.
         */
        std::vector<ecs::Entity> _load_order;
        std::vector<Light> _lights;
        float _ambient = 0.12f;

        /// @return The local (parent-relative) matrix for `transform`, including accumulated spin.
        mat4 local_transform(const TransformComponent &transform) const;
};

} // namespace vre
