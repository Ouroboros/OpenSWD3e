#include <array>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <utility>
#include <vector>

#include "game_data.h"
#include "talk/talk.h"
#include "test.h"

using swd3e::u8;

namespace
{

constexpr swd3e::u32 kWindowShiftXAt = 0x004AA4C4;  // where swd3e.exe keeps the window shift tables
constexpr swd3e::u32 kWindowShiftYAt = 0x004AA4E4;

constexpr int kUntouched = 7;
constexpr std::array<std::pair<std::string_view, int>, 4> kNumbers = {{
    {"123", 123},
    {"-45", -45},
    {"+9", 9},
    {"999999999", 999999999},  // the longest number taken
}};

}  // namespace

TEST_CASE("talk: numbers of the sound code")
{
    int value = kUntouched;

    for (const auto& [text, number] : kNumbers)
    {
        CHECK(swd3e::Talk::ParseNumber(text, value));
        CHECK_EQ(value, number);
    }

    value = kUntouched;
    CHECK(!swd3e::Talk::ParseNumber("1234567890", value));
    CHECK(!swd3e::Talk::ParseNumber("12a", value));
    CHECK_EQ(value, kUntouched);
    CHECK(swd3e::Talk::ParseNumber("", value));
    CHECK_EQ(value, 0);
}

TEST_CASE("talk: window shift tables match swd3e.exe")
{
    const std::vector<u8> exe = swd3etest::ReadGameFile("swd3e.exe");

    if (exe.empty())
    {
        std::printf("  (swd3e.exe not found, skipped)\n");
        return;
    }

    CHECK(swd3etest::SameTable(exe, kWindowShiftXAt, swd3e::Talk::WindowShiftX));
    CHECK(swd3etest::SameTable(exe, kWindowShiftYAt, swd3e::Talk::WindowShiftY));
}
