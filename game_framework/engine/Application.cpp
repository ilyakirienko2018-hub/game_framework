#include "Application.hpp"
#include "Logger.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <thread>

namespace gf
{
    namespace
    {
        std::string glfwError()
        {
            const char* detail = nullptr;
            return glfwGetError(&detail) != GLFW_NO_ERROR && detail ? detail : "no GLFW diagnostic";
        }

        class GlfwSession
        {
        public:
            GlfwSession()
            {
                if (active_)
                    throw std::logic_error("Only one Application may own GLFW at a time.");
                if (glfwInit() != GLFW_TRUE)
                    throw std::runtime_error("GLFW initialization failed: " + glfwError());
                active_ = true;
            }

            ~GlfwSession()
            {
                glfwTerminate();
                active_ = false;
            }

            GlfwSession(const GlfwSession&) = delete;
            GlfwSession& operator=(const GlfwSession&) = delete;

        private:
            // GLFW initialization, windowing and teardown run on the main thread.
            inline static bool active_{};
        };

        std::string utf8Title(std::wstring_view wide)
        {
            std::string result;
            for (std::size_t i = 0; i < wide.size(); ++i)
            {
                auto scalar = static_cast<std::uint32_t>(wide[i]);
                if constexpr (sizeof(wchar_t) == 2)
                {
                    if (scalar >= 0xD800 && scalar <= 0xDBFF && i + 1 < wide.size())
                    {
                        const auto low = static_cast<std::uint32_t>(wide[i + 1]);
                        if (low >= 0xDC00 && low <= 0xDFFF)
                        {
                            scalar = 0x10000 + ((scalar - 0xD800) << 10) + (low - 0xDC00);
                            ++i;
                        }
                    }
                }
                if (scalar > 0x10FFFF || (scalar >= 0xD800 && scalar <= 0xDFFF))
                    scalar = 0xFFFD;
                if (scalar < 0x80)
                {
                    result += static_cast<char>(scalar);
                    continue;
                }
                const unsigned tail = scalar < 0x800 ? 1 : scalar < 0x10000 ? 2 : 3;
                constexpr unsigned prefixes[]{0, 0xC0, 0xE0, 0xF0};
                result += static_cast<char>(prefixes[tail] | (scalar >> (tail * 6)));
                for (unsigned remaining = tail; remaining > 0; --remaining)
                    result += static_cast<char>(0x80 | ((scalar >> ((remaining - 1) * 6)) & 0x3F));
            }
            return result;
        }

        std::optional<unsigned> mapKey(int key)
        {
            if ((key >= GLFW_KEY_0 && key <= GLFW_KEY_9) || (key >= GLFW_KEY_A && key <= GLFW_KEY_Z))
                return static_cast<unsigned>(key);
            switch (key)
            {
            case GLFW_KEY_ENTER:  return static_cast<unsigned>(Key::Enter);
            case GLFW_KEY_ESCAPE: return static_cast<unsigned>(Key::Escape);
            case GLFW_KEY_SPACE:  return static_cast<unsigned>(Key::Space);
            case GLFW_KEY_LEFT:   return static_cast<unsigned>(Key::Left);
            case GLFW_KEY_RIGHT:  return static_cast<unsigned>(Key::Right);
            case GLFW_KEY_UP:     return static_cast<unsigned>(Key::Up);
            case GLFW_KEY_DOWN:   return static_cast<unsigned>(Key::Down);
            case GLFW_KEY_F1:     return static_cast<unsigned>(Key::F1);
            case GLFW_KEY_F2:     return static_cast<unsigned>(Key::F2);
            case GLFW_KEY_F3:     return static_cast<unsigned>(Key::F3);
            default:             return std::nullopt;
            }
        }
    }

    void Scene::tick(const Input& input)
    {
        constexpr auto dt = static_cast<float>(FixedStep);
        world_.beginStep();
        update(input, dt);
        physics_.step(world_, dt);
        afterPhysics(dt);
    }

    struct Application::Window
    {
        GlfwSession glfw;
        std::unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)> handle{nullptr, glfwDestroyWindow};
        std::string title;
        WindowOptions options;
        Input input;
        int windowedX{}, windowedY{}, windowedWidth{}, windowedHeight{};
        bool pauseRequested{}, fullscreenRequested{}, running{};

        Window(std::string name, WindowOptions config) : title(std::move(name)), options(config)
        {
            if (options.maxFramesPerSecond < 0)
                throw std::invalid_argument("Frame rate limit must be non-negative.");

            if (options.preferOpenGl46)
            {
                handle.reset(createContext(4, 6));
                if (!handle)
                {
                    const auto reason = glfwError();
                    if (options.logger)
                        options.logger->write(LogLevel::Warning, "OpenGL 4.6 unavailable: " + reason
                            + ". Trying OpenGL 3.3 Core.");
                }
            }
            if (!handle)
                handle.reset(createContext(3, 3));
            if (!handle)
                throw std::runtime_error("Could not create an OpenGL 3.3 Core context: " + glfwError());
            glfwMakeContextCurrent(handle.get());
            if (glfwGetCurrentContext() != handle.get())
                throw std::runtime_error("Could not activate the OpenGL context: " + glfwError());
            glfwSwapInterval(options.vsync ? 1 : 0);

            if (options.logger)
            {
                const auto [major, minor] = contextVersion();
                options.logger->write(LogLevel::Info,
                    "GLFW context ready: OpenGL " + std::to_string(major) + "." + std::to_string(minor)
                    + " Core, VSync request " + (options.vsync ? "on" : "off"));
            }

            glfwSetWindowUserPointer(handle.get(), this);
            glfwSetWindowFocusCallback(handle.get(), [](GLFWwindow* native, int focused)
            {
                auto& window = *static_cast<Window*>(glfwGetWindowUserPointer(native));
                if (!focused)
                {
                    window.input.loseFocus();
                    window.pauseRequested = false;
                    window.fullscreenRequested = false;
                }
            });
            glfwSetKeyCallback(handle.get(), [](GLFWwindow* native, int key, int, int action, int)
            {
                auto& window = *static_cast<Window*>(glfwGetWindowUserPointer(native));
                if (action == GLFW_REPEAT)
                    return;
                if (key == GLFW_KEY_P || key == GLFW_KEY_F11)
                {
                    if (action == GLFW_PRESS)
                    {
                        auto& request = key == GLFW_KEY_P ? window.pauseRequested : window.fullscreenRequested;
                        request = !request;
                    }
                    return;
                }
                if (const auto mapped = mapKey(key))
                    window.input.keyEvent(*mapped, action == GLFW_PRESS);
            });
            glfwSetMouseButtonCallback(handle.get(), [](GLFWwindow* native, int button, int action, int)
            {
                if (button != GLFW_MOUSE_BUTTON_LEFT && button != GLFW_MOUSE_BUTTON_RIGHT)
                    return;
                auto& window = *static_cast<Window*>(glfwGetWindowUserPointer(native));
                const auto key = button == GLFW_MOUSE_BUTTON_LEFT ? Key::MouseLeft : Key::MouseRight;
                const bool pressed = action == GLFW_PRESS;
                window.input.mouseButtonEvent(key, pressed, pressed ? window.cursor() : std::nullopt);
            });
        }

        ~Window()
        {
            if (handle)
                glfwSetWindowUserPointer(handle.get(), nullptr);
            handle.reset();
        }

        GLFWwindow* createContext(int major, int minor) const
        {
            glfwDefaultWindowHints();
            glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, major);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, minor);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
            glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
            return glfwCreateWindow(CanvasWidth, CanvasHeight, title.c_str(), nullptr, nullptr);
        }

        std::pair<int, int> contextVersion() const
        {
            return {glfwGetWindowAttrib(handle.get(), GLFW_CONTEXT_VERSION_MAJOR),
                glfwGetWindowAttrib(handle.get(), GLFW_CONTEXT_VERSION_MINOR)};
        }

        std::pair<int, int> framebufferSize() const
        {
            int width = 0, height = 0;
            glfwGetFramebufferSize(handle.get(), &width, &height);
            return {width, height};
        }

        std::optional<Vec2> cursor() const
        {
            int width = 0, height = 0;
            glfwGetWindowSize(handle.get(), &width, &height);
            double x = 0, y = 0;
            glfwGetCursorPos(handle.get(), &x, &y);
            if (width <= 0 || height <= 0 || x < 0 || y < 0 || x >= width || y >= height)
                return std::nullopt;
            const auto [pixelsWide, pixelsHigh] = framebufferSize();
            return Viewport::fit(pixelsWide, pixelsHigh).mapMouse(
                static_cast<int>(x * pixelsWide / width), static_cast<int>(y * pixelsHigh / height));
        }

        void toggleFullscreen()
        {
            auto* native = handle.get();
            if (glfwGetWindowMonitor(native))
            {
                glfwSetWindowMonitor(native, nullptr, windowedX, windowedY, windowedWidth,
                    windowedHeight, GLFW_DONT_CARE);
            }
            else
            {
                auto* monitor = glfwGetPrimaryMonitor();
                const auto* mode = monitor ? glfwGetVideoMode(monitor) : nullptr;
                if (!mode)
                {
                    if (options.logger)
                        options.logger->write(LogLevel::Warning, "Fullscreen requires an available monitor.");
                    return;
                }
                glfwGetWindowPos(native, &windowedX, &windowedY);
                glfwGetWindowSize(native, &windowedWidth, &windowedHeight);
                glfwSetWindowMonitor(native, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
            }
            glfwSwapInterval(options.vsync ? 1 : 0);
        }

        void showStats(const FrameStats& stats, bool paused) const
        {
            std::ostringstream caption;
            caption << title << " | " << std::fixed << std::setprecision(1)
                << stats.fps << " FPS | " << stats.frameTimeMs << " ms";
            if (paused)
                caption << " | Paused";
            glfwSetWindowTitle(handle.get(), caption.str().c_str());
        }
    };

    Application::Application(std::string title, WindowOptions options)
        : window_(std::make_unique<Window>(std::move(title), options)),
          renderer_(new Renderer(window_->handle.get()))
    {}

    Application::Application(const wchar_t* title, WindowOptions options)
        : Application(utf8Title(title ? std::wstring_view(title) : std::wstring_view{}), options)
    {}

    Application::~Application() = default;

    std::pair<int, int> Application::framebufferSize() const { return window_->framebufferSize(); }
    std::pair<int, int> Application::contextVersion() const { return window_->contextVersion(); }

    void Application::run(std::unique_ptr<Scene> scene, const SceneSwitch& changeScene, std::size_t frameLimit)
    {
        if (!scene)
            throw std::invalid_argument("Application requires an initial scene.");
        auto& window = *window_;
        if (window.running)
            throw std::logic_error("The application loop is already running.");

        struct ActiveRun
        {
            Window& window;
            ~ActiveRun()
            {
                if (window.options.visible)
                    glfwHideWindow(window.handle.get());
                window.input.loseFocus();
                window.running = false;
            }
        } activeRun{window};
        window.running = true;

        using Clock = std::chrono::steady_clock;
        using Seconds = std::chrono::duration<double>;
        auto previous = Clock::now();
        auto lastTitle = previous;
        double accumulator = 0;
        double averageFrame = 0;
        bool debug = false;
        bool paused = false;
        auto* native = window.handle.get();
        const auto frameRateLimit = window.options.maxFramesPerSecond;
        const auto targetFrame = Seconds(frameRateLimit > 0 ? 1.0 / frameRateLimit : 0.0);
        stats_ = {};
        if (window.options.visible)
            glfwShowWindow(native);

        while (!glfwWindowShouldClose(native))
        {
            const auto frameStart = Clock::now();
            glfwPollEvents();
            if (glfwWindowShouldClose(native) || window.input.pressed(Key::Escape))
                break;
            if (std::exchange(window.fullscreenRequested, false))
                window.toggleFullscreen();
            if (std::exchange(window.pauseRequested, false))
            {
                paused = !paused;
                accumulator = 0;
                scene->world_.beginStep();
            }

            const auto [width, height] = window.framebufferSize();
            if (glfwGetWindowAttrib(native, GLFW_ICONIFIED) || width <= 0 || height <= 0)
            {
                glfwWaitEvents();
                previous = Clock::now();
                accumulator = 0;
                continue;
            }

            const bool focused = glfwGetWindowAttrib(native, GLFW_FOCUSED) == GLFW_TRUE;
            window.input.mouseEvent(focused ? window.cursor() : std::nullopt);
            stats_.deltaSeconds = std::min(Seconds(frameStart - previous).count(), 0.125);
            previous = frameStart;
            if (paused)
                window.input.endStep();
            else
            {
                accumulator += stats_.deltaSeconds;
                while (accumulator >= FixedStep)
                {
                    if (window.input.pressed(Key::F3))
                        debug = !debug;
                    auto next = changeScene ? changeScene(window.input) : nullptr;
                    if (next)
                        scene = std::move(next);
                    else
                    {
                        scene->tick(window.input);
                        ++stats_.fixedUpdates;
                    }
                    window.input.endStep();
                    accumulator -= FixedStep;
                }
                scene->variableUpdate(static_cast<float>(stats_.deltaSeconds));
            }

            renderer_->begin(width, height);
            scene->render(*renderer_, paused ? 1.f : static_cast<float>(accumulator / FixedStep), debug);
            renderer_->present();
            if (frameRateLimit > 0)
                std::this_thread::sleep_until(frameStart + targetFrame);

            const auto frameEnd = Clock::now();
            const double frameSeconds = Seconds(frameEnd - frameStart).count();
            averageFrame = stats_.frames == 0 ? frameSeconds : 0.9 * averageFrame + 0.1 * frameSeconds;
            ++stats_.frames;
            stats_.frameTimeMs = frameSeconds * 1000;
            stats_.fps = averageFrame > 0 ? 1 / averageFrame : 0;

            if (window.options.visible && frameEnd - lastTitle >= std::chrono::milliseconds(250))
            {
                window.showStats(stats_, paused);
                lastTitle = frameEnd;
            }
            if (frameLimit != 0 && stats_.frames >= frameLimit)
                break;
        }
    }
}
