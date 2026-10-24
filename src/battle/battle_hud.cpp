#include "battle/battle_hud.h"

#include <algorithm>

#include "audio/audio_manager.h"
#include "battle/battle_hud_constants.h"
#include "battle/battle_message.h"
#include "battle/battle_services.h"
#include "battle/battle_state.h"
#include "battle/smooth/smooth_hud.h"
#include "config/config.h"
#include "core/format.h"
#include "core/words.h"
#include "crt/msvc.h"
#include "draw/painter.h"
#include "pic/picture.h"
#include "pic/picture_ops.h"
#include "platform/display.h"
#include "random/random.h"
#include "res/act_archive.h"
#include "res/tsw_archive.h"

namespace swd3e
{

namespace
{
constexpr u32 kColorWord = 0xFFFF;  // FillColor: the pixel the colour argument starts with
constexpr int kRouletteFlags = BattleMessage::BlackBar | BattleMessage::Centered;

}  // namespace

BattleHud::BattleHud(Painter& painter, TswArchive& archive, ActArchive& acts, Random& random, AudioManager& audio)
    : painter(painter),
      archive(archive),
      acts(acts),
      random(random),
      audio(audio)
{
}

void BattleHud::Attach(BattleServices& battle)
{
    this->services = &battle;
}

// Picture `frame` of `id` as the source and the last picture.
const Picture* BattleHud::Select(int id, int frame)
{
    const Picture* picture = this->archive.GetPicture(static_cast<u32>(id), static_cast<u32>(frame));
    this->lastPicture = picture;

    if (picture != nullptr)
    {
        this->painter.SetSource(picture->data.data());
    }

    return picture;
}

void BattleHud::Draw(const Picture* picture, int x, int y, int width, int height, u32 mode)
{
    smooth::Scene scene;
    smooth::HudFrame(*this).DrawAdvance(picture, x, y, width, height, mode, scene);
    scene.Show(*this->services);
}

// swd3e.exe 0x004578C0
int BattleHud::DrawPictureFrame0(int id, int x, int y)
{
    smooth::Scene scene;
    const auto result = smooth::HudFrame(*this).DrawPictureFrame0Advance(id, x, y, scene);
    scene.Show(*this->services);
    return result;
}

// swd3e.exe 0x00457900: portrait action `index` of act 0x2390, outlined in green when highlighted, darkened when `dim`
// is 1
int BattleHud::DrawPortrait(u16 index, int x, int y, int dim)
{
    smooth::Scene scene;
    const auto result = smooth::HudFrame(*this).DrawPortraitAdvance(index, x, y, dim, scene);
    scene.Show(*this->services);
    return result;
}

// swd3e.exe 0x00457A50: the looping arrow (act 0x2392) of one of four slots, tinted by -4 in each channel
int BattleHud::DrawArrow(int x, int y, int slot)
{
    smooth::Scene scene;
    const auto result = smooth::HudFrame(*this).DrawArrowAdvance(x, y, slot, scene);
    scene.Show(*this->services);
    return result;
}

// swd3e.exe 0x00457B00: only positive widths are drawn
int BattleHud::DrawBar(int id, int frame, int x, int y, int width)
{
    smooth::Scene scene;
    const auto result = smooth::HudFrame(*this).DrawBarAdvance(id, frame, x, y, width, scene);
    scene.Show(*this->services);
    return result;
}

// swd3e.exe 0x00457B50
int BattleHud::DrawPictureFrame(int id, int frame, int x, int y)
{
    smooth::Scene scene;
    const auto result = smooth::HudFrame(*this).DrawPictureFrameAdvance(id, frame, x, y, scene);
    scene.Show(*this->services);
    return result;
}

// swd3e.exe 0x00457BA0: frame 0 as the background, `fill` pixels of frame 1 over it
int BattleHud::DrawGauge(int id, int x, int y, int fill)
{
    smooth::Scene scene;
    const auto result = smooth::HudFrame(*this).DrawGaugeAdvance(id, x, y, fill, scene);
    scene.Show(*this->services);
    return result;
}

// swd3e.exe 0x00457C20: as DrawGauge with frame 2
int BattleHud::DrawGaugeAlt(int id, int x, int y, int fill)
{
    smooth::Scene scene;
    const auto result = smooth::HudFrame(*this).DrawGaugeAltAdvance(id, x, y, fill, scene);
    scene.Show(*this->services);
    return result;
}

// swd3e.exe 0x00457CA0: frame 1 drawn fill + 2 pixels wide (also when fill is 0)
int BattleHud::DrawGaugeWide(int id, int x, int y, u16 fill)
{
    smooth::Scene scene;
    const auto result = smooth::HudFrame(*this).DrawGaugeWideAdvance(id, x, y, fill, scene);
    scene.Show(*this->services);
    return result;
}

// swd3e.exe 0x00457D20: up to ten decimal digits right aligned at x (frames of picture `font`)
int BattleHud::DrawSmallNumber(int font, int value, int x, int y)
{
    smooth::Scene scene;
    const auto result = smooth::HudFrame(*this).DrawSmallNumberAdvance(font, value, x, y, scene);
    scene.Show(*this->services);
    return result;
}

// swd3e.exe 0x00457E10: a ten-digit number from x without leading zeros (the units digit always drawn)
int BattleHud::DrawNumber(i16 font, int value, int x, int y)
{
    smooth::Scene scene;
    const auto result = smooth::HudFrame(*this).DrawNumberAdvance(font, value, x, y, scene);
    scene.Show(*this->services);
    return result;
}

// swd3e.exe 0x00457F70: the digit for `divisor` 16 pixels left of numberX; its width (0 for a leading zero)
u16 BattleHud::DrawNumberDigit(u32 divisor)
{
    smooth::Scene scene;
    const auto result = smooth::HudFrame(*this).DrawNumberDigitAdvance(divisor, scene);
    scene.Show(*this->services);
    return result;
}

// swd3e.exe 0x00458040: the looping animation `id` of one of eight slots
int BattleHud::DrawSlotAnimation(int id, int slot, int x, int y)
{
    smooth::Scene scene;
    const auto result = smooth::HudFrame(*this).DrawSlotAnimationAdvance(id, slot, x, y, scene);
    scene.Show(*this->services);
    return result;
}

// swd3e.exe 0x004580C0: the colour argument itself is the source picture: its first pixel blended in by the raw mode 8.
// A colour whose low word is the RLE magic makes the original decode its stack argument as an RLE picture with the
// alpha map of a null argument (deviations.md): nothing is drawn.
int BattleHud::FillColor(int x, int y, int width, int height, u32 color)
{
    if ((color & kColorWord) == this->painter.Rle().magic)
    {
        return 0;
    }

    u32 slot = color;
    this->painter.SetSource(&slot);
    const int result = this->painter.DrawPicture(x, y, width, height, Painter::RawGradient);
    this->painter.SetSource(nullptr);
    return result;
}

// swd3e.exe 0x004580F0: one step of the shared effect animation (mirrored when `mirror` is 1); 1 once it reached its
// end
int BattleHud::DrawEffectAt(int id, int action, int x, int y, int mirror)
{
    smooth::Scene scene;
    const auto result = smooth::HudFrame(*this).DrawEffectAtAdvance(id, action, x, y, mirror, scene);
    scene.Show(*this->services);
    return result;
}

// swd3e.exe 0x004581B0
int BattleHud::ResetEffectAt()
{
    this->effectAct = ActState{};
    return 0;
}

// swd3e.exe 0x004581D0
int BattleHud::DrawCursorAnimation(int id, int x, int y)
{
    smooth::Scene scene;
    const auto result = smooth::HudFrame(*this).DrawCursorAnimationAdvance(id, x, y, scene);
    scene.Show(*this->services);
    return result;
}

// swd3e.exe 0x00458240: action `action` of the role icon act 0x23AA at (x, y) less the action's offset; the picture is
// not kept as the last one
int BattleHud::DrawRoleIcon(int action, int x, int y)
{
    smooth::Scene scene;
    const auto result = smooth::HudFrame(*this).DrawRoleIconAdvance(action, x, y, scene);
    scene.Show(*this->services);
    return result;
}

// swd3e.exe 0x004582C0: a digit of font 0 (picture 0x2359) or 1 (0x2358); other fonts redraw the last picture
u16 BattleHud::DrawDigit(int font, int digit, int x, int y)
{
    smooth::Scene scene;
    const auto result = smooth::HudFrame(*this).DrawDigitAdvance(font, digit, x, y, scene);
    scene.Show(*this->services);
    return result;
}

const Picture* BattleHud::RunScroll(u16 id, int action)
{
    ActState& state = this->scrollAct;
    state.id = id;
    state.action = action;

    if (this->acts.Run(state) == 0)
    {
        return nullptr;
    }

    const Picture* picture = this->archive.GetPicture(state.frame, state.picture);

    if (picture != nullptr)
    {
        this->painter.SetSource(picture->data.data());
    }

    return picture;
}

// swd3e.exe 0x00458330: the scroll bar of act `id`: the top cap (action 0x1A, 0x1E with style 1), `rows` track pieces
// (0x18), the thumb (0x19) sized 7 / scrollCount of the track and placed by scrollFirst, the bottom cap (0x1B, 0x1F
// with style 2)
int BattleHud::DrawScrollBar(u16 id, int x, int y, int rows, int offset, int style)
{
    smooth::Scene scene;
    const auto result = smooth::HudFrame(*this).DrawScrollBarAdvance(id, x, y, rows, offset, style, scene);
    scene.Show(*this->services);
    return result;
}

// swd3e.exe 0x00458670: the coin toss of act 0x239B (a side drawn first, the wins cleared once counted): the coin spins
// centred at (0x104, 0xC8), shrinking by coinZoom, its turns slowing down every 25 frames (a sound on each turn); at a
// delay of 64 it stops (0x117; side 1 counts a win in the high word of coinState); its face (action side + 1) shows
// for 30 frames (0x197), then action 3 grows by five times coinZoom (0x3C a frame) up to 0x400, 1 at its end.
int BattleHud::DrawCoinToss()
{
    smooth::Scene scene;
    const auto result = smooth::HudFrame(*this).DrawCoinTossAdvance(scene);
    scene.Show(*this->services);
    return result;
}

// swd3e.exe 0x00458A00: the timing bar (picture 0x2380) with three marks and a moving needle; timingHit is the mark
// (1-based) under the needle unless it equals the low word of numberFont, which is cleared otherwise. 1 when the needle
// reaches the end.
int BattleHud::DrawTimingBar(int x, int y)
{
    smooth::Scene scene;
    const auto result = smooth::HudFrame(*this).DrawTimingBarAdvance(x, y, scene);
    scene.Show(*this->services);
    return result;
}

// swd3e.exe 0x00458BB0: the vertical gauge of picture 0x237A showing `level` sixths of its body (its body and shine
// shorter for a gauge of 4 `levels`)
int BattleHud::DrawTallGauge(int x, int y, int level, int levels)
{
    smooth::Scene scene;
    const auto result = smooth::HudFrame(*this).DrawTallGaugeAdvance(x, y, level, levels, scene);
    scene.Show(*this->services);
    return result;
}

// swd3e.exe 0x00459640: one step of the animation with the picture loaded for its frame; its waiting flag
int BattleHud::DrawAnimation(BattleAnimation& animation)
{
    smooth::Scene scene;
    const auto result = smooth::HudFrame(*this).DrawAnimationAdvance(animation, scene);
    scene.Show(*this->services);
    return result;
}

// swd3e.exe 0x00459830
int BattleHud::FreeAnimation(BattleAnimation& animation)
{
    for (std::unique_ptr<Picture>& picture : animation.pictures)
    {
        picture.reset();
    }

    animation.id = 0;
    animation.wave = 0;
    animation.act = ActState{};
    return 0;
}

// swd3e.exe 0x004594F0: loads every frame picture of act `id` (run to its end) into the animation, each shifted right
// by 640 / `divisor`; the wave mode of 'WT' is kept. `unused` is ignored. Always 0.
int BattleHud::LoadAnimation(BattleAnimation& animation, int, int x, int y, u16 id, u16 divisor)
{
    std::array<u16, BattleAnimation::Pictures> loaded = {};
    loaded.fill(kNotLoaded);
    ActState& state = animation.act;
    animation.id = id;
    animation.x = x;
    animation.y = y;
    state.id = id;
    state.action = 0;

    if (this->acts.Run(state) == 0)
    {
        return 0;
    }

    for (;;)
    {
        if (state.wait != 0)
        {
            animation.wave = state.wait;
        }

        const u16 frame = state.picture;

        if (frame < BattleAnimation::Pictures && loaded[frame] == kNotLoaded)
        {
            animation.pictures[frame] = this->archive.LoadPictureDirect(state.frame, state.picture);
            loaded[frame] = frame;

            if (animation.pictures[frame] != nullptr)
            {
                ShiftPicture(animation.pictures[frame]->data, ShiftMode::Right, Display::Width / divisor);
            }
        }

        if (state.position == 0)
        {
            break;
        }

        state.action = 0;
        state.id = animation.id;

        if (this->acts.Run(state) == 0)
        {
            return 0;
        }
    }

    state = ActState{};
    return 0;
}

// swd3e.exe 0x004596E0: plays the loaded animation to its end in one call, every new frame shifted by `amount` (right
// when positive, left when negative) and drawn; 1 when it ended, 0 without an animation. `unused` is ignored.
int BattleHud::PlayAnimationShifted(BattleAnimation& animation, int unused, int amount)
{
    smooth::Scene scene;
    const auto result = smooth::HudFrame(*this).PlayAnimationShiftedAdvance(animation, unused, amount, scene);
    scene.Show(*this->services);
    return result;
}

// swd3e.exe 0x00457630: a box of `columns` x `rows` tiles of picture `id` around a filled area (`fill`, from frame 4,
// 16 pixels a tile with a border tile each side): frames 0..2 the top (corner, edge repeated, corner), 3 and 5 the
// sides of each row, 6..8 the bottom. A missing frame is read through a null pointer in the original; it is not drawn
// and takes no room here.
void BattleHud::DrawTileBox(int id, int x, int y, int columns, int rows, i8 fill)
{
    smooth::Scene scene;
    smooth::HudFrame(*this).DrawTileBoxAdvance(id, x, y, columns, rows, fill, scene);
    scene.Show(*this->services);
}

// swd3e.exe 0x00459170: the top of the meter
i16 BattleHud::SetMeterLimit(i16 limit)
{
    this->meterLimit = limit;
    return limit;
}

// swd3e.exe 0x00459180: `amount` added to the meter (-1 empties it; the last amount kept, a negative one as 0, and a
// meter below 0 empties); past its top the meter empties (0), else 1
int BattleHud::AddToMeter(i16 amount)
{
    if (amount == kEmptyMeter)
    {
        this->meterLastAdd = 0;
        this->meterValue = 0;
    }
    else
    {
        this->meterLastAdd = amount < 0 ? i16{0} : amount;
        this->meterValue = static_cast<i16>(this->meterValue + amount);

        if (this->meterValue < 0)
        {
            this->meterValue = 0;
        }
    }

    if (this->meterValue <= this->meterLimit)
    {
        return 1;
    }

    this->meterLastAdd = 0;
    this->meterValue = 0;
    return 0;
}

// swd3e.exe 0x004591E0
int BattleHud::ResetMeter()
{
    this->meterLevel = 0;
    this->meterValue = 0;
    this->meterShown = 0;
    return 0;
}

// swd3e.exe 0x00459200: while the meter shows, its frame (picture 0x2397 frame 0) and its fill (frame 1) clipped to the
// shown level, which moves a pixel a frame towards the meter's share of its top (in pixels of the fill; tinted while it
// moves); the value
// below it
void BattleHud::DrawMeter(int x, int y, smooth::Scene& scene)
{
    if (this->meterShown == 0)
    {
        return;
    }

    smooth::ScenePainter painter(*this->services, scene);
    const Picture* frame = this->archive.GetPicture(kMeterPicture, 0);
    this->lastPicture = frame;

    // A missing picture is read through a null pointer in the original.
    if (frame == nullptr)
    {
        return;
    }

    painter.SetSource(frame->data.data());
    painter.DrawPicture(x, y, frame->width, frame->height, Painter::Copy, PaletteArgument(*frame));
    const Picture* fill = this->archive.GetPicture(kMeterPicture, 1);
    this->lastPicture = fill;

    if (fill == nullptr)
    {
        return;
    }

    double share = static_cast<double>(this->meterValue) / static_cast<double>(this->meterLimit);

    if (share > 1.0)
    {
        share = 1.0;
    }

    const int height = fill->height;
    const auto target = static_cast<i32>(crt::Ftol(static_cast<double>(height) * share));

    if (this->meterLevel != target)
    {
        this->meterLevel = this->meterLevel >= target ? this->meterLevel - 1 : this->meterLevel + 1;
    }

    painter.SetClipClamped(x, y + height - this->meterLevel, x + fill->width, height + y);
    painter.SetSource(fill->data.data());
    painter.DrawPicture(x, y, fill->width, fill->height, Painter::Copy, PaletteArgument(*fill));

    if (target != this->meterLevel)
    {
        DrawOptions& options = painter.Options();
        options.tintRed = kMeterTint;
        options.tintGreen = kMeterTint;
        options.tintBlue = kMeterTint;
        painter.DrawPicture(x, y, fill->width, fill->height, Painter::TintUnder, PaletteArgument(*fill));
    }

    painter.SetClipClamped(0, 0, Display::Width, Display::Height);
    smooth::HudFrame(*this)
        .DrawNumberAdvance(kMeterNumbers, this->meterValue, x + (fill->width >> 1), y + fill->height, scene);
}

// swd3e.exe 0x00458DC0: the roulette widened a step: the first two digits rise to 9, then a third digit (to 9), then
// a fourth
void BattleHud::RaiseRoulette()
{
    Roulette& roulette = this->roulette;

    for (std::size_t digit = 0; digit < roulette.tops.size(); ++digit)
    {
        if (roulette.tops[digit] < Roulette::TopDigit)
        {
            if (digit >= Roulette::BaseDigits)
            {
                roulette.digits = static_cast<i32>(digit + 1);
            }

            ++roulette.tops[digit];
            return;
        }
    }
}

// swd3e.exe 0x00458D80: the roulette of the dragon treasure (accessory kind 0x25): two digits of 0..5, widened `steps`
// times (its proficiency)
void BattleHud::SetupRoulette(i16 steps)
{
    Roulette& roulette = this->roulette;
    roulette.tops = Roulette::InitialTops;
    roulette.digits = Roulette::BaseDigits;

    for (i16 step = 0; step < steps; ++step)
    {
        this->RaiseRoulette();
    }
}

// swd3e.exe 0x00458FD0: no digit stopped, each spinning from a random 0..9
void BattleHud::ResetRoulette()
{
    Roulette& roulette = this->roulette;
    roulette.stopped = 0;
    roulette.result = {};
    roulette.spins = {};

    for (i32 digit = 0; digit < roulette.digits; ++digit)
    {
        roulette.spinning[static_cast<std::size_t>(digit)] = static_cast<u8>(this->random.Below(Roulette::Digits));
    }
}

// swd3e.exe 0x00458E30: one frame of the roulette at (x, y) (its frame picture 0x23B0, frame 1 when `stop`): the
// stopped digits, then the spinning ones from the right, the first two stepping every third frame, the third every
// second, the fourth every frame (sound 0x125 as the next to stop turns). `stop` 1 stops the next digit; once all are
// stopped `value` takes their number (1).
int BattleHud::DrawRoulette(int x, int y, i32& value, int stop)
{
    Roulette& roulette = this->roulette;
    const Picture* frame = this->archive.GetPicture(kRoulettePicture, stop == 1 ? 1 : 0);
    this->lastPicture = frame;

    // A missing picture is read through a null pointer in the original.
    if (frame != nullptr)
    {
        smooth::Scene scene;
        smooth::ScenePainter painter(*this->services, scene);
        painter.SetSource(frame->data.data());
        painter.DrawPicture(
            x - frame->width + kRouletteFrameLeft,
            y - (frame->height >> 1) + kRouletteFrameDown,
            frame->width,
            frame->height,
            Painter::Copy,
            PaletteArgument(*frame)
        );
        scene.Show(*this->services);
    }

    int period = Roulette::SlowPeriod;
    int digitX = x;

    for (i32 digit = 0; digit < roulette.digits; ++digit)
    {
        const auto at = static_cast<std::size_t>(digit);

        if (digit >= Roulette::BaseDigits)
        {
            --period;
        }

        if (digit < roulette.stopped)
        {
            this->DrawNumber(kMeterNumbers, roulette.result[at], digitX, y);
        }
        else
        {
            const u8 shown = roulette.spinning[at];

            if (stop == 1)
            {
                roulette.result[at] = static_cast<i8>(shown);
            }

            this->DrawNumber(kMeterNumbers, shown, digitX, y);

            if (roulette.spins[at] == period)
            {
                roulette.spins[at] = 0;
                roulette.spinning[at] = static_cast<u8>((roulette.spinning[at] + 1) % (roulette.tops[at] + 1));
            }

            ++roulette.spins[at];
        }

        digitX -= kRouletteDigitWidth;
    }

    // The counter after the last digit is read past them in the original when all are stopped; 0 here.
    const auto next = static_cast<std::size_t>(roulette.stopped);

    if (next < roulette.spins.size() && roulette.spins[next] == 1)
    {
        this->audio.PlaySound(kRouletteSound, this->services->config.soundVolume);
    }

    if (stop == 1)
    {
        ++roulette.stopped;
    }

    if (roulette.stopped < roulette.digits)
    {
        return 0;
    }

    value = roulette.result[0] + kTens * roulette.result[1] + kHundreds * roulette.result[2] +
            kThousands * roulette.result[3];
    return 1;
}

// swd3e.exe 0x00459010: the prize of the stopped roulette: all zeros raise the battle's experience by ten percent a
// digit; all sevens double its money a seven (from the second digit); other equal digits give the treasure ten points
// of experience a digit (returned). Each is announced.
u16 BattleHud::RouletteBonus()
{
    Roulette& roulette = this->roulette;
    BattleState& battle = this->services->state;
    const i32 digits = roulette.digits;
    const i8 first = roulette.result[0];

    auto announce = [this, &roulette](std::u8string_view format, int amount)
    {
        const std::string text = WinFormat(format, {amount});
        const std::size_t length = std::min(text.size(), roulette.text.size() - 1);
        std::copy_n(text.begin(), length, roulette.text.begin());
        roulette.text[length] = '\0';
        this->services->messages
            .Add(kRouletteMessageX, kRouletteMessageY, kRouletteMessageFrames, roulette.text, kRouletteFlags);
    };

    if (first == 0)
    {
        int bonus = kRouletteStep;
        battle.experienceBonus = kRouletteStep;

        for (i32 digit = 1; digit < digits; ++digit)
        {
            if (roulette.result[static_cast<std::size_t>(digit)] != 0)
            {
                battle.experienceBonus = 0;
                return 0;
            }

            bonus += kRouletteStep;
        }

        battle.experienceBonus = static_cast<u16>(bonus);
        announce(kExperienceBonus, bonus);
        return 0;
    }

    if (first == kLuckyDigit)
    {
        u16 times = battle.moneyTimes;

        for (i32 digit = 1; digit < digits; ++digit)
        {
            if (roulette.result[static_cast<std::size_t>(digit)] != kLuckyDigit)
            {
                battle.moneyTimes = 0;
                return 0;
            }

            times = static_cast<u16>(times + kLuckyStep);

            if (digit + 1 >= digits)
            {
                battle.moneyTimes = times;
            }
        }

        if (times != 0)
        {
            announce(kMoneyTimes, times);
        }

        return 0;
    }

    u16 bonus = 0;

    if (digits > 1)
    {
        for (i32 digit = 1; digit < digits; ++digit)
        {
            if (roulette.result[static_cast<std::size_t>(digit)] != first)
            {
                return 0;
            }

            bonus = static_cast<u16>(bonus + kRouletteStep);
        }

        if (bonus != 0)
        {
            announce(kTreasureExperience, bonus);
        }
    }

    return bonus;
}

// swd3e.exe 0x004593C0: one frame of picture `id` growing from its middle in slot `slot` (0..2), added to the screen
// with no tint: its zoom widens by 0x3C and heightens by 0x50 a frame; 1 (the slot cleared) once it is 0x4B0 wider
int BattleHud::DrawGrowing(int slot, int id, int x, int y, smooth::Scene& scene)
{
    const auto at = static_cast<std::size_t>(slot);

    // Slots past the three write the zooms after them in the original (deviations.md): over at once here.
    if (at >= this->growWidth.size())
    {
        return 1;
    }

    const Picture* picture = this->archive.GetPicture(static_cast<u32>(id), 0);
    this->lastPicture = picture;

    // A missing picture is read through a null pointer in the original.
    if (picture != nullptr)
    {
        smooth::ScenePainter painter(*this->services, scene);
        painter.SetSource(picture->data.data());
        painter.SetZoom(
            picture->width >> 1,
            picture->height >> 1,
            this->growWidth[at] + kGrowUnit,
            this->growHeight[at] + kGrowUnit
        );
        DrawOptions& options = painter.Options();
        options.tintRed = 0;
        options.tintGreen = 0;
        options.tintBlue = 0;
        painter.DrawPicture(x, y, picture->width, picture->height, Painter::ZoomAddTinted);
    }

    this->growHeight[at] += kGrowHeightStep;
    this->growWidth[at] += kGrowWidthStep;
    --this->growFrames[at];

    if (this->growWidth[at] < kGrowEnd)
    {
        return 0;
    }

    this->growHeight[at] = 0;
    this->growWidth[at] = 0;
    this->growFrames[at] = 0;
    return 1;
}

// swd3e.exe 0x004594B0: frame 0 of picture `id` at (x, y) with its palette
int BattleHud::DrawPicturePalette(int id, int x, int y)
{
    const Picture* picture = this->archive.GetPicture(static_cast<u32>(id), 0);
    this->lastPicture = picture;

    // A missing picture is read through a null pointer in the original.
    if (picture == nullptr)
    {
        return 0;
    }

    smooth::Scene scene;
    smooth::ScenePainter painter(*this->services, scene);
    painter.SetSource(picture->data.data());
    const int result =
        painter.DrawPicture(x, y, picture->width, picture->height, Painter::Copy, PaletteArgument(*picture));
    scene.Show(*this->services);
    return result;
}
}  // namespace swd3e
