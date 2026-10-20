#include "res/tsw_archive.h"

#include <algorithm>
#include <cstring>
#include <new>

#include "codec/lzo.h"
#include "core/format.h"
#include "core/words.h"
#include "memmgr/heap_block.h"
#include "pic/rle.h"

namespace swd3e
{

namespace
{

// LoadPictureKept: the pictures kept, enough for the direct pictures of a battle frame and an animation.
constexpr std::size_t kKeptPictures = 32;

u16 ReadWord(const u8* bytes)
{
    u16 value = 0;
    std::memcpy(&value, bytes, sizeof(value));
    return value;
}

// Bucket of the cache chains: by id, map pictures by unit.
int ChainOf(u32 id, u32 frame)
{
    return LowWord(id) == TswArchive::MapPicture ? static_cast<int>(LowWord(frame) & TswArchive::ChainMask)
                                                 : static_cast<int>(LowWord(id) & TswArchive::ChainMask);
}

}  // namespace

TswArchive::TswArchive(const RleCodec& codec, [[maybe_unused]] ErrorReporter& errors)
    : codec(codec)
{
}

void TswArchive::SetMapPictureSource(MapPictureSource* source)
{
    this->mapPictures = source;
}

int TswArchive::CacheBytes() const
{
    return static_cast<int>(this->cacheBytes);
}

// swd3e.exe 0x004364C0
void TswArchive::SetCacheLimit(int bytes)
{
    this->cacheLimit = bytes;
}

// swd3e.exe 0x004364D0: the directory of the archives with a backslash appended; an empty one leaves it unchanged
void TswArchive::SetGameDirectory(std::u8string_view directory)
{
    if (!directory.empty())
    {
        this->gameDirectory = std::u8string(directory) + u8"\\";
    }
}

// swd3e.exe 0x00436510: opens the six archives and reads the offset of every directory record into the table. A
// record that cannot be read (an archive missing) leaves the offset of the record before it; the first such record
// takes 0 where the original has its uninitialised stack buffer (see deviations.md).
void TswArchive::OpenArchives()
{
    for (int i = 0; i < TswArchive::FileCount; ++i)
    {
        this->OpenFile(
            this->files[static_cast<std::size_t>(i)],
            PathFormat("%s%s", {this->gameDirectory, TswArchive::FileNames[static_cast<std::size_t>(i)]})
        );
    }

    TswDirectoryEntry record;
    std::size_t slot = 1;

    for (File& file : this->files)
    {
        for (int index = 0; index < TswArchive::IndicesPerFile; ++index)
        {
            file.Seek(
                File::Origin::Begin,
                static_cast<i32>(TswArchive::DirectoryStart + sizeof(TswDirectoryEntry) * static_cast<u32>(index))
            );
            u32 bytes = sizeof(record);
            file.Read(&record, bytes);
            this->offsets[slot++] = record.offset;
        }
    }

    this->directoryRead = true;
}

File* TswArchive::ArchiveOf(u32 id)
{
    const auto index = static_cast<std::size_t>(LowWord(id) / TswArchive::IndicesPerFile);
    // Ids from 18000 on index past the six handles in the original; they find no archive here (see deviations.md).
    return index < this->files.size() ? &this->files[index] : nullptr;
}

// swd3e.exe 0x00436720: the cached picture, loaded on a miss after the cache is trimmed once it holds the limit;
// nullptr when it cannot be loaded
Picture* TswArchive::GetPicture(u32 id, u32 frame)
{
    if (this->CacheFind(id, frame) == nullptr)
    {
        if (this->cacheBytes >= static_cast<u32>(this->cacheLimit))
        {
            this->CacheEvict();
        }

        if (!this->CacheLoad(id, frame))
        {
            this->current = nullptr;
        }
    }

    return this->current;
}

// swd3e.exe 0x00436770: a picture outside the cache, owned by the caller: loaded into one static picture whose copy is
// returned. The load result is not checked: a failed load returns the picture of the last direct load again (the
// original returns the same block, freed by its first owner, see deviations.md).
std::unique_ptr<Picture> TswArchive::LoadPictureDirect(u32 id, u32 frame)
{
    if (File* archive = this->ArchiveOf(id))
    {
        this->LoadPictureB(*archive, static_cast<int>(LowWord(id)), LowWord(frame), this->direct);
    }

    this->codec.ConvertTo16(this->direct);
    return std::make_unique<Picture>(this->direct);
}

const Picture* TswArchive::LoadPictureKept(u32 id, u32 frame)
{
    std::unique_ptr<Picture> picture = this->LoadPictureDirect(id, frame);

    for (auto entry = this->kept.begin(); entry != this->kept.end(); ++entry)
    {
        if (entry->id == LowWord(id) && entry->frame == LowWord(frame))
        {
            this->kept.erase(entry);
            break;
        }
    }

    this->kept.push_front(CacheEntry{.id = LowWord(id), .frame = LowWord(frame), .picture = std::move(*picture)});

    if (this->kept.size() > kKeptPictures)
    {
        this->kept.pop_back();
    }

    return &this->kept.front().picture;
}

// swd3e.exe 0x004367D0: empties the cache and releases the decompression buffers (the archives stay open)
void TswArchive::Flush()
{
    this->CacheClear();
    this->FreeBuffers();
}

// swd3e.exe 0x004366B0: closes the archives, marks the offset table unread and empties the cache. The original also
// closes a handle no code opens (0x004F4FB4).
int TswArchive::CloseAll()
{
    for (File& file : this->files)
    {
        file.Close();
    }

    this->directoryRead = false;
    this->CacheClear();
    this->FreeBuffers();
    this->kept.clear();
    return 0;
}

// swd3e.exe 0x004367E0: the cached picture without loading
Picture* TswArchive::FindPicture(u32 id, u32 frame)
{
    this->current = nullptr;
    this->CacheFind(id, frame);
    return this->current;
}

// swd3e.exe 0x00436810: a new entry goes to the front of its chain before loading; a failed load leaves it there
// with an empty picture (its count stays). Map pictures come from the map module. The picture size counts towards
// the limit.
bool TswArchive::CacheLoad(u32 id, u32 frame)
{
    const int chain = ChainOf(id, frame);
    ++this->chainCounts[static_cast<std::size_t>(chain)];
    std::list<CacheEntry>& entries = this->chains[static_cast<std::size_t>(chain)];
    entries.push_front(
        CacheEntry{.id = LowWord(id), .frame = LowWord(frame), .picture = Picture{}, .tag = this->CacheTag()}
    );
    Picture& picture = entries.front().picture;

    if (LowWord(id) == TswArchive::MapPicture)
    {
        if (this->mapPictures != nullptr)
        {
            this->mapPictures->LoadMapPicture(static_cast<int>(LowWord(frame)), picture);
        }
    }
    else
    {
        File* archive = this->ArchiveOf(id);

        if (archive == nullptr || !this->LoadPicture(*archive, static_cast<int>(LowWord(id)), LowWord(frame), picture))
        {
            return false;
        }

        this->codec.ConvertTo16(picture);
    }

    this->cacheBytes += static_cast<u32>(picture.data.size());
    this->current = &picture;
    return true;
}

// swd3e.exe 0x00436950: a found entry moves to the front of its chain
Picture* TswArchive::CacheFind(u32 id, u32 frame)
{
    this->current = nullptr;
    std::list<CacheEntry>& entries = this->chains[static_cast<std::size_t>(ChainOf(id, frame))];

    for (auto entry = entries.begin(); entry != entries.end(); ++entry)
    {
        if (entry->id == LowWord(id) && entry->frame == LowWord(frame))
        {
            entries.splice(entries.begin(), entries, entry);
            this->current = &entries.front().picture;
            return this->current;
        }
    }

    return nullptr;
}

// swd3e.exe 0x004369F0: drops entries from the end of the longest chain (the first of equal ones) until the cache is
// below its limit or that chain is empty
void TswArchive::CacheEvict()
{
    std::size_t longest = 0;

    for (std::size_t i = 0; i < this->chainCounts.size(); ++i)
    {
        if (this->chainCounts[i] > this->chainCounts[longest])
        {
            longest = i;
        }
    }

    std::list<CacheEntry>& entries = this->chains[longest];

    while (this->cacheBytes >= static_cast<u32>(this->cacheLimit))
    {
        if (!entries.empty())
        {
            this->cacheBytes -= static_cast<u32>(entries.back().picture.data.size());
            entries.pop_back();
        }

        if (--this->chainCounts[longest] == 0)
        {
            break;
        }
    }
}

// swd3e.exe 0x00436AC0
void TswArchive::CacheClear()
{
    for (std::size_t i = 0; i < this->chains.size(); ++i)
    {
        for (const CacheEntry& entry : this->chains[i])
        {
            this->cacheBytes -= static_cast<u32>(entry.picture.data.size());
            --this->chainCounts[i];
        }

        this->chains[i].clear();
    }
}

// swd3e.exe 0x00438010
u8 TswArchive::CacheTag() const
{
    return this->tag;
}

// swd3e.exe 0x00438020: read-only, shared for reading
bool TswArchive::OpenFile(File& file, const std::u8string& name)
{
    return file.Open(name, File::Disposition::OpenExisting, File::Access::Read);
}

// swd3e.exe 0x00438050: reads 0x2E0C bytes of the entry into the static header buffer (a short read keeps the rest
// of the previous entry), checks the magic and copies frame record `frame` out of it; an 8-bit entry's palette goes
// to the palette shared by all entries. Records past the buffer read the data after it in the original; they read 0
// here (see deviations.md).
bool TswArchive::ReadHeader(File& file, FrameInfo& info, TswFrameEntry& record, u32 frame)
{
    u32 bytes = TswArchive::HeaderReadBytes;
    file.Read(this->header.data(), bytes);
    const u16 format = ReadWord(this->header.data() + TswArchive::FormatAt);

    if (ReadWord(this->header.data() + TswArchive::MagicAt) != TswArchive::EntryMagic)
    {
        return false;
    }

    info.format = format;
    std::size_t frames = TswArchive::EntryHeaderBytes;

    if (format == TswArchive::PaletteFormat)
    {
        this->palette.assign(TswArchive::PaletteColors, 0);
        std::memcpy(this->palette.data(), this->header.data() + frames, TswArchive::PaletteColors * sizeof(u16));
        frames += TswArchive::PaletteColors * sizeof(u16);
        info.palette = this->palette;
    }

    const u64 at = frames + static_cast<u64>(frame) * sizeof(TswFrameEntry);
    record = TswFrameEntry{};

    if (at < this->header.size())
    {
        const std::size_t count =
            std::min<std::size_t>(sizeof(record), this->header.size() - static_cast<std::size_t>(at));
        std::memcpy(&record, this->header.data() + at, count);
    }

    return true;
}

// The directory offset of a picture (inline in 0x004380F0 and 0x00438320): from the table once it is read, at the
// full id (the record before the id's own, so the first index of a file reads the last record of the file before it
// and id 0 the decompression buffer's capacity); else read from the archive at the record of the full id, which past
// the first file lies beyond the directory. A record that cannot be read gives 0 where the original has its
// uninitialised stack buffer (see deviations.md).
u32 TswArchive::DirectoryOffset(File& file, int id)
{
    if (this->directoryRead)
    {
        const auto index = static_cast<std::size_t>(id);

        if (index < this->offsets.size())
        {
            return this->offsets[index];
        }

        // The dword after the table is the direct load's buffer capacity; ids beyond read other data in the original.
        return index == this->offsets.size() ? this->capacityB : 0;
    }

    TswDirectoryEntry record;
    file.Seek(File::Origin::Begin, static_cast<i32>(sizeof(TswDirectoryEntry) * static_cast<u32>(id) - 0x10));
    u32 bytes = sizeof(record);
    file.Read(&record, bytes);
    return record.offset;
}

// swd3e.exe 0x004380F0: a cached picture: the entry header, then the frame. Nothing is reported on failure.
bool TswArchive::LoadPicture(File& file, int id, u32 frame, Picture& picture)
{
    FrameInfo info;
    TswFrameEntry record;
    const u32 offset = this->DirectoryOffset(file, id);
    file.Seek(File::Origin::Begin, static_cast<i32>(offset));

    if (!this->ReadHeader(file, info, record, frame) ||
        !this->ReadFrame(this->packed, this->offsets[0], file, offset, info, record))
    {
        return false;
    }

    picture.data = std::move(info.data);
    picture.palette = std::move(info.palette);
    picture.width = info.width;
    picture.height = info.height;
    return true;
}

// swd3e.exe 0x00438220: with the cache load's buffer
// swd3e.exe 0x00438450: with the direct load's buffer
// Reads the packed frame into the buffer, grown to it, and decompresses it to its stored size; a size of 0 or a
// decompressed length other than it fails (the buffer is let go on the latter, its capacity kept).
bool TswArchive::ReadFrame(
    PackedBuffer& buffer,
    u32& capacity,
    File& file,
    u32 base,
    FrameInfo& info,
    const TswFrameEntry& record
)
{
    file.Seek(File::Origin::Begin, static_cast<i32>(base + record.offset));
    const u32 packedSize = record.packedSize;

    if (packedSize > capacity)
    {
        HeapBlock::Free(buffer.bytes);
        buffer.present = HeapBlock::Allocate(buffer.bytes, packedSize);
        capacity = packedSize;
    }

    if (buffer.present)
    {
        u32 bytes = packedSize;
        file.Read(buffer.bytes.data(), bytes);
    }

    std::vector<u8> out;

    if (!HeapBlock::Allocate(out, record.size) || !buffer.present)
    {
        return false;
    }

    LzoDecoder decoder({buffer.bytes.data(), packedSize}, out);
    decoder.Run();

    if (decoder.Written() != record.size)
    {
        HeapBlock::Free(buffer.bytes);
        buffer.present = false;
        return false;
    }

    // The picture's size field is the stored size; the block's rounding is not part of the picture.
    out.resize(record.size);
    info.data = std::move(out);
    info.width = record.width;
    info.height = record.height;
    return true;
}

// swd3e.exe 0x00438320: the direct-load variant of 0x004380F0 with its own decompression buffer
bool TswArchive::LoadPictureB(File& file, int id, u32 frame, Picture& picture)
{
    FrameInfo info;
    TswFrameEntry record;
    const u32 offset = this->DirectoryOffset(file, id);
    file.Seek(File::Origin::Begin, static_cast<i32>(offset));

    if (!this->ReadHeader(file, info, record, frame) ||
        !this->ReadFrame(this->packedB, this->capacityB, file, offset, info, record))
    {
        return false;
    }

    picture.data = std::move(info.data);
    picture.palette = std::move(info.palette);
    picture.width = info.width;
    picture.height = info.height;
    return true;
}

// swd3e.exe 0x004386B0
void TswArchive::FreeBuffers()
{
    this->packed = PackedBuffer{};
    this->packedB = PackedBuffer{};
    this->offsets[0] = 0;
    this->capacityB = 0;
}

bool TswArchive::Locate(const Picture* picture, u32& id, u32& frame) const
{
    for (const std::list<CacheEntry>& entries : this->chains)
    {
        for (const CacheEntry& entry : entries)
        {
            if (&entry.picture == picture)
            {
                id = entry.id;
                frame = entry.frame;
                return true;
            }
        }
    }

    return false;
}

const Picture* TswArchive::LocateData(const void* data, u32& id, u32& frame, bool& kept) const
{
    return this->LocateMatch(
        [data](const Picture& picture)
        {
            return picture.data.data() == data;
        },
        id,
        frame,
        kept
    );
}

const Picture* TswArchive::LocatePalette(const void* palette, u32& id, u32& frame, bool& kept) const
{
    return this->LocateMatch(
        [palette](const Picture& picture)
        {
            return !picture.palette.empty() && picture.palette.data() == palette;
        },
        id,
        frame,
        kept
    );
}

const Picture* TswArchive::LocateMatch(
    const std::function<bool(const Picture&)>& matches,
    u32& id,
    u32& frame,
    bool& kept
) const
{
    kept = false;

    for (const std::list<CacheEntry>& entries : this->chains)
    {
        for (const CacheEntry& entry : entries)
        {
            if (matches(entry.picture))
            {
                id = entry.id;
                frame = entry.frame;
                return &entry.picture;
            }
        }
    }

    for (const CacheEntry& entry : this->kept)
    {
        if (matches(entry.picture))
        {
            id = entry.id;
            frame = entry.frame;
            kept = true;
            return &entry.picture;
        }
    }

    return nullptr;
}

const Picture* TswArchive::FindKept(u32 id, u32 frame)
{
    for (auto entry = this->kept.begin(); entry != this->kept.end(); ++entry)
    {
        if (entry->id == LowWord(id) && entry->frame == LowWord(frame))
        {
            this->kept.splice(this->kept.begin(), this->kept, entry);
            return &this->kept.front().picture;
        }
    }

    return nullptr;
}

const Picture* TswArchive::Peek(u32 id, u32 frame) const
{
    for (const CacheEntry& entry : this->chains[static_cast<std::size_t>(ChainOf(id, frame))])
    {
        if (entry.id == LowWord(id) && entry.frame == LowWord(frame))
        {
            return &entry.picture;
        }
    }

    return nullptr;
}

}  // namespace swd3e
