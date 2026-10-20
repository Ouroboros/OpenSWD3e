#include <cstdio>
#include <fstream>
#include <functional>
#include <map>
#include <string>

#include "core/types.h"
#include "draw/painter.h"
#include "game_data.h"
#include "golden/tsw_scenarios.h"
#include "golden_check.h"
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

golden::PictureResult Result(const swd3e::Picture* picture)
{
    golden::PictureResult result;

    if (picture == nullptr)
    {
        return result;
    }

    result.present = true;
    result.data = picture->data;
    result.width = picture->width;
    result.height = picture->height;
    result.paletteEntries = picture->palette.size();
    return result;
}

class ArchiveBackend : public golden::TswBackend
{
public:
    explicit ArchiveBackend(swd3e::TswArchive& archive)
        : archive(archive)
    {
    }

    void SetCacheLimit(int bytes) override
    {
        this->archive.SetCacheLimit(bytes);
    }

    golden::PictureResult Get(u32 id, u32 frame) override
    {
        return Result(this->archive.GetPicture(id, frame));
    }

    golden::PictureResult Find(u32 id, u32 frame) override
    {
        return Result(this->archive.FindPicture(id, frame));
    }

    golden::PictureResult Direct(u32 id, u32 frame) override
    {
        const auto picture = this->archive.LoadPictureDirect(id, frame);
        return Result(picture.get());
    }

    int CacheBytes() override
    {
        return this->archive.CacheBytes();
    }

    void CloseAll() override
    {
        this->archive.CloseAll();
    }

private:
    swd3e::TswArchive& archive;
};

// all_sys.tsw entry 24, a picture of a single frame.
constexpr std::size_t kSysFile = 3;
static_assert(swd3e::TswArchive::FileNames[kSysFile] == "all_sys.tsw");
constexpr u32 kSingleFrame = kSysFile * swd3e::TswArchive::IndicesPerFile + 24;

}  // namespace

TEST_CASE("tsw: matches the golden results of the legacy archives")
{
    const std::map<std::string, std::string> golden =
        swd3etest::LoadGolden(SWD3_SOURCE_DIR "/tests/golden/tsw_golden.txt");

    if (golden.empty() || swd3etest::ReadGameFile("all_sys.act").empty())
    {
        std::printf("  (game data or golden file not found, skipped)\n");
        return;
    }

    swd3e::Surface screen(swd3e::Display::Width, swd3e::Display::Height);
    swd3e::Painter painter;
    painter.Init(screen);
    const swd3e::RleCodec codec(painter);
    swd3etest::RecordingReporter errors;
    swd3e::TswArchive archive(codec, errors);
    archive.SetGameDirectory(swd3etest::GameDirectory());
    ArchiveBackend backend(archive);
    swd3etest::GoldenCheck check(golden);
    golden::RunTsw(backend, std::ref(check));
    std::printf("  compared %d scenarios\n", check.compared);
    CHECK_EQ(check.mismatches, 0);
    CHECK_EQ(check.compared, static_cast<int>(golden.size()));
}

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
