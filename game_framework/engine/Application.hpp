#pragma once

#include "Input.hpp"
#include "Physics.hpp"
#include "Renderer.hpp"
#include <cstddef>
#include <functional>
#include <string>
#include <utility>

namespace gf
{
    inline constexpr double FixedStep = 1.0 / 120.0;
    class Logger;

    struct WindowOptions
    {
        bool visible{true};
        bool vsync{true};
        int maxFramesPerSecond{120}; // Zero leaves the frame rate uncapped.
        const Logger* logger{};      // The logger must outlive the application.
        bool preferOpenGl46{true};
    };

    struct FrameStats
    {
        std::size_t frames{}, fixedUpdates{};
        double deltaSeconds{}, frameTimeMs{}, fps{};
    };

    class Scene
    {
    public:
        virtual ~Scene() = default;
        void tick(const Input& input);
        virtual void render(Renderer& renderer, float alpha, bool debug) const = 0;
        const World& objects() const { return world_; }

    protected:
        World world_;
        Physics physics_;
        virtual void update(const Input&, float) {}
        virtual void afterPhysics(float) {}
        virtual void variableUpdate(float) {}

    private:
        friend class Application;
    };

    class Application
    {
    public:
        using SceneSwitch = std::function<std::unique_ptr<Scene>(const Input&)>;

        explicit Application(std::string title, WindowOptions options = {});
        explicit Application(const wchar_t* title, WindowOptions options = {});
        ~Application();
        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        Renderer& renderer() { return *renderer_; }
        FrameStats frameStats() const { return stats_; }
        std::pair<int, int> framebufferSize() const;
        std::pair<int, int> contextVersion() const;
        void run(std::unique_ptr<Scene> scene, const SceneSwitch& changeScene = {}, std::size_t frameLimit = 0);

    private:
        struct Window;
        std::unique_ptr<Window> window_;
        std::unique_ptr<Renderer> renderer_;
        FrameStats stats_;
    };
}
