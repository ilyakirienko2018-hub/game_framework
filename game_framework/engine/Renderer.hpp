#pragma once

#include "World.hpp"
#include <array>
#include <filesystem>
#include <memory>
#include <span>
#include <string_view>

struct GLFWwindow;

namespace gf
{
    class Application;

    inline constexpr int CanvasWidth = 960;
    inline constexpr int CanvasHeight = 540;

    struct Viewport
    {
        int x{}, y{}, width{}, height{};
        static Viewport fit(int width, int height);
        std::optional<Vec2> mapMouse(int x, int y) const;
    };

    class Renderer
    {
    public:
        ~Renderer();
        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        // Texture rows are supplied top to bottom in RGBA8 format.
        Texture createTexture(int width, int height, std::span<const std::uint8_t> rgba);
        Texture loadTexture(const std::filesystem::path& path);
        void destroyTexture(Texture texture);

        void begin(int width, int height);
        void present();
        void quad(Rect rect, Color color, Texture texture = {}, UV uv = {}, bool flipX = false);
        void gradient(Rect rect, Color bottom, Color top);
        void line(Vec2 from, Vec2 to, float thickness, Color color);
        void outline(Rect rect, float thickness, Color color);
        void text(Vec2 topLeft, std::string_view value, float scale, Color color);
        void drawWorld(const World& world, Vec2 camera, float alpha, bool debug = false);

        std::array<std::uint8_t, 4> readPixel(Vec2 point);
        const char* driver() const;
        const char* version() const;

    private:
        friend class Application;
        explicit Renderer(GLFWwindow* window);

        struct State;
        std::unique_ptr<State> state_;
    };
}
