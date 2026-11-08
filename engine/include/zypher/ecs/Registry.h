// Entity component system.
//
// Sparse set storage per component type with generation checked handles. The
// design is intentionally close to EnTT (which can be enabled as a drop-in
// backend later) but stays dependency free and deterministic: iteration follows
// component insertion order and never depends on pointer values.
#pragma once

#include "zypher/core/Assert.h"
#include "zypher/ecs/Entity.h"

#include <algorithm>
#include <typeindex>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace zy {

class Registry final : NonCopyable {
public:
    Registry() = default;
    ~Registry() = default;

    [[nodiscard]] Entity create() {
        u32 index = 0;
        if (!m_free.empty()) {
            index = m_free.back();
            m_free.pop_back();
        } else {
            index = static_cast<u32>(m_generations.size());
            m_generations.push_back(0);
            m_aliveFlags.push_back(false);
        }
        m_aliveFlags[index] = true;
        ++m_aliveCount;
        return Entity{index, m_generations[index]};
    }

    bool destroy(Entity entity) {
        if (!alive(entity)) {
            return false;
        }
        for (auto& storage : m_storages) {
            storage->remove(entity);
        }
        m_aliveFlags[entity.index] = false;
        ++m_generations[entity.index];
        m_free.push_back(entity.index);
        --m_aliveCount;
        return true;
    }

    [[nodiscard]] bool alive(Entity entity) const {
        return entity.index < m_generations.size() && m_aliveFlags[entity.index] &&
               m_generations[entity.index] == entity.generation;
    }

    [[nodiscard]] usize entityCount() const { return m_aliveCount; }

    void clear() {
        for (auto& storage : m_storages) {
            storage->clear();
        }
        m_generations.clear();
        m_aliveFlags.clear();
        m_free.clear();
        m_aliveCount = 0;
    }

    template <class T, class... Args>
    T& add(Entity entity, Args&&... args) {
        ZY_ASSERT(alive(entity), "add() called with a dead entity");
        Storage<T>& store = ensureStorage<T>();
        ZY_ASSERT(!store.contains(entity), "entity already has this component");
        store.insert(entity, T{std::forward<Args>(args)...});
        return *store.find(entity);
    }

    template <class T>
    void remove(Entity entity) {
        if (auto* store = storageIfExists<T>(); store != nullptr) {
            store->remove(entity);
        }
    }

    template <class T>
    [[nodiscard]] bool has(Entity entity) const {
        const auto* store = storageIfExists<T>();
        return store != nullptr && store->contains(entity);
    }

    template <class T>
    [[nodiscard]] T* tryGet(Entity entity) {
        auto* store = storageIfExists<T>();
        return store != nullptr ? store->find(entity) : nullptr;
    }

    template <class T>
    [[nodiscard]] const T* tryGet(Entity entity) const {
        const auto* store = storageIfExists<T>();
        return store != nullptr ? store->find(entity) : nullptr;
    }

    template <class T>
    [[nodiscard]] T& get(Entity entity) {
        T* component = tryGet<T>(entity);
        ZY_ASSERT(component != nullptr, "get() called for a missing component");
        return *component;
    }

    template <class T>
    [[nodiscard]] const T& get(Entity entity) const {
        const T* component = tryGet<T>(entity);
        ZY_ASSERT(component != nullptr, "get() called for a missing component");
        return *component;
    }

    // Calls fn(Entity, Ts&...) for every entity that owns all components.
    // The entity list is snapshotted so fn may add/remove components or spawn
    // entities safely. Prefer collecting structural changes when possible.
    template <class... Ts, class Fn>
    void each(Fn&& fn) {
        static_assert(sizeof...(Ts) > 0, "each() needs at least one component type");
        using First = std::tuple_element_t<0, std::tuple<Ts...>>;
        Storage<First>& first = ensureStorage<First>();
        std::vector<Entity> snapshot = first.entities();
        for (const Entity entity : snapshot) {
            if (!alive(entity)) {
                continue;
            }
            if (!(has<Ts>(entity) && ...)) {
                continue;
            }
            std::invoke(fn, entity, get<Ts>(entity)...);
        }
    }

    // Const variant: fn(const Entity&, const Ts&...).
    template <class... Ts, class Fn>
    void each(Fn&& fn) const {
        static_assert(sizeof...(Ts) > 0, "each() needs at least one component type");
        using First = std::tuple_element_t<0, std::tuple<Ts...>>;
        const auto* first = storageIfExists<First>();
        if (first == nullptr) {
            return;
        }
        for (const Entity entity : first->entities()) {
            if (!alive(entity)) {
                continue;
            }
            if (!(has<Ts>(entity) && ...)) {
                continue;
            }
            std::invoke(fn, entity, get<Ts>(entity)...);
        }
    }

    template <class... Ts>
    [[nodiscard]] usize count() const {
        static_assert(sizeof...(Ts) > 0, "count() needs at least one component type");
        using First = std::tuple_element_t<0, std::tuple<Ts...>>;
        const auto* first = storageIfExists<First>();
        if (first == nullptr) {
            return 0;
        }
        usize total = 0;
        for (const Entity entity : first->entities()) {
            if (alive(entity) && (has<Ts>(entity) && ...)) {
                ++total;
            }
        }
        return total;
    }

    template <class T>
    [[nodiscard]] const std::vector<Entity>& entitiesWith() {
        return ensureStorage<T>().entities();
    }

private:
    struct IStorage {
        virtual ~IStorage() = default;
        virtual void remove(Entity entity) = 0;
        virtual void clear() = 0;
        [[nodiscard]] virtual bool contains(Entity entity) const = 0;
        [[nodiscard]] virtual usize size() const = 0;
    };

    template <class T>
    class Storage final : public IStorage {
    public:
        void insert(Entity entity, T value) {
            if (contains(entity)) {
                *find(entity) = std::move(value);
                return;
            }
            m_slots[entity.index] = static_cast<u32>(m_entities.size());
            m_entities.push_back(entity);
            m_data.push_back(std::move(value));
        }

        void remove(Entity entity) override {
            const auto iterator = m_slots.find(entity.index);
            if (iterator == m_slots.end()) {
                return;
            }
            const u32 denseIndex = iterator->second;
            const u32 lastIndex = static_cast<u32>(m_entities.size()) - 1;
            if (denseIndex != lastIndex) {
                m_entities[denseIndex] = m_entities[lastIndex];
                m_data[denseIndex] = std::move(m_data[lastIndex]);
                m_slots[m_entities[denseIndex].index] = denseIndex;
            }
            m_entities.pop_back();
            m_data.pop_back();
            m_slots.erase(iterator);
        }

        void clear() override {
            m_entities.clear();
            m_data.clear();
            m_slots.clear();
        }

        [[nodiscard]] bool contains(Entity entity) const override {
            const auto iterator = m_slots.find(entity.index);
            return iterator != m_slots.end() && m_entities[iterator->second].generation == entity.generation;
        }

        [[nodiscard]] usize size() const override { return m_entities.size(); }

        [[nodiscard]] T* find(Entity entity) {
            const auto iterator = m_slots.find(entity.index);
            if (iterator == m_slots.end()) {
                return nullptr;
            }
            const u32 denseIndex = iterator->second;
            if (m_entities[denseIndex].generation != entity.generation) {
                return nullptr;
            }
            return &m_data[denseIndex];
        }

        [[nodiscard]] const T* find(Entity entity) const {
            const auto iterator = m_slots.find(entity.index);
            if (iterator == m_slots.end()) {
                return nullptr;
            }
            const u32 denseIndex = iterator->second;
            if (m_entities[denseIndex].generation != entity.generation) {
                return nullptr;
            }
            return &m_data[denseIndex];
        }

        [[nodiscard]] const std::vector<Entity>& entities() const { return m_entities; }

    private:
        std::vector<Entity> m_entities;
        std::vector<T> m_data;
        std::unordered_map<u32, u32> m_slots;
    };

    template <class T>
    Storage<T>& ensureStorage() {
        const std::type_index key(typeid(T));
        const auto iterator = m_storageIndices.find(key);
        if (iterator != m_storageIndices.end()) {
            return *static_cast<Storage<T>*>(m_storages[iterator->second].get());
        }
        const u32 index = static_cast<u32>(m_storages.size());
        m_storageIndices.emplace(key, index);
        m_storages.push_back(makeScope<Storage<T>>());
        return *static_cast<Storage<T>*>(m_storages.back().get());
    }

    template <class T>
    Storage<T>* storageIfExists() {
        const auto iterator = m_storageIndices.find(std::type_index(typeid(T)));
        if (iterator == m_storageIndices.end()) {
            return nullptr;
        }
        return static_cast<Storage<T>*>(m_storages[iterator->second].get());
    }

    template <class T>
    const Storage<T>* storageIfExists() const {
        const auto iterator = m_storageIndices.find(std::type_index(typeid(T)));
        if (iterator == m_storageIndices.end()) {
            return nullptr;
        }
        return static_cast<const Storage<T>*>(m_storages[iterator->second].get());
    }

    std::vector<u32> m_generations;
    std::vector<bool> m_aliveFlags;
    std::vector<u32> m_free;
    usize m_aliveCount = 0;
    std::unordered_map<std::type_index, u32> m_storageIndices;
    std::vector<Scope<IStorage>> m_storages;
};

} // namespace zy
