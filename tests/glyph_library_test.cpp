#include <array>
#include <cstdio>
#include <string>

#include "core/types.h"
#include "core/words.h"
#include "platform/glyph_rasterizer.h"
#include "platform/surface.h"
#include "test.h"
#include "text/big5.h"

using swd3e::u8;

namespace
{

constexpr std::array<int, 6> kSizes = {11, 12, 13, 15, 16, 20};  // every size of font.bin
constexpr int kMissingSize = 14;
constexpr swd3e::u16 kEtenLetter = 0xB1C7;  // Big5 C7 B1: an ETen extension letter drawn from the outline
constexpr int kSurfaceSide = 64;
constexpr int kSize = 16;
constexpr swd3e::u32 kWhiteRef = 0xFFFFFF;

}  // namespace

TEST_CASE("glyph library: MingLiU cells of the sizes the game draws")
{
    swd3e::GlyphLibrary library;

    if (!library.Load(swd3e::GlyphLibraryPath()))
    {
        std::printf("  (asset/font.bin not found, skipped)\n");
        return;
    }

    const std::string one = swd3e::Big5(u8"一");
    const swd3e::u16 oneCode = swd3e::MakeBytes(static_cast<u8>(one[1]), static_cast<u8>(one[0]));  // lead byte low

    for (const int size : kSizes)
    {
        const u8* letter = library.Find(size, 'A');
        const u8* han = library.Find(size, oneCode);
        const u8* cyrillic = library.Find(size, kEtenLetter);
        CHECK(letter != nullptr);
        CHECK(han != nullptr);
        CHECK(cyrillic != nullptr);
    }

    CHECK(library.Find(kMissingSize, 'A') == nullptr);
    CHECK(library.Find(kSize, 1) == nullptr);
    swd3e::LibraryGlyphRasterizer rasterizer(library);
    swd3e::Surface surface(kSurfaceSide, kSurfaceSide);
    surface.Fill(0);
    rasterizer.RenderText(surface, "", kSize, one, kWhiteRef);
    int set = 0;
    int outside = 0;

    for (int y = 0; y < kSurfaceSide; ++y)
    {
        for (int x = 0; x < kSurfaceSide; ++x)
        {
            if (surface.Row(y)[x] != 0)
            {
                ++(x < kSize && y < kSize ? set : outside);
                CHECK_EQ(surface.Row(y)[x], swd3e::DevicePixel(kWhiteRef));
            }
        }
    }

    CHECK(set > 0);
    CHECK_EQ(outside, 0);
}
