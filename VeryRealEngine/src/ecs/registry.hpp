/**
 * @file registry.hpp
 * @brief A minimal, generic entity-component-system core: entities are
 * plain integer handles, components are plain data structs, storage is a
 * type-erased map from component type to a dense-by-entity table.
 *
 * This file knows nothing about scenes, meshes, or transforms — those are
 * defined as ordinary components by whoever uses this (see
 * src/scene/scene.hpp) — which is what makes it a real, reusable ECS
 * rather than the old scene graph with new names: any future subsystem
 * can define its own component types and iterate entities that have them,
 * without touching this file or Scene at all.
 *
 * Deliberately not reusing FullLibft's Modules/Game (a bespoke RPG-style
 * game-logic layer, not an ECS — see ../../verdict.md) or any third-party
 * ECS library: the subject requires this specific core component to be
 * implemented from scratch.
 */
#pragma once

#include <cstdint>
#include <memory>
#include <typeindex>
#include <unordered_map>

namespace vre::ecs
{

/// Opaque handle identifying an entity. Never recycled (see Registry).
using Entity = uint32_t;
/// Sentinel Entity value meaning "no entity" (e.g. an entity with no parent).
constexpr Entity kInvalidEntity = static_cast<Entity>(-1);

/**
 * @brief Type-erased base so a Registry can hold pools of unrelated
 * component types (Pool<Transform>, Pool<Mesh>, ...) in one homogeneous
 * map, keyed by std::type_index.
 *
 * Each Pool<T> owns exactly one unordered_map<Entity, T> — the actual
 * component storage for type T.
 */
class PoolBase
{
    public:
        virtual ~PoolBase() = default;
};

/// Concrete, typed component storage for component type `T`.
template <typename T>
class Pool : public PoolBase
{
    public:
        std::unordered_map<Entity, T> data; ///< Component instances, keyed by owning entity.
};

/**
 * @brief The registry itself: entity allocation plus generic
 * add/remove/get/query access to per-type component pools.
 *
 * Entities are never recycled (a monotonically increasing counter) —
 * simpler and sufficient for scenes that are loaded once and never
 * destroy individual entities at runtime, which is the only way this
 * engine currently uses it; recycling would only matter for a game that
 * spawns/despawns large numbers of entities continuously.
 */
class Registry
{
    public:
        /// @return A freshly allocated entity handle, unique for this registry's lifetime.
        Entity create() { return _next_entity++; }

        /**
         * @brief Adds (or replaces) a component of type `T` on `entity`.
         * @param entity Entity to attach the component to.
         * @param value Component value (defaults to `T{}`).
         * @return A reference to the stored component.
         */
        template <typename T>
        T &emplace(Entity entity, T value = T{})
        {
            auto &table = storage<T>();
            table[entity] = std::move(value);
            return table[entity];
        }

        /// Removes `entity`'s component of type `T`, if it has one.
        template <typename T>
        void remove(Entity entity)
        {
            auto it = _pools.find(std::type_index(typeid(T)));
            if (it != _pools.end())
                static_cast<Pool<T> *>(it->second.get())->data.erase(entity);
        }

        /// @return true if `entity` currently has a component of type `T`.
        template <typename T>
        bool has(Entity entity) const
        {
            auto it = _pools.find(std::type_index(typeid(T)));
            if (it == _pools.end())
                return false;
            const auto &table = static_cast<const Pool<T> *>(it->second.get())->data;
            return table.find(entity) != table.end();
        }

        /// @return A pointer to `entity`'s component of type `T`, or nullptr if it has none.
        template <typename T>
        T *try_get(Entity entity)
        {
            auto it = _pools.find(std::type_index(typeid(T)));
            if (it == _pools.end())
                return nullptr;
            auto &table = static_cast<Pool<T> *>(it->second.get())->data;
            auto entry = table.find(entity);
            return entry == table.end() ? nullptr : &entry->second;
        }

        /// @return A const pointer to `entity`'s component of type `T`, or nullptr if it has none.
        template <typename T>
        const T *try_get(Entity entity) const
        {
            return const_cast<Registry *>(this)->try_get<T>(entity);
        }

        /**
         * @brief Direct access to a whole component table, for systems that
         * need to iterate every entity carrying a given component (e.g.
         * "every entity with a Transform").
         *
         * Creates the (empty) table on first use for a component type
         * nothing has been added to yet, exactly like emplace() would.
         * @return A mutable reference to the entity->component map for type `T`.
         */
        template <typename T>
        std::unordered_map<Entity, T> &storage()
        {
            std::type_index key = std::type_index(typeid(T));
            auto it = _pools.find(key);
            if (it == _pools.end())
                it = _pools.emplace(key, std::make_unique<Pool<T>>()).first;
            return static_cast<Pool<T> *>(it->second.get())->data;
        }

        /// @return A const reference to the entity->component map for type `T`.
        template <typename T>
        const std::unordered_map<Entity, T> &storage() const
        {
            return const_cast<Registry *>(this)->storage<T>();
        }

    private:
        std::unordered_map<std::type_index, std::unique_ptr<PoolBase>> _pools; ///< One pool per component type.
        Entity _next_entity = 0; ///< Next handle create() will hand out.
};

} // namespace vre::ecs
