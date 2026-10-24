#include <cstdio>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <string>

#include "core/format.h"
#include "draw/painter.h"
#include "game_data.h"
#include "pic/rle.h"
#include "platform/display.h"
#include "platform/surface.h"
#include "recording_reporter.h"
#include "res/tsw_archive.h"
#include "test.h"
#include "ui/ui_draw.h"

using swd3e::u16;
using swd3e::u32;
using swd3e::u64;

namespace
{

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
