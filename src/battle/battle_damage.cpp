#include <string_view>

#include "battle/battle_damage_constants.h"
#include "battle/smooth/smooth_damage.h"
#include "game/game.h"

namespace swd3e
{

namespace
{

constexpr std::u8string_view kNoRetreat = u8"無法撤退!!";  // 0x004BF540
constexpr int kRetreatX = 0x118;
constexpr int kRetreatY = 0xA;
constexpr i16 kRetreatFrames = 0x32;
constexpr u16 kNoRetreatSound = 0x8C;

// DrainSpellFrame.

// MagicAttackFrame: a waiting follow-up darkens by kMagicFadeStep a frame down to kMagicFadeEnd.

// EnemyAttackFrame.

}  // namespace

// swd3e.exe 0x00467E50: the drain of `attacker`'s command applied after a hit on `target` of `amount` damage. A life
// drain heals the attacker by it; a spirit or stamina drain takes it (at most what the target has, shown on the target)
// to the attacker, records it as the spirit or stamina damage of the hit and clears `amount`. Each drain is added to
// its sum (drainedLife, drainedSpirit, drainedStamina); while deferDrains is set the attacker's number and gain wait
// for them (HitFrame).
void Game::ApplyDrain(i32& amount, BattleUnit& attacker, BattleUnit& target)
{
    BattleState& battle = this->battleState;
    BattleServices& services = this->battleServices;
    i32 value = amount;

    if (value == 0)
    {
        return;
    }

    u16 maximum = 0;
    u16 left = 0;
    const u16 drain = attacker.DrainKind();

    if (drain == BattleUnit::DrainedHitPoints)
    {
        attacker.PushTarget(BattleUnit::HealTarget);
        value = -value;
        battle.drainedLife += value;

        if (battle.deferDrains == 0)
        {
            attacker.SetDamageDigits(services, value);
            attacker.StepTargetCount(services, 1);
            attacker.Damage(services, value, 0, 0);
        }

        return;
    }

    if (drain != BattleUnit::DrainedSpirit && drain != BattleUnit::DrainedStamina)
    {
        return;
    }

    if (drain == BattleUnit::DrainedSpirit)
    {
        target.GetSpirit(maximum, left);
    }
    else
    {
        target.GetStamina(maximum, left);
    }

    if (static_cast<i16>(left) - value <= 0)
    {
        value = static_cast<i16>(left);
        target.SetDamageDigits(services, value);
    }

    attacker.PushTarget(
        drain == BattleUnit::DrainedSpirit ? BattleUnit::SpiritGainTarget : BattleUnit::StaminaGainTarget
    );

    if (battle.deferDrains == 0)
    {
        attacker.SetDamageDigits(services, value);
        attacker.StepTargetCount(services, 1);

        if (drain == BattleUnit::DrainedSpirit)
        {
            attacker.Damage(services, 0, static_cast<i16>(value), 0);
        }
        else
        {
            attacker.Damage(services, 0, 0, static_cast<i16>(value));
        }
    }

    if (drain == BattleUnit::DrainedSpirit)
    {
        battle.drainedSpirit -= value;
        battle.spiritDamage = static_cast<i16>(-value);
    }
    else
    {
        battle.drainedStamina -= value;
        battle.staminaDamage = static_cast<i16>(-value);
    }

    amount = 0;
}

// swd3e.exe 0x00468000: the drains summed over a hit on many units (drainedLife, drainedSpirit, drainedStamina) shown
// on `unit` and given to it by its drain kind
void Game::ApplyDrainSums(BattleUnit& unit)
{
    BattleState& battle = this->battleState;
    BattleServices& services = this->battleServices;
    const u16 drain = unit.DrainKind();

    if (drain == BattleUnit::DrainedHitPoints)
    {
        unit.SetDamageDigits(services, battle.drainedLife);
        unit.StepTargetCount(services, 1);
        unit.Damage(services, battle.drainedLife, 0, 0);
    }
    else if (drain == BattleUnit::DrainedSpirit)
    {
        unit.SetDamageDigits(services, battle.drainedSpirit);
        unit.StepTargetCount(services, 1);
        unit.Damage(services, 0, static_cast<i16>(battle.drainedSpirit), 0);
    }
    else if (drain == BattleUnit::DrainedStamina)
    {
        unit.SetDamageDigits(services, battle.drainedStamina);
        unit.StepTargetCount(services, 1);
        unit.Damage(services, 0, 0, static_cast<i16>(battle.drainedStamina));
    }
}

// swd3e.exe 0x00469860: the retreat command of role `role`. When the role can act, a party whose first role is held
// or whose battle forbids it gets the "無法撤退!!" message; otherwise the turn state is cleared for the retreat.
void Game::TryRetreat(int role)
{
    BattleState& battle = this->battleState;

    if (this->battleRoles[static_cast<std::size_t>(role)].ActFlag11() != 1)
    {
        return;
    }

    if (!this->battleRoles[0].MarkBit11Clear() || (battle.panelFlags & BattleState::NoRetreat) != 0)
    {
        this->battleMessages
            .Add(kRetreatX, kRetreatY, kRetreatFrames, kNoRetreat, BattleMessage::BlackBar | BattleMessage::Centered);
        this->audio.PlaySound(kNoRetreatSound, this->config.soundVolume);
        return;
    }

    battle.unk53BFC0 = 1;
    battle.unk53BFC4 = 1;
    battle.unk53BF5C = 0;
    battle.undarkening = 0;
    battle.turnUnit = 0;
    battle.currentUnit = 0;
    battle.actingUnit = -1;
    battle.result = 0;
    battle.enemiesGone = static_cast<u8>(battle.enemyCount);
    battle.endFade = 1;
}

// swd3e.exe 0x004840A0: one frame of `role`'s drain spell (its magic act, slot act 8) at its place over its shadow;
// the act's colour darkens the field. Once the act strikes or follows up, the cast act flies from the target back
// into the role (one more step each frame); when it arrives the hit lands on the target (at most kMaxDamage) and the
// role regains it: the negated damage shows as healing and the hit's damage as a stamina gain 8 frames later, and the
// role takes both. 1 (both acts cleared) then.
int Game::DrainSpellFrame(BattleRole& role, BattleUnit& target)
{
    smooth::Scene scene;
    const auto result = smooth::DamageFrame(*this).DrainSpellFrameAdvance(role, target, scene);
    scene.Show(this->battleServices);
    return result;
}

// swd3e.exe 0x004854B0: one frame of `role`'s magic attack (its magic act in slot act 8, drawn by DrawAction; the
// act's colour darkens the field); 1 once it is over. The magic act's target bits play the effect in the recoil act
// (holding the act), quake, raise flag 26B8 of the target, fade the field, start the follow-up (the cast act or the
// effect, slot act 0) or strike. The follow-up flies to the target (TargetsUnit; the original takes the start from
// the stand act and marks the stand act waiting) or is placed on it, and may quake, raise flag 26B8 or strike; once
// it waits it darkens by 8 a frame (not while held) before the acts, the trails and the targets are cleared.
int Game::MagicAttackFrame(BattleRole& role, BattleUnit& target)
{
    smooth::Scene scene;
    const auto result = smooth::DamageFrame(*this).MagicAttackFrameAdvance(role, target, scene);
    scene.Show(this->battleServices);
    return result;
}

// swd3e.exe 0x00487FE0 (a method of the unit in the original): one frame of `enemy`'s attack on `target`; 1 once it
// is over (its gauge then gains 10..129). An UnkFlag8 command fades the enemy out and in at the target first (turning
// first with unk26B8 bit 28). The attack act (by unk2A8C) is drawn, an error box when it cannot run; under unk26B8
// bit 16 the enemy then darkens away and falls. Its target bits play the effect in the recoil act, fade the field,
// start the follow-up (queued at once when it flies to the unit), quake, slide the units to it or back, raise flag
// 26B8 of the target or strike; the act's colour darkens the field. The follow-up flies (TargetsUnit, mirrored once
// under unk26B8 bit 24) or is placed on the target, may add a hit picture and strike; once it waits and the hit
// pictures are done it darkens by 4 a frame (the original takes the palette from the shadow picture) before the acts,
// the trails and the targets are cleared.
int Game::EnemyAttackFrame(BattleUnit& enemy, BattleUnit& target)
{
    smooth::Scene scene;
    const auto result = smooth::DamageFrame(*this).EnemyAttackFrameAdvance(enemy, target, scene);
    scene.Show(this->battleServices);
    return result;
}

}  // namespace swd3e
