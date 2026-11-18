#include "Renderer.hpp"
#ifdef _WIN32
#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#endif
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace gf
{
    namespace
    {
        struct Vertex
        {
            Vec2 point;
            Vec2 uv;
            Color color;
        };

        struct Glyph
        {
            char character;
            unsigned rows[7];
        };

        constexpr Glyph Glyphs[] = {
            {'0',{14,17,19,21,25,17,14}}, {'1',{4,12,4,4,4,4,14}}, {'2',{14,17,1,2,4,8,31}},
            {'3',{30,1,1,14,1,1,30}}, {'4',{2,6,10,18,31,2,2}}, {'5',{31,16,16,30,1,1,30}},
            {'6',{14,16,16,30,17,17,14}}, {'7',{31,1,2,4,8,8,8}}, {'8',{14,17,17,14,17,17,14}},
            {'9',{14,17,17,15,1,1,14}}, {'A',{14,17,17,31,17,17,17}}, {'B',{30,17,17,30,17,17,30}},
            {'C',{14,17,16,16,16,17,14}}, {'D',{30,17,17,17,17,17,30}}, {'E',{31,16,16,30,16,16,31}},
            {'F',{31,16,16,30,16,16,16}}, {'G',{14,17,16,23,17,17,15}}, {'H',{17,17,17,31,17,17,17}},
            {'I',{14,4,4,4,4,4,14}}, {'J',{7,2,2,2,18,18,12}}, {'K',{17,18,20,24,20,18,17}},
            {'L',{16,16,16,16,16,16,31}}, {'M',{17,27,21,21,17,17,17}}, {'N',{17,25,21,19,17,17,17}},
            {'O',{14,17,17,17,17,17,14}}, {'P',{30,17,17,30,16,16,16}}, {'Q',{14,17,17,17,21,18,13}},
            {'R',{30,17,17,30,20,18,17}}, {'S',{15,16,16,14,1,1,30}}, {'T',{31,4,4,4,4,4,4}},
            {'U',{17,17,17,17,17,17,14}}, {'V',{17,17,17,17,17,10,4}}, {'W',{17,17,17,21,21,21,10}},
            {'X',{17,17,10,4,10,17,17}}, {'Y',{17,17,10,4,4,4,4}}, {'Z',{31,1,2,4,8,16,31}},
            {'/',{1,1,2,4,8,16,16}}, {'-',{0,0,0,31,0,0,0}}, {'+',{0,4,4,31,4,4,0}},
            {'.',{0,0,0,0,0,12,12}}, {':',{0,12,12,0,12,12,0}}, {'!',{4,4,4,4,4,0,4}},
            {'?',{14,17,1,2,4,0,4}}, {'[',{14,8,8,8,8,8,14}}, {']',{14,2,2,2,2,2,14}}
        };
        constexpr int FontWidth = 96, FontHeight = 48;

        const char* VertexSource = R"(#version 330 core
layout(location=0) in vec2 point;
layout(location=1) in vec2 uv;
layout(location=2) in vec4 tint;
out vec2 texCoord;
out vec4 color;
void main() {
    gl_Position = vec4(point, 0.0, 1.0);
    texCoord = uv;
    color = tint;
})";
        const char* FragmentSource = R"(#version 330 core
in vec2 texCoord;
in vec4 color;
uniform sampler2D atlas;
out vec4 pixel;
void main() {
    pixel = color * texture(atlas, texCoord);
})";
    }

    struct Renderer::State
    {
        GLFWwindow* window{};
        GLuint program{}, vao{}, vbo{}, white{}, font{};
        GLuint activeTexture{};
        Viewport viewport;
        std::string driver, version;
        std::vector<Vertex> vertices;
        std::vector<const Object*> renderList;

        ~State()
        {
            if (!window || glfwGetCurrentContext() != window)
                return;
            if (font) glDeleteTextures(1, &font);
            if (white) glDeleteTextures(1, &white);
            if (vbo) glDeleteBuffers(1, &vbo);
            if (vao) glDeleteVertexArrays(1, &vao);
            if (program)
            {
                glUseProgram(0);
                glDeleteProgram(program);
            }
        }

        void initialize(GLFWwindow* handle)
        {
            if (!handle || glfwGetCurrentContext() != handle)
                throw std::runtime_error("A current GLFW context is required to initialize the renderer.");
            if (!gladLoadGL(glfwGetProcAddress) || !GLAD_GL_VERSION_3_3)
                throw std::runtime_error("GLAD could not load the required OpenGL 3.3 Core functions.");
            window = handle;
            const auto* renderer = glGetString(GL_RENDERER);
            driver = renderer ? reinterpret_cast<const char*>(renderer) : "Unknown renderer";
            const auto* contextVersion = glGetString(GL_VERSION);
            version = contextVersion ? reinterpret_cast<const char*>(contextVersion) : "Unknown version";

            program = glCreateProgram();
            if (!program)
                throw std::runtime_error("OpenGL could not create a shader program.");
            for (const auto& [type, source] : { std::pair{GL_VERTEX_SHADER, VertexSource},
                std::pair{GL_FRAGMENT_SHADER, FragmentSource} })
            {
                const auto shader = glCreateShader(type);
                if (!shader)
                    throw std::runtime_error("OpenGL could not create a shader.");
                glShaderSource(shader, 1, &source, nullptr);
                glCompileShader(shader);
                GLint compiled = 0;
                glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
                if (!compiled)
                {
                    GLint length = 0;
                    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
                    std::string log(std::max(1, length), '\0');
                    glGetShaderInfoLog(shader, length, nullptr, log.data());
                    glDeleteShader(shader);
                    throw std::runtime_error("GLSL compilation failed: " + log);
                }
                glAttachShader(program, shader);
                glDeleteShader(shader);
            }

            glLinkProgram(program);
            GLint linked = 0;
            glGetProgramiv(program, GL_LINK_STATUS, &linked);
            if (!linked)
            {
                GLint length = 0;
                glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
                std::string log(std::max(1, length), '\0');
                glGetProgramInfoLog(program, length, nullptr, log.data());
                throw std::runtime_error("GLSL linking failed: " + log);
            }
            glUseProgram(program); // The only sampler uses texture unit 0.

            glGenVertexArrays(1, &vao);
            glBindVertexArray(vao);
            glGenBuffers(1, &vbo);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                reinterpret_cast<const void*>(offsetof(Vertex, uv)));
            glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                reinterpret_cast<const void*>(offsetof(Vertex, color)));
            for (GLuint attribute = 0; attribute < 3; ++attribute)
                glEnableVertexAttribArray(attribute);

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            if (glGetError() != GL_NO_ERROR)
                throw std::runtime_error("OpenGL could not initialize the 2D renderer state.");
            vertices.reserve(12288);
        }

        void flush()
        {
            if (vertices.empty())
                return;
            glBindTexture(GL_TEXTURE_2D, activeTexture);
            const auto size = static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex));
            glBufferData(GL_ARRAY_BUFFER, size, vertices.data(), GL_STREAM_DRAW);
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
            vertices.clear();
        }

        void emit(const std::array<Vec2, 4>& points, Color bottom, Color top, GLuint texture, UV uv)
        {
            if (texture != activeTexture || vertices.size() + 6 > vertices.capacity())
            {
                flush();
                activeTexture = texture;
            }
            constexpr unsigned order[] = { 0, 1, 2, 0, 2, 3 };
            const Vec2 coords[] = { {uv.left, uv.bottom}, {uv.right, uv.bottom},
                {uv.right, uv.top}, {uv.left, uv.top} };
            for (unsigned i : order)
            {
                const auto p = points[i];
                vertices.push_back({ {p.x * (2.f / CanvasWidth) - 1.f,
                    p.y * (2.f / CanvasHeight) - 1.f}, coords[i], i < 2 ? bottom : top });
            }
        }
    };

    Viewport Viewport::fit(int width, int height)
    {
        if (width <= 0 || height <= 0) return {};
        const auto scale = std::min(static_cast<double>(width) / CanvasWidth,
            static_cast<double>(height) / CanvasHeight);
        const auto w = std::max(1, static_cast<int>(std::lround(CanvasWidth * scale)));
        const auto h = std::max(1, static_cast<int>(std::lround(CanvasHeight * scale)));
        return { (width - w) / 2, (height - h) / 2, w, h };
    }

    std::optional<Vec2> Viewport::mapMouse(int px, int py) const
    {
        if (px < x || px >= x + width || py < y || py >= y + height)
            return {};
        return Vec2{ (px - x + 0.5f) * CanvasWidth / width,
            CanvasHeight - (py - y + 0.5f) * CanvasHeight / height };
    }

    Renderer::Renderer(GLFWwindow* window) : state_(std::make_unique<State>())
    {
        state_->initialize(window);
        constexpr std::array<std::uint8_t, 4> white{ 255, 255, 255, 255 };
        state_->white = createTexture(1, 1, white);
        std::array<std::uint8_t, FontWidth * FontHeight * 4> pixels{};
        for (const auto& glyph : Glyphs)
        {
            const int index = glyph.character - 32;
            for (int row = 0; row < 7; ++row)
            {
                for (int column = 0; column < 5; ++column)
                {
                    if ((glyph.rows[row] & (16u >> column)) == 0)
                        continue;
                    const int x = (index % 16) * 6 + column;
                    const int y = (index / 16) * 8 + row;
                    std::fill_n(pixels.data() + (y * FontWidth + x) * 4, 4, std::uint8_t{255});
                }
            }
        }
        state_->font = createTexture(FontWidth, FontHeight, pixels);
    }

    Renderer::~Renderer() = default;

    Texture Renderer::createTexture(int width, int height, std::span<const std::uint8_t> rgba)
    {
        GLint maximum = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximum);
        if (width <= 0 || height <= 0 || width > maximum || height > maximum
            || rgba.size() != static_cast<std::size_t>(width) * height * 4)
            throw std::invalid_argument("Invalid RGBA texture dimensions or data size.");
        GLuint texture = 0;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        if (glGetError() != GL_NO_ERROR)
        {
            glDeleteTextures(1, &texture);
            throw std::runtime_error("OpenGL texture upload failed.");
        }
        return texture;
    }

    Texture Renderer::loadTexture(const std::filesystem::path& path)
    {
#ifdef _WIN32
        using Microsoft::WRL::ComPtr;
        const HRESULT apartment = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(apartment) && apartment != RPC_E_CHANGED_MODE)
            throw std::runtime_error("COM initialization failed while loading a texture.");
        struct ComScope
        {
            bool owns;
            ~ComScope() { if (owns) CoUninitialize(); }
        } scope{ SUCCEEDED(apartment) };
        const auto check = [](HRESULT result)
        {
            if (FAILED(result)) throw std::runtime_error("WIC could not decode the texture.");
        };
        ComPtr<IWICImagingFactory> factory;
        check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&factory)));
        ComPtr<IWICBitmapDecoder> decoder;
        check(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
            WICDecodeMetadataCacheOnDemand, &decoder));
        ComPtr<IWICBitmapFrameDecode> frame;
        check(decoder->GetFrame(0, &frame));
        ComPtr<IWICFormatConverter> converter;
        check(factory->CreateFormatConverter(&converter));
        check(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom));
        UINT width = 0, height = 0;
        check(converter->GetSize(&width, &height));
        GLint maximum = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximum);
        const auto size = static_cast<std::uint64_t>(width) * height * 4;
        if (!width || !height || width > static_cast<UINT>(maximum) || height > static_cast<UINT>(maximum)
            || size > std::numeric_limits<UINT>::max())
            throw std::runtime_error("Decoded texture dimensions exceed the limits of the renderer.");
        std::vector<std::uint8_t> rgba(static_cast<std::size_t>(size));
        check(converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(size), rgba.data()));
        return createTexture(static_cast<int>(width), static_cast<int>(height), rgba);
#else
        (void)path;
        throw std::runtime_error("File-based texture loading currently requires Windows Imaging Component.");
#endif
    }

    void Renderer::destroyTexture(Texture texture)
    {
        if (texture)
        {
            state_->flush();
            glDeleteTextures(1, &texture);
        }
    }

    void Renderer::begin(int width, int height)
    {
        auto& s = *state_;
        s.viewport = Viewport::fit(width, height);
        const auto& view = s.viewport;
        glDisable(GL_SCISSOR_TEST);
        glClearColor(0.013f, 0.018f, 0.032f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        glViewport(view.x, height - view.y - view.height, view.width, view.height);
    }

    void Renderer::present()
    {
        state_->flush();
        glfwSwapBuffers(state_->window);
    }

    void Renderer::quad(Rect r, Color color, Texture texture, UV uv, bool flipX)
    {
        if (flipX) std::swap(uv.left, uv.right);
        state_->emit({ Vec2{r.x,r.y}, Vec2{r.right(),r.y}, Vec2{r.right(),r.top()}, Vec2{r.x,r.top()} },
            color, color, texture ? texture : state_->white, uv);
    }

    void Renderer::gradient(Rect r, Color bottom, Color top)
    {
        state_->emit({ Vec2{r.x,r.y}, Vec2{r.right(),r.y}, Vec2{r.right(),r.top()}, Vec2{r.x,r.top()} },
            bottom, top, state_->white, {});
    }

    void Renderer::line(Vec2 from, Vec2 to, float thickness, Color color)
    {
        const auto delta = to - from;
        const float length = std::hypot(delta.x, delta.y);
        if (length == 0) return;
        const Vec2 normal = Vec2{-delta.y, delta.x} * (thickness * 0.5f / length);
        state_->emit({ from - normal, to - normal, to + normal, from + normal },
            color, color, state_->white, {});
    }

    void Renderer::outline(Rect r, float thickness, Color color)
    {
        quad({r.x, r.y, r.w, thickness}, color);
        quad({r.x, r.top() - thickness, r.w, thickness}, color);
        quad({r.x, r.y + thickness, thickness, r.h - thickness * 2}, color);
        quad({r.right() - thickness, r.y + thickness, thickness, r.h - thickness * 2}, color);
    }

    void Renderer::text(Vec2 at, std::string_view value, float scale, Color color)
    {
        const float origin = at.x;
        for (unsigned char c : value)
        {
            if (c == '\n') { at.x = origin; at.y -= 9 * scale; continue; }
            if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
            if (c != ' ')
            {
                const int index = c >= 32 && c < 128 ? c - 32 : '?' - 32;
                const float x = static_cast<float>((index % 16) * 6);
                const float y = static_cast<float>((index / 16) * 8);
                quad({at.x, at.y - 7 * scale, 5 * scale, 7 * scale}, color, state_->font,
                    {x / FontWidth, y / FontHeight, (x + 5) / FontWidth, (y + 7) / FontHeight});
            }
            at.x += 6 * scale;
        }
    }

    void Renderer::drawWorld(const World& world, Vec2 camera, float alpha, bool debug)
    {
        auto& list = state_->renderList;
        list.clear();
        for (const auto& [id, object] : world.objects())
            if (object.sprite) list.push_back(&object);
        std::stable_sort(list.begin(), list.end(), [](const Object* lhs, const Object* rhs)
            { return lhs->sprite->layer < rhs->sprite->layer; });
        for (const auto* object : list)
        {
            const auto& transform = object->transform;
            const auto position = transform.previous + (transform.position - transform.previous) * alpha;
            const auto& sprite = *object->sprite;
            quad(sprite.area.offset(position - camera), sprite.color, sprite.texture, sprite.uv, sprite.flipX);
        }
        if (debug)
        {
            for (const auto& [id, object] : world.objects())
            {
                if (object.body)
                    outline(object.body->area.offset(object.transform.position - camera), 1,
                        object.body->type == BodyType::Static ? Color::hex(0x5CF7B0) : Color::hex(0xFF7078));
            }
        }
    }

    std::array<std::uint8_t, 4> Renderer::readPixel(Vec2 point)
    {
        state_->flush();
        GLint viewport[4]{};
        glGetIntegerv(GL_VIEWPORT, viewport);
        std::array<std::uint8_t, 4> pixel{};
        glReadPixels(viewport[0] + static_cast<int>(point.x * viewport[2] / CanvasWidth),
            viewport[1] + static_cast<int>(point.y * viewport[3] / CanvasHeight),
            1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        return pixel;
    }

    const char* Renderer::driver() const { return state_->driver.c_str(); }
    const char* Renderer::version() const { return state_->version.c_str(); }
}
