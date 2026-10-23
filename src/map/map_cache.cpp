#include "map/map_cache.h"

#include <algorithm>
#include <array>
#include <functional>
#include <limits>
#include <string>

#include "codec/lzo.h"
#include "config/config.h"
#include "core/byte_reader.h"
#include "core/debug_log.h"
#include "core/error_box.h"
#include "core/format.h"
#include "core/log.h"
#include "draw/painter.h"
#include "file/file.h"
#include "game/game_state.h"

namespace swd3e
{

namespace
{

constexpr std::string_view kUnitFormat = "%sdata\\%d.cm";
constexpr std::string_view kTableFile = "mcache.dat";

}  // namespace

MapCache::MapCache(const Config& config, const Painter& painter, const MapHeader& map, ErrorReporter& errors)
    : config(config),
      painter(painter),
      map(map),
      errors(errors)
{
}

// swd3e.exe 0x004291B0: mode 0 empties the cache when the session mark is set (and clears it); mode 1 empties it when
// the display's colour masks differ from those recorded (and records the new ones). The original keeps both in Env.dat
// (the mark in its last byte, the masks at 0x1E); the rebuild in [game] (session_open, red_mask ...).
// The bytes the cache holds afterwards.
// swd3e.exe EH funclet (tail) of 0x004291B0, swd32 0x00498190 (EH unwind: the new object freed (operator delete))
// swd3e.exe EH funclet (tail) of 0x004291B0, swd32 0x0049819B (EH unwind: the new object freed (operator delete))
// swd3e.exe EH funclet (tail) of 0x004291B0, swd32 0x004981A6 (EH unwind: the new object freed (operator delete))
// swd3e.exe EH funclet (tail) of 0x004291B0, swd32 0x004981B1 (EH handler of 0x00425570)
u32 MapCache::Check(int mode)
{
    bool clear = false;

    if (mode == MapCache::CheckSessionMark)
    {
        this->Store(
            [&clear](GameSettings& game)
            {
                clear = game.sessionOpen.value_or(false);
                game.sessionOpen = false;
            }
        );
    }
    else if (mode == MapCache::CheckColorMasks)
    {
        u32 red = 0;
        u32 green = 0;
        u32 blue = 0;
        this->painter.ColorMasks(red, green, blue);
        const std::array<u16, Config::Channels>& recorded = this->config.channelMasks;

        if (red != recorded[Config::RedChannel] || green != recorded[Config::GreenChannel] ||
            blue != recorded[Config::BlueChannel])
        {
            clear = true;
            this->Store(
                [red, green, blue](GameSettings& game)
                {
                    game.redMask = static_cast<u16>(red);
                    game.greenMask = static_cast<u16>(green);
                    game.blueMask = static_cast<u16>(blue);
                }
            );
        }
    }

    if (clear)
    {
        this->Clear();
    }

    return this->CacheBytes();
}

// Empties every unit file and resets the table.
void MapCache::Clear()
{
    for (int unit = 0; unit < MapCache::Units; ++unit)
    {
        File file;

        if (file.Open(
                PathFormat(kUnitFormat, {this->config.installDirectory, unit}),
                File::Disposition::OpenExisting,
                File::Access::ReadWrite
            ))
        {
            file.SeekBegin(0);
            file.Truncate();
        }

        file.Close();
    }

    File table;

    if (table.Open(
            PathFormat("%sdata\\%s", {this->config.installDirectory, kTableFile}),
            File::Disposition::OpenExisting,
            File::Access::ReadWrite
        ))
    {
        std::array<Slot, MapCache::Units> slots;

        for (int unit = 0; unit < MapCache::Units; ++unit)
        {
            slots[static_cast<std::size_t>(unit)].unit = static_cast<u32>(unit);
        }

        u32 bytes = MapCache::TableBytes;
        table.SeekBegin(0);
        table.Truncate();
        table.Write(slots.data(), bytes);
    }

    table.Close();
}

// swd3e.exe 0x00429520: clears the session mark (at a clean exit)
// swd3e.exe EH funclet (tail) of 0x00429520, swd32 0x004981C0 (EH unwind: the new object freed (operator delete))
// swd3e.exe EH funclet (tail) of 0x00429520, swd32 0x004981CB (EH handler of 0x004258E0)
void MapCache::MarkClean()
{
    this->Store(
        [](GameSettings& game)
        {
            game.sessionOpen = false;
        }
    );
}

// swd3e.exe 0x004295F0: sets the session mark (once the game runs)
// swd3e.exe EH funclet (tail) of 0x004295F0, swd32 0x004981E0 (EH unwind: the new object freed (operator delete))
// swd3e.exe EH funclet (tail) of 0x004295F0, swd32 0x004981EB (EH handler of 0x004259B0)
void MapCache::MarkDirty()
{
    this->Store(
        [](GameSettings& game)
        {
            game.sessionOpen = true;
        }
    );
}

// [game] of the program's settings file changed by `change` (the original writes Env.dat; a failure is logged).
void MapCache::Store(const std::function<void(GameSettings&)>& change) const
{
    std::string error;

    if (!UpdateSettingsFile(
            this->config.settingsFile,
            [&change](Settings& settings)
            {
                change(settings.game);
            },
            error
        ))
    {
        LogWarning(u8"settings: the map cache's [game] values cannot be saved: %s", AsUtf8(error));
    }
}

// swd3e.exe 0x004296B0: the sizes of the unit files that exist
// swd3e.exe EH funclet (tail) of 0x004296B0, swd32 0x00498200 (EH unwind: the new object freed (operator delete))
// swd3e.exe EH funclet (tail) of 0x004296B0, swd32 0x0049820B (EH handler of 0x00425A70)
u32 MapCache::CacheBytes() const
{
    u32 total = 0;

    for (int unit = 0; unit < MapCache::Units; ++unit)
    {
        File file;

        if (file.Open(
                PathFormat(kUnitFormat, {this->config.installDirectory, unit}),
                File::Disposition::OpenExisting,
                File::Access::Read
            ))
        {
            total += file.Size();
        }

        file.Close();
    }

    return total;
}

namespace
{

constexpr int kTableSlots = 0x20;  // the 0x200-byte buffer the table is read into
constexpr u32 kKeptHeader = 0x10;
constexpr int kDictionaryBits = 14;

// The unit header of a map in HUGE.lmf: the unpacked size, the block size, then a length per block.
constexpr std::size_t kUnpackedAt = 0x10;
constexpr std::size_t kBlockSizeAt = 0x14;
constexpr std::size_t kBlocksAt = 0x1C;
constexpr std::size_t kBlockEntryBytes = 8;
// The MSF3 unit header (swd3e): the block size, the unpacked size, then from kMsf3BlocksAt the offset of each block
// from the map and its length.
constexpr u32 kMsf3HeaderBytes = 0x400;
constexpr std::size_t kMsf3BlockSizeAt = 4;
constexpr std::size_t kMsf3UnpackedAt = 8;
constexpr std::size_t kMsf3BlocksAt = 0x10;
constexpr std::u8string_view kCacheTitle = u8"地圖快取製作";   // 0x004B2278
constexpr std::u8string_view kOldFormat = u8"這地圖是舊格式";  // 0x004B2288
constexpr int kSlackShift = 10;                                // a block buffer has 1/1024 to spare for packed data
constexpr u32 kProgressStart = 15;  // the loading screen runs from 15 to 60 percent while unpacking
constexpr u32 kProgressSpan = 45;
constexpr int kMegabyteShift = 20;
constexpr i32 kAgeShare = 4;  // a slot older than a quarter of the cache size in megabytes goes first

u32 HeaderDword(const std::array<u8, MapCache::HeaderBytes>& header, std::size_t at)
{
    return ByteReader::Word32At(header, at);
}

}  // namespace

// swd3e.exe 0x00429BA0: 14-bit LZO1X-1 (the original packs into `size` + 0x20 bytes with a 64 KB dictionary)
std::vector<u8> MapCache::Pack(std::span<const u8> data)
{
    return LzoEncoder(kDictionaryBits).Compress(data);
}

// swd3e.exe 0x00429C00: the bytes it unpacked
std::size_t MapCache::Unpack(std::span<const u8> packed, std::span<u8> output)
{
    LzoDecoder decoder(packed, output);
    decoder.Run();
    return decoder.Written();
}

// swd3e.exe 0x0042A620: the unpacked size of a map's unit (the dword at +0x10 of a legacy header, at +8 of the MSF3
// one); a map without a unit is reported in the error box
void MapCache::ReadUnitBytes(u32 base, u32 size, int legacy, u32& bytes)
{
    if (size == 0)
    {
        ErrorBox(this->errors, kCacheTitle, kOldFormat);
        return;
    }

    this->lmf.SeekBegin(static_cast<i32>(base + size + (legacy != 0 ? kUnpackedAt : kMsf3UnpackedAt)));
    this->lmf.ReadDword(bytes);
}

// swd3e.exe 0x0042A180: unpacks the unit of a map from HUGE.lmf block by block into data\<unit>.cm (16-bit pixels
// converted to the display when the map is 16-bit), the loading screen from 15 to 60. A legacy unit (swd32's header:
// the block lengths in order, each block the block size but the last) is written block by block of the block size; an
// MSF3 unit (a 0x400-byte header: the block size, the unpacked size, then each block's offset from the map and length)
// writes what each block unpacks to, until exactly the unpacked size is written.
// swd3e.exe EH funclet (tail) of 0x0042A180, swd32 0x00498250 (EH unwind: the cFile, closed by its destructor)
// swd3e.exe EH funclet (tail) of 0x0042A180, swd32 0x00498258 (EH handler of 0x00426DF0)
void MapCache::DecompressMap(int unit, u32 base, u32 size, int legacy, u32& bytes)
{
    if (size == 0)
    {
        ErrorBox(this->errors, kCacheTitle, kOldFormat);
        return;
    }

    File cached;
    const std::u8string path = PathFormat(kUnitFormat, {this->config.installDirectory, unit});

    if (!cached.Open(path, File::Disposition::OpenAlways, File::Access::ReadWrite))
    {
        ErrorLog(u8"DecompressMap ,%s can't open", path);
        return;
    }

    cached.SeekBegin(0);
    this->lmf.SeekBegin(static_cast<i32>(size + base));

    if (legacy == 0)
    {
        this->DecompressMsf3(cached, base, bytes);
        return;
    }

    std::array<u8, MapCache::HeaderBytes> header = {};
    u32 read = MapCache::HeaderBytes;

    if (!this->lmf.Read(header.data(), read))
    {
        cached.Close();
        return;
    }

    const u32 total = HeaderDword(header, kUnpackedAt);
    bytes = total;
    const u32 blockSize = HeaderDword(header, kBlockSizeAt);

    if (blockSize == 0)
    {
        cached.Close();
        return;
    }

    std::vector<u8> packed((blockSize >> kSlackShift) + blockSize, 0);
    std::vector<u8> output((blockSize >> kSlackShift) + blockSize, 0);
    const u32 blocks = (blockSize + total) / blockSize;
    u32 remaining = total;
    u32 step = 0;

    for (u32 block = 0; block < blocks; ++block)
    {
        const std::size_t entry = kBlocksAt + kBlockEntryBytes * static_cast<std::size_t>(block);

        if (entry + sizeof(u32) > header.size())
        {
            break;
        }

        u32 length = HeaderDword(header, entry);
        packed.resize(std::max<std::size_t>(packed.size(), length));

        if (!this->lmf.Read(packed.data(), length))
        {
            ErrorLog(u8"DecompressMap ,read block fail when ");
            cached.Close();
            return;
        }

        MapCache::Unpack(std::span<const u8>(packed).first(length), output);
        u32 written = std::min(blockSize, remaining);

        if (this->map.bitsPerPixel == MapHeader::TrueColorBits)
        {
            this->painter.ConvertPixels(std::span<u16>(reinterpret_cast<u16*>(output.data()), written / sizeof(u16)));
        }

        cached.Write(output.data(), written);
        remaining -= blockSize;

        if (this->progress)
        {
            this->progress(static_cast<int>(step / blocks + kProgressStart));
        }

        step += kProgressSpan;
    }

    cached.Close();
}

// Part of 0x0042A180: the MSF3 unit, its header at the current position of HUGE.lmf, into `cached`. A block size of
// 0 or a block list past the header ends it (the original divides by it, or reads on past its buffer).
void MapCache::DecompressMsf3(File& cached, u32 base, u32& bytes)
{
    std::array<u8, kMsf3HeaderBytes> header = {};
    u32 read = kMsf3HeaderBytes;

    if (!this->lmf.Read(header.data(), read))
    {
        cached.Close();
        return;
    }

    const u32 blockSize = ByteReader::Word32At(header, kMsf3BlockSizeAt);
    const u32 total = ByteReader::Word32At(header, kMsf3UnpackedAt);
    bytes = total;
    std::vector<u8> packed((blockSize >> kSlackShift) + blockSize, 0);
    std::vector<u8> output((blockSize >> kSlackShift) + blockSize, 0);

    if (total == 0 || blockSize == 0)
    {
        cached.Close();
        return;
    }

    const u32 blocks = (blockSize + total) / blockSize;
    u32 written = 0;
    u32 done = 0;

    for (std::size_t entry = kMsf3BlocksAt; entry + kBlockEntryBytes <= header.size(); entry += kBlockEntryBytes)
    {
        u32 length = ByteReader::Word32At(header, entry + sizeof(u32));
        this->lmf.SeekBegin(static_cast<i32>(base + ByteReader::Word32At(header, entry)));
        packed.resize(std::max<std::size_t>(packed.size(), length));

        if (!this->lmf.Read(packed.data(), length))
        {
            ErrorLog(u8"DecompressMap ,read block fail when ");
            cached.Close();
            return;
        }

        const std::size_t produced = MapCache::Unpack(std::span<const u8>(packed).first(length), output);

        if (this->map.bitsPerPixel == MapHeader::TrueColorBits)
        {
            this->painter.ConvertPixels(std::span<u16>(reinterpret_cast<u16*>(output.data()), produced / sizeof(u16)));
        }

        auto count = static_cast<u32>(produced);
        cached.Write(output.data(), count);
        written += static_cast<u32>(produced);
        ++done;

        if (this->progress)
        {
            this->progress(static_cast<int>(kProgressSpan * done / blocks + kProgressStart));
        }

        if (written == total)
        {
            break;
        }
    }

    cached.Close();
}

// swd3e.exe 0x0042A670: maps data\<unit>.cm as the view of the map (the previous view released)
// swd3e.exe EH funclet (tail) of 0x0042A670, swd32 0x00498270 (EH unwind: the cFile, closed by its destructor)
// swd3e.exe EH funclet (tail) of 0x0042A670, swd32 0x00498278 (EH handler of 0x00427140)
const u8* MapCache::OpenUnit(int unit)
{
    const std::u8string path = PathFormat(kUnitFormat, {this->config.installDirectory, unit});
    this->unitFile.UnmapView(this->view);
    this->view = nullptr;
    this->unitFile.CloseMapping();
    this->unitFile.Close();
    this->unitPath.clear();

    if (!this->unitFile.Open(path, File::Disposition::OpenAlways, File::Access::Read))
    {
        ErrorLog(u8"GetCacheMapUnit ,can't open %s", path);
        return nullptr;
    }

    if (!this->unitFile.CreateMapping())
    {
        ErrorLog(u8"GetCacheMapUnit ,can't createmapping:%s ,", path);
    }

    this->view = this->unitFile.MapView(0, 0);

    if (this->view == nullptr)
    {
        ErrorLog(u8"GetCacheMapUnit ,can't createview:%s ,", path);
    }

    this->unitPath = path;
    return this->view;
}

// swd3e.exe 0x00429C20: the view of map `map` from the cache: every slot ages; a cached map is opened (its age back to
// 0); otherwise slots are emptied (an old one, else the smallest) while the cache would exceed its size or no slot is
// free, and the map is unpacked into the first free slot. An empty table is started with the map in slot 0.
// swd3e.exe EH funclet (tail) of 0x00429C20, swd32 0x00498220 (EH unwind: the cFile, closed by its destructor)
// swd3e.exe EH funclet (tail) of 0x00429C20, swd32 0x0049822B (EH unwind: the cFile, closed by its destructor)
// swd3e.exe EH funclet (tail) of 0x00429C20, swd32 0x00498233 (EH unwind: the cFile, closed by its destructor)
// swd3e.exe EH funclet (tail) of 0x00429C20, swd32 0x0049823E (EH handler of 0x00426840)
const u8* MapCache::GetCachedMap(u32 map, u32 base, u32 size, int legacy)
{
    File table;
    const std::u8string tablePath = PathFormat("%sdata\\%s", {this->config.installDirectory, kTableFile});

    if (!table.Open(tablePath, File::Disposition::OpenAlways, File::Access::ReadWrite))
    {
        ErrorLog(u8"GetCachedMap ,mcache.dat can't open");
        return nullptr;
    }

    std::array<Slot, kTableSlots> slots;
    u32 bytes = table.Size();

    if (bytes == 0)
    {
        u32 unpacked = 0;
        this->DecompressMap(0, base, size, legacy, unpacked);
        slots[0] = {map, unpacked, 0, 0};

        for (int unit = 1; unit < MapCache::Units; ++unit)
        {
            slots[static_cast<std::size_t>(unit)].unit = static_cast<u32>(unit);
        }

        u32 written = MapCache::TableBytes;
        table.SeekBegin(0);
        table.Write(slots.data(), written);
        table.Truncate();
        table.Close();
        return this->OpenUnit(0);
    }

    table.SeekBegin(0);
    u32 read = std::min<u32>(bytes, sizeof(slots));
    table.Read(slots.data(), read);
    const u32 count = bytes / sizeof(Slot);
    const u32 kept = std::min<u32>(count, kTableSlots);

    for (u32 slot = 0; slot < kept; ++slot)
    {
        ++slots[slot].age;
    }

    u32 used = 0;
    u32 found = 0;

    while (found < kept && slots[found].map != map)
    {
        used += slots[found].bytes;
        ++found;
    }

    if (found < count && found < kept)
    {
        const u8* opened = this->OpenUnit(static_cast<int>(slots[found].unit));
        slots[found].age = 0;
        table.SeekBegin(0);
        u32 written = sizeof(Slot) * kept;
        table.Write(slots.data(), written);
        table.Close();
        return opened;
    }

    u32 needed = 0;
    this->ReadUnitBytes(base, size, legacy, needed);
    bool full = true;

    for (u32 slot = 0; slot < kept; ++slot)
    {
        if (slots[slot].map == Slot::Free)
        {
            full = false;
            break;
        }
    }

    const auto limit = static_cast<u32>(this->config.cacheSize) << kMegabyteShift;
    u32 victim = count;
    u32 victimUnit = 0;

    while ((used + needed > limit && count != 0) || full)
    {
        u32 smallest = std::numeric_limits<u32>::max();

        for (u32 slot = 0; slot < kept; ++slot)
        {
            const Slot& entry = slots[slot];

            if (entry.map == Slot::Free)
            {
                continue;
            }

            if (entry.bytes < smallest)
            {
                smallest = entry.bytes;
                victimUnit = entry.unit;
                victim = slot;
            }

            if (entry.age > static_cast<u32>(this->config.cacheSize / kAgeShare))
            {
                smallest = entry.bytes;
                victimUnit = entry.unit;
                victim = slot;
                break;
            }
        }

        full = false;

        if (victim < kTableSlots)
        {
            slots[victim] = {Slot::Free, 0, 0, victim};
        }

        File cached;

        if (cached.Open(
                PathFormat(kUnitFormat, {this->config.installDirectory, static_cast<i32>(victimUnit)}),
                File::Disposition::OpenAlways,
                File::Access::ReadWrite
            ))
        {
            cached.SeekBegin(static_cast<i32>(kKeptHeader));
            cached.Truncate();
        }

        cached.Close();
        used -= smallest;
    }

    u32 free = 0;

    while (free < kept && slots[free].map != Slot::Free)
    {
        ++free;
    }

    if (free == count)
    {
        ErrorLog(u8"GetCachedMap ,No free slot");
    }

    if (free < kTableSlots)
    {
        slots[free] = {map, needed, 0, free};
    }

    table.SeekBegin(0);
    u32 written = sizeof(Slot) * kept;
    table.Write(slots.data(), written);
    table.Truncate();
    table.Close();
    this->DecompressMap(static_cast<int>(free), base, size, legacy, needed);
    return this->OpenUnit(static_cast<int>(free));
}

}  // namespace swd3e
