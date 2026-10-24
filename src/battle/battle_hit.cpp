#include "game/game.h"
#include <algorithm>

#include "battle/battle_hit_constants.h"
#include "battle/smooth/smooth_hit.h"
#include "core/fault.h"
#include "core/format.h"
#include "platform/display.h"

namespace swd3e
{

namespace
{

constexpr i16 kPushStart = 0x1A4;  // the push of a hit, halved each call
constexpr i16 kSlideStep = 0x1E;   // the units slide back by at most this a call
constexpr int kSlideToShare = 8;
constexpr u16 kWalkModeMask = 0x7FFF;  // WalkLine (swd3e): BattleState::walkModes
constexpr u16 kWalkRises = 1;
constexpr u16 kWalkRisen = 0x8000;
constexpr i32 kWalkRiseTop = 0x32;
constexpr int kWalkRiseSteps = 0xE;
constexpr u32 kWalkFastest = 0xFFFFFFFF;  // a speed of -1: the step factor doubles
constexpr u16 kWalkDoublingTop = 0x40;
constexpr int kSlideBackShare = 4;

constexpr u16 kHitNudgeFrames = 0x1E;
constexpr std::array<std::pair<u16, u16>, 3> kHitNudges = {{{0x1000, 1}, {0x2000, 2}, {0x4000, 3}}};
constexpr i32 kMaxHitDamage = 0x270F;
constexpr u16 kDigitsDelay = 8;
constexpr int kAllHitLeftX = 0xA0;  // HitAllFrame: the place of a picture without an anchor
constexpr int kAllHitRightX = 0x1E0;
constexpr int kAllHitY = 0xEB;

constexpr u16 kWhitePixel = 0xFFFF;  // HitFrame: the frame flashed white on a defeat
constexpr u16 kEnemyAttack = 1;      // EnemyActionFrame: the command A of the acting enemy
constexpr u16 kEnemySpell = 2;
constexpr u16 kEnemyDrain = 6;
constexpr u16 kEnemySkill = 4;       // swd3e: the enemy's skill act (EnemySkillFrame)
constexpr u16 kEnemyDefeatRole = 9;  // swd3e (swd32: 7)
constexpr u16 kEnemyFadeOut = 0xA;
constexpr u16 kEnemyFadeIn = 0xB;
constexpr u16 kEnemyHide = 0xC;
constexpr u16 kEnemyCall = 0xF;
constexpr u16 kEnemyEscape = 0x11;
constexpr u16 kEnemyDone = 100;
constexpr u16 kEnemyWalk = 200;
constexpr u16 kEnemyReturn = 300;
constexpr int kRippleGrid = 0xC8;
constexpr i16 kDrainDark = -12;
constexpr i32 kDefeatedUnitShift = 4;
constexpr i16 kFadedWord = 8;
constexpr i32 kEndCheckState = 0x63;
constexpr int kAfterAttackCommand = 0x12C;
constexpr u16 kCallStarted = 0x8000;
constexpr u16 kCallCount = 0x7FFF;
constexpr u16 kCallFrames = 0x19;
constexpr i16 kSlideMiddle = 0x140;  // swd3e: slideX around an enemy's attack
constexpr u32 kHoldingAilment = 0x400;
constexpr u16 kEnemyStoleSound = 0x117;
constexpr int kStealNoticeX = 0x118;
constexpr int kStealNoticeY = 0xA;
constexpr i16 kStealNoticeFrames = 0x32;
constexpr std::u8string_view kEnemyStoleFormat = u8"敵人成功偷走了[%s]";  // 0x004BF3A4
constexpr std::u8string_view kEnemyStealFailed = u8"偷竊失敗";            // 0x004BF398
constexpr u16 kEnrageHitPicture = 0x2389;                                 // the picture of the enrage damage
constexpr i32 kSpellMusicFloor = 5;  // the battle music fades down to this during an enemy spell
constexpr i16 kCallX = 0xF0;
constexpr i16 kCallY = 0xC8;  // swd3e
constexpr i16 kSecondCallY = 0x154;
constexpr int kEnemySlotRecord = 2;
constexpr int kHangDraws = 0x10000;   // random draws after which a search of the original is taken as never ending
constexpr int kOrderTargetShift = 8;  // BattleState::enemyOrders: the high byte holds the bits, the low the target
constexpr u16 kOrderTarget = 0xFF;
constexpr u16 kOrderPrepared = 0x8000;
constexpr u16 kOrderSpell = 0x4000;
constexpr u16 kOrderDrain = 0x2000;
constexpr int kRunCommand = 0x11;
constexpr i16 kRunWord = 2;
constexpr int kEnemySkillX = 0x118;
constexpr i16 kEnemySkillFrames = 0x23;
constexpr int kEnemySpellCommand = 2;
constexpr int kEnemyDrainCommand = 6;
constexpr i32 kLastAimedUnit = 0x14;
constexpr int kStepHomeCommand = 0x12C;                   // CatchRoll 0: the enemy only shows its name and steps home
constexpr u32 kUnaimedEffect = SkillScript::UnkAilment4;  // an enemy does not aim at a unit with this effect
constexpr u32 kTauntEffect = SkillScript::UnkAilment8;    // the enemies aim past a role with it while others are free
constexpr i32 kSameTargetLimit = 3;     // an enemy aims at the same role at most this many times in a row
constexpr i32 kMarkedRoleBit = 0x1000;  // unk26B8 of an enemy marking a role (unk576FC8)
constexpr int kMarkedShare = 5;         // HitPointsAtMost: the role marked is at a twentieth of its hit points
constexpr i32 kMarkedTurns = 2;
constexpr u16 kTransferTarget = 0x2389;  // the hit picture of a transferred damage
constexpr i16 kOutOfChoicesCause = 0x1E;
constexpr i32 kFacingPanel = 0x80;
constexpr u16 kTalkCountdown = 0x7FFF;
constexpr u16 kNoAimedEnemy = 0xFFFF;

}  // namespace

// swd3e.exe 0x00463ED0: one frame of act `act` on `unit` (unit `index`, its hit act hitActs); 1 once the act waits
// (cleared) or ends (the follow act followActs cleared instead). A quaking act quakes the screen. The picture is read
// straight from its file each frame, mirrored with the sides, at the unit's offset from its origin (or its draw
// place), with the act's sound panned by the side.
int Game::PlayHitAct(BattleUnit& unit, u32 act, int index)
{
    smooth::Scene scene;
    const auto result = smooth::HitFrame(*this).PlayHitActAdvance(unit, act, index, scene);
    scene.Show(this->battleServices);
    return result;
}

// swd3e.exe 0x00464110: one frame of act `act` (action `action`) fading on `unit` (unit `index`, its act
// fadeActs): 1 at once without an act, or once the fade (fadeActLevels, darker by kHitFadeStep a frame) is past
// kHitFadeEnd (reset to 0); 0 when the act ends.
int Game::FadeHitAct(BattleUnit& unit, u32 act, i32 action, int index)
{
    smooth::Scene scene;
    const auto result = smooth::HitFrame(*this).FadeHitActAdvance(unit, act, action, index, scene);
    scene.Show(this->battleServices);
    return result;
}

// swd3e.exe 0x004680A0: the point unit `index` reaches walking the line from (fromX, fromY) to (toX, toY), `speed`
// times the step factor (walkLineCalls) steps a call; 1 once it reaches toX (its state reset). Walking (walkModes low
// bits 0), the factor grows after `delay` calls (none without one): by one, or with speed 0 or -1 (taken as 1) doubled
// up to 0x40 and by 0x40 after. Mode 1 first rises 14 steps a factor towards (fromX, 0x32), turning to 0x8000 once
// there, the line then kept towards that point. Another mode stays put and returns 1 only with speed -1.
int Game::WalkLine(i32& x, i32& y, int fromX, int fromY, int toX, int toY, u32 speed, int delay, int index)
{
    BattleState& battle = this->battleState;
    const auto at = static_cast<std::size_t>(index);
    u16& factor = battle.walkLineCalls[at];
    u16& waited = battle.walkDelays[at];
    u16& mode = battle.walkModes[at];
    LineWalker walker;
    walker.fromX = fromX;
    walker.fromY = fromY;
    walker.toX = toX;
    walker.toY = toY;
    int lastX = fromX;
    const u16 low = static_cast<u16>(mode & kWalkModeMask);
    u32 steps = speed;
    bool grows = low == 0;

    if (grows && delay != 0)
    {
        ++waited;
        grows = waited >= delay;
    }

    if (grows)
    {
        if (speed == 0 || speed == kWalkFastest)
        {
            steps = 1;

            if (factor == 0)
            {
                factor = 1;
            }

            factor = static_cast<u16>(factor > kWalkDoublingTop ? factor + kWalkDoublingTop : 2 * factor);
        }
        else
        {
            ++factor;
        }
    }

    i32 outcome = static_cast<i32>(speed);

    if (low == kWalkRises)
    {
        outcome = 0;
        walker.toX = fromX;
        walker.toY = kWalkRiseTop;
        bool reached = false;

        if (kWalkRiseSteps * factor > 0)
        {
            while (true)
            {
                const bool done = LineStep(walker);
                lastX = walker.fromX;

                if (done || walker.fromX + walker.x == walker.toX)
                {
                    reached = true;
                    break;
                }

                if (++outcome >= kWalkRiseSteps * factor)
                {
                    break;
                }
            }
        }

        if (reached)
        {
            factor = 0;
            mode = kWalkRisen;
        }

        ++factor;
    }

    if ((mode & kWalkModeMask) == 0)
    {
        outcome = 0;

        if (steps * factor > 0)
        {
            while (true)
            {
                const bool done = LineStep(walker);
                lastX = walker.fromX;

                if (done || walker.fromX + walker.x == walker.toX)
                {
                    outcome = -1;
                    break;
                }

                if (static_cast<u32>(++outcome) >= steps * factor)
                {
                    break;
                }
            }
        }
    }

    x = lastX + walker.x;
    y = walker.fromY + walker.y;

    if (outcome != -1)
    {
        return 0;
    }

    waited = 0;
    factor = 0;
    mode = 0;
    return 1;
}

// swd3e.exe 0x00468440: the units start sliding to `x` (SlideUnits mode 1)
i16 BattleState::StartSlideTo(i16 x)
{
    this->slideMode = BattleState::SlideToUnit;
    this->slideTarget = x;
    return x;
}

// swd3e.exe 0x00468460: and back (mode 2)
i16 BattleState::StartSlideBack(i16 x)
{
    this->slideMode = BattleState::SlideBack;
    this->slideTarget = x;
    return x;
}

// swd3e.exe 0x004682B0: one step of the slide (only while slideEnabled, which nothing sets): mode 1 moves the field
// (fieldX) an eighth of the way from `unit`'s x to `x` (a pixel when less), mode 2 a quarter of the way from the
// field back to the middle, ending there (mode 0); every unit moves with the step (shake). 0 without a slide.
int Game::SlideUnits(const BattleUnit& unit, i16 x)
{
    BattleState& battle = this->battleState;

    if (battle.slideEnabled == 0 || battle.slideMode == 0)
    {
        return 0;
    }

    int step = battle.shake;

    if (battle.slideMode == BattleState::SlideToUnit)
    {
        unit.GetPosition(battle.unk53BF4A, battle.unk53BF4E);
        const auto from = static_cast<i16>(battle.unk53BF4A);
        step = (x - from) / kSlideToShare;
        battle.shake = step;

        if (step == 0 && x != from)
        {
            step = from <= x ? 1 : -1;
            battle.shake = step;
        }

        battle.fieldX = static_cast<u16>(battle.fieldX + step);
    }

    if (battle.slideMode == BattleState::SlideBack)
    {
        step = (Display::Width / 2 - static_cast<i16>(battle.fieldX)) / kSlideBackShare;
        battle.shake = step;

        if (step == 0)
        {
            if (static_cast<int>(battle.fieldX) == x)
            {
                battle.slideMode = 0;
                battle.shake = 0;
                return 1;
            }

            step = static_cast<i16>(battle.fieldX) <= x ? 1 : -1;
            battle.shake = step;
        }

        battle.fieldX = static_cast<u16>(battle.fieldX + step);
    }

    if (step != 0)
    {
        for (u32 role = 0; role < battle.roleCount; ++role)
        {
            this->battleRoles[role].Move(static_cast<i16>(battle.shake), 0);
        }

        for (u32 enemy = 0; enemy < battle.enemyCount; ++enemy)
        {
            this->battleEnemies[enemy].Move(static_cast<i16>(battle.shake), 0);
        }
    }

    return 1;
}

// swd3e.exe 0x00466310: the push of a hit across the field, every unit moved by `shake` (the background too): while
// the push (hitPush) lasts it halves each call, the half added to the pushed distance (fieldX) and moved away
// from the side (left unless mirrored); 0 while the units move. Once it is spent and the hit act is past picture
// slideBackPicture, the units slide back by up to kSlideStep a call and the field darkens by tint steps; 1 then
// (`restart` 1 sets the push again).
int Game::PushUnits(u16 picture, int restart)
{
    BattleState& battle = this->battleState;
    ++battle.pushFrames;

    if (battle.hitPush > 0)
    {
        battle.pushFrames = 0;
        const auto half = static_cast<i16>(battle.hitPush / 2);
        battle.fieldX = static_cast<u16>(battle.fieldX + half);
        battle.hitPush = half;
        battle.shake = battle.mirrored != 0 ? half : -half;
    }

    if (battle.shake != 0)
    {
        this->ShiftUnits(battle.shake);
        return 0;
    }

    if (static_cast<int>(picture) > battle.slideBackPicture)
    {
        const auto pushed = static_cast<i16>(battle.fieldX);

        if (pushed > 0)
        {
            const i16 step = pushed < kSlideStep ? pushed : kSlideStep;
            battle.fieldX = static_cast<u16>(pushed - step);
            battle.shake = battle.mirrored != 0 ? -step : step;
            this->ShiftUnits(battle.shake);
        }

        battle.darkenByTint = 1;
    }

    if (restart == 1)
    {
        battle.hitPush = kPushStart;
    }

    return 1;
}

// The moves of PushUnits (inline twice in the original): the roles, then the enemies, moved across by `shift`.
void Game::ShiftUnits(int shift)
{
    BattleState& battle = this->battleState;

    auto move = [shift](BattleUnit& unit)
    {
        u16 x = 0;
        u16 y = 0;
        unit.GetPosition(x, y);
        unit.SetPosition(static_cast<i16>(x + shift), static_cast<i16>(y));
    };

    for (i16 index = 0; index < static_cast<i32>(battle.roleCount); ++index)
    {
        move(this->battleRoles[static_cast<std::size_t>(index)]);
    }

    for (i16 index = 0; index < static_cast<i32>(battle.enemyCount); ++index)
    {
        move(this->battleEnemies[static_cast<std::size_t>(index)]);
    }
}

// swd3e.exe 0x004695B0: the frame shown shaken by the quake step of quakePhase (QuakeSteps): odd steps move it down,
// even ones up, the band left uncovered black (the frame's top rows are cleared for it); the original blits to the
// primary surface instead of the plain copy. Every quakeStepFrames frames (counted in glowFrames, shared with the panel
// glow) the phase steps on (between 0 and 1 for UnkPanel100); after 10 the quake ends.
void Game::QuakeFrame()
{
    BattleState& battle = this->battleState;
    const auto phase = static_cast<std::size_t>(battle.quakePhase);
    const int step = phase < BattleState::QuakeSteps.size() ? BattleState::QuakeSteps[phase] : 0;
    const bool down = battle.quakePhase % 2 != 0;
    Surface* primary = this->graphics.GetSurface(Graphics::SurfaceId::Primary);

    auto copyRows = [this, primary](int from, int to, int rows)
    {
        if (primary == nullptr || this->offscreen == nullptr)
        {
            return;
        }

        for (int row = 0; row < rows; ++row)
        {
            std::copy_n(this->offscreen->Row(from + row), Display::Width, primary->Row(to + row));
        }
    };

    if (down)
    {
        copyRows(0, step, Display::Height - step);
    }
    else
    {
        copyRows(step, 0, Display::Height - step);
    }

    std::fill_n(this->drawSurface, static_cast<std::size_t>(Display::Width * step), u16{0});
    copyRows(0, down ? 0 : Display::Height - step, step);
    this->graphics.Present();

    if (++battle.glowFrames > battle.quakeStepFrames)
    {
        battle.glowFrames = 0;

        if ((battle.panelFlags & BattleState::UnkPanel100) != 0)
        {
            battle.quakePhase = (battle.quakePhase + 1) % 2;
        }
        else
        {
            ++battle.quakePhase;
        }
    }

    if (battle.quakePhase == static_cast<i32>(BattleState::QuakeSteps.size()))
    {
        battle.quaking = 0;
        battle.quakePhase = 0;
    }
}

// swd3e.exe 0x004626D0: one frame of the hit act `act` of `attacker` on `target` (unit slot `index`, its hit act
// hitActs); 1 once the hit act waits with no second-tier spell picture left (the follow act and the ripple frames
// cleared), 1 too when an act ends. In the ripple battle (rippleHits) a ripple runs instead: walking from the attacker
// to the target (then held for kRippleHold frames) for a moving attack, splashes over the target otherwise. Else the
// picture is read straight from its file (zoomed by its 'CS' values) and drawn, walking to the target for a moving
// attack; reaching it strikes, a command of several turns (enemyCommandTurns) walks again, and the attacker's follow
// act becomes the act's effect: added as a second-tier spell picture where the act was drawn. The target bits:
// Quakes the quake, FollowsUp the nudges and the field fade, UnkTarget4 raises flag 26B8, Strikes (and UnkTarget10)
// take the damage of the hit with its digits (and the spirit and stamina digits of a role's hit).
int Game::HitTargetFrame(BattleUnit& target, BattleUnit& attacker, u32 act, int index)
{
    smooth::Scene scene;
    const auto result = smooth::HitFrame(*this).HitTargetFrameAdvance(target, attacker, act, index, scene);
    scene.Show(this->battleServices);
    return result;
}

// The ripple of HitTargetFrame (inline in the original); see there. The ripple (BattleState::ripple) is refracted over
// the frame at the walked point (kRippleSound once it reaches the target) or over the target.
void Game::RippleHitFrame(BattleUnit& target, BattleUnit& attacker, int index)
{
    smooth::Scene scene;
    smooth::HitFrame(*this).RippleHitFrameAdvance(target, attacker, index, scene);
    scene.Show(this->battleServices);
}

// The picture of HitTargetFrame (inline in the original); see there. The act runs a step (false once it ends: the
// follow act cleared); the picture is mirrored for an enemy attacker and with the sides, placed at the attacker's
// offset from its origin less the act's anchor, or at the target less the act's place, its sound panned by the side.
void Game::DrawHitPicture(BattleUnit& target, BattleUnit& attacker, u32 act, int index)
{
    smooth::Scene scene;
    smooth::HitFrame(*this).DrawHitPictureAdvance(target, attacker, act, index, scene);
    scene.Show(this->battleServices);
}

// The target bits of HitTargetFrame (inline in the original); see there. A strike's damage (capped at kMaxHitDamage,
// none for -1) adds to slotDamage of the slot and becomes hitDamage; resisted (ActFlag14) it shows the target's
// resistance panel and ends the menu.
void Game::TakeHitBits(BattleUnit& target, BattleUnit& attacker, int index)
{
    BattleState& battle = this->battleState;
    BattleMessages& messages = this->battleMessages;
    const auto at = static_cast<std::size_t>(index);
    ActState& hit = battle.hitActs[at];

    if ((hit.target & ActState::Quakes) != 0)
    {
        battle.quakePhase = 0;
        battle.quaking = 1;
    }

    if ((hit.target & ActState::FollowsUp) != 0)
    {
        for (const auto& [bit, nudge] : kHitNudges)
        {
            if ((hit.target & bit) != 0)
            {
                hit.target = static_cast<u16>(hit.target & ~bit);
                target.SetNudge(kHitNudgeFrames, nudge);
            }
        }

        if ((hit.target & ActState::FadesField) != 0)
        {
            battle.StartFieldFade(
                static_cast<i16>(hit.lights[0]),
                static_cast<i16>(hit.lights[1]),
                static_cast<i16>(hit.lights[2]),
                static_cast<i16>(hit.lights[3]),
                static_cast<i16>(hit.lights[4]),
                static_cast<i16>(hit.lights[5]),
                static_cast<i16>(hit.lights[6])
            );
            hit.target = static_cast<u16>(hit.target & ~ActState::FadesField);
            battle.fadingField = 1;
        }
    }

    if ((hit.target & ActState::UnkTarget4) != 0)
    {
        target.RaiseFlag26B8(this->battleState);
        hit.target = 0;
    }

    if ((hit.target & ActState::Strikes) != 0)
    {
        target.RaiseFlag26B8(this->battleState);
    }
    else if ((hit.target & ActState::UnkTarget10) == 0)
    {
        return;
    }

    if (attacker.ActFlag14() == 1)
    {
        battle.panelFlags |= BattleState::ResistancePanel;
        const ObjectRecord* record = target.Data();
        messages.panelUnit = record != nullptr ? &record->data : nullptr;
        battle.result = 0;
    }

    const i16 damage = attacker.kind == 1
                           ? attacker.HitDamage(this->battleServices, target, battle.spiritDamage, battle.staminaDamage)
                           : attacker.EnemyHitDamage(this->battleServices, target);
    battle.lastDamage = damage;

    if (battle.lastDamage >= kMaxHitDamage)
    {
        battle.lastDamage = kMaxHitDamage;
    }

    u16 delay = 0;

    if (battle.lastDamage == -1)
    {
        battle.lastDamage = 0;
    }
    else
    {
        target.SetDamageDigits(this->battleServices, battle.lastDamage);
        target.StepTargetCount(this->battleServices, 1);
        delay = static_cast<u16>(delay + kDigitsDelay);
    }

    if (battle.spiritDamage != 0)
    {
        target.PushTarget(BattleUnit::SpiritGainTarget);
        target.SetDamageDigits(this->battleServices, battle.spiritDamage);
        target.PushTargetAlt(delay);
        target.StepTargetCount(this->battleServices, 1);
        delay = static_cast<u16>(delay + kDigitsDelay);
    }

    if (battle.staminaDamage != 0)
    {
        target.PushTarget(BattleUnit::StaminaGainTarget);
        target.SetDamageDigits(this->battleServices, battle.staminaDamage);
        target.PushTargetAlt(delay);
        target.StepTargetCount(this->battleServices, 1);
    }

    battle.lastSpiritDamage[at] = battle.spiritDamage;
    battle.slotDamage[at] += battle.lastDamage;
    battle.lastStaminaDamage[at] = battle.staminaDamage;
    battle.hitDamage = battle.slotDamage[at];
    battle.hitFading[at] = 0;
    hit.target = 0;
}

// swd3e.exe 0x004632B0: one frame of the hit act `act` of `attacker` on `target` or, with `all` 1, on its whole side
// (unit slot `index`); 1 once the hit act waits with no second-tier spell picture left (a cast that defeats every
// enemy, unk5772F8: once they are all gone), 0 when an act ends. The picture is read straight from its file (counting
// hitAllFrames), placed at the attacker's offset from its origin less the act's anchor, or on a fixed place by the
// side (kAllHitLeftX, kAllHitRightX, kAllHitY: an enemy attacker unmirrored keeps y 0; with unk5772F8 the middle of the
// field), walking along y 0 for a moving attack; an act marked AddsPicture adds its effect as a second-tier picture.
// The target bits work as in HitTargetFrame, on every unit of the side with `all`: the roles hit by a role while
// unk53BF64 and unk53AE7C take the damage at once (Damage), as do all roles while hitsAllRoles; the spirit and stamina
// of a hit are cleared after their digits (so the slots record 0 for them), and the digit delay of the enemies runs
// on from the walk speed without a reset (kept).
int Game::HitAllFrame(BattleUnit& target, BattleUnit& attacker, u32 act, int index, int all)
{
    smooth::Scene scene;
    const auto result = smooth::HitFrame(*this).HitAllFrameAdvance(target, attacker, act, index, all, scene);
    scene.Show(this->battleServices);
    return result;
}

// The place of the picture of HitAllFrame (inline in the original); see there. `delay` takes the walk speed of a
// moving attack, which walks along y 0 from the attacker (slot 0 counts the walk); reaching the end strikes and ends
// the act.
void Game::PlaceAllHit(BattleUnit& attacker, int width, int index, u16& delay)
{
    BattleState& battle = this->battleState;
    ActState& hit = battle.hitActs[static_cast<std::size_t>(index)];
    u16 offsetX = 0;
    u16 offsetY = 0;
    u16 x = 0;
    u16 y = 0;
    attacker.GetOffset(offsetX, offsetY);
    int originX = hit.x;
    u16 anchorX = hit.anchorX;

    if (attacker.kind == 0)
    {
        if (originX != 0)
        {
            originX = width - hit.x;
        }

        if (hit.anchorX != 0)
        {
            anchorX = static_cast<u16>(width - hit.anchorX);
        }
    }

    if (battle.mirrored == 1)
    {
        if (originX != 0)
        {
            originX = width - hit.x;
        }

        if (hit.anchorX != 0)
        {
            anchorX = hit.anchorX;
        }
    }

    // An offset with an anchor places the picture at the attacker's origin; otherwise it stands on a fixed place.
    if ((offsetX != 0 || offsetY != 0) && (anchorX != 0 || hit.anchorY != 0))
    {
        attacker.GetOrigin(x, y);
        x = static_cast<u16>(x + offsetX);
        y = static_cast<u16>(y + offsetY);

        if (x != 0 || y != 0)
        {
            x = static_cast<u16>(x - anchorX);
            y = static_cast<u16>(y - hit.anchorY);
        }
    }
    else
    {
        x = 0;
        y = 0;

        if (battle.mirrored == 1)
        {
            x = static_cast<u16>(Display::Width - width);
        }

        if (originX != 0)
        {
            const bool leftSide = (attacker.kind != 0) != (battle.mirrored == 1);
            x = static_cast<u16>((leftSide ? kAllHitLeftX : kAllHitRightX) - originX);

            if (attacker.kind != 0 || battle.mirrored == 1)
            {
                y = static_cast<u16>(kAllHitY - static_cast<u16>(hit.y));
            }

            if (attacker.kind == 0 && battle.mirrored == 1)
            {
                x = static_cast<u16>(kAllHitLeftX - static_cast<u16>(hit.x));
            }
        }
    }

    battle.hitPictureX = static_cast<i16>(x);
    battle.hitPictureY = static_cast<i16>(y);

    u16 second = 0;

    if (attacker.CommandTarget(delay, second) != 1)
    {
        return;
    }

    u16 fromX = 0;
    u16 fromY = 0;
    attacker.GetDrawPosition(fromX, fromY);
    fromX = static_cast<u16>(fromX - originX);

    if (x == 0)
    {
        x = static_cast<u16>(-originX);
    }

    if (this->WalkLine(
            battle.hitPictureX,
            battle.hitPictureY,
            static_cast<i16>(fromX),
            0,
            static_cast<i16>(x),
            0,
            delay,
            static_cast<i16>(second),
            0
        ) == 1)
    {
        hit.target |= ActState::Strikes;
        hit.waiting = 1;
    }
}

// The nudge of HitAllFrame (inline in the original): with `all` every enemy (a role attacking) or every role,
// otherwise the target.
void Game::NudgeAllHit(BattleUnit& target, BattleUnit& attacker, int all)
{
    BattleState& battle = this->battleState;

    if (all != 1)
    {
        target.SetNudge(kHitNudgeFrames, 1);
        return;
    }

    if (attacker.kind == 1)
    {
        for (u32 index = 0; index < battle.enemyCount; ++index)
        {
            this->battleEnemies[index].SetNudge(kHitNudgeFrames, 1);
        }

        return;
    }

    for (u32 index = 0; index < battle.roleCount; ++index)
    {
        this->battleRoles[index].SetNudge(kHitNudgeFrames, 1);
    }
}

// The flag 26B8 of HitAllFrame (inline in the original): with `all` every role in battle standing (while
// hitsAllRoles) or every enemy standing, otherwise the target.
void Game::RaiseAllHit(BattleUnit& target, int all)
{
    BattleState& battle = this->battleState;

    if (all != 1)
    {
        target.RaiseFlag26B8(this->battleState);
        return;
    }

    if (battle.hitsAllRoles == 1)
    {
        for (u32 index = 0; index < battle.roleCount; ++index)
        {
            BattleRole& role = this->battleRoles[index];

            if (role.GuestControl() != 1 && role.Ghost() != 1 && !role.IsDefeated())
            {
                role.RaiseFlag26B8(this->battleState);
            }
        }

        return;
    }

    for (u32 index = 0; index < battle.enemyCount; ++index)
    {
        BattleEnemy& enemy = this->battleEnemies[index];

        if (!enemy.IsDefeated())
        {
            enemy.RaiseFlag26B8(this->battleState);
        }
    }
}

// The digits of one hit of HitAllFrame (inline in the original): the damage capped at kMaxHitDamage (none for -1),
// then the spirit and stamina digits each kDigitsDelay later, those two cleared once shown. The spirit and stamina
// left are returned.
void Game::ShowAllHitDigits(BattleUnit& unit, i16 damage, u16& delay, i32& spirit, i32& stamina)
{
    BattleState& battle = this->battleState;
    battle.lastDamage = damage;

    if (battle.lastDamage >= kMaxHitDamage)
    {
        battle.lastDamage = kMaxHitDamage;
    }

    if (battle.lastDamage == -1)
    {
        battle.lastDamage = 0;
    }
    else
    {
        unit.SetDamageDigits(this->battleServices, battle.lastDamage);
        unit.StepTargetCount(this->battleServices, 1);
        delay = static_cast<u16>(delay + kDigitsDelay);
    }

    if (battle.spiritDamage != 0)
    {
        unit.PushTarget(BattleUnit::SpiritGainTarget);
        unit.SetDamageDigits(this->battleServices, battle.spiritDamage);
        unit.PushTargetAlt(delay);
        unit.StepTargetCount(this->battleServices, 1);
        delay = static_cast<u16>(delay + kDigitsDelay);
        battle.spiritDamage = 0;
    }

    if (battle.staminaDamage != 0)
    {
        unit.PushTarget(BattleUnit::StaminaGainTarget);
        unit.SetDamageDigits(this->battleServices, battle.staminaDamage);
        unit.PushTargetAlt(delay);
        unit.StepTargetCount(this->battleServices, 1);
        battle.staminaDamage = 0;
    }

    spirit = battle.spiritDamage;
    stamina = battle.staminaDamage;
}

// The strike of HitAllFrame (inline in the original); see there.
void Game::StrikeAllHit(BattleUnit& target, BattleUnit& attacker, int index, int all, u16& delay)
{
    BattleState& battle = this->battleState;
    ActState& hit = battle.hitActs[static_cast<std::size_t>(index)];
    i32 spirit = 0;
    i32 stamina = 0;

    if (all != 1)
    {
        if ((hit.target & ActState::Strikes) != 0)
        {
            target.RaiseFlag26B8(this->battleState);
            battle.quakePhase = 0;
        }

        const i16 damage =
            attacker.kind == 1
                ? attacker.HitDamage(this->battleServices, target, battle.spiritDamage, battle.staminaDamage)
                : attacker.EnemyHitDamage(this->battleServices, target);
        delay = 0;
        this->ShowAllHitDigits(target, damage, delay, spirit, stamina);
        battle.hitDamage += battle.lastDamage;
        return;
    }

    if ((battle.unk53BF64 == 1 && battle.unk53AE7C == 1) || battle.hitsAllRoles == 1)
    {
        for (u32 slot = 0; slot < battle.roleCount; ++slot)
        {
            BattleRole& role = this->battleRoles[slot];

            if (role.GuestControl() == 1 || role.Ghost() == 1 || role.StatusBit13() == 1)
            {
                continue;
            }

            i16 damage = 0;

            if (battle.hitsAllRoles != 1)
            {
                damage = attacker.HitDamage(this->battleServices, role, battle.spiritDamage, battle.staminaDamage);
            }
            else
            {
                if (role.IsDefeated())
                {
                    continue;
                }

                if ((hit.target & ActState::Strikes) != 0)
                {
                    role.RaiseFlag26B8(this->battleState);
                }

                damage = attacker.EnemyHitDamage(this->battleServices, role);
            }

            delay = 0;
            this->ShowAllHitDigits(role, damage, delay, spirit, stamina);
            battle.slotDamage[slot] += battle.lastDamage;
            battle.lastSpiritDamage[slot] = spirit;
            battle.hitDamage = battle.slotDamage[slot];
            battle.lastStaminaDamage[slot] = stamina;

            if (battle.unk53BF64 == 1 && battle.unk53AE7C == 1)
            {
                role.Damage(
                    this->battleServices,
                    battle.slotDamage[slot],
                    static_cast<i16>(spirit),
                    static_cast<i16>(stamina)
                );
            }
        }

        return;
    }

    for (u32 slot = 0; slot < battle.enemyCount; ++slot)
    {
        BattleEnemy& enemy = this->battleEnemies[slot];

        if (enemy.IsDefeated())
        {
            continue;
        }

        if ((hit.target & ActState::Strikes) != 0)
        {
            enemy.RaiseFlag26B8(this->battleState);
        }

        const i16 damage = attacker.HitDamage(this->battleServices, enemy, battle.spiritDamage, battle.staminaDamage);
        this->ShowAllHitDigits(enemy, damage, delay, spirit, stamina);
        battle.slotDamage[slot] += battle.lastDamage;
        battle.lastSpiritDamage[slot] = spirit;
        battle.lastStaminaDamage[slot] = stamina;
        battle.hitDamage = battle.slotDamage[slot];
    }
}

// swd3e.exe 0x004665A0: the hits of the acting unit actingUnit once its spell took effect (SpellCast, not SpellOver);
// 1 once they are over (the hit acts and, after a sweep, the slot totals cleared). The target is the attacker's
// word 29A2. The coin toss (UnkSpell400) runs YinYangAttack instead. A defeated target (Damage 1) flashes the frame
// white and is recorded. See RoleHitFrame and EnemyHitFrame.
int Game::HitFrame()
{
    BattleState& battle = this->battleState;

    if ((battle.spellStage & BattleState::SpellCast) == 0 || (battle.spellStage & BattleState::SpellOver) != 0)
    {
        return 0;
    }

    const bool enemy = battle.actingUnit < BattleState::FirstRoleUnit;
    const int unit = enemy ? battle.actingUnit : battle.actingUnit - BattleState::FirstRoleUnit;

    if ((battle.spellStage & BattleState::UnkSpell400) != 0)
    {
        return this->YinYangAttack(unit) == 1 ? 1 : 0;
    }

    return enemy ? this->EnemyHitFrame(unit) : this->RoleHitFrame(unit);
}

// The white frame of a defeat in HitFrame (inline in the original): the frame filled with 0xFFFF, as many pixels as
// the client area holds (written from the start of the frame without its pitch).
void Game::FlashFrameWhite()
{
    smooth::Scene scene;
    smooth::FillScreen(kWhitePixel, static_cast<std::size_t>(Display::Width) * Display::Height, scene);
    scene.Show(this->battleServices);
}

// The damage of the last hit cleared (inline in 0x0045C010).
void Game::ClearHitDamage()
{
    BattleState& battle = this->battleState;
    battle.hitDamage = 0;
    battle.spiritDamage = 0;
    battle.staminaDamage = 0;
}

// The slot totals of a sweep cleared (inline in 0x0045C010).
void Game::ClearHitTotals()
{
    BattleState& battle = this->battleState;
    battle.slotDamage.fill(0);
    battle.lastSpiritDamage.fill(0);
    battle.lastStaminaDamage.fill(0);
}

// One step of a sweep (inline in 0x0045C010): while fewer than `units` are swept (sweptUnits), every
// sweepGap frames one more is.
void Game::StepSweep(u32 units)
{
    BattleState& battle = this->battleState;

    if (battle.sweptUnits < units)
    {
        ++battle.pushFrames;

        if (battle.pushFrames >= battle.sweepGap)
        {
            ++battle.sweptUnits;
            battle.pushFrames = 0;
        }
    }
}

// The spirit a role drains from its damage (inline in 0x0045C010): with DrainShare giving a positive share, the
// share shows as spirit digits and is added as spirit.
void Game::DrainToSpirit(BattleRole& role, i32& damage)
{
    if (role.DrainShare(this->battleServices, damage) != 1 || damage <= 0)
    {
        return;
    }

    role.PushTarget(BattleUnit::SpiritGainTarget);
    role.SetDamageDigits(this->battleServices, damage);
    role.StepTargetCount(this->battleServices, 1);
    role.Damage(this->battleServices, 0, static_cast<i16>(damage), 0);
}

// A role attacking in HitFrame (inline in the original): its act is its unk4FE5D4. With wholeSide its hit sweeps a
// side (hitsAtOnce 1: the whole side at once, the hits taken by the enemies; otherwise one unit after another as the
// sweep reaches them, until sweepTotal are hit), else one target is hit (hitsAtOnce 1: HitAllFrame on the target
// alone). unk53AE7C makes the party the target side.
int Game::RoleHitFrame(int role)
{
    BattleState& battle = this->battleState;
    BattleRole& attacker = this->battleRoles[static_cast<std::size_t>(role)];
    const auto target = static_cast<i16>(attacker.GetWord29A2());
    const auto at = static_cast<std::size_t>(target);
    const u32 act = static_cast<u32>(battle.unk4FE5D4[static_cast<std::size_t>(role)]);

    if (battle.wholeSide != 0)
    {
        if (battle.hitsAtOnce == 1)
        {
            if (this->HitAllFrame(this->battleEnemies[at], attacker, act, 0, 1) != 1)
            {
                return 0;
            }

            if (battle.unk53AE7C == 0)
            {
                for (u32 index = 0; index < battle.enemyCount; ++index)
                {
                    BattleEnemy& enemy = this->battleEnemies[index];

                    if (enemy.IsDefeated())
                    {
                        continue;
                    }

                    battle.slotTargets[index] = static_cast<i32>(index);
                    battle.hitLanded = 1;
                    battle.sweepHit = 1;

                    if (battle.unk53AF68 == 0 && enemy.Damage(
                                                     this->battleServices,
                                                     battle.slotDamage[index],
                                                     static_cast<i16>(battle.lastSpiritDamage[index]),
                                                     static_cast<i16>(battle.lastStaminaDamage[index])
                                                 ) == 1)
                    {
                        battle.flashing = 1;
                        ++battle.unk53BD88;
                        battle.unk4A7626 = 0;
                        battle.unk502984[index] = static_cast<i32>(index);
                        this->FlashFrameWhite();
                    }

                    this->ClearHitDamage();
                    ++battle.unk53BD80;
                    battle.slotDamage[index] = 0;
                    battle.lastSpiritDamage[index] = 0;
                    battle.lastStaminaDamage[index] = 0;
                }
            }

            this->ClearHitDamage();
            battle.unk53BD80 = 0;
            battle.hitActs.fill(ActState{});
            this->ClearHitTotals();
            return 1;
        }

        if (!this->SweepFromRole(attacker, target, act))
        {
            return 0;
        }

        battle.unk53BD80 = 0;
        battle.sweptUnits = 1;
        battle.hitActs.fill(ActState{});
        this->ClearHitTotals();
        return 1;
    }

    if (battle.hitsAtOnce == 1)
    {
        if (battle.unk53AE7C == 0)
        {
            BattleEnemy& enemy = this->battleEnemies[at];

            if (this->HitAllFrame(enemy, attacker, act, 0, 0) != 1)
            {
                return 0;
            }

            battle.unk4FDF7C = attacker.GetWord29A2();
            battle.lastEnemyHit = static_cast<u16>(target);
            battle.hitLanded = 1;
            battle.slotTargets[at] = target;

            if (battle.unk53AF68 == 0 &&
                enemy.Damage(this->battleServices, battle.hitDamage, battle.spiritDamage, battle.staminaDamage) == 1)
            {
                battle.unk4A7626 = static_cast<u16>(target);
                battle.flashing = 1;
                battle.unk502984[at] = target;
                this->FlashFrameWhite();
            }
        }
        else
        {
            BattleRole& aimed = this->battleRoles[at];

            if (this->HitAllFrame(aimed, attacker, act, 0, 0) != 1)
            {
                return 0;
            }

            aimed.Damage(this->battleServices, battle.hitDamage, battle.spiritDamage, battle.staminaDamage);
            battle.slotTargets[at] = target;
        }

        battle.hitActs.fill(ActState{});
        this->ClearHitDamage();
        return 1;
    }

    if (battle.unk53AE7C == 0)
    {
        BattleEnemy& enemy = this->battleEnemies[at];

        if (this->HitTargetFrame(enemy, attacker, act, 0) != 1)
        {
            return 0;
        }

        battle.unk4FDF7C = attacker.GetWord29A2();
        battle.lastEnemyHit = static_cast<u16>(target);
        battle.hitLanded = 1;
        battle.slotTargets[at] = target;
        this->ApplyDrain(battle.hitDamage, attacker, enemy);

        if (battle.unk53AF68 == 0 &&
            enemy.Damage(this->battleServices, battle.hitDamage, battle.spiritDamage, battle.staminaDamage) == 1)
        {
            battle.unk4A7626 = static_cast<u16>(target);
            battle.flashing = 1;
            battle.unk502984[at] = target;
            this->FlashFrameWhite();
        }

        this->ClearHitDamage();
        battle.hitActs.fill(ActState{});
        return 1;
    }

    BattleRole& aimed = this->battleRoles[at];

    if (this->HitTargetFrame(aimed, attacker, act, 0) != 1)
    {
        return 0;
    }

    battle.unk4FDF7C = attacker.GetWord29A2();
    battle.lastRoleHit = static_cast<u16>(target);

    if (battle.unk53AF68 == 0 &&
        aimed.Damage(this->battleServices, battle.slotDamage[0], battle.spiritDamage, battle.staminaDamage) == 1)
    {
        battle.lastRoleDefeated = static_cast<u16>(target);
        this->FlashFrameWhite();
    }

    this->ClearHitDamage();
    battle.slotDamage[0] = 0;
    battle.hitActs.fill(ActState{});
    return 1;
}

// The sweep of a role's hit in RoleHitFrame (inline in the original): over the enemies (a defeated one widens the
// sweep at once) or the roles in battle (unk53AE7C; not escorts, guests or the ghost; the sweep widens up to the
// roles less the ghost) the sweep has reached, each hit once (slotTargets) takes its slot totals; true once sweepTotal
// are hit.
bool Game::SweepFromRole(BattleRole& attacker, i16 target, u32 act)
{
    BattleState& battle = this->battleState;

    auto hitOne = [this, &battle](BattleUnit& unit, std::size_t slot, bool enemy, i16 aimed)
    {
        battle.slotTargets[slot] = static_cast<i32>(slot);
        battle.hitLanded = 1;
        battle.sweepHit = 1;

        if (battle.unk53AF68 == 0 && unit.Damage(
                                         this->battleServices,
                                         battle.slotDamage[slot],
                                         static_cast<i16>(battle.lastSpiritDamage[slot]),
                                         static_cast<i16>(battle.lastStaminaDamage[slot])
                                     ) == 1)
        {
            ++battle.unk53BD88;

            if (enemy)
            {
                battle.unk4A7626 = static_cast<u16>(aimed);
            }

            battle.unk502984[slot] = static_cast<i32>(slot);
            battle.flashing = 1;
            this->FlashFrameWhite();
        }

        this->ClearHitDamage();
        return ++battle.unk53BD80 >= battle.sweepTotal;
    };

    if (battle.unk53AE7C == 0)
    {
        for (u32 slot = 0; slot < battle.sweptUnits; ++slot)
        {
            BattleEnemy& enemy = this->battleEnemies[slot];

            if (enemy.IsDefeated())
            {
                if (battle.sweptUnits < battle.enemyCount)
                {
                    ++battle.sweptUnits;
                }

                battle.pushFrames = 0;
                continue;
            }

            this->StepSweep(battle.enemyCount);

            if (this->HitTargetFrame(enemy, attacker, act, static_cast<int>(slot)) == 1 &&
                battle.slotTargets[slot] == -1 && hitOne(enemy, slot, true, target))
            {
                return true;
            }
        }

        return false;
    }

    for (u32 slot = 0; slot < battle.sweptUnits; ++slot)
    {
        this->StepSweep(battle.roleCount - battle.unk5770D4);
        BattleRole& role = this->battleRoles[slot];

        if (role.AutoControl() != 1 && role.GuestControl() != 1 && role.Ghost() != 1 &&
            this->HitTargetFrame(role, attacker, act, static_cast<int>(slot)) == 1 && battle.slotTargets[slot] == -1 &&
            hitOne(role, slot, false, target))
        {
            return true;
        }
    }

    return false;
}

// An enemy attacking in HitFrame (inline in the original): its act is its enemyHitActs. Without wholeSide one target
// is hit: a role (hitsAtOnce 1: HitAllFrame on it alone) steps its orb counters by the enemy's command, takes the drain
// before the damage and its DrainShare as spirit, a held aimed role ends the menu; an enemy (unk53AE80) takes the hit.
// With wholeSide the hit sweeps: hitsAtOnce 1 all roles at once, otherwise the enemies (unk53AE80) or the roles the
// sweep has reached, until sweepTargets are hit (counting the defeats in sweepDefeated); the drains of the roles are
// summed and given to the enemy at the end (ApplyDrainSums).
int Game::EnemyHitFrame(int enemy)
{
    BattleState& battle = this->battleState;
    BattleEnemy& attacker = this->battleEnemies[static_cast<std::size_t>(enemy)];
    CounterList& proficiency = this->party.fame.lists[Fame::ProficiencyList];
    const auto target = static_cast<i16>(attacker.GetWord29A2());
    const auto at = static_cast<std::size_t>(target);
    const u32 act = static_cast<u32>(battle.enemyHitActs[static_cast<std::size_t>(enemy)]);

    if (battle.wholeSide == 0)
    {
        if (battle.hitsAtOnce == 1)
        {
            BattleRole& role = this->battleRoles[at];

            if (this->HitAllFrame(role, attacker, act, 0, 0) != 1)
            {
                this->DrainToSpirit(role, battle.hitDamage);
                return 0;
            }

            role.StepOrbCounters(proficiency, attacker.command);
            battle.unk4FDF7C = attacker.GetWord29A2();
            battle.lastRoleHit = static_cast<u16>(target);
            battle.hitLanded = 1;
            this->ApplyDrain(battle.hitDamage, attacker, role);

            if (battle.unk53AF68 == 0 &&
                role.Damage(this->battleServices, battle.hitDamage, battle.spiritDamage, battle.staminaDamage) == 1)
            {
                battle.flashing = 1;
                battle.hitDamage = -1;
                battle.lastRoleDefeated = static_cast<u16>(target);
                this->FlashFrameWhite();
            }

            if (role.IsHeld() == 1 && target == battle.currentUnit - BattleState::FirstRoleUnit)
            {
                battle.result = 0;
            }

            battle.hitDamage = 0;
            battle.hitActs.fill(ActState{});
            battle.spiritDamage = 0;
            battle.staminaDamage = 0;
            return 1;
        }

        if (battle.unk53AE80 != 0)
        {
            BattleEnemy& aimed = this->battleEnemies[at];

            if (this->HitTargetFrame(aimed, attacker, act, 0) != 1)
            {
                return 0;
            }

            battle.unk4FDF7C = attacker.GetWord29A2();
            battle.lastEnemyHit = static_cast<u16>(target);
            battle.hitLanded = 1;

            if (battle.unk53AF68 == 0 && aimed.Damage(this->battleServices, battle.slotDamage[0], 0, 0) == 1)
            {
                battle.unk4A7626 = static_cast<u16>(target);
                this->FlashFrameWhite();
            }

            this->ClearHitDamage();
            battle.slotDamage[0] = 0;
            battle.hitActs.fill(ActState{});
            return 1;
        }

        BattleRole& role = this->battleRoles[at];

        if (this->HitTargetFrame(role, attacker, act, 0) != 1)
        {
            return 0;
        }

        role.StepOrbCounters(proficiency, attacker.command);
        battle.unk4FDF7C = attacker.GetWord29A2();
        battle.lastRoleHit = static_cast<u16>(target);
        battle.hitLanded = 1;
        this->ApplyDrain(battle.slotDamage[0], attacker, role);

        if (battle.unk53AF68 == 0 &&
            role.Damage(this->battleServices, battle.slotDamage[0], battle.spiritDamage, battle.staminaDamage) == 1)
        {
            battle.flashing = 1;
            battle.lastRoleDefeated = static_cast<u16>(target);
            this->FlashFrameWhite();
        }

        if (role.IsHeld() == 1 && target == battle.currentUnit - BattleState::FirstRoleUnit)
        {
            battle.result = 0;
        }

        this->DrainToSpirit(role, battle.slotDamage[0]);
        battle.hitDamage = 0;
        battle.staminaDamage = 0;
        battle.spiritDamage = 0;
        battle.slotDamage[0] = 0;
        battle.hitActs.fill(ActState{});
        return 1;
    }

    if (!this->SweepFromEnemy(attacker, target, act))
    {
        return 0;
    }

    if (battle.hitsAtOnce != 1)
    {
        battle.sweptUnits = 1;
    }

    battle.hitActs.fill(ActState{});
    battle.slotDamage.fill(0);
    return 1;
}

// The drains summed over a sweep of an enemy given to it (inline in EnemyHitFrame), the sums and the sweep count
// cleared.
void Game::EndEnemyDrains(BattleEnemy& attacker)
{
    BattleState& battle = this->battleState;

    if (battle.deferDrains == 1)
    {
        this->ApplyDrainSums(attacker);
    }

    battle.drainedLife = 0;
    battle.drainedSpirit = 0;
    battle.drainedStamina = 0;
    battle.deferDrains = 0;
}

// The sweep of an enemy's hit in EnemyHitFrame (inline in the original); see there. True once sweepTargets are hit.
bool Game::SweepFromEnemy(BattleEnemy& attacker, i16 target, u32 act)
{
    BattleState& battle = this->battleState;
    CounterList& proficiency = this->party.fame.lists[Fame::ProficiencyList];

    auto standing = [](const BattleRole& role)
    {
        return !role.IsDefeated() && role.GuestControl() != 1 && role.Ghost() != 1;
    };

    // A role of the sweep hit (inline in the original): its orb counters, the drain (summed), the damage and the spirit
    // it drains.
    auto hitRole = [this, &battle, &attacker, &proficiency](BattleRole& role, std::size_t slot, i32& defeats)
    {
        role.StepOrbCounters(proficiency, attacker.command);
        battle.slotTargets[slot] = static_cast<i32>(slot);
        battle.hitLanded = 1;
        battle.sweepHit = 1;
        battle.deferDrains = 1;
        this->ApplyDrain(battle.slotDamage[slot], attacker, role);

        if (battle.unk53AF68 == 0 &&
            role.Damage(this->battleServices, battle.slotDamage[slot], battle.spiritDamage, battle.staminaDamage) == 1)
        {
            ++defeats;
            battle.flashing = 1;
            this->FlashFrameWhite();
        }

        this->DrainToSpirit(role, battle.slotDamage[slot]);
    };

    if (battle.hitsAtOnce == 1)
    {
        if (this->HitAllFrame(this->battleRoles[static_cast<std::size_t>(target)], attacker, act, 0, 1) != 1)
        {
            return false;
        }

        for (u32 slot = 0; slot < battle.roleCount; ++slot)
        {
            BattleRole& role = this->battleRoles[slot];

            if (!standing(role))
            {
                continue;
            }

            role.StepOrbCounters(proficiency, attacker.command);

            if (battle.slotDamage[slot] > 0)
            {
                battle.hitDamage = battle.slotDamage[slot];
            }

            battle.deferDrains = 1;
            this->ApplyDrain(battle.slotDamage[slot], attacker, role);
            battle.hitLanded = 1;
            battle.sweepHit = 1;
            battle.slotTargets[slot] = static_cast<i32>(slot);

            if (battle.unk53AF68 == 0 &&
                role.Damage(this->battleServices, battle.slotDamage[slot], battle.spiritDamage, battle.staminaDamage) ==
                    1)
            {
                battle.flashing = 1;
                ++battle.unk53BD88;
                this->FlashFrameWhite();
            }

            this->DrainToSpirit(role, battle.slotDamage[slot]);
            battle.hitDamage = 0;
            battle.spiritDamage = 0;
            battle.staminaDamage = 0;
            battle.slotDamage[slot] = 0;

            if (++battle.unk53BD80 == battle.sweepTargets)
            {
                this->EndEnemyDrains(attacker);
                battle.unk53BD80 = 0;
                return true;
            }
        }

        return false;
    }

    if (battle.unk53AE80 != 0)
    {
        for (u32 slot = 0; slot < battle.sweptUnits; ++slot)
        {
            this->StepSweep(battle.enemyCount);
            BattleEnemy& enemy = this->battleEnemies[slot];

            if (enemy.IsDefeated() || this->HitTargetFrame(enemy, attacker, act, static_cast<int>(slot)) != 1 ||
                battle.slotTargets[slot] != -1)
            {
                continue;
            }

            battle.slotTargets[slot] = static_cast<i32>(slot);
            battle.hitLanded = 1;
            battle.sweepHit = 1;

            if (battle.unk53AF68 == 0 && enemy.Damage(this->battleServices, battle.slotDamage[slot], 0, 0) == 1)
            {
                battle.flashing = 1;
                ++battle.sweepDefeated;
                this->FlashFrameWhite();
            }

            this->ClearHitDamage();
            battle.slotDamage[slot] = 0;

            if (++battle.unk53BD80 == battle.sweepTargets)
            {
                battle.unk53BD80 = 0;
                return true;
            }
        }

        return false;
    }

    for (u32 slot = 0; slot < battle.sweptUnits; ++slot)
    {
        this->StepSweep(battle.roleCount);
        BattleRole& role = this->battleRoles[slot];

        if (!standing(role) || this->HitTargetFrame(role, attacker, act, static_cast<int>(slot)) != 1 ||
            battle.slotTargets[slot] != -1)
        {
            continue;
        }

        hitRole(role, slot, battle.sweepDefeated);
        battle.slotDamage[slot] = 0;

        if (++battle.unk53BD80 >= battle.sweepTargets)
        {
            this->EndEnemyDrains(attacker);
            this->ClearHitDamage();
            battle.unk53BD80 = 0;
            return true;
        }
    }

    return false;
}

// swd3e.exe 0x0045EE20: one frame of the action of enemy `enemy` (its command A) on role `target` (or enemy `target`
// while unk53AE80); 1 once the action is over (a defeated enemy or one without commands at once). 1: the attack (the
// stolen item, the drain and the damage, a defeat flashing white, the enrage damage; on a role it goes on with
// command 0x12C and stays 0); 2: the spell (SpellCast set, the sweep reset, the hit act timed, a ripple battle for
// ActFlag13; the music fades meanwhile) until SpellOver; 4: the enemy's skill act (EnemySkillFrame, then command
// 0x12C); 6: the drain of a role out of battle; 9: the role's defeat, with unit `target` + 4 taken out of the slot
// records and the menu (as in the original); 0xA the fade out; 0xB and 0xC the fade in and out (0xC ends the battle
// once every enemy left); 0xF calls unk53BF2A enemies of act unk53BF28 for 0x19 frames; 0x11 the escape; 100 at once;
// 200 walks to the role; 300 returns home.
// swd32.exe 0x00498370 (unmatched in swd3e) (EH unwind: the new ripple, freed by its unique_ptr)
// swd32.exe 0x0049837B (unmatched in swd3e) (EH handler of 0x00455D60)
int Game::EnemyActionFrame(int enemy, int target)
{
    BattleState& battle = this->battleState;
    BattleServices& services = this->battleServices;
    BattleEnemy& unit = this->battleEnemies[static_cast<std::size_t>(enemy)];
    const auto at = static_cast<std::size_t>(target);

    if (unit.IsDefeated())
    {
        return 1;
    }

    if (unit.GetCommandA() == 0 && unit.GetCommandB() == 0)
    {
        return 1;
    }

    switch (unit.GetCommandA())
    {
        case kEnemyAttack:
            return this->EnemyAttackAction(enemy, unit, target);

        case kEnemySpell:
            return this->EnemySpellAction(enemy, unit, target);

        case kEnemySkill:
            if (this->EnemySkillFrame(unit, this->battleRoles[at]) == 1)
            {
                unit.SetCommand(kAfterAttackCommand);
            }

            return 0;

        case kEnemyDrain:
        {
            BattleRole& role = this->battleRoles[at];

            if (battle.pushFrames == 0)
            {
                unit.StartDrainEffect(services, target, role);
                role.StartDefeat(1);
                role.SetGuarding(1);
                battle.RemoveSlotRecord(target + BattleState::FirstRoleUnit);
                battle.darkRed = kDrainDark;
                battle.darkGreen = kDrainDark;
                battle.darkBlue = kDrainDark;
                battle.unk53BF94 = 1;
                this->DarkenByTint();
                battle.pushFrames = 1;
                battle.sweptUnits = 1;
            }

            if (unit.DrainEffectFrame(services, target) != 1)
            {
                return 0;
            }

            battle.sweptUnits = 1;
            battle.pushFrames = 0;
            battle.unk53BF94 = 0;
            battle.darkenUnit = -1;
            battle.undarkening = 1;
            return 1;
        }

        case kEnemyDefeatRole:
        {
            BattleRole& role = this->battleRoles[at];

            if (role.DefeatFrame(services) != 1)
            {
                return 0;
            }

            role.SetCommand(0);
            const i32 removed = target + kDefeatedUnitShift;
            battle.RemoveSlotRecord(removed);

            if (target >= 0 && target < static_cast<int>(Party::Roles))
            {
                ++battle.defeatedRoles;
            }

            if (battle.currentUnit == removed)
            {
                battle.currentUnit = 0;
            }

            if (battle.actingUnit == removed)
            {
                battle.actingUnit = -1;
                battle.unk53BF74 = 0;
            }

            return 1;
        }

        case kEnemyFadeOut:
            unit.FadeMainOut(services);
            return 0;

        case kEnemyFadeIn:
            if (unit.DrawFadeIn(services, unit) != 1)
            {
                return 0;
            }

            unit.ClearStatus(BattleUnit::GuardingMark);
            unit.SetGuarding(0);
            unit.SetWord2A0A(kFadedWord);
            battle.enemyFaded = 1;
            return 1;

        case kEnemyHide:
            if (unit.DrawFadeOut(services) != 1)
            {
                return 0;
            }

            unit.SetGuarding(1);
            unit.SetStatus(BattleUnit::GuardingMark);
            unit.SetWord2A0A(kFadedWord);
            battle.enemyFaded = 1;

            if (battle.enemiesGone >= battle.enemyCount)
            {
                battle.autoBattle = 0;
                battle.result = kEndCheckState;
            }

            return 1;

        case kEnemyCall:
            return this->CallEnemies(unit);

        case kEnemyEscape:
            if (unit.RunAway(services) != 1)
            {
                return 0;
            }

            unit.SetGuarding(1);
            unit.SetWord2A0A(kFadedWord);
            unit.StartDefeat(1);
            unit.SetDefeatPose(0);
            battle.escapedEnemies[static_cast<std::size_t>(enemy)] = 1;
            battle.enemyFaded = 1;
            return 1;

        case kEnemyDone:
            return 1;

        case kEnemyWalk:
            unit.WalkToTarget(services, this->battleRoles[at]);
            return 0;

        case kEnemyReturn:
            if (unit.ReturnHome(services, this->battleRoles[at], -1, 0) == 1)
            {
                battle.darkenUnit = -1;
                return 1;
            }

            return 0;

        default:
            return 0;
    }
}

// Command 1 of EnemyActionFrame (inline in the original): the attack of enemy `enemy`. On a role: the units slide
// back after it; ailment 0x400 on the role remembers the enemy (the first three roles); a stolen item (itemStolen)
// goes to the enemy's first free slot of stolenItems with its notice (偷竊失敗 without one); the drain and the
// damage (a defeat flashing white), the enemy then on command 0x12C, and the enrage damage of a step-in attack that
// felled the enemy (1 then). On an enemy: the damage, a defeat flashing white.
int Game::EnemyAttackAction(int enemy, BattleEnemy& unit, int target)
{
    BattleState& battle = this->battleState;
    BattleServices& services = this->battleServices;
    const auto at = static_cast<std::size_t>(target);

    if (battle.unk53AE80 == 0)
    {
        battle.slideUnit = enemy;
        battle.slideX = kSlideMiddle;
        BattleRole& role = this->battleRoles[at];

        if (this->EnemyAttackFrame(unit, role) != 1)
        {
            return 0;
        }

        battle.slideX = kSlideMiddle;
        battle.slideMode = BattleState::SlideBack;

        // Roles past the first three write the words after ailment400From in the original (deviations.md).
        if (role.HasEffectBits(kHoldingAilment) == 1 && at < battle.ailment400From.size() &&
            battle.ailment400From[at] == 0)
        {
            battle.ailment400From[at] = static_cast<u16>(enemy + 1);
        }

        if (battle.itemStolen == 1)
        {
            battle.noticeText.fill(' ');
            std::array<u16, 0x20>& stolen = battle.stolenItems[static_cast<std::size_t>(enemy)];
            std::size_t slot = 0;

            while (slot < stolen.size() && stolen[slot] != 0)
            {
                ++slot;
            }

            const u16 item = role.TakeRandomItem(services);

            // A full list writes the item past it in the original (deviations.md): not kept.
            if (slot < stolen.size())
            {
                stolen[slot] = item;
            }

            if (item != 0)
            {
                this->database.LoadObject(battle.lastObject, item);
                this->database.FreeDescription(battle.lastObject);
                const std::string name = ObjectName(battle.lastObject);
                const std::string line = WinFormat(kEnemyStoleFormat, {std::string_view(name)});
                const std::size_t length = std::min(line.size(), battle.noticeText.size() - 1);
                std::copy_n(line.begin(), length, battle.noticeText.begin());
                battle.noticeText[length] = '\0';
                this->audio.PlaySound(kEnemyStoleSound, this->config.soundVolume);
                this->battleMessages.Add(
                    kStealNoticeX,
                    kStealNoticeY,
                    kStealNoticeFrames,
                    std::span<const char>(battle.noticeText.data(), length),
                    BattleMessage::Frame | BattleMessage::Centered
                );
                battle.itemStolen = 0;
            }
            else
            {
                this->battleMessages.Add(
                    kStealNoticeX,
                    kStealNoticeY,
                    kStealNoticeFrames,
                    kEnemyStealFailed,
                    BattleMessage::BlackBar | BattleMessage::Centered
                );
            }
        }

        battle.hitLanded = 1;
        battle.unk53BFC0 = 0;
        battle.lastRoleHit = static_cast<u16>(target);
        this->ApplyDrain(battle.hitDamage, unit, role);

        if (battle.unk53AF68 == 0 &&
            role.Damage(services, battle.hitDamage, battle.spiritDamage, battle.staminaDamage) == 1)
        {
            battle.lastRoleDefeated = static_cast<u16>(target);
            battle.flashing = 1;
            this->FlashFrameWhite();
        }

        battle.staminaDamage = 0;
        battle.spiritDamage = 0;
        battle.darkenUnit = -1;
        role.ClearUnk2FD1();
        unit.SetCommand(kAfterAttackCommand);

        if (battle.unk577360 != 1)
        {
            return 0;
        }

        role.RaiseFlag26B8(battle);
        role.SetDamageDigits(services, battle.enrageDamage);
        role.PushTarget(kEnrageHitPicture);
        role.StepTargetCount(services, 1);
        role.Damage(services, battle.enrageDamage, 0, 0);
        battle.unk577360 = 0;
        battle.enrageDamage = 0;
        return 1;
    }

    BattleEnemy& aimed = this->battleEnemies[at];

    if (this->EnemyAttackFrame(unit, aimed) != 1)
    {
        return 0;
    }

    battle.hitLanded = 1;
    battle.lastRoleHit = static_cast<u16>(target);
    battle.unk53BFC0 = 0;

    if (battle.unk53AF68 == 0 && aimed.Damage(services, battle.hitDamage, 0, 0) == 1)
    {
        battle.unk502984[at] = target;
        battle.lastRoleDefeated = static_cast<u16>(target);
        battle.flashing = 1;
        this->FlashFrameWhite();
    }

    battle.hitDamage = 0;
    battle.darkenUnit = -1;
    return 1;
}

// Command 2 of EnemyActionFrame (inline in the original): the first frame (SpellCast clear) casts the spell: the
// sweep reset, a ripple for ActFlag13, the turns of the command kept for the roles, the enemy's hit act run once
// (half its round limit kept), the standing targets of a whole side counted (otherwise a defeated target gives way
// to the first role that can be hit). The battle music fades a step a frame down to 5 until SpellOver, which
// restores it and ends the action.
int Game::EnemySpellAction(int enemy, BattleEnemy& unit, int target)
{
    BattleState& battle = this->battleState;

    auto hittable = [](const BattleRole& role)
    {
        return !role.IsDefeated() && !role.IsGuarding() && role.Ghost() != 1;
    };

    if ((battle.spellStage & BattleState::SpellCast) == 0)
    {
        battle.spellStage = static_cast<u16>(battle.spellStage | BattleState::SpellCast);
        battle.sweepGap = unit.GetWordDA6();
        battle.slotTargets.fill(-1);
        battle.sweptUnits = 1;
        battle.slotDamage.fill(0);

        if (unit.ActFlag13() == 1)
        {
            battle.rippleHits = 1;
            battle.ripple =
                std::make_unique<ScriptObject>(Display::Width, Display::Height, 0, 0, kRippleGrid, kRippleGrid);
        }

        battle.enemyCommandTurns[0] = unit.CommandTurns();
        ActState& hit = battle.hitActs[0];
        hit.id = battle.enemyHitActs[static_cast<std::size_t>(enemy)];
        hit.action = 0;
        this->acts.Run(hit);
        battle.unk575FD0 = static_cast<u8>(LowByte(hit.counter) >> 1);

        if (battle.wholeSide == 1)
        {
            battle.sweepTargets = 0;

            if (battle.unk53AE80 != 0)
            {
                for (u32 index = 0; index < battle.enemyCount; ++index)
                {
                    if (!this->battleEnemies[index].IsDefeated())
                    {
                        ++battle.sweepTargets;
                    }
                }
            }
            else
            {
                for (u32 index = 0; index < battle.roleCount; ++index)
                {
                    if (hittable(this->battleRoles[index]))
                    {
                        ++battle.sweepTargets;
                    }
                }
            }

            for (u32 index = 0; index < battle.roleCount; ++index)
            {
                battle.enemyCommandTurns[index] = unit.CommandTurns();
            }
        }
        else if (this->battleRoles[static_cast<std::size_t>(target)].IsDefeated())
        {
            for (u32 index = 0; index < battle.roleCount; ++index)
            {
                if (hittable(this->battleRoles[index]))
                {
                    unit.Retarget(static_cast<i16>(index));
                    break;
                }
            }
        }

        hit = ActState{};
    }

    MusicQueue& queue = this->audio.Queue();

    if (battle.musicLevel > kSpellMusicFloor)
    {
        --battle.musicLevel;
        queue.SetVolume(MusicQueue::MaxVolume * battle.musicLevel / Config::MaxVolume);
    }

    if ((battle.spellStage & BattleState::SpellOver) == 0)
    {
        return 0;
    }

    battle.musicLevel = this->config.musicVolume;
    queue.SetVolume(MusicQueue::MaxVolume * this->config.musicVolume / Config::MaxVolume);
    battle.enemyCommandTurns = {};
    battle.hitDamage = 0;
    battle.spiritDamage = 0;
    battle.staminaDamage = 0;

    if (battle.rippleHits == 1)
    {
        battle.ripple.reset();
    }

    battle.rippleHits = 0;
    battle.undarkening = 1;
    return 1;
}

// Command 0xF of EnemyActionFrame (inline in the original): the first frame (bit kCallStarted clear) places the
// called enemies after the others (at kCallX, kCallY and kSecondCallY for the second, mirrored with the sides),
// loads their act record and sets them up hidden by BattleUnit::UnkMark400; the call is over kCallFrames frames later.
int Game::CallEnemies(BattleEnemy& unit)
{
    BattleState& battle = this->battleState;

    if ((battle.pushFrames & kCallStarted) == 0)
    {
        battle.pushFrames = kCallStarted | kCallFrames;
        unit.CommandValues(battle.unk53BF28, battle.unk53BF2A);

        for (u16 called = 0; called < battle.unk53BF2A; ++called)
        {
            // The original sets up enemies past the last one with the field full.
            if (battle.enemyCount >= this->battleEnemies.size())
            {
                break;
            }

            BattleEnemy& enemy = this->battleEnemies[battle.enemyCount];
            enemy.Reset(this->battleServices);
            battle.unk5246E0 = ObjectRecord{};
            UnitPlacement& place = battle.enemyPlaces[battle.enemyCount];
            place.actId = battle.unk53BF28;
            place.x = kCallX;
            place.y = called == 1 ? kSecondCallY : kCallY;
            place.kind = 0;

            if (battle.mirrored == 1)
            {
                enemy.SetFacing(1);
                place.x = static_cast<i16>(Display::Width - place.x);
            }

            this->database.LoadObject(battle.unk5246E0, battle.unk53BF28);
            this->database.FreeDescription(battle.unk5246E0);
            enemy.Setup(place, battle.unk53BF28, this->database, this->acts);
            enemy.SetStatus(BattleUnit::UnkMark400);
            ++battle.enemyCount;
            this->MeasureUnits();
            battle.SortDrawOrder();
            battle.ListEnemyOrder();
        }
    }

    --battle.pushFrames;

    if ((battle.pushFrames & kCallCount) != 0)
    {
        return 0;
    }

    battle.darkenUnit = -1;
    battle.pushFrames = 0;
    battle.unk53BF2A = 0;
    return 1;
}

// swd3e.exe 0x004612D0: one frame of enemy `enemy`. While the fight talk lets the battle run (battleRuns): with no
// unit acting its effects tick and its gauge fills (a full gauge queues its slot record), and unless the battle waits
// (unk53BF64, unk53BF1C) the acting enemy (actingUnit) takes its turn: a held or defeated one ends it; a sweep of the
// whole side aims at the first unit; otherwise it picks a target (EnemyPickTarget) and, once ready, a command
// (EnemyChooseCommand). A chosen command waits for the second gauge (unk50CE6C) and is slotted again; then the enemy
// acts (EnemyActionFrame) until the action is over (EnemyAfterAction). Every frame it is drawn at rest (its acts
// holding while the field is darkened with word 2A74, or while an enemy enrages), its hit act plays, its enrage runs
// and a defeat is taken out; the result of RemoveDefeated is returned (ResetTurn's when a held or defeated acting enemy
// ends its turn, the target when no role is left to aim at).
int Game::EnemyFrame(int enemy)
{
    BattleState& battle = this->battleState;
    BattleServices& services = this->battleServices;
    const auto at = static_cast<std::size_t>(enemy);
    BattleEnemy& unit = this->battleEnemies[at];

    if (this->fightTalk.battleRuns == 1)
    {
        if (battle.actingUnit == -1 && !unit.IsDefeated() && battle.unk53BFC0 == 0 && battle.unk53BFC4 == 0)
        {
            unit.TickEffects(services);

            if (unit.unk2AB0 == 0 && unit.AdvanceGauge(services, battle.enemyGaugeBoost) == 1)
            {
                battle.AddSlotRecord(kEnemySlotRecord, enemy);
                battle.unk53BFC4 = 1;
                battle.unk53BFC0 = 1;
            }
        }

        if (battle.unk53BF64 == 0 && battle.unk53BF1C == 0)
        {
            if (battle.actingUnit == enemy)
            {
                if (unit.IsHeld() == 1 || unit.IsDefeated())
                {
                    const bool held = unit.IsHeld() == 1;
                    battle.unk53BF5C = 0;
                    battle.unk53BF74 = 0;
                    battle.hitsAllRoles = 0;
                    battle.unk53BFC0 = 0;
                    battle.actingUnit = -1;

                    if (held)
                    {
                        battle.heldEnemy = enemy + 1;
                    }

                    return unit.ResetTurn(services);
                }

                if (battle.wholeSide == 1)
                {
                    this->EnemyAimSide(unit);
                }
                else
                {
                    if (battle.enemyTargetPicked == 0)
                    {
                        if (!this->EnemyPickTarget(unit, enemy))
                        {
                            battle.actingUnit = -1;
                            return battle.enemyTarget;
                        }

                        battle.enemyTargetPicked = 1;
                    }

                    if (unit.GetValueAB4() == 0)
                    {
                        this->EnemyChooseCommand(unit, enemy);
                    }
                }
            }

            if (battle.unk50CE6C[at] == 1 && battle.actingUnit == -1 && battle.result == 0 &&
                this->fightTalk.battleRuns == 1 && unit.DrainSecondGauge() == 1)
            {
                battle.unk53BFC0 = 1;
                battle.unk53BFC4 = 1;
                battle.unk50CE6C[at] = 0;
                battle.AddSlotRecord(kEnemySlotRecord, enemy);
            }

            if (battle.actingUnit == enemy && battle.unk50CE6C[at] == 0 && unit.GetValueAB4() == 1)
            {
                this->EnemyActFrame(unit, enemy);
            }
        }
    }

    const bool holding =
        (unit.GetWord2A74() != 0 && (battle.unk53BF94 == 1 || battle.unk53BF98 == 1)) || battle.enraging == 1;
    unit.IdleFrame(services, holding ? 1 : 0);

    if (battle.unk53AE90[at] != -1 && this->PlayHitAct(unit, static_cast<u32>(battle.enemiesHitAct), enemy) == 1)
    {
        battle.unk53AE90[at] = -1;
    }

    unit.EnrageFrame(services);
    const int removed = this->RemoveDefeated(enemy, 0);

    if (removed == 1)
    {
        battle.unk502984[at] = -1;
        battle.slotDamage[at] = 0;
        battle.slotTargets[at] = -1;
        battle.unk4A7626 = kNoAimedEnemy;
        battle.enemyFaded = 1;
        battle.unk53BFDC = 0;
    }

    return removed;
}

// A sweep of the whole side by the acting enemy in EnemyFrame (inline in the original): it aims at the first unit; on
// the enemies the enemies standing count for the sweep (sweepTargets), on the roles the battle waits.
void Game::EnemyAimSide(BattleEnemy& unit)
{
    BattleState& battle = this->battleState;

    if (battle.unk53AE80 != 0)
    {
        battle.sweepTargets = 0;
        battle.enemySweeps = 1;
        unit.Retarget(0);

        for (u32 index = 0; index < battle.enemyCount; ++index)
        {
            if (!this->battleEnemies[index].IsDefeated())
            {
                ++battle.sweepTargets;
            }
        }

        return;
    }

    unit.Retarget(0);
    battle.enemySweeps = 1;
    battle.unk53BFC0 = 1;
}

// A role an enemy may aim at in EnemyPickTarget (inline in the original): standing, not stepped away (unk2AF8), not the
// talisman ghost and not StatusBit13.
bool Game::EnemyMayAim(const BattleRole& role) const
{
    return !role.IsDefeated() && role.unk2AF8 != 1 && role.Ghost() != 1 && role.StatusBit13() != 1;
}

// The target of an acting enemy in EnemyFrame (inline in the original); false when every role has fallen (the enemy
// does not act). An enemy kept from acting (StatusBit5) aims at a role at random when it is the last enemy standing,
// else at another enemy at random (not StatusBit13, without kUnaimedEffect; unk26B8 bit 28 raised). Otherwise, unless
// an order aims it already, the role its enrage element points at, else a role at random: the role the enemy marked
// (unk576FC8, counting down unk576FCC), skipping roles with effect 8 (taunted; all of them: none skipped) and with
// kUnaimedEffect; the same role three times in a row (unk576FC4, unk5774E8) is drawn again while others are left. The
// original draws for ever when nothing qualifies.
bool Game::EnemyPickTarget(BattleEnemy& unit, int enemy)
{
    BattleState& battle = this->battleState;
    i32& target = battle.enemyTarget;

    auto roleAt = [this](i32 index) -> BattleRole&
    {
        return this->battleRoles[static_cast<std::size_t>(index)];
    };

    if (unit.StatusBit5() != 0)
    {
        u32 dying = 0;

        for (u32 index = 0; index < battle.enemyCount; ++index)
        {
            if (this->battleEnemies[index].dying == 1)
            {
                ++dying;
            }
        }

        if (static_cast<i32>(battle.enemyCount) - battle.enemiesGone - static_cast<i32>(dying) == 1)
        {
            for (int tries = 0;; ++tries)
            {
                if (tries >= kHangDraws)
                {
                    Hang();
                }

                if (battle.roleCount != 0)
                {
                    target = static_cast<i32>(this->random.Below(battle.roleCount));
                }

                if (this->EnemyMayAim(roleAt(target)))
                {
                    return true;
                }
            }
        }

        for (int tries = 0;; ++tries)
        {
            if (tries >= kHangDraws)
            {
                Hang();
            }

            if (battle.enemyCount != 0)
            {
                target = static_cast<i32>(this->random.Below(battle.enemyCount));
            }

            BattleEnemy& other = this->battleEnemies[static_cast<std::size_t>(target)];

            if (!other.IsDefeated() && other.StatusBit13() != 1 && other.HasEffectBits(kUnaimedEffect) != 1 &&
                target != enemy)
            {
                break;
            }
        }

        unit.RaiseUnk26B8Bit28();
        battle.unk53AE80 = 1;
        return true;
    }

    if (battle.enemyPendingOrders[static_cast<std::size_t>(enemy)].unk00 != 0)
    {
        return true;
    }

    const u32 chosen = this->PickRoleTarget(unit.EnrageElement());
    target = static_cast<i32>(chosen);

    if (chosen != 0)
    {
        target = static_cast<i32>(chosen) - 1;
        return true;
    }

    const auto partyRoles = static_cast<i32>(battle.roleCount) - battle.unk5770D4;
    i32 taunted = 0;
    battle.unk57736C = 0;

    for (i32 index = 0; index < partyRoles; ++index)
    {
        if (!roleAt(index).IsDefeated() && roleAt(index).HasEffectBits(kTauntEffect) == 1)
        {
            ++taunted;
        }
    }

    if (taunted >= partyRoles - battle.defeatedRoles)
    {
        battle.unk57736C = 1;
        taunted = 0;
    }

    i32 unaimed = 0;

    for (u32 index = 0; index < battle.roleCount; ++index)
    {
        if (this->battleRoles[index].Ghost() != 1 && this->battleRoles[index].HasEffectBits(kUnaimedEffect) == 1)
        {
            ++unaimed;
        }
    }

    for (int tries = 0;; ++tries)
    {
        if (tries >= kHangDraws)
        {
            Hang();
        }

        const i32 fighting =
            static_cast<i32>(battle.roleCount) - battle.escortCount - battle.guestCount - battle.unk5770D4;

        if (static_cast<u32>(battle.defeatedRoles) >= static_cast<u32>(fighting))
        {
            return false;
        }

        if (battle.roleCount != 0)
        {
            target = static_cast<i32>(this->random.Below(battle.roleCount));
        }

        if (battle.unk576FC8 != 0 && battle.unk576FD0 == enemy)
        {
            BattleRole& marked = roleAt(battle.unk576FC8 - 1);

            if (!marked.IsDefeated() && marked.Ghost() != 1)
            {
                target = battle.unk576FC8 - 1;

                if (battle.unk576FCC-- == 1)
                {
                    battle.unk576FD0 = 0;
                    battle.unk576FC8 = 0;
                    unit.ClearUnk26B8(kMarkedRoleBit);
                }

                return true;
            }

            battle.unk576FD0 = 0;
            battle.unk576FC8 = 0;
            battle.unk576FCC = 0;
            unit.ClearUnk26B8(kMarkedRoleBit);
        }

        const i32 others = static_cast<i32>(battle.roleCount) - battle.escortCount - battle.defeatedRoles -
                           battle.unk5770D4 - unaimed - taunted;

        if (static_cast<u32>(others) > 1 && battle.unk576FC4 == target && battle.unk5774E8 >= kSameTargetLimit)
        {
            continue;
        }

        if (battle.unk57736C == 0 && roleAt(target).HasEffectBits(kTauntEffect) == 1)
        {
            continue;
        }

        BattleRole& aimed = roleAt(target);

        if (aimed.IsDefeated() || aimed.unk2AF8 == 1 || aimed.Ghost() == 1 || aimed.StatusBit13() == 1 ||
            aimed.HasEffectBits(kUnaimedEffect) == 1)
        {
            continue;
        }

        if (battle.unk5774E8 != 0 && battle.unk576FC4 == target)
        {
            ++battle.unk5774E8;
        }
        else
        {
            battle.unk5774E8 = 1;
            battle.unk576FC4 = target;
        }

        return true;
    }
}

// A unit `index` of the aimed side is down for the target search of EnemyChooseCommand (inline in the original); the
// units past the last are read past their array by the original: taken as down.
bool Game::AimedDown(i32 index) const
{
    if (this->battleState.unk53AE80 == 1)
    {
        return index < 0 || static_cast<std::size_t>(index) >= this->battleEnemies.size() ||
               this->battleEnemies[static_cast<std::size_t>(index)].IsDefeated();
    }

    return index < 0 || static_cast<std::size_t>(index) >= this->battleRoles.size() ||
           this->battleRoles[static_cast<std::size_t>(index)].IsDefeated();
}

// The command of an acting enemy in EnemyFrame (inline in the original): the order's low byte aims it when its bits
// are set; the catch roll (the first party role's level) makes it run (0x11) or step home (0x12C, its name shown).
// With an order from the fight talk (enemyOrders) the order's bits give the command: 0x8000 a prepared command, 0x4000
// the spell (it waits for its second gauge), 0x2000 the drain; ActFlag1 hits at once, ActFlag0 sweeps; the aim passes
// on from a downed target to the next standing unit (up to unit 0x14). Otherwise a healer may aim at itself or at the
// weakest enemy, and it chooses its command (then it waits for its second gauge with the skill name to show); a
// supporting command aims at itself, a sweep starts, and the target is aimed at when it stands.
void Game::EnemyChooseCommand(BattleEnemy& unit, int enemy)
{
    BattleState& battle = this->battleState;
    BattleServices& services = this->battleServices;
    const auto at = static_cast<std::size_t>(enemy);
    PendingOrder& pending = battle.enemyPendingOrders[at];
    const u16 order = battle.enemyOrders[at];
    pending.unk04 = 0;

    if ((order >> kOrderTargetShift) != 0)
    {
        battle.enemyTarget = order & kOrderTarget;
    }

    const u8 level = this->party.roles[static_cast<std::size_t>(battle.partyRoles[0])].level;
    const int caught = unit.CatchRoll(services, level);

    if (caught == 1)
    {
        unit.Retarget(0);
        unit.SetCommand(kRunCommand);
        unit.SetWord2A0A(kRunWord);
        return;
    }

    if (caught == 0)
    {
        unit.Retarget(0);
        unit.SetCommand(kStepHomeCommand);
        return;
    }

    auto waitSecondGauge = [&battle, &unit, at]
    {
        battle.actingUnit = -1;
        battle.unk50CE6C[at] = 1;
        battle.unk53BF74 = 0;
        battle.unk53BFC4 = 0;
        battle.unk53BFC0 = 0;
        unit.FillSecondGauge();
    };

    auto hitsAtOnce = [&battle, &unit, &pending]
    {
        pending.unk08 = 0;

        if (unit.ActFlag1() == 1)
        {
            pending.unk08 = 1;
            battle.enemyHitsAll = 1;
        }
    };

    if (static_cast<i16>(order) < 0 || (order & (kOrderSpell | kOrderDrain)) != 0)
    {
        if (battle.enemyAimsAtEnemies == 1)
        {
            battle.unk53AE80 = 1;
            battle.enemyAimsAtEnemies = 0;
        }

        if ((order & kOrderPrepared) != 0)
        {
            battle.enemyHitActs[at] = unit.PrepareCommand(this->database);
            battle.darkenUnit = static_cast<i16>(enemy);
        }

        if ((order & kOrderSpell) != 0)
        {
            unit.SetCommand(kEnemySpellCommand);
            battle.darkenUnit = static_cast<i16>(enemy);
            battle.skillNamePending[at] = 1;
            waitSecondGauge();
        }

        if ((order & kOrderDrain) != 0)
        {
            unit.SetCommand(kEnemyDrainCommand);
            battle.darkenUnit = static_cast<i16>(enemy);
        }

        hitsAtOnce();

        if (unit.ActFlag0() == 1)
        {
            pending.unk04 = 1;
            battle.sweepTargets = 0;
        }

        battle.unk53BD48 = 0;

        if (this->AimedDown(battle.enemyTarget))
        {
            while (++battle.enemyTarget <= kLastAimedUnit && this->AimedDown(battle.enemyTarget))
            {
            }
        }

        if (!this->AimedDown(battle.enemyTarget))
        {
            unit.Retarget(static_cast<i16>(battle.enemyTarget));
        }

        return;
    }

    if (unit.StatusBit5() == 0 && unit.CanHeal())
    {
        this->EnemyAimHeal(unit, enemy);
    }

    u32 act = static_cast<u32>(battle.enemyHitActs[at]);
    const int chosen = unit.ChooseCommand(services, this->database, act);
    battle.enemyHitActs[at] = static_cast<i32>(act);

    if (chosen == 1)
    {
        battle.darkenUnit = static_cast<i16>(enemy);
        battle.unk53BD0C = 0;
        waitSecondGauge();
        hitsAtOnce();
        battle.skillNamePending[at] = 1;
    }

    if (unit.CommandSupports() == 1)
    {
        pending.unk00 = 1;
        battle.unk53AE80 = 1;
        battle.enemyTarget = enemy;
    }

    battle.darkenUnit = static_cast<i16>(enemy);

    if (unit.ActFlag0() == 1)
    {
        pending.unk04 = 1;
        battle.sweepTargets = 0;
    }

    if (pending.unk04 == 0 && !this->AimedDown(battle.enemyTarget))
    {
        unit.Retarget(static_cast<i16>(battle.enemyTarget));
    }
}

// The heal of an enemy in EnemyChooseCommand (inline in the original), unless an order aims it already: an enemy of a
// healing kind (UnkKind20) heals itself when weak; otherwise, with a weak enemy standing, it heals the weakest one by
// hit points (measured against the first enemy standing only, as in the original; a weak one of the healing kind is
// taken at once).
void Game::EnemyAimHeal(BattleEnemy& unit, int enemy)
{
    BattleState& battle = this->battleState;
    PendingOrder& pending = battle.enemyPendingOrders[static_cast<std::size_t>(enemy)];

    auto heal = [this, &battle, &unit, &pending](i32 target)
    {
        if (pending.unk00 == 0 && unit.ChooseHeal(this->battleServices))
        {
            battle.unk53AE80 = 1;
            pending.unk00 = 1;
            battle.enemyTarget = target;
        }
    };

    // Without an object the original reads through a null pointer: taken as not of the healing kind.
    auto healer = [](const BattleEnemy& other)
    {
        const ObjectRecord* data = other.Data();
        return data != nullptr && (data->data.kind & ObjectData::UnkKind20) != 0;
    };

    battle.lastDefeatedObject = unit.Data();

    if (healer(unit))
    {
        if (unit.IsWeakEnemy())
        {
            heal(enemy);
        }

        return;
    }

    bool weak = false;

    for (u32 index = 0; index < battle.enemyCount; ++index)
    {
        if (!this->battleEnemies[index].IsDefeated() && this->battleEnemies[index].IsWeakEnemy())
        {
            weak = true;
            break;
        }
    }

    if (!weak)
    {
        return;
    }

    u32 first = 0;
    i32 firstHitPoints = 0;
    i32 maximum = 0;

    do
    {
        this->battleEnemies[first].GetHitPoints(firstHitPoints, maximum);
        ++first;
    } while (this->battleEnemies[first - 1].IsDefeated());

    i32 weakest = static_cast<i32>(first) - 1;

    for (u32 index = 1; index < battle.enemyCount; ++index)
    {
        BattleEnemy& other = this->battleEnemies[index];

        if (other.IsDefeated() || !other.IsWeakEnemy())
        {
            continue;
        }

        battle.lastDefeatedObject = other.Data();

        if (healer(other))
        {
            weakest = static_cast<i32>(index);
            break;
        }

        i32 hitPoints = 0;
        other.GetHitPoints(hitPoints, maximum);

        if (hitPoints < firstHitPoints)
        {
            weakest = static_cast<i32>(index);
        }
    }

    heal(weakest);
}

// The acting enemy of EnemyFrame once it is ready (inline in the original): the battle waits on it, its skill name
// shows once, and it runs EnemyActionFrame until the action is over (EnemyAfterAction).
void Game::EnemyActFrame(BattleEnemy& unit, int enemy)
{
    BattleState& battle = this->battleState;
    const auto at = static_cast<std::size_t>(enemy);
    battle.hitsAllRoles = 1;
    battle.unk53BFC0 = 1;
    unit.MarkAC8();
    battle.darkenUnit = static_cast<i16>(enemy);

    if (battle.skillNamePending[at] == 1)
    {
        if (unit.skillName[0] != '\0')
        {
            this->battleMessages.Add(kEnemySkillX, 0, kEnemySkillFrames, unit.skillName, BattleMessage::Rise);
        }

        battle.skillNamePending[at] = 0;
    }

    if (this->EnemyActionFrame(enemy, static_cast<i16>(unit.GetWord29A2())) == 1)
    {
        this->EnemyAfterAction(unit, enemy);
    }
    else
    {
        battle.hitsAllRoles = 1;
    }
}

// A held role whose escort is out gives it back (inline in EnemyAfterAction): the escort record and order cleared,
// the turn and gauge reset and the escort's object back in the inventory.
void Game::ReturnHeldEscort(BattleRole& role, std::size_t index, bool dropCommand)
{
    BattleState& battle = this->battleState;
    u16& escort = battle.escortCalled[index + static_cast<std::size_t>(BattleState::FirstRoleUnit)];

    if (role.IsHeld() != 1 || escort == 0)
    {
        return;
    }

    escort = 0;
    battle.callingEscort = 0;
    battle.pendingOrders[index].unk00 = 0;

    if (dropCommand)
    {
        role.CancelCommand();
    }

    role.ResetTurn(this->battleServices);
    role.ResetGauge(this->battleServices);
    const auto place = static_cast<std::size_t>(battle.roleCount - battle.escortCount);
    AddOneItem(this->party.inventory, battle.rolePlaces[place].actId, 0, this->database);
}

// The end of an enemy's action in EnemyFrame (inline in the original): the talisman ghost strikes back at the enemy
// that hit role 0 (unless it is low on hit points); a damage transfer (傷害轉移) on the role hit passes the damage to
// the enemy it names (or a random one standing); a held role gives its escort back; the enemy's turn resets, the
// roles face again after a facing change (panel flag 0x80), the turn state clears (the acting unit when an enemy
// acted), the fight talk's countdown steps, the poison spreads, the enemy checks enrage causes 0 and 0x1E (out of
// choices), and a named enemy marks the first party role at a twentieth of its hit points for two turns.
void Game::EnemyAfterAction(BattleEnemy& unit, int enemy)
{
    BattleState& battle = this->battleState;
    BattleServices& services = this->battleServices;
    battle.unk53BF9C = 0;
    const auto target = static_cast<i16>(unit.GetWord29A2());
    BattleRole& ghost = this->battleRoles[static_cast<std::size_t>(battle.ghostSlot)];

    if (ghost.GhostObeys(services) == 1 && target == 0 && battle.unk53AE80 == 0 &&
        this->battleRoles[0].LowHitPoints(0) != 1)
    {
        battle.turnUnit = battle.ghostSlot + BattleState::FirstRoleUnit;
        battle.aimTarget = battle.actingUnit + 1;
        battle.aiTarget = battle.actingUnit + 1;
        ghost.Retarget(static_cast<i16>(battle.actingUnit));
        ghost.GhostAttackCommand(this->database);
        battle.InsertSlotRecord(1, battle.ghostSlot + BattleState::FirstRoleUnit, 0);
        battle.unk53BFC0 = 1;
        battle.unk53BFC4 = 1;
    }

    // A target outside the roles (an enemy, or none) is read past the roles in the original (deviations.md): taken as
    // no role.
    BattleRole* hit = target >= 0 && static_cast<std::size_t>(target) < this->battleRoles.size()
                          ? &this->battleRoles[static_cast<std::size_t>(target)]
                          : nullptr;

    if (hit != nullptr && hit->HasEffectBits(BattleUnit::DamageTransfer) == 1 && battle.hitDamage > 0)
    {
        auto passedTo = static_cast<std::size_t>(hit->Unk34C0());

        if (this->battleEnemies[passedTo].IsDefeated())
        {
            do
            {
                passedTo = this->random.Below(battle.enemyCount);
            } while (this->battleEnemies[passedTo].IsDefeated());
        }

        BattleEnemy& other = this->battleEnemies[passedTo];
        other.PushTarget(kTransferTarget);
        other.RaiseFlag26B8(this->battleState);
        other.SetDamageDigits(services, battle.hitDamage);
        other.StepTargetCount(services, 1);
        other.Damage(services, battle.hitDamage, 0, 0);
    }

    if (unit.GetAA8() == 1)
    {
        for (u32 index = 0; index < battle.roleCount; ++index)
        {
            this->ReturnHeldEscort(this->battleRoles[index], index, true);
        }
    }
    else if (battle.unk53AE80 == 0 && hit != nullptr)
    {
        this->ReturnHeldEscort(*hit, static_cast<std::size_t>(target), false);
    }

    unit.ResetTurn(services);

    if ((battle.panelFlags & kFacingPanel) != 0)
    {
        for (u32 index = 0; index < battle.roleCount; ++index)
        {
            BattleRole& role = this->battleRoles[index];

            if (role.GuestControl() == 0 && !role.IsDefeated())
            {
                role.SetFacingIfAble(0);
            }
        }

        battle.panelFlags &= ~kFacingPanel;
    }

    if (battle.unk5772C8 == 0)
    {
        battle.unk53BF74 = 0;
        battle.unk53BFC0 = 0;

        if (battle.unk53BF98 == 1)
        {
            battle.undarkening = 1;
        }
    }

    battle.enemyTargetPicked = 0;
    battle.hitsAllRoles = 0;
    battle.unk53BFC4 = 0;
    battle.enemyFaded = 1;

    if (battle.actingUnit < BattleState::FirstRoleUnit)
    {
        battle.ClearActing();
        battle.actingUnit = -1;
        battle.unk53AE78 = 0;
    }

    battle.hitDamage = 0;
    battle.unk53BF46 = 0;
    battle.unk53BF48 = 0;
    battle.enemySweeps = 0;
    battle.enemyHitsAll = 0;
    battle.gatheringSweep = 0;
    battle.enemyPendingOrders[static_cast<std::size_t>(enemy)].unk04 = 0;
    battle.sweepTargets = 0;
    battle.spellStage = 0;
    battle.enemyOrders[static_cast<std::size_t>(enemy)] = 0;
    battle.orderSorted = 0;

    if ((this->fightTalk.unk53CCDC & kTalkCountdown) != 0)
    {
        --this->fightTalk.unk53CCDC;
    }

    this->SpreadPoison(1);
    unit.EnrageCheck(services, 0, 0);

    if (unit.OutOfChoices() == 1)
    {
        unit.EnrageCheck(services, kOutOfChoicesCause, 0);
    }

    if (unit.Named() != 1 || battle.unk576FCC != 0)
    {
        return;
    }

    battle.unk576FC8 = 0;

    for (u32 index = 0; index < battle.roleCount; ++index)
    {
        BattleRole& role = this->battleRoles[index];

        if (!role.IsDefeated() && role.PartyMember() != 0 && role.HitPointsAtMost(kMarkedShare))
        {
            battle.unk576FC8 = static_cast<i32>(index) + 1;
            battle.unk576FCC = kMarkedTurns;
            battle.unk576FD0 = enemy;
            unit.RaiseUnk26B8(kMarkedRoleBit);
            break;
        }
    }
}

// swd3e.exe 0x00469F30: enemy `enemy` fell: the roles whose ailment 0x400 it holds are cured of it
void Game::CureHeldBy(int enemy)
{
    BattleState& battle = this->battleState;

    for (std::size_t role = 0; role < battle.ailment400From.size(); ++role)
    {
        if (battle.ailment400From[role] == enemy + 1)
        {
            battle.ailment400From[role] = 0;
            this->battleRoles[role].CureEffects(this->battleServices, BattleUnit::UnkStatus400);
        }
    }
}

// swd3e.exe 0x0046A9A0: a party member with a buff (each role skipped by the enemy of its index fallen, as in the
// original)
bool AnyRoleBuffed(const BattleServices& services)
{
    for (u32 index = 0; index < services.state.roleCount; ++index)
    {
        const BattleRole& role = services.roles[index];

        if (services.enemies[index].IsDefeated() || role.PartyMember() == 0 || role.HasAnyBuff() != 1)
        {
            continue;
        }

        return true;
    }

    return false;
}

// swd3e.exe 0x0046A930: every party member has an effect of `bits` (each role skipped by the enemy of its index
// fallen, as in the original)
bool AllRolesHaveEffect(const BattleServices& services, u32 bits)
{
    for (u32 index = 0; index < services.state.roleCount; ++index)
    {
        const BattleRole& role = services.roles[index];

        if (services.enemies[index].IsDefeated() || role.PartyMember() == 0 || role.HasEffectBits(bits) != 0)
        {
            continue;
        }

        return false;
    }

    return true;
}
}  // namespace swd3e
