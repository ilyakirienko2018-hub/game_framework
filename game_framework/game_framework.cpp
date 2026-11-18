#include "demo/Demo.hpp"
#include "engine/Logger.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string_view>

namespace
{
    using namespace gf;
    using MaskFunction = decltype(&gf_aabb_masks);

    void expect(bool result, const char* message)
    {
        if (!result) throw std::runtime_error(message);
    }

    struct MaskData
    {
        std::array<BoundsBlock, 256> blocks{};
        std::array<Edges, 64> queries{};

        MaskData()
        {
            std::mt19937 generator(114);
            const auto position = [&generator]() { return static_cast<float>(generator() % 4096); };
            for (auto& block : blocks)
            {
                for (unsigned i = 0; i < 4; ++i)
                {
                    block.left[i] = position();
                    block.bottom[i] = position();
                    block.right[i] = block.left[i] + 20 + position() / 8;
                    block.top[i] = block.bottom[i] + 20 + position() / 8;
                }
            }
            for (auto& query : queries)
            {
                const float x = position(), y = position();
                query = { x, y, x + 490, y + 610 };
            }
        }
    };

    void checkMasks(const MaskData& data)
    {
        gf_aabb_masks(nullptr, 0, nullptr, nullptr);
        std::array<std::uint8_t, 256> expected{}, actual{};
        for (const auto& query : data.queries)
        {
            referenceAabbMasks(data.blocks.data(), data.blocks.size(), &query, expected.data());
            gf_aabb_masks(data.blocks.data(), data.blocks.size(), &query, actual.data());
            expect(actual == expected, "Selected AABB backend differs from the C++ reference.");
        }

        const BoundsBlock block{ {0, 11, 6, 6}, {0, 6, 11, 6}, {5, 20, 9, 7}, {5, 9, 20, 7} };
        const Edges query{5, 5, 10, 10};
        gf_aabb_masks(&block, 1, &query, actual.data());
        expect(actual[0] == 0b1001, "AABB mask handles edges or lanes incorrectly.");
    }

    void checkPhysics()
    {
        World world;
        Physics physics({0, 0});
        world.create({0, 0}, {}, Body{BodyType::Static, {0, 0, 10, 10}});
        world.create({25, 0}, {}, Body{BodyType::Static, {0, 0, 10, 10}});
        const auto falling = world.create({2, 30}, {}, Body{BodyType::Dynamic, {0, 0, 2, 2}, {0, -8000}});
        const auto rising = world.create({3, -30}, {}, Body{BodyType::Dynamic, {0, 0, 2, 2}, {0, 8000}});
        const auto moving = world.create({-15, 3}, {}, Body{BodyType::Dynamic, {0, 0, 2, 2}, {8000, 0}});
        const auto left = world.create({48, 3}, {}, Body{BodyType::Dynamic, {0, 0, 2, 2}, {-8000, 0}});
        physics.step(world, static_cast<float>(FixedStep));
        expect(world.at(falling).transform.position.y == 10 && world.at(falling).body->grounded
            && world.at(falling).body->velocity.y == 0, "High-speed floor collision failed.");
        expect(world.at(rising).transform.position.y == -2 && !world.at(rising).body->grounded,
            "Ceiling collision failed.");
        expect(world.at(moving).transform.position.x == -2 && world.at(left).transform.position.x == 35,
            "Wall collision failed or chose the wrong wall.");
        world.destroy(falling);
        expect(world.find(falling) == nullptr, "World retained a destroyed object.");
    }

    void checkInput()
    {
        Input input;
        const auto key = static_cast<unsigned>(Key::Space);
        input.keyEvent(key, true);
        input.keyEvent(key, false);
        expect(input.pressed(Key::Space) && !input.down(Key::Space), "Input lost a short press.");
        input.endStep();
        input.keyEvent(key, true);
        input.endStep();
        input.keyEvent(key, true);
        expect(!input.pressed(Key::Space) && input.down(Key::Space), "Key repeat produced another press.");
        input.loseFocus();
        expect(!input.down(Key::Space), "Input remained held after focus loss.");
        const auto view = Viewport::fit(1000, 800);
        expect(!view.mapMouse(500, 20) && view.mapMouse(500, 400).has_value(),
            "Letterbox coordinates are incorrect.");
    }

    double timeMasks(MaskFunction function, const MaskData& data, std::uint64_t& checksum)
    {
        std::array<std::uint8_t, 256> output{};
        const auto start = std::chrono::steady_clock::now();
        for (unsigned i = 0; i < 24000; ++i)
        {
            const auto& query = data.queries[i % data.queries.size()];
            function(data.blocks.data(), data.blocks.size(), &query, output.data());
            checksum += output[(i / data.queries.size()) % output.size()];
        }
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    }

    void benchmark()
    {
        const MaskData data;
        checkMasks(data);
        std::uint64_t cppSum = 0, asmSum = 0;
        std::array<double, 5> cpp{}, assembly{};
        for (std::size_t i = 0; i < cpp.size(); ++i)
        {
            cpp[i] = timeMasks(referenceAabbMasks, data, cppSum);
            assembly[i] = timeMasks(gf_aabb_masks, data, asmSum);
        }
        expect(cppSum == asmSum && cppSum != 0, "Benchmark checksums differ.");
        std::ranges::sort(cpp);
        std::ranges::sort(assembly);
        const auto middle = cpp.size() / 2;
        std::cout << "Median of 5 trials, 24000 queries x 1024 AABBs per trial\n"
            << "C++ reference: " << cpp[middle] << " ms\n";
        if (aabbUsesAssembly())
            std::cout << "NASM/SSE: " << assembly[middle]
                << " ms\nSpeedup: " << cpp[middle] / assembly[middle] << "x\n";
        else
            std::cout << "Active backend: C++ fallback (" << assembly[middle]
                << " ms). Install NASM and reconfigure to compare C++ against assembly.\n";
    }

    struct LoopCounts
    {
        std::size_t fixed{}, variable{}, rendered{};
        bool destroyed{};
    };

    class LoopScene final : public Scene
    {
    public:
        explicit LoopScene(LoopCounts& counts) : counts_(counts) {}
        ~LoopScene() override { counts_.destroyed = true; }

        void render(Renderer& renderer, float alpha, bool) const override
        {
            expect(alpha >= 0 && alpha <= 1, "Frame interpolation is out of range.");
            ++counts_.rendered;
            renderer.quad({32, 32, 128, 128}, Color::hex(0x6AE5C4));
        }

    private:
        void update(const Input&, float dt) override
        {
            expect(dt == static_cast<float>(FixedStep), "Fixed step changed during the loop.");
            ++counts_.fixed;
        }

        void variableUpdate(float dt) override
        {
            expect(dt >= 0, "Frame delta became negative.");
            ++counts_.variable;
        }

        LoopCounts& counts_;
    };

    void checkLoop(const Logger& logger)
    {
        constexpr std::size_t frames = 12;
        LoopCounts counts;
        {
            Application app("Game Framework loop check",
                {.visible = false, .vsync = false, .maxFramesPerSecond = 60, .logger = &logger});
            app.run(std::make_unique<LoopScene>(counts), {}, frames);
            const auto stats = app.frameStats();
            expect(stats.frames == frames && counts.rendered == frames && counts.variable == frames,
                "The loop did not produce all expected frame updates.");
            expect(stats.fixedUpdates > 0 && stats.fixedUpdates == counts.fixed
                && stats.fps > 0 && stats.frameTimeMs > 0,
                "Fixed updates or frame statistics are missing.");
            std::cout << "Loop check passed: " << stats.frames << " frames, " << stats.fixedUpdates
                << " fixed steps, " << stats.fps << " FPS, " << stats.frameTimeMs << " ms.\n";
        }
        expect(counts.destroyed, "The scene was not destroyed on shutdown.");

        Application restarted("Game Framework restart check", {.visible = false, .logger = &logger});
        expect(restarted.framebufferSize().first > 0, "A new context failed after shutdown.");
    }

    void checkRender(const Logger& logger)
    {
        Application app("Game Framework OpenGL verification",
            {.visible = false, .vsync = false, .logger = &logger, .preferOpenGl46 = false});
        auto& renderer = app.renderer();
        const auto [width, height] = app.framebufferSize();
        expect(width > 0 && height > 0, "The GLFW framebuffer has invalid dimensions.");
        renderer.begin(width, height);
        constexpr std::array<std::uint8_t, 8> colors{ 250, 10, 20, 255, 15, 20, 240, 255 };
        const auto texture = renderer.createTexture(1, 2, colors);
        renderer.quad({10, 10, 80, 80}, {}, texture);
        const auto upper = renderer.readPixel({50, 75});
        const auto lower = renderer.readPixel({50, 25});
        expect(upper[0] > 240 && upper[2] < 30 && lower[2] > 230 && lower[0] < 30,
            "The OpenGL texture or its orientation is incorrect.");
        renderer.destroyTexture(texture);

        const demo::Art art(renderer);
        const Input input;
        const auto platform = demo::makeScene(demo::Mode::Platform, &art);
        platform->tick(input);
        renderer.begin(width, height);
        platform->render(renderer, 1, true);
        const auto platformColor = renderer.readPixel({29, 506});
        expect(platformColor[1] > 245 && platformColor[0] > 145 && platformColor[0] < 175,
            "Platform HUD did not render.");
        renderer.present();

        const auto combat = demo::makeScene(demo::Mode::Combat, &art);
        combat->tick(input);
        renderer.begin(width, height);
        combat->render(renderer, 1, false);
        const auto combatColor = renderer.readPixel({29, 506});
        expect(combatColor[0] > 225 && combatColor[1] > 100 && combatColor[1] < 140,
            "Combat HUD did not render.");
        renderer.present();
        std::cout << "OpenGL checks passed with the 3.3 request. Version: " << renderer.version()
            << ". Renderer: " << renderer.driver() << '\n';
    }
}

int main(int argc, char** argv)
{
    const gf::Logger logger("sandbox.log");
    try
    {
        const std::string_view option = argc > 1 ? argv[1] : "";
        if (argc > 2)
            throw std::invalid_argument("Only one command-line option is supported.");
        if (option == "--self-test")
        {
            checkMasks(MaskData{});
            checkPhysics();
            checkInput();
            demo::checkPlatform();
            demo::checkCombat();
            std::cout << "PASS: AABB " << (aabbUsesAssembly() ? "NASM/SSE" : "C++ fallback")
                << ", physics, input, object lifecycle, platform and combat logic.\n";
            return 0;
        }
        if (option == "--benchmark")
        {
            benchmark();
            return 0;
        }
        if (option == "--smoke-test")
        {
            checkLoop(logger);
            return 0;
        }
        if (option == "--render-test")
        {
            checkRender(logger);
            return 0;
        }
        if (!option.empty() && option != "--vault")
        {
            std::cerr << "Usage: Sandbox [--vault | --self-test | --smoke-test | --render-test | --benchmark]\n";
            return 2;
        }

        Application app("Game Framework 2D", {.logger = &logger});
        logger.write(LogLevel::Info, std::string("OpenGL renderer: ") + app.renderer().driver());
        logger.write(LogLevel::Info, aabbUsesAssembly() ? "AABB backend: NASM/SSE" : "AABB backend: C++");
        const demo::Art art(app.renderer());
        auto mode = option == "--vault" ? demo::Mode::Combat : demo::Mode::Platform;
        app.run(demo::makeScene(mode, &art), [&](const Input& input) -> std::unique_ptr<Scene>
        {
            if (input.pressed(Key::F1)) mode = demo::Mode::Platform;
            else if (input.pressed(Key::F2)) mode = demo::Mode::Combat;
            else if (!input.pressed(Key::R)) return {};
            return demo::makeScene(mode, &art);
        });
        logger.write(LogLevel::Info, "Sandbox closed cleanly.");
        return 0;
    }
    catch (const std::exception& error)
    {
        logger.write(LogLevel::Error, error.what());
        return 1;
    }
}
