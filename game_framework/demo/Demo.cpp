#include "Demo.hpp"
#include <array>

namespace demo
{
    namespace
    {
        constexpr int Size = 16;
        constexpr int Width = Tiles * Size;
        using Pixels = std::array<std::string_view, Size>;
        constexpr Pixels scout{
            "................", ".....HHHHHH.....", "....HSSSSSSH....", "...HSSSSSSSSH...",
            "...HSMMMMMMMH...", "...HSMCCMMMMH...", "...HSMCCCCMMH...", "....HSSSSSSH....",
            ".....HFFFFH.....", "....HFFOOFFH....", "...HHFFEFFFH....", "...H.FFFFFF.H...",
            ".....HFFFFH.....", ".....HHHHHH.....", ".....SS..SS.....", "....HHH..HHH...."
        };
        constexpr Pixels scoutStep{
            "................", ".....HHHHHH.....", "....HSSSSSSH....", "...HSSSSSSSSH...",
            "...HSMMMMMMMH...", "...HSMCCMMMMH...", "...HSMCCCCMMH...", "....HSSSSSSH....",
            ".....HFFFFH.....", "....HFFOOFFH....", "...H.FFEFFFHH...", "..H..FFFFFF.....",
            ".....HHHHH......", "....SS...SS.....", "...HH.....SS....", "..........HHH..."
        };
        constexpr Pixels crystal{
            "................", ".......H........", "......HCH.......", ".....HCCCH......",
            ".....HCCCCH.....", "....HCCCSSCH....", "....HCCSSSSH....", "...HCCCSSSSSH...",
            "...HCCSSSMMMH...", "...HCSSSMMMMH...", "....HSSMMMMH....", ".....HSMMMH.....",
            "......HMHH......", ".......H........", "................", "................"
        };
        constexpr Pixels knight{
            "......HHHH......", ".....HGGGGH.....", "....HGOOGGGH....", "....HGHHHHGH....",
            "....HGMMCMGH....", "....HGGGGGGH....", ".....HGGGGH.....", "...HHHEEEHHHH...",
            "..HGGEFEFEEGGS..", "..HGGEEEEEGG.S..", "..HGGHEEEH.HGS..", "...H.HGGGH.HGS..",
            ".....HGGGH..H...", "....HGG.GGH.....", "....HGG.GGH.....", "...HHHH.HHHH...."
        };
        constexpr Pixels sentinel{
            "..G..........G..", "..GH........HG..", "...GH......HG...", "...HGGGGGGGGH...",
            "...HGGMHMGGGH...", "...HGFOHOFHGH...", "....HGGGGGGH....", "..HHHGGGGGGHHH..",
            ".HGGHHHHHHHHGGH.", ".HGGHFFOOFFHGGH.", ".HGGHFOOOOFHGGH.", "..HHHFHOOHFHHH..",
            "....HHGGGGHH....", "....HGG..GGH....", "...HGGH..HGGH...", "...HHHH..HHHH..."
        };
        constexpr std::array Patterns{ scout, scoutStep, crystal, knight, sentinel };
        static_assert([] {
            for (const auto& tile : Patterns)
                for (auto row : tile)
                    if (row.size() != Size) return false;
            return true;
        }());

        constexpr unsigned rgb(char ink, Tile tile)
        {
            switch (ink)
            {
            case 'H': return 0x10152C;
            case 'S': return 0x3CBEB1;
            case 'M': return 0x246074;
            case 'C': return 0xB2FFE6;
            case 'F': return tile == Sentinel ? 0x982D50 : 0xDD5C50;
            case 'O': return 0xFFB577;
            case 'E': return 0x913F57;
            case 'G': return tile == Sentinel ? 0xAA6478 : 0xCDD3E7;
            default: return 0;
            }
        }
    }

    Art::Art(gf::Renderer& renderer) : renderer_(renderer)
    {
        std::array<std::uint8_t, Width * Size * 4> rgba{};
        for (int tile = 0; tile < Tiles; ++tile)
        {
            for (int y = 0; y < Size; ++y)
            {
                for (int x = 0; x < Size; ++x)
                {
                    const auto color = rgb(Patterns[tile][y][x], static_cast<Tile>(tile));
                    if (!color) continue;
                    const auto index = (y * Width + tile * Size + x) * 4;
                    rgba[index] = static_cast<std::uint8_t>(color >> 16);
                    rgba[index + 1] = static_cast<std::uint8_t>(color >> 8);
                    rgba[index + 2] = static_cast<std::uint8_t>(color);
                    rgba[index + 3] = 255;
                }
            }
        }
        texture_ = renderer_.createTexture(Width, Size, rgba);
    }

    Art::~Art() { renderer_.destroyTexture(texture_); }

    gf::UV Art::uv(Tile tile) const
    {
        const auto index = static_cast<float>(tile);
        const auto count = static_cast<float>(Tiles);
        return { index / count, 0, (index + 1) / count, 1 };
    }

    std::unique_ptr<gf::Scene> makeScene(Mode mode, const Art* art)
    {
        return mode == Mode::Platform ? makePlatform(art) : makeCombat(art);
    }
}
