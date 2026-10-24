#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <map>
#include <string>

#include "core/types.h"
#include "game_data.h"
#include "recording_reporter.h"
#include "res/act_archive.h"
#include "test.h"

using swd3e::i32;
using swd3e::u16;
using swd3e::u64;

namespace
{

constexpr std::size_t kLeadWords = 2;  // the first words of a script go into the snapshot
constexpr std::size_t kStates = 8;

}  // namespace

TEST_CASE("act: the archives of swd3e give the same scripts cached and direct, and Run steps them")
{
    if (swd3etest::ReadGameFile("all_sys.act").empty())
    {
        std::printf("  (game data not found, skipped)\n");
        return;
    }

    constexpr int kSysFile = 3;
    constexpr int kScanned = 100;
    static_assert(swd3e::ActArchive::FileNames[kSysFile] == "all_sys.act");
    swd3etest::RecordingReporter errors;
    swd3e::ActArchive cached(errors);
    swd3e::ActArchive direct(errors);
    cached.SetGameDirectory(swd3etest::GameDirectory());
    direct.SetGameDirectory(swd3etest::GameDirectory());
    int found = 0;

    for (int index = 1; index <= kScanned; ++index)
    {
        const int id = kSysFile * swd3e::ActArchive::IndicesPerFile + index;
        const swd3e::ActScript fromCache = cached.Get(id, 0);
        const swd3e::ActScript loaded = direct.LoadDirect(id, 0);
        CHECK((fromCache == nullptr) == (loaded == nullptr));

        if (fromCache == nullptr || loaded == nullptr)
        {
            continue;
        }

        ++found;
        const std::size_t bytes = std::min(fromCache.size(), loaded.size());
        CHECK(bytes > 0);
        CHECK(std::memcmp(fromCache.data(), loaded.data(), bytes) == 0);
        swd3e::ActState state;
        state.id = id;
        CHECK_EQ(direct.Run(state), 1);
        CHECK(state.script != nullptr);
    }

    std::printf("  %d of %d entries of all_sys.act give a script\n", found, kScanned);
    CHECK(found > 0);
    CHECK_EQ(cached.CloseAll(), 0);
    CHECK_EQ(cached.CacheBytes(), 0);
    CHECK(errors.messages.empty());
}
