#include <array>
#include <cstdio>
#include <fstream>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "game/game_state.h"
#include "map/blocking_map.h"
#include "recording_reporter.h"
#include "role/path_finder.h"
#include "test.h"

using swd3e::u64;
using swd3e::u8;

namespace
{

constexpr std::size_t kDirections = std::tuple_size_v<decltype(swd3e::MoveRecord::directions)>;
constexpr u8 kUnwritten = 0xEE;
constexpr int kTwice = 2;
constexpr int kSide = 10;  // a 10 x 10 map walled in, a wall at kWallX from the top border down to kWallEnd
constexpr int kWallX = 5;
constexpr int kWallEnd = 7;
constexpr int kStartX = 2;
constexpr int kStartY = 2;
constexpr int kGoalX = 7;
// The step of each direction byte (0 up left, clockwise).
constexpr std::array<int, 8> kStepX = {-1, 0, 1, 1, 1, 0, -1, -1};
constexpr std::array<int, 8> kStepY = {-1, -1, -1, 0, 1, 1, 1, 0};

}  // namespace

TEST_CASE("path finder: a path around a wall, and the second search of one finder")
{
    using swd3e::MapHeader;
    swd3e::PathNodePool pool;
    pool.Init();
    swd3e::BlockingMap map;
    map.width = kSide;
    std::vector<swd3e::u32> cells(static_cast<std::size_t>(kSide * kSide), 0);
    map.cells = cells;

    auto cell = [](int x, int y)
    {
        return static_cast<std::size_t>(y * kSide + x);
    };

    for (int along = 0; along < kSide; ++along)
    {
        cells[cell(0, along)] = MapHeader::Outside;
        cells[cell(kSide - 1, along)] = MapHeader::Outside;
        cells[cell(along, 0)] = MapHeader::Outside;
        cells[cell(along, kSide - 1)] = MapHeader::Outside;
    }

    for (int y = 1; y < kWallEnd; ++y)
    {
        cells[cell(kWallX, y)] = MapHeader::Wall;
    }

    swd3etest::RecordingReporter reporter;
    swd3e::PathFinder finder(pool, map, reporter);
    std::vector<u8> directions(kDirections);
    constexpr int kTile = MapHeader::TileSize;
    CHECK_EQ(finder.FindPath(kStartX * kTile, kStartY * kTile, kGoalX * kTile, kStartY * kTile, 1, 1, directions), 1);
    int x = kStartX;
    int y = kStartY;
    int steps = 0;

    for (const u8 direction : directions)
    {
        if (direction == swd3e::PathFinder::End)
        {
            break;
        }

        // A direction byte is looked up from the tile minus the next one, so it names the opposite of the step.
        x -= kStepX[direction];
        y -= kStepY[direction];
        CHECK(map.cells[cell(x, y)] == 0);
        ++steps;
    }

    CHECK_EQ(x, kGoalX);
    CHECK_EQ(y, kStartY);
    CHECK(steps > kGoalX - kStartX);  // longer than the way straight through the wall
    CHECK_EQ(finder.found, 1);
    CHECK_EQ(
        finder.FindPath(kStartX * kTile, kStartY * kTile, kGoalX * kTile, (kStartY + 1) * kTile, 1, 1, directions),
        0
    );
    finder.Reset();
    CHECK_EQ(pool.peak, swd3e::PathNodePool::Initial);
    CHECK(pool.Release());
    CHECK(reporter.messages.empty());
}
