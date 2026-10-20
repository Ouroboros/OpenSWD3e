#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "config/config.h"
#include "core/format.h"
#include "draw/painter.h"
#include "draw/weave.h"
#include "game/databases.h"
#include "game/game.h"
#include "game/game_state.h"
#include "game_data.h"
#include "map/blocking_map.h"
#include "map/field.h"
#include "map/field_view.h"
#include "map/map_cache.h"
#include "pic/rle.h"
#include "platform/display.h"
#include "platform/surface.h"
#include "recording_reporter.h"
#include "role/role.h"
#include "test.h"

using swd3e::u16;
using swd3e::u32;
using swd3e::u8;

namespace
{

// A directory with HUGE.lmf of the game linked in, for the cache files the loading writes.
std::string MapDirectory()
{
    const std::string directory = SWD3_SOURCE_DIR "/tmp/field_test/";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory + "data");
    std::error_code error;
    std::filesystem::create_symlink(swd3etest::GameFile("HUGE.lmf"), directory + "HUGE.lmf", error);
    return directory;
}

struct Fixture
{
    Fixture()
        : surface(swd3e::Display::Width, swd3e::Display::Height),
          cache(this->config, this->painter, this->state.map, this->reporter),
          codec(this->painter),
          field(this->state, this->cache, this->config, this->codec, this->reporter)
    {
        this->painter.Init(this->surface);
    }

    swd3e::Config config;
    swd3e::Surface surface;
    swd3e::Painter painter;
    swd3e::GameState state;
    swd3etest::RecordingReporter reporter;
    swd3e::MapCache cache;
    swd3e::RleCodec codec;
    swd3e::Field field;
};

constexpr u32 kNoSuchUnit = 0xFFFE;
constexpr int kRows = 4;
constexpr u16 kFirstId = 5;  // three roles in one row
constexpr u16 kSecondId = 3;
constexpr u16 kThirdId = 7;
constexpr u16 kAbsentId = 9;
constexpr swd3e::i32 kRoleY = 0x20;
constexpr int kGridSide = 8;
constexpr int kCellX = 3;  // a one-cell footprint
constexpr int kCellY = 3;
constexpr int kLargeX = 2;  // a 3 x 3 footprint
constexpr int kLargeY = 2;
constexpr int kLargeSide = 3;

}  // namespace

TEST_CASE("field: the first map of a new game from HUGE.lmf")
{
    using swd3e::MapsRecord;
    std::vector<u8> maps = swd3etest::ReadGameFile("maps.dat");

    if (maps.size() <= swd3e::GameDatabases::MapsHeader || !std::filesystem::exists(swd3etest::GameFile("HUGE.lmf")))
    {
        std::printf("  (maps.dat or HUGE.lmf not found, skipped)\n");
        return;
    }

    maps.erase(maps.begin(), maps.begin() + swd3e::GameDatabases::MapsHeader);
    const u32 start = swd3e::MapsTable(maps, swd3e::MapsDataTable::NewStory);
    const u16 map = swd3e::MapsWord(maps, start);
    u16 unit = MapsRecord::TableEnd;

    for (u32 at = swd3e::MapsTable(maps, swd3e::MapsDataTable::MapSettings);
         swd3e::MapsWord(maps, at) != MapsRecord::TableEnd;
         at += MapsRecord::SettingsBytes)
    {
        if (swd3e::MapsWord(maps, at) == map)
        {
            unit = swd3e::MapsWord(maps, at + MapsRecord::SettingsUnitAt);
            break;
        }
    }

    CHECK(unit != MapsRecord::TableEnd);
    Fixture all;
    all.config.installDirectory = swd3e::Utf8Bytes(MapDirectory());
    u32 flags = 0;
    CHECK(all.field.LoadMap(unit, flags));
    CHECK_EQ(flags, 0u);
    const swd3e::MapHeader& header = all.state.map;
    CHECK(header.width > 0);
    CHECK(header.height > 0);
    CHECK(
        header.bitsPerPixel == swd3e::MapHeader::IndexedBits || header.bitsPerPixel == swd3e::MapHeader::TrueColorBits
    );
    CHECK_EQ(header.blocks.size(), static_cast<std::size_t>(header.width) * header.height);
    // swd3e keeps one tile word more than the map has (the dword after the tiles).
    CHECK_EQ(header.tiles.size(), static_cast<std::size_t>(header.width) * header.height * header.layers + 1);
    CHECK(header.pixels != nullptr);
    CHECK(std::filesystem::exists(std::filesystem::path(all.config.installDirectory + u8"data/mcache.dat")));
    CHECK_EQ(all.field.rows[0].size(), static_cast<std::size_t>(header.height) + 2 * swd3e::Field::RowMargin);

    const int width = header.width;
    CHECK(all.field.LoadMap(unit, flags));
    CHECK_EQ(all.state.map.width, width);
    CHECK(all.state.map.pixels != nullptr);

    flags = 0;
    CHECK(!all.field.LoadMap(kNoSuchUnit, flags));
    CHECK_EQ(flags, static_cast<u32>(swd3e::Game::QuitRequested));
    std::filesystem::remove_all(all.config.installDirectory);
}

TEST_CASE("field: the speed nibbles of the map settings match swd32.exe")
{
    constexpr swd3e::u32 kSpeedNibblesAt = 0x0049FBA8;
    const std::vector<swd3e::u8> exe = swd3etest::ReadGameFile("swd32.exe");

    if (exe.empty())
    {
        return;
    }

    CHECK(swd3etest::SameTable(exe, kSpeedNibblesAt, swd3e::Game::SpeedNibbles));
}

TEST_CASE("field: the drawing order of the roles in a row")
{
    Fixture all;
    all.state.map.height = kRows;
    all.field.ResetRows();
    swd3e::Role first;
    swd3e::Role second;
    swd3e::Role third;
    first.id = kFirstId;
    second.id = kSecondId;
    third.id = kThirdId;

    for (swd3e::Role* role : {&first, &second, &third})
    {
        role->y = kRoleY;
        all.field.InsertRole(*role, 0);
    }

    swd3e::Role* head = all.field.rows[0][(kRoleY >> swd3e::MapHeader::TileShift) + swd3e::Field::RowMargin];
    CHECK(head == &third);
    CHECK(head->next == &first);
    CHECK(first.next == &second);
    CHECK(second.next == nullptr);

    CHECK(all.field.RemoveRole(kFirstId, 0, 0, 1) == &first);
    CHECK(third.next == &second);
    CHECK(all.field.RemoveRole(kAbsentId, 0, 0, 1) == nullptr);
    CHECK(all.field.RemoveRole(kSecondId, 0, 0, 0) == nullptr);
    CHECK(third.next == &second);
}

TEST_CASE("field: the cells around a footprint")
{
    using swd3e::BlockingMap;
    using swd3e::MapHeader;
    Fixture all;
    MapHeader& map = all.state.map;
    map.width = kGridSide;
    map.height = kGridSide;
    map.blocks.assign(kGridSide * kGridSide, 0);

    auto at = [](int x, int y)
    {
        return static_cast<std::size_t>(x + kGridSide * y);
    };

    constexpr u32 kMask = MapHeader::Wall | MapHeader::SolidRole | MapHeader::Unk800000;
    map.blocks[at(kCellX - 1, kCellY - 1)] = MapHeader::Wall;   // up left
    map.blocks[at(kCellX + 1, kCellY)] = MapHeader::SolidRole;  // right
    map.blocks[at(kCellX, kCellY + 1)] = MapHeader::Unk800000;  // down
    map.blocks[at(kCellX - 1, kCellY + 1)] = 1;                 // down left, a region outside the mask
    CHECK_EQ(
        all.field.ProbeCells(static_cast<std::ptrdiff_t>(at(kCellX, kCellY)), 1, 1, kMask),
        BlockingMap::ProbeUpLeft | BlockingMap::ProbeRight | BlockingMap::ProbeDown
    );
    CHECK_EQ(
        all.field.ProbeCells(static_cast<std::ptrdiff_t>(at(0, 0)), 1, 1, kMask),
        BlockingMap::ProbeUpLeft | BlockingMap::ProbeUp | BlockingMap::ProbeUpRight | BlockingMap::ProbeLeft
    );  // the left column wraps to the row end

    map.blocks.assign(kGridSide * kGridSide, 0);
    map.blocks[at(kLargeX + kLargeSide, kLargeY)] = MapHeader::SolidRole;  // right of the footprint, its top row
    map.blocks[at(kLargeX + 1, kLargeY - 1)] = MapHeader::SolidRole;       // above its middle
    CHECK_EQ(
        all.field.ProbeCells(static_cast<std::ptrdiff_t>(at(kLargeX, kLargeY)), kLargeSide, kLargeSide, kMask),
        BlockingMap::ProbeUpLeft | BlockingMap::ProbeUp | BlockingMap::ProbeUpRight | BlockingMap::ProbeRight
    );
}

TEST_CASE("field view: the parts of a render scale are woven into its blocks")
{
    using swd3e::Display;
    using swd3e::FieldView;
    constexpr int kWidth = Display::Width;
    constexpr int kHeight = Display::Height;
    constexpr std::size_t kPartStride = 0x1000;  // every pixel of every part a value of its own
    constexpr int kColumnStride = 7;
    int wrong = 0;

    for (int scale = 1; scale <= swd3e::Game::MaxRenderScale; ++scale)
    {
        const auto blocks = static_cast<std::size_t>(scale);
        std::vector<swd3e::Surface> parts(blocks * blocks);

        for (std::size_t part = 0; part < parts.size(); ++part)
        {
            parts[part].Resize(kWidth, kHeight);

            for (int y = 0; y < kHeight; ++y)
            {
                for (int x = 0; x < kWidth; ++x)
                {
                    parts[part].Row(y)[x] =
                        static_cast<swd3e::u16>(part * kPartStride + static_cast<std::size_t>(y + x * kColumnStride));
                }
            }
        }

        std::vector<const swd3e::Surface*> pointers;

        for (const swd3e::Surface& part : parts)
        {
            pointers.push_back(&part);
        }

        std::vector<swd3e::u16> woven(blocks * blocks * Display::Pixels);
        swd3e::Weave(pointers, scale, woven);
        const std::size_t pitch = blocks * kWidth;

        for (std::size_t y = 0; y < blocks * kHeight; ++y)
        {
            for (std::size_t x = 0; x < pitch; ++x)
            {
                const swd3e::Surface& part = parts[(y % blocks) * blocks + x % blocks];
                wrong += woven[y * pitch + x] == part.Row(static_cast<int>(y / blocks))[x / blocks] ? 0 : 1;
            }
        }
    }

    CHECK_EQ(wrong, 0);
}

TEST_CASE("field view: spans of a logical layer scaled into blocks")
{
    using swd3e::Display;
    using swd3e::FieldView;
    constexpr int kScale = 3;
    constexpr std::size_t kBlocks = kScale;
    constexpr std::size_t kPitch = kBlocks * Display::Width;
    constexpr swd3e::u32 kStart = Display::Width - 5;  // a run over the end of the first row
    constexpr swd3e::u32 kLength = 12;
    constexpr swd3e::u16 kUntouched = 0xFFFF;
    constexpr std::size_t kPatternPeriod = 0x7FFF;  // pixel values that differ from kUntouched
    std::vector<swd3e::u16> layer(Display::Pixels);

    for (std::size_t index = 0; index < layer.size(); ++index)
    {
        layer[index] = static_cast<swd3e::u16>(index % kPatternPeriod);
    }

    std::vector<swd3e::u16> screen(kBlocks * kBlocks * Display::Pixels, kUntouched);
    FieldView::CopySpans({FieldView::Span{kStart, kLength}}, layer, kScale, Display::Width, screen);
    int wrong = 0;

    for (std::size_t y = 0; y < kBlocks * Display::Height; ++y)
    {
        for (std::size_t x = 0; x < kPitch; ++x)
        {
            const std::size_t index = (y / kBlocks) * Display::Width + x / kBlocks;
            const bool inside = index >= kStart && index < kStart + kLength;
            wrong += screen[y * kPitch + x] == (inside ? layer[index] : kUntouched) ? 0 : 1;
        }
    }

    CHECK_EQ(wrong, 0);
}
