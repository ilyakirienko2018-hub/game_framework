#pragma once

namespace gf
{
    struct Vec2
    {
        float x{};
        float y{};

        Vec2 operator+(Vec2 other) const { return { x + other.x, y + other.y }; }
        Vec2 operator-(Vec2 other) const { return { x - other.x, y - other.y }; }
        Vec2 operator*(float scale) const { return { x * scale, y * scale }; }
        Vec2& operator+=(Vec2 other) { return *this = *this + other; }
    };

    struct Rect
    {
        float x{}, y{}, w{}, h{};

        float right() const { return x + w; }
        float top() const { return y + h; }
        bool contains(Vec2 point) const
        {
            return point.x >= x && point.x < right() && point.y >= y && point.y < top();
        }
        bool overlaps(Rect other) const
        {
            return x < other.right() && right() > other.x && y < other.top() && top() > other.y;
        }
        Rect offset(Vec2 delta) const { return { x + delta.x, y + delta.y, w, h }; }
    };

    struct Color
    {
        float r{ 1 }, g{ 1 }, b{ 1 }, a{ 1 };

        static constexpr Color hex(unsigned rgb, float alpha = 1)
        {
            return { ((rgb >> 16) & 255) / 255.f, ((rgb >> 8) & 255) / 255.f,
                (rgb & 255) / 255.f, alpha };
        }
    };

    using Texture = unsigned;

    struct UV
    {
        float left{}, top{}, right{ 1 }, bottom{ 1 };
    };
}
