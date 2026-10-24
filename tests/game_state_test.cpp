#include <algorithm>
#include <cstdio>
#include <vector>

#include "core/types.h"
#include "draw/painter.h"
#include "game/direction.h"
#include "game/game_state.h"
#include "game_data.h"
#include "map/field_view.h"
#include "platform/display.h"
#include "platform/surface.h"
#include "res/act_state.h"
#include "role/role_control.h"
#include "role/role_path.h"
#include "test.h"
#include "ui/marching_rect.h"

using swd3e::GameState;
using swd3e::u16;
using swd3e::u8;

namespace
{

// Where swd3e.exe keeps the tables: every copy a function reads (the compiler keeps one per translation unit).
constexpr swd3e::u32 kSineTableAt = 0x004B086C;
constexpr std::array<swd3e::u32, 4> kBitMasksCopies = {0x004AA468, 0x004AA474, 0x004AA4BC, 0x004AA674};
constexpr std::array<swd3e::u32, 2> kBlockedByCopies = {0x004AA3E8, 0x004AA5F4};
constexpr std::array<swd3e::u32, 4> kStepCopies = {0x004AA34C, 0x004AA408, 0x004AA47C, 0x004AA614};  // x, then y
constexpr std::array<swd3e::u32, 3> kFacingCopies = {0x004AA448, 0x004AA544, 0x004AA654};
constexpr swd3e::u32 kAheadXAt = 0x004AA398;
constexpr swd3e::u32 kAheadYAt = 0x004AA3B8;
constexpr swd3e::u32 kRunToggleAt = 0x004AA38C;
constexpr swd3e::u32 kCellStepXAt = 0x004AA504;
constexpr swd3e::u32 kCellStepYAt = 0x004AA524;

constexpr int kFlag = 13;
constexpr int kNeighbourFlag = 8;  // in the byte of kFlag
constexpr int kNotOne = 2;         // SetGameFlagTo clears for anything but 1
constexpr int kFrameInterval = 5;

constexpr int kRegionLeft = 10;  // a small region inside a large one
constexpr int kRegionTop = 10;
constexpr int kRegionRight = 20;
constexpr int kRegionBottom = 20;
constexpr int kLargeSize = 100;
constexpr std::array<swd3e::i32, 3> kRegionState = {1, 2, 3};

constexpr int kSamePoint = 5;
constexpr int kAbove = 30;
constexpr int kStraightUpAngle = 85;  // the sine table finds 85 degrees for a point straight above
constexpr int kRightAngle = 90;
constexpr int kDirectionAt90 = 0;
constexpr int kLastDegree = 359;
constexpr int kDirectionAt359 = 3;
constexpr int kPlayer = 3;
constexpr int kPlayerAt = 100;
constexpr int kBelowPlayer = 130;

constexpr int kFrameWidth = 40;  // a frame reaching the right edge is not drawn
constexpr int kFrameHeight = 10;
constexpr int kFrameX = 10;
constexpr int kFrameY = 10;
constexpr int kSmallWidth = 4;
constexpr int kSmallHeight = 3;
constexpr int kRowStep = 7;  // the test picture: pixel y * kRowStep + x
constexpr int kShrink = 4;   // ShrinkScreenQuarter keeps every fourth pixel and row
constexpr int kActPosition = 9;

}  // namespace

TEST_CASE("game state: constant tables match swd3e.exe")
{
    const std::vector<u8> exe = swd3etest::ReadGameFile("swd3e.exe");

    if (exe.empty())
    {
        std::printf("  (swd3e.exe not found, skipped)\n");
        return;
    }

    using swd3etest::SameTable;
    CHECK(SameTable(exe, kSineTableAt, swd3e::kSineTable));

    for (const swd3e::u32 copy : kBitMasksCopies)
    {
        CHECK(SameTable(exe, copy, GameState::BitMasks));
    }

    for (const swd3e::u32 copy : kBlockedByCopies)
    {
        CHECK(SameTable(exe, copy, swd3e::RolePath::BlockedBy));
    }

    for (const swd3e::u32 copy : kStepCopies)
    {
        CHECK(SameTable(exe, copy, swd3e::RolePath::StepX));
        CHECK(SameTable(exe, copy + sizeof(swd3e::RolePath::StepX), swd3e::RolePath::StepY));
    }

    for (const swd3e::u32 copy : kFacingCopies)
    {
        CHECK(SameTable(exe, copy, swd3e::RolePath::Facing));
    }

    CHECK(SameTable(exe, kAheadXAt, swd3e::RoleControl::AheadX));
    CHECK(SameTable(exe, kAheadYAt, swd3e::RoleControl::AheadY));
    CHECK(SameTable(exe, kRunToggleAt, swd3e::RoleControl::RunToggle));
    CHECK(SameTable(exe, kCellStepXAt, swd3e::FieldView::CellStepX));
    CHECK(SameTable(exe, kCellStepYAt, swd3e::FieldView::CellStepY));
}

TEST_CASE("game state: story flags")
{
    GameState state;
    const u8 flagBit = GameState::BitMasks[kFlag & GameState::FlagBitMask];
    const u8 neighbourBit = GameState::BitMasks[kNeighbourFlag & GameState::FlagBitMask];
    CHECK(!state.GameFlag(kFlag));
    CHECK_EQ(state.SetGameFlag(kFlag), flagBit);
    CHECK(state.GameFlag(kFlag));
    CHECK_EQ(state.SetGameFlagTo(kNeighbourFlag, 1), flagBit | neighbourBit);
    CHECK_EQ(state.SetGameFlagTo(kFlag, kNotOne), neighbourBit);
    CHECK(!state.GameFlag(kFlag));
    CHECK_EQ(state.ClearGameFlag(kNeighbourFlag), 0);
    CHECK_EQ(state.SetFrameInterval(kFrameInterval), 1);
    CHECK_EQ(state.frameInterval, kFrameInterval);
}

TEST_CASE("game state: click regions are matched strictly inside, the head first")
{
    swd3e::ClickRegions regions;
    regions.regions.push_front({0, kRegionLeft, kRegionTop, kRegionRight, kRegionBottom});
    regions.regions.push_front({0, 0, 0, kLargeSize, kLargeSize});
    CHECK_EQ(regions.Count(), static_cast<int>(regions.regions.size()));
    swd3e::ClickRegion* found = nullptr;
    CHECK_EQ(regions.Find((kRegionLeft + kRegionRight) / 2, (kRegionTop + kRegionBottom) / 2, found), 0);
    CHECK(found == &regions.regions.front());
    CHECK_EQ(regions.Find(kLargeSize, kLargeSize / 2, found), regions.Count());
    CHECK(found == nullptr);
    regions.state = kRegionState;
    regions.Free();
    CHECK_EQ(regions.Count(), 0);
    CHECK(
        std::all_of(
            regions.state.begin(),
            regions.state.end(),
            [](swd3e::i32 value)
            {
                return value == 0;
            }
        )
    );
}

TEST_CASE("game state: angles and directions")
{
    swd3e::i32 distance = -1;
    CHECK_EQ(swd3e::AngleBetween(kSamePoint, kSamePoint, kSamePoint, kSamePoint, distance), 0);
    CHECK_EQ(distance, 0);
    CHECK_EQ(swd3e::AngleBetween(0, -kAbove, 0, 0, distance), kStraightUpAngle);
    CHECK_EQ(distance, kAbove);
    CHECK_EQ(swd3e::AngleToDirection(kRightAngle), kDirectionAt90);
    CHECK_EQ(swd3e::AngleToDirection(kLastDegree), kDirectionAt359);
    GameState state;
    state.playerRole = kPlayer;
    state.roles[kPlayer].x = kPlayerAt;
    state.roles[kPlayer].y = kPlayerAt;
    CHECK_EQ(
        state.DirectionToPlayer(kPlayerAt, kBelowPlayer),
        swd3e::AngleToDirection(swd3e::AngleBetween(kPlayerAt, kPlayerAt, kPlayerAt, kBelowPlayer, distance))
    );
}

TEST_CASE("game state: marching frame and the save thumbnail")
{
    using swd3e::Display;
    swd3e::Surface screen(Display::Width, Display::Height);
    screen.Fill(0);
    swd3e::Painter painter;
    CHECK(painter.Init(screen));
    swd3e::MarchingRect frame;
    frame.Draw(screen, painter, Display::Width - kFrameWidth, kFrameY, kFrameWidth, kFrameHeight);
    CHECK_EQ(frame.phase, 0);
    frame.Draw(screen, painter, kFrameX, kFrameY, kSmallWidth, kSmallHeight);
    CHECK_EQ(frame.phase, 1);
    CHECK(screen.Row(kFrameY)[kFrameX] != 0);
    CHECK(screen.Row(kFrameY + kSmallHeight)[kFrameX] != 0);
    CHECK_EQ(screen.Row(kFrameY + 1)[kFrameX + 1], 0);

    for (int y = 0; y < Display::Height; ++y)
    {
        for (int x = 0; x < Display::Width; ++x)
        {
            screen.Row(y)[x] = static_cast<u16>(y * kRowStep + x);
        }
    }

    swd3e::ShrinkScreenQuarter(screen);
    constexpr int kShrunkWidth = Display::Width / kShrink;
    constexpr int kShrunkHeight = Display::Height / kShrink;
    const u16* shrunk = screen.Pixels();
    CHECK_EQ(shrunk[0], 0);
    CHECK_EQ(shrunk[1], kShrink);
    CHECK_EQ(shrunk[kShrunkWidth], static_cast<u16>(kShrink * kRowStep));
    CHECK_EQ(
        shrunk[(kShrunkHeight - 1) * kShrunkWidth + kShrunkWidth - 1],
        static_cast<u16>((Display::Height - kShrink) * kRowStep + Display::Width - kShrink)
    );
    swd3e::ActState act;
    act.position = kActPosition;
    act.hold = 1;
    swd3e::ResetPlayback(act);
    CHECK_EQ(act.position, 0);
    CHECK_EQ(act.unknown1C, -1);
    CHECK_EQ(act.hold, 0);
}

TEST_CASE("game state: a role between tiles goes back to its tile as the original's loops take it")
{
    // The original takes the step off until the place is on a tile; its places are multiples of the 4 pixel steps.
    constexpr int kFrom = -0x40;
    constexpr int kTo = 0x400;
    constexpr int kStep = 4;

    auto loop = [](swd3e::i32 place, swd3e::i32 step)
    {
        while ((place & swd3e::MapHeader::TileFraction) != 0)
        {
            place -= step;
        }

        return place;
    };

    for (const swd3e::i32 step : swd3e::RolePath::StepX)
    {
        for (int place = kFrom; place <= kTo; place += kStep)
        {
            if (step == 0 && (place & swd3e::MapHeader::TileFraction) != 0)
            {
                continue;
            }

            CHECK_EQ(swd3e::RolePath::TileStart(place, step), loop(place, step));
            CHECK_EQ(swd3e::RolePath::TileStart(place, -step), loop(place, -step));
        }
    }

    // Any other place (a fraction the step does not divide) never comes to a tile in those loops: TileStart hangs.
}
