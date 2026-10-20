#include "core/types.h"
#include "gfx/graphics.h"
#include "platform/display.h"
#include "platform/surface.h"
#include "recording_reporter.h"
#include "test.h"

using swd3e::Graphics;
using swd3e::i32;

namespace
{

constexpr int kDisplayBits = 16;
constexpr int kIndexedBits = 8;
constexpr int kWindowWidth = 320;  // a window size the game passes; the back buffer stays the screen size
constexpr int kWindowHeight = 200;
constexpr swd3e::u16 kColor = 0x1234;
constexpr int kSmallSide = 4;
constexpr swd3e::u16 kBackground = 0x0F0F;
constexpr swd3e::u32 kWhiteRef = 0x00FFFFFF;  // COLORREF values (0x00BBGGRR)
constexpr swd3e::u32 kRedRef = 0x000000FF;
constexpr swd3e::u32 kGreenRef = 0x0000FF00;
constexpr swd3e::u32 kBlueRef = 0x00FF0000;

}  // namespace

TEST_CASE("graphics: full screen setup as the game requests it")
{
    swd3e::Display display;
    swd3etest::RecordingReporter errors;
    Graphics graphics(display, errors);
    CHECK(graphics.Init(Graphics::FullScreen, swd3e::Display::Width, swd3e::Display::Height, kDisplayBits));
    swd3e::Surface* primary = graphics.GetSurface(Graphics::SurfaceId::Primary);
    CHECK(primary != nullptr);
    CHECK(graphics.GetSurface(Graphics::SurfaceId::Back) == nullptr);
    CHECK(graphics.GdiSurface() == primary);
    i32 pitch = 0;
    i32 height = 0;
    CHECK_EQ(graphics.BackSize(pitch, height), swd3e::Display::Height);
    CHECK_EQ(pitch, swd3e::Display::Width * static_cast<i32>(sizeof(swd3e::Pixel)));
    CHECK_EQ(graphics.BitDepth(), static_cast<swd3e::u32>(kDisplayBits));
    CHECK_EQ(graphics.DetectPixelFormat(), swd3e::Surface::GreenMask);
    CHECK(graphics.Present());
    CHECK(errors.messages.empty());
}

TEST_CASE("graphics: other depths fail like an unsupported display mode")
{
    swd3e::Display display;
    swd3etest::RecordingReporter errors;
    Graphics graphics(display, errors);
    CHECK(!graphics.Init(Graphics::FullScreen, swd3e::Display::Width, swd3e::Display::Height, kIndexedBits));
    CHECK_EQ(errors.messages.size(), 1u);
    CHECK(errors.messages[0] == "DirectDraw SetDisplayMode Error!!: DDERR_UNSUPPORTEDMODE  ");
}

TEST_CASE("graphics: windowed mode presents the back buffer")
{
    swd3e::Display display;
    swd3etest::RecordingReporter errors;
    Graphics graphics(display, errors);
    CHECK(graphics.Init(Graphics::Windowed, kWindowWidth, kWindowHeight, kDisplayBits));
    swd3e::Surface* back = graphics.GetSurface(Graphics::SurfaceId::Back);
    CHECK(back != nullptr);
    CHECK_EQ(back->Width(), swd3e::Display::Width);
    back->Fill(kColor);
    graphics.Present();
    CHECK_EQ(
        graphics.GetSurface(Graphics::SurfaceId::Primary)->Row(swd3e::Display::Height - 1)[swd3e::Display::Width - 1],
        kColor
    );
}

TEST_CASE("graphics: device colours of COLORREF values")
{
    using swd3e::Surface;
    Surface surface(kSmallSide, kSmallSide);
    surface.Fill(kBackground);
    CHECK_EQ(swd3e::MatchSurfaceColor(surface, kWhiteRef), Surface::RedMask | Surface::GreenMask | Surface::BlueMask);
    CHECK_EQ(swd3e::MatchSurfaceColor(surface, kRedRef), Surface::RedMask);
    CHECK_EQ(swd3e::MatchSurfaceColor(surface, kBlueRef), Surface::BlueMask);
    CHECK_EQ(swd3e::MatchSurfaceColor(surface, swd3e::kInvalidColor), kBackground);
    CHECK_EQ(surface.Row(0)[0], kBackground);
    swd3e::FillSurfaceColor(surface, kGreenRef);
    CHECK_EQ(surface.Row(kSmallSide - 1)[kSmallSide - 1], Surface::GreenMask);
}
