#pragma once

#include "../engine/Application.hpp"

namespace demo
{
    enum class Mode { Platform, Combat };
    enum Tile { Scout, ScoutStep, Crystal, Knight, Sentinel, Tiles };

    class Art
    {
    public:
        explicit Art(gf::Renderer& renderer);
        ~Art();
        Art(const Art&) = delete;
        Art& operator=(const Art&) = delete;

        gf::UV uv(Tile tile) const;
        gf::Texture texture() const { return texture_; }

    private:
        gf::Renderer& renderer_;
        gf::Texture texture_{};
    };

    std::unique_ptr<gf::Scene> makeScene(Mode mode, const Art* art);
    std::unique_ptr<gf::Scene> makePlatform(const Art* art);
    std::unique_ptr<gf::Scene> makeCombat(const Art* art);
    void checkPlatform();
    void checkCombat();
}
