#include "battle/smooth/smooth_hud.h"

#include <array>

#include "audio/audio_manager.h"
#include "battle/battle_hud.h"
#include "battle/battle_hud_constants.h"
#include "battle/battle_services.h"
#include "config/config.h"
#include "core/words.h"
#include "crt/msvc.h"
#include "draw/painter.h"
#include "pic/picture.h"
#include "pic/picture_ops.h"
#include "platform/display.h"
#include "random/random.h"
#include "res/act_archive.h"
#include "res/tsw_archive.h"

namespace swd3e::smooth
{

namespace
{

constexpr int kArrowAct = 0x2392;
constexpr int kArrowTint = -4;  // the tint of all three channels of an arrow
constexpr int kBodyInset = 4;
constexpr int kBottomCapStyle = 2;
constexpr u16 kCoinAct = 0x239B;  // the coin toss (swd3e)
constexpr u32 kCoinSides = 2;
constexpr i32 kCoinSlowFrames = 0x19;  // the coin flips kCoinSlowdown times slower every kCoinSlowFrames frames
constexpr int kCoinSlowdown = 2;
constexpr u16 kCoinSound = 0x2E;
constexpr u16 kCoinStopDelay = 0x40;
constexpr int kCoinFullZoom = 0x400;
constexpr u16 kCoinStopSound = 0x117;
constexpr u16 kCoinFaceFrames = 0x1E;  // the face shows this long
constexpr u16 kCoinFaceSound = 0x197;
constexpr i32 kCoinEndAction = 3;  // grows out at the end
constexpr int kCoinEndGrowth = 5;
constexpr int kCoinZoomStep = 0x3C;
constexpr int kCoinX = 0x104;  // the centre of the coin
constexpr int kCoinY = 0xC8;
constexpr int kDecimal = 10;
constexpr u32 kDigitFont =
    0x2370;  // the digits of DrawDigit when its font is 0 (any other font redraws the last picture)
constexpr u16 kDigitGlow4000 = 0x4000;
constexpr u16 kDigitGlow8000 = 0x8000;
constexpr int kDimTint = -10;  // the green and blue tint of a dimmed portrait
constexpr int kGaugeAltFill = 2;
constexpr int kGaugeBack = 0;  // the frames of a gauge picture
constexpr int kGaugeBody = 2;
constexpr int kGaugeBottom = 1;
constexpr int kGaugeFill = 1;
constexpr int kGaugeLevels = 6;
constexpr int kGaugeShine = 3;
constexpr int kGaugeTop = 0;   // the frames of the tall gauge
constexpr int kLineWidth = 1;  // the timing marks and needle
constexpr int kMinThumb = 5;
constexpr int kNeedleEnd = 0x3D;
constexpr int kNeedleReach = 2;  // a mark is hit up to two pixels past it
constexpr int kNeedleShift = 1;  // the needle moves a pixel every second frame
constexpr int kNumberDigitWidth = 0x10;
constexpr u32 kOutlineGreen = 0x07E007E0;
constexpr int kPortraitAct = 0x2390;
constexpr int kRoleIconAct = 0x23AA;  // the icons of the roles by action (DrawRoleIcon)
constexpr int kScrollBottom = 0x1B;
constexpr int kScrollClipWidth = 0x20;
constexpr int kScrollPiece = 0x18;  // the actions of the scroll bar act
constexpr int kScrollStyle1Top = 0x1E;
constexpr int kScrollStyle2Bottom = 0x1F;
constexpr int kScrollThumb = 0x19;
constexpr int kScrollTop = 0x1A;
constexpr int kShineAlpha = 8;
constexpr int kShineX = 0xE;  // swd3e (swd32: 0xB)
constexpr int kShineY = 0x1F;
constexpr int kSmallDigits = 10;
constexpr int kSmallTopDivisor = 1000000000;
constexpr u32 kTallGauge = 0x237A;  // swd3e (swd32: 0x241A)
constexpr int kShortGauge = 4;      // swd3e: a gauge of four levels draws its body 0x20 shorter
constexpr int kShortGaugeCut = 0x20;
constexpr int kShortShineCut = 0x1E;
constexpr int kGaugeOverlap = 2;  // the body starts 2 above the top's bottom
constexpr int kBottomOverlap = 3;
constexpr int kThumbInset = 5;
constexpr int kTileBottom = 7;
constexpr int kTileBottomLeft = 6;
constexpr int kTileBottomRight = 8;
constexpr int kTileFill = 4;  // the frames of a tile box (DrawTileBox)
constexpr int kTileLeft = 3;
constexpr int kTileRight = 5;
constexpr int kTileSize = 0x10;
constexpr int kTileTop = 1;
constexpr int kTileTopLeft = 0;
constexpr int kTileTopRight = 2;
constexpr u16 kTimingDone = 0x8000;
constexpr u32 kTimingPicture = 0x2380;
constexpr int kTopCapStyle = 1;           // DrawScrollBar styles
constexpr u32 kTopDivisor = 1000000000u;  // DrawNumber: ten digits
constexpr int kTrackInset = 2;
constexpr int kVisibleRows = 7;
constexpr int kWideFillExtra = 2;
constexpr u16 kWinningSide = 1;
}  // namespace

HudFrame::HudFrame(BattleHud& hud)
    : hud(hud)
{
}

void HudFrame::FillColor(int x, int y, int width, int height, u32 color, Scene& scene)
{
    scene.Add(
        [x, y, width, height, color](BattleServices& drawing)
        {
            drawing.hud.FillColor(x, y, width, height, color);
        }
    );
}

// What BattleHud::Draw decides of the frame; the draws into `scene`.
void HudFrame::DrawAdvance(const Picture* picture, int x, int y, int width, int height, u32 mode, Scene& scene)
{
    BattleHud& hud = this->hud;
    BattleServices& services = *hud.services;
    ScenePainter painter(services, scene);

    if (picture != nullptr)
    {
        painter.DrawPicture(x, y, width, height, mode);
    }
}

// What BattleHud::DrawPictureFrame0 decides of the frame; the draws into `scene`.
int HudFrame::DrawPictureFrame0Advance(int id, int x, int y, Scene& scene)
{
    BattleHud& hud = this->hud;
    BattleServices& services = *hud.services;
    ScenePainter painter(services, scene);
    const Picture* picture = hud.Select(id, 0);
    return picture != nullptr ? painter.DrawPicture(x, y, picture->width, picture->height, 0) : 0;
}

// What BattleHud::DrawPortrait decides of the frame; the draws into `scene`.
int HudFrame::DrawPortraitAdvance(u16 index, int x, int y, int dim, Scene& scene)
{
    BattleHud& hud = this->hud;
    BattleServices& services = *hud.services;
    ScenePainter painter(services, scene);
    ActState state;
    state.id = kPortraitAct;
    state.action = index;

    if (hud.acts.Run(state) == 0)
    {
        return 0;
    }

    const Picture* picture = hud.Select(state.frame, state.picture);

    if (picture == nullptr)
    {
        return 0;
    }

    // A ghost's portrait (index 4..8) reads hitFading past the four bytes in the original (deviations.md).
    if (index < hud.portraitHighlight.size() && hud.portraitHighlight[index] == 1)
    {
        DrawArgument outline;
        outline.colorPair = kOutlineGreen;
        painter.DrawOutline(x - state.x, y - state.y, picture->width, picture->height, 0, outline);
        picture = hud.lastPicture;
    }

    painter.DrawPicture(x - state.x, y - state.y, picture->width, picture->height, 0);

    if (dim != 1)
    {
        return dim;
    }

    DrawOptions& options = painter.Options();
    options.tintGreen = kDimTint;
    options.tintBlue = kDimTint;
    options.tintRed = 0;
    const Picture* current = hud.lastPicture;
    return painter.DrawPicture(
        x - state.x,
        y - state.y,
        current->width,
        current->height,
        state.flags | Painter::TintUnder,
        PaletteArgument(*current)
    );
}

// What BattleHud::DrawArrow decides of the frame; the draws into `scene`.
int HudFrame::DrawArrowAdvance(int x, int y, int slot, Scene& scene)
{
    BattleHud& hud = this->hud;
    BattleServices& services = *hud.services;
    ScenePainter painter(services, scene);
    ActState& state = hud.arrowActs[static_cast<std::size_t>(slot)];
    state.id = kArrowAct;
    state.action = 0;

    if (hud.acts.Run(state) == 0)
    {
        return 0;
    }

    const Picture* picture = hud.Select(state.frame, state.picture);

    if (picture == nullptr)
    {
        return 0;
    }

    DrawOptions& options = painter.Options();
    options.tintGreen = kArrowTint;
    options.tintBlue = kArrowTint;
    options.tintRed = kArrowTint;
    return painter
        .DrawPicture(x - state.x, y - state.y, picture->width, picture->height, state.flags, PaletteArgument(*picture));
}

// What BattleHud::DrawBar decides of the frame; the draws into `scene`.
int HudFrame::DrawBarAdvance(int id, int frame, int x, int y, int width, Scene& scene)
{
    BattleHud& hud = this->hud;
    BattleServices& services = *hud.services;
    ScenePainter painter(services, scene);
    const Picture* picture = hud.Select(id, frame);

    if (picture != nullptr && width > 0)
    {
        return painter.DrawPicture(x, y, width, picture->height, 0);
    }

    return picture != nullptr ? 1 : 0;
}

// What BattleHud::DrawPictureFrame decides of the frame; the draws into `scene`.
int HudFrame::DrawPictureFrameAdvance(int id, int frame, int x, int y, Scene& scene)
{
    BattleHud& hud = this->hud;
    BattleServices& services = *hud.services;
    ScenePainter painter(services, scene);
    const Picture* picture = hud.Select(id, frame);
    return picture != nullptr ? painter.DrawPicture(x, y, picture->width, picture->height, 0) : 0;
}

// What BattleHud::DrawGauge decides of the frame; the draws into `scene`.
int HudFrame::DrawGaugeAdvance(int id, int x, int y, int fill, Scene& scene)
{
    BattleHud& hud = this->hud;
    BattleServices& services = *hud.services;
    ScenePainter painter(services, scene);
    const Picture* back = hud.Select(id, kGaugeBack);

    if (back != nullptr)
    {
        painter.DrawPicture(x, y, back->width, back->height, 0);
    }

    const Picture* front = hud.Select(id, kGaugeFill);

    if (front != nullptr && fill != 0)
    {
        return painter.DrawPicture(x, y, fill, front->height, 0);
    }

    return front != nullptr ? 1 : 0;
}

// What BattleHud::DrawGaugeAlt decides of the frame; the draws into `scene`.
int HudFrame::DrawGaugeAltAdvance(int id, int x, int y, int fill, Scene& scene)
{
    BattleHud& hud = this->hud;
    BattleServices& services = *hud.services;
    ScenePainter painter(services, scene);
    const Picture* back = hud.Select(id, kGaugeBack);

    if (back != nullptr)
    {
        painter.DrawPicture(x, y, back->width, back->height, 0);
    }

    const Picture* front = hud.Select(id, kGaugeAltFill);

    if (front != nullptr && fill != 0)
    {
        painter.DrawPicture(x, y, fill, front->height, 0);
    }

    return 1;
}

// What BattleHud::DrawGaugeWide decides of the frame; the draws into `scene`.
int HudFrame::DrawGaugeWideAdvance(int id, int x, int y, u16 fill, Scene& scene)
{
    BattleHud& hud = this->hud;
    BattleServices& services = *hud.services;
    ScenePainter painter(services, scene);
    const Picture* back = hud.Select(id, kGaugeBack);

    if (back != nullptr)
    {
        painter.DrawPicture(x, y, back->width, back->height, 0);
    }

    const Picture* front = hud.Select(id, kGaugeFill);
    return front != nullptr ? painter.DrawPicture(x, y, fill + kWideFillExtra, front->height, 0) : 0;
}

// What BattleHud::DrawSmallNumber decides of the frame; the draws into `scene`.
int HudFrame::DrawSmallNumberAdvance(int font, int value, int x, int y, Scene& scene)
{
    BattleHud& hud = this->hud;
    BattleServices& services = *hud.services;
    ScenePainter painter(services, scene);
    std::array<int, kSmallDigits> digits = {};
    int count = 0;
    int divisor = kSmallTopDivisor;
    hud.numberX = x;
    hud.numberY = y;
    hud.numberStarted = 0;

    for (int& digit : digits)
    {
        hud.numberValue = value;
        digit = value / divisor;
        value %= divisor;

        if (digit != 0 || hud.numberStarted == 1)
        {
            hud.numberStarted = 1;
            ++count;
        }

        divisor /= kDecimal;
    }

    if (count == 0)
    {
        count = 1;
    }

    int result = 0;

    for (int i = 0; i < count; ++i)
    {
        const Picture* picture = hud.Select(font, digits[static_cast<std::size_t>(kSmallDigits - 1 - i)]);

        if (picture == nullptr)
        {
            continue;
        }

        result = painter.DrawPicture(x, y, picture->width, picture->height, 0);
        x -= hud.lastPicture->width;
    }

    return result;
}

// What BattleHud::DrawNumberDigit decides of the frame; the draws into `scene`.
u16 HudFrame::DrawNumberDigitAdvance(u32 divisor, Scene& scene)
{
    BattleHud& hud = this->hud;
    const u32 digit = static_cast<u32>(hud.numberValue) / divisor;

    if (static_cast<u16>(digit) == 0 && hud.numberStarted == 0)
    {
        return 0;
    }

    const Picture* picture = hud.Select(HighWord(hud.numberFont), static_cast<int>(digit));
    this->DrawAdvance(
        picture,
        hud.numberX - kNumberDigitWidth,
        hud.numberY,
        picture != nullptr ? picture->width : 0,
        picture != nullptr ? picture->height : 0,
        hud.digitMode == kDigitGlow8000 ? Painter::ScaledGlow : 0,
        scene
    );
    hud.numberStarted = 1;
    hud.numberValue = static_cast<i32>(static_cast<u32>(hud.numberValue) - divisor * static_cast<u16>(digit));
    return hud.lastPicture != nullptr ? hud.lastPicture->width : 0;
}

// What BattleHud::DrawNumber decides of the frame; the draws into `scene`.
int HudFrame::DrawNumberAdvance(i16 font, int value, int x, int y, Scene& scene)
{
    BattleHud& hud = this->hud;
    hud.numberFont = MakeWords(static_cast<u16>(font), LowWord(hud.numberFont));
    hud.numberValue = value;
    hud.numberX = x;
    hud.numberY = y;
    hud.numberStarted = 0;
    hud.numberX = this->DrawNumberDigitAdvance(kTopDivisor, scene) + x;

    for (u32 divisor = kTopDivisor / kDecimal; divisor >= kDecimal; divisor /= kDecimal)
    {
        hud.numberX += this->DrawNumberDigitAdvance(divisor, scene);
    }

    hud.numberStarted = 1;
    const int width = this->DrawNumberDigitAdvance(1u, scene);
    hud.numberX += width;
    return width;
}

// What BattleHud::DrawSlotAnimation decides of the frame; the draws into `scene`.
int HudFrame::DrawSlotAnimationAdvance(int id, int slot, int x, int y, Scene& scene)
{
    BattleHud& hud = this->hud;
    BattleServices& services = *hud.services;
    ScenePainter painter(services, scene);
    ActState& state = hud.slotActs[static_cast<std::size_t>(slot)];
    state.id = id;
    state.action = 0;

    if (hud.acts.Run(state) == 0)
    {
        return 0;
    }

    const Picture* picture = hud.archive.GetPicture(state.frame, state.picture);

    if (picture == nullptr)
    {
        return 0;
    }

    painter.SetSource(*picture);
    return painter.DrawPicture(x, y, picture->width, picture->height, state.flags);
}

// What BattleHud::DrawEffectAt decides of the frame; the draws into `scene`.
int HudFrame::DrawEffectAtAdvance(int id, int action, int x, int y, int mirror, Scene& scene)
{
    BattleHud& hud = this->hud;
    BattleServices& services = *hud.services;
    ScenePainter painter(services, scene);
    ActState& state = hud.effectAct;
    state.id = id;
    state.action = action;

    if (hud.acts.Run(state) == 0)
    {
        return 0;
    }

    const Picture* picture = hud.archive.GetPicture(state.frame, state.picture);

    if (picture == nullptr)
    {
        return 0;
    }

    u32 flags = state.flags;
    i32 offset = state.x;

    if (mirror == 1)
    {
        flags = (state.flags & 1) != 0 ? state.flags & ~1u : state.flags | 1;
        // 0x00450AE2 mov dx / sub dx: only the low word becomes width - x, the high word of x stays
        offset = static_cast<i32>(
            (static_cast<u32>(state.x) & 0xFFFF0000u) | static_cast<u16>(picture->width - static_cast<u16>(state.x))
        );
    }

    painter.SetSource(*picture);
    painter.DrawPicture(x - offset, y - state.y, picture->width, picture->height, flags);
    return hud.effectAct.waiting == 1 ? 1 : 0;
}

// What BattleHud::DrawCursorAnimation decides of the frame; the draws into `scene`.
int HudFrame::DrawCursorAnimationAdvance(int id, int x, int y, Scene& scene)
{
    BattleHud& hud = this->hud;
    BattleServices& services = *hud.services;
    ScenePainter painter(services, scene);
    ActState& state = hud.cursorAct;
    state.id = id;
    state.action = 0;

    if (hud.acts.Run(state) == 0)
    {
        return 0;
    }

    const Picture* picture = hud.archive.GetPicture(state.frame, state.picture);

    if (picture == nullptr)
    {
        return 0;
    }

    painter.SetSource(*picture);
    return painter.DrawPicture(x, y, picture->width, picture->height, hud.cursorAct.flags);
}

int HudFrame::DrawRoleIconAdvance(int action, int x, int y, Scene& scene)
{
    BattleHud& hud = this->hud;
    BattleServices& services = *hud.services;
    ScenePainter painter(services, scene);
    ActState& state = hud.roleIconAct;
    state.id = kRoleIconAct;
    state.action = action;

    if (hud.acts.Run(state) == 0)
    {
        return 0;
    }

    const Picture* picture = hud.archive.GetPicture(state.frame, state.picture);

    if (picture == nullptr)
    {
        return 0;
    }

    painter.SetSource(*picture);
    return painter.DrawPicture(x - state.x, y - state.y, picture->width, picture->height, state.flags);
}

// What BattleHud::DrawDigit decides of the frame; the draws into `scene`.
u16 HudFrame::DrawDigitAdvance(int font, int digit, int x, int y, Scene& scene)
{
    BattleHud& hud = this->hud;
    BattleServices& services = *hud.services;
    ScenePainter painter(services, scene);
    const Picture* picture = hud.lastPicture;

    if (font == 0)
    {
        picture = hud.archive.GetPicture(kDigitFont, static_cast<u32>(digit));
        hud.lastPicture = picture;

        if (picture != nullptr)
        {
            painter.SetSource(*picture);
        }
    }

    if (picture == nullptr)
    {
        return 0;
    }

    const u32 glow = hud.digitMode == kDigitGlow4000 ? Painter::ScaledGlow : 0;
    painter.DrawPicture(x, y, picture->width, picture->height, glow);
    return hud.lastPicture->width;
}

// What BattleHud::DrawScrollBar decides of the frame; the draws into `scene`.
int HudFrame::DrawScrollBarAdvance(u16 id, int x, int y, int rows, int offset, int style, Scene& scene)
{
    BattleHud& hud = this->hud;
    BattleServices& services = *hud.services;
    ScenePainter painter(services, scene);
    ActState& state = hud.scrollAct;
    const Picture* cap = hud.RunScroll(id, style == kTopCapStyle ? kScrollStyle1Top : kScrollTop);

    if (cap == nullptr)
    {
        return 0;
    }

    painter.DrawPicture(x, y, cap->width, cap->height, state.flags);
    const int capHeight = cap->height;
    const int trackTop = capHeight + y;
    state = ActState{};
    const Picture* piece = hud.RunScroll(id, kScrollPiece);

    if (piece == nullptr)
    {
        return 0;
    }

    int row = 0;

    for (; row < rows; ++row)
    {
        const int rowY = y + capHeight + row * piece->height;
        painter.DrawPicture(x + kTrackInset, rowY, piece->width, piece->height, state.flags);
    }

    const u16 pieceHeight = piece->height;
    const int bottomOffset = row * pieceHeight + capHeight;
    const int track = rows * pieceHeight;
    state = ActState{};
    const Picture* thumb = hud.RunScroll(id, kScrollThumb);

    if (thumb == nullptr)
    {
        return 0;
    }

    const auto thumbLength = static_cast<i32>(
        crt::Ftol(static_cast<double>(kVisibleRows) / static_cast<double>(hud.scrollCount) * static_cast<double>(track))
    );
    hud.scrollStep = (track - thumbLength) / (hud.scrollCount - kVisibleRows);
    const int thumbTop = offset + hud.scrollStep * hud.scrollFirst + trackTop;
    int thumbBottom = offset + trackTop + hud.scrollStep * hud.scrollFirst + thumbLength;
    hud.scrollThumbY = thumbTop;

    if (thumbBottom - thumbTop < kMinThumb)
    {
        thumbBottom = thumbTop + kMinThumb;
    }

    if (hud.scrollFirst + kVisibleRows >= hud.scrollCount)
    {
        thumbBottom = trackTop + track;
    }

    hud.scrollTrackTop = trackTop;
    hud.scrollThumbTop = thumbTop;
    hud.scrollThumbBottom = thumbBottom;
    hud.scrollTrackBottom = trackTop + track;
    painter.SetClipClamped(x, y, x + kScrollClipWidth, thumbBottom);

    if (thumbLength > 0)
    {
        int drawn = 0;

        do
        {
            painter.DrawPicture(x + kThumbInset, drawn + hud.scrollThumbY, thumb->width, thumb->height, state.flags);
            drawn += thumb->height;
        } while (drawn < thumbLength && thumb->height != 0);
    }

    painter.SetClipClamped(0, 0, Display::Width, Display::Height);
    state = ActState{};
    const Picture* bottom = hud.RunScroll(id, style == kBottomCapStyle ? kScrollStyle2Bottom : kScrollBottom);

    if (bottom == nullptr)
    {
        return 0;
    }

    return painter.DrawPicture(x, y + bottomOffset, bottom->width, bottom->height, state.flags);
}

// What BattleHud::DrawCoinToss decides of the frame; the draws into `scene`.
int HudFrame::DrawCoinTossAdvance(Scene& scene)
{
    BattleHud& hud = this->hud;
    BattleServices& services = *hud.services;
    ScenePainter painter(services, scene);

    if (hud.coinFrames == 0)
    {
        if (HighWord(hud.coinState) != 0)
        {
            hud.coinState = 0;
        }
        else
        {
            hud.coinState = MakeWords(HighWord(hud.coinState), static_cast<u16>(hud.random.Below(kCoinSides)));
        }
    }

    ActState& spin = hud.scrollAct;
    spin.id = kCoinAct;
    spin.action = 0;

    if (hud.acts.Run(spin) == 0)
    {
        return 0;
    }

    const Picture* picture = hud.archive.GetPicture(spin.frame, spin.picture);

    // A missing picture is read through a null pointer in the original.
    if (picture != nullptr)
    {
        if (hud.coinZoom < kCoinFullZoom)
        {
            painter.SetZoom(
                picture->width >> 1,
                picture->height >> 1,
                kCoinFullZoom - hud.coinZoom,
                kCoinFullZoom - hud.coinZoom
            );
        }

        painter.SetSource(*picture);
        painter.DrawPicture(
            kCoinX - spin.x,
            kCoinY - spin.y,
            picture->width,
            picture->height,
            spin.flags | Painter::ZoomCopy
        );
    }

    if (spin.hold == 0)
    {
        u16 delay = 0;

        if (++hud.coinFrames % kCoinSlowFrames != 0)
        {
            delay = LowWord(hud.coinDelay);
        }
        else
        {
            delay = static_cast<u16>(kCoinSlowdown * hud.coinDelay);
            ++spin.frameDelay;
            hud.coinDelay = MakeWords(delay, delay);
            spin.delay = spin.frameDelay;
        }

        if (delay >= kCoinStopDelay)
        {
            hud.coinDelay = BattleHud::InitialCoinDelay;
            hud.coinZoom = 0;

            if (LowWord(hud.coinState) == kWinningSide)
            {
                hud.coinState = MakeWords(static_cast<u16>(HighWord(hud.coinState) + 1), LowWord(hud.coinState));
            }

            spin.hold = 1;
            hud.audio.PlaySound(kCoinStopSound, services.config.soundVolume);
            delay = LowWord(hud.coinDelay);
        }

        auto countdown = static_cast<u16>(HighWord(hud.coinDelay) - 1);

        if (countdown == 0)
        {
            countdown = delay;
        }

        hud.coinDelay = MakeWords(countdown, LowWord(hud.coinDelay));

        if (spin.delay == 0)
        {
            hud.audio.PlaySound(kCoinSound, services.config.soundVolume);
        }

        return 0;
    }

    ActState& face = hud.coinFaceAct;
    face.id = kCoinAct;

    if (hud.coinFrames != 1)
    {
        face.action = static_cast<u32>(LowWord(hud.coinState)) + 1;

        if (hud.acts.Run(face) == 0)
        {
            return 0;
        }

        const Picture* shown = hud.archive.GetPicture(face.frame, face.picture);

        // A missing picture is read through a null pointer in the original.
        if (shown != nullptr)
        {
            painter.SetSource(*shown);
            painter.DrawPicture(kCoinX - face.x, kCoinY - face.y, shown->width, shown->height, face.flags);
        }

        const auto shownFrames = static_cast<u16>(HighWord(hud.coinDelay) + 1);
        hud.coinDelay = MakeWords(shownFrames, LowWord(hud.coinDelay));

        if (shownFrames > kCoinFaceFrames)
        {
            hud.audio.PlaySound(kCoinFaceSound, services.config.soundVolume);
            face = ActState{};
            hud.coinFrames = 1;
            hud.coinDelay = MakeWords(1, LowWord(hud.coinDelay));
        }

        return 0;
    }

    face.action = kCoinEndAction;

    if (hud.acts.Run(face) == 0)
    {
        return 0;
    }

    const Picture* end = hud.archive.GetPicture(face.frame, face.picture);

    // A missing picture is read through a null pointer in the original.
    if (end != nullptr)
    {
        const int zoom = kCoinEndGrowth * hud.coinZoom + kCoinFullZoom;
        painter.SetZoom(end->width >> 1, end->height >> 1, zoom, zoom);
        DrawOptions& options = painter.Options();
        options.tintRed = 0;
        options.tintGreen = 0;
        options.tintBlue = 0;
        painter.SetSource(*end);
        painter.DrawPicture(
            kCoinX - face.x,
            kCoinY - face.y,
            end->width,
            end->height,
            Painter::ZoomCopy | Painter::Additive
        );
    }

    hud.coinZoom += kCoinZoomStep;

    if (hud.coinZoom < kCoinFullZoom)
    {
        return 0;
    }

    hud.coinFrames = 0;
    hud.coinZoom = 0;
    hud.coinDelay = MakeWords(1, LowWord(hud.coinDelay));
    spin = ActState{};
    face = ActState{};
    return 1;
}

// What BattleHud::DrawTimingBar decides of the frame; the draws into `scene`.
int HudFrame::DrawTimingBarAdvance(int x, int y, Scene& scene)
{
    BattleHud& hud = this->hud;
    BattleServices& services = *hud.services;
    ScenePainter painter(services, scene);
    const Picture* back = hud.Select(static_cast<int>(kTimingPicture), 0);

    if (back == nullptr)
    {
        return 0;
    }

    painter.DrawPicture(x, y, back->width, back->height, 0);
    hud.timingHit = 0;

    // A frame that does not load leaves no last picture: the original reads its height through a null pointer.
    auto lastHeight = [&hud]
    {
        return hud.lastPicture != nullptr ? static_cast<int>(hud.lastPicture->height) : 0;
    };

    for (int mark = 0; mark < static_cast<int>(BattleHud::TimingMarks.size()); ++mark)
    {
        const u16 position = BattleHud::TimingMarks[static_cast<std::size_t>(mark)];
        painter.SetClipClamped(x + position, y, x + position + kLineWidth, y + lastHeight());
        const Picture* line = hud.Select(static_cast<int>(kTimingPicture), 1);
        this->DrawAdvance(line, x, y, line != nullptr ? line->width : 0, line != nullptr ? line->height : 0, 0, scene);
        const int needle = (LowWord(hud.timingCounter) >> kNeedleShift) + 1;

        if (needle >= position && needle <= position + kNeedleReach)
        {
            hud.timingHit = static_cast<u16>(mark + 1);

            if (LowWord(hud.numberFont) == mark + 1)
            {
                hud.timingHit = 0;
            }
            else
            {
                hud.numberFont = MakeWords(HighWord(hud.numberFont), 0);
            }
        }
    }

    const int needle = LowWord(hud.timingCounter) >> kNeedleShift;
    painter.SetClipClamped(needle + x + 1, y, needle + x + 1 + kLineWidth, y + lastHeight());
    const Picture* line = hud.Select(static_cast<int>(kTimingPicture), 1);
    this->DrawAdvance(line, x, y, line != nullptr ? line->width : 0, line != nullptr ? line->height : 0, 0, scene);
    painter.SetClipClamped(0, 0, Display::Width, Display::Height);
    hud.timingCounter =
        static_cast<i32>(MakeWords(HighWord(hud.timingCounter), static_cast<u16>(hud.timingCounter + 1)));

    if ((LowWord(hud.timingCounter) >> kNeedleShift) != kNeedleEnd)
    {
        return 0;
    }

    hud.timingCounter = static_cast<i32>(MakeWords(HighWord(hud.timingCounter), kTimingDone));
    return 1;
}

// What BattleHud::DrawTallGauge decides of the frame; the draws into `scene`.
int HudFrame::DrawTallGaugeAdvance(int x, int y, int level, int levels, Scene& scene)
{
    BattleHud& hud = this->hud;
    BattleServices& services = *hud.services;
    ScenePainter painter(services, scene);
    const Picture* body = hud.archive.GetPicture(kTallGauge, kGaugeBody);
    hud.lastPicture = body;

    // A missing picture is read through a null pointer in the original.
    if (body == nullptr)
    {
        return 0;
    }

    const int step =
        levels == kShortGauge ? (body->height - kShortGaugeCut) / kGaugeLevels : body->height / kGaugeLevels;
    const int shown = level * step;
    const Picture* top = hud.Select(static_cast<int>(kTallGauge), kGaugeTop);

    if (top == nullptr)
    {
        return 0;
    }

    painter.DrawPicture(x, y, top->width, top->height, 0);
    const int bodyTop = y + top->height - kGaugeOverlap;

    if (level < kGaugeLevels)
    {
        painter.SetClipClamped(x, bodyTop, x + top->width, bodyTop + shown);
    }

    body = hud.Select(static_cast<int>(kTallGauge), kGaugeBody);

    if (body != nullptr)
    {
        const int height = levels == kGaugeLevels ? body->height : body->height - kShortGaugeCut;
        painter.DrawPicture(x + kBodyInset, bodyTop, body->width, height, 0);
    }

    const Picture* shine = hud.Select(static_cast<int>(kTallGauge), kGaugeShine);
    painter.Options().alpha = kShineAlpha;

    if (shine != nullptr)
    {
        const int height = levels == kGaugeLevels ? shine->height : shine->height - kShortShineCut;
        painter.DrawPicture(x + kShineX, bodyTop + kShineY, shine->width, height, Painter::Translucent);
    }

    painter.SetClipClamped(0, 0, Display::Width, Display::Height);
    const Picture* bottom = hud.Select(static_cast<int>(kTallGauge), kGaugeBottom);
    return bottom != nullptr
               ? painter.DrawPicture(x, bodyTop + shown - kBottomOverlap, bottom->width, bottom->height, 0)
               : 0;
}

// What BattleHud::DrawAnimation decides of the frame; the draws into `scene`.
int HudFrame::DrawAnimationAdvance(BattleAnimation& animation, Scene& scene)
{
    BattleHud& hud = this->hud;
    BattleServices& services = *hud.services;
    ScenePainter painter(services, scene);

    if (animation.id == 0)
    {
        return 0;
    }

    ActState& state = animation.act;
    state.id = animation.id;
    state.action = 0;
    hud.acts.Run(state);
    const Picture* picture =
        state.picture < BattleAnimation::Pictures ? animation.pictures[state.picture].get() : nullptr;

    if (picture != nullptr)
    {
        painter.SetSource(*picture);
        painter.SetWave(animation.wave);
        painter.DrawPicture(animation.x - state.x, animation.y - state.y, picture->width, picture->height, state.flags);
        painter.SetWave(0);
    }

    return state.waiting;
}

// What BattleHud::PlayAnimationShifted decides of the frame; the draws into `scene`.
int HudFrame::PlayAnimationShiftedAdvance(BattleAnimation& animation, int, int amount, Scene& scene)
{
    BattleHud& hud = this->hud;
    BattleServices& services = *hud.services;
    ScenePainter painter(services, scene);
    std::array<u16, BattleAnimation::Pictures> shown = {};
    shown.fill(kNotLoaded);

    if (animation.id == 0)
    {
        return 0;
    }

    ActState& state = animation.act;
    state = ActState{};
    state.action = 0;
    state.id = animation.id;

    if (hud.acts.Run(state) == 0)
    {
        return 0;
    }

    for (;;)
    {
        const u16 frame = state.picture;

        if (frame < BattleAnimation::Pictures && shown[frame] == kNotLoaded)
        {
            shown[frame] = frame;
            Picture* picture = animation.pictures[frame].get();

            if (picture != nullptr)
            {
                if (amount > 0)
                {
                    ShiftPicture(picture->data, ShiftMode::Right, amount);
                }
                else if (amount < 0)
                {
                    ShiftPicture(picture->data, ShiftMode::Left, -amount);
                }

                painter.SetSource(*picture);
                painter.DrawPicture(
                    animation.x - state.x,
                    animation.y - state.y,
                    picture->width,
                    picture->height,
                    state.flags
                );
            }
        }

        const bool ended = state.position == 0;
        state.delay = 0;
        state.frameDelay = 0;

        if (ended)
        {
            break;
        }

        state.action = 0;
        state.id = animation.id;

        if (hud.acts.Run(state) == 0)
        {
            return 0;
        }
    }

    state = ActState{};
    return 1;
}

// What BattleHud::DrawTileBox decides of the frame; the draws into `scene`.
void HudFrame::DrawTileBoxAdvance(int id, int x, int y, int columns, int rows, i8 fill, Scene& scene)
{
    BattleHud& hud = this->hud;
    BattleServices& services = *hud.services;
    ScenePainter painter(services, scene);

    auto width = [](const Picture* picture)
    {
        return picture != nullptr ? static_cast<int>(picture->width) : 0;
    };

    auto height = [](const Picture* picture)
    {
        return picture != nullptr ? static_cast<int>(picture->height) : 0;
    };

    auto draw = [&painter](const Picture* picture, int left, int top)
    {
        if (picture != nullptr)
        {
            painter.DrawPicture(left, top, picture->width, picture->height, 0, PaletteArgument(*picture));
        }
    };

    hud.Select(id, kTileFill);
    this->FillColor(
        x,
        y,
        kTileSize * (columns + 2),
        kTileSize * (rows + 2),
        static_cast<u32>(static_cast<i32>(fill)),
        scene
    );
    const Picture* corner = hud.Select(id, kTileTopLeft);
    draw(corner, x, y);
    int left = x + width(corner);
    const Picture* edge = hud.Select(id, kTileTop);

    for (int column = 0; column < columns; ++column)
    {
        draw(edge, left, y);
        left += width(edge);
    }

    const Picture* topRight = hud.Select(id, kTileTopRight);
    draw(topRight, left, y);
    int top = y + height(topRight);

    for (int row = 0; row < rows; ++row)
    {
        const Picture* side = hud.Select(id, kTileLeft);
        draw(side, x, top);
        const int rightX = x + (columns + 1) * width(side);
        const Picture* right = hud.Select(id, kTileRight);
        draw(right, rightX, top);
        top += height(right);
    }

    const Picture* bottomLeft = hud.Select(id, kTileBottomLeft);
    draw(bottomLeft, x, top);
    left = x + width(bottomLeft);
    const Picture* bottom = hud.Select(id, kTileBottom);

    for (int column = 0; column < columns; ++column)
    {
        draw(bottom, left, top);
        left += width(bottom);
    }

    draw(hud.Select(id, kTileBottomRight), left, top);
}

}  // namespace swd3e::smooth
