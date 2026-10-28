#pragma once

#include <array>
#include <cstdint>
#include <span>

#include "core/types.h"

namespace swd3e
{

class Surface;

// Extra data of the effects that take one (the original passes a pointer as the sixth argument).
struct DrawArgument
{
    const u8* alphaMap = nullptr;  // kAlphaMap: one level byte (multiple of 4) per picture pixel
    u32 colorPair = 0;             // kSilhouette: the colour in both 16-bit halves
};

struct Picture;

// The palette of a picture as the draw argument of an effect (the original passes the palette pointer).
DrawArgument PaletteArgument(const Picture& picture);

// Options read by the next DrawPicture and reset after it (the original's draw option globals).
struct DrawOptions
{
    bool dropLines = false;  // every third line is skipped (kAdditive, kSubtractive)
    int tintRed = 0;         // 0x004ED9A4 tint steps per channel, in 1/16 of the channel range
    int tintGreen = 0;       // 0x004EC180
    int tintBlue = 0;        // 0x004EC178
    int unusedFlag = 0;      // reset like the others, read by no effect
    int scaleHeight = 0;     // output height of the vertically scaled effects (0: unscaled)
    int shear = 0;           // horizontal shear over the picture height (kShadow)
    int alpha = 0;           // 0x004E6D58 kTranslucent level 1..15
    // The rebuild's smooth motion (a menu frame painted between frames): a translucent draw that far (0..1) from
    // `alpha` towards the next level, the colours of the two levels mixed.
    double alphaFraction = 0.0;

    bool operator==(const DrawOptions& other) const = default;
};

// One colour channel of the display format.
struct ColorChannel
{
    static constexpr int Levels = 32;  // a 5-bit channel
    static constexpr std::size_t Shifts = 5;
    static constexpr std::size_t TintHalf = 4;  // halves[TintHalf] is one tint step

    u32 mask = 0;
    u32 inverseMask = 0;  // 0xFFFFFFFF - mask
    int shift = 0;
    // halves[k]: the mask shifted right by k with the bits of the channel kept; halves[4] is one tint step.
    std::array<u32, Shifts + 1> halves{};
    // steps[k]: (k + 1) << shift.
    std::array<u32, Shifts> steps{};
    // Saturating sum lookup: 32 zero entries, the ramp k * halves[4] for k = 0..31, 32 entries of the mask.
    std::array<u32, 3 * Levels> ramp{};

    u32 Ramp(int index) const;
};

// Colours of the RLE format in the display format (the codes 0x8000 / 0xC000 stand for runs of them).
struct RleColors
{
    u32 magic = 0xFFFF;
    u16 fill16 = 0x26B;   // 0xC000 runs
    u16 skip16 = 0x319F;  // 0x8000 runs
    u8 fill8 = 1;
    u8 reserved8 = 2;
    u8 skip8 = 3;
};

// The drawing kernel (0x00417510-0x004277DE): pixel format detection and colour tables, clipping, and the blitters
// of RLE pictures (see pic/rle.h), raw pictures and 16x16 map tiles on a 16-bit RGB surface.
class Painter
{
public:
    // Draw modes of Painter::DrawPicture. Bit 0 mirrors horizontally, bit 1 flips vertically (only honoured by the
    // effects that advance lines by the signed pitch), bits 2..15 select the effect and bit 31 marks RLE source data
    // (set automatically for data starting with the RLE magic when no palette is selected).
    enum DrawMode : u32
    {
        Mirror = 0x1,
        Flip = 0x2,
        RleSource = 0x80000000u,
        EffectMask = 0xFFFC,

        // RLE effects.
        Copy = 0x00,         // literal pixels copied
        Additive = 0x04,     // saturating add, optionally tinted
        AlphaMap = 0x08,     // per-pixel level from a byte map (DrawArgument::alphaMap)
        Shadow = 0x0C,       // sheared and scaled tint of the destination
        TintUnder = 0x10,    // tint of the destination under the picture
        Translucent = 0x14,  // blend at DrawOptions::alpha
        EdgeMark = 0x18,     // copy, first and last pixel of each run marked
        FadeDown = 0x1C,     // blend level falling from the top
        ScaledGlow = 0x20,   // vertically scaled tinted add
        Silhouette = 0x24,   // literal pixels filled with DrawArgument::color
        GrayUnder = 0x28,    // destination under the picture turned grey
        Subtractive = 0x2C,  // saturating subtract, tinted
        SmearUnder = 0x30,   // destination smeared under the picture

        // Raw effects (16-bit pixels or palette indices).
        RawCopy = 0x00,
        RawKeyed = 0x04,        // fill colour transparent
        RawGradient = 0x08,     // destination blended towards the first source pixel
        RawTranslucent = 0x14,  // blend at DrawOptions::alpha, fill colour transparent

        // swd3e: the RLE source scaled by SetZoom, drawn without the clipping of DrawPicture (each clips itself). The
        // modes from ZoomModes on (bits 2..29) select them; bits 0 and 1 still mirror and flip.
        ZoomCopy = 0x80,
        ZoomAddTinted = 0x84,       // the source added to the destination, then tinted
        ZoomTint = 0x90,            // the destination under the picture tinted (a shadow)
        ZoomTranslucent = 0x94,     // blend at DrawOptions::alpha
        ZoomSilhouette = 0xA4,      // filled with ZoomFill
        ZoomGray = 0xA8,            // the destination under the picture turned grey
        ZoomSubtractTinted = 0xAC,  // the tinted source subtracted from the destination
        PaletteCopy = 0xC0,         // unscaled 8-bit RLE through the palette
    };

    static constexpr u32 RawBlitters = 0x40;  // the blitters of raw pictures follow the RLE ones (swd32: 0x80)
    static constexpr u32 ZoomModes = 0x80;
    static constexpr u32 ZoomModeMask = 0x3FFFFFFC;
    static constexpr std::size_t ZoomTableSize = 0x500;  // the entries of the zoom tables (0x00423670)
    static constexpr u16 ZoomFill = 0;                   // 0x004ED9DC, read by ZoomSilhouette and written by nothing
    static constexpr int TileSide = 16;                  // DrawTile16 and DrawTile8
    static constexpr int MaxAlpha = 0xF;                 // DrawOptions::alpha of a fully opaque blend
    static constexpr int FractionBits = 10;              // the scale and shear steps are fixed point
    static constexpr int DitherPhases = 3;
    static constexpr u8 KeyIndex = 1;             // DrawTile8Keyed: the transparent palette index
    static constexpr int WaveRowBytes = 0x84;     // a row of the wave offsets (33 dwords)
    static constexpr int WideWaveStride = 0x528;  // the stride of the tint and grey walks
    static constexpr int WaveEntryBytes = sizeof(i32);

    // How the display stores colours; pictures are stored as RGB555 and converted on load.
    enum class Conversion
    {
        None,    // RGB555 display
        To5551,  // R5 G5 B5 A1
        To565,   // RGB565 (the converter of the reverse direction is set as well)
        To6551,  // R6 G5 B5 with the low red bit left over
    };

    Painter();

    // Pixel format and screen geometry of the screen surface; without a surface the RGB555 defaults are used.
    bool Init(Surface& screen);
    // The rebuild (deviations.md): the screen drawn to is `width` x `height` pixels from now on (the map filling
    // the window draws to a larger one), the clip the whole of it; the pixel format stays.
    void SetScreen(int width, int height);
    bool InitPixelFormat(const Surface* screen);
    void ColorMasks(u32& red, u32& green, u32& blue) const;
    u32 FillColorPair() const;
    void ConvertPixels(std::span<u16> pixels) const;
    void ConvertPixelsBack(std::span<u16> pixels) const;
    u32 MakeColor(u32 red, u32 green, u32 blue) const;
    const RleColors& Rle() const;
    u16 SetRleFill(u16 fill);
    const ColorChannel& Red() const;
    const ColorChannel& Green() const;
    const ColorChannel& Blue() const;

    // Target surface and clipping.
    u16* LockSurface(Surface& surface);
    void UnlockSurface(Surface& surface);
    void SetTarget(u16* pixels);
    u16* Target() const;
    bool SetClip(int left, int top, int right, int bottom);
    void SetClipClamped(int left, int top, int right, int bottom);
    // The clip rectangle as left, top, width and height.
    std::array<int, 4> ClipRect() const;
    // Pixels per line of the screen.
    int ScreenPitch() const;
    // The channel bits kept by (pair >> (index + 1)) in both halves of a pixel pair.
    u32 HalfMaskPair(int index) const;

    // Source and persistent effect state.
    void SetSource(const void* data);
    const void* Source() const;
    void SetPalette(const u16* palette);
    DrawOptions& Options();
    void SetWave(int mode);
    int WavePhase() const;
    void SetWavePhase(int phase);
    void SetZoom(int anchorX, int anchorY, int zoomX, int zoomY);
    void SetZoomSheared(int anchorX, int anchorY, int zoomX, int zoomY, int shear);

    // Drawing.
    int DrawPicture(int x, int y, int width, int height, u32 mode, const DrawArgument& argument = {});
    void DrawOutline(int x, int y, int width, int height, u32 mode, const DrawArgument& argument = {});
    void DrawTile16(int x, int y);
    void DrawTile16Keyed(int x, int y);
    void DrawTile8(int x, int y);
    void DrawTile8Keyed(int x, int y);
    template <typename Write>
    void ForTilePixels(int x, int y, Write&& write) const;
    void DrawZoomed(int x, int y, int width, int height);
    void DrawZoomedMirrored(int x, int y, int width, int height);

    // Pixel operations on arrays of display pixels.
    void BlendPairsWithColor(std::span<u32> pairs, u32 colorPair) const;
    void TintPixels(std::span<u16> pixels, int red, int green, int blue) const;
    // The rebuild's: what TintPixels gives pixel by pixel, faster. With every tint step the lowest bit of its channel
    // (the formats the game sets up) a tinted channel is its value plus the level kept within the channel, done here
    // 16 (AVX2) or 8 (SSE2) pixels at a time; other formats go pixel by pixel.
    void TintPixelsFast(std::span<u16> pixels, int red, int green, int blue) const;

    // The widest instructions TintPixelsFast uses (for the tests and benchmarks of each path): the best the processor
    // runs (AVX2 or SSE2), SSE2, or none.
    enum class TintPath
    {
        Best,
        Sse2,
        Scalar,
    };

    static void SetTintPath(TintPath path);
    void ShiftRed(std::span<u16> pixels, int delta) const;
    void ShiftGreenPairs(std::span<u16> pixels, int delta) const;
    void ShiftBluePairs(std::span<u16> pixels, int delta) const;
    void MixPixels(std::span<const u16> other, std::span<u16> pixels);
    void GrayPixels(std::span<u16> pixels) const;

    // Per-line horizontal offsets of the wave effect: 15 rows of 33 (0x004B16B8).
    static std::span<const i32> WaveOffsets();

private:
    struct Placement;

public:
    // The rebuild's smooth battle (deviations.md): what a draw reads besides its arguments, kept with a recorded draw
    // to draw it again alike.
    struct DrawState;
    DrawState SaveDrawState() const;
    void RestoreDrawState(const DrawState& state);

private:
    // Placement of the current DrawPicture after clipping.
    struct Placement
    {
        int skipTop = 0;
        int skipBottom = 0;
        int skipLeft = 0;
        int width = 0;  // visible columns
        int rows = 0;   // visible lines
        u16* dest = nullptr;
        int pitch = 0;  // pixels between lines, negative when flipped
        const u8* sourceLine = nullptr;
        int ditherPhase = 0;
        u32 scaleFraction = 0;
        bool scaleUp = false;
        int shearStep = 0;
        int shearDirection = 0;
    };

    // Run of an RLE line handed to the effect operations: kind, the literal pixels and how far its visible part
    // lies from the first drawn column.
    enum class RunKind
    {
        Literal,  // code 0x0000 | n, n pixels follow
        Fill,     // code 0xC000 | n (and 0x4000 | n), the fill colour
        Skip,     // code 0x8000 | n, the skip colour
    };

    // How an effect classifies the run codes of a line (see ForEachChainedRun).
    enum class RunChain
    {
        Checked,          // both kind bits of every code
        LiteralThenSkip,  // the code after a drawn literal run is taken as a 0x4000/0x8000 run unchecked
        Split,            // the alpha map and tint effects: the next kind follows from the previous one
    };

    struct Tint
    {
        u32 red = 0;
        u32 green = 0;
        u32 blue = 0;
        u32 redSaturated = 0;
        u32 greenSaturated = 0;
        u32 blueSaturated = 0;
    };

    struct LineWalk
    {
        bool mirrored = false;
        bool signedPitch = false;  // lines advance by Placement::pitch (else by the screen pitch, ignoring kFlip)
        bool dropLines = false;    // honour DrawOptions::dropLines
        bool advanceWave = true;
        int waveStride = WaveRowBytes;  // bytes between the wave rows (WideWaveStride in some effects)
        bool scaled = false;            // vertical scaling (kScaledGlow)
        RunChain chain = RunChain::Checked;
    };

    using Blitter = int (Painter::*)(int x, int y, int width, int height, const DrawArgument& argument);

    void BuildBlitTables();
    int Dispatch(u32 index, int x, int y, int width, int height, const DrawArgument& argument);
    u16* Pixel(int x, int y) const;
    std::ptrdiff_t RowOffset(int y) const;

    // Pixel helpers.
    u16 Blend(u16 dest, u16 source, int level) const;
    // Blend at `level`, or between it and the next level by DrawOptions::alphaFraction.
    u16 BlendFine(u16 dest, u16 source, int level) const;
    u16 AlphaBlend(u16 dest, u16 source, int level) const;
    u32 AlphaBlendPair(u32 dest, u32 source, int level) const;
    Tint MakeTint(int red, int green, int blue) const;
    Tint CurrentTint() const;
    bool Tinted() const;
    u16 TintPixel(const Tint& tint, u16 dest) const;
    void TintPixelsEach(std::span<u16> pixels, int red, int green, int blue) const;
    u16 AddTinted(const Tint& tint, u16 source, u16 dest) const;
    u16 Add(u16 source, u16 dest) const;
    u16 SubtractTintedSource(const Tint& tint, u16 source, u16 dest) const;
    u16 SubtractFromTintedDest(const Tint& tint, u16 source, u16 dest) const;
    u16 Gray(u16 dest) const;
    void Smear(u16* at);

    // RLE line walking.
    void WaveBegin(int stride);
    u16* WaveLine(u16* dest);
    void WaveAdvance();
    u32 ScaleStep(u32& fraction) const;
    const u8* ScaledSkipTop(const u8* lines, u32& fraction) const;
    const u8* ScaledNextLine(const u8* line, u32& fraction) const;
    template <typename RunOp>
    void ForEachRun(const u8* codes, int start, int width, RunOp&& op) const;
    template <typename RunOp>
    void ForEachChainedRun(const u8* codes, int start, int width, RunChain chain, RunOp&& op) const;
    template <typename LineOp, typename SkipHook>
    void ForEachLine(const LineWalk& walk, LineOp&& op, SkipHook&& skippedLine);
    template <typename RunOp>
    void WalkRuns(const LineWalk& walk, RunOp&& op);

    // RLE blitters.
    int BlitCopy(int x, int y, int width, int height, const DrawArgument& argument);
    int BlitCopyMirrored(int x, int y, int width, int height, const DrawArgument& argument);
    int BlitAdditive(int x, int y, int width, int height, const DrawArgument& argument);
    int BlitAdditiveMirrored(int x, int y, int width, int height, const DrawArgument& argument);
    int BlitAlphaMap(int x, int y, int width, int height, const DrawArgument& argument);
    int BlitAlphaMapMirrored(int x, int y, int width, int height, const DrawArgument& argument);
    int BlitShadow(int x, int y, int width, int height, const DrawArgument& argument);
    int BlitShadowMirrored(int x, int y, int width, int height, const DrawArgument& argument);
    int BlitTint(int x, int y, int width, int height, const DrawArgument& argument);
    int BlitTintMirrored(int x, int y, int width, int height, const DrawArgument& argument);
    int BlitTranslucent(int x, int y, int width, int height, const DrawArgument& argument);
    int BlitTranslucentMirrored(int x, int y, int width, int height, const DrawArgument& argument);
    int BlitEdgeMark(int x, int y, int width, int height, const DrawArgument& argument);
    int BlitEdgeMarkMirrored(int x, int y, int width, int height, const DrawArgument& argument);
    int BlitFadeDown(int x, int y, int width, int height, const DrawArgument& argument);
    int BlitScaledGlow(int x, int y, int width, int height, const DrawArgument& argument);
    int BlitScaledGlowMirrored(int x, int y, int width, int height, const DrawArgument& argument);
    int BlitSilhouette(int x, int y, int width, int height, const DrawArgument& argument);
    int BlitSilhouetteMirrored(int x, int y, int width, int height, const DrawArgument& argument);
    int BlitGray(int x, int y, int width, int height, const DrawArgument& argument);
    int BlitGrayMirrored(int x, int y, int width, int height, const DrawArgument& argument);
    int BlitSubtractive(int x, int y, int width, int height, const DrawArgument& argument);
    int BlitSubtractiveMirrored(int x, int y, int width, int height, const DrawArgument& argument);
    int BlitSmear(int x, int y, int width, int height, const DrawArgument& argument);
    int BlitSmearMirrored(int x, int y, int width, int height, const DrawArgument& argument);
    template <typename LiteralOp>
    void WalkShear(bool mirrored, int x, int y, int width, LiteralOp&& literal);

    // Raw blitters.
    int BlitRawCopy(int x, int y, int width, int height, const DrawArgument& argument);
    int BlitRawCopyMirrored(int x, int y, int width, int height, const DrawArgument& argument);
    int BlitRawKeyed(int x, int y, int width, int height, const DrawArgument& argument);
    int BlitRawKeyedMirrored(int x, int y, int width, int height, const DrawArgument& argument);
    int BlitRawTranslucent(int x, int y, int width, int height, const DrawArgument& argument);
    int BlitRawGradient(int x, int y, int width, int height, const DrawArgument& argument);

    // Zoomed blitters (swd3e), each with the mode for its mirror and flip bits.
    using ZoomBlitter = int (Painter::*)(int x, int y, int width, int height, u32 mode);
    void BuildZoomTables();
    int DispatchZoom(u32 index, int x, int y, int width, int height, u32 mode);
    template <typename PixelOp>
    int BlitZoomed(int x, int y, int width, int height, u32 mode, bool keepsClipTop, PixelOp&& pixel);
    int BlitZoomCopy(int x, int y, int width, int height, u32 mode);
    int BlitZoomAddTinted(int x, int y, int width, int height, u32 mode);
    int BlitZoomTint(int x, int y, int width, int height, u32 mode);
    int BlitZoomTranslucent(int x, int y, int width, int height, u32 mode);
    int BlitZoomSilhouette(int x, int y, int width, int height, u32 mode);
    int BlitZoomGray(int x, int y, int width, int height, u32 mode);
    int BlitZoomSubtractTinted(int x, int y, int width, int height, u32 mode);
    int BlitPaletteCopy(int x, int y, int width, int height, u32 mode);

    // Display format.
    static const std::array<i32, 495> WaveTable;
    u32 displayRed = 0;
    u32 displayGreen = 0;
    u32 displayBlue = 0;
    Conversion conversion = Conversion::None;
    Conversion conversionBack = Conversion::None;
    ColorChannel red;
    ColorChannel green;
    ColorChannel blue;
    std::array<u32, 5> halfMaskPairs{};  // channel bits kept by (pair >> (k + 1))
    u32 greenMaskPair = 0;               // edge mark colour (green mask in both halves)
    u32 fillColorPair = 0;
    u32 skipColorPair = 0;
    RleColors rleColors;

    // Screen geometry and target.
    int screenWidth = 0;
    int screenHeight = 0;
    int screenPitch = 0;   // pixels
    int surfacePitch = 0;  // pixels of the last locked surface
    u16* target = nullptr;
    // 0x004E6D64: y * pitch for all 2048 rows (swd32 filled only the screen rows of 1024, leaving the rest 0).
    std::array<i32, 0x800> rowOffsets{};
    int clipX = 0;
    int clipY = 0;
    int clipWidth = 0;
    int clipHeight = 0;

    // Source and options.
    const u8* source = nullptr;
    const u16* palette = nullptr;
    DrawOptions options;
    Placement placement;
    int mirrorStart = 0;  // first drawn picture column of a mirrored walk
    std::array<Blitter, 256> blitters{};

    // Persistent effect state.
    int waveMode = 0;  // 0x004ED9AC the row of the wave offsets plus 1 (0: no wave)
    int wavePhase = 0;
    int waveStart = 0;
    int wavePosition = 0;
    int waveEnd = 0;
    int smearCounter = 0;
    int zoomAnchorX = 0;  // 0x004EC188 the point of the picture that stays in place
    int zoomAnchorY = 0;  // 0x004ED9C4
    int zoomX = 0x400;    // 0x004B1E7C in 1/1024
    int zoomY = 0x400;    // 0x004B1E80
    int zoomShear = 0;    // 0x004EEE74 the rows shifted sideways by zoomShears
    // The zoom tables: the source column of each target column, the source line of each target line, the shift of
    // each line; each rebuilt when its value changes (the values they were built for after them).
    std::array<i32, ZoomTableSize> zoomColumns{};  // 0x004EAD6C
    std::array<i32, ZoomTableSize> zoomRows{};     // 0x004EC5A0
    std::array<i32, ZoomTableSize> zoomShears{};   // 0x004EDA4C
    int zoomColumnsFor = 0;                        // 0x004EEE78
    int zoomRowsFor = 0;                           // 0x004EEE7C
    int zoomShearsFor = 0;                         // 0x004EEE80
    std::array<ZoomBlitter, 0x100> zoomBlitters{};
};

struct Painter::DrawState
{
    u16* target = nullptr;
    int clipX = 0;
    int clipY = 0;
    int clipWidth = 0;
    int clipHeight = 0;
    const u8* source = nullptr;
    const u16* palette = nullptr;
    DrawOptions options;
    Placement placement;
    int mirrorStart = 0;
    int waveMode = 0;
    int wavePhase = 0;
    int waveStart = 0;
    int wavePosition = 0;
    int waveEnd = 0;
    int smearCounter = 0;
    int zoomAnchorX = 0;
    int zoomAnchorY = 0;
    int zoomX = 0;
    int zoomY = 0;
    int zoomShear = 0;
};

}  // namespace swd3e
