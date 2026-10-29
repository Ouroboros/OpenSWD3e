#include "map/field.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <span>
#include <string>

#include "config/config.h"
#include "core/byte_reader.h"
#include "core/debug_log.h"
#include "core/error_box.h"
#include "core/format.h"
#include "core/words.h"
#include "game/game.h"
#include "game/game_state.h"
#include "map/blocking_map.h"
#include "map/map_cache.h"
#include "pic/picture.h"
#include "pic/rle.h"
#include "platform/display.h"

namespace swd3e
{

namespace
{

constexpr std::string_view kLmf = "HUGE.lmf";

// HUGE.lmf: its first dword is the offset of the directory at its end, one kDirectoryEntryBytes entry per map with the
// start of the map and its number. Offsets inside a map are from its start.
constexpr std::size_t kDirectoryEntryBytes = 0x10;
constexpr int kEntryShift = 4;  // the entries in the directory's bytes less one
constexpr std::size_t kEntryStartAt = 0;
constexpr std::size_t kEntryMapAt = 8;

// The MSF3 map (swd3e): the magic, then a header of kHeaderRead bytes (offsets below are into it) whose sections
// are each an offset from the start of the map and a size; the tiles lie at kTilesAt from the start of the map.
constexpr u32 kHeaderRead = 0x400;
constexpr std::size_t kBlocksAt = 0x08;
constexpr std::size_t kBlocksBytesAt = 0x0C;
constexpr std::size_t kRegionsAt = 0x10;
constexpr std::size_t kRegionsBytesAt = 0x14;
constexpr std::size_t kObjectsAt = 0x18;
constexpr std::size_t kObjectsBytesAt = 0x1C;
constexpr std::size_t kRolesAt = 0x20;
constexpr std::size_t kRolesBytesAt = 0x24;
constexpr std::size_t kOverlaysAt = 0x28;
constexpr std::size_t kOverlaysBytesAt = 0x2C;
constexpr std::size_t kUnitSizeAt = 0x38;
constexpr std::size_t kScalePictureAt = 0x40;
constexpr std::size_t kScalePictureBytesAt = 0x44;
constexpr std::size_t kScalesAt = 0x48;
constexpr std::size_t kScalesBytesAt = 0x4C;
constexpr std::size_t kScalesUnpackedAt = 0x50;
constexpr std::size_t kWidthAt = 0x100;
constexpr std::size_t kHeightAt = 0x102;
constexpr std::size_t kBitsAt = 0x104;
constexpr std::size_t kUnk0CAt = 0x106;
constexpr std::size_t kLayersAt = 0x108;
constexpr std::size_t kShadowAt = 0x10C;
constexpr u32 kTilesAt = 0x118;

// A region (0x30 bytes after the count): its id, event, flag and block flag, then its name.
constexpr std::size_t kRegionBytes = 0x30;
constexpr std::size_t kRegionEventAt = 4;
constexpr std::size_t kRegionFlagAt = 8;
constexpr std::size_t kRegionBlockFlagAt = 0xC;
constexpr std::size_t kRegionNameAt = 0x10;

// The object and role sections: a word count, then from kRecordsAt their records.
constexpr std::size_t kRecordsAt = 8;
// An object: its data's offset, packed and unpacked sizes, its number and its flags.
constexpr std::size_t kObjectBytes = 0x10;
constexpr std::size_t kObjectPackedAt = 4;
constexpr std::size_t kObjectUnpackedAt = 8;
constexpr std::size_t kObjectNumberAt = 0xC;
constexpr std::size_t kObjectFlagsAt = 0xE;
constexpr u32 kAnimatedBytes = 0x10;  // read of an animated object's data: its act and action first
// A role: its object, its x, the y of its top, a word, its y and the placement word.
constexpr std::size_t kRoleBytes = 0x10;
constexpr std::size_t kRoleXAt = 2;
constexpr std::size_t kRoleTopAt = 4;
constexpr std::size_t kRoleYAt = 8;
constexpr std::size_t kRolePlacementAt = 0xA;
constexpr int kRoleMarginX = 0x28;  // a role further right than this many tiles past the map is not placed
constexpr int kRoleMarginY = 0x1E;  // or further down

// The placement word of a role: its offsets in its low byte (bits 4..7 across, bits 0..3 down), its layer in bits
// 8..9 by kPlacementLayers.
constexpr int kPlacementLayerShift = 8;
constexpr u16 kPlacementLowByte = 0xFF;
constexpr int kPlacementOffsetXShift = 4;
constexpr u16 kPlacementOffsetMask = 0xF;
constexpr std::array<u32, 3> kPlacementLayers = {Role::BackLayer, 0, Role::FrontLayer};
// Placement layer 3 reads the dword after the table on the stack: the return address of 0x0042A7D0 in its caller.
constexpr u32 kPlacementLayerPast = 0x0040CB13;

// An overlay (0x18 bytes): its data's offset, packed and unpacked sizes, its kinds word (bits 0..2 the kind, bits
// 3..6 of its low byte the parallax across, bits 7..10 down), then its rectangle in tiles (bytes).
constexpr std::size_t kOverlayBytes = 0x18;
constexpr std::size_t kOverlayPackedAt = 4;
constexpr std::size_t kOverlayUnpackedAt = 8;
constexpr std::size_t kOverlayKindsAt = 0xC;
constexpr std::size_t kOverlayLeftAt = 0xE;
constexpr std::size_t kOverlayTopAt = 0xF;
constexpr std::size_t kOverlayRightAt = 0x10;
constexpr std::size_t kOverlayBottomAt = 0x11;
constexpr u16 kOverlayKindMask = 7;
constexpr int kParallaxXShift = 3;
constexpr int kParallaxYShift = 7;
constexpr u16 kParallaxMask = 0xF;

// The depth scale's picture block: a dword, the unpacked size, then the packed data.
constexpr std::size_t kScaleUnpackedAt = 4;
constexpr std::size_t kScaleDataAt = 8;

// An object picture: a palette before the RLE header when it is not 16-bit RLE.
constexpr std::size_t kPaletteBytes = 0x200;

constexpr std::size_t kCountBytes = sizeof(u32);  // a section starts with its count
constexpr std::size_t kTileEntryBytes = sizeof(u32);

constexpr i32 kTwoTiles = 2;  // a role this wide or high has its second cell set directly

u16 Word(std::span<const u8> bytes, std::size_t at)
{
    return ByteReader::Word16At(bytes, at);
}

u8 Byte(std::span<const u8> bytes, std::size_t at)
{
    return at < bytes.size() ? bytes[at] : u8{0};
}

u32 Dword(std::span<const u8> bytes, std::size_t at)
{
    return MakeWords(Word(bytes, at + sizeof(u16)), Word(bytes, at));
}

std::size_t StringLength(std::span<const u8> bytes, std::size_t at)
{
    std::size_t length = 0;

    while (at + length < bytes.size() && bytes[at + length] != 0)
    {
        ++length;
    }

    return length;
}

// Reads `bytes` at the current position (fewer at the end of the file; the rest stays zero).
std::vector<u8> ReadBlock(File& file, u32 bytes)
{
    std::vector<u8> block(bytes, 0);
    u32 read = bytes;
    file.Read(block.data(), read);
    return block;
}

// A camera's place on an axis held inside the map as the original holds it: not before the start, and at the end where
// the view would pass it (before the start on a map smaller than the view). The rebuild's view wider (higher) than
// 640x480 (`wide`) shows a map smaller than it in its middle instead.
i32 HoldPlace(i32 place, i32 extent, i32 view, bool wide)
{
    if (wide && extent < view)
    {
        return (extent - view) / 2;
    }

    place = std::max(place, 0);
    return place + view >= extent ? extent - view : place;
}

}  // namespace

Field::Field(GameState& state, MapCache& cache, const Config& config, const RleCodec& codec, ErrorReporter& errors)
    : state(state),
      cache(cache),
      config(config),
      codec(codec),
      errors(errors)
{
}

// swd3e.exe 0x00411970: the row lists of the three layers (the map's height plus 40 rows, all empty)
void Field::ResetRows()
{
    for (std::vector<Role*>& layer : this->rows)
    {
        layer.assign(
            static_cast<std::size_t>(std::max(this->state.map.height, 0)) + Field::RowMargin + Field::RowMargin,
            nullptr
        );
    }
}

// swd3e.exe 0x00411790: `role` into the row of its y in `layer`, before the first role of the row it is not above (by
// y, then by id)
void Field::InsertRole(Role& role, int layer)
{
    // The original indexes past its three layers for a layer of 3 (a placement of 3, Field::LoadMap).
    if (layer < 0 || layer >= Field::Layers)
    {
        return;
    }

    const auto y = static_cast<u32>(role.y);
    const auto row = static_cast<std::size_t>(static_cast<i32>(y) / MapHeader::TileSize + Field::RowMargin);
    std::vector<Role*>& lists = this->rows[static_cast<std::size_t>(layer)];

    if (row >= lists.size())
    {
        return;
    }

    Role*& head = lists[row];
    Role* current = head;

    if (current == nullptr)
    {
        head = &role;
        role.next = nullptr;
        return;
    }

    Role* following = current->next;
    const auto currentY = static_cast<u32>(current->y);

    if (following == nullptr)
    {
        if (currentY <= y && current->id >= role.id)
        {
            role.next = nullptr;
            current->next = &role;
            return;
        }

        role.next = current;
        head = &role;
        return;
    }

    if (currentY <= y && role.id >= current->id)
    {
        role.next = current;
        head = &role;
        return;
    }

    for (;;)
    {
        if (static_cast<u32>(current->y) <= y && current->id >= role.id && following->id <= role.id)
        {
            break;
        }

        current = following;
        following = following->next;

        if (following == nullptr)
        {
            current->next = &role;
            role.next = nullptr;
            return;
        }
    }

    role.next = current->next;
    current->next = &role;
}

// swd3e.exe 0x00411890: unlinks the role with `id` from the first row at or below `row` of `layer` holding it; with
// `keep` 1 it is returned, otherwise put back by its y (and nullptr returned)
Role* Field::RemoveRole(u16 id, int layer, int row, int keep)
{
    // The original indexes past its three layers for a layer of 3, as InsertRole does.
    if (layer < 0 || layer >= Field::Layers)
    {
        return nullptr;
    }

    std::vector<Role*>& lists = this->rows[static_cast<std::size_t>(layer)];

    for (int at = row; at < this->state.map.height; ++at)
    {
        const auto index = static_cast<std::size_t>(at + Field::RowMargin);

        if (index >= lists.size())
        {
            break;
        }

        Role*& head = lists[index];
        Role* first = head;

        if (first == nullptr)
        {
            continue;
        }

        Role* removed = nullptr;

        if (first->id == id)
        {
            removed = first;
            head = first->next;
        }
        else
        {
            Role* previous = first;

            for (Role* current = first->next; current != nullptr; previous = current, current = current->next)
            {
                if (current->id == id)
                {
                    removed = current;
                    previous->next = current->next;
                    break;
                }
            }
        }

        if (removed == nullptr)
        {
            continue;
        }

        removed->next = nullptr;

        if (keep == 1)
        {
            return removed;
        }

        this->InsertRole(*removed, static_cast<int>(removed->flags & Role::LayerMask));
        return nullptr;
    }

    // swd3e drops swd32's report of a role not found.
    return nullptr;
}

// swd3e.exe 0x00411830: `role` unlinked from the row of its y (y / 16, rounded towards zero) of `layer`; the role when
// it was there, otherwise nullptr
Role* Field::UnlinkRole(Role& role, int layer)
{
    // The original indexes past its three layers for a layer of 3, as InsertRole does.
    if (layer < 0 || layer >= Field::Layers)
    {
        return nullptr;
    }

    std::vector<Role*>& lists = this->rows[static_cast<std::size_t>(layer)];
    const auto index = static_cast<std::size_t>(role.y / MapHeader::TileSize + Field::RowMargin);

    if (index >= lists.size())
    {
        return nullptr;
    }

    Role*& head = lists[index];
    Role* previous = nullptr;

    for (Role* current = head; current != nullptr; previous = current, current = current->next)
    {
        if (current != &role)
        {
            continue;
        }

        if (previous != nullptr)
        {
            previous->next = current->next;
        }
        else
        {
            head = current->next;
        }

        current->next = nullptr;
        return current;
    }

    return nullptr;
}

// swd3e.exe 0x0040B060: the picture of map object `object` unpacked from HUGE.lmf: a 16-bit RLE picture is converted
// to the display, any other (a 256-entry palette followed by an 8-bit RLE picture, whose size it gives) becomes a
// 16-bit RLE picture. Always true; an object past the tables gives no picture (the original reads past them).
bool Field::LoadObjectPicture(u32 object, Picture& picture)
{
    picture.data.clear();

    if (object >= Field::Objects)
    {
        return true;
    }

    const MapObject& entry = this->objects[object];
    this->cache.lmf.SeekBegin(static_cast<i32>(this->unitBase + entry.offset));
    const std::vector<u8> packed = ReadBlock(this->cache.lmf, entry.packed);
    picture.data.assign(entry.unpacked, 0);
    MapCache::Unpack(packed, picture.data);
    const bool rle = Word(picture.data, 0) == RleHeader::Magic;
    const std::size_t header = rle ? 0 : kPaletteBytes;
    picture.width = Word(picture.data, header + RleHeader::WidthAt);
    picture.height = Word(picture.data, header + RleHeader::HeightAt);
    const u16 format = Word(picture.data, header + RleHeader::FormatAt);

    if ((format & RleHeader::CountMask) == RleHeader::TrueColor)
    {
        RleHeader converted;
        this->codec.ConvertToDisplay(picture.data, converted);
        picture.width = converted.width;
        picture.height = converted.height;
    }
    else
    {
        this->codec.ConvertTileTo16(picture.data);
    }

    return true;
}

// The pictures of the map objects for the picture archive (id 0xFFFF).
bool Field::LoadMapPicture(int unit, Picture& picture)
{
    return this->LoadObjectPicture(static_cast<u32>(unit), picture);
}

// swd3e.exe 0x0040D410: the camera centred on the controlled role (304 pixels left of it, 240 above: half the view
// less a tile across), held inside the map
void Field::CenterCamera()
{
    const Role& player = this->state.Player();
    const std::array<i32, 2> place = this->CameraCentredOn(player.x, player.y);
    this->cameraLeft = place[0];
    this->cameraTop = place[1];
    this->cameraRight = this->cameraLeft + this->viewWidth;
    this->cameraBottom = this->cameraTop + this->viewHeight;
    this->original =
        this->HoldScreen({player.x - (Display::Width / 2 - MapHeader::TileSize), player.y - Display::Height / 2});
    this->originalPan = this->original;
}

std::array<i32, 2> Field::CameraCentredOn(i32 x, i32 y, int scale) const
{
    return this
        ->HoldCamera(x - (this->viewWidth / 2 - MapHeader::TileSize) * scale, y - this->viewHeight / 2 * scale, scale);
}

// The camera at (left, top) (in 1/scale pixels) held inside the map (HoldPlace; the rebuild's wider view, see
// deviations.md).
std::array<i32, 2> Field::HoldCamera(i32 left, i32 top, int scale) const
{
    const i32 width = MapHeader::TileSize * this->state.map.width;
    const i32 height = MapHeader::TileSize * this->state.map.height;
    return {
        HoldPlace(left, width * scale, this->viewWidth * scale, this->viewWidth > Display::Width),
        HoldPlace(top, height * scale, this->viewHeight * scale, this->viewHeight > Display::Height),
    };
}

std::array<bool, 2> Field::MapInsideView() const
{
    const bool wide = this->viewWidth > Display::Width;
    const bool high = this->viewHeight > Display::Height;
    return {
        wide && MapHeader::TileSize * this->state.map.width < this->viewWidth,
        high && MapHeader::TileSize * this->state.map.height < this->viewHeight,
    };
}

bool Field::WiderView() const
{
    return this->viewWidth != Display::Width || this->viewHeight != Display::Height;
}

Point Field::HoldScreen(Point place) const
{
    return {
        HoldPlace(place.x, MapHeader::TileSize * this->state.map.width, Display::Width, false),
        HoldPlace(place.y, MapHeader::TileSize * this->state.map.height, Display::Height, false),
    };
}

void Field::MoveOriginal(Point by)
{
    this->original = this->HoldScreen({this->original.x + by.x, this->original.y + by.y});
    this->originalPan = this->HoldScreen({this->originalPan.x + by.x, this->originalPan.y + by.y});
}

Field::ScrollSides Field::ScrollSidesOf(const CameraRect& camera, i32 width, i32 height, i32 x, i32 y) const
{
    const auto screenX = static_cast<u32>(x - camera.left);
    const auto screenY = static_cast<u32>(y - camera.top);
    const auto scrollX = static_cast<u32>(width / 2 - MapHeader::TileSize);
    const auto scrollY = static_cast<u32>(height / 2);
    return ScrollSides{
        camera.left > 0 && screenX <= scrollX,
        screenX >= scrollX && camera.right < MapHeader::TileSize * this->state.map.width,
        camera.top > 0 && screenY <= scrollY,
        screenY >= scrollY && camera.bottom < MapHeader::TileSize * this->state.map.height,
    };
}

void Field::DecideOriginalScroll(i32 x, i32 y, int stepX, int stepY)
{
    const CameraRect screen{
        this->original.x,
        this->original.y,
        this->original.x + Display::Width,
        this->original.y + Display::Height,
    };

    const ScrollSides sides = this->ScrollSidesOf(screen, Display::Width, Display::Height, x, y);
    this->originalScrolls = {
        (stepX < 0 && sides.left) || (stepX > 0 && sides.right),
        (stepY < 0 && sides.up) || (stepY > 0 && sides.down),
    };
}

void Field::FollowOriginal(Point by)
{
    this->MoveOriginal({this->originalScrolls[0] ? by.x : 0, this->originalScrolls[1] ? by.y : 0});
}

void Field::PanOriginal(Point to, Point step)
{
    this->originalPan = this->HoldScreen(to);
    this->originalPanStep = {std::abs(step.x), std::abs(step.y)};
}

// The original's pan steps until it is where it goes (a step that does not divide the distance stops there).
void Field::StepOriginalPan()
{
    auto approach = [](i32 place, i32 target, i32 step)
    {
        step = std::max(step, 1);
        return place + std::clamp(target - place, -step, step);
    };

    this->original.x = approach(this->original.x, this->originalPan.x, this->originalPanStep.x);
    this->original.y = approach(this->original.y, this->originalPan.y, this->originalPanStep.y);
}

bool Field::OriginalPanning() const
{
    return this->original != this->originalPan;
}

i32 Field::ScreenLeft() const
{
    return this->cameraLeft + this->screenX;
}

i32 Field::ScreenTop() const
{
    return this->cameraTop + this->screenY;
}

// swd3e.exe 0x00433650: HUGE.lmf opened again after a map movie or command 0xC2 closed it (the minimal installation's
// from the CD after a CD check in the original; the rebuild reads the installation's, see deviations.md). A file that
// cannot be opened is only logged with the map shown.
void Field::ReopenArchive(int map)
{
    const std::u8string path = PathFormat("%s%s", {this->config.installDirectory, kLmf});

    if (!this->cache.lmf.Open(path, File::Disposition::OpenExisting, File::Access::Read))
    {
        ErrorLog(u8"Load(%d)  ,LMF can't open:%s", map, path);  // 0x004B21AC
    }
}

// swd3e.exe 0x0042A7D0: map `map` of HUGE.lmf (MSF3; the CD's for the small installation): its header, the cached
// unit of its pixels, the tiles (a word kept of each dword, one more than the map's), the blocks, the regions, the
// object table, the roles, the overlays and the depth scale. False on a missing file or map (a missing map also ends
// the game); another magic is reported and read on. A role placed out past 40 tiles right or 30 below the map takes
// no slot; a role whose placement layer is 3 takes the caller's return address as its flags (the layer table has
// three entries).
bool Field::LoadMap(u32 map, u32& systemFlags)
{
    MapHeader& header = this->state.map;
    File& lmf = this->cache.lmf;
    lmf.Close();
    header.blocks.clear();
    header.tiles.clear();
    this->scale.Clear();
    this->regions = MapRegion{};
    // The original reads the minimal installation's HUGE.lmf from the CD (<cd>swd3e\HUGE.lmf); the rebuild reads it
    // from the installation whatever the type (see deviations.md).
    const std::u8string path = PathFormat("%s%s", {this->config.installDirectory, kLmf});

    if (!lmf.Open(path, File::Disposition::OpenExisting, File::Access::Read))
    {
        ErrorLog(u8"Load(%d)  ,LMF can't open:%s", map, path);
        return false;
    }

    u32 directory = 0;
    lmf.ReadDword(directory);
    const u32 directoryBytes = lmf.Size() - directory;
    const u32 entries = (directoryBytes - 1) >> kEntryShift;
    lmf.SeekBegin(static_cast<i32>(directory));
    const std::vector<u8> table = ReadBlock(lmf, directoryBytes);
    u32 entry = 0;

    while (entry < entries && Dword(table, kDirectoryEntryBytes * static_cast<std::size_t>(entry) + kEntryMapAt) != map)
    {
        ++entry;
    }

    if (entry == entries)
    {
        systemFlags |= Game::QuitRequested;
        ErrorLog(u8"LoadMap3(%d) ,MapCode fail:%d", map, map);
        return false;
    }

    this->unitBase = Dword(table, kDirectoryEntryBytes * static_cast<std::size_t>(entry) + kEntryStartAt);
    const u32 base = this->unitBase;
    lmf.SeekBegin(static_cast<i32>(base));
    u32 magic = 0;
    lmf.ReadDword(magic);

    if (magic != Field::Msf3)
    {
        ErrorBox(this->errors, u8"LoadMap3", u8"地圖[%d]是舊格式", map);  // 0x004B218C
    }

    const std::vector<u8> head = ReadBlock(lmf, kHeaderRead);
    header.width = Word(head, kWidthAt);
    header.height = Word(head, kHeightAt);
    header.bitsPerPixel = Word(head, kBitsAt);
    header.unk0C = Word(head, kUnk0CAt);
    header.layers = Word(head, kLayersAt);
    this->shadowKind = Word(head, kShadowAt);
    header.unk14 = 0;
    this->ResetRows();
    header.name[0] = 0;
    this->cache.unitFile.UnmapView(header.pixels);
    header.pixels = nullptr;
    this->cache.view = nullptr;
    this->cache.unitFile.CloseMapping();
    this->cache.unitFile.Close();
    header.pixels = this->cache.GetCachedMap(map, base, Dword(head, kUnitSizeAt), 0);

    // The tiles: a dword each (and one more), of which the low word is kept.
    const u32 tileBytes = kTileEntryBytes * static_cast<u32>(header.height) * static_cast<u32>(header.width) *
                              static_cast<u32>(header.layers) +
                          kTileEntryBytes;
    lmf.SeekBegin(static_cast<i32>(base + kTilesAt));
    const std::vector<u8> tileWords = ReadBlock(lmf, tileBytes);
    header.tiles.assign(tileBytes / kTileEntryBytes, 0);

    for (std::size_t tile = 0; tile < header.tiles.size(); ++tile)
    {
        header.tiles[tile] = Word(tileWords, kTileEntryBytes * tile);
    }

    lmf.SeekBegin(static_cast<i32>(base + Dword(head, kBlocksAt)));
    const std::vector<u8> packed = ReadBlock(lmf, Dword(head, kBlocksBytesAt));
    header.blocks.assign(static_cast<std::size_t>(header.width) * static_cast<std::size_t>(header.height), 0);
    MapCache::Unpack(
        packed,
        std::span<u8>(reinterpret_cast<u8*>(header.blocks.data()), header.blocks.size() * sizeof(u32))
    );

    lmf.SeekBegin(static_cast<i32>(base + Dword(head, kRegionsAt)));
    std::vector<u8> block = ReadBlock(lmf, Dword(head, kRegionsBytesAt));
    const u32 regionCount = Dword(block, 0);

    for (u32 region = 0; region < regionCount; ++region)
    {
        const std::size_t at = kCountBytes + kRegionBytes * static_cast<std::size_t>(region);
        auto node = std::make_unique<MapRegion>();
        node->id = static_cast<i32>(Dword(block, at));
        node->event = static_cast<i32>(Dword(block, at + kRegionEventAt));
        node->flag = static_cast<i32>(Dword(block, at + kRegionFlagAt));
        node->blockFlag = static_cast<i32>(Dword(block, at + kRegionBlockFlagAt));
        const std::size_t length = StringLength(block, at + kRegionNameAt);
        std::copy_n(
            block.begin() + static_cast<std::ptrdiff_t>(std::min(at + kRegionNameAt, block.size())),
            std::min<std::size_t>(length, node->name.size() - 1),
            node->name.begin()
        );
        node->next = std::move(this->regions.next);
        this->regions.next = std::move(node);
    }

    lmf.SeekBegin(static_cast<i32>(base + Dword(head, kObjectsAt)));
    block = ReadBlock(lmf, Dword(head, kObjectsBytesAt));
    const u16 objectCount = Word(block, 0);

    for (u16 index = 0; index < objectCount; ++index)
    {
        const std::size_t at = kRecordsAt + kObjectBytes * index;
        const u16 object = Word(block, at + kObjectNumberAt);

        if (object < Field::Objects)
        {
            this->objects[object] = {
                Dword(block, at),
                Dword(block, at + kObjectPackedAt),
                Dword(block, at + kObjectUnpackedAt),
                Word(block, at + kObjectFlagsAt),
            };
        }
    }

    lmf.SeekBegin(static_cast<i32>(base + Dword(head, kRolesAt)));
    block = ReadBlock(lmf, Dword(head, kRolesBytesAt));
    const u16 roleCount = Word(block, 0);

    for (u16 index = 0; index < roleCount; ++index)
    {
        if (this->state.roleCount < 0 || this->state.roleCount >= GameState::Roles)
        {
            break;
        }

        const std::size_t at = kRecordsAt + kRoleBytes * index;
        Role& role = this->state.roles[static_cast<std::size_t>(this->state.roleCount)];
        role.act = ActState{};
        ResetPlayback(role.act);
        const u16 placement = Word(block, at + kRolePlacementAt);
        const std::size_t layer = (placement >> kPlacementLayerShift) & Role::LayerMask;
        role.flags = (layer < kPlacementLayers.size() ? kPlacementLayers[layer] : kPlacementLayerPast) | Role::Visible |
                     Role::StepsAct;
        role.drawOffsetX = static_cast<u16>((placement & kPlacementLowByte) >> kPlacementOffsetXShift);
        role.drawOffsetY = static_cast<u16>(placement & kPlacementOffsetMask);
        const u16 x = Word(block, at + kRoleXAt);
        const u16 y = Word(block, at + kRoleYAt);
        role.x = x;
        role.y = y;
        role.id = Role::NoId;

        if (x > static_cast<u32>(MapHeader::TileSize * (header.width + kRoleMarginX)) ||
            y > MapHeader::TileSize * (header.height + kRoleMarginY))
        {
            continue;
        }

        const u16 object = Word(block, at);
        const MapObject* definition = object < Field::Objects ? &this->objects[object] : nullptr;

        if (definition != nullptr && (definition->flags & (MapObject::Animated | MapObject::Unk1000000)) != 0)
        {
            lmf.SeekBegin(static_cast<i32>(base + definition->offset));
            const std::vector<u8> act = ReadBlock(lmf, kAnimatedBytes);
            role.act.id = Word(act, 0);
            role.act.action = Word(act, sizeof(u16));
            role.act.x = 0;
            role.act.y = 0;
            role.cell = nullptr;
        }
        else
        {
            role.act.id = 0;
            role.act.picture = object;
            role.act.x = 0;
            role.cell = nullptr;
            role.act.y = Word(block, at + kRoleYAt) - Word(block, at + kRoleTopAt);
        }

        this->InsertRole(role, static_cast<int>(role.flags & Role::LayerMask));
        ++this->state.roleCount;
    }

    if (Dword(head, kOverlaysAt) != 0)
    {
        lmf.SeekBegin(static_cast<i32>(base + Dword(head, kOverlaysAt)));
        block = ReadBlock(lmf, Dword(head, kOverlaysBytesAt));
        const u16 overlayCount = Word(block, 0);

        for (u16 index = 0; index < overlayCount; ++index)
        {
            const std::size_t at = kRecordsAt + kOverlayBytes * index;
            const u32 packedBytes = Dword(block, at + kOverlayPackedAt);

            if (packedBytes == 0)
            {
                continue;
            }

            lmf.SeekBegin(static_cast<i32>(base + Dword(block, at)));
            const std::vector<u8> data = ReadBlock(lmf, packedBytes);
            auto node = std::make_unique<MapOverlay>();
            node->picture.assign(Dword(block, at + kOverlayUnpackedAt), 0);
            MapCache::Unpack(data, node->picture);
            node->width = Word(node->picture, RleHeader::WidthAt);
            node->height = Word(node->picture, RleHeader::HeightAt);
            node->left = static_cast<u16>(MapHeader::TileSize * Byte(block, at + kOverlayLeftAt));
            node->top = static_cast<u16>(MapHeader::TileSize * Byte(block, at + kOverlayTopAt));
            node->right = static_cast<u16>(MapHeader::TileSize * Byte(block, at + kOverlayRightAt));
            node->bottom = static_cast<u16>(MapHeader::TileSize * Byte(block, at + kOverlayBottomAt));
            const u16 kinds = Word(block, at + kOverlayKindsAt);
            node->kind = kinds & kOverlayKindMask;
            node->parallaxX = static_cast<u16>((Byte(block, at + kOverlayKindsAt) >> kParallaxXShift) & kParallaxMask);
            node->parallaxY = static_cast<u16>((kinds >> kParallaxYShift) & kParallaxMask);

            // 0x00401680 writes back the same width and height words read above, and nothing for another magic, so
            // the size stays as read.
            if ((Word(node->picture, RleHeader::FormatAt) & RleHeader::CountMask) == RleHeader::TrueColor)
            {
                RleHeader converted;
                this->codec.ConvertToDisplay(node->picture, converted);
            }

            node->next = std::move(this->overlays);
            this->overlays = std::move(node);
        }
    }

    if (Dword(head, kScalePictureAt) != 0)
    {
        lmf.SeekBegin(static_cast<i32>(base + Dword(head, kScalePictureAt)));
        const std::vector<u8> data = ReadBlock(lmf, Dword(head, kScalePictureBytesAt));
        std::vector<u8> unpacked(Dword(data, kScaleUnpackedAt), 0);
        const std::size_t produced =
            MapCache::Unpack(std::span<const u8>(data).subspan(std::min(kScaleDataAt, data.size())), unpacked);
        this->scale.SetPicture(std::span<const u8>(unpacked).first(std::min(produced, unpacked.size())));
    }

    if (Dword(head, kScalesAt) != 0)
    {
        lmf.SeekBegin(static_cast<i32>(base + Dword(head, kScalesAt)));
        const std::vector<u8> data = ReadBlock(lmf, Dword(head, kScalesBytesAt));
        std::vector<u8> unpacked(Dword(head, kScalesUnpackedAt), 0);
        const std::size_t produced = MapCache::Unpack(data, unpacked);
        this->scale.SetScales(std::span<const u8>(unpacked).first(std::min(produced, unpacked.size())));
    }

    return true;
}

namespace
{

// The cells a role covers from its own cell: its width and height in tiles (the act's 'HW'); the original walks them
// through a pointer into the grid. Past the single-cell, 2x1 and 1x2 cases, a cell is changed only where its row
// and column (from the role's position, shifted unsigned) lie inside the map.
template <typename Change>
void ChangeRoleCells(std::vector<u32>& blocks, i32 mapWidth, i32 mapHeight, const Role& role, Change change)
{
    if (role.cell == nullptr || blocks.empty())
    {
        return;
    }

    const auto start = static_cast<std::ptrdiff_t>(role.cell - blocks.data());
    const i32 width = role.act.width;
    const i32 height = role.act.height;

    auto apply = [&](std::ptrdiff_t at)
    {
        if (at >= 0 && static_cast<std::size_t>(at) < blocks.size())
        {
            change(blocks[static_cast<std::size_t>(at)]);
        }
    };

    apply(start);

    if (height == 1)
    {
        if (width == kTwoTiles)
        {
            apply(start + 1);
            return;
        }

        if (width == 1)
        {
            return;
        }
    }
    else if (height == kTwoTiles && width == 1)
    {
        apply(start + mapWidth);
        return;
    }

    std::ptrdiff_t rowStart = start;
    auto row = static_cast<i32>(static_cast<u32>(role.y) >> MapHeader::TileShift);

    for (i32 rows = 0; rows < height; ++rows, ++row)
    {
        if (row < mapHeight && row >= 0)
        {
            auto column = static_cast<i32>(static_cast<u32>(role.x) >> MapHeader::TileShift);
            std::ptrdiff_t at = rowStart;

            for (i32 columns = 0; columns < width; ++columns, ++column, ++at)
            {
                if (column < mapWidth && column >= 0)
                {
                    apply(at);
                }
            }
        }

        rowStart += mapWidth;
    }
}

}  // namespace

// swd3e.exe 0x0040B270: the cells under a role marked taken (bit 28; bit 29 too for flag 0x4000, bit 23 for flag 0x10,
// bit 8 for the leader); the first cell is marked once more by the walk
void Field::MarkRoleCells(const Role& role, i32 leader)
{
    u32 bits = (role.flags & Role::Solid) != 0 ? MapHeader::RoleCell | MapHeader::SolidRole : MapHeader::RoleCell;

    if ((role.flags & Role::Unk10) != 0)
    {
        bits |= MapHeader::Unk800000;
    }

    if (static_cast<i32>(role.id) == leader)
    {
        bits |= MapHeader::LeaderCell;
    }

    ChangeRoleCells(
        this->state.map.blocks,
        this->state.map.width,
        this->state.map.height,
        role,
        [bits](u32& cell)
        {
            cell |= bits;
        }
    );
}

// swd3e.exe 0x0040B180: the marks of a role taken off its cells (the leader's bit 8 too)
void Field::ClearRoleCells(const Role& role, i32 leader)
{
    const u32 mask =
        ~(MapHeader::RoleCell | MapHeader::SolidRole | MapHeader::Unk800000 |
          (static_cast<i32>(role.id) == leader ? MapHeader::LeaderCell : 0u));
    ChangeRoleCells(
        this->state.map.blocks,
        this->state.map.width,
        this->state.map.height,
        role,
        [mask](u32& cell)
        {
            cell &= mask;
        }
    );
}

// swd3e.exe 0x0040DFA0: the region with `id` after the list head (nullptr when none)
MapRegion* Field::FindRegion(int id)
{
    for (MapRegion* region = this->regions.next.get(); region != nullptr; region = region->next.get())
    {
        if (region->id == id)
        {
            return region;
        }
    }

    return nullptr;
}

// swd3e.exe 0x0040BF50: the cells around a footprint of width x height cells at `cell` holding bits of `mask`, by
// direction: 1 up left, 2 up, 4 up right, 8 right, 0x10 down right, 0x20 down, 0x40 down left, 0x80 left. Footprints of
// at most 2 cells take an unrolled path that treats sizes below 1 like 1. The original keeps the four edges in one
// 32-entry stack array whose parts overlap for footprints above 6 cells and run past it from 7 cells high; here they
// are separate. A cell outside the grid has every bit set.
u32 Field::ProbeCells(std::ptrdiff_t cell, int width, int height, u32 mask) const
{
    const std::vector<u32>& blocks = this->state.map.blocks;
    const int mapWidth = this->state.map.width;

    auto read = [&blocks, mask](std::ptrdiff_t at) -> u32
    {
        return (at >= 0 && static_cast<std::size_t>(at) < blocks.size() ? blocks[static_cast<std::size_t>(at)]
                                                                        : BlockingMap::Outside) &
               mask;
    };

    u32 upLeft = 0;
    u32 up = 0;
    u32 upRight = 0;
    u32 right = 0;
    u32 downRight = 0;
    u32 down = 0;
    u32 downLeft = 0;
    u32 left = 0;
    const std::ptrdiff_t topBase = cell - (mapWidth + 1);
    const std::ptrdiff_t bottomBase = cell + static_cast<std::ptrdiff_t>(height) * mapWidth - 1;
    std::vector<u32> top(
        static_cast<std::size_t>(std::max(width + FootprintEdge::Corners, FootprintEdge::SmallCells)),
        0
    );
    std::vector<u32> bottom(top.size(), 0);

    if (width > FootprintEdge::SmallFootprint)
    {
        for (int index = 0; index < width + FootprintEdge::Corners; ++index)
        {
            top[static_cast<std::size_t>(index)] |= read(topBase + index);
            bottom[static_cast<std::size_t>(index)] |= read(bottomBase + index);
        }

        for (std::size_t index = 0; index < static_cast<std::size_t>(width); ++index)
        {
            up |= top[index + FootprintEdge::First];
            upLeft |= top[index];
            upRight |= top[index + FootprintEdge::AfterOne];
            down |= bottom[index + FootprintEdge::First];
            downLeft |= bottom[index];
            downRight |= bottom[index + FootprintEdge::AfterOne];
        }
    }
    else
    {
        for (int index = 0; index < static_cast<int>(FootprintEdge::AfterTwo); ++index)
        {
            top[static_cast<std::size_t>(index)] |= read(topBase + index);
            bottom[static_cast<std::size_t>(index)] |= read(bottomBase + index);
        }

        if (width == FootprintEdge::SmallFootprint)
        {
            top[FootprintEdge::AfterTwo] |= read(topBase + static_cast<std::ptrdiff_t>(FootprintEdge::AfterTwo));
            bottom[FootprintEdge::AfterTwo] |= read(bottomBase + static_cast<std::ptrdiff_t>(FootprintEdge::AfterTwo));
        }

        upLeft = top[FootprintEdge::Before];
        downLeft = bottom[FootprintEdge::Before];
        up = top[FootprintEdge::First];
        down = bottom[FootprintEdge::First];
        downRight = bottom[FootprintEdge::AfterOne];
        upRight = top[FootprintEdge::AfterOne];

        if (width == FootprintEdge::SmallFootprint)
        {
            upLeft = top[FootprintEdge::First] | top[FootprintEdge::Before];
            up = upRight | top[FootprintEdge::First];
            upRight |= top[FootprintEdge::AfterTwo];
            downLeft = bottom[FootprintEdge::First] | bottom[FootprintEdge::Before];
            down = bottom[FootprintEdge::AfterOne] | bottom[FootprintEdge::First];
            downRight = bottom[FootprintEdge::AfterTwo] | bottom[FootprintEdge::AfterOne];
        }
    }

    const std::ptrdiff_t leftBase = topBase;
    const std::ptrdiff_t rightBase = cell + width - mapWidth;
    std::vector<u32> leftSide(
        static_cast<std::size_t>(std::max(height + FootprintEdge::Corners, FootprintEdge::SmallCells)),
        0
    );
    std::vector<u32> rightSide(leftSide.size(), 0);

    if (height > FootprintEdge::SmallFootprint)
    {
        for (int index = 0; index < height + FootprintEdge::Corners; ++index)
        {
            leftSide[static_cast<std::size_t>(index)] |= read(leftBase + static_cast<std::ptrdiff_t>(index) * mapWidth);
            rightSide[static_cast<std::size_t>(index)] |=
                read(rightBase + static_cast<std::ptrdiff_t>(index) * mapWidth);
        }

        for (std::size_t index = 0; index < static_cast<std::size_t>(height); ++index)
        {
            left |= leftSide[index + FootprintEdge::First];
            upLeft |= leftSide[index];
            downLeft |= leftSide[index + FootprintEdge::AfterOne];
            right |= rightSide[index + FootprintEdge::First];
            upRight |= rightSide[index];
            downRight |= rightSide[index + FootprintEdge::AfterOne];
        }
    }
    else
    {
        for (int index = 0; index < static_cast<int>(FootprintEdge::AfterTwo); ++index)
        {
            leftSide[static_cast<std::size_t>(index)] |= read(leftBase + static_cast<std::ptrdiff_t>(index) * mapWidth);
            rightSide[static_cast<std::size_t>(index)] |=
                read(rightBase + static_cast<std::ptrdiff_t>(index) * mapWidth);
        }

        if (height == FootprintEdge::SmallFootprint)
        {
            leftSide[FootprintEdge::AfterTwo] |=
                read(leftBase + static_cast<std::ptrdiff_t>(FootprintEdge::AfterTwo) * mapWidth);
            rightSide[FootprintEdge::AfterTwo] |=
                read(rightBase + static_cast<std::ptrdiff_t>(FootprintEdge::AfterTwo) * mapWidth);
        }

        upLeft |= leftSide[FootprintEdge::Before];
        left = leftSide[FootprintEdge::First];
        downLeft |= leftSide[FootprintEdge::AfterOne];
        upRight |= rightSide[FootprintEdge::Before];
        right = rightSide[FootprintEdge::First];
        const u32 lowRight = rightSide[FootprintEdge::AfterOne];
        downRight |= lowRight;

        if (height == FootprintEdge::SmallFootprint)
        {
            upLeft |= leftSide[FootprintEdge::First];
            left = leftSide[FootprintEdge::AfterOne] | leftSide[FootprintEdge::First];
            downLeft |= leftSide[FootprintEdge::AfterTwo];
            upRight |= rightSide[FootprintEdge::First];
            right = lowRight | rightSide[FootprintEdge::First];
            downRight |= rightSide[FootprintEdge::AfterTwo];
        }
    }

    u32 result = 0;
    result |= up != 0 ? BlockingMap::ProbeUp : 0u;
    result |= upLeft != 0 ? BlockingMap::ProbeUpLeft : 0u;
    result |= upRight != 0 ? BlockingMap::ProbeUpRight : 0u;
    result |= down != 0 ? BlockingMap::ProbeDown : 0u;
    result |= downLeft != 0 ? BlockingMap::ProbeDownLeft : 0u;
    result |= downRight != 0 ? BlockingMap::ProbeDownRight : 0u;
    result |= left != 0 ? BlockingMap::ProbeLeft : 0u;
    result |= right != 0 ? BlockingMap::ProbeRight : 0u;
    return result;
}

}  // namespace swd3e
