#include "Physics.hpp"
#include <algorithm>
#include <bit>
#include <cstddef>

namespace gf
{
    static_assert(sizeof(BoundsBlock) == 64 && alignof(BoundsBlock) == 16);
    static_assert(offsetof(BoundsBlock, bottom) == 16 && offsetof(BoundsBlock, right) == 32
        && offsetof(BoundsBlock, top) == 48);
    static_assert(sizeof(Edges) == 16 && offsetof(Edges, top) == 12);

#if defined(_MSC_VER)
    __declspec(noinline)
#elif defined(__GNUC__)
    __attribute__((noinline))
#endif
    void referenceAabbMasks(const BoundsBlock* blocks, std::size_t count,
        const Edges* query, std::uint8_t* masks)
    {
        for (std::size_t index = 0; index < count; ++index)
        {
            const auto& block = blocks[index];
            unsigned mask = 0;
            for (unsigned lane = 0; lane < 4; ++lane)
            {
                if (block.left[lane] <= query->right && block.right[lane] >= query->left
                    && block.bottom[lane] <= query->top && block.top[lane] >= query->bottom)
                    mask |= 1u << lane;
            }
            masks[index] = static_cast<std::uint8_t>(mask);
        }
    }

    bool aabbUsesAssembly() noexcept
    {
#ifdef GF_USE_NASM
        return true;
#else
        return false;
#endif
    }

#ifndef GF_USE_NASM
    extern "C" void gf_aabb_masks(const BoundsBlock* blocks, std::size_t count,
        const Edges* query, std::uint8_t* masks)
    {
        referenceAabbMasks(blocks, count, query, masks);
    }
#endif

    void Physics::step(World& world, float dt)
    {
        colliders_.clear();
        for (const auto& [id, object] : world.objects_)
        {
            if (object.body && object.body->type == BodyType::Static)
                colliders_.push_back(object.body->area.offset(object.transform.position));
        }

        blocks_.resize((colliders_.size() + 3) / 4);
        masks_.resize(blocks_.size());
        for (std::size_t index = 0; index < colliders_.size(); ++index)
        {
            auto& block = blocks_[index / 4];
            const auto lane = index % 4;
            const Rect rect = colliders_[index];
            block.left[lane] = rect.x;
            block.bottom[lane] = rect.y;
            block.right[lane] = rect.right();
            block.top[lane] = rect.top();
        }

        for (auto& [id, object] : world.objects_)
        {
            if (!object.body || object.body->type != BodyType::Dynamic)
                continue;

            auto& body = *object.body;
            body.velocity += gravity_ * (dt * body.gravityScale);
            body.grounded = false;
            Vec2 delta = body.velocity * dt;
            Rect moving = body.area.offset(object.transform.position);
            const Edges sweep{
                moving.x + std::min(delta.x, 0.f), moving.y + std::min(delta.y, 0.f),
                moving.right() + std::max(delta.x, 0.f), moving.top() + std::max(delta.y, 0.f)
            };
            gf_aabb_masks(blocks_.data(), blocks_.size(), &sweep, masks_.data());

            const auto visitCandidates = [&](auto check)
            {
                for (std::size_t block = 0; block < masks_.size(); ++block)
                {
                    for (unsigned mask = masks_[block]; mask != 0; mask &= mask - 1)
                    {
                        const auto index = block * 4 + std::countr_zero(mask);
                        if (index < colliders_.size())
                            check(colliders_[index]);
                    }
                }
            };

            visitCandidates([&](Rect fixed)
            {
                if (moving.y >= fixed.top() || moving.top() <= fixed.y)
                    return;
                if (delta.x > 0 && moving.right() <= fixed.x && moving.right() + delta.x >= fixed.x)
                {
                    delta.x = fixed.x - moving.right();
                    body.velocity.x = 0;
                }
                if (delta.x < 0 && moving.x >= fixed.right() && moving.x + delta.x <= fixed.right())
                {
                    delta.x = fixed.right() - moving.x;
                    body.velocity.x = 0;
                }
            });
            moving.x += delta.x;

            visitCandidates([&](Rect fixed)
            {
                if (moving.x >= fixed.right() || moving.right() <= fixed.x)
                    return;
                if (delta.y > 0 && moving.top() <= fixed.y && moving.top() + delta.y >= fixed.y)
                {
                    delta.y = fixed.y - moving.top();
                    body.velocity.y = 0;
                }
                if (delta.y < 0 && moving.y >= fixed.top() && moving.y + delta.y <= fixed.top())
                {
                    delta.y = fixed.top() - moving.y;
                    body.velocity.y = 0;
                    body.grounded = true;
                }
            });

            object.transform.position += delta;
        }
    }
}
