#pragma once

#include <array>
#include <span>
#include <vector>

#include "core/types.h"

namespace swd3e
{

// The depth scale of the map (swd3e, the object at 0x004F3DC0, 0x614 bytes): the scale (in 1/1024) a role is drawn at
// by where it stands. Kind Picture: a picture laid over the map whose pixel (an index) picks one of 256 levels; kind
// Rows: a scale per tile row; otherwise the default 1:1.
class MapScale
{
public:
    enum class Kind : i32
    {
        None = 0,
        Picture = 1,
        Rows = 2,
    };

    static constexpr i32 DefaultScale = 0x400;
    static constexpr std::size_t Levels = 0x100;
    static constexpr std::size_t LevelBytes = Levels * sizeof(i16);  // SetPicture: the levels before the picture

    MapScale();
    ~MapScale();
    MapScale(const MapScale&) = delete;
    MapScale& operator=(const MapScale&) = delete;

    bool SetScales(std::span<const u8> data);
    bool SetPicture(std::span<const u8> data);
    bool Clear();
    int LevelAt(int x, int y) const;
    int Scale(int level) const;
    Kind GetKind() const;

private:
    Kind kind = Kind::None;                     // +0x000
    std::vector<i32> rowScales;                 // +0x004 Rows: a scale per tile row (their count at +0x008)
    std::array<i32, Levels> unk00C{};           // +0x00C set by SetScales for kind Picture; nothing reads them
    std::array<i16, Levels> levels{};           // +0x40C the scales the picture's indices pick
    std::vector<u8> picture;                    // +0x60C an RLE picture of level indices
    i32 defaultScale = MapScale::DefaultScale;  // +0x610
};

}  // namespace swd3e
