#include "battle/battle_unit.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <utility>

#include "audio/audio_manager.h"
#include "battle/battle_enemy.h"
#include "battle/battle_hud.h"
#include "battle/battle_message.h"
#include "battle/battle_role.h"
#include "battle/battle_services.h"
#include "battle/battle_state.h"
#include "battle/battle_unit_constants.h"
#include "battle/smooth/smooth_projector.h"
#include "battle/smooth/smooth_unit.h"
#include "config/config.h"
#include "core/error_box.h"
#include "core/format.h"
#include "core/words.h"
#include "crt/msvc.h"
#include "draw/draw_placer.h"
#include "draw/paint_record.h"
#include "draw/painter.h"
#include "org/database.h"
#include "org/fame.h"
#include "party/party.h"
#include "pic/picture.h"
#include "pic/rle.h"
#include "platform/display.h"
#include "random/random.h"
#include "res/act_archive.h"
#include "res/tsw_archive.h"

namespace swd3e::smooth
{

namespace
{

constexpr i32 kRoleHomeAction = 0x2F;     // swd3e (swd32: 0x32)
constexpr u32 kStandsAside = 0x10000000;  // unk2BC0: under StatusBit5 the unit stands aside and does not walk home
constexpr int kFixedOriginX = 0xF0;       // HasFixedOrigin: the walk starts from here
constexpr int kFixedOriginY = 0x12C;
constexpr int kHomeSteps = 0x1C;  // ReturnHome: line steps per frame and frame followed up
constexpr int kGhostHomeSteps = 0x18;
constexpr i32 kHomePose = 0x28;
constexpr i32 kHomePose1 = 0x30;           // an enemy with unk2FB6 1
constexpr i32 kHomePose2 = 0x31;           // and with kSecondHomePose
constexpr i32 kRoleWalkAction = 0x2E;      // swd3e (swd32: 0x2A)
constexpr u32 kWalkBlocked = 0x80000000u;  // unk2BC0: the unit does not walk (WalkToTarget, swd3e)
constexpr u32 kVanishWalk = 0x2000000;     // unk2BC0: the act shows at the target until it ends (swd3e)
constexpr i32 kVanishAction = 0x39;
constexpr u16 kVanishingAct = 1;  // a unit of act 1 sets kVanishWalk when it arrives
constexpr int kWalkSteps = 0x18;  // WalkToTarget: line steps per frame walked
constexpr int kGhostWalkSteps = 0x10;
constexpr int kWalkShare = 16;         // with UnkFlag20 a sixteenth of the distance once the act follows up
constexpr u16 kArrivedMark = 6;        // unk2EDC when the walk arrives
constexpr u16 kNudgesTarget = 0x1000;  // DrawAction (swd3e): ActState::target bit nudging the target once
constexpr u16 kActNudge = 0x28;
constexpr u16 kEnemyFixedX = 0x1A4;  // HasFixedOrigin: an enemy's act stands here
constexpr u16 kEnemyFixedY = 0x118;
constexpr u16 kAfterImageTarget = 0x100;  // with FollowsUp the act leaves afterimages
constexpr u32 kAfterImages = 0x40000;     // unk2BC0: the unit's acts leave afterimages
constexpr int kRoundShift = 8;            // ActState::counter: the round in the high byte
constexpr i16 kAfterImageTintBlue = 0xA;
constexpr i16 kAfterImageLevel = 0xE;
constexpr i16 kAfterImageFade = 1;

constexpr int kNumberAbove = 0x1E;
constexpr int kDigitWidth = 0x10;  // swd3e (swd32: 0xC)
constexpr i16 kLastHop = 9;
constexpr i16 kNumberFadeStep = 2;
constexpr int kNumberScaleBase = 0x1A;
constexpr int kGuardTextOffset = 0x14;  // the guard text starts left of and above the unit
constexpr i16 kGuardPhaseEnd = 9;       // the last phase of a guard text letter
constexpr i16 kGuardTextRise = 0x18;    // pixels the guard text rises before it is gone
constexpr i16 kGuardTextStep = 4;       // pixels the guard text rises a frame
constexpr int kGuardTextHeight = 0x1A;
constexpr std::array<int, 4> kGuardLetterX = {0, 0x12, 0x1E, 0x2E};  // swd3e: the guard letters from the left
constexpr i16 kRecoilFadeStep = 2;
constexpr i16 kRecoilFadeEnd = -0x20;  // DrawRecoil: the tint at which a waiting recoil act is over
constexpr i8 kAdditiveTrailFade = 3;   // an additive act's images fade by 3 a frame, the others by 1
constexpr i16 kAdditiveTrailLevel = 0x18;
constexpr i32 kQueuedMaxDamage = 0x270F;
constexpr i16 kSpeedingTop = 0x20;  // RunQueuedActions (swd3e): a speeding flight doubles up to it, then adds 0x30
constexpr i16 kSpeedingStep = 0x30;
constexpr u32 kEvadeChanceStatus = 0x80;  // status bit 7: the target evades with a chance of kStatusEvadeChance
constexpr u16 kStatusEvadeChance = 8;
constexpr int kTrailBlue = -16;
constexpr int kTrailCentre = 0xC;
constexpr u8 kTrailDelay = 1;  // a new image every second frame
constexpr i16 kTrailLevel = 0xC;
constexpr int kTrailSpacing = 0x1E;
constexpr int kTrailTintBase = 0x1F;
constexpr i16 kBlueFillStep = 6;  // swd3e (swd32: 4)
constexpr int kBlueFillTint = 0x10;
constexpr i32 kCommand1Action = 0x28;  // the fade in action after command 1
constexpr i16 kDarkFillStep = 0xE;
constexpr int kDarkFillTint = -0x18;
constexpr u16 kEffectIcons = 0x23B6;  // swd3e
constexpr int kFadeInTint = 0x14;     // swd3e: a unit fading in is tinted lighter under itself, less as it shows
constexpr u16 kFixedFadeInX = 0x1A4;  // swd3e: where an enemy of UnitCommand::FixedOrigin fades in
constexpr u16 kFixedFadeInY = 0x118;
constexpr int kFadeOutSound = 0x29B;   // swd3e (swd32: 0x2F)
constexpr int kFadeOutTint = 5;        // swd3e: a fading unit is tinted lighter under itself
constexpr std::size_t kFadeTrail = 1;  // swd3e: DrawFadeOut ends with trail 1 stopped
constexpr i16 kFadeOutStep = 2;
constexpr i16 kHalvesFast = 4;
constexpr i16 kHalvesPause = 8;
constexpr i16 kHalvesSlow = 9;
constexpr u16 kPose2 = 2;
constexpr i32 kPose2Action = 0x31;  // of pose 2
constexpr i32 kPoseAction = 0x30;   // the fade in action of a posed unit
constexpr int kQuarterDrop = 0x10;
constexpr i16 kRisingEnd = 0x1E0;
constexpr i16 kRisingStep = 0x1F;
constexpr int kRisingTintDivisor = -31;
constexpr int kRisingTintLift = 4;
constexpr u16 kRunSound = 0x2F;
constexpr int kShadowLeftShift = 5;
constexpr int kSinkAlphaBase = 0x1F;
constexpr i16 kSinkFadeFrom = -0x10;
constexpr int kSinkStep = 10;
constexpr i16 kSlideFadeFrom = -15;
constexpr i16 kSplitEnd = 0x20;
constexpr i16 kSplitStep = 2;
constexpr u16 kStepPicture = 1;  // the picture of the act that plays the step sound
constexpr u16 kStepSound = 0x10F;
constexpr i16 kWaveEnd = 0x2D;
constexpr int kWaveShare = 3;
constexpr i16 kParticlesStarted = 0x65;
constexpr i32 kBurstTarget = 2;
constexpr int kBurstTargetShare = 3;     // swd3e: pose 2 bursts towards a third of the picture (swd32: half)
constexpr u32 kBurstDecodeLine = 0x75D;  // the lines of role.cpp reported when a particle pose cannot decode
constexpr u32 kSweepDecodeLine = 0x8EA;
constexpr u32 kStripDecodeLine = 0xB38;
constexpr u16 kBurstSpeed = 0x32;
constexpr u16 kBurstLife = 0xFA;
constexpr u16 kBurstDivisor = 0xA;
constexpr i32 kSweepLeft = -0x1E;
constexpr i32 kSweepRight = 0x29E;
constexpr i32 kSweepAbove = 0xA;
constexpr u16 kSweepSpeed = 0x64;
constexpr u16 kSweepLife = 0x64;
constexpr i32 kProjectedAngle = 0x5A;
constexpr i16 kProjectedStep = 2;
constexpr i32 kProjectedTint = 5;
constexpr i32 kProjectedGrowth = 4;
constexpr i32 kWobbles = 6;
constexpr i16 kWobbleTop = 0x18;
constexpr i16 kWobbleBottom = -8;
constexpr i16 kWobbleRise = 4;
constexpr i32 kStripRight = 0x154;
constexpr i32 kStripWidth = 0x140;
constexpr u16 kStripSpeed = 0x18;
constexpr u16 kStripLife = 0x64;
constexpr u16 kStripDivisor = 0x28;
constexpr i32 kStripTint = 0xF;
constexpr int kShakeSpan = 3;
}  // namespace

UnitFrame::UnitFrame(BattleUnit& unit)
    : unit(unit)
{
}

// swd3e.exe 0x0049A830: the overlay picture (`frame`, `picture` - 1; nothing when either is 0) drawn additively at
// (x + offsetX, y + offsetY), mirrored like the unit: a facing unit measures offsetX (when not 0) from the right edge
// of its current picture, less the overlay's width. The rebuild's continuous motion keeps the choice here (at the
// walk's place in `view`), the draw in DrawWalk.
void UnitFrame::ChooseOverlay(
    BattleServices& services,
    u16 frame,
    u16 picture,
    int offsetX,
    int offsetY,
    WalkView& view
)
{
    BattleUnit& unit = this->unit;

    if (frame == 0 || picture == 0)
    {
        return;
    }

    unit.overlayX = static_cast<u16>(offsetX);
    unit.overlayPicture = services.archive.GetPicture(frame, static_cast<u16>(picture - 1));

    // A missing picture is read through a null pointer in the original.
    if (unit.overlayPicture == nullptr)
    {
        return;
    }

    const bool forced = unit.autoControl == 1 || unit.guestControl == 1;

    auto mirror = [&unit, offsetX]
    {
        if (offsetX != 0 && unit.picture != nullptr)
        {
            unit.overlayX = static_cast<u16>(unit.picture->width - offsetX - unit.overlayPicture->width);
        }
    };

    if (forced && unit.facing == 1)
    {
        mirror();
    }

    if (unit.facing == 1 && unit.autoControl == 0 && unit.guestControl == 0)
    {
        mirror();
    }

    view.overlay = {frame, static_cast<u16>(picture - 1), true};
    view.overlayX = view.x + static_cast<i16>(unit.overlayX);
    view.overlayY = view.y + offsetY;
    view.overlayFlags = unit.frameFlags | Painter::Additive;
}

// What ReturnHome decides of the frame: the acts stepped, the line walked, the sound, the arrival and the overlay
// chosen; the view keeps no picture when it draws nothing.
int UnitFrame::HomeAdvance(BattleServices& services, const BattleUnit& target, i32 id, i32 action, WalkView& view)
{
    BattleUnit& unit = this->unit;

    if (unit.kind != 0)
    {
        if ((unit.unk2BD4 & kWalksHome) == 0 &&
            (unit.command.flags & (UnitCommand::UnkFlag8 | UnitCommand::UnkFlag20)) == 0)
        {
            return 1;
        }
    }
    else if ((unit.command.flags & UnitCommand::UnkFlag10) == 0)
    {
        return 1;
    }

    if (unit.autoControl == 1 || unit.guestControl == 1 ||
        (unit.StatusBit5() == 1 && (unit.unk2BC0 & kStandsAside) != 0) || unit.MarkBit4() == 1)
    {
        return 1;
    }

    unit.unk2FEC = 1;
    ActState& act = unit.attackAct;
    ActState& pose = unit.recoilAct;
    act.hold = unit.held == 1 ? 1 : 0;
    act.id = unit.actId;
    act.action = unit.kind != 0 ? kRoleHomeAction : kEnemyHomeAction;

    if (id != -1)
    {
        act.id = static_cast<u32>(id);
    }

    if (action != 0)
    {
        act.action = action;
    }

    if (services.acts.Run(act) == 0)
    {
        return 0;
    }

    unit.picture = services.archive.GetPicture(act.frame, act.picture);
    pose.id = unit.actId;
    pose.action = kHomePose;

    if (unit.kind == 0)
    {
        if (unit.unk2FB6 == 1)
        {
            pose.action = kHomePose1;
        }

        if (unit.unk2FB6 == kSecondHomePose)
        {
            pose.action = kHomePose2;
        }
    }

    if (services.acts.Run(pose) == 0)
    {
        return 0;
    }

    // A missing picture is read through a null pointer in the original.
    if (unit.picture == nullptr)
    {
        return 0;
    }

    const Picture& picture = *unit.picture;
    services.audio.PlaySound(act.view, services.config.soundVolume);
    act.view = 0;
    unit.actOffsetX = static_cast<u16>(act.x);
    unit.frameFlags = act.flags;
    unit.actAnchorX = act.anchorX;
    unit.homePoseX = pose.x;

    if (unit.facing == 1 || unit.ghost == 1)
    {
        unit.frameFlags ^= Painter::Mirror;
        unit.actOffsetX = static_cast<u16>(picture.width - static_cast<u16>(act.x));
        unit.homePoseX = picture.width - pose.x;

        if (act.anchorX != 0)
        {
            unit.actAnchorX = static_cast<u16>(picture.width - act.anchorX);
        }
    }

    u16 targetX = 0;
    u16 targetY = 0;
    target.GetDrawPosition(targetX, targetY);

    if (unit.HasFixedOrigin() == 1)
    {
        targetX = kFixedOriginX;
        targetY = kFixedOriginY;
    }

    LineWalker& line = unit.flight;
    line = LineWalker{};
    line.fromX = static_cast<i16>(unit.actAnchorX) + static_cast<i16>(targetX) - static_cast<i16>(unit.actOffsetX);
    line.fromY = static_cast<i16>(targetY) + static_cast<i16>(act.anchorY) - act.y;
    line.toX = unit.savedPlacement.x;
    line.toY = unit.savedPlacement.y;

    if (unit.ghost == 1)
    {
        line.toX = unit.placement.x;
        line.toY = unit.placement.y;
    }

    if ((act.target & ActState::FollowsUp) != 0)
    {
        unit.stage |= BattleUnit::FollowingUp;
    }

    if ((unit.stage & BattleUnit::FollowingUp) != 0)
    {
        ++unit.throwSteps;
    }

    const int stepsPerFrame = unit.ghost == 1 ? kGhostHomeSteps : kHomeSteps;
    int outcome = 0;

    if (stepsPerFrame * unit.throwSteps > 0)
    {
        while (true)
        {
            LineStep(line);

            if (line.fromX + line.x == line.toX)
            {
                outcome = -1;
                break;
            }

            if (++outcome >= stepsPerFrame * unit.throwSteps)
            {
                break;
            }
        }
    }

    services.audio.PlaySound(act.view, services.config.soundVolume);
    act.view = 0;

    if (unit.ghost == 0)
    {
        unit.placement.x = static_cast<i16>(line.fromX + line.x);
        unit.placement.y = static_cast<i16>(line.fromY + line.y);
    }

    const bool invisible = (unit.status & BattleUnit::Invisible) != 0;
    view.picture = {act.frame, act.picture, true};
    view.who = &unit;

    if (outcome == -1)
    {
        view.x = line.toX - static_cast<i16>(unit.actAnchorX);
        view.y = line.toY - static_cast<i16>(act.anchorY);
        view.place = {line.toX, line.toY};
        view.flags = invisible ? unit.frameFlags | Painter::SmearUnder : unit.frameFlags;
        unit.actAnchorX = 0;
        unit.throwSteps = 0;
        unit.stage = 0;
        unit.placement = unit.savedPlacement;
        line = LineWalker{};
        pose = ActState{};
        act = ActState{};
        unit.unk2BD4 &= ~kWalksHome;
        return 1;
    }

    const int pointX = line.fromX + line.x;
    const int pointY = line.fromY + line.y;
    view.x = pointX;
    view.y = pointY;
    this->ChooseOverlay(
        services,
        act.rotation,
        act.actionCode,
        static_cast<i16>(act.offsetX),
        static_cast<i16>(act.offsetY),
        view
    );
    view.x = pointX - static_cast<i16>(unit.actAnchorX);
    view.y = pointY - static_cast<i16>(act.anchorY);
    view.place = {pointX, pointY};

    if (invisible)
    {
        view.flags = unit.frameFlags | Painter::SmearUnder;
        return 0;
    }

    view.flags = unit.frameFlags;
    view.shadow = true;
    view.shadowY = pointY;
    view.shadowTint = unit.autoControl == 1 ? kInvisibleShadowTint : kShadowTint;
    unit.shadowMode = (unit.frameFlags & (Painter::RleSource | Painter::Mirror | Painter::Flip)) | Painter::Shadow;
    return 0;
}

// What WalkToTarget decides of the frame: the acts stepped, the line walked, the sound, the arrival; the view keeps
// no picture when it draws nothing.
int UnitFrame::WalkAdvance(BattleServices& services, const BattleUnit& target, WalkView& view)
{
    BattleUnit& unit = this->unit;

    if (unit.unk3018 == 1)
    {
        return 0;
    }

    if (unit.MarkBit4() == 1 || (unit.autoControl == 1 && (unit.command.flags & UnitCommand::FadesIn) != 0))
    {
        unit.commandA = unit.commandB;
        return 1;
    }

    if ((unit.status & BattleUnit::Invisible) != 0 && unit.FadeInvisible(services, 0) == 1)
    {
        return 1;
    }

    if ((unit.unk2BC0 & kWalkBlocked) != 0)
    {
        return 0;
    }

    unit.unk2FEC = 1;
    ActState& act = unit.attackAct;
    ActState& pose = unit.recoilAct;
    const bool vanishing = (unit.unk2BC0 & kVanishWalk) != 0;
    act.id = unit.actId;
    act.action = vanishing ? kVanishAction : kRoleWalkAction;

    if (unit.kind == 0)
    {
        act.action = kEnemyHomeAction;
    }

    act.hold = unit.held == 1 ? 1 : 0;

    if (services.acts.Run(act) == 0)
    {
        return 0;
    }

    unit.picture = services.archive.GetPicture(act.frame, act.picture);
    u16 targetX = 0;
    u16 targetY = 0;
    target.GetDrawPosition(targetX, targetY);

    if (unit.HasFixedOrigin() == 1)
    {
        targetX = kFixedOriginX;
        targetY = kFixedOriginY;
    }

    pose.id = unit.actId;
    pose.action = kHomePose;

    if (unit.kind == 0)
    {
        if (unit.unk2FB6 == 1)
        {
            pose.action = kHomePose1;
        }

        if (unit.unk2FB6 == kSecondHomePose)
        {
            pose.action = kHomePose2;
        }
    }

    if (services.acts.Run(pose) == 0)
    {
        return 0;
    }

    // A missing picture is read through a null pointer in the original.
    if (unit.picture == nullptr)
    {
        return 0;
    }

    const Picture& picture = *unit.picture;
    unit.actOffsetX = static_cast<u16>(act.x);
    unit.frameFlags = act.flags;
    unit.actAnchorX = act.anchorX;
    unit.homePoseX = pose.x;

    if (unit.facing == 1)
    {
        unit.frameFlags ^= Painter::Mirror;
        unit.actOffsetX = static_cast<u16>(picture.width - static_cast<u16>(act.x));
        unit.homePoseX = picture.width - pose.x;

        if (act.anchorX != 0)
        {
            unit.actAnchorX = static_cast<u16>(picture.width - act.anchorX);
        }
    }

    const auto toX = static_cast<i16>(targetX);
    const auto toY = static_cast<i16>(targetY);
    view.picture = {act.frame, act.picture, true};
    view.who = &unit;

    // swd3e: unk2BC0 bit 25 shows the act at the target until it ends, then takes the command.
    if (vanishing)
    {
        view.x = toX - static_cast<i16>(unit.actOffsetX);
        view.y = toY - act.y;
        view.place = {toX, toY};
        view.flags = unit.frameFlags;

        if (act.waiting != 1)
        {
            return 0;
        }

        unit.unk2BC0 &= ~kVanishWalk;
        unit.flight = LineWalker{};
        pose = ActState{};
        act = ActState{};
        unit.commandA = unit.commandB;
        return 1;
    }

    if (unit.unk2A8E != act.picture && unit.kind == 1)
    {
        unit.unk2A8E = act.picture;
    }

    ++unit.throwSteps;
    LineWalker& line = unit.flight;
    line = LineWalker{};
    line.fromX = unit.savedPlacement.x;
    line.fromY = unit.savedPlacement.y;

    if (unit.ghost == 1)
    {
        line.fromX = unit.placement.x;
        line.fromY = unit.placement.y;
    }

    line.toX = static_cast<i16>(unit.actAnchorX) - static_cast<i16>(unit.actOffsetX) + toX;
    line.toY = toY + static_cast<i16>(act.anchorY) - act.y;
    int stepsPerFrame = unit.ghost == 1 ? kGhostWalkSteps : kWalkSteps;

    if ((unit.command.flags & UnitCommand::UnkFlag20) != 0)
    {
        stepsPerFrame = 0;

        if ((act.target & ActState::FollowsUp) != 0)
        {
            stepsPerFrame = std::abs(line.toX - line.fromX) / kWalkShare;
        }
        else
        {
            --unit.throwSteps;
        }
    }

    int outcome = 0;

    if (stepsPerFrame * unit.throwSteps > 0)
    {
        while (true)
        {
            LineStep(line);

            if (line.x + line.fromX == line.toX)
            {
                outcome = -1;
                break;
            }

            if (++outcome >= stepsPerFrame * unit.throwSteps)
            {
                break;
            }
        }
    }

    services.audio.PlaySound(act.view, services.config.soundVolume);
    act.view = 0;

    if (unit.ghost == 0)
    {
        unit.placement.x = static_cast<i16>(line.fromX + line.x);
        unit.placement.y = static_cast<i16>(line.fromY + line.y);
    }

    const bool invisible = (unit.status & BattleUnit::Invisible) != 0;

    if (outcome == -1)
    {
        view.x = line.toX - static_cast<i16>(unit.actAnchorX);
        view.y = line.toY - static_cast<i16>(act.anchorY);
        view.place = {line.toX, line.toY};
        view.flags = invisible ? unit.frameFlags | Painter::SmearUnder : unit.frameFlags;
        unit.actAnchorX = 0;
        unit.throwSteps = 0;
        unit.unk2A8E = 0;
        unit.unk2EDE = 0;
        unit.unk2EDC = kArrivedMark;
        line = LineWalker{};
        pose = ActState{};
        act = ActState{};

        if (unit.actId == kVanishingAct)
        {
            unit.unk2BC0 |= kVanishWalk;
        }
        else
        {
            unit.commandA = unit.commandB;
        }

        return 1;
    }

    const int pointX = line.x + line.fromX;
    const int pointY = line.fromY + line.y;
    view.x = pointX - static_cast<i16>(unit.actAnchorX);
    view.y = pointY - static_cast<i16>(act.anchorY);
    view.place = {pointX, pointY};

    if (invisible)
    {
        view.flags = unit.frameFlags | Painter::SmearUnder;
        return 0;
    }

    view.flags = unit.frameFlags;
    view.shadow = true;
    view.shadowY = pointY;
    view.shadowTint = unit.autoControl == 1 ? kInvisibleShadowTint : kShadowTint;
    unit.shadowMode = (unit.frameFlags & (Painter::RleSource | Painter::Mirror | Painter::Flip)) | Painter::Shadow;
    return 0;
}

// A walk as WalkAdvance or HomeAdvance decided it: the shadow under the picture, the picture, the overlay.
void UnitFrame::DrawWalk(BattleServices& services, const WalkView& view)
{
    const DrawPlacer::Holding holding(view.place[0], view.place[1], view.who);
    Painter& painter = services.painter;
    const Picture* walking = view.picture.Get(services.archive);

    if (walking == nullptr)
    {
        return;
    }

    const Picture& picture = *walking;
    painter.SetSource(picture.data.data());

    if (view.shadow)
    {
        const int shadowHeight = picture.height / kShadowShare;
        DrawOptions& options = painter.Options();
        options.scaleHeight = shadowHeight;
        options.shear = picture.height >> kShadowShearShift;
        options.tintRed = view.shadowTint;
        options.tintGreen = view.shadowTint;
        options.tintBlue = view.shadowTint;
        painter.DrawPicture(
            view.x,
            view.shadowY - shadowHeight,
            picture.width,
            picture.height,
            (view.flags & (Painter::RleSource | Painter::Mirror | Painter::Flip)) | Painter::Shadow
        );
    }

    painter.DrawPicture(view.x, view.y, picture.width, picture.height, view.flags);

    if (const Picture* overlay = view.overlay.Get(services.archive); overlay != nullptr)
    {
        painter.SetSource(overlay->data.data());
        painter.DrawPicture(view.overlayX, view.overlayY, overlay->width, overlay->height, view.overlayFlags);
    }
}

// What DrawDamageNumbers decides of the frame: the delays counted down, the hops stepped, the extra number taken, the
// fades risen and the slots cleared; the draws in their order into the view.
int UnitFrame::NumbersAdvance(BattleServices& services, NumbersView& view)
{
    BattleUnit& unit = this->unit;

    if (unit.unk3018 == 0)
    {
        return 0;
    }

    auto clearMode = [&view]
    {
        view.steps.push_back(NumberStep{NumberStep::Kind::ClearMode});
    };

    auto fadeStep = [&view](int fade)
    {
        view.steps.push_back(NumberStep{.kind = NumberStep::Kind::Fade, .fade = fade});
    };

    auto digitStep = [&view](i16 font, int value, int x, int y)
    {
        view.steps.push_back(NumberStep{.kind = NumberStep::Kind::Digit, .font = font, .value = value, .x = x, .y = y});
    };

    const int x = unit.savedPlacement.x;  // swd3e: the saved place (swd32: the place)
    const int y = unit.savedPlacement.y - kNumberAbove;

    // A target count stepped below zero (0x00490FF0) runs the original's slots past the tables; here they end with
    // the tables.
    auto slotEnd = [&unit]
    {
        return std::min<u16>(unit.targetCount, BattleUnit::MaxTargets);
    };

    for (u16 slot = unit.damageSlot; slot < slotEnd(); ++slot)
    {
        std::array<i16, BattleUnit::DamageDigits>& digits = unit.damageDigits[slot];
        std::array<i16, BattleUnit::DamageDigits>& hops = unit.unk283C[slot];
        i16& fade = unit.numberFades[slot];
        const auto font = static_cast<i16>(unit.pushedTargets[slot]);

        if (unit.damageDelays[slot] != 0)
        {
            --unit.damageDelays[slot];
            continue;
        }

        const bool landed = std::all_of(
            hops.begin(),
            hops.end(),
            [](i16 hop)
            {
                return hop == kLastHop;
            }
        );

        if (!landed)
        {
            clearMode();

            auto hopY = [y, &hops](std::size_t digit)
            {
                return y - BattleUnit::DigitHops[static_cast<std::size_t>(hops[digit])];
            };

            auto drawHop = [&](std::size_t digit, int at)
            {
                digitStep(font, digits[digit], at, hopY(digit));
            };

            if (digits[0] != 0)
            {
                drawHop(0, x - kDigitWidth);
            }

            if ((digits[1] != 0 || digits[0] != 0) && hops[0] > 0)
            {
                drawHop(1, x);
            }

            if (hops[1] > 0)
            {
                if (digits[0] != 0 || digits[1] != 0 || digits[2] != 0)
                {
                    drawHop(2, x + kDigitWidth);
                }

                drawHop(3, x + 2 * kDigitWidth);
            }

            if (unit.held == 0)
            {
                for (std::size_t digit = 0; digit < hops.size(); ++digit)
                {
                    if (digit > 0 && hops[digit - 1] <= 0)
                    {
                        break;
                    }

                    hops[digit] = static_cast<i16>(std::min<int>(hops[digit] + 1, kLastHop));
                }
            }

            continue;
        }

        // The extra number takes the next slot (its fourth digit as it was; the original writes past the slots when
        // they are all used, here it is dropped).
        if (unit.extraNumber == 1)
        {
            unit.extraNumber = 0;

            if (unit.targetCount < BattleUnit::MaxTargets)
            {
                std::array<i16, BattleUnit::DamageDigits>& extra = unit.damageDigits[unit.targetCount];
                extra[0] = static_cast<i16>(services.state.unk575FC4);
                extra[1] = static_cast<i16>(services.state.unk575FC8);
                extra[2] = static_cast<i16>(services.state.unk575FCC);
                ++unit.targetCount;
            }
        }

        if (fade >= kNumberFadeEnd)
        {
            hops.fill(0);
            fade = 0;
            clearMode();
            ++unit.damageSlot;
            digits.fill(0);
            continue;
        }

        if (unit.held == 0)
        {
            fade = static_cast<i16>(fade + kNumberFadeStep);
        }

        const int risenY = y - fade;
        fadeStep(fade);

        if (digits[0] != 0)
        {
            digitStep(font, digits[0], x - kDigitWidth, risenY);
        }

        if (digits[0] != 0 || digits[1] != 0)
        {
            fadeStep(fade);
            digitStep(font, digits[1], x, risenY);
        }

        fadeStep(fade);

        if (digits[0] != 0 || digits[1] != 0 || digits[2] != 0)
        {
            digitStep(font, digits[2], x + kDigitWidth, risenY);
        }

        digitStep(font, digits[3], x + 2 * kDigitWidth, risenY);
        clearMode();
    }

    if (unit.damageSlot < slotEnd())
    {
        return 0;
    }

    clearMode();
    unit.targetCount = 0;
    unit.damageSlot = 0;
    unit.unk3018 = 0;
    return 1;
}

// What DrawDamageNumbers draws, as NumbersAdvance decided it.
void UnitFrame::DrawNumbers(BattleServices& services, const NumbersView& view)
{
    BattleHud& hud = services.hud;
    DrawOptions& options = services.painter.Options();

    for (const NumberStep& step : view.steps)
    {
        switch (step.kind)
        {
            case NumberStep::Kind::ClearMode:
                hud.digitMode = 0;
                break;

            case NumberStep::Kind::Fade:
                options.tintRed = -step.fade;
                options.tintGreen = -step.fade;
                options.tintBlue = -step.fade;
                options.scaleHeight = step.fade + kNumberScaleBase;
                break;

            case NumberStep::Kind::Digit:
            {
                hud.DrawNumber(step.font, step.value, step.x, step.y);
                break;
            }
        }
    }
}

// What DrawGuardText decides of the frame: the letters popped in or risen, the mark cleared at the top.
void UnitFrame::GuardAdvance(GuardView& view)
{
    BattleUnit& unit = this->unit;

    if ((unit.marks & 1) == 0)
    {
        return;
    }

    const int top = unit.placement.y - kGuardTextOffset;
    view.left = unit.placement.x - kGuardTextOffset;
    auto& phases = unit.guardPhases;

    if (std::all_of(
            phases.begin(),
            phases.end(),
            [](i16 phase)
            {
                return phase == kGuardPhaseEnd;
            }
        ))
    {
        const i16 rise = unit.guardRise;

        if (rise >= kGuardTextRise)
        {
            phases = {};
            unit.guardRise = 0;
            view.clearMode = true;
            unit.marks = static_cast<u16>(unit.marks & ~BattleUnit::UnkMark1);
            unit.action = 0;
            unit.unk26A8 = 0;
            unit.unk26AC = 0;
            return;
        }

        if (unit.held == 0)
        {
            unit.guardRise = static_cast<i16>(rise + kGuardTextStep);
        }

        view.fading = true;
        view.fade = unit.guardRise;
        view.letters = static_cast<int>(phases.size());

        for (std::size_t letter = 0; letter < phases.size(); ++letter)
        {
            view.y[letter] = top - unit.guardRise;
        }

        return;
    }

    view.clearMode = true;
    view.letters = 1;
    view.y[0] = top - BattleUnit::PopOffsets[static_cast<std::size_t>(phases[0])];

    for (std::size_t letter = 1; letter < phases.size() && phases[0] > 0; ++letter)
    {
        view.letters = static_cast<int>(letter) + 1;
        view.y[letter] = top - BattleUnit::PopOffsets[static_cast<std::size_t>(phases[letter])];

        if (unit.held == 0)
        {
            ++phases[letter];
        }

        if (phases[letter] > kGuardPhaseEnd)
        {
            phases[letter] = kGuardPhaseEnd;
        }
    }

    if (++phases[0] > kGuardPhaseEnd)
    {
        phases[0] = kGuardPhaseEnd;
    }
}

// What DrawGuardText draws, as GuardAdvance decided it: the letters at fixed places from `left` (swd3e; swd32 placed
// each after the width of the ones before it).
void UnitFrame::DrawGuard(BattleServices& services, const GuardView& view)
{
    BattleHud& hud = services.hud;

    if (view.clearMode)
    {
        hud.digitMode = 0;
    }

    for (int letter = 0; letter < view.letters; ++letter)
    {
        const auto at = static_cast<std::size_t>(letter);

        if (view.fading)
        {
            DrawOptions& options = services.painter.Options();
            options.tintRed = -view.fade;
            options.tintGreen = -view.fade;
            options.tintBlue = -view.fade;
            options.scaleHeight = view.fade + kGuardTextHeight;
        }

        hud.DrawDigit(0, letter, view.left + kGuardLetterX[at], view.y[at]);
    }
}

// What DrawAction decides of the frame: the act stepped, its place on the target or the unit, the target nudged, its
// sound, the afterimages, the overlay chosen and cleared once the act waits; 0 when the act cannot run.
int UnitFrame::ActionAdvance(BattleServices& services, BattleUnit& target, ActState& act, ActionView& view)
{
    BattleUnit& unit = this->unit;

    if (services.acts.Run(act) == 0)
    {
        return 0;
    }

    unit.picture = services.archive.GetPicture(act.frame, act.picture);

    // A missing picture is read through a null pointer in the original.
    if (unit.picture == nullptr)
    {
        return 0;
    }

    const Picture& picture = *unit.picture;
    unit.frameFlags = act.flags;
    unit.actOffsetX = static_cast<u16>(act.x);
    unit.actAnchorX = act.anchorX;
    unit.overlayX = act.offsetX;
    const bool forced = unit.autoControl == 1 || unit.guestControl == 1;
    const bool still = unit.autoControl == 0 && unit.guestControl == 0;

    auto mirror = [&unit, &act, &picture]
    {
        unit.frameFlags ^= Painter::Mirror;
        unit.actOffsetX = static_cast<u16>(picture.width - static_cast<u16>(act.x));

        if (act.anchorX != 0)
        {
            unit.actAnchorX = static_cast<u16>(picture.width - act.anchorX);
        }
    };

    if (forced)
    {
        if (unit.facing == 1)
        {
            mirror();
        }

        if (unit.autoControl == 1 && (unit.frameFlags & ActState::NoBlend) == 0)
        {
            unit.frameFlags |= Painter::Additive;
        }
    }

    if (unit.facing == 1 && still)
    {
        mirror();
    }

    // swd3e: target bit 0x1000 nudges the target once.
    if ((act.target & kNudgesTarget) != 0)
    {
        target.SetNudge(kActNudge, 0);
        act.target = static_cast<u16>(act.target & ~kNudgesTarget);
    }

    u16 targetX = 0;
    u16 targetY = 0;
    target.GetDrawPosition(targetX, targetY);
    const u32 mirrored = unit.frameFlags & (Painter::RleSource | Painter::Mirror | Painter::Flip);

    if ((unit.command.flags & UnitCommand::AtTarget) != 0)
    {
        if (unit.HasFixedOrigin() == 1)
        {
            targetX = unit.kind == 0 ? kEnemyFixedX : kFixedOriginX;
            targetY = unit.kind == 0 ? kEnemyFixedY : kFixedOriginY;
        }

        unit.actX = static_cast<u16>(targetX - unit.actOffsetX);
        unit.actY = static_cast<u16>(targetY - static_cast<u16>(act.y));
    }
    else
    {
        unit.actX = static_cast<u16>(unit.placement.x - unit.actOffsetX);
        unit.actY = static_cast<u16>(unit.placement.y - static_cast<u16>(act.y));

        // The outline is drawn at once, before the afterimages (as in the original). The original sets no source
        // first and reads the last drawn picture's pixels by this picture's size; this draws its own pixels.
        if (unit.outline == 1)
        {
            unit.colorPair = BattleUnit::OutlineColorPair;
            services.painter.SetSource(picture.data.data());
            services.painter.DrawOutline(
                static_cast<i16>(unit.actX),
                static_cast<i16>(unit.actY),
                picture.width,
                picture.height,
                unit.frameFlags & (Painter::RleSource | Painter::Mirror | Painter::Flip),
                DrawArgument{.colorPair = unit.colorPair}
            );
            unit.colorPair = BattleUnit::OutlineColorPair;
        }
    }

    services.audio.PlaySound(act.view, services.config.soundVolume);
    const int centre = static_cast<i16>(unit.actX) + static_cast<i16>(unit.actOffsetX);
    services.audio.SetSoundPan(act.view, centre >= kPanCentre ? kFadePan : -kFadePan);
    act.view = 0;

    // swd3e: an act that follows up with target bit 0x100 leaves afterimages (unk2BC0 bit 18; not for an auto unit).
    if ((act.target & ActState::FollowsUp) != 0 && (act.target & kAfterImageTarget) != 0)
    {
        if (unit.autoControl == 0)
        {
            unit.unk2BC0 |= kAfterImages;
        }

        act.target = 0;
    }

    if ((unit.unk2BC0 & kAfterImages) != 0)
    {
        if (act.waiting == 0)
        {
            unit.images.Add(
                ImageList::Image{
                    .x = static_cast<i16>(unit.actX),
                    .y = static_cast<i16>(unit.actY),
                    .mode = unit.frameFlags,
                    .frame = act.frame,
                    .picture = act.picture,
                    .unkB8 = static_cast<u16>(act.counter >> kRoundShift),
                    .flags = ImageList::Image::Once,
                    .tintBlue = kAfterImageTintBlue,
                    .level = kAfterImageLevel,
                    .step = kAfterImageFade,
                }
            );
        }

        unit.images.Draw(services);
    }

    view.picture = {act.frame, act.picture, true};
    view.x = static_cast<i16>(unit.actX);
    view.y = static_cast<i16>(unit.actY);
    view.mirrored = mirrored;
    view.flags = unit.frameFlags;
    view.shadowY = act.y + static_cast<i16>(unit.actY);
    view.shadowTint = (unit.status & BattleUnit::Invisible) != 0 ? kInvisibleShadowTint : kShadowTint;
    view.signal = (unit.frameFlags & ActState::Signal) != 0;
    view.alpha = act.signal;
    view.wave = unit.mainAct.wait;
    unit.shadowMode = mirrored | Painter::Shadow;

    if (act.rotation == 0 || act.actionCode == 0)
    {
        return 1;
    }

    unit.overlayPicture = services.archive.GetPicture(act.rotation, static_cast<u16>(act.actionCode - 1));

    // A missing picture is read through a null pointer in the original.
    if (unit.overlayPicture == nullptr)
    {
        return 1;
    }

    if (forced && unit.facing == 1 && act.offsetX != 0)
    {
        unit.overlayX = static_cast<u16>(picture.width - act.offsetX - unit.overlayPicture->width);
    }

    if (unit.facing == 1 && still && act.offsetX != 0)
    {
        unit.overlayX = static_cast<u16>(picture.width - act.offsetX - unit.overlayPicture->width);
    }

    view.overlay = {act.rotation, static_cast<u16>(act.actionCode - 1), true};
    view.overlayX = static_cast<i16>(unit.actX) + static_cast<i16>(unit.overlayX);
    view.overlayY = static_cast<i16>(unit.actY) + static_cast<i16>(act.offsetY);
    view.overlayFlags = unit.frameFlags | Painter::Additive;

    if (act.waiting == 1)
    {
        act.rotation = 0;
        act.actionCode = 0;
        act.offsetX = 0;
        act.offsetY = 0;
    }

    return 1;
}

// What DrawAction draws, as ActionAdvance decided it: the outline, the shadow, the act (smeared when invisible), the
// overlay.
void UnitFrame::DrawActionView(BattleServices& services, const ActionView& view)
{
    Painter& painter = services.painter;
    const Picture* drawn = view.picture.Get(services.archive);

    if (drawn == nullptr)
    {
        return;
    }

    const Picture& picture = *drawn;
    painter.SetSource(picture.data.data());

    const int shadowHeight = picture.height / kShadowShare;
    DrawOptions& options = painter.Options();
    options.scaleHeight = shadowHeight;
    options.shear = picture.height >> kShadowShearShift;
    options.tintRed = view.shadowTint;
    options.tintGreen = view.shadowTint;
    options.tintBlue = view.shadowTint;
    painter.SetSource(picture.data.data());
    painter.DrawPicture(
        view.x,
        view.shadowY - shadowHeight,
        picture.width,
        picture.height,
        view.mirrored | Painter::Shadow
    );
    painter.SetSource(picture.data.data());

    if (view.signal)
    {
        painter.Options().alpha = view.alpha;
    }

    if (view.wave != 0)
    {
        painter.SetWave(view.wave);
    }

    painter.DrawPicture(view.x, view.y, picture.width, picture.height, view.flags);
    painter.SetWave(0);

    if (const Picture* overlay = view.overlay.Get(services.archive); overlay != nullptr)
    {
        painter.SetSource(overlay->data.data());
        painter.DrawPicture(view.overlayX, view.overlayY, overlay->width, overlay->height, view.overlayFlags);
    }
}

// swd3e.exe 0x00497350: the main act's effect while the unit idles: when the main act recoils (Recoils), its effectA
// plays in the cast act (holding the main act with Holds; the command's follow-up act waits for it) above the main
// act's anchor, the sound panned to the unit's side; when it waits, the effect and the holds are cleared. The rebuild's
// smooth battle: the decisions here, the draw into `steps`.
void UnitFrame::IdleEffectAdvance(BattleServices& services, PictureSteps& steps)
{
    BattleUnit& unit = this->unit;
    ActState& main = unit.mainAct;
    ActState& effect = unit.castAct;
    const u16 bits = main.target;

    if ((bits & ActState::Recoils) != 0)
    {
        if (main.effectA != 0)
        {
            unit.stage |= BattleUnit::Recoiling;
        }

        if (unit.command.followAct != 0)
        {
            unit.stage |= BattleUnit::FollowingUp;
        }

        if ((bits & ActState::Holds) != 0)
        {
            main.hold = 1;
        }

        main.target = static_cast<u16>(bits & ~ActState::Recoils);
        effect = ActState{};
    }

    if ((unit.stage & BattleUnit::Recoiling) == 0)
    {
        return;
    }

    // swd3e: without an effect the stage ends at once.
    if (main.effectA == 0)
    {
        unit.stage &= ~BattleUnit::Recoiling;
        effect = ActState{};
        return;
    }

    effect.action = main.effectQ;
    effect.id = main.effectA;

    if (services.acts.Run(effect) == 0)
    {
        return;
    }

    services.audio.PlaySound(effect.view, services.config.soundVolume);

    if (effect.view != 0)
    {
        services.audio.SetSoundPan(effect.view, unit.placement.x >= kPanCentre ? kFadePan : -kFadePan);
    }

    effect.view = 0;
    unit.picture = services.archive.GetPicture(effect.frame, effect.picture);

    // A missing picture is read through a null pointer in the original.
    if (unit.picture == nullptr)
    {
        return;
    }

    u32 flags = effect.flags;
    int offsetX = effect.x;

    if (unit.facing == 1)
    {
        flags ^= Painter::Mirror;

        if (offsetX != 0)
        {
            offsetX = unit.picture->width - offsetX;
        }
    }

    steps.push_back(
        PictureStep{
            .picture = {effect.frame, effect.picture, true},
            .x = static_cast<i16>(unit.actAnchorX) + unit.placement.x - static_cast<i16>(unit.originX) - offsetX,
            .y = static_cast<i16>(main.anchorY) + unit.placement.y - effect.y - main.y,
            .mode = flags,
        }
    );

    if (effect.waiting != 1)
    {
        return;
    }

    main.target = static_cast<u16>(main.target & ~ActState::Holds);
    unit.stage &= ~BattleUnit::Recoiling;
    unit.slotTint = 0;
    unit.actAnchorX = 0;
    unit.unk2ACC = 0;
    main.hold = 0;
    main.effectA = 0;
    effect = ActState{};
}

// swd3e.exe 0x004975E0: the command's follow-up act (in the attack act) while the unit idles and FollowingUp is set,
// at the unit's place, the sound panned to its side; when it waits, every stage and the main act's effect are cleared.
// The rebuild's smooth battle: the decisions here, the draw into `steps`.
void UnitFrame::FollowUpAdvance(BattleServices& services, PictureSteps& steps)
{
    BattleUnit& unit = this->unit;

    if ((unit.stage & BattleUnit::FollowingUp) == 0)
    {
        return;
    }

    ActState& act = unit.attackAct;
    act.id = unit.command.followAct;
    act.action = 0;

    if (services.acts.Run(act) == 0)
    {
        return;
    }

    services.audio.PlaySound(act.view, services.config.soundVolume);

    if (act.view != 0)
    {
        services.audio.SetSoundPan(act.view, unit.placement.x >= kPanCentre ? kFadePan : -kFadePan);
    }

    act.view = 0;
    unit.picture = services.archive.GetPicture(act.frame, act.picture);

    // A missing picture is read through a null pointer in the original.
    if (unit.picture == nullptr)
    {
        return;
    }

    u32 flags = act.flags;
    int offsetX = act.x;

    if (unit.facing == 1)
    {
        flags ^= Painter::Mirror;
        offsetX = unit.picture->width - offsetX;
    }

    steps.push_back(
        PictureStep{
            .picture = {act.frame, act.picture, true},
            .x = unit.placement.x - offsetX,
            .y = unit.placement.y - act.y,
            .mode = flags,
        }
    );

    if (act.waiting != 1)
    {
        return;
    }

    unit.stage = 0;
    unit.slotTint = 0;
    unit.mainAct.target = 0;
    unit.actAnchorX = 0;
    unit.unk2ACC = 0;
    unit.mainAct.hold = 0;
    unit.mainAct.effectA = 0;
    act = ActState{};
}

// What DrawRecoil decides of the frame: the recoil act stepped with its sound, its place, the fade once it waits; its
// result (1 once the recoil is over), the draw into `steps`.
int UnitFrame::RecoilAdvance(BattleServices& services, ActState& act, i16 y, PictureSteps& steps)
{
    BattleUnit& unit = this->unit;

    if (services.acts.Run(act) == 0)
    {
        return 0;
    }

    unit.picture = services.archive.GetPicture(act.frame, act.picture);
    services.audio.PlaySound(act.view, services.config.soundVolume);
    services.audio.SetSoundPan(act.view, static_cast<i16>(unit.actX) >= kPanCentre ? kFadePan : -kFadePan);
    act.view = 0;

    // A missing picture is read through a null pointer in the original.
    if (unit.picture == nullptr)
    {
        return 0;
    }

    const Picture& picture = *unit.picture;
    unit.shadowMode = act.flags;
    unit.aimOffsetX = static_cast<u16>(act.x);

    auto mirror = [&unit, &act, &picture]
    {
        unit.shadowMode ^= Painter::Mirror;

        if (act.x != 0)
        {
            unit.aimOffsetX = static_cast<u16>(picture.width - static_cast<u16>(act.x));
        }
    };

    if ((unit.autoControl == 1 || unit.guestControl == 1) && unit.facing == 1)
    {
        mirror();
    }

    if (unit.facing == 1 && unit.autoControl == 0 && unit.guestControl == 0)
    {
        mirror();
    }

    services.painter.SetSource(picture.data.data());
    PictureStep step{
        .picture = {act.frame, act.picture, true},
        .x = static_cast<i16>(unit.actAnchorX) - static_cast<i16>(unit.aimOffsetX) + static_cast<i16>(unit.actX),
        .y = static_cast<i16>(unit.actY) - act.y + y,
        .mode = unit.shadowMode,
    };

    if (act.waiting != 1)
    {
        step.blended = (unit.shadowMode & ActState::Signal) != 0;
        step.alpha = act.signal;
        steps.push_back(step);
        return 0;
    }

    if (unit.recoilTint <= kRecoilFadeEnd)
    {
        unit.recoilTint = 0;
        return 1;
    }

    step.tinted = true;
    step.tintRed = unit.recoilTint;
    step.tintGreen = unit.recoilTint;
    step.tintBlue = unit.recoilTint;
    steps.push_back(step);
    unit.recoilTint = static_cast<i16>(unit.recoilTint - kRecoilFadeStep);

    if (unit.held == 1)
    {
        unit.recoilTint = static_cast<i16>(unit.recoilTint + kRecoilFadeStep);
    }

    return 0;
}

// What DrawTrail decides of the frame: every third frame the images move back and the current picture comes in front,
// each image fades; the draws into `steps`.
void UnitFrame::TrailAdvance(
    BattleServices& services,
    AfterimageTrail& trail,
    u16 frame,
    u16 picture,
    u32 mode,
    i16 x,
    i16 y,
    PictureSteps& steps
)
{
    BattleUnit& unit = this->unit;
    int spacing = trail.direction == AfterimageTrail::Left ? -kTrailSpacing : kTrailSpacing;

    if (trail.delay == 0)
    {
        for (int place = trail.count - 1; place > 0; --place)
        {
            trail.images[static_cast<std::size_t>(place)] = trail.images[static_cast<std::size_t>(place - 1)];
        }

        AfterimageTrail::Image& front = trail.images[0];
        front.level = kTrailLevel;
        front.picture = picture;

        if ((mode & Painter::Additive) != 0)
        {
            front.level = kAdditiveTrailLevel;
        }

        front.mode = mode;
        front.x = x;
        front.y = y;
        trail.delay = kTrailDelay;
    }
    else
    {
        --trail.delay;
    }

    for (int place = 0; place < trail.count; ++place)
    {
        AfterimageTrail::Image& image = trail.images[static_cast<std::size_t>(place)];

        if (image.picture == AfterimageTrail::NoPicture)
        {
            continue;
        }

        int offset = kTrailCentre - image.x;

        if (trail.direction == AfterimageTrail::Left)
        {
            offset = image.y - kTrailCentre;
        }

        if (trail.direction == AfterimageTrail::InPlace)
        {
            offset = 0;
            spacing = 0;
        }

        unit.unk2550 = services.archive.GetPicture(frame, image.picture);
        const PictureRef shown = {frame, image.picture, true};

        if (image.level > 0)
        {
            image.level = static_cast<i16>(image.level - trail.fadeStep);

            if (image.level <= 0)
            {
                image.picture = AfterimageTrail::NoPicture;
                image.level = 0;
            }
        }

        // A missing picture is read through a null pointer in the original.
        if (unit.unk2550 == nullptr)
        {
            continue;
        }

        const int drawX = image.x + spacing * place + offset;

        if ((mode & Painter::Additive) != 0)
        {
            steps.push_back(
                PictureStep{
                    .picture = shown,
                    .x = drawX,
                    .y = image.y,
                    .mode = mode,
                    .tinted = true,
                    .tintRed = image.level - kTrailTintBase,
                    .tintGreen = image.level - kTrailTintBase,
                    .tintBlue = kTrailBlue,
                }
            );
            continue;
        }

        steps.push_back(
            PictureStep{
                .picture = shown,
                .x = drawX,
                .y = image.y,
                .mode = mode | Painter::Translucent,
                .blended = true,
                .alpha = image.level,
            }
        );

        if (image.level > 1)
        {
            steps.push_back(
                PictureStep{
                    .picture = shown,
                    .x = drawX,
                    .y = image.y,
                    .mode = mode | Painter::TintUnder,
                    .tinted = true,
                    .tintRed = trail.tintRed,
                    .tintGreen = trail.tintGreen,
                    .tintBlue = trail.tintBlue,
                }
            );
        }
    }
}

// What FlyAct decides of the frame: the act stepped with its sound, the line walked; its result (1 once the target's
// x is reached), the draw into `scene`.
int UnitFrame::Fly(
    BattleServices& services,
    const BattleUnit& target,
    ActState& act,
    int slot,
    i32 id,
    int fromX,
    int fromY,
    int steps,
    i16 delay,
    int backwards,
    Scene& scene
)
{
    const Scene::Owner owner(scene, &this->unit);
    BattleUnit& unit = this->unit;
    u16 targetX = 0;
    u16 targetY = 0;
    act.id = id;
    act.action = 0;
    act.hold = 0;

    if (unit.held == 1)
    {
        act.hold = 1;
    }

    if (services.acts.Run(act) == 0)
    {
        return 0;
    }

    unit.picture = services.archive.GetPicture(act.frame, act.picture);

    // A missing picture is read through a null pointer in the original.
    if (unit.picture == nullptr)
    {
        return 0;
    }

    const Picture& picture = *unit.picture;
    const u16 anchorX = act.anchorX;
    u32 flags = act.flags;
    auto offsetX = static_cast<u16>(act.x);
    i32 anchorShift = static_cast<i16>(anchorX);
    const bool forced = unit.autoControl == 1 || unit.guestControl == 1;

    if (forced && unit.facing == 1)
    {
        flags ^= Painter::Mirror;

        if (act.x != 0)
        {
            offsetX = static_cast<u16>(picture.width - static_cast<u16>(act.x));
        }

        if (anchorX != 0)
        {
            anchorShift = picture.width - static_cast<i16>(act.anchorX);
        }
    }

    if (unit.facing == 1 && unit.autoControl == 0 && unit.guestControl == 0)
    {
        flags ^= Painter::Mirror;
        offsetX = static_cast<u16>(picture.width - static_cast<u16>(act.x));

        if (act.anchorX != 0)
        {
            anchorShift = picture.width - static_cast<i16>(anchorX);
        }
    }

    target.GetDrawPosition(targetX, targetY);
    const auto toY = static_cast<i16>(targetY - static_cast<u16>(act.y));
    const auto toX = static_cast<i16>(targetX - offsetX);
    LineWalker& line = unit.flight;
    line = LineWalker{};
    const i32 startX = fromX - anchorShift;
    line.fromX = startX;
    line.fromY = fromY - static_cast<i16>(act.anchorY);
    line.toX = toX;
    line.toY = toY;

    if (backwards == 1)
    {
        line.fromX = toX;
        line.fromY = toY;
        line.toX = startX;
        line.toY = fromY - static_cast<i16>(act.anchorY);
    }

    if (fromY == 0)
    {
        line.fromY = 0;
        line.toY = 0;
    }

    i32& count = unit.flightSteps[static_cast<std::size_t>(slot)];

    // swd3e: the flight waits `delay` frames before its steps grow.
    if (unit.flightDelay != static_cast<u16>(delay))
    {
        ++unit.flightDelay;
    }
    else if (steps == 0 || steps == kSpeeding)
    {
        steps = 1;

        if (count == 0)
        {
            count = 1;
        }

        count = count <= kFlightDoubling ? 2 * count : count + kFlightGrowth;
    }
    else
    {
        ++count;
    }

    int taken = 0;

    if (count * steps > 0)
    {
        while (true)
        {
            LineStep(line);

            if (line.fromX + line.x == line.toX)
            {
                taken = kArrived;
                break;
            }

            ++taken;

            if (taken >= count * steps)
            {
                break;
            }
        }
    }

    services.audio.PlaySound(act.view, services.config.soundVolume);
    act.view = 0;

    if ((unit.command.actionFlags & UnitCommand::ShrinksInFlight) != 0)
    {
        // swd3e: the act shrinks about its origin as its steps grow.
        ScenePainter painter(services, scene);
        painter.SetSource(picture.data.data());
        const int zoom = kZoomUnit - kFlightShrink * count;
        painter.SetZoomSheared(offsetX, act.y, zoom, zoom, 0);
        painter.DrawPicture(
            line.fromX + line.x,
            line.fromY + line.y,
            picture.width,
            picture.height,
            flags | Painter::ZoomCopy
        );
    }
    else
    {
        // The flown act stands at its line's point less the act's anchor: a new picture keeps the point.
        scene.Add(
            PictureStep{
                .picture = {act.frame, act.picture, true},
                .x = line.fromX + line.x,
                .y = line.fromY + line.y,
                .mode = flags,
                .held = true,
                .stand = {line.fromX + line.x + offsetX, line.fromY + line.y + act.y},
                .who = &count,
            }
        );
    }

    if (taken != kArrived)
    {
        return 0;
    }

    unit.unk2EDE = 0;
    unit.flightDelay = 0;
    count = 0;
    line = LineWalker{};
    return 1;
}

// What RunQueuedActions decides of the frame: each queued act stepped, shaken or flown with its trail, its impact
// played, the hits landed and the done nodes freed; its result (1 when there are none), the draws into `steps`.
int UnitFrame::QueuedAdvance(BattleServices& services, BattleUnit& target, i32 action, PictureSteps& steps)
{
    BattleUnit& unit = this->unit;
    BattleState& battle = services.state;
    LineWalker& line = unit.flight;
    QueuedAction* node = unit.actions.get();
    // A shaking node keeps the outcome of the node before it (the original's first is uninitialised).
    int outcome = 0;

    if (node == nullptr)
    {
        unit.queuedActions = 0;
        return 1;
    }

    while (node != nullptr)
    {
        ActState& act = node->act;

        if (action != 0)
        {
            act.action = static_cast<u32>(action);
        }

        if (services.acts.Run(act) == 0)
        {
            return 0;
        }

        unit.picture = services.archive.GetPicture(act.frame, act.picture);

        // A missing picture is read through a null pointer in the original.
        if (unit.picture == nullptr)
        {
            return 0;
        }

        u32 flags = act.flags;
        auto offsetX = static_cast<i16>(act.x);

        if (unit.facing == 1)
        {
            flags ^= Painter::Mirror;
            offsetX = static_cast<i16>(unit.picture->width - static_cast<u16>(act.x));
        }

        line = LineWalker{};
        line.fromX = node->fromX - offsetX;
        line.fromY = node->fromY - act.y;
        line.toX = node->toX - offsetX;
        line.toY = node->toY - act.y;

        if (act.anchorX != 0)
        {
            line.fromX = node->fromX - static_cast<i16>(act.anchorX);
        }

        if (act.anchorY != 0)
        {
            line.fromY = node->fromY - static_cast<i16>(act.anchorY);
        }

        if (node->fromY == 0)
        {
            line.fromY = 0;
        }

        if (node->toY == 0)
        {
            line.toY = 0;
        }

        if (node->shakeFrames != 0)
        {
            --node->shakeFrames;
        }
        else
        {
            int stepsPerFrame = 1;

            if (node->steps > 0)
            {
                ++node->flown;
                stepsPerFrame = node->steps;
            }
            else
            {
                if (node->flown == 0)
                {
                    node->flown = 1;
                }

                node->flown =
                    static_cast<i16>(node->flown > kSpeedingTop ? node->flown + kSpeedingStep : 2 * node->flown);
            }

            outcome = 0;

            if (stepsPerFrame * node->flown > 0)
            {
                while (true)
                {
                    LineStep(line);

                    if (line.fromX + line.x == line.toX && line.fromY + line.y == line.toY)
                    {
                        outcome = -1;
                        node->flags = static_cast<u8>(node->flags | QueuedAction::UnkFlag2);
                        break;
                    }

                    if (++outcome >= stepsPerFrame * node->flown)
                    {
                        break;
                    }
                }
            }
        }

        services.audio.PlaySound(act.view, services.config.soundVolume);
        act.view = 0;
        const int x = line.fromX + line.x;
        const int y = line.fromY + line.y;

        if (node->trailed == 1)
        {
            node->trail.fadeStep = (flags & Painter::Additive) != 0 ? kAdditiveTrailFade : 1;
            const std::size_t trailFrom = steps.size();
            this->TrailAdvance(
                services,
                node->trail,
                act.frame,
                act.picture,
                flags,
                static_cast<i16>(x),
                static_cast<i16>(y),
                steps
            );

            // The afterimages are the node's: each moves by where it is drawn.
            for (std::size_t at = trailFrom; at < steps.size(); ++at)
            {
                steps[at].held = true;
                steps[at].stand = {steps[at].x, steps[at].y};
                steps[at].who = node;
            }
        }

        if ((node->flags & QueuedAction::UnkFlag2) == 0)
        {
            steps.push_back(
                PictureStep{
                    .picture = {act.frame, act.picture, true},
                    .x = x,
                    .y = y,
                    .mode = flags,
                    .held = true,
                    .stand = {x + offsetX, y + act.y},
                    .who = node,
                }
            );
        }

        if (outcome != -1)
        {
            node = node->next.get();
            continue;
        }

        ActState& impact = node->impact;

        if (static_cast<i32>(impact.id) > 0)
        {
            if (services.acts.Run(impact) == 0)
            {
                return 0;
            }

            unit.picture = services.archive.GetPicture(impact.frame, impact.picture);

            if (unit.picture == nullptr)
            {
                return 0;
            }

            u32 impactFlags = impact.flags;

            if (unit.facing == 1)
            {
                impactFlags ^= Painter::Mirror;
            }

            services.audio.PlaySound(impact.view, services.config.soundVolume);
            services.audio.SetSoundPan(impact.view, line.toX >= kPanCentre ? kFadePan : -kFadePan);
            impact.view = 0;
            steps.push_back(
                PictureStep{
                    .picture = {impact.frame, impact.picture, true},
                    .x = x,
                    .y = y,
                    .mode = impactFlags,
                }
            );

            if (impact.waiting == 0)
            {
                node = node->next.get();
                continue;
            }
        }

        node->done = 1;
        ++unit.landedActions;
        line = LineWalker{};

        if ((node->flags & QueuedAction::UnkFlag4) != 0)
        {
            node = node->next.get();
            continue;
        }

        const u16 chance = (unit.status & kEvadeChanceStatus) != 0 ? kStatusEvadeChance : u16{0};

        if (unit.evadeRolled != 0 || target.RollEvade(services, chance) == 0)
        {
            target.RaiseFlag2BC0(battle);
            i16 spirit = 0;
            i16 stamina = 0;
            i32 damage = unit.kind != 0 ? unit.HitDamage(services, target, spirit, stamina)
                                        : unit.EnemyHitDamage(services, target);
            battle.lastDamage = damage;

            if (damage >= kQueuedMaxDamage)
            {
                damage = kQueuedMaxDamage;
                battle.lastDamage = damage;
            }

            battle.hitDamage += damage;
            target.SetDamageDigits(services, damage);
            target.StepTargetCount(services, 1);
        }

        node = node->next.get();
    }

    std::unique_ptr<QueuedAction>* link = &unit.actions;

    while (*link != nullptr)
    {
        if ((*link)->done == 1)
        {
            *link = std::move((*link)->next);
        }
        else
        {
            link = &(*link)->next;
        }
    }

    return 0;
}

int UnitFrame::Action(BattleServices& services, BattleUnit& target, ActState& act, Scene& scene)
{
    const Scene::Owner owner(scene, &this->unit);
    ActionView view;
    const int result = this->ActionAdvance(services, target, act, view);

    if (view.picture.shown)
    {
        scene.Add(
            [view](BattleServices& drawing)
            {
                UnitFrame::DrawActionView(drawing, view);
            }
        );
    }

    return result;
}

int UnitFrame::Recoil(BattleServices& services, ActState& act, i16 y, Scene& scene)
{
    const Scene::Owner owner(scene, &this->unit);
    PictureSteps steps;
    const int result = this->RecoilAdvance(services, act, y, steps);
    scene.Add(std::move(steps));
    return result;
}

void UnitFrame::Trail(
    BattleServices& services,
    AfterimageTrail& trail,
    u16 frame,
    u16 picture,
    u32 mode,
    i16 x,
    i16 y,
    Scene& scene
)
{
    const Scene::Owner owner(scene, &this->unit);
    PictureSteps steps;
    this->TrailAdvance(services, trail, frame, picture, mode, x, y, steps);
    scene.Add(std::move(steps));
}

int UnitFrame::Queued(BattleServices& services, BattleUnit& target, i32 action, Scene& scene)
{
    const Scene::Owner owner(scene, &this->unit);
    PictureSteps steps;
    const int result = this->QueuedAdvance(services, target, action, steps);
    scene.Add(std::move(steps));
    return result;
}

int UnitFrame::Home(BattleServices& services, const BattleUnit& target, i32 id, i32 action, Scene& scene)
{
    const Scene::Owner owner(scene, &this->unit);
    WalkView view;
    const int result = this->HomeAdvance(services, target, id, action, view);

    if (view.picture.shown)
    {
        scene.Add(
            [view](BattleServices& drawing)
            {
                UnitFrame::DrawWalk(drawing, view);
            }
        );
    }

    return result;
}

int UnitFrame::Walk(BattleServices& services, const BattleUnit& target, Scene& scene)
{
    const Scene::Owner owner(scene, &this->unit);
    WalkView view;
    const int result = this->WalkAdvance(services, target, view);

    if (view.picture.shown)
    {
        scene.Add(
            [view](BattleServices& drawing)
            {
                UnitFrame::DrawWalk(drawing, view);
            }
        );
    }

    return result;
}

int UnitFrame::Numbers(BattleServices& services, Scene& scene)
{
    const Scene::Owner owner(scene, &this->unit);
    NumbersView view;
    const int result = this->NumbersAdvance(services, view);
    scene.Add(
        [view = std::move(view)](BattleServices& drawing)
        {
            UnitFrame::DrawNumbers(drawing, view);
        }
    );
    return result;
}

void UnitFrame::Guard(Scene& scene)
{
    const Scene::Owner owner(scene, &this->unit);
    GuardView view;
    this->GuardAdvance(view);
    scene.Add(
        [view](BattleServices& drawing)
        {
            UnitFrame::DrawGuard(drawing, view);
        }
    );
}

// What BattleUnit::DrawActionAt decides of the frame; the draws into `scene`.
void UnitFrame::DrawActionAtAdvance(BattleServices& services, i16 x, i16 y, ActState& act, Scene& scene)
{
    const Scene::Owner owner(scene, &this->unit);
    BattleUnit& unit = this->unit;

    if (services.acts.Run(act) == 0)
    {
        return;
    }

    unit.picture = services.archive.GetPicture(act.frame, act.picture);

    // A missing picture is read through a null pointer in the original.
    if (unit.picture == nullptr)
    {
        return;
    }

    ScenePainter painter(services, scene);
    unit.frameFlags = act.flags;
    unit.actOffsetX = static_cast<u16>(act.x);
    unit.actAnchorX = act.anchorX;

    auto mirror = [&unit, &act]
    {
        unit.frameFlags ^= Painter::Mirror;
        unit.actOffsetX = static_cast<u16>(unit.picture->width - static_cast<u16>(act.x));

        if (act.anchorX != 0)
        {
            unit.actAnchorX = static_cast<u16>(unit.picture->width - act.anchorX);
        }
    };

    if (unit.autoControl == 1 || unit.guestControl == 1)
    {
        if (unit.facing == 1)
        {
            mirror();
        }

        if (unit.autoControl == 1)
        {
            unit.frameFlags |= Painter::Additive;
        }
    }

    if (unit.facing == 1 && unit.autoControl == 0 && unit.guestControl == 0)
    {
        mirror();
    }

    unit.actX = static_cast<u16>(x - unit.actOffsetX);
    unit.actY = static_cast<u16>(y - static_cast<u16>(act.y));
    services.audio.PlaySound(act.view, services.config.soundVolume);
    const int centre = static_cast<i16>(unit.actX) + static_cast<i16>(unit.actOffsetX);
    services.audio.SetSoundPan(act.view, centre >= kPanCentre ? kFadePan : -kFadePan);
    act.view = 0;
    const int shadowHeight = unit.picture->height / kShadowShare;
    DrawOptions& options = painter.Options();
    options.scaleHeight = shadowHeight;
    options.shear = unit.picture->height >> kShadowShearShift;
    const bool invisible = (unit.status & BattleUnit::Invisible) != 0;
    const int tint = invisible ? kInvisibleShadowTint : kShadowTint;
    options.tintRed = tint;
    options.tintGreen = tint;
    options.tintBlue = tint;
    unit.shadowMode = (unit.frameFlags & (Painter::RleSource | Painter::Mirror | Painter::Flip)) | Painter::Shadow;
    painter.SetSource(unit.picture->data.data());
    painter.DrawPicture(
        static_cast<i16>(unit.actX),
        act.y + static_cast<i16>(unit.actY) - shadowHeight,
        unit.picture->width,
        unit.picture->height,
        unit.shadowMode
    );
    painter.SetSource(unit.picture->data.data());

    if (invisible)
    {
        painter.DrawPicture(
            static_cast<i16>(unit.actX),
            static_cast<i16>(unit.actY),
            unit.picture->width,
            unit.picture->height,
            unit.frameFlags | Painter::SmearUnder
        );
        return;
    }

    if ((unit.frameFlags & ActState::Signal) == ActState::Signal)
    {
        painter.Options().alpha = unit.mainAct.signal;
    }

    painter.DrawPicture(
        static_cast<i16>(unit.actX),
        static_cast<i16>(unit.actY),
        unit.picture->width,
        unit.picture->height,
        unit.frameFlags
    );
}

// What BattleUnit::DrawAimed decides of the frame; the draws into `scene`.
int UnitFrame::DrawAimedAdvance(
    BattleServices& services,
    BattleUnit& target,
    ActState& act,
    i16 shiftX,
    i16 shiftY,
    Scene& scene
)
{
    const Scene::Owner owner(scene, &this->unit);
    BattleUnit& unit = this->unit;
    ScenePainter painter(services, scene);
    u16 targetX = 0;
    u16 targetY = 0;
    act.hold = 0;

    if (unit.held == 1)
    {
        act.hold = 1;
    }

    if (services.acts.Run(act) == 0)
    {
        return 0;
    }

    unit.picture = services.archive.GetPicture(act.frame, act.picture);

    // A missing picture is read through a null pointer in the original.
    if (unit.picture == nullptr)
    {
        return 0;
    }

    unit.shadowMode = act.flags;
    unit.aimOffsetX = static_cast<u16>(act.x);
    unit.aimAnchorX = act.anchorX;

    if (unit.facing == 1)
    {
        unit.shadowMode ^= Painter::Mirror;
        unit.aimOffsetX = static_cast<u16>(unit.picture->width - static_cast<u16>(act.x));

        if (act.anchorX != 0)
        {
            unit.aimAnchorX = static_cast<u16>(unit.picture->width - act.anchorX);
        }
    }

    target.GetDrawPosition(targetX, targetY);
    unit.aimX = static_cast<u16>(targetX - unit.aimOffsetX);
    unit.aimY = static_cast<u16>(targetY - static_cast<u16>(act.y));

    if ((act.anchorX != 0 || act.anchorY != 0) && (shiftX != 0 || shiftY != 0))
    {
        unit.aimX = static_cast<u16>(unit.actX + unit.actAnchorX - unit.aimAnchorX);
        unit.aimY = static_cast<u16>(unit.actY - act.anchorY + shiftY);
    }

    unit.ClearMoveAxes(unit.aimX, unit.aimY);
    services.audio.PlaySound(act.view, services.config.soundVolume);
    services.audio.SetSoundPan(act.view, static_cast<i16>(targetX) >= kPanCentre ? kFadePan : -kFadePan);
    act.view = 0;

    if ((act.target & ActState::Quakes) != 0)
    {
        services.state.quaking = 1;
        services.state.quakePhase = 0;
    }

    if ((act.target & ActState::UnkTarget4) != 0)
    {
        act.target = 0;
        target.RaiseFlag2BC0(services.state);
    }

    painter.SetSource(unit.picture->data.data());

    // The original reads the palette of the shadow picture through a null pointer when there is none.
    const DrawArgument argument = unit.shadowPicture != nullptr ? PaletteArgument(*unit.shadowPicture) : DrawArgument{};
    painter.DrawPicture(
        static_cast<i16>(unit.aimX),
        static_cast<i16>(unit.aimY),
        unit.picture->width,
        unit.picture->height,
        unit.shadowMode,
        argument
    );
    return act.waiting == 1 ? 1 : 0;
}

// What BattleUnit::DrawMainFaded decides of the frame; the draws into `scene`.
void UnitFrame::DrawMainFadedAdvance(BattleServices& services, int tint, Scene& scene)
{
    const Scene::Owner owner(scene, &this->unit);
    BattleUnit& unit = this->unit;
    ScenePainter painter(services, scene);
    ActState& act = unit.mainAct;
    act.hold = 0;

    if (unit.held == 1)
    {
        act.hold = 1;
    }

    unit.picture = services.archive.GetPicture(act.frame, act.picture);

    auto tintAll = [&painter, tint]
    {
        DrawOptions& options = painter.Options();
        options.tintRed = tint;
        options.tintGreen = tint;
        options.tintBlue = tint;
    };

    const int x = unit.placement.x - static_cast<i16>(unit.originX);
    const int y = unit.placement.y - act.y;

    // A missing picture is read through a null pointer in the original.
    if (unit.picture != nullptr)
    {
        painter.SetSource(unit.picture->data.data());
        tintAll();
        painter.DrawPicture(x, y, unit.picture->width, unit.picture->height, unit.drawFlags | Painter::Additive);
    }

    if (act.rotation == 0 || act.actionCode == 0)
    {
        return;
    }

    tintAll();
    unit.overlayPicture = services.archive.GetPicture(act.rotation, static_cast<u16>(act.actionCode - 1));

    if (unit.overlayPicture == nullptr)
    {
        return;
    }

    painter.SetSource(unit.overlayPicture->data.data());
    painter.DrawPicture(
        x + static_cast<i16>(act.offsetX),
        y + static_cast<i16>(act.offsetY),
        unit.overlayPicture->width,
        unit.overlayPicture->height,
        unit.drawFlags | Painter::Additive
    );
}

// What BattleUnit::FadeMainIn decides of the frame; the draws into `scene`.
int UnitFrame::FadeMainInAdvance(BattleServices& services, Scene& scene)
{
    const Scene::Owner owner(scene, &this->unit);
    BattleUnit& unit = this->unit;
    this->DrawMainFadedAdvance(services, unit.brightLevel, scene);

    if (unit.held == 0)
    {
        ++unit.brightLevel;
    }

    if (unit.brightLevel < 0)
    {
        return 0;
    }

    unit.marks = static_cast<u16>(unit.marks & ~BattleUnit::UnkMark200);
    unit.brightLevel = 1;
    return 1;
}

// What BattleUnit::FadeMainOut decides of the frame; the draws into `scene`.
int UnitFrame::FadeMainOutAdvance(BattleServices& services, Scene& scene)
{
    const Scene::Owner owner(scene, &this->unit);
    BattleUnit& unit = this->unit;
    this->DrawMainFadedAdvance(services, unit.glowLevel, scene);

    if (unit.held == 0)
    {
        unit.glowLevel = static_cast<i16>(unit.glowLevel - kFadeOutStep);
    }

    if (unit.glowLevel > kFadedOut)
    {
        return 0;
    }

    unit.glowLevel = kFadedOut;
    unit.recoilAct = ActState{};
    return 1;
}

// What BattleUnit::FadeInvisible decides of the frame; the draws into `scene`.
int UnitFrame::FadeInvisibleAdvance(BattleServices& services, int mode, Scene& scene)
{
    const Scene::Owner owner(scene, &this->unit);
    BattleUnit& unit = this->unit;
    ScenePainter painter(services, scene);
    unit.mainAct.hold = 0;

    if (unit.held == 1)
    {
        unit.mainAct.hold = 1;
    }

    unit.picture = services.archive.GetPicture(unit.mainAct.frame, unit.mainAct.picture);
    ++unit.glowLevel;

    if (unit.glowLevel > kInvisibleFadeFrames)
    {
        unit.glowLevel = kInvisibleFadeFrames;

        if (mode == 1)
        {
            unit.marks = static_cast<u16>(unit.marks & ~(BattleUnit::InvisibleMark | BattleUnit::UnkMark400));
            unit.glowLevel = 0;
            return 0;
        }

        if (mode == 0)
        {
            unit.status |= BattleUnit::Invisible;
        }

        unit.marks = static_cast<u16>(unit.marks & ~(BattleUnit::InvisibleMark | BattleUnit::UnkMark400));
        return 0;
    }

    // A missing picture is read through a null pointer in the original.
    if (unit.picture == nullptr || unit.shadowPicture == nullptr)
    {
        return 1;
    }

    painter.SetSource(unit.picture->data.data());
    const int shadowHeight = unit.shadowPicture->height / kShadowShare;
    DrawOptions& options = painter.Options();
    options.scaleHeight = shadowHeight;
    options.shear = unit.shadowPicture->height >> kShadowShearShift;
    options.tintRed = kInvisibleShadowTint;
    options.tintGreen = kInvisibleShadowTint;
    options.tintBlue = kInvisibleShadowTint;
    unit.shadowMode = (unit.drawFlags & (Painter::RleSource | Painter::Mirror | Painter::Flip)) | Painter::Shadow;
    const int x = unit.placement.x - static_cast<i16>(unit.originX);
    painter.DrawPicture(
        x - kShadowLeftShift,
        unit.placement.y - shadowHeight,
        unit.shadowPicture->width,
        unit.shadowPicture->height,
        unit.shadowMode
    );

    if (mode == 0)
    {
        painter.Options().alpha = Painter::MaxAlpha - unit.glowLevel / 2;
    }
    else if (mode == 1)
    {
        painter.Options().alpha = unit.glowLevel / 2;
    }

    unit.shadowMode = unit.drawFlags & (Painter::RleSource | Painter::Mirror | Painter::Flip);
    painter.DrawPicture(
        x,
        unit.placement.y - unit.mainAct.y,
        unit.picture->width,
        unit.picture->height,
        unit.shadowMode | Painter::Translucent
    );
    return 1;
}

// What BattleUnit::DrawFadeIn decides of the frame; the draws into `scene`.
int UnitFrame::DrawFadeInAdvance(BattleServices& services, const BattleUnit& anchor, Scene& scene)
{
    const Scene::Owner owner(scene, &this->unit);
    BattleUnit& unit = this->unit;
    ScenePainter painter(services, scene);

    if ((unit.status & BattleUnit::Invisible) != 0)
    {
        return 1;
    }

    if (unit.brightLevel >= BattleUnit::Opaque)
    {
        unit.recoilAct = ActState{};
        return 1;
    }

    unit.unk2FEC = 1;
    unit.recoilAct.action = unit.commandA == BattleUnit::UnkCommand1 ? kCommand1Action : kShadowAction;
    unit.recoilAct.id = unit.actId;
    const u16 pose = unit.unk2FB6;

    if (pose != 0)
    {
        unit.recoilAct.action = pose == kPose2 ? kPose2Action : kPoseAction;
    }

    if (services.acts.Run(unit.recoilAct) == 0)
    {
        const std::string_view name(unit.displayName.data(), unit.displayName.size());
        ErrorBox(
            services.errors,
            u8"Fade Act",
            u8"[%s]QQ:%d號錯誤!!",
            name.substr(0, name.find('\0')),
            unit.recoilAct.action
        );
        return 0;
    }

    if ((unit.recoilAct.flags & ActState::Signal) != 0)
    {
        unit.brightLevel = BattleUnit::FadedIn;
        return 0;
    }

    unit.picture = services.archive.GetPicture(unit.recoilAct.frame, 0);

    // A missing picture is read through a null pointer in the original.
    if (unit.picture == nullptr)
    {
        return 0;
    }

    painter.SetSource(unit.picture->data.data());
    const u32 actFlags = unit.recoilAct.flags;
    unit.originX = static_cast<u16>(unit.recoilAct.x);
    const i32 blend = unit.autoControl;
    unit.drawFlags = actFlags;
    unit.recoilAct.hold = 1;
    unit.MirrorRecoil(actFlags, blend == 1);
    u16 anchorX = 0;
    u16 anchorY = 0;
    anchor.GetDrawPosition(anchorX, anchorY);

    if (unit.kind == 0 && unit.HasFixedOrigin() == 1)
    {
        anchorX = kFixedFadeInX;
        anchorY = kFixedFadeInY;
    }

    const int x = static_cast<i16>(anchorX) - static_cast<i16>(unit.originX);
    const int y = static_cast<i16>(anchorY) - unit.recoilAct.y;
    const i32 level = unit.brightLevel;
    DrawOptions& options = painter.Options();

    if (blend == 1)
    {
        options.tintRed = level - BattleUnit::FadedIn;
        options.tintGreen = level - BattleUnit::FadedIn;
        options.tintBlue = level - BattleUnit::FadedIn;
        painter.DrawPicture(x, y, unit.picture->width, unit.picture->height, unit.drawFlags);

        if (unit.held == 0)
        {
            ++unit.brightLevel;
        }

        return 0;
    }

    options.alpha = level;
    painter.DrawPicture(
        x,
        y,
        unit.picture->width,
        unit.picture->height,
        unit.drawFlags | Painter::Translucent,
        PaletteArgument(*unit.picture)
    );
    options.alpha = 0;
    options.tintRed = kFadeInTint - unit.brightLevel;
    options.tintGreen = kFadeInTint - unit.brightLevel;
    options.tintBlue = kFadeInTint - unit.brightLevel;
    painter.DrawPicture(x, y, unit.picture->width, unit.picture->height, unit.drawFlags | Painter::TintUnder);

    if (unit.held == 0)
    {
        unit.brightLevel += unit.fadeStep;
    }

    return 0;
}

// What BattleUnit::DrawFadeOut decides of the frame; the draws into `scene`.
int UnitFrame::DrawFadeOutAdvance(BattleServices& services, Scene& scene)
{
    const Scene::Owner owner(scene, &this->unit);
    BattleUnit& unit = this->unit;
    ScenePainter painter(services, scene);

    if ((unit.unk2BC0 & kFlag2BC0) != 0)
    {
        return 0;
    }

    if ((unit.status & BattleUnit::Invisible) != 0)
    {
        return 1;
    }

    if (unit.fadeLevel <= 0)
    {
        unit.trails[kFadeTrail].started = 0;
        unit.recoilAct = ActState{};
        return 1;
    }

    unit.recoilAct.id = unit.actId;
    unit.unk2FEC = 1;
    unit.recoilAct.action = kShadowAction;
    unit.recoilAct.hold = unit.held == 1 ? 1 : 0;

    if (services.acts.Run(unit.recoilAct) == 0)
    {
        return 1;
    }

    unit.picture = services.archive.GetPicture(unit.recoilAct.frame, unit.recoilAct.picture);

    if ((unit.recoilAct.flags & ActState::Signal) != 0)
    {
        unit.fadeLevel = 0;
        return 0;
    }

    if (unit.fadeLevel == BattleUnit::Opaque)
    {
        unit.recoilAct.view = kFadeOutSound;
        services.audio.PlaySound(kFadeOutSound, services.config.soundVolume);
        services.audio.SetSoundPan(unit.recoilAct.view, unit.kind == 1 ? kFadePan : -kFadePan);
        unit.recoilAct.view = 0;
    }

    // A missing picture is read through a null pointer in the original.
    if (unit.picture == nullptr)
    {
        return 0;
    }

    painter.SetSource(unit.picture->data.data());
    const i32 blend = unit.autoControl;
    const u32 actFlags = unit.recoilAct.flags;
    unit.drawFlags = actFlags;
    unit.MirrorRecoil(actFlags, blend == 1);
    const i32 level = unit.fadeLevel;
    const int x = unit.placement.x - static_cast<i16>(unit.originX);
    const int y = unit.placement.y - unit.recoilAct.y;
    DrawOptions& options = painter.Options();

    if (blend == 1)
    {
        options.tintRed = level - BattleUnit::Opaque;
        options.tintGreen = level - BattleUnit::Opaque;
        options.tintBlue = level - BattleUnit::Opaque;
        painter.DrawPicture(
            x,
            y,
            unit.picture->width,
            unit.picture->height,
            unit.drawFlags,
            PaletteArgument(*unit.picture)
        );

        if (unit.held == 0)
        {
            --unit.fadeLevel;
        }

        return 0;
    }

    options.alpha = level;
    painter.DrawPicture(
        x,
        y,
        unit.picture->width,
        unit.picture->height,
        unit.drawFlags | Painter::Translucent,
        PaletteArgument(*unit.picture)
    );
    options.alpha = 0;
    options.tintRed = unit.fadeLevel + kFadeOutTint;
    options.tintGreen = unit.fadeLevel + kFadeOutTint;
    options.tintBlue = unit.fadeLevel + kFadeOutTint;
    painter.DrawPicture(x, y, unit.picture->width, unit.picture->height, unit.drawFlags | Painter::TintUnder);

    if (unit.held == 0)
    {
        unit.fadeLevel -= unit.fadeStep;
    }

    return 0;
}

// What BattleUnit::DrawEffectIcons decides of the frame; the draws into `scene`.
void UnitFrame::DrawEffectIconsAdvance(BattleServices& services, int x, int y, Scene& scene)
{
    const Scene::Owner owner(scene, &this->unit);
    BattleUnit& unit = this->unit;
    ScenePainter painter(services, scene);

    if (unit.effects.next == nullptr)
    {
        return;
    }

    int count = 0;

    for (const EffectNode* node = &unit.effects; node != nullptr; node = node->next.get())
    {
        if (node->icon != 0)
        {
            unit.picture = services.archive.GetPicture(kEffectIcons, static_cast<u16>(node->icon - 1u));

            if (unit.picture != nullptr)
            {
                painter.SetSource(unit.picture->data.data());
                painter.DrawPicture(x + count * unit.picture->width, y, unit.picture->width, unit.picture->height, 0);
            }

            ++count;
        }
    }
}

// What BattleUnit::RunAway decides of the frame; the draws into `scene`.
int UnitFrame::RunAwayAdvance(BattleServices& services, Scene& scene)
{
    const Scene::Owner owner(scene, &this->unit);
    BattleUnit& unit = this->unit;
    ScenePainter painter(services, scene);
    ActState& run = unit.recoilAct;

    if (unit.fadeLevel <= kRunEnd)
    {
        unit.defeated = 1;
        run = ActState{};
        return 1;
    }

    if (run.picture == kStepPicture)
    {
        services.audio.PlaySound(kStepSound, services.config.soundVolume);
    }

    run.id = unit.actId;
    unit.unk2FEC = 1;
    run.action = kShadowAction;
    run.hold = 0;

    if (unit.held == 1)
    {
        run.hold = 1;
    }

    if (services.acts.Run(run) == 0)
    {
        return 1;
    }

    unit.picture = services.archive.GetPicture(run.frame, run.picture);
    unit.frameFlags = run.flags;
    unit.actOffsetX = static_cast<u16>(run.x);

    if (unit.fadeLevel == kRunSoundAt)
    {
        run.view = kRunSound;
        services.audio.PlaySound(run.view, services.config.soundVolume);
        services.audio.SetSoundPan(run.view, unit.facing == 1 ? kFadePan : -kFadePan);
        run.view = 0;
    }

    unit.frameFlags ^= Painter::Mirror;

    if (unit.facing == 1)
    {
        unit.frameFlags ^= Painter::Mirror;

        // A missing picture is read through a null pointer in the original.
        if (run.x != 0 && unit.picture != nullptr)
        {
            unit.actOffsetX = static_cast<u16>(unit.picture->width - static_cast<u16>(run.x));
        }
    }

    u16 x = 0;
    u16 y = 0;
    unit.GetPosition(x, y);
    const int stepped = unit.facing == 1 ? x + kRunStep : x - kRunStep;
    unit.SetPosition(static_cast<i16>(stepped), static_cast<i16>(y));

    if (unit.picture != nullptr)
    {
        painter.SetSource(unit.picture->data.data());
        painter.DrawPicture(
            unit.placement.x - static_cast<i16>(unit.actOffsetX),
            unit.placement.y - run.y,
            unit.picture->width,
            unit.picture->height,
            unit.frameFlags,
            PaletteArgument(*unit.picture)
        );
    }

    --unit.fadeLevel;
    return 0;
}

// What BattleUnit::DrawDefeatPiece decides of the frame; the draws into `scene`.
void UnitFrame::DrawDefeatPieceAdvance(
    BattleServices& services,
    const Picture& picture,
    int left,
    int top,
    int right,
    int bottom,
    int x,
    int y,
    int height,
    Scene& scene
)
{
    const Scene::Owner owner(scene, &this->unit);
    BattleUnit& unit = this->unit;
    ScenePainter painter(services, scene);
    painter.SetClipClamped(left, top, right, bottom);
    DrawOptions& options = painter.Options();
    options.tintRed = -unit.glowLevel;
    options.tintGreen = -unit.glowLevel;
    options.tintBlue = -unit.glowLevel;
    painter.DrawPicture(x, y, picture.width, height, unit.drawFlags | Painter::Additive);
}

// What BattleUnit::DefeatFadeOut decides of the frame; the draws into `scene`.
int UnitFrame::DefeatFadeOutAdvance(BattleServices& services, const Picture& picture, Scene& scene)
{
    const Scene::Owner owner(scene, &this->unit);
    BattleUnit& unit = this->unit;

    if (unit.glowLevel <= kFadedOut)
    {
        unit.glowLevel = 0;
        unit.slotTint = 0;
        return unit.EndDefeat(services);
    }

    unit.StartDefeatDraw(services, picture);
    ScenePainter painter(services, scene);
    DrawOptions& options = painter.Options();
    options.tintRed = unit.glowLevel;
    options.tintGreen = unit.glowLevel;
    options.tintBlue = unit.glowLevel;
    painter.DrawPicture(
        unit.placement.x - static_cast<i16>(unit.originX),
        unit.placement.y - unit.mainAct.y,
        picture.width,
        picture.height,
        unit.drawFlags | Painter::Additive
    );
    --unit.glowLevel;
    return 0;
}

// What BattleUnit::DefeatFill decides of the frame; the draws into `scene`.
int UnitFrame::DefeatFillAdvance(BattleServices& services, const Picture& picture, int x, int y, Scene& scene)
{
    const Scene::Owner owner(scene, &this->unit);
    BattleUnit& unit = this->unit;

    if (unit.glowLevel >= picture.height)
    {
        unit.defeatPose = kBurstPose;
        unit.glowLevel = 0;
        return 0;
    }

    unit.StartDefeatDraw(services, picture);
    ScenePainter painter(services, scene);
    painter.DrawPicture(x, y, picture.width, picture.height, unit.drawFlags);

    if (unit.glowLevel > picture.height)
    {
        unit.glowLevel = static_cast<i16>(picture.height - 1);
    }

    DrawOptions& options = painter.Options();
    const bool blue = unit.defeatPose == kBlueFillPose;
    options.tintRed = blue ? 0 : kDarkFillTint;
    options.tintGreen = blue ? 0 : kDarkFillTint;
    options.tintBlue = blue ? kBlueFillTint : kDarkFillTint;
    painter.DrawPicture(
        x,
        y,
        picture.width,
        blue ? unit.glowLevel + 1 : unit.glowLevel,
        unit.drawFlags | Painter::TintUnder
    );
    painter.SetClipClamped(0, 0, Display::Width, Display::Height);
    unit.glowLevel = static_cast<i16>(unit.glowLevel + (blue ? kBlueFillStep : kDarkFillStep));
    return 0;
}

// What BattleUnit::DefeatHalves decides of the frame; the draws into `scene`.
int UnitFrame::DefeatHalvesAdvance(BattleServices& services, const Picture& picture, int x, int y, Scene& scene)
{
    const Scene::Owner owner(scene, &this->unit);
    BattleUnit& unit = this->unit;
    ScenePainter painter(services, scene);

    if (unit.glowLevel > kSplitEnd)
    {
        unit.glowLevel = 0;
        unit.slotTint = 0;
        return unit.EndDefeat(services);
    }

    unit.StartDefeatDraw(services, picture);
    const int shift = 2 * unit.glowLevel;
    const int half = picture.height >> 1;
    this->DrawDefeatPieceAdvance(
        services,
        picture,
        x - shift,
        y,
        x + picture.width - shift,
        y + half,
        x - shift,
        y,
        picture.height,
        scene
    );
    this->DrawDefeatPieceAdvance(
        services,
        picture,
        x + shift,
        y + half,
        x + shift + picture.width,
        y + picture.height,
        x + shift,
        y,
        picture.height,
        scene
    );

    if (unit.glowLevel >= kHalvesSlow)
    {
        ++unit.glowLevel;
    }
    else if (unit.glowLevel == kHalvesPause)
    {
        ++unit.slotTint;

        if (unit.slotTint == kHalvesPause)
        {
            unit.glowLevel = kHalvesSlow;
        }
    }
    else
    {
        unit.glowLevel = static_cast<i16>(unit.glowLevel + kHalvesFast);
    }

    painter.SetClipClamped(0, 0, Display::Width, Display::Height);
    return 0;
}

// What BattleUnit::DefeatHop decides of the frame; the draws into `scene`.
int UnitFrame::DefeatHopAdvance(BattleServices& services, const Picture& picture, int x, int y, Scene& scene)
{
    const Scene::Owner owner(scene, &this->unit);
    BattleUnit& unit = this->unit;

    if (unit.glowLevel >= static_cast<i16>(BattleUnit::HopHeights.size()))
    {
        unit.glowLevel = 0;
        return unit.EndDefeat(services);
    }

    if (unit.glowLevel == 0)
    {
        const u16 sound = unit.castAct.view;
        unit.standAct.view = kDefeatSound;
        services.audio.PlaySound(sound, services.config.soundVolume);
    }

    ScenePainter painter(services, scene);
    painter.SetSource(picture.data.data());
    painter.Options().alpha = Painter::MaxAlpha - unit.glowLevel;
    painter.DrawPicture(
        x,
        y - BattleUnit::HopHeights[static_cast<std::size_t>(unit.glowLevel)],
        picture.width,
        picture.height,
        unit.drawFlags | Painter::Translucent | Painter::Flip
    );
    ++unit.glowLevel;
    return 0;
}

// What BattleUnit::DefeatQuarters decides of the frame; the draws into `scene`.
int UnitFrame::DefeatQuartersAdvance(BattleServices& services, const Picture& picture, int x, int y, Scene& scene)
{
    const Scene::Owner owner(scene, &this->unit);
    BattleUnit& unit = this->unit;
    ScenePainter painter(services, scene);

    if (unit.glowLevel >= kSplitEnd)
    {
        unit.glowLevel = 0;
        return unit.EndDefeat(services);
    }

    unit.StartDefeatDraw(services, picture);
    const int shift = unit.glowLevel;
    const int halfWidth = picture.width >> 1;
    const int halfHeight = picture.height >> 1;
    this->DrawDefeatPieceAdvance(
        services,
        picture,
        x - shift,
        y - shift,
        x + halfWidth - shift,
        y + halfHeight - shift,
        x - shift,
        y - shift,
        halfHeight,
        scene
    );
    this->DrawDefeatPieceAdvance(
        services,
        picture,
        x - shift,
        y + halfHeight + shift,
        x + halfWidth - shift,
        y + picture.height + shift,
        x - shift,
        y + shift,
        picture.height,
        scene
    );
    this->DrawDefeatPieceAdvance(
        services,
        picture,
        x + halfWidth + shift,
        y - shift,
        x + picture.width + shift,
        y + halfHeight - shift,
        x + shift,
        y - shift,
        picture.height,
        scene
    );
    this->DrawDefeatPieceAdvance(
        services,
        picture,
        x + halfWidth + shift,
        y + halfHeight + shift,
        x + picture.width + shift,
        y + picture.height + shift,
        x + shift,
        y + shift + kQuarterDrop,
        picture.height,
        scene
    );
    ++unit.glowLevel;
    painter.SetClipClamped(0, 0, Display::Width, Display::Height);
    return 0;
}

// What BattleUnit::DefeatRising decides of the frame; the draws into `scene`.
int UnitFrame::DefeatRisingAdvance(BattleServices& services, const Picture& picture, int x, int y, Scene& scene)
{
    const Scene::Owner owner(scene, &this->unit);
    BattleUnit& unit = this->unit;

    if (unit.glowLevel > kRisingEnd)
    {
        unit.glowLevel = 0;
        return unit.EndDefeat(services);
    }

    unit.StartDefeatDraw(services, picture);
    ScenePainter painter(services, scene);
    const int tint = 2 * (unit.glowLevel / kRisingTintDivisor);
    DrawOptions& options = painter.Options();
    options.tintRed = tint;
    options.tintGreen = tint;
    options.tintBlue = tint;
    options.scaleHeight = unit.glowLevel + picture.height;
    unit.drawFlags |= Painter::ScaledGlow;
    painter.DrawPicture(x, y - kRisingTintLift * tint - unit.glowLevel, picture.width, picture.height, unit.drawFlags);
    unit.glowLevel = static_cast<i16>(unit.glowLevel + kRisingStep);
    return 0;
}

// What BattleUnit::DefeatSink decides of the frame; the draws into `scene`.
int UnitFrame::DefeatSinkAdvance(BattleServices& services, const Picture& picture, Scene& scene)
{
    const Scene::Owner owner(scene, &this->unit);
    BattleUnit& unit = this->unit;

    if (unit.glowLevel <= kFadedOut)
    {
        unit.glowLevel = 0;
        unit.slotTint = 0;
        return unit.EndDefeat(services);
    }

    unit.StartDefeatDraw(services, picture);
    ScenePainter painter(services, scene);
    const int x = unit.placement.x - static_cast<i16>(unit.originX);
    const int y = unit.placement.y - unit.mainAct.y;
    painter.Options().alpha = Painter::MaxAlpha;

    if (unit.glowLevel <= kSinkFadeFrom)
    {
        painter.Options().alpha = unit.glowLevel + kSinkAlphaBase;
    }

    painter.DrawPicture(x, y, picture.width, picture.height, unit.drawFlags | Painter::Translucent);
    painter.Options().alpha = 0;
    DrawOptions& options = painter.Options();
    options.tintRed = unit.glowLevel;
    options.tintGreen = unit.glowLevel;
    options.tintBlue = unit.glowLevel;
    painter.DrawPicture(
        x,
        y + kSinkStep * unit.glowLevel,
        picture.width,
        picture.height,
        unit.drawFlags | Painter::Additive
    );
    --unit.glowLevel;
    return 0;
}

// What BattleUnit::DefeatSlide decides of the frame; the draws into `scene`.
int UnitFrame::DefeatSlideAdvance(BattleServices& services, const Picture& picture, Scene& scene)
{
    const Scene::Owner owner(scene, &this->unit);
    BattleUnit& unit = this->unit;

    if (unit.glowLevel <= kFadedOut)
    {
        unit.glowLevel = 0;
        unit.slotTint = 0;
        return unit.EndDefeat(services);
    }

    unit.StartDefeatDraw(services, picture);
    ScenePainter painter(services, scene);
    const int x = unit.placement.x - static_cast<i16>(unit.originX);
    const int y = unit.placement.y - unit.mainAct.y;

    if (unit.glowLevel >= kSlideFadeFrom)
    {
        painter.Options().alpha = unit.glowLevel + Painter::MaxAlpha;
    }

    painter.DrawPicture(x, y, picture.width, picture.height, unit.drawFlags | Painter::Translucent);
    painter.Options().alpha = 0;
    DrawOptions& options = painter.Options();
    options.tintRed = unit.glowLevel;
    options.tintGreen = unit.glowLevel;
    options.tintBlue = unit.glowLevel;
    painter.DrawPicture(x + 2 * unit.glowLevel, y, picture.width, picture.height, unit.drawFlags | Painter::Additive);

    --unit.glowLevel;
    return 0;
}

// What BattleUnit::DefeatSplitColumns decides of the frame; the draws into `scene`.
int UnitFrame::DefeatSplitColumnsAdvance(BattleServices& services, const Picture& picture, int x, int y, Scene& scene)
{
    const Scene::Owner owner(scene, &this->unit);
    BattleUnit& unit = this->unit;
    ScenePainter painter(services, scene);

    if (unit.glowLevel > kSplitEnd)
    {
        unit.glowLevel = 0;
        return unit.EndDefeat(services);
    }

    unit.StartDefeatDraw(services, picture);
    const int shift = 2 * unit.glowLevel;
    const int half = picture.height >> 1;
    this->DrawDefeatPieceAdvance(
        services,
        picture,
        x - shift,
        y,
        x + picture.width - shift,
        y + half,
        x - shift,
        y,
        picture.height,
        scene
    );
    this->DrawDefeatPieceAdvance(
        services,
        picture,
        x + shift,
        y + half,
        x + shift + picture.width,
        y + picture.height,
        x + shift,
        y,
        picture.height,
        scene
    );
    unit.glowLevel = static_cast<i16>(unit.glowLevel + kSplitStep);
    painter.SetClipClamped(0, 0, Display::Width, Display::Height);
    return 0;
}

// What BattleUnit::DefeatSplitRows decides of the frame; the draws into `scene`.
int UnitFrame::DefeatSplitRowsAdvance(BattleServices& services, const Picture& picture, int x, int y, Scene& scene)
{
    const Scene::Owner owner(scene, &this->unit);
    BattleUnit& unit = this->unit;
    ScenePainter painter(services, scene);

    if (unit.glowLevel > kSplitEnd)
    {
        unit.glowLevel = 0;
        return unit.EndDefeat(services);
    }

    unit.StartDefeatDraw(services, picture);
    const int shift = 2 * unit.glowLevel;
    const int half = picture.width >> 1;
    this->DrawDefeatPieceAdvance(
        services,
        picture,
        x,
        y - shift,
        x + half,
        y + picture.height - shift,
        x,
        y - shift,
        picture.height,
        scene
    );
    this->DrawDefeatPieceAdvance(
        services,
        picture,
        x + half,
        y + shift,
        x + picture.width,
        y + shift + picture.height,
        x,
        y + shift,
        picture.height,
        scene
    );
    unit.glowLevel = static_cast<i16>(unit.glowLevel + kSplitStep);
    painter.SetClipClamped(0, 0, Display::Width, Display::Height);
    return 0;
}

// What BattleUnit::DefeatStrips decides of the frame; the draws into `scene`.
int UnitFrame::DefeatStripsAdvance(BattleServices& services, const Picture& picture, int x, int y, Scene& scene)
{
    const Scene::Owner owner(scene, &this->unit);
    BattleUnit& unit = this->unit;
    ScenePainter painter(services, scene);

    if (unit.glowLevel > kSplitEnd)
    {
        unit.glowLevel = 0;
        return unit.EndDefeat(services);
    }

    unit.StartDefeatDraw(services, picture);
    const int shift = 2 * unit.glowLevel;
    const int strip = picture.height >> 2;
    const int height = picture.height;
    this->DrawDefeatPieceAdvance(
        services,
        picture,
        x - shift,
        y,
        x + picture.width - shift,
        y + strip,
        x - shift,
        y,
        height,
        scene
    );
    this->DrawDefeatPieceAdvance(
        services,
        picture,
        x + shift,
        y + strip,
        x + shift + picture.width,
        y + 2 * strip,
        x + shift,
        y,
        height,
        scene
    );
    this->DrawDefeatPieceAdvance(
        services,
        picture,
        x - shift,
        y + 2 * strip,
        x + picture.width - shift,
        y + 3 * strip,
        x - shift,
        y,
        height,
        scene
    );
    this->DrawDefeatPieceAdvance(
        services,
        picture,
        x + shift,
        y + 3 * strip,
        x + shift + picture.width,
        y + height,
        x + shift,
        y,
        height,
        scene
    );
    unit.glowLevel = static_cast<i16>(unit.glowLevel + kSplitStep);
    painter.SetClipClamped(0, 0, Display::Width, Display::Height);
    return 0;
}

// What BattleUnit::DefeatWave decides of the frame; the draws into `scene`.
int UnitFrame::DefeatWaveAdvance(BattleServices& services, const Picture& picture, Scene& scene)
{
    const Scene::Owner owner(scene, &this->unit);
    BattleUnit& unit = this->unit;

    if (unit.glowLevel >= kWaveEnd)
    {
        unit.glowLevel = 0;
        unit.slotTint = 0;
        return unit.EndDefeat(services);
    }

    unit.StartDefeatDraw(services, picture);
    ScenePainter painter(services, scene);
    const int wave = unit.glowLevel / kWaveShare + 1;
    painter.SetWave(wave);
    painter.Options().alpha = Painter::MaxAlpha - wave;
    painter.DrawPicture(
        unit.placement.x - static_cast<i16>(unit.originX),
        unit.placement.y - unit.mainAct.y,
        picture.width,
        picture.height,
        unit.drawFlags | Painter::Translucent
    );
    painter.Options().alpha = 0;
    painter.SetWave(0);
    ++unit.glowLevel;
    return 0;
}

// What BattleUnit::RunDefeatParticles decides of the frame; the draws into `scene`.
int UnitFrame::RunDefeatParticlesAdvance(BattleServices& services, Scene& scene)
{
    const Scene::Owner owner(scene, &this->unit);
    BattleUnit& unit = this->unit;

    if (ExplosionFrame(services.state.projector)
            .Explosion(services.screen, services.painter, services.crtRandom, unit.absorb, scene) == 1)
    {
        unit.glowLevel = kParticlesDone;
    }

    return 0;
}

// What BattleUnit::DefeatBurst decides of the frame; the draws into `scene`.
int UnitFrame::DefeatBurstAdvance(BattleServices& services, const Picture& picture, int x, int y, Scene& scene)
{
    const Scene::Owner owner(scene, &this->unit);
    BattleUnit& unit = this->unit;

    if (unit.glowLevel == kParticlesDone)
    {
        return unit.EndParticleDefeat(services);
    }

    if (unit.absorbPixels.empty())
    {
        ParticleEffect& effect = unit.absorb;
        effect = ParticleEffect{};
        unit.DecodeForParticles(services, picture, unit.defeatPose == kBurstPose ? kBurstDecodeLine : kSweepDecodeLine);
        effect.width = picture.width;
        effect.height = picture.height;
        effect.x = x;
        effect.y = y;
        effect.flags = static_cast<u16>(effect.flags | ParticleEffect::Mixed);
        effect.bursts = 1;
        effect.divisor = kBurstDivisor;

        if (unit.defeatPose == kBurstPose)
        {
            effect.targetLeft = x + picture.width / kBurstTargetShare;
            effect.targetTop = y + (picture.height >> 1);
            effect.targetWidth = kBurstTarget;
            effect.targetHeight = kBurstTarget;
            effect.speed = kBurstSpeed;
            effect.lifeBase = kBurstLife;
        }
        else
        {
            effect.targetLeft = unit.facing == 1 ? kSweepRight : kSweepLeft;
            effect.targetTop = y - kSweepAbove;
            effect.targetWidth = kBurstTarget;
            effect.targetHeight = picture.height + kSweepAbove;
            effect.speed = kSweepSpeed;
            effect.lifeBase = kSweepLife;
        }

        if (unit.Mirrored() == 1)
        {
            effect.flags = static_cast<u16>(effect.flags | ParticleEffect::Mirrored);
        }

        services.state.projector.SetScreen(Display::Width, Display::Height);

        if (unit.defeatPose == kBurstPose)
        {
            unit.castAct.view = kDefeatSound;
        }

        services.audio.PlaySound(kDefeatSound, services.config.soundVolume);
        unit.glowLevel = 1;
    }

    return this->RunDefeatParticlesAdvance(services, scene);
}

// What BattleUnit::DefeatParticles decides of the frame; the draws into `scene`.
int UnitFrame::DefeatParticlesAdvance(BattleServices& services, const Picture& picture, int x, int y, Scene& scene)
{
    const Scene::Owner owner(scene, &this->unit);
    BattleUnit& unit = this->unit;

    if (unit.glowLevel == kParticlesDone)
    {
        return unit.EndParticleDefeat(services);
    }

    if (unit.glowLevel == 0)
    {
        if (unit.throwSteps >= kWobbles)
        {
            unit.recoilTint = 0;
            unit.glowLevel = 1;
            unit.throwSteps = 0;
            return 0;
        }

        if (unit.recoilTint == kWobbleTop)
        {
            services.audio.PlaySound(kWobbleSound, services.config.soundVolume);
        }

        ScenePainter painter(services, scene);
        painter.SetSource(picture.data.data());
        DrawOptions& options = painter.Options();
        options.tintRed = unit.recoilTint;
        options.tintGreen = unit.recoilTint;
        options.tintBlue = unit.recoilTint;
        painter.DrawPicture(
            unit.placement.x - static_cast<i16>(unit.originX),
            unit.placement.y - unit.mainAct.y,
            picture.width,
            picture.height,
            unit.drawFlags | Painter::Additive
        );
        const i32 wobble = unit.throwSteps;

        if (wobble % 2 != 0)
        {
            --unit.recoilTint;
        }
        else
        {
            unit.recoilTint = static_cast<i16>(unit.recoilTint + kWobbleRise);
        }

        if (unit.recoilTint >= kWobbleTop || unit.recoilTint <= kWobbleBottom)
        {
            unit.throwSteps = wobble + 1;
        }

        return 0;
    }

    if (unit.absorbPixels.empty())
    {
        ParticleEffect& effect = unit.absorb;
        effect = ParticleEffect{};
        unit.DecodeForParticles(services, picture, kStripDecodeLine);
        effect.width = picture.width;
        effect.height = picture.height;
        effect.x = x;
        effect.y = y;
        effect.targetLeft = unit.facing == 1 ? kStripRight : 0;
        effect.targetTop = 1;
        effect.targetWidth = kStripWidth;
        effect.targetHeight = 1;
        effect.speed = kStripSpeed;
        effect.lifeBase = kStripLife;
        effect.bursts = static_cast<u16>(picture.height >> 2);
        effect.divisor = kStripDivisor;
        effect.flags = kStripFlags;

        if (unit.Mirrored() == 1)
        {
            effect.flags = static_cast<u16>(effect.flags | ParticleEffect::Mirrored);
        }

        effect.tintRed = kStripTint;
        effect.tintGreen = kStripTint;
        effect.tintBlue = kStripTint;
        services.state.projector.SetScreen(Display::Width, Display::Height);
        services.audio.PlaySound(kDefeatSound, services.config.soundVolume);
        unit.glowLevel = kParticlesStarted;
    }

    ++unit.glowLevel;
    unit.absorb.x = x + unit.glowLevel % kShakeSpan;
    return this->RunDefeatParticlesAdvance(services, scene);
}

// What BattleUnit::DefeatProjected decides of the frame; the draws into `scene`.
int UnitFrame::DefeatProjectedAdvance(BattleServices& services, const Picture& picture, Scene& scene)
{
    const Scene::Owner owner(scene, &this->unit);
    BattleUnit& unit = this->unit;

    if (unit.glowLevel >= kParticlesDone)
    {
        unit.glowLevel = 0;
        return unit.EndDefeat(services);
    }

    ProjectedPicture& projected = unit.projected;

    if (unit.glowLevel == 0)
    {
        projected.scaleX = kZoomUnit;
        projected.scaleY = kZoomUnit;
        projected.unk08 = picture.width >> 1;
        projected.unk0C = picture.height;
        projected.width = picture.width;
        projected.height = picture.height;
        projected.flags = ProjectedPicture::Mixed;
        projected.x = unit.placement.x - (picture.width >> 1);
        projected.y = unit.placement.y;

        if (unit.Mirrored() == 1)
        {
            projected.flags = static_cast<u16>(projected.flags | ProjectedPicture::Mirrored);
        }

        projected.angle = kProjectedAngle;
        unit.projectedPixels.clear();
        const std::optional<RawPicture> raw = RleCodec(services.painter).Decode(picture.data);

        if (raw.has_value())
        {
            unit.projectedPixels.resize(raw->pixels.size() / sizeof(u16));
            std::memcpy(unit.projectedPixels.data(), raw->pixels.data(), unit.projectedPixels.size() * sizeof(u16));
        }

        projected.pixels = unit.projectedPixels;
        const u16 sound = unit.castAct.view;
        unit.standAct.view = kDefeatSound;
        services.audio.PlaySound(sound, services.config.soundVolume);
    }

    unit.glowLevel = static_cast<i16>(unit.glowLevel + kProjectedStep);
    projected.tintRed = kProjectedTint;
    projected.tintGreen = kProjectedTint;
    projected.tintBlue = kProjectedTint;
    projected.scaleX += kProjectedGrowth;
    projected.scaleY += kProjectedGrowth;
    ExplosionFrame(services.state.projector).Scaled(services.screen, services.painter, projected, scene);
    return 0;
}

// What BattleUnit::DrainEffectFrame decides of the frame; the draws into `scene`.
int UnitFrame::DrainEffectFrameAdvance(BattleServices& services, int slot, Scene& scene)
{
    const Scene::Owner owner(scene, &this->unit);
    BattleUnit& unit = this->unit;
    ParticleEffect& effect = unit.drains[static_cast<std::size_t>(slot)];
    effect.tintRed = kDrainTint;
    effect.tintGreen = kDrainTint;
    effect.tintBlue = kDrainTint;
    unit.absorbing = 1;

    if (ExplosionFrame(services.state.projector)
            .Explosion(services.screen, services.painter, services.crtRandom, effect, scene) != 1)
    {
        return 0;
    }

    unit.drainPixels[static_cast<std::size_t>(slot)].clear();
    effect = ParticleEffect{};
    return 1;
}

}  // namespace swd3e::smooth
