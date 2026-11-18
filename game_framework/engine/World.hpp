#pragma once

#include "Math.hpp"
#include <cstdint>
#include <map>
#include <optional>
#include <utility>

namespace gf
{
    using Entity = std::uint64_t;

    struct Transform
    {
        Vec2 position{};
        Vec2 previous{};
    };

    struct Sprite
    {
        Rect area;  // Local rectangle relative to Transform::position.
        Color color{};
        Texture texture{};
        UV uv{};
        int layer{};
        bool flipX{};
    };

    enum class BodyType { Static, Dynamic };

    struct Body
    {
        BodyType type;
        Rect area;
        Vec2 velocity{};
        float gravityScale{ 1 };
        bool grounded{};
    };

    struct Object
    {
        Transform transform;
        std::optional<Sprite> sprite;
        std::optional<Body> body;
    };

    class World
    {
    public:
        Entity create(Vec2 position, std::optional<Sprite> sprite = {}, std::optional<Body> body = {})
        {
            const Entity id = next_++;
            objects_.emplace(id, Object{ { position, position }, std::move(sprite), std::move(body) });
            return id;
        }

        void destroy(Entity id) { objects_.erase(id); }

        Object* find(Entity id)
        {
            const auto it = objects_.find(id);
            return it == objects_.end() ? nullptr : &it->second;
        }
        const Object* find(Entity id) const
        {
            const auto it = objects_.find(id);
            return it == objects_.end() ? nullptr : &it->second;
        }

        Object& at(Entity id) { return objects_.at(id); }
        const Object& at(Entity id) const { return objects_.at(id); }
        const std::map<Entity, Object>& objects() const { return objects_; }

        void beginStep()
        {
            for (auto& item : objects_)
                item.second.transform.previous = item.second.transform.position;
        }

    private:
        friend class Physics;
        std::map<Entity, Object> objects_;
        Entity next_{ 1 };
    };
}
