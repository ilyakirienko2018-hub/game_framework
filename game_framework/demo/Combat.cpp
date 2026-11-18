#include "Demo.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>

namespace demo
{
    namespace
    {
        using namespace gf;
        constexpr Color Bone = Color::hex(0xF5DEC4), Dust = Color::hex(0xBCA1A9),
            Amber = Color::hex(0xFFC77E), Rose = Color::hex(0xEF7888), Mint = Color::hex(0x9DDEBA);
        constexpr std::array<int, 4> Pattern{ 2, 4, 3, 5 };
        constexpr std::array<Rect, 3> Buttons{
            Rect{34, 48, 280, 76}, {340, 48, 280, 76}, {646, 48, 280, 76}
        };
        constexpr std::array<Key, 3> ActionKeys{ Key::One, Key::Two, Key::Three };
        constexpr int HeroMax = 12, FoeMax = 15;
        enum Action { Strike, Guard, Repair };

        void health(Renderer& r, float x, float y, int value, int limit, Color color)
        {
            for (int i = 0; i < limit; ++i)
                r.quad({x + i * 15.f, y, 12, 7}, i < value ? color : Color::hex(0x584056));
        }

        class Combat final : public Scene
        {
        public:
            explicit Combat(const Art* art)
            {
                hero_ = world_.create({237, 204}, Sprite{{-66, 0, 132, 132}, {},
                    art ? art->texture() : 0, art ? art->uv(Knight) : UV{}, 1});
                foe_ = world_.create({716, 203}, Sprite{{-87, 0, 174, 174}, {},
                    art ? art->texture() : 0, art ? art->uv(Sentinel) : UV{}, 1});
            }

            void update(const Input& input, float dt) override
            {
                clock_ += dt;
                auto& hero = world_.at(hero_);
                auto& foe = world_.at(foe_);
                hover_ = input.mouse();
                const float recoil = std::max(0.f, 1.f - (clock_ - hitAt_) * 5);
                hero.transform.position.x = 237 - (hurtHero_ ? recoil * 22 : 0);
                foe.transform.position.x = 716 + (!hurtHero_ ? recoil * 26 : 0);
                hero.sprite->color.a = heroHp_ > 0 ? 1.f : 0.22f;
                foe.sprite->color.a = foeHp_ > 0 ? 1.f : 0.22f;

                if (heroHp_ <= 0 || foeHp_ <= 0)
                    return;
                if (pending_ > 0)
                {
                    pending_ -= dt;
                    if (pending_ <= 0)
                    {
                        const int damage = std::max(0, intent() - shield_);
                        heroHp_ = std::max(0, heroHp_ - damage);
                        shield_ = 0;
                        ++round_;
                        hitAt_ = clock_;
                        hurtHero_ = damage > 0;
                        if (!hurtHero_) hitAt_ = -10;
                        message_ = damage ? "THE WARDEN HITS FOR " + std::to_string(damage)
                            : "THE BLOW WAS BLOCKED";
                    }
                    return;
                }

                const auto click = input.leftClick();
                for (std::size_t i = 0; i < Buttons.size(); ++i)
                {
                    if (input.pressed(ActionKeys[i]) || (click && Buttons[i].contains(*click)))
                    {
                        act(static_cast<Action>(i));
                        return;
                    }
                }
            }

            void render(Renderer& r, float alpha, bool debug) const override
            {
                r.gradient({0, 0, 960, 540}, Color::hex(0x562C43), Color::hex(0x1B1728));
                for (int row = 0; row < 10; ++row)
                {
                    const float y = 178 + row * 31.f;
                    for (int column = -1; column < 14; ++column)
                    {
                        const float x = column * 79.f + (row % 2 ? 39.f : 0.f);
                        const unsigned shade = (row * 23 + (column + 1) * 17) % 11;
                        r.quad({x + 1, y + 1, 77, 29}, Color::hex(0x1A172A, shade * 0.021f + 0.12f));
                    }
                }
                for (int i = 0; i < 5; ++i)
                {
                    const float x = 28 + i * 222.f;
                    r.gradient({x, 180, 19, 291}, Color::hex(0x312037), Color::hex(0x4E3447));
                    r.quad({x - 9, 194, 38, 10}, Color::hex(0x704553));
                    r.quad({x - 7, 453, 34, 9}, Color::hex(0x694353));
                }
                for (int i : { 120, 841 })
                {
                    const float flicker = std::sin(clock_ * 13 + i);
                    r.quad({i - 8.f, 309, 17, 6}, Color::hex(0x1B1627));
                    r.quad({i - 2.f, 291, 5, 20}, Color::hex(0x271625));
                    r.quad({i - 22.f, 297, 44, 41}, Color::hex(0xFF8158, 0.07f));
                    r.quad({i - 4.f, 315, 8, 12 + flicker * 3}, Rose);
                    r.quad({i - 2.f, 316, 5, 7}, Amber);
                }
                r.gradient({0, 150, 960, 66}, Color::hex(0x21192B), Color::hex(0x4C3446));
                r.quad({0, 214, 960, 2}, Color::hex(0x865063));
                for (int x = 0; x < 960; x += 64)
                    r.line({x * 1.f, 215}, {480 + (x - 480) * 1.22f, 151}, 1, Color::hex(0x825269, 0.35f));
                r.quad({159, 196, 157, 11}, Color::hex(0x1F1728, 0.7f));
                r.quad({625, 195, 180, 11}, Color::hex(0x1F1728, 0.7f));
                r.drawWorld(world_, {}, alpha, debug);
                if (shield_)
                    r.outline({176, 211, 122, 123}, 2, Color::hex(0x9DDEBA, 0.75f));

                r.quad({0, 470, 960, 70}, Color::hex(0x201929));
                r.quad({28, 488, 3, 33}, Rose);
                r.text({44, 522}, "THE VAULT", 3.5f, Bone);
                r.text({45, 489}, "DEFEAT THE WARDEN", 1.5f, Dust);
                r.text({746, 513}, "TURN " + std::to_string(round_ + 1), 2.3f, Amber);

                r.text({150, 391}, "SCOUT", 2, Bone);
                r.text({608, 425}, "WARDEN", 2, Bone);
                health(r, 150, 370, heroHp_, HeroMax, Mint);
                health(r, 608, 404, foeHp_, FoeMax, Rose);
                if (heroHp_ > 0 && foeHp_ > 0)
                    r.text({608, 391}, "NEXT ATTACK: " + std::to_string(intent()), 1.5f, Amber);

                r.quad({0, 0, 960, 162}, Color::hex(0x201929));
                r.quad({0, 161, 960, 1}, Color::hex(0x805266));
                r.text({34, 146}, message_, 1.7f, pending_ > 0 ? Amber : Bone);
                actions(r);
                r.text({34, 23}, "F1  SIGNAL BELOW    R  RESTART", 1.6f, Dust);
                if (heroHp_ == 0 || foeHp_ == 0)
                {
                    const auto color = heroHp_ == 0 ? Rose : Mint;
                    r.quad({273, 236, 414, 112}, Color::hex(0x21192A, 0.98f));
                    r.outline({273, 236, 414, 112}, 2, color);
                    r.text({304, 318}, heroHp_ == 0 ? "SCOUT DOWN" : "VAULT SECURED", 3.7f, color);
                    r.text({304, 266}, "R  TRY AGAIN    F1  RETURN", 1.7f, Bone);
                }
            }

            int heroHp() const { return heroHp_; }
            int foeHp() const { return foeHp_; }
            unsigned round() const { return round_; }

        private:
            int intent() const { return Pattern[round_ % Pattern.size()]; }

            void act(Action action)
            {
                switch (action)
                {
                case Strike:
                    foeHp_ = std::max(0, foeHp_ - 3);
                    hitAt_ = clock_;
                    hurtHero_ = false;
                    message_ = foeHp_ > 0 ? "STRIKE LANDING: 3 DAMAGE" : "THE WARDEN FALLS";
                    break;
                case Guard:
                    shield_ = 5;
                    message_ = "GUARD RAISED";
                    break;
                case Repair:
                    if (repairs_ == 0 || heroHp_ == HeroMax) return;
                    --repairs_;
                    heroHp_ = std::min(HeroMax, heroHp_ + 4);
                    message_ = "SUIT REPAIRED";
                    break;
                }
                if (foeHp_ > 0) pending_ = 0.42f;
            }

            void actions(Renderer& r) const
            {
                const std::array<std::string, 3> help{
                    "DEAL 3 DAMAGE", "BLOCK UP TO 5 DAMAGE", "HEAL 4 / " + std::to_string(repairs_) + " LEFT"
                };
                constexpr std::array<std::string_view, 3> labels{ "STRIKE", "GUARD", "REPAIR" };
                constexpr std::array<Color, 3> colors{ Rose, Amber, Mint };
                for (std::size_t i = 0; i < Buttons.size(); ++i)
                {
                    const auto rect = Buttons[i];
                    const bool enabled = pending_ <= 0 && heroHp_ > 0 && foeHp_ > 0
                        && (i != Repair || (repairs_ > 0 && heroHp_ < HeroMax));
                    const bool hovered = enabled && hover_ && rect.contains(*hover_);
                    const auto accent = enabled ? colors[i] : Dust;
                    r.quad(rect, Color::hex(hovered ? 0x50354A : 0x302538));
                    r.outline(rect, 1, hovered ? accent : Color::hex(0x725065));
                    r.quad({rect.x, rect.top() - 3, rect.w, 3}, accent);
                    r.text({rect.x + 16, rect.top() - 16}, std::to_string(i + 1), 2.5f, accent);
                    r.text({rect.x + 45, rect.top() - 16}, labels[i], 2.5f, enabled ? Bone : Dust);
                    r.text({rect.x + 46, rect.top() - 46}, help[i], 1.6f, Dust);
                }
            }

            Entity hero_{}, foe_{};
            int heroHp_{HeroMax}, foeHp_{FoeMax}, shield_{}, repairs_{2};
            unsigned round_{};
            float clock_{}, pending_{}, hitAt_{-10};
            bool hurtHero_{};
            std::optional<Vec2> hover_;
            std::string message_{"CHOOSE AN ACTION"};
        };
    }

    std::unique_ptr<gf::Scene> makeCombat(const Art* art) { return std::make_unique<Combat>(art); }

    void checkCombat()
    {
        Combat scene(nullptr);
        Input input;
        auto action = [&](Key key)
        {
            input.keyEvent(static_cast<unsigned>(key), true);
            scene.tick(input);
            input.loseFocus();
            for (int i = 0; i < 60; ++i) scene.tick(input);
        };
        action(Key::Three);
        if (scene.round() != 0)
            throw std::runtime_error("Disabled repair consumed a turn.");
        action(Key::One);
        if (scene.heroHp() != 10 || scene.foeHp() != 12 || scene.round() != 1)
            throw std::runtime_error("Combat damage or turn order is incorrect.");
        action(Key::Two);
        if (scene.heroHp() != 10 || scene.round() != 2)
            throw std::runtime_error("Guard failed to block the attack.");
        for (int i = 0; i < 3; ++i)
        {
            action(Key::One);
            action(Key::Two);
        }
        action(Key::One);
        if (scene.foeHp() != 0 || scene.heroHp() <= 0 || scene.round() != 8)
            throw std::runtime_error("The combat victory path failed.");

        Combat mouseScene(nullptr);
        input.mouseButtonEvent(Key::MouseLeft, true, Vec2{100, 90});
        input.mouseButtonEvent(Key::MouseLeft, false);
        input.mouseEvent(Vec2{400, 90});
        mouseScene.tick(input);
        if (mouseScene.foeHp() != 12)
            throw std::runtime_error("Mouse action did not use the position of the click.");
    }
}
