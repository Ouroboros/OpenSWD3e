#include <cstring>

#include "draw/draw_placer.h"
#include "draw/painter.h"
#include "pic/rle.h"

namespace swd3e
{

namespace
{

constexpr int kZoomUnit = 1 << Painter::FractionBits;  // the zoom scales are in 1/1024
constexpr std::ptrdiff_t kRleLines = 8;                // the lines of an RLE picture start after its 8-byte header
constexpr u16 kRunCount = 0x3FFF;                      // a run code: its count of pixels (or a line: its bytes)
constexpr u16 kRunSkip = 0xC000;                       // a run code with these bits skips its pixels
constexpr int kPastTable = 0x7FFFFFFF;                 // a zoom table entry read past its end

u16 Word(const u8* bytes, std::ptrdiff_t index = 0)
{
    u16 value = 0;
    std::memcpy(&value, bytes + sizeof(value) * index, sizeof(value));
    return value;
}

}  // namespace

// swd32.exe 0x00422C70 (unmatched in swd3e): the RLE source scaled by the zoom factors (1/1024) so that its anchor
// point stays in place;
// 0x4000 and 0x8000 runs are transparent. Source lines above the clip rectangle are consumed while the row is above
// it; the row remainder is then cut below 0x400, dropping pending repeats. Rows below the clip bottom drop the
// remaining repeats of their source line in the same way. Positions are tracked in pixels from the target start.
void Painter::DrawZoomed(int x, int y, int width, int height)
{
    if (DrawPlacer* placer = DrawPlacer::Active(); placer != nullptr)
    {
        placer->Place(x, y, __builtin_return_address(0), this->source);
    }

    const int scaleX = this->zoomX;
    const int scaleY = this->zoomY;
    const int left = this->zoomAnchorX - this->zoomAnchorX * scaleX / kZoomUnit + x;
    int row = this->zoomAnchorY - this->zoomAnchorY * scaleY / kZoomUnit + y;

    if (left >= this->clipX + this->clipWidth || width * scaleX / kZoomUnit + left < this->clipX)
    {
        return;
    }

    if (row >= this->clipY + this->clipHeight || height * scaleY / kZoomUnit + row < this->clipY)
    {
        return;
    }

    const std::ptrdiff_t pitch = this->screenPitch;
    const std::ptrdiff_t clipTop = this->clipY * pitch;
    std::ptrdiff_t rowClipLeft = row * pitch + this->clipX;
    std::ptrdiff_t rowClipRight = rowClipLeft + this->clipWidth;
    std::ptrdiff_t rowDest = row * pitch + left;
    const u8* line = this->source + RleHeader::HeaderBytes;
    int rowFraction = 0;

    if (Word(line) != 0)
    {
        bool reached = false;

        while (!reached)
        {
            rowFraction += scaleY;

            while (rowFraction >= kZoomUnit)
            {
                if (rowClipLeft >= clipTop)
                {
                    reached = true;
                    break;
                }

                rowFraction -= kZoomUnit;
                ++row;
                rowClipLeft += pitch;
                rowClipRight = rowClipLeft + this->clipWidth;
                rowDest += pitch;
            }

            if (reached)
            {
                break;
            }

            line += Word(line) & RleHeader::CountMask;

            if (Word(line) == 0)
            {
                break;
            }
        }
    }

    rowFraction %= kZoomUnit;

    if (Word(line) == 0)
    {
        return;
    }

    while (true)
    {
        rowFraction += scaleY;

        while (rowFraction >= kZoomUnit)
        {
            rowFraction -= kZoomUnit;

            if (row >= this->clipY + this->clipHeight)
            {
                break;
            }

            std::ptrdiff_t dest = rowDest;
            const u8* code = line + sizeof(u16);
            int columnFraction = 0;

            for (u16 value = Word(code); value != 0; value = Word(code))
            {
                const int count = value & RleHeader::CountMask;
                const bool literal = (value & RleHeader::KindMask) == 0;
                const int scaled = scaleX * count / kZoomUnit;
                const u8* pixels = code + sizeof(u16);
                const std::ptrdiff_t fromLeft = dest - rowClipLeft;
                const std::ptrdiff_t over = dest + scaled - rowClipRight;
                std::ptrdiff_t lower = 0;
                std::ptrdiff_t upper = 0;
                bool drawn = true;

                if (fromLeft < 0)
                {
                    lower = rowClipLeft;
                    upper = dest + scaled;

                    if (fromLeft + scaled <= 0)
                    {
                        drawn = false;
                    }
                    else if (over > 0)
                    {
                        upper += -1 - over;
                    }
                }
                else if (fromLeft >= this->clipWidth)
                {
                    drawn = false;
                }
                else
                {
                    lower = dest;
                    upper = dest + scaled;

                    if (over > 0)
                    {
                        upper += -1 - over;
                    }
                }

                if (drawn)
                {
                    if (!literal)
                    {
                        columnFraction += scaleX * scaled;
                    }
                    else
                    {
                        std::ptrdiff_t out = dest;
                        columnFraction %= kZoomUnit;
                        bool stopped = false;

                        for (int i = 0; i < count && !stopped; ++i)
                        {
                            columnFraction += scaleX;

                            while (columnFraction >= kZoomUnit)
                            {
                                columnFraction -= kZoomUnit;

                                if (out > upper)
                                {
                                    stopped = true;
                                    break;
                                }

                                if (out >= lower)
                                {
                                    this->target[out] = Word(pixels, i);
                                }

                                ++out;
                            }
                        }
                    }
                }

                dest += scaled;
                code += literal ? sizeof(u16) + sizeof(u16) * count : sizeof(u16);
            }

            ++row;
            rowClipLeft += pitch;
            rowClipRight = rowClipLeft + this->clipWidth;
            rowDest += pitch;
        }

        line += Word(line) & RleHeader::CountMask;

        if (Word(line) == 0)
        {
            return;
        }
    }
}

// swd32.exe 0x00423020 (unmatched in swd3e): mirrored 0x00422C70. The anchor is mirrored ((width - anchor) scaled), and
// the picture is not
// drawn at all when its scaled bottom reaches the clip bottom or its top lies above the clip top. The pixels of a
// run are written from the current right edge itself leftwards (one pixel right of the mirrored span); the right
// limit is that edge, or edge + span for runs cut by the left clip edge.
void Painter::DrawZoomedMirrored(int x, int y, int width, int height)
{
    if (DrawPlacer* placer = DrawPlacer::Active(); placer != nullptr)
    {
        placer->Place(x, y, __builtin_return_address(0), this->source);
    }

    const int scaleX = this->zoomX;
    const int scaleY = this->zoomY;
    const int right = (width - this->zoomAnchorX) * scaleX / kZoomUnit + this->zoomAnchorX + x;
    int row = this->zoomAnchorY - this->zoomAnchorY * scaleY / kZoomUnit + y;

    if (right - width * scaleX / kZoomUnit >= this->clipX + this->clipWidth || right < this->clipX)
    {
        return;
    }

    if (height * scaleY / kZoomUnit + row >= this->clipY + this->clipHeight || row < this->clipY)
    {
        return;
    }

    const std::ptrdiff_t pitch = this->screenPitch;
    const std::ptrdiff_t clipTop = this->clipY * pitch;
    std::ptrdiff_t rowClipLeft = row * pitch + this->clipX;
    std::ptrdiff_t rowClipRight = rowClipLeft + this->clipWidth;
    std::ptrdiff_t rowDest = row * pitch + right;
    const u8* line = this->source + RleHeader::HeaderBytes;
    int rowFraction = 0;

    if (Word(line) != 0)
    {
        bool reached = false;

        while (!reached)
        {
            rowFraction += scaleY;

            while (rowFraction >= kZoomUnit)
            {
                if (rowClipLeft >= clipTop)
                {
                    reached = true;
                    break;
                }

                rowFraction -= kZoomUnit;
                ++row;
                rowClipLeft += pitch;
                rowClipRight = rowClipLeft + this->clipWidth;
                rowDest += pitch;
            }

            if (reached)
            {
                break;
            }

            line += Word(line) & RleHeader::CountMask;

            if (Word(line) == 0)
            {
                break;
            }
        }
    }

    rowFraction %= kZoomUnit;

    if (Word(line) == 0)
    {
        return;
    }

    while (true)
    {
        rowFraction += scaleY;

        while (rowFraction >= kZoomUnit)
        {
            rowFraction -= kZoomUnit;

            if (row >= this->clipY + this->clipHeight)
            {
                break;
            }

            std::ptrdiff_t dest = rowDest;
            const u8* code = line + sizeof(u16);
            int columnFraction = 0;

            for (u16 value = Word(code); value != 0; value = Word(code))
            {
                const int count = value & RleHeader::CountMask;
                const bool literal = (value & RleHeader::KindMask) == 0;
                const int scaled = scaleX * count / kZoomUnit;
                const u8* pixels = code + sizeof(u16);
                const std::ptrdiff_t next = dest - scaled;
                const std::ptrdiff_t fromLeft = next - rowClipLeft;
                const std::ptrdiff_t over = dest - rowClipRight;
                std::ptrdiff_t lower = 0;
                std::ptrdiff_t upper = 0;
                bool drawn = true;

                if (fromLeft < 0)
                {
                    lower = rowClipLeft;
                    upper = dest + scaled;

                    if (fromLeft + scaled <= 0)
                    {
                        drawn = false;
                    }
                    else if (over > 0)
                    {
                        upper += -1 - over;
                    }
                }
                else if (fromLeft >= this->clipWidth)
                {
                    drawn = false;
                }
                else
                {
                    lower = next;
                    upper = dest;

                    if (over > 0)
                    {
                        upper -= over + 1;
                    }
                }

                if (drawn)
                {
                    if (!literal)
                    {
                        columnFraction += scaleX * scaled;
                    }
                    else
                    {
                        std::ptrdiff_t out = dest;
                        columnFraction %= kZoomUnit;
                        bool stopped = false;

                        for (int i = 0; i < count && !stopped; ++i)
                        {
                            columnFraction += scaleX;

                            while (columnFraction >= kZoomUnit)
                            {
                                columnFraction -= kZoomUnit;

                                if (out < lower)
                                {
                                    stopped = true;
                                    break;
                                }

                                if (out <= upper)
                                {
                                    this->target[out] = Word(pixels, i);
                                }

                                --out;
                            }
                        }
                    }
                }

                dest = next;
                code += literal ? sizeof(u16) + sizeof(u16) * count : sizeof(u16);
            }

            ++row;
            rowClipLeft += pitch;
            rowClipRight = rowClipLeft + this->clipWidth;
            rowDest += pitch;
        }

        line += Word(line) & RleHeader::CountMask;

        if (Word(line) == 0)
        {
            return;
        }
    }
}

// swd3e.exe 0x00423630: SetZoomSheared without a shear (it joins 0x00423670 where the tables are built)
void Painter::SetZoom(int anchorX, int anchorY, int zoomX, int zoomY)
{
    this->zoomAnchorX = anchorX;
    this->zoomAnchorY = anchorY;
    this->zoomX = zoomX;
    this->zoomY = zoomY;
    this->zoomShear = 0;
    this->BuildZoomTables();
}

// swd3e.exe 0x00423670: the anchor, the scales and the shear of the zoomed blitters
void Painter::SetZoomSheared(int anchorX, int anchorY, int zoomX, int zoomY, int shear)
{
    this->zoomAnchorX = anchorX;
    this->zoomAnchorY = anchorY;
    this->zoomX = zoomX;
    this->zoomY = zoomY;
    this->zoomShear = shear;
    this->BuildZoomTables();
}

// Part of 0x00423670 (from 0x004236B0): each table rebuilt when its value differs from the one it was built for. A
// scale of 0 is made 1 when its table is rebuilt (a table still built for 0 leaves it 0); a shear of 0 keeps the old
// shifts, which nothing reads then.
void Painter::BuildZoomTables()
{
    auto build = [](std::array<i32, Painter::ZoomTableSize>& table, int& scale, int& builtFor)
    {
        if (builtFor == scale)
        {
            return;
        }

        if (scale == 0)
        {
            scale = 1;
        }

        for (std::size_t index = 0; index < table.size(); ++index)
        {
            table[index] = kZoomUnit * static_cast<int>(index) / scale;
        }

        builtFor = scale;
    };

    build(this->zoomColumns, this->zoomX, this->zoomColumnsFor);
    build(this->zoomRows, this->zoomY, this->zoomRowsFor);

    if (this->zoomShearsFor != this->zoomShear)
    {
        if (this->zoomShear != 0)
        {
            for (std::size_t index = 0; index < this->zoomShears.size(); ++index)
            {
                this->zoomShears[index] = static_cast<int>(index) * this->zoomShear / kZoomUnit;
            }
        }

        this->zoomShearsFor = this->zoomShear;
    }
}

// Part of 0x00417A60: the zoomed blitter of `index` (mode bits 2..29); one not set draws nothing (the original calls
// through the empty or out-of-range table entry).
int Painter::DispatchZoom(u32 index, int x, int y, int width, int height, u32 mode)
{
    if (index >= this->zoomBlitters.size() || this->zoomBlitters[index] == nullptr)
    {
        return 0;
    }

    return (this->*this->zoomBlitters[index])(x, y, width, height, mode);
}

// The walk shared by the zoomed blitters (0x00423830 and the six after it): the anchor point of the picture stays in
// place while the picture is scaled by zoomX and zoomY (mirrored around the anchor's mirror image, flipped around
// it); each target line takes the source line of zoomRows, each target column the source column of zoomColumns, the
// lines shifted by zoomShears (by the line direction) when sheared. Whole lines are drawn only within the clip rows
// (from the clip top's next row unless `keepsClipTop`); columns are checked against the line once the picture is cut
// at a side or sheared. `pixel` writes a covered target pixel from the literal pixels of the run and its source
// column. Table entries past the end count as reached and a source line table running backwards (a negative scale)
// ends the picture (the original reads on into the next globals, or counts its line up through the whole range);
// the original returns a leftover value nobody reads.
template <typename PixelOp>
int Painter::BlitZoomed(int x, int y, int width, int height, u32 mode, bool keepsClipTop, PixelOp&& pixel)
{
    const int pitch = this->screenPitch;
    std::ptrdiff_t rowStep = pitch;
    int columnStep = 1;
    int anchorX = this->zoomAnchorX;

    if ((mode & Painter::Mirror) != 0)
    {
        columnStep = -1;
        anchorX = width - this->zoomAnchorX;
    }

    int lineStep = 1;
    bool flipped = false;
    int shearLine = 0;
    int anchorY = this->zoomAnchorY;

    if ((mode & Painter::Flip) != 0)
    {
        rowStep = -pitch;
        lineStep = -1;
        flipped = true;
        shearLine = height * this->zoomY / kZoomUnit;
        anchorY = height - this->zoomAnchorY;
        y += 2 * this->zoomAnchorY - height;
    }

    const int left = x + anchorX - anchorX * this->zoomX / kZoomUnit;
    const int scaledAnchorY = anchorY * this->zoomY / kZoomUnit;
    const int top = anchorY + y - scaledAnchorY;
    const int right = width * this->zoomX / kZoomUnit + left - 1;
    const int bottom = height * this->zoomY / kZoomUnit + top - 1;
    const int clipRight = this->clipX + this->clipWidth;
    const int clipBottom = this->clipY + this->clipHeight;

    if (left >= clipRight || right < this->clipX || top >= clipBottom || bottom < this->clipY)
    {
        return 0;
    }

    const bool checksColumns = left <= this->clipX || right >= clipRight || this->zoomShear != 0;
    std::ptrdiff_t rowStart = static_cast<std::ptrdiff_t>(pitch) * (lineStep == 1 ? top : bottom);
    std::ptrdiff_t rowEnd = rowStart + pitch;
    std::ptrdiff_t lineStart = rowStart + (columnStep == 1 ? left : right);
    const std::ptrdiff_t clipTopAt = keepsClipTop ? this->RowOffset(this->clipY) : this->RowOffset(this->clipY + 1);
    const std::ptrdiff_t clipBottomAt =
        clipTopAt + (keepsClipTop ? this->RowOffset(this->clipHeight) : this->RowOffset(this->clipHeight - 1));
    const auto pixels = static_cast<std::ptrdiff_t>(this->screenPitch) * this->screenHeight;

    auto column = [this](std::size_t index)
    {
        return index < this->zoomColumns.size() ? this->zoomColumns[index] : kPastTable;
    };

    auto shift = [this](int index)
    {
        const auto at = static_cast<std::size_t>(index);
        return index >= 0 && at < this->zoomShears.size() ? this->zoomShears[at] : 0;
    };

    const u8* line = this->source + kRleLines;
    std::size_t lineIndex = 0;
    int sourceLine = this->zoomRows[0];

    while (Word(line) != 0)
    {
        const u8* runs = line + sizeof(u16);

        if (rowStart >= clipTopAt && rowEnd <= clipBottomAt)
        {
            std::ptrdiff_t at = lineStart;

            if (this->zoomShear != 0)
            {
                at += lineStep * (shift(scaledAnchorY) - shift(shearLine));
            }

            std::size_t index = 0;
            int position = 0;

            for (u16 code = Word(runs); code != 0; code = Word(runs))
            {
                const int end = position + (code & kRunCount);

                if ((code & kRunSkip) != 0)
                {
                    for (; column(index) < end; ++index)
                    {
                        at += columnStep;
                    }

                    runs += sizeof(u16);
                }
                else
                {
                    const u8* literal = runs + sizeof(u16);

                    for (; column(index) < end; ++index)
                    {
                        if ((!checksColumns || (at >= rowStart && at < rowEnd)) && at >= 0 && at < pixels)
                        {
                            pixel(this->target[at], literal, column(index) - position);
                        }

                        at += columnStep;
                    }

                    runs += sizeof(u16) * static_cast<std::size_t>(end - position + 1);
                }

                position = end;
            }
        }

        if (++lineIndex >= this->zoomRows.size() || this->zoomRows[lineIndex] < sourceLine)
        {
            break;
        }

        for (; sourceLine != this->zoomRows[lineIndex]; ++sourceLine)
        {
            line += Word(line) & kRunCount;
        }

        shearLine += flipped ? -1 : 1;
        lineStart += rowStep;
        rowStart += rowStep;
        rowEnd += rowStep;
    }

    return 0;
}

// swd3e.exe 0x00423830: the source pixels scaled (clipped from the clip top row on)
int Painter::BlitZoomCopy(int x, int y, int width, int height, u32 mode)
{
    return this->BlitZoomed(
        x,
        y,
        width,
        height,
        mode,
        true,
        [](u16& dest, const u8* literal, int column)
        {
            dest = Word(literal, column);
        }
    );
}

// swd3e.exe 0x00423D40: the source added to the destination per channel (saturated), then tinted by the tint options
// (saturated, or 0 for a negative tint)
int Painter::BlitZoomAddTinted(int x, int y, int width, int height, u32 mode)
{
    const Tint tint = this->CurrentTint();
    return this->BlitZoomed(
        x,
        y,
        width,
        height,
        mode,
        false,
        [this, &tint](u16& dest, const u8* literal, int column)
        {
            dest = this->TintPixel(tint, this->Add(Word(literal, column), dest));
        }
    );
}

// swd3e.exe 0x00424620: the destination under the picture tinted by the tint options
int Painter::BlitZoomTint(int x, int y, int width, int height, u32 mode)
{
    const Tint tint = this->CurrentTint();
    return this->BlitZoomed(
        x,
        y,
        width,
        height,
        mode,
        false,
        [this, &tint](u16& dest, const u8*, int)
        {
            dest = this->TintPixel(tint, dest);
        }
    );
}

// swd3e.exe 0x00424E40: blended at DrawOptions::alpha: 0 leaves the destination, 1..14 blend by sixteenths, any other
// level (15, negative or above) copies the source
int Painter::BlitZoomTranslucent(int x, int y, int width, int height, u32 mode)
{
    const int level = this->options.alpha;
    return this->BlitZoomed(
        x,
        y,
        width,
        height,
        mode,
        false,
        [this, level](u16& dest, const u8* literal, int column)
        {
            if (level == 0)
            {
                return;
            }

            const u16 source = Word(literal, column);
            dest = level > 0 && level < Painter::MaxAlpha ? this->BlendFine(dest, source, level) : source;
        }
    );
}

// swd3e.exe 0x00425B10: the picture's shape filled with ZoomFill
int Painter::BlitZoomSilhouette(int x, int y, int width, int height, u32 mode)
{
    return this->BlitZoomed(
        x,
        y,
        width,
        height,
        mode,
        false,
        [](u16& dest, const u8*, int)
        {
            dest = Painter::ZoomFill;
        }
    );
}

// swd3e.exe 0x00425FF0: the destination under the picture turned grey (the channel sum over four in every channel)
int Painter::BlitZoomGray(int x, int y, int width, int height, u32 mode)
{
    return this->BlitZoomed(
        x,
        y,
        width,
        height,
        mode,
        false,
        [this](u16& dest, const u8*, int)
        {
            dest = this->Gray(dest);
        }
    );
}

// swd3e.exe 0x004265B0: the tinted source (saturated) subtracted from the destination per channel, down to 0
int Painter::BlitZoomSubtractTinted(int x, int y, int width, int height, u32 mode)
{
    const Tint tint = this->CurrentTint();
    return this->BlitZoomed(
        x,
        y,
        width,
        height,
        mode,
        false,
        [this, &tint](u16& dest, const u8* literal, int column)
        {
            dest = this->SubtractTintedSource(tint, Word(literal, column), dest);
        }
    );
}

// swd3e.exe 0x00426C80: an unscaled 8-bit RLE picture through the palette (literal runs are bytes, the next code
// right after them). Skips advance the target by their count whatever the direction (a mirrored picture runs off its
// line); a cut picture checks its pixels against the clip left plus the picture's width less one, not the clip width.
int Painter::BlitPaletteCopy(int x, int y, int width, int height, u32 mode)
{
    const int pitch = this->screenPitch;
    const int right = x + width - 1;
    const int bottom = y + height - 1;
    std::ptrdiff_t rowStep = pitch;
    const int columnStep = (mode & Painter::Mirror) != 0 ? -1 : 1;
    const bool flipped = (mode & Painter::Flip) != 0;

    if (flipped)
    {
        rowStep = -pitch;
    }

    const int clipRight = this->clipX + this->clipWidth;
    const int clipBottom = this->clipY + this->clipHeight;

    if (x >= clipRight || right < this->clipX || y >= clipBottom || bottom < this->clipY || this->palette == nullptr)
    {
        return 0;
    }

    const bool checksColumns = x <= this->clipX || right >= clipRight;
    const int firstColumn = columnStep == 1 ? x : right;
    std::ptrdiff_t lineStart = firstColumn + static_cast<std::ptrdiff_t>(pitch) * (flipped ? bottom : y);
    std::ptrdiff_t windowStart = lineStart + (this->clipX - firstColumn);
    std::ptrdiff_t windowEnd = windowStart + (width - 1);
    const std::ptrdiff_t clipTopAt = this->RowOffset(this->clipY + 1);
    const std::ptrdiff_t clipBottomAt = clipTopAt + this->RowOffset(this->clipHeight - 1);
    const auto pixels = static_cast<std::ptrdiff_t>(this->screenPitch) * this->screenHeight;

    for (const u8* line = this->source + kRleLines; Word(line) != 0; line += Word(line) & kRunCount)
    {
        const u8* runs = line + sizeof(u16);

        if (windowStart >= clipTopAt && windowEnd <= clipBottomAt)
        {
            std::ptrdiff_t at = lineStart;

            while (Word(runs) != 0)
            {
                const int count = Word(runs) & kRunCount;

                if ((Word(runs) & kRunSkip) != 0)
                {
                    at += count;
                    runs += sizeof(u16);
                    continue;
                }

                const u8* indices = runs + sizeof(u16);

                for (int index = 0; index < count; ++index)
                {
                    if ((!checksColumns || (at >= windowStart && at < windowEnd)) && at >= 0 && at < pixels)
                    {
                        this->target[at] = this->palette[indices[index]];
                    }

                    at += columnStep;
                }

                runs = indices + count;
            }
        }

        lineStart += rowStep;
        windowStart += rowStep;
        windowEnd += rowStep;
    }

    return 0;
}

}  // namespace swd3e
