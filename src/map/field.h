#pragma once

#include <array>
#include <memory>
#include <vector>

#include "core/types.h"
#include "map/map_scale.h"
#include "platform/display.h"
#include "res/tsw_archive.h"

namespace swd3e
{

class Config;
class ErrorReporter;
class GameState;
class MapCache;
class RleCodec;
struct Picture;
struct Role;

// A named region of the map (0x34 bytes; the list at 0x004ACAC0 starts with a node of its own).
struct MapRegion
{
    static constexpr i32 EventMask = 0x7FFF;      // in `event`
    static constexpr u32 ClickFlagMask = 0xFFFF;  // in `flag`: the story flag that lets it be clicked
    static constexpr u32 BlockFlagShift = 16;     // in `flag`: above it, the story flag that blocks it

    std::unique_ptr<MapRegion> next;  // +0x00
    i32 id = 0;                       // +0x04
    i32 event = 0;                    // +0x08 bit 15 aside: the event run when the region is clicked
    i32 flag = 0;                     // +0x0C ClickFlagMask and BlockFlagShift
    i32 blockFlag = 0;  // +0x10 swd3e: the story flag that stops the controlled role entering (0x00402AA0)
    std::array<char, 0x20> name = {};  // +0x14
};

// A picture laid over the map (0x20 bytes, the list at 0x004AB2F4): its RLE picture and placement.
struct MapOverlay
{
    static constexpr u16 FixedToMap = 8;       // a parallax that moves with the map
    static constexpr int ParallaxUnit = 0x10;  // other parallaxes are in sixteenths of the camera's distance

    std::vector<u8> picture;           // +0x00 a 16-bit RLE picture
    u16 kind = 0;                      // +0x04 swd32's drawing order (0..30); swd3e draws in list order (0x004157B0)
    u16 width = 0;                     // +0x06
    u16 height = 0;                    // +0x08
    u16 left = 0;                      // +0x0A map pixels of the area it shows in
    u16 top = 0;                       // +0x0C
    u16 right = 0;                     // +0x0E
    u16 bottom = 0;                    // +0x10
    u16 parallaxX = 0;                 // +0x16 FixedToMap or sixteenths (ParallaxUnit)
    u16 parallaxY = 0;                 // +0x18
    std::unique_ptr<MapOverlay> next;  // +0x1C
};

// The map the party is on: HUGE.lmf's map (its blocks, tiles, units and roles), the drawing order of the roles by row
// for the three layers, the regions, the overlays and the camera.
// A camera rectangle in pixels (four dwords in the original).
// An object of the map (swd3e; the four tables at 0x004F0DBC, 0x004F25BC, 0x004F19BC and 0x004F31BC by object number):
// where its data lies in the map, packed and unpacked, and its flags.
struct MapObject
{
    // Bits of `flags`.
    enum Flag : u32
    {
        Animated = 0x100,        // its data starts with the act and action a role of it runs
        Unk1000000 = 0x1000000,  // tested with Animated; the flags are a word, so never set
    };

    u32 offset = 0;    // 0x004F0DBC from the start of the map
    u32 packed = 0;    // 0x004F25BC
    u32 unpacked = 0;  // 0x004F19BC
    u32 flags = 0;     // 0x004F31BC MapObject::Flag
};

struct CameraRect
{
    i32 left = 0;
    i32 top = 0;
    i32 right = 0;
    i32 bottom = 0;
};

class Field : public MapPictureSource
{
public:
    // Map numbers the code tests, named by their place names in maps.dat (table 0x14).
    enum Map : int
    {
        TwoRiversPlain = 6,             // 兩河平原
        WesternRegions = 8,             // 西域
        DemonRefiningPot = 0x16,        // 煉妖壺
        MonasteryOutside = 0x32,        // 修道院外
        VeniceTavern = 0x51,            // 威尼斯酒屋
        MediterraneanEastCoast = 0xC8,  // 地中海東岸
    };

    static constexpr int Layers = 3;
    static constexpr int RowMargin = 0x14;
    static constexpr std::size_t Objects = 0x300;
    static constexpr u32 Msf3 = 0x3346534D;  // "MSF3", the map format of swd3e

    Field(GameState& state, MapCache& cache, const Config& config, const RleCodec& codec, ErrorReporter& errors);

    bool LoadMap(u32 map, u32& systemFlags);
    void ReopenArchive(int map);
    void ResetRows();
    void InsertRole(Role& role, int layer);
    Role* RemoveRole(u16 id, int layer, int row, int keep);
    Role* UnlinkRole(Role& role, int layer);
    bool LoadObjectPicture(u32 object, Picture& picture);
    bool LoadMapPicture(int unit, Picture& picture) override;
    void CenterCamera();
    // The camera CenterCamera gives a controlled role at (x, y): its left and top (in 1/scale pixels, as x and y).
    std::array<i32, 2> CameraCentredOn(i32 x, i32 y, int scale = 1) const;
    std::array<i32, 2> HoldCamera(i32 left, i32 top, int scale = 1) const;
    // Whether the rebuild's wider (higher) view holds the camera in its middle across (down): the map is narrower
    // (lower) than the view, so the camera does not move on that axis (HoldCamera).
    std::array<bool, 2> MapInsideView() const;
    // Where the game's 640x480 screen starts on the map (the camera when the map does not fill the window): the
    // interface places what it shows of the map from here.
    i32 ScreenLeft() const;
    i32 ScreenTop() const;
    void MarkRoleCells(const Role& role, i32 leader);
    void ClearRoleCells(const Role& role, i32 leader);
    MapRegion* FindRegion(int id);
    u32 ProbeCells(std::ptrdiff_t cell, int width, int height, u32 mask) const;
    void CameraAround(int x, int y, CameraRect& rect) const;

    MapRegion regions;                            // 0x004C6258
    std::unique_ptr<MapOverlay> overlays;         // 0x004C4A7C
    std::array<std::vector<Role*>, Layers> rows;  // 0x004C3184 the roles by row, RowMargin rows above the map
    u32 unitBase = 0;                             // 0x004F01B8 the map in HUGE.lmf
    std::array<MapObject, Objects> objects = {};  // 0x004F0DBC, 0x004F25BC, 0x004F19BC, 0x004F31BC
    MapScale scale;                               // 0x004F3DC0
    // 0x004D183C the map's kind: set from maps.dat table 3 by LoadCity, then from the MSF3 header by LoadMap; how the
    // roles' shadows fall and where the weather comes from
    i32 shadowKind = 0;
    i32 cameraLeft = 0;    // 0x004C5118
    i32 cameraTop = 0;     // 0x004C511C
    i32 cameraRight = 0;   // 0x004C5120
    i32 cameraBottom = 0;  // 0x004C5124
    // The rebuild (deviations.md): the canvas the map is drawn to, which the camera spans (the 640x480 screen unless
    // the map fills the window), and where the game's 640x480 screen lies on it (the interface is drawn there).
    i32 viewWidth = Display::Width;
    i32 viewHeight = Display::Height;
    i32 screenX = 0;
    i32 screenY = 0;
    std::vector<u16> palette;  // 0x004D0D58 the colours of an 8-bit map

private:
    GameState& state;
    MapCache& cache;
    const Config& config;
    const RleCodec& codec;
    ErrorReporter& errors;
};

}  // namespace swd3e
