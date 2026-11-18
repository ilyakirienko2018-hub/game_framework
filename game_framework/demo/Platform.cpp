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
        constexpr Color Ink = Color::hex(0xE4F2E9), Mint = Color::hex(0xA0FFD8),
            Muted = Color::hex(0x8C9AAF), Coral = Color::hex(0xFF986F);
        constexpr Rect Gate{ 2244, 88, 70, 106 };
        constexpr std::array<Rect, 13> Terrain{
            Rect{0, 0, 574, 88}, {674, 0, 484, 88}, {1266, 0, 486, 88}, {1866, 0, 514, 88},
            {225, 132, 128, 15}, {390, 173, 112, 15}, {745, 130, 133, 15}, {900, 178, 132, 15},
            {1350, 131, 102, 15}, {1498, 164, 130, 15}, {1694, 127, 95, 15}, {1938, 135, 128, 15},
            {2106, 180, 112, 15}
        };
        constexpr std::array<Vec2, 5> Signals{
            Vec2{288, 170}, {448, 210}, {810, 172}, {1394, 168}, {2006, 173}
        };

        void skyline(Renderer& r, float camera, float rate, float bottom, Color color, int seed)
        {
            for (int i = -2; i < 13; ++i)
            {
                const float x = i * 130.f - std::fmod(camera * rate, 520.f);
                const auto pattern = ((i + 4) % 4 + seed) % 4;
                const float height = 43.f + static_cast<float>((pattern * 57 + seed * 29) % 117);
                r.quad({ x, bottom, 133, height }, color);
                r.quad({ x + 12, bottom + height, 106, 24 }, color);
                r.quad({ x + 34, bottom + height + 24, 71, 19 }, color);
                r.quad({ x + 57, bottom + height + 43, 37, 17 }, color);
            }
        }

        class Platform final : public Scene
        {
        public:
            explicit Platform(const Art* art) : art_(art)
            {
                for (const auto rect : Terrain)
                    world_.create({rect.x, rect.y}, {}, Body{BodyType::Static, {0, 0, rect.w, rect.h}});
                player_ = world_.create({82, 88}, Sprite{{-24, 0, 48, 48}, {},
                    art_ ? art_->texture() : 0, art_ ? art_->uv(Scout) : UV{}, 10},
                    Body{BodyType::Dynamic, {-12, 0, 24, 41}});
                for (std::size_t i = 0; i < signals_.size(); ++i)
                {
                    signals_[i] = world_.create(Signals[i],
                        Sprite{{-16, -16, 32, 32}, {}, art_ ? art_->texture() : 0,
                            art_ ? art_->uv(Crystal) : UV{}, 1});
                }
            }

            void update(const Input& input, float dt) override
            {
                clock_ += dt;
                if (complete_) return;
                auto& player = world_.at(player_);
                auto& body = *player.body;
                const int direction = (input.down(Key::D) || input.down(Key::Right))
                    - (input.down(Key::A) || input.down(Key::Left));
                const float target = direction * 276.f;
                const float acceleration = (body.grounded ? 2400.f : 1700.f) * dt;
                body.velocity.x += std::clamp(target - body.velocity.x, -acceleration, acceleration);
                if (direction)
                    player.sprite->flipX = direction < 0;
                if (art_)
                {
                    const bool footstep = body.grounded && std::abs(body.velocity.x) > 25
                        && static_cast<int>(clock_ * 12) % 2 == 1;
                    player.sprite->uv = art_->uv(footstep ? ScoutStep : Scout);
                }

                coyote_ = body.grounded ? 0.085f : std::max(0.f, coyote_ - dt);
                if (input.pressed(Key::Space) || input.pressed(Key::W) || input.pressed(Key::Up))
                    jumpBuffer_ = 0.12f;
                else
                    jumpBuffer_ = std::max(0.f, jumpBuffer_ - dt);
                if (jumpBuffer_ > 0 && coyote_ > 0)
                {
                    body.velocity.y = 518.f;
                    coyote_ = jumpBuffer_ = 0;
                }
            }

            void afterPhysics(float dt) override
            {
                auto& player = world_.at(player_);
                auto& p = player.transform.position;
                if (complete_)
                {
                    player.body->velocity.x = 0;
                    return;
                }
                p.x = std::clamp(p.x, 16.f, 2362.f);
                if (p.y < -70)
                {
                    player.transform = { checkpoint_, checkpoint_ };
                    player.body->velocity = {};
                    jumpBuffer_ = coyote_ = 0;
                }
                if (player.body->grounded && p.y == 88)
                {
                    if (p.x > 1884) checkpoint_.x = 1900;
                    else if (p.x > 1280) checkpoint_.x = std::max(checkpoint_.x, 1290.f);
                    else if (p.x > 695) checkpoint_.x = std::max(checkpoint_.x, 710.f);
                }
                for (std::size_t i = 0; i < signals_.size(); ++i)
                {
                    auto* signal = world_.find(signals_[i]);
                    if (!signal) continue;
                    signal->transform.position.y = Signals[i].y + std::sin(clock_ * 4 + static_cast<float>(i)) * 4;
                    if (player.body->area.offset(p).overlaps(signal->sprite->area.offset(signal->transform.position)))
                    {
                        world_.destroy(signals_[i]);
                        ++collected_;
                    }
                }
                if (collected_ == signals_.size() && player.body->area.offset(p).overlaps(Gate))
                {
                    complete_ = true;
                    player.body->velocity.x = 0;
                }
                previousCamera_ = camera_;
                const float target = std::clamp(p.x - 350.f, 0.f, 1420.f);
                camera_ += (target - camera_) * std::min(1.f, dt * 9);
            }

            void render(Renderer& r, float alpha, bool debug) const override
            {
                const float camera = previousCamera_ + (camera_ - previousCamera_) * alpha;
                r.gradient({0, 0, 960, 540}, Color::hex(0x3A425D), Color::hex(0x141B34));
                for (int i = 0; i < 62; ++i)
                {
                    const float x = std::fmod(1000.f + (i * 139 + i * i * 3) % 960 - camera * 0.12f, 960.f);
                    const float y = 185.f + static_cast<float>((i * 73 + i * i * 7) % 292);
                    const float size = i % 7 == 0 ? 3.f : 1.f;
                    r.quad({x, y, size, size}, Color::hex(0xD3F4EE, i % 7 == 0 ? 0.55f : 0.24f));
                }
                const float moonX = 751 - camera * 0.13f;
                for (int row = -14; row <= 14; ++row)
                {
                    const float half = std::floor(std::sqrt(225.f - row * row)) * 3;
                    r.quad({moonX - half, 346.f + row * 3.f, half * 2, 3},
                        Color::hex(row > 1 ? 0xC3CCC4 : 0xA3B4BA));
                }
                r.quad({moonX - 63, 335, 126, 3}, Color::hex(0x344760, 0.6f));
                r.quad({moonX - 79, 326, 136, 3}, Color::hex(0x344760, 0.8f));
                skyline(r, camera, 0.25f, 50, Color::hex(0x303F5F), 5);
                skyline(r, camera, 0.47f, 8, Color::hex(0x242F4F), 2);
                skyline(r, camera, 0.68f, -18, Color::hex(0x1C2645), 0);
                r.gradient({0, 0, 960, 80}, Color::hex(0x13223C), Color::hex(0x426674, 0.12f));
                r.quad({0, 16, 960, 2}, Color::hex(0x52828A, 0.28f));

                for (auto rect : Terrain)
                {
                    rect.x -= camera;
                    if (rect.right() <= 0 || rect.x >= CanvasWidth) continue;
                    r.gradient(rect, Color::hex(0x10182F), Color::hex(0x27364A));
                    r.quad({rect.x, rect.top() - 4, rect.w, 4}, Color::hex(0x4D9A89));
                    for (int x = 8; x + 10 < rect.w; x += 19)
                    {
                        const float px = rect.x + x;
                        const float width = static_cast<float>((x * 7) % 9 + 3);
                        r.quad({px, rect.top() - 3, width, 2}, Color::hex(0x9BCDA6));
                        r.quad({px + 3, rect.top() - 10, width, 2}, Color::hex(0x4D665F));
                        if (rect.h > 20)
                        {
                            const float y = 11.f + static_cast<float>((x * 17) % 45);
                            r.quad({px, y, width + 5, 2}, Color::hex(0x31405A));
                        }
                    }
                }

                const auto gate = Gate.offset({-camera, 0});
                const auto gateLight = collected_ == signals_.size() ? Mint : Coral;
                r.quad(gate, Color::hex(0x152135));
                r.outline(gate, 5, Color::hex(0x466477));
                r.outline({gate.x + 10, gate.y + 5, 50, 85}, 2, gateLight);
                r.quad({gate.x + 25, gate.y + 78, 20, 4}, gateLight);
                r.text({gate.x - 1, gate.top() + 21}, "RELAY 04", 1.5f, gateLight);

                for (std::size_t i = 0; i < signals_.size(); ++i)
                {
                    const auto* signal = world_.find(signals_[i]);
                    if (!signal) continue;
                    const Vec2 p = signal->transform.position - Vec2{camera, 0};
                    r.quad({p.x - 21, p.y - 21, 42, 42}, Color::hex(0x7BF1CD, 0.06f));
                    r.quad({p.x - 26, p.y + 8, 3, 3}, Mint);
                    r.quad({p.x + 19, p.y - 8, 2, 2}, Mint);
                }
                r.drawWorld(world_, {camera, 0}, alpha, debug);
                hud(r);
                if (complete_)
                {
                    r.quad({261, 198, 438, 138}, Color::hex(0x101A2F, 0.97f));
                    r.outline({261, 198, 438, 138}, 2, Mint);
                    r.text({291, 302}, "RELAY ONLINE", 4, Mint);
                    r.text({291, 250}, "F2  ENTER THE VAULT    R  REPLAY", 1.8f, Ink);
                }
            }

            unsigned collected() const { return collected_; }
            Entity player() const { return player_; }
            bool complete() const { return complete_; }

        private:
            void hud(Renderer& r) const
            {
                r.quad({0, 470, 960, 70}, Color::hex(0x121B30, 0.96f));
                r.quad({28, 488, 3, 33}, Mint);
                r.text({44, 522}, "SIGNAL BELOW", 3.5f, Ink);
                r.text({45, 489}, "REACH THE RELAY WITH ALL FIVE SIGNALS", 1.45f, Muted);
                r.text({704, 520}, "SIGNALS", 1.7f, Muted);
                for (unsigned i = 0; i < signals_.size(); ++i)
                {
                    const Rect rect{704 + i * 43.f, 485, 36, 14};
                    r.quad(rect, i < collected_ ? Mint : Color::hex(0x32465A));
                }
                r.quad({0, 0, 960, 34}, Color::hex(0x121B30, 0.96f));
                r.text({29, 23}, "A / D  MOVE    SPACE  JUMP", 1.6f, Ink);
                r.text({507, 23}, "F2  THE VAULT    F3  COLLIDERS    R  RESET", 1.6f, Muted);
            }

            const Art* art_;
            Entity player_{};
            std::array<Entity, Signals.size()> signals_{};
            Vec2 checkpoint_{82, 88};
            unsigned collected_{};
            bool complete_{};
            float clock_{}, camera_{}, previousCamera_{}, coyote_{}, jumpBuffer_{};
        };
    }

    std::unique_ptr<Scene> makePlatform(const Art* art) { return std::make_unique<Platform>(art); }

    void checkPlatform()
    {
        Platform scene(nullptr);
        Input input;
        scene.tick(input);
        if (!scene.objects().at(scene.player()).body->grounded)
            throw std::runtime_error("Platform player could not stand on the ground.");
        const auto& player = scene.objects().at(scene.player());
        input.keyEvent(static_cast<unsigned>(Key::D), true);
        constexpr std::array jumps{164.f, 317.f, 500.f, 703.f, 855.f, 1130.f, 1300.f, 1428.f,
            1655.f, 1780.f, 2038.f};
        for (const float jumpAt : jumps)
        {
            for (int i = 0; i < 240 && (player.transform.position.x < jumpAt || !player.body->grounded); ++i)
            {
                scene.tick(input);
                input.endStep();
            }
            if (player.transform.position.x < jumpAt || !player.body->grounded)
                throw std::runtime_error("Could not reach platform jump at " + std::to_string(jumpAt)
                    + ". Player position: " + std::to_string(player.transform.position.x) + ", "
                    + std::to_string(player.transform.position.y));
            const auto beforeJump = player.transform.position.y;
            input.keyEvent(static_cast<unsigned>(Key::Space), true);
            scene.tick(input);
            input.endStep();
            input.keyEvent(static_cast<unsigned>(Key::Space), false);
            if (player.transform.position.y <= beforeJump)
                throw std::runtime_error("Platform jump did not move the player at " + std::to_string(jumpAt));
        }
        for (int i = 0; i < 240 && !scene.complete(); ++i)
        {
            scene.tick(input);
            input.endStep();
        }
        if (!scene.complete())
        {
            std::string detail;
            for (const auto& [id, object] : scene.objects().objects())
                if (object.sprite && object.sprite->layer == 1)
                    detail += " " + std::to_string(object.transform.position.x);
            throw std::runtime_error("Could not unlock the relay. Signals collected: "
                + std::to_string(scene.collected()) + ". Remaining signals at x =" + detail
                + ". Player x = " + std::to_string(player.transform.position.x));
        }
    }
}
