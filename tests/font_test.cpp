#include <climits>
#include <cstdio>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <string>

#include "core/types.h"
#include "font/text_font.h"
#include "gfx/graphics.h"
#include "golden/font_scenarios.h"
#include "golden_check.h"
#include "platform/display.h"
#include "platform/glyph_rasterizer.h"
#include "platform/surface.h"
#include "recording_reporter.h"
#include "test.h"

using swd3e::u16;
using swd3e::u32;
using swd3e::u64;
using swd3e::u8;

namespace
{

constexpr int kAdvance = 16;  // the pixels a character moves the pen
constexpr int kDisplayBits = 16;

class NullRasterizer : public swd3e::GlyphRasterizer
{
public:
    void RenderText(swd3e::Surface&, std::string_view, int, std::string_view, u32) override
    {
    }
};

class FontBackend : public golden::FontBackend
{
public:
    explicit FontBackend(swd3e::TextFont& font)
        : font(font)
    {
    }

    void Reset(u32 seed) override
    {
        this->screen = std::make_unique<swd3e::Surface>(golden::kScreenWidth, golden::kScreenHeight);

        for (int y = 0; y < golden::kScreenHeight; ++y)
        {
            for (int x = 0; x < golden::kScreenWidth; ++x)
            {
                this->screen->Row(y)[x] = golden::Background(seed, x, y);
            }
        }

        this->font.SetClip(0, 0, golden::kScreenWidth, golden::kScreenHeight);
        this->font.SetBackground(swd3e::TextFont::NoBackground);
        this->font.SetAdvance(kAdvance);
    }

    void DrawText(int x, int y, std::string_view text, u16 color, int style) override
    {
        this->font.DrawText(this->screen->Pixels(), x, y, text, color, style);
    }

    void SetClip(int x, int y, int width, int height) override
    {
        this->font.SetClip(x, y, width, height);
    }

    void SetBackground(u16 color) override
    {
        this->font.SetBackground(color);
    }

    void SetShadow(u16 color) override
    {
        this->font.SetShadow(color);
    }

    void SetAdvance(int advance) override
    {
        this->font.SetAdvance(advance);
    }

    void FillRect(int x, int y, int width, int height, u16 color) override
    {
        this->font.FillRect(this->screen->Pixels(), x, y, width, height, color);
    }

    int GlyphCount() override
    {
        return this->font.GlyphCount();
    }

    u16 GlyphCode(int slot) override
    {
        return this->font.GlyphCode(slot);
    }

    u8* GlyphBits(int slot) override
    {
        return this->font.GlyphBits(slot);
    }

    int GlyphBytes() override
    {
        return ((this->font.GlyphWidth() + CHAR_BIT - 1) / CHAR_BIT) * this->font.GlyphHeight();
    }

    const u16* Screen() override
    {
        return this->screen->Pixels();
    }

private:
    swd3e::TextFont& font;
    std::unique_ptr<swd3e::Surface> screen;
};

}  // namespace

TEST_CASE("text font: matches the golden results of the legacy renderer")
{
    const std::map<std::string, std::string> golden =
        swd3etest::LoadGolden(SWD3_SOURCE_DIR "/tests/golden/font_golden.txt");

    if (golden.empty())
    {
        std::printf("  (golden file not found, skipped)\n");
        return;
    }

    swd3e::Display display;
    swd3etest::RecordingReporter errors;
    swd3e::Graphics graphics(display, errors);
    graphics.Init(swd3e::Graphics::FullScreen, swd3e::Display::Width, swd3e::Display::Height, kDisplayBits);
    NullRasterizer rasterizer;
    swd3e::TextFont font(rasterizer);
    font.Init(graphics);
    FontBackend backend(font);
    swd3etest::GoldenCheck check(golden);
    golden::RunFont(backend, std::ref(check));
    std::printf("  compared %d scenarios\n", check.compared);
    CHECK_EQ(check.mismatches, 0);
    CHECK_EQ(check.compared, static_cast<int>(golden.size()));
}
