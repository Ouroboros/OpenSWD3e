#pragma once

#include <functional>
#include <span>
#include <string>
#include <vector>

#include "core/types.h"
#include "file/file.h"

namespace swd3e
{

class Config;
class Painter;
struct GameSettings;
struct MapHeader;
class ErrorReporter;

// The cache of decompressed map units in <install directory>data\0.cm .. 23.cm with their table in mcache.dat. The
// session mark ([game] session_open; the last byte of Env.dat in the original) marks a session that did not end
// cleanly: the next start empties the cache.
class MapCache
{
public:
    static constexpr int Units = 0x18;
    static constexpr u32 TableBytes = 0x180;

    // MapCache::Check modes.
    static constexpr int CheckSessionMark = 0;
    static constexpr int CheckColorMasks = 1;

    // One entry of mcache.dat (0x10 bytes).
    struct Slot
    {
        static constexpr u32 Free = 0xFFFFFFFF;

        u32 map = Free;  // the cached map
        u32 bytes = 0;
        u32 age = 0;
        u32 unit = 0;  // the .cm file
    };

    static constexpr u32 HeaderBytes = 0x1A8;

    MapCache(const Config& config, const Painter& painter, const MapHeader& map, ErrorReporter& errors);

    u32 Check(int mode);
    void MarkClean();
    void MarkDirty();
    u32 CacheBytes() const;
    // `legacy`: the unit has the 0x1A8-byte header of swd32's maps (swd3e's loader always passes 0).
    const u8* GetCachedMap(u32 map, u32 base, u32 size, int legacy);
    void DecompressMap(int unit, u32 base, u32 size, int legacy, u32& bytes);
    void ReadUnitBytes(u32 base, u32 size, int legacy, u32& bytes);
    const u8* OpenUnit(int unit);
    static std::vector<u8> Pack(std::span<const u8> data);
    static std::size_t Unpack(std::span<const u8> packed, std::span<u8> output);

    File lmf;                           // 0x004D1DB0 HUGE.lmf
    File unitFile;                      // 0x004E6830 the mapped unit
    const u8* view = nullptr;           // the mapped view of unitFile (the map header's pixels)
    std::u8string unitPath;             // swd32 0x004A9ACC
    std::function<void(int)> progress;  // the loading screen at a percentage

private:
    void Clear();
    void Store(const std::function<void(GameSettings&)>& change) const;
    void DecompressMsf3(File& cached, u32 base, u32& bytes);

    const Config& config;
    const Painter& painter;
    const MapHeader& map;  // 0x004D1298 its colour depth (0x004D12A0) decides the pixel conversion
    ErrorReporter& errors;
};

}  // namespace swd3e
