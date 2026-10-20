#include <cstdio>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <string>

#include "core/format.h"
#include "draw/painter.h"
#include "game_data.h"
#include "golden/ui_scenarios.h"
#include "golden_check.h"
#include "pic/rle.h"
#include "platform/display.h"
#include "platform/surface.h"
#include "recording_reporter.h"
#include "res/tsw_archive.h"
#include "test.h"
#include "ui/ui_draw.h"

using golden::u16;
using golden::u32;
using golden::u64;

namespace
{

class UiBackend : public golden::UiBackend
{
public:
    UiBackend()
        : screen(swd3e::Display::Width, swd3e::Display::Height),
          codec(this->painter),
          archive(this->codec, this->errors)
    {
        this->painter.Init(this->screen);
        this->archive.SetGameDirectory(swd3etest::GameDirectory());
    }

    void Reset(u32 seed, int left, int top, int right, int bottom) override
    {
        for (int y = 0; y < swd3e::Display::Height; ++y)
        {
            for (int x = 0; x < swd3e::Display::Width; ++x)
            {
                this->screen.Row(y)[x] = golden::Background(seed, x, y);
            }
        }

        this->painter.SetTarget(this->screen.Pixels());
        this->painter.SetClipClamped(left, top, right, bottom);
    }

    void Fill(int x, int y, int width, int height, int red, int green, int blue, int mode) override
    {
        swd3e::FillBox(this->painter, x, y, width, height, red, green, blue, static_cast<swd3e::FillMode>(mode));
    }

    void Frame(int id, int left, int top, int right, int bottom, int alpha, int border) override
    {
        swd3e::DrawWindowFrame(this->painter, this->archive, id, left, top, right, bottom, alpha, border);
    }

    const u16* Screen() const override
    {
        return this->screen.Pixels();
    }

private:
    swd3e::Surface screen;
    swd3e::Painter painter;
    swd3e::RleCodec codec;
    swd3etest::RecordingReporter errors;
    swd3e::TswArchive archive;
};

constexpr swd3e::u16 kGrey = 0x7BEF;
constexpr int kBoxX = 10;  // an even column: the pixel and the one right of it form a pair
constexpr int kBoxY = 10;
constexpr int kBoxHeight = 3;
constexpr int kTopLevel = 31;  // a five-bit channel at its top

}  // namespace

TEST_CASE("ui drawing: a blend one pixel wide covers one pair")
{
    swd3e::Surface screen(swd3e::Display::Width, swd3e::Display::Height);
    screen.Fill(kGrey);
    swd3e::Painter painter;
    CHECK(painter.Init(screen));
    painter.SetTarget(screen.Pixels());
    painter.SetClipClamped(0, 0, swd3e::Display::Width, swd3e::Display::Height);
    swd3e::FillBox(painter, kBoxX, kBoxY, 1, kBoxHeight, kTopLevel, kTopLevel, kTopLevel, swd3e::FillMode::Blend);
    CHECK(screen.Row(kBoxY)[kBoxX] != kGrey);
    CHECK(screen.Row(kBoxY)[kBoxX + 1] != kGrey);  // the pair of the one pixel
    CHECK_EQ(screen.Row(kBoxY)[kBoxX + 2], kGrey);
    CHECK_EQ(screen.Row(kBoxY + kBoxHeight)[kBoxX], kGrey);
}

TEST_CASE("ui drawing: matches the golden results of the legacy boxes and frames")
{
    const std::map<std::string, std::string> golden =
        swd3etest::LoadGolden(SWD3_SOURCE_DIR "/tests/golden/ui_golden.txt");

    if (golden.empty() || swd3etest::ReadGameFile("all_sys.tsw").empty())
    {
        std::printf("  (game data or golden file not found, skipped)\n");
        return;
    }

    UiBackend backend;
    swd3etest::GoldenCheck check(golden);
    golden::RunUi(backend, std::string(swd3e::AsBytes(swd3etest::GameFile("all_sys.tsw"))), std::ref(check));
    std::printf("  compared %d scenarios\n", check.compared);
    CHECK_EQ(check.mismatches, 0);
    CHECK_EQ(check.compared, static_cast<int>(golden.size()));
}
