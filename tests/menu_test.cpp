#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "draw/painter.h"
#include "game_data.h"
#include "menu/menu.h"
#include "menu/menu_text.h"
#include "party/party.h"
#include "platform/display.h"
#include "platform/surface.h"
#include "save/save_games.h"
#include "save/save_slots.h"
#include "test.h"
#include "text/big5.h"

using swd3e::u32;
using swd3e::u8;

namespace
{

// Where swd32.exe keeps the texts and tables.
constexpr std::array<std::pair<swd3e::u32, std::u8string_view>, 6> kTexts = {{
    {0x004A70EC, swd3e::menu_text::kSoundVolume},
    {0x004A70A4, swd3e::menu_text::kCacheLine},
    {0x004A7054, swd3e::menu_text::kSample},
    {0x004A7068, swd3e::menu_text::kSpeedSlow},
    {0x004A7088, swd3e::menu_text::kSpeedFastest},
    {0x0049FA9C, swd3e::SaveSlots::Hours},
}};

constexpr swd3e::u32 kDateTemplateAt = 0x0049FAA4;
constexpr swd3e::u32 kInitialTabsAt = 0x004A6A38;
constexpr swd3e::u32 kKeyNamesAt = 0x0049E470;
constexpr swd3e::u32 kKeyNameBytes = 0x10;
constexpr swd3e::u32 kKeyRowsAt = 0x004A7230;
constexpr swd3e::u32 kKeyRowBytes = 0xC;
constexpr swd3e::u32 kKeyRowSlotsAt = 0x004A7210;

// The texts 0x00429790 copies: the default names, then the names of the talk texts, three parts a role.
constexpr std::array<std::array<swd3e::u32, 3>, swd3e::Party::Roles> kDefaultNamesAt = {{
    {0x004B2188, 0x004B2180, 0x004B2178},
    {0x004B2174, 0x004B216C, 0x004B2164},
    {0x004B215C, 0x004B2154, 0x004B214C},
    {0x004B2148, 0x004B2144, 0x004B213C},
}};

constexpr std::array<std::array<swd3e::u32, 3>, swd3e::Party::Roles> kNameTagsAt = {{
    {0x004B2134, 0x004B2180, 0x004B2178},
    {0x004B212C, 0x004B216C, 0x004B2164},
    {0x004B215C, 0x004B2154, 0x004B214C},
    {0x004B2148, 0x004B2144, 0x004B213C},
}};

constexpr std::size_t kYearDigits = 4;  // the date starts with the year
constexpr swd3e::u16 kAbsentRole = 0xFFFF;

}  // namespace

TEST_CASE("menu: texts, names and tabs match swd32.exe")
{
    const std::vector<u8> exe = swd3etest::ReadGameFile("swd32.exe");

    if (exe.empty())
    {
        std::printf("  (swd32.exe not found, skipped)\n");
        return;
    }

    for (const auto& [address, text] : kTexts)
    {
        CHECK(swd3etest::SameText(exe, address, text));
    }

    CHECK(swd3etest::SameText(exe, kDateTemplateAt, swd3e::SaveSlots::DateTemplate));

    static_assert(sizeof(swd3e::MenuTab) == 0x1C);
    CHECK(swd3etest::SameTable(exe, kInitialTabsAt, swd3e::Menu::InitialTabs));

    for (std::size_t key = 0; key < swd3e::menu_text::kKeyNames.size(); ++key)
    {
        const auto address = kKeyNamesAt + kKeyNameBytes * static_cast<swd3e::u32>(key);
        CHECK(swd3etest::SameText(exe, address, swd3e::menu_text::kKeyNames[key]));
    }

    for (std::size_t row = 0; row < swd3e::menu_text::kKeyRows.size(); ++row)
    {
        const auto address = kKeyRowsAt + kKeyRowBytes * static_cast<swd3e::u32>(row);
        CHECK(swd3etest::SameText(exe, address, swd3e::menu_text::kKeyRows[row]));
    }

    CHECK(swd3etest::SameTable(exe, kKeyRowSlotsAt, swd3e::Menu::KeyRowSlots));
}

TEST_CASE("party: the default names and those of the talk texts match swd3e.exe")
{
    const std::vector<u8> exe = swd3etest::ReadGameFile("swd3e.exe");

    if (exe.empty())
    {
        std::printf("  (swd3e.exe not found, skipped)\n");
        return;
    }

    swd3e::Party party;
    party.ResetNames();

    for (std::size_t role = 0; role < swd3e::Party::Roles; ++role)
    {
        const swd3e::RoleName& name = party.names[role];
        const swd3e::RoleName& tag = party.nameTags[role];
        CHECK(swd3etest::SameText(exe, kDefaultNamesAt[role][0], name.Surname()));
        CHECK(swd3etest::SameText(exe, kDefaultNamesAt[role][1], name.Given()));
        CHECK(swd3etest::SameText(exe, kDefaultNamesAt[role][2], name.Nickname()));
        CHECK(swd3etest::SameText(exe, kNameTagsAt[role][0], tag.Surname()));
        CHECK(swd3etest::SameText(exe, kNameTagsAt[role][1], tag.Given()));
        CHECK(swd3etest::SameText(exe, kNameTagsAt[role][2], tag.Nickname()));
    }

    CHECK(party.names[0].FullName() + party.names[0].Nickname() == swd3e::Big5(u8"陳靖仇阿仇"));
}

TEST_CASE("menu: the preview of a saved game")
{
    const std::u8string directory = swd3etest::GameDirectory() + u8"/Save/";
    const std::vector<u8> save = swd3etest::ReadGameFile("Save/0.sav");

    if (save.empty())
    {
        std::printf("  (Save/0.sav not found, skipped)\n");
        return;
    }

    swd3e::Surface surface(swd3e::Display::Width, swd3e::Display::Height);
    swd3e::Painter painter;
    painter.Init(surface);
    swd3e::SavePreview preview;
    swd3e::SaveSlots::LoadPreview(preview, 0, directory, painter);
    CHECK_EQ(preview.state, swd3e::SavePreview::Loaded);
    CHECK_EQ(preview.date.size(), swd3e::SaveSlots::DateTemplate.size());
    CHECK_EQ(preview.date[kYearDigits], swd3e::SaveSlots::DateTemplate[kYearDigits]);
    CHECK_EQ(preview.thumbnail.size(), swd3e::SaveSlots::ThumbnailPixels);
    CHECK(!preview.location.empty());
    CHECK(preview.roles[0].maxHitPoints != kAbsentRole);
    CHECK(preview.roles[0].level >= 1);
    CHECK(preview.names[0] != 0);
    CHECK(!preview.playTime.empty());
    swd3e::SavePreview missing;
    swd3e::SaveSlots::LoadPreview(missing, swd3e::SaveGames::LastSlot - 1, directory, painter);
    CHECK_EQ(missing.state, swd3e::SavePreview::Missing);
}
