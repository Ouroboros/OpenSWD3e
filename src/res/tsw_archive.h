#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include "core/types.h"
#include "file/file.h"
#include "pic/picture.h"
#include "text/big5.h"

namespace swd3e
{

class ErrorReporter;
class RleCodec;

// Directory record of a tsw file: 0x2C bytes at 0x1C + 0x2C * (index - 1).
struct TswDirectoryEntry
{
    std::array<char, 0x18> name = {};
    u32 offset = 0;
    std::array<u8, 0x10> unknown1C = {};

    // The name up to its NUL.
    std::string_view Name() const
    {
        const std::string_view all(this->name.data(), this->name.size());
        return all.substr(0, all.find('\0'));
    }
};

static_assert(sizeof(TswDirectoryEntry) == 0x2C);

// Frame record of an entry (0x24 bytes after the 12-byte entry header and the palette of 8-bit entries).
struct TswFrameEntry
{
    u32 offset = 0;
    u32 packedSize = 0;
    u32 size = 0;
    u32 flag = 0;
    std::array<u32, 4> unknown10 = {};
    u16 width = 0;
    u16 height = 0;
};

static_assert(sizeof(TswFrameEntry) == 0x24);

// Pictures of the map units come from the map module (picture id 0xFFFF, the frame is the unit).
class MapPictureSource
{
public:
    virtual ~MapPictureSource() = default;
    virtual bool LoadMapPicture(int unit, Picture& picture) = 0;
};

// The tsw picture archives (0x004364C0-0x004386B0): six files addressed by id / 3000 (character, item, magic,
// system, two map sets), opened once at start with the offsets of all their directory records read into one table,
// a byte-limited picture cache in sixteen chains and a direct load for callers that keep the picture.
class TswArchive
{
public:
    static constexpr std::array<std::string_view, 6> FileNames = {
        "all_char.tsw",
        "all_item.tsw",
        "all_magic.tsw",
        "all_sys.tsw",
        "all_map1.tsw",
        "all_map2.tsw",
    };

    // The file format: the directory of TswDirectoryEntry records, then entries of a header (a dword, the magic, the
    // frame count, the format), the palette of 8-bit entries and the TswFrameEntry records.
    static constexpr u32 DirectoryStart = 0x1C;
    static constexpr std::size_t MagicAt = 4;
    static constexpr std::size_t FormatAt = 8;
    static constexpr std::size_t EntryHeaderBytes = 12;
    static constexpr u16 EntryMagic = 0xABCD;
    static constexpr u16 PaletteFormat = 8;
    static constexpr u32 PaletteColors = 0x100;
    static constexpr int FileCount = 6;
    static constexpr int IndicesPerFile = 3000;
    static constexpr int Chains = 16;
    static constexpr u16 ChainMask = 0xF;
    static constexpr u16 MapPicture = 0xFFFF;
    // ReadHeader reads this much of an entry at once (0x004F540C): the header, the palette and the frame records.
    static constexpr u32 HeaderReadBytes = 0x2E0C;
    // The offset table (0x004F82EC): the dword before the records of the six directories is the capacity of the
    // cache load's decompression buffer, which directory index 0 reads.
    static constexpr std::size_t OffsetTableSize = 1 + FileCount * IndicesPerFile;

    TswArchive(const RleCodec& codec, ErrorReporter& errors);

    void SetGameDirectory(std::u8string_view directory);
    void SetMapPictureSource(MapPictureSource* source);

    void SetCacheLimit(int bytes);
    void OpenArchives();
    Picture* GetPicture(u32 id, u32 frame);
    std::unique_ptr<Picture> LoadPictureDirect(u32 id, u32 frame);
    void Flush();
    int CloseAll();
    Picture* FindPicture(u32 id, u32 frame);
    int CacheBytes() const;
    // The rebuild's smooth battle: the id and frame of a picture in the cache; false for one outside it.
    bool Locate(const Picture* picture, u32& id, u32& frame) const;
    // The same for a picture of the cache or the kept ones by its pixel data (Painter::SetSource), with the picture;
    // `kept` tells which.
    const Picture* LocateData(const void* data, u32& id, u32& frame, bool& kept) const;
    // The same by its palette (the argument PaletteArgument gives).
    const Picture* LocatePalette(const void* palette, u32& id, u32& frame, bool& kept) const;
    // The rebuild: the load of LoadPictureDirect with its picture kept apart from the cache, so the smooth battle can
    // draw it again; every call loads as LoadPictureDirect does.
    const Picture* LoadPictureKept(u32 id, u32 frame);
    // The kept picture of id and frame; nullptr once it is let go.
    const Picture* FindKept(u32 id, u32 frame);
    // The rebuild's smooth battle: the cached picture of id and frame without reading or reordering the cache; nullptr
    // once it is evicted.
    const Picture* Peek(u32 id, u32 frame) const;

private:
    // What ReadHeader and ReadFrame give of an entry (the 0x18-byte local of 0x004380F0).
    struct FrameInfo
    {
        std::vector<u8> data;
        std::vector<u16> palette;
        u16 format = 0;
        u16 width = 0;
        u16 height = 0;
    };

    struct CacheEntry
    {
        u16 id = 0;
        u16 frame = 0;
        Picture picture;
        u8 tag = 0;  // node + 0x1E: CacheTag at the load
    };

    // A decompression buffer that grows to the largest packed frame; a failed decompression releases it while its
    // capacity stays, so the following loads of frames no larger fail until a larger one reallocates it. The
    // capacity of the cache load's buffer is offsets[0].
    struct PackedBuffer
    {
        std::vector<u8> bytes;
        bool present = false;
    };

    bool CacheLoad(u32 id, u32 frame);
    Picture* CacheFind(u32 id, u32 frame);
    void CacheEvict();
    void CacheClear();
    bool OpenFile(File& file, const std::u8string& name);
    bool ReadHeader(File& file, FrameInfo& info, TswFrameEntry& record, u32 frame);
    bool LoadPicture(File& file, int id, u32 frame, Picture& picture);
    bool ReadFrame(
        PackedBuffer& buffer,
        u32& capacity,
        File& file,
        u32 base,
        FrameInfo& info,
        const TswFrameEntry& record
    );
    bool LoadPictureB(File& file, int id, u32 frame, Picture& picture);
    u8 CacheTag() const;
    void FreeBuffers();
    u32 DirectoryOffset(File& file, int id);
    const Picture* LocateMatch(
        const std::function<bool(const Picture&)>& matches,
        u32& id,
        u32& frame,
        bool& kept
    ) const;
    File* ArchiveOf(u32 id);

    const RleCodec& codec;
    MapPictureSource* mapPictures = nullptr;
    std::u8string gameDirectory;  // 0x004F4BB4
    std::array<File, FileCount> files;
    bool directoryRead = false;  // 0x004F5408
    std::array<u32, OffsetTableSize> offsets{};
    int cacheLimit = 0x600000;  // 0x004B705C
    u32 cacheBytes = 0;         // 0x004F4B7C
    std::array<std::list<CacheEntry>, Chains> chains;
    std::array<u16, Chains> chainCounts{};  // 0x004F4B5C
    Picture* current = nullptr;             // 0x004F4B80
    u8 tag = 0;                             // 0x004F5404, never written
    std::array<u8, HeaderReadBytes> header{};
    std::vector<u16> palette;  // 0x00509C34, shared by every 8-bit entry read
    PackedBuffer packed;       // 0x00509E78, capacity offsets[0] (0x004F82EC)
    PackedBuffer packedB;      // 0x00509E74
    u32 capacityB = 0;         // 0x00509C30
    Picture direct;            // 0x004F4BA0
    std::list<CacheEntry> kept;
};

}  // namespace swd3e
