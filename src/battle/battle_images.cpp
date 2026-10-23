#include "audio/audio_manager.h"
#include "battle/battle_services.h"
#include "battle/battle_unit.h"
#include "battle/battle_unit_constants.h"
#include "battle/smooth/smooth_draw.h"
#include "config/config.h"
#include "draw/painter.h"
#include "pic/picture.h"
#include "res/act_archive.h"
#include "res/tsw_archive.h"

namespace swd3e
{

// swd3e.exe 0x0046B000: `image` appended (a copy), unless it is marked Once and repeats the last one (its frame and
// picture, or its word +0xB8)
void ImageList::Add(const Image& image)
{
    const bool repeated =
        (this->lastFrame == image.frame && this->lastPicture == image.picture) || this->lastB8 == image.unkB8;

    if ((image.flags & Image::Once) != 0 && repeated)
    {
        return;
    }

    std::unique_ptr<Image>* link = &this->first;

    while (*link != nullptr)
    {
        link = &(*link)->next;
    }

    *link = std::make_unique<Image>(Image{
        .x = image.x,
        .y = image.y,
        .mode = image.mode,
        .frame = image.frame,
        .picture = image.picture,
        .unkB8 = image.unkB8,
        .flags = image.flags,
        .tintRed = image.tintRed,
        .tintGreen = image.tintGreen,
        .tintBlue = image.tintBlue,
        .level = image.level,
        .step = image.step,
    });

    ++this->count;
    this->lastFrame = image.frame;
    this->lastPicture = image.picture;
    this->lastB8 = image.unkB8;
}

// swd3e.exe 0x0046B0C0: each image fades by its step (not below 0) and is drawn translucent at its level, then tinted
// under itself while the level lasts; a faded one is freed. Always 1.
int ImageList::Draw(BattleServices& services)
{
    smooth::Scene scene;
    smooth::ScenePainter painter(services, scene);
    std::unique_ptr<Image>* link = &this->first;

    while (*link != nullptr)
    {
        Image& image = **link;
        const Picture* picture = services.archive.GetPicture(image.frame, image.picture);

        if (image.level > 0)
        {
            image.level = static_cast<i16>(image.level - image.step);

            if (image.level < 0)
            {
                image.level = 0;
            }
        }

        // A missing picture is read through a null pointer in the original.
        if (picture != nullptr)
        {
            painter.SetSource(picture->data.data());
            painter.Options().alpha = image.level;
            painter.DrawPicture(image.x, image.y, picture->width, picture->height, image.mode | Painter::Translucent);
        }

        bool kept = false;

        if (image.level > 0)
        {
            if (picture != nullptr)
            {
                DrawOptions& options = painter.Options();
                options.tintRed = image.tintRed;
                options.tintGreen = image.tintGreen;
                options.tintBlue = image.tintBlue;
                painter.DrawPicture(image.x, image.y, picture->width, picture->height, image.mode | Painter::TintUnder);
            }

            kept = image.level > 0;
        }

        if (kept)
        {
            link = &image.next;
            continue;
        }

        --this->count;
        *link = std::move(image.next);
    }

    scene.Show(services);
    return 1;
}

// swd3e.exe 0x0046B200
bool ImageList::Empty() const
{
    return this->first == nullptr;
}

namespace
{

constexpr i32 kKnockBackAction = 0x3A;
constexpr i32 kStillAct = 0xFA;  // the act of a unit under mark bit 4
constexpr i32 kStandAction = 0x24;
constexpr u16 kKnockBackSound = 5;
constexpr int kKnockBackStep = 8;  // the unit slides 8 pixels a frame, six frames
constexpr u16 kKnockBackSteps = 6;
constexpr i16 kKnockBackTintBlue = 0xF;
constexpr i16 kKnockBackLevel = 0xF;
constexpr i16 kKnockBackFade = 2;
constexpr std::size_t kKnockBackTrail = 1;

}  // namespace

// swd3e.exe 0x0048FB20: one frame of a knock back (unk26B8 bit 26): the attack act takes action 0x3A (0x24 for an
// enemy or under mark bit 4, act 0xFA then) and slides the unit 8 pixels a frame away from the enemies for six
// frames with sound 5, each frame leaving a fading afterimage; the unit is drawn over its shadow. Once the
// afterimages are gone (and mark bit 0 is clear) the knock back ends: 1. 1 also when the act cannot run or the unit
// guards; 0 while it lasts.
int BattleUnit::KnockBackFrame(BattleServices& services)
{
    ActState& act = this->attackAct;
    this->unk2AAC = 1;
    act.id = this->actId;
    act.action = kKnockBackAction;

    if (this->MarkBit4() == 1)
    {
        act.id = kStillAct;
        act.action = kStandAction;
    }

    if (this->kind == 0)
    {
        act.action = kStandAction;
    }

    act.hold = this->held == 1 ? 1 : 0;

    if (services.acts.Run(act) == 0 || this->guarding == 1)
    {
        return 1;
    }

    this->shadowPicture = services.archive.GetPicture(act.frame, act.picture);

    if (this->hurtStep == 0)
    {
        services.audio.PlaySound(kKnockBackSound, services.config.soundVolume);
        services.audio.SetSoundPan(kKnockBackSound, this->kind == BattleUnit::PartyRole ? kFadePan : -kFadePan);
    }

    const int y = this->placement.y - act.y;
    const u16 step = this->hurtStep;
    const int away = this->kind == BattleUnit::PartyRole ? step : -static_cast<int>(step);
    const int x = this->placement.x - act.x + kKnockBackStep * away;
    this->drawFlags = act.flags;

    if (step < kKnockBackSteps)
    {
        this->hurtStep = static_cast<u16>(step + 1);
        this->images.Add(
            ImageList::Image{
                .x = x,
                .y = y,
                .mode = act.flags,
                .frame = act.frame,
                .picture = act.picture,
                .tintBlue = kKnockBackTintBlue,
                .level = kKnockBackLevel,
                .step = kKnockBackFade,
            }
        );
    }

    this->images.Draw(services);

    // A missing picture is read through a null pointer in the original.
    if (this->shadowPicture != nullptr)
    {
        const Picture& picture = *this->shadowPicture;
        smooth::Scene scene;
        smooth::ScenePainter painter(services, scene);
        painter.SetSource(picture.data.data());
        const int shadowHeight = picture.height / kShadowShare;
        DrawOptions& options = painter.Options();
        options.scaleHeight = shadowHeight;
        options.shear = picture.height >> kShadowShearShift;
        options.tintRed = kShadowTint;
        options.tintGreen = kShadowTint;
        options.tintBlue = kShadowTint;
        this->shadowMode = (this->drawFlags & (Painter::RleSource | Painter::Mirror | Painter::Flip)) | Painter::Shadow;
        painter.DrawPicture(x, this->placement.y - shadowHeight, picture.width, picture.height, this->shadowMode);

        if ((this->drawFlags & ActState::Signal) == ActState::Signal)
        {
            painter.Options().alpha = act.signal;
        }

        // The main act's wave decides whether the attack act's is used.
        if (this->mainAct.wait != 0)
        {
            painter.SetWave(act.wait);
        }

        painter.DrawPicture(x, y, picture.width, picture.height, this->drawFlags);
        painter.SetWave(0);
        scene.Show(services);
    }

    if ((this->marks & BattleUnit::UnkMark1) != 0 || !this->images.Empty())
    {
        return 0;
    }

    this->hurtStep = 0;
    this->trails[kKnockBackTrail].started = 0;
    this->unk2AAC = 0;
    this->unk26B8 &= ~kFlag26B8Blocked;
    act = ActState{};
    return 1;
}
}  // namespace swd3e
