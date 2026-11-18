#pragma once

#include "Math.hpp"
#include <array>
#include <optional>

namespace gf
{
    enum class Key : unsigned
    {
        MouseLeft = 1, MouseRight = 2, Enter = 13, Escape = 27, Space = 32,
        Left = 37, Up, Right, Down, A = 'A', D = 'D', R = 'R', S = 'S',
        W = 'W', One = '1', Two = '2', Three = '3', F1 = 112, F2, F3
    };

    class Input
    {
    public:
        bool down(Key key) const { return held_[static_cast<unsigned>(key)]; }
        bool pressed(Key key) const { return pressed_[static_cast<unsigned>(key)]; }
        std::optional<Vec2> mouse() const { return mouse_; }
        std::optional<Vec2> leftClick() const { return leftClick_; }

        void keyEvent(unsigned key, bool down)
        {
            if (key >= held_.size())
                return;
            pressed_[key] = pressed_[key] || (down && !held_[key]);
            held_[key] = down;
        }

        void mouseEvent(std::optional<Vec2> point) { mouse_ = point; }
        void mouseButtonEvent(Key key, bool down, std::optional<Vec2> point = {})
        {
            if (key == Key::MouseLeft && down && !this->down(key))
                leftClick_ = point;
            keyEvent(static_cast<unsigned>(key), down);
        }
        void endStep() { pressed_.fill(false); leftClick_.reset(); }
        void loseFocus() { held_.fill(false); endStep(); mouse_.reset(); }

    private:
        std::array<bool, 256> held_{};
        std::array<bool, 256> pressed_{};
        std::optional<Vec2> mouse_;
        std::optional<Vec2> leftClick_;
    };
}
