#include <cstdio>
#include <vector>

#include "game/cheat_dialog.h"
#include "game_data.h"
#include "test.h"

TEST_CASE("cheat dialog: the attribute names match swd32.exe")
{
    constexpr swd3e::u32 kAttributeNamesAt = 0x0049FC14;
    constexpr swd3e::u32 kNameBytes = 8;
    const std::vector<swd3e::u8> exe = swd3etest::ReadGameFile("swd32.exe");

    if (exe.empty())
    {
        std::printf("  (swd32.exe not found, skipped)\n");
        return;
    }

    for (std::size_t index = 0; index < swd3e::CheatDialog::AttributeNames.size(); ++index)
    {
        const auto at = kAttributeNamesAt + static_cast<swd3e::u32>(index) * kNameBytes;
        CHECK(swd3etest::SameText(exe, at, swd3e::CheatDialog::AttributeNames[index]));
    }
}
