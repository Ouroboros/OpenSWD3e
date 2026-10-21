#include <algorithm>
#include <bit>
#include <climits>
#include <cstdio>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <SDL3/SDL.h>

#include "game/game.h"
#include "game/game_state.h"
#include "launcher/launcher.h"
#include "platform/display.h"
#include "platform/exe_resources.h"
#include "platform/file_system.h"
#include "platform/surface.h"
#include "platform/system_sound.h"
#include "test.h"

using swd3e::Launcher;

namespace
{

constexpr std::size_t kDialogBytes = 274;  // the launcher dialog template and its hover sound in swd3e.exe
constexpr std::size_t kHoverSoundBytes = 2348;
constexpr int kNoSuchWave = 0x99;
constexpr int kHighlightWidth = 0x70;  // a highlight picture, the size of its static control
constexpr int kHighlightHeight = 0x28;
constexpr int kOutside = 0;  // above and left of every picture

}  // namespace

TEST_CASE("launcher: resources of swd3e.exe and the hover hotspots")
{
    swd3e::ExeResources resources;
    resources.Open(swd3e::AssetDirectory() + std::u8string(swd3e::ExeResources::Subdirectory));
    swd3e::Surface picture;

    if (!resources.LoadBitmap(Launcher::Background, picture))
    {
        std::printf("  (asset not found, skipped)\n");
        return;
    }

    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    CHECK_EQ(resources.ReadDialog(Launcher::Dialog).size(), kDialogBytes);
    CHECK_EQ(resources.ReadSound(Launcher::HoverSound).size(), kHoverSoundBytes);
    CHECK(resources.ReadSound(kNoSuchWave).empty());
    CHECK_EQ(picture.Width(), swd3e::Display::Width);
    CHECK_EQ(picture.Height(), swd3e::Display::Height);
    CHECK(resources.LoadBitmap(Launcher::Layout.back().picture, picture));
    CHECK_EQ(picture.Width(), kHighlightWidth);
    CHECK_EQ(picture.Height(), kHighlightHeight);

    swd3e::GameState state;
    swd3e::SystemSound sound;
    Launcher launcher(resources, state, sound, u8"");
    CHECK(launcher.LoadPictures());
    launcher.Init();
    CHECK(launcher.shown[0]);
    CHECK(launcher.OnMouse(0, kOutside, kOutside) == Launcher::Choice::None);
    CHECK(!launcher.shown[0]);
    const Launcher::Item& first = Launcher::Layout.front();
    const Launcher::Item& start = Launcher::Layout[1];
    const Launcher::Item& last = Launcher::Layout.back();
    CHECK(launcher.OnMouse(1, first.x, first.y) == Launcher::Choice::None);
    CHECK_EQ(launcher.lit[0], 0);
    state.SetGameFlag(Launcher::SavedGameFlag);
    CHECK(launcher.OnMouse(0, first.x, first.y) == Launcher::Choice::None);
    CHECK_EQ(launcher.lit.front(), Launcher::Shown);
    CHECK(launcher.shown[0]);
    CHECK(launcher.OnMouse(1, first.x, first.y) == Launcher::Choice::Continue);
    CHECK(launcher.OnMouse(1, start.x, start.y + kHighlightHeight - 1) == Launcher::Choice::Start);
    CHECK(launcher.OnMouse(1, start.x + kHighlightWidth, start.y) == Launcher::Choice::None);
    CHECK(launcher.OnMouse(1, start.x - 1, start.y) == Launcher::Choice::None);
    CHECK(launcher.OnMouse(1, last.x, last.y) == Launcher::Choice::Exit);
    CHECK_EQ(launcher.lit.back(), Launcher::Shown);
    CHECK(launcher.OnClose() == Launcher::Choice::Exit);
    swd3e::Surface screen(swd3e::Display::Width, swd3e::Display::Height);
    launcher.Draw(screen);
}

TEST_CASE("launcher: the window icon and cursor of swd3e.exe")
{
    constexpr int kSide = 32;      // the cursor
    constexpr int kIconSide = 48;  // the icon member the window takes: the first with the most bits (8)
    constexpr std::u8string_view kIconFile = u8"125.ico";  // Game::WindowIcon
    constexpr std::size_t kEntriesAt = 6;
    constexpr std::size_t kEntryBytes = 16;
    constexpr std::size_t kMemberAtField = 12;
    constexpr std::size_t kChosenEntry = 1;
    constexpr std::size_t kHeaderBytes = 40;
    constexpr std::size_t kBitsAt = 14;  // biBitCount
    constexpr std::size_t kIndexBits = 8;
    constexpr std::size_t kPaletteBytes = 4 * 256;
    constexpr std::size_t kMaskRowBytes = 8;  // 48 bits padded to a dword boundary
    constexpr std::size_t kPixelsAt = kHeaderBytes + kPaletteBytes;
    constexpr std::size_t kMaskAt = kPixelsAt + kIconSide * kIconSide;
    constexpr swd3e::u32 kOpaque = 0xFF000000u;
    swd3e::ExeResources resources;
    resources.Open(swd3e::AssetDirectory() + std::u8string(swd3e::ExeResources::Subdirectory));
    swd3e::IconImage icon;
    std::vector<swd3e::u8> file;

    if (!resources.LoadIcon(swd3e::Game::WindowIcon, icon) ||
        !swd3e::ReadSharedFile(
            swd3e::AssetDirectory() + std::u8string(swd3e::ExeResources::Subdirectory) + std::u8string(kIconFile),
            file
        ))
    {
        std::printf("  (asset not found, skipped)\n");
        return;
    }

    CHECK_EQ(icon.width, kIconSide);
    CHECK_EQ(icon.height, kIconSide);
    CHECK_EQ(icon.pixels.size(), static_cast<std::size_t>(kIconSide * kIconSide));

    const std::size_t field = kEntriesAt + kEntryBytes * kChosenEntry + kMemberAtField;
    const std::size_t memberAt = static_cast<std::size_t>(file[field]) | static_cast<std::size_t>(file[field + 1]) << 8;
    CHECK(file.size() > memberAt + kMaskAt + kMaskRowBytes * kIconSide - 1);
    const std::span<const swd3e::u8> member = std::span<const swd3e::u8>(file).subspan(memberAt);
    CHECK_EQ(member[0], kHeaderBytes);
    CHECK_EQ(member[kBitsAt], kIndexBits);
    // Shown: the pixels the mask leaves, and the masked ones of a colour (they invert the screen, drawn black).
    std::size_t clear = 0;

    for (std::size_t row = 0; row < kIconSide; ++row)
    {
        for (std::size_t column = 0; column < kIconSide; ++column)
        {
            const swd3e::u8 bits = member[kMaskAt + kMaskRowBytes * row + column / CHAR_BIT];
            const std::size_t index = member[kPixelsAt + kIconSide * row + column];
            const std::size_t entry = kHeaderBytes + 4 * index;
            const bool coloured = (member[entry] | member[entry + 1] | member[entry + 2]) != 0;
            clear += (bits >> (CHAR_BIT - 1 - column % CHAR_BIT) & 1) == 0 || coloured ? 1 : 0;
        }
    }

    const auto opaque = static_cast<std::size_t>(std::count_if(
        icon.pixels.begin(),
        icon.pixels.end(),
        [](swd3e::u32 pixel)
        {
            return pixel >= kOpaque;
        }
    ));
    CHECK_EQ(opaque, clear);

    const std::size_t topLeft = member[kPixelsAt + static_cast<std::size_t>(kIconSide) * (kIconSide - 1)];
    const std::size_t entry = kHeaderBytes + 4 * topLeft;
    const swd3e::u32 color = static_cast<swd3e::u32>(member[entry + 2]) << 16 |
                             static_cast<swd3e::u32>(member[entry + 1]) << 8 | member[entry];
    CHECK_EQ(icon.pixels.front() & ~kOpaque, (icon.pixels.front() >= kOpaque ? color : 0));

    swd3e::IconImage cursor;
    CHECK(resources.LoadCursor(swd3e::Game::WindowCursor, cursor));
    CHECK_EQ(cursor.width, kSide);
    CHECK_EQ(cursor.height, kSide);
    CHECK_EQ(cursor.hotX, 0);
    CHECK_EQ(cursor.hotY, 0);
    CHECK(
        std::all_of(
            cursor.pixels.begin(),
            cursor.pixels.end(),
            [](swd3e::u32 pixel)
            {
                return pixel == 0;
            }
        )
    );
    CHECK(!resources.LoadIcon(kNoSuchWave, icon));
}
