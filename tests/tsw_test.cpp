#include <cstdio>
#include <fstream>
#include <functional>
#include <map>
#include <string>

#include "core/types.h"
#include "draw/painter.h"
#include "game_data.h"
#include "pic/rle.h"
#include "platform/display.h"
#include "platform/surface.h"
#include "recording_reporter.h"
#include "res/tsw_archive.h"
#include "test.h"

using swd3e::u32;
using swd3e::u64;

namespace
{

// all_sys.tsw entry 24, a picture of a single frame.
constexpr std::size_t kSysFile = 3;
static_assert(swd3e::TswArchive::FileNames[kSysFile] == "all_sys.tsw");
constexpr u32 kSingleFrame = kSysFile * swd3e::TswArchive::IndicesPerFile + 24;

}  // namespace

TEST_CASE("tsw: the archives of swd3e load into the cache, the direct load and the flush")
{
    if (swd3etest::ReadGameFile("all_sys.tsw").empty())
    {
        std::printf("  (game data not found, skipped)\n");
        return;
    }

    constexpr u32 kScanned = 200;
    swd3e::Surface screen(swd3e::Display::Width, swd3e::Display::Height);
    swd3e::Painter painter;
    painter.Init(screen);
    const swd3e::RleCodec codec(painter);
    swd3etest::RecordingReporter errors;
    swd3e::TswArchive archive(codec, errors);
    archive.SetGameDirectory(swd3etest::GameDirectory());
    archive.OpenArchives();
    // Large enough that the scan evicts nothing.
    constexpr int kNoEviction = 0x40000000;
    archive.SetCacheLimit(kNoEviction);
    u64 bytes = 0;
    u32 loaded = 0;
    u32 first = 0;

    for (u32 index = 1; index <= kScanned; ++index)
    {
        const u32 id = kSysFile * swd3e::TswArchive::IndicesPerFile + index;
        const swd3e::Picture* picture = archive.GetPicture(id, 0);

        if (picture != nullptr)
        {
            CHECK(!picture->data.empty());
            CHECK(archive.FindPicture(id, 0) == picture);
            bytes += picture->data.size();
            first = loaded == 0 ? id : first;
            ++loaded;
        }
    }

    std::printf("  %u of %u pictures of all_sys.tsw loaded\n", loaded, kScanned);
    CHECK(loaded > 0);
    CHECK_EQ(static_cast<u64>(archive.CacheBytes()), bytes);

    if (loaded > 0)
    {
        const std::vector<swd3e::u8> cached = archive.FindPicture(first, 0)->data;
        const auto direct = archive.LoadPictureDirect(first, 0);
        CHECK(direct->data == cached);
    }

    archive.Flush();
    CHECK_EQ(archive.CacheBytes(), 0);
    CHECK(archive.FindPicture(first, 0) == nullptr);
    CHECK(errors.messages.empty());
}
