#pragma once

#include "World.hpp"
#include <cstdint>
#include <vector>

namespace gf
{
    struct alignas(16) BoundsBlock
    {
        float left[4]{}, bottom[4]{}, right[4]{}, top[4]{};
    };

    struct Edges
    {
        float left, bottom, right, top;
    };

    // Each output byte contains four bits, one for each candidate in its block.
    extern "C" void gf_aabb_masks(const BoundsBlock* blocks, std::size_t count,
        const Edges* query, std::uint8_t* masks);
    void referenceAabbMasks(const BoundsBlock* blocks, std::size_t count,
        const Edges* query, std::uint8_t* masks);
    bool aabbUsesAssembly() noexcept;

    class Physics
    {
    public:
        explicit Physics(Vec2 gravity = { 0, -1700 }) : gravity_(gravity) {}
        void step(World& world, float dt);

    private:
        Vec2 gravity_;
        std::vector<Rect> colliders_;
        std::vector<BoundsBlock> blocks_;
        std::vector<std::uint8_t> masks_;
    };
}
