#include "map/map_scale.h"

#include <algorithm>

#include "core/byte_reader.h"
#include "game/game_state.h"
#include "pic/rle.h"

namespace swd3e
{

namespace
{

// SetScales: the kind, the count of row scales, then from kScalesAt the scales (dwords).
constexpr std::size_t kKindAt = 0;
constexpr std::size_t kCountAt = 4;
constexpr std::size_t kScalesAt = 0x10;

}  // namespace

// swd3e.exe 0x0042B1C0: no kind, the default scale; the constructor fills the levels with the default as dwords, so
// they alternate between 0x400 and 0 (its writes run on past the object into the globals after it, not repeated
// here: deviations.md)
MapScale::MapScale()
{
    this->unk00C.fill(this->defaultScale);

    for (std::size_t level = 0; level < this->levels.size(); ++level)
    {
        this->levels[level] = level % 2 == 0 ? static_cast<i16>(this->defaultScale) : i16{0};
    }
}

// swd3e.exe 0x0042B250 (the atexit destructor, which clears it)
MapScale::~MapScale()
{
    this->Clear();
}

// swd3e.exe 0x0042B260: the scales of `data`: kind Picture keeps 256 dwords nothing reads, kind Rows the scale of
// each tile row; another kind leaves none. Always false.
bool MapScale::SetScales(std::span<const u8> data)
{
    this->kind = Kind::None;
    const u32 kind = ByteReader::Word32At(data, kKindAt);

    if (kind == static_cast<u32>(Kind::Picture))
    {
        for (std::size_t index = 0; index < this->unk00C.size(); ++index)
        {
            this->unk00C[index] = static_cast<i32>(ByteReader::Word32At(data, kScalesAt + sizeof(i32) * index));
        }

        this->kind = Kind::Picture;
        return false;
    }

    if (kind != static_cast<u32>(Kind::Rows))
    {
        return false;
    }

    const u32 count = ByteReader::Word32At(data, kCountAt);
    this->rowScales.assign(count, 0);

    for (std::size_t row = 0; row < this->rowScales.size(); ++row)
    {
        this->rowScales[row] = static_cast<i32>(ByteReader::Word32At(data, kScalesAt + sizeof(i32) * row));
    }

    this->kind = Kind::Rows;
    return false;
}

// swd3e.exe 0x0042B2E0: the 256 levels, then the picture, from `data`; the picture's buffer is the whole size, its
// last 0x200 bytes zero
bool MapScale::SetPicture(std::span<const u8> data)
{
    for (std::size_t level = 0; level < this->levels.size(); ++level)
    {
        this->levels[level] = static_cast<i16>(ByteReader::Word16At(data, sizeof(i16) * level));
    }

    this->picture.assign(data.size(), 0);

    if (data.size() > MapScale::LevelBytes)
    {
        std::copy(data.begin() + MapScale::LevelBytes, data.end(), this->picture.begin());
    }

    return true;
}

// swd3e.exe 0x0042B350: no kind, the default scale for every level, no rows and no picture
bool MapScale::Clear()
{
    this->kind = Kind::None;
    this->defaultScale = MapScale::DefaultScale;
    this->rowScales.clear();
    this->unk00C.fill(this->defaultScale);
    this->levels.fill(static_cast<i16>(this->defaultScale));
    this->picture.clear();
    return true;
}

// swd3e.exe 0x0042B3D0: the level of the point (x, y): kind Picture its picture's pixel there, which picks a level
// (a transparent point or no picture the default); kind Rows its tile row; otherwise the default. An index past the
// levels gives the default (the original reads past them).
int MapScale::LevelAt(int x, int y) const
{
    if (this->kind == Kind::Picture)
    {
        u32 index = 0;

        if (!this->picture.empty() && RleCodec::PixelAt(this->picture, x, y, &index))
        {
            return index < this->levels.size() ? this->levels[index] : this->defaultScale;
        }
    }
    else if (this->kind == Kind::Rows)
    {
        return y / MapHeader::TileSize;
    }

    return this->defaultScale;
}

// swd3e.exe 0x0042B430: the scale of `level`: kind Picture the level's entry (the same table LevelAt picks from),
// kind Rows the row's scale; out of range or otherwise the default
int MapScale::Scale(int level) const
{
    if (this->kind == Kind::Picture)
    {
        if (level >= 0 && static_cast<std::size_t>(level) < this->levels.size())
        {
            return this->levels[static_cast<std::size_t>(level)];
        }
    }
    else if (this->kind == Kind::Rows && level >= 0 && static_cast<std::size_t>(level) < this->rowScales.size())
    {
        return this->rowScales[static_cast<std::size_t>(level)];
    }

    return this->defaultScale;
}

// swd3e.exe 0x0043C8E0 (the same code reads the first field of other objects)
MapScale::Kind MapScale::GetKind() const
{
    return this->kind;
}

}  // namespace swd3e
