#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <span>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "core/point.h"
#include "core/types.h"
#include "crt/msvc.h"
#include "draw/painter.h"
#include "file/file.h"
#include "platform/display.h"
#include "platform/surface.h"
#include "talk/event_scan.h"
#include "text/big5.h"

namespace swd3e
{

class Game;
class Surface;
struct Picture;
struct MapOverlay;
struct Role;
struct ScriptAnimation;
struct ScriptObject;

// A chunk of a map movie (0x24 bytes, the list at 0x004CAD28): where it lies in the file, its size and rows, and the
// size, rows and word of the chunk after it (read from its last 12 bytes).
struct MovieChunk
{
    static constexpr u32 TrailerBytes = 0xC;  // the next chunk's word, rows and size end each chunk

    u32 offset = 0;                    // +0x00
    u32 size = 0;                      // +0x04
    u32 rows = 0;                      // +0x08
    u32 unk0C = 0;                     // +0x0C
    u32 nextSize = 0;                  // +0x10
    u32 nextRows = 0;                  // +0x14
    u32 nextUnk0C = 0;                 // +0x18
    i32 frame = 0;                     // +0x1C
    std::unique_ptr<MovieChunk> next;  // +0x20
};

// The header of a map movie (0x24 bytes): its first chunk (FirstChunkBytes) and that chunk's trailer.
struct MovieHeader
{
    static constexpr std::size_t Bytes = 0x24;
    static constexpr u32 FirstChunkBytes = 0x18;

    u32 unk00 = 0;         // +0x00
    u32 lastFrame = 0;     // +0x04
    u16 bitsPerPixel = 0;  // +0x08 8: the frames index the palettes
    u16 unk0A = 0;         // +0x0A
    u16 unk0C = 0;         // +0x0C
    u16 active = 0;        // +0x0E
    u16 height = 0;        // +0x10 the rows of a frame
    u16 unk12 = 0;         // +0x12
    u16 paletteCount = 0;  // +0x14
    u16 unk16 = 0;         // +0x16
    u32 nextUnk0C = 0;     // +0x18 the trailer of the first chunk
    u32 nextRows = 0;      // +0x1C
    u32 nextSize = 0;      // +0x20

    static MovieHeader Read(std::span<const u8, Bytes> bytes);
};

// The movie played over the map (opened by the talk scripts from Video\): LZO-packed frames of rows drawn centred,
// 8-bit frames through palettes changing at given frames, faded in and out with black bars.
struct MapMovie
{
    static constexpr u32 FileHeaderBytes = 0xC;  // the chunk offsets count from after it
    static constexpr u32 PaletteColors = 0x100;
    static constexpr std::size_t RedAt = 0;  // a palette entry: red, green and blue bytes
    static constexpr std::size_t GreenAt = 1;
    static constexpr std::size_t BlueAt = 2;
    static constexpr u32 PaletteBytes = 3 * PaletteColors;    // RGB triplets, stored before PaletteTailBytes at the end
    static constexpr u32 PaletteTailBytes = 0x400;            // the frames the palettes change at (u16 each)
    static constexpr u16 IndexedBits = 8;                     // MovieHeader::bitsPerPixel of a paletted movie
    static constexpr std::size_t FrameBufferBytes = 0x9C400;  // the unpacked frame and the packed chunk buffers

    // An RGB triplet of a palette as an RGB555 pixel (the top five bits of each channel).
    static u16 Rgb555(u8 red, u8 green, u8 blue);

    // The bits of `options`.
    enum Option : u32
    {
        DarkenOut = 1,  // the fade out darkens the last frame instead of closing black bars
        NoFadeIn = 2,   // the movie starts without the bars opening
        Paused = 0x10,
    };

    static constexpr i32 FadeInStart = -13;     // `frameIndex` when the bars start opening
    static constexpr int BarStep = 4;           // lines the bars move per frame
    static constexpr i32 FadeOutStart = 10000;  // `frameIndex` once the last frame was shown
    static constexpr i32 DarkenFrames = 0xF;    // the length of the darkening fade out
    static constexpr i32 CloseFrames = 0x1E;    // the length of the closing fade out

    File file;                       // 0x004C2A48
    MovieChunk chunks;               // swd32 0x004CAD28 the list head
    MovieChunk* current = nullptr;   // 0x004E4DBC
    std::vector<u8> frame;           // 0x004D1C80 the unpacked frame
    std::vector<u8> buffer;          // swd32 0x004B7924 the packed chunk (also a saved screen)
    std::vector<u16> palette;        // 0x004D1F50
    std::vector<u16> paletteFrames;  // swd32 0x004AB37C the frames the palettes change at
    u16 paletteCount = 0;            // swd32 0x004B7994
    u16 paletteIndex = 0;            // swd32 0x004ACBBC
    i32 lastFrame = 0;               // swd32 0x004B729C
    i32 active = 0;                  // swd32 0x004CAE8C
    i32 height = 0;                  // 0x004E5590
    i32 frameIndex = 0;              // swd32 0x004B7AC8 below 1 the fade in, from 10000 the fade out
    i32 saved = 0;                   // swd32 0x004B88B8 the screen is saved in the buffer
    u32 options = 0;                 // swd32 0x004B7AA0 bit 0: fade out darkening, bit 1: no fade in, bit 4: paused
};

// The drawing of the map (the original's picPaint, 0x004126A0..0x00417290): its tiles, the roles by row with their
// shadows, reflections, sounds and bubbles, the script lists (animations, sprites, role heads, lines), the overlays and
// the screen effects of the map (weather, leaves, heat, lightning, fireflies, spotlight, ripples).
class FieldView
{
public:
    // The cell step of each path direction (0x004AA504 across, 0x004AA524 down), read by Render.
    static constexpr std::array<i32, 8> CellStepX = {1, 0, -1, -1, -1, 0, 1, 1};
    static constexpr std::array<i32, 8> CellStepY = {1, 1, 1, 0, -1, -1, -1, 0};

    explicit FieldView(Game& game);

    void DrawTiles16(int unused);
    void DrawTiles16Scrolled(int unused);
    void DrawTiles8(int unused);
    void DrawTiles8Scrolled(int unused);
    void DrawRoles();
    int DrawRole(Role& role);
    // The rebuild's smooth motion: the roles' places as a game frame starts (the steps it takes are drawn from them).
    void NoteStep(std::size_t index, i32 x, i32 y);
    void KeepRolePlaces();
    void ArriveMoving(std::size_t index, std::size_t move, u8 direction);
    void UpdateRoleSound(Role& role);
    void DrawShadow(const Role& role, const Picture& picture, int x, int y, int kind, int tint, int scale);
    void DrawBubbles(int x, int y, u16 role);
    void FadeColors(int step);
    void DrawAnimations(std::unique_ptr<ScriptAnimation>& list);
    void DrawSprites();
    void DrawRoleHeads();
    void DrawItemHints();
    void DrawLines();
    void BlendLines(std::span<Pixel> screen) const;
    void DrawGold();
    void DrawFinalAgain();
    void ClearRowStarts();
    void DrawOverlays();
    void DrawWeather();
    void DrawLeaves();
    void DrawHeat();
    void DrawLightning();
    void DrawFireflies();
    void DrawSpotlight();
    void DrawRipples();
    void DrawTimer(int x, int y, int which);
    // The pictures DrawTimer draws, each handed to `each` with its place (the digits' act stepped as DrawTimer steps
    // it).
    void ForTimerPictures(int x, int y, int which, const std::function<void(const Picture&, int, int)>& each);
    void FillCellMark(int x, int y, int width, int height, int color);
    void MarkCells(u32 mask, int color, int offsetX, int offsetY, int inset);
    void PanCamera();
    void ShakeCamera();
    int UpdateMovie();
    void DrawMovieFrame();
    int ReadMovieFrame();
    void Paint();
    void Render();
    // The rebuild's continuous motion (deviations.md): the controlled role and the camera moved a millisecond at a time
    // up to `now` (SystemUptime milliseconds), the controlled role settled on each tile it reaches; their places in
    // 1/scale pixels.
    void AdvanceMotion(u32 now);
    void SettlePlayer();
    std::array<i32, 2> CameraFine(int scale) const;
    std::array<i32, 2> PlayerFine(int scale) const;
    void UpdateRoleCellFlags(Role& role);

    // swd3e.exe 0x004351F0: a formatted line in the small font (a UTF-8 format; UTF-8 arguments are converted to Big5,
    // char strings are game text already)
    template <typename... Args>
    void DrawDebugText(int x, int y, u32 color, std::u8string_view format, const Args&... args)
    {
        this->DrawSmallText(x, y, color, crt::Sprintf(Big5(format), GameArgument(args)...));
    }

    void DrawDebug(int screenLeft, int screenTop);

    // The rebuild's smooth motion (deviations.md): the map drawn between two game frames with the camera, the roles
    // and the effect elements where their continuous motion has them now, the interface of the last frame
    // laid over it. `out` is the logical screen times a whole render scale N: the camera and the roles are placed to
    // 1/N of a pixel, every pixel of the game shown as an N x N block. Nothing the game frames read is changed.
    void PaintBetween(Surface& out);
    // The same laid by the GPU (field_view_gpu.cpp, reverse/analysis/smooth-gpu.md): the map recorded once a game frame
    // in layers (the still parts and each moving role apart), laid on the window where the continuous motion has them
    // now at render scale `scale`, the interface over them; false (nothing shown) when the frame holds what the layers
    // cannot show, for PaintBetween.
    bool LayBetween(int scale);

    // The rebuild's map overview (deviations.md): the whole map drawn into `out` (made its size) as a still picture,
    // its overlays, tiles and the roles of the map file (none of the story's: the people). Nothing the game frames
    // read is changed.
    void PaintOverview(Surface& out);

    // An exit of the map for the overview: a region whose event changes the map, at the middle of its cells (map
    // pixels), and the maps it leads to.
    struct MapExit
    {
        Point at;
        std::vector<u16> maps;
    };

    std::vector<MapExit> MapExits();

    // A run of pixels of the logical screen (a later step of the last game frame drew over them).
    struct Span
    {
        u32 start = 0;
        u32 length = 0;
    };

    // The pixels of `spans` of a logical layer `width` pixels across into a screen `scale` times as large, each as a
    // scale x scale block.
    static void CopySpans(
        const std::vector<Span>& spans,
        const std::vector<Pixel>& from,
        int scale,
        int width,
        std::span<Pixel> to
    );
    static void ScaleLayer(const std::vector<Pixel>& from, int scale, int width, std::span<Pixel> to);

    i32 tileX = 0;  // 0x004E6D4C the screen x of the first visible tile column
    i32 tileY = 0;  // 0x004E6D50
    MapMovie movie;
    bool movieFrame = false;          // the movie played in the last game frame (shown as drawn between game frames)
    bool replaying = false;           // PaintBetween is drawing: the state of the game frames is left alone
    std::array<i32, 2> shakeShift{};  // the quake's offset of the camera in the frame shown
    // Where the original's 640x480 screen lies on the canvas in the frame shown (Field::original less the camera): the
    // story's screen places (its animations, the spotlight) are drawn from there.
    Point originalShift;

private:
    struct TileSet;
    class PaintKept;

    void PaintWorld();
    void PaintOverviewPart();
    void ClipToSpotlight();
    void CapturePlaces();
    void CaptureLayer(std::vector<Pixel>& layer) const;

    // The move of an element of a screen effect (a weather particle, a leaf, a bolt, a firefly, a bubble, a sprite,
    // a role head, the spotlight) under continuous motion: a game frame moves the element by a step, which it then
    // takes over `length` milliseconds from `start` (the motion clock), from (fromX, fromY) to (toX, toY), its place
    // as the frame left it; whether its place is on the screen rather than the map, the game frame that last drew or
    // moved it, and where the painting under way shows it (in 1/scale pixels).
    struct EffectMotion
    {
        double fromX = 0.0;
        double fromY = 0.0;
        i32 toX = 0;
        i32 toY = 0;
        u32 start = 0;
        i32 length = 1;
        bool screen = false;
        u32 seen = 0;
        i32 fineX = 0;
        i32 fineY = 0;
    };

    // Where an effect element at (x, y) is drawn: in a game frame where its move under way has it (where it is when
    // none: a move that does not end there is over); in a painting between game frames the same, for the block column
    // and row being painted.
    std::array<i32, 2> EffectAt(u64 key, i32 x, i32 y, bool screen);
    // A game frame moved the element from (fromX, fromY) to (toX, toY): it takes the step over `frames` game frames,
    // from where it is shown now (a step further than `snap` jumps).
    void MoveEffect(u64 key, i32 fromX, i32 fromY, i32 toX, i32 toY, i32 frames, bool screen, i32 snap);
    // Its place now, in pixels.
    std::array<double, 2> EffectShown(const EffectMotion& effect) const;
    // Its place for the block column and row of a render scale painted now.
    std::array<i32, 2> EffectPlace(const EffectMotion& effect, int column, int row) const;

    bool placesValid = false;
    u32 placesFrame = 0;  // Game::gameFrames of the last map frame
    i32 placesLayer = 0;  // its tile layer (Scene::layerOffset steps on for the next frame after it is drawn)
    int placesMap = 0;
    i32 cameraWidth = 0;
    i32 cameraHeight = 0;

    // The step of the controlled role under way (continuous motion): where it and the camera started, the walking
    // speed (pixels a game frame), the directions across and down and whether the camera follows, the milliseconds
    // gone, and the places the motion left (anything else moving them ends it).
    struct PlayerMotion
    {
        bool active = false;
        i32 startX = 0;
        i32 startY = 0;
        i32 cameraX = 0;
        i32 cameraY = 0;
        i32 speed = 0;
        i32 across = 0;
        i32 down = 0;
        i32 scrollAcross = 0;
        i32 scrollDown = 0;
        i32 elapsed = 0;
        i32 leftX = 0;
        i32 leftY = 0;
        i32 leftCameraX = 0;
        i32 leftCameraY = 0;
    };

    // A role where a game frame started, its act, whether it was walking then (a step drawn or taken the frame before)
    // and held for an event (KeepRolePlaces).
    struct RolePlace
    {
        u16 id = 0;
        bool shown = false;
        i32 x = 0;
        i32 y = 0;
        ActState act;
        bool walking = false;
        bool held = false;
    };

    // A role as it is drawn (the frames move the roles as the original does; the drawing follows them): while the role
    // stays where the frames last left it (target), it is drawn going from one place to another (in 1/MotionFine
    // pixels) over `length` milliseconds, `elapsed` of them gone, then at the second. A step the frames took goes from
    // where the role was drawn to where they left it; a role stopped (or turned, or held) is drawn where it was, even
    // where the frames moved it on (`frozen`). The game frame of the last step; a step that stops a walking role (its
    // action changed) drawn with the act it had where the step started until it gets there.
    struct RoleMotion
    {
        bool active = false;
        i32 targetX = 0;
        i32 targetY = 0;
        i32 fromX = 0;
        i32 fromY = 0;
        i32 toX = 0;
        i32 toY = 0;
        i32 length = 1;
        i32 elapsed = 0;
        u32 frame = 0;
        bool stops = false;
        bool frozen = false;
        bool turning =
            false;  // a walking follower turned on its tile: drawn with `act`, its first turn's, until it steps
        // Drawn at its walk's mean pace (PaceOf): paceDistance (1/MotionFine pixels) every paceTime milliseconds.
        i32 paceTime = 0;
        i32 paceDistance = 0;
        ActState act;
    };

    static constexpr i32 MotionFine = 0x100;  // RoleMotion places: parts of a pixel

    // The pan of the camera under way: the step a frame took, the milliseconds gone and the camera it left.
    struct PanMotion
    {
        bool active = false;
        i32 fromX = 0;
        i32 fromY = 0;
        i32 byX = 0;
        i32 byY = 0;
        i32 elapsed = 0;
        i32 leftX = 0;
        i32 leftY = 0;
    };

    static constexpr i32 RoleStepMostFrames = 0x80;  // a role whose act holds longer has stopped: it steps in a frame

    // The pace of a walk: the game frames its next steps take and how many steps; uneven when they are not all alike.
    struct WalkPace
    {
        i32 frames = 0;
        i32 steps = 0;
        bool uneven = false;
    };

    void MotionTick();
    void BeginPlayerMotion();
    void EndPlayerMotion(bool atFrame);
    i32 MotionPixels(int scale) const;
    void CaptureRoleSteps(bool continuous);
    void RoleTick(std::size_t index);
    bool RoleOnStep(std::size_t index) const;
    bool RoleStopping(std::size_t index) const;
    bool RoleTurning(std::size_t index) const;
    i32 StepsToEnd(std::size_t index, const RolePlace& from) const;
    std::size_t RoleIndex(const Role& role) const;

    // The step of the zoom phases at render scale `parts` (DrawRole): the zoomed pictures are sampled at twice the
    // original's resolution at an even render scale (the steps part a mirrored picture's phases as an upright one's),
    // at every part of an odd one.
    static int ZoomPhaseStep(int parts)
    {
        constexpr int kSamplingParts = 2;
        return parts % kSamplingParts == 0 ? parts / kSamplingParts : 1;
    }

    WalkPace PaceOf(const ActState& act);
    int ZoomAnchorX(const Role& role);
    std::array<i32, 2> MotionShown(const RoleMotion& step) const;
    void ArriveFollower(std::size_t index, std::size_t move, u8 direction);
    void StartPan(i32 byX, i32 byY);
    void PanTick();
    std::array<i32, 2> RoleFine(std::size_t index, int scale) const;
    PlayerMotion playerMotion;
    std::vector<RolePlace> framePlaces;  // per role
    // The steps each role walked since its place was kept (the frames' moves and the controlled role's motion): what
    // else moved it put it there (CaptureRoleSteps).
    std::array<Point, GameState::Roles> frameSteps{};
    u32 framePlacesFrame = 0;             // the game frame they were kept for
    std::vector<RoleMotion> roleMotions;  // per role
    PanMotion panMotion;
    u32 motionClock = 0;
    bool motionClockSet = false;
    int motionMap = 0;  // the map the motions under way are on
    std::unordered_map<u64, EffectMotion> effectMotions;
    // The painting PaintBetween does: its render scale, its camera (in 1/scale pixels), the block column and row.
    int paintScale = 1;
    i32 paintCameraX = 0;
    i32 paintCameraY = 0;
    int paintColumn = 0;
    int paintRow = 0;

    // Every role's place in 1/paintScale pixels (RoleFine): the place its depth scale is read at on every block.
    std::array<i32, GameState::Roles> paintRoleX = {};
    std::array<i32, GameState::Roles> paintRoleY = {};
    // The places PaintBetween last painted at (the camera's, then every role's), its game frame and its target.
    std::vector<i32> paintedKey;
    std::vector<i32> placesKey;
    u32 paintedFrame = 0;
    const Pixel* paintedTarget = nullptr;
    bool paintedValid = false;
    bool overview = false;  // PaintOverview is drawing: the roles of the map file alone

    // The rebuild's map filling the window (deviations.md): the canvas of the map frame (Game::mapCanvas), and where
    // the 640x480 screen the interface is drawn on starts in it.
    std::span<Pixel> Canvas() const;
    // The map is drawn larger than the 640x480 screen.
    bool FillsWindow() const;
    // The tile rows the canvas spans (15 on the 640x480 screen).
    int ViewRows() const;
    std::size_t ScreenAt() const;
    // The first row of the 640x480 screen on the surface drawn (the movie plays on its rows across the canvas).
    u8* MovieScreen() const;
    // Black on the canvas above and below the 640x480 screen, and beside it with `beside`.
    void BlackenAroundScreen(bool beside);

    std::vector<Pixel> finalLayer;  // the last game frame as shown (scaled up between game frames while a movie plays)

    // The rebuild's smooth motion: the blends of the screen lines the game frame drew (DrawLines), drawn again over the
    // map painted between game frames (BlendLines) before its talk windows (Talk::DrawAgain).
    struct LineBlend
    {
        i64 at = 0;
        u32 color = 0;
        int pixels = 0;
    };

    std::vector<LineBlend> lineBlends;
    ActState itemHint;                    // DrawItemHints: the arrow, stepped once a game frame
    EventScan eventScan;                  // DrawItemHints: the events that give items
    bool pointerShown = false;            // the frame drew the pointer
    std::array<i32, 2> pointerSway = {};  // the offset of the pointer's picture last drawn to (EffectKind::Pointer)
    std::vector<Surface> parts;           // the logical screens of a render scale above 1 (PaintBetween)

    // LayBetween (field_view_gpu.cpp): the layers of the game frame recorded last, in drawing order: still layers and
    // the sprites of the moving roles between them. A layer's pixels are in halves of a pixel (the zoomed pictures
    // sampled at twice the original's resolution); a still layer lies on the recorded canvas (`recordLeft`,
    // `recordTop` its corner on the map), a sprite at its role's place (`originX`, `originY` the role's place on it).
    // `shaded`: its shadows are a layer of their own darkening what lies under it.
    struct GpuLayer
    {
        Display::Layer color;
        Display::Layer shade;
        bool shaded = false;
        bool drawn = false;
        std::size_t role = GameState::Roles;  // the moving role of a sprite (Roles: a still layer)
        i32 left = 0;                         // what it holds, in halves of a pixel of its canvas
        i32 top = 0;
        i32 width = 0;
        i32 height = 0;
        i32 originX = 0;  // a sprite: the role's place on it (pixels)
        i32 originY = 0;
        i32 canvasWidth = 0;  // its canvas (pixels)
        i32 canvasHeight = 0;
        i32 spriteHeight =
            0;  // a sprite: what its role's drawing takes of its canvas (the canvas holds the font's rows)
        // Its canvases of the recording's blocks (colour over black, over white, shade) and what was drawn on them
        // (pixels).
        std::array<Surface, 4> colors;
        std::array<Surface, 4> whites;
        std::array<Surface, 4> shades;
        std::array<i32, 4> dirty = {};
        // What its first block held when it was last handed to the GPU (an unchanged layer is kept as it is).
        std::vector<Pixel> kept;
        std::array<i32, 4> keptDirty = {};
        std::size_t keptRole = GameState::Roles + 1;
        i32 keptWidth = 0;
        i32 keptHeight = 0;
        bool changed = false;
        bool single =
            false;  // handed over at its canvas's resolution (the ground, a role not zoomed), else at twice it
        bool additive = false;  // a sprite of a role drawn added to what is under it: its colour over black, added
    };

    enum class RecordPass
    {
        None,
        Color,  // over black (the first layer as it is)
        White,  // the layers past the first over white
        Shade,  // their shadows over white
    };

    // The screen effects moving between game frames (script sprites, leaves, lightning, fireflies, weather, the
    // spotlight), recorded as draws once a painting (Painter::SetRecorder) and laid by the GPU over the map's layers.
    struct EffectDraw
    {
        bool tint = false;                // a tint of an area (`area`, `tintLevels`), or a picture (`command`)
        std::array<float, 2> shift = {};  // what its element is off the pixel it is drawn at (EffectAt)
        DrawCommand command;
        std::array<i32, 4> area = {};  // x, y, width, height
        std::array<int, 3> tintLevels = {};
    };

    class EffectRecorder;
    void RecordEffects();
    void RecordOverlays();
    // The heat haze and the ripples while recording effects: what each pixel of the canvas then shows, as the pixel
    // of the map laid before them (distortSource), handed to the GPU as pieces copied within the screen.
    void DistortMove(std::ptrdiff_t to, std::ptrdiff_t from, std::size_t count);
    void DistortRipple(const ScriptObject& ripple, std::size_t pixels);
    void DistortTouch(std::ptrdiff_t at, std::size_t count);
    void DistortPieces();
    void LayEffects(const std::vector<EffectDraw>& draws, std::size_t first, std::size_t end);
    const Display::Layer* EffectTexture(const DrawCommand& command, u32 decodeMode, bool mask);

    bool RecordLayers();
    void ScanDirty(GpuLayer& layer);
    void RecordStill();
    void ClaimLayer(std::size_t at, std::size_t role);
    bool RecordSprite(Role& role);
    void RecordTarget();
    void RecordDirty(i32 left, i32 top, i32 right, i32 bottom);
    static void WeaveBlocks(
        const std::array<Surface, 4>& blocks,
        const std::array<i32, 4>& dirty,
        std::vector<Pixel>& woven
    );
    bool LayInterface(int mouseX, int mouseY);

    std::vector<GpuLayer> gpuLayers;
    std::vector<i32> gpuKey;  // what the layers were recorded for (LayBetween)
    std::vector<i32> gpuKeyNow;
    bool gpuValid = false;
    i32 recordLeft = 0;  // the corner of the recorded canvas on the map (pixels)
    i32 recordTop = 0;
    RecordPass recordPass = RecordPass::None;
    std::size_t recordLayer = 0;  // the layer the recording draws on
    std::size_t recordBlock = 0;
    bool recordInSprite = false;
    bool recordStillPending = false;  // RecordSprite: the layer drawn on is a sprite done with (RecordStill)
    // Past the first block's colour pass, a layer whose first block did not change is not drawn again (its roles
    // and the ground skipped, what else is drawn going to recordSkipScratch).
    bool recordSkip = false;
    Surface recordSkipScratch;
    Surface effectScratch;  // RecordEffects, RecordOverlays: what an effect draws on the canvas itself
    // The map has an overlay with a parallax: every overlay is laid by the GPU where the camera glides (gpuOverlays,
    // in their order), the first layer recorded without them.
    bool recordOverlays = false;
    std::vector<EffectDraw> gpuOverlays;
    const MapOverlay* gpuOverlaysOf = nullptr;  // the overlays the pictures of gpuPictures were decoded for
    bool recordFailed = false;
    std::array<i32, GameState::Roles> recordSpriteOf = {};  // the layer of each moving role's sprite (0: none)
    std::vector<Pixel> gpuWoven;                            // a layer woven from its blocks (over black)
    std::vector<Pixel> gpuWovenWhite;                       // over white
    Surface recordScratch;  // the first layer's canvas in the white and shade passes (nothing is kept of it)
    std::vector<EffectDraw> gpuEffects;           // RecordEffects
    std::size_t gpuEffectsBefore = 0;             // those drawn before the heat haze and the ripples
    std::vector<i32> distortSource;               // identity outside the rows of distortRows
    std::vector<i32> distortCopy;                 // a ripple's copy of the screen
    std::vector<std::array<i32, 2>> distortRows;  // the columns each row has touched (first, end)
    std::vector<Display::MapPiece> gpuPieces;
    bool distortValid = false;  // gpuPieces hold the bends of the game frame and canvas of distortKey
    std::array<i32, 3> distortKey = {};
    bool recordingEffects = false;            // RecordEffects under way
    std::array<float, 2> effectShift = {};    // EffectAt while recording effects: the element's shift off its pixel
    std::array<float, 2> gpuCamera = {};      // the camera laid (canvas pixels, fractions kept)
    std::array<i32, 2> gpuScreenCamera = {};  // the camera's pixel nearest it
    bool gpuSpotlightMoving = false;          // the spotlight glides: shown at gpuSpotlight
    std::array<i32, 2> gpuSpotlight = {};
    // The pictures of the effects as textures (their source, palette, argument, size and mode decoded).
    std::map<std::tuple<const void*, const void*, const void*, u32, i32, i32, u32>, Display::Layer> gpuPictures;
    std::array<Surface, 2> gpuDecode;  // a picture decoded over black, over white
    Display::Layer gpuFinal;           // the last game frame as shown (a movie, LayBetween)
    std::vector<Pixel> gpuFinalShown;  // what was last given to gpuFinal
    Display::Layer gpuInterface;
    std::array<Surface, 2> gpuInterfaceCanvas;            // drawn over black, over white
    std::array<std::vector<Pixel>, 2> gpuInterfaceShown;  // what was last given to gpuInterface
    bool fadeTinted = false;

    // The levels the fade tinted the last game frame by.
    struct FadeLevels
    {
        int red = 0;
        int green = 0;
        int blue = 0;
    };

    FadeLevels fadeLevels;

    void DrawSmallText(int x, int y, u32 color, const std::string& text);
    void DrawTileGrid(const TileSet& tiles);
    void DrawTileGridScrolled(const TileSet& tiles);
    void DrawTileCanvas(const TileSet& tiles);
    void DrawHeatCanvas(const std::function<void(std::ptrdiff_t, std::ptrdiff_t, std::size_t)>& move);
    u32 Block(int index) const;
    u16 Tile(int index) const;
    void SetTileSource(const TileSet& tiles, int index, bool wideRead = false);
    void DrawScreenTint(int red, int green, int blue);

    Game& game;
};

}  // namespace swd3e
