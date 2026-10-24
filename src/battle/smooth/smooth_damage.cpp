#include "battle/smooth/smooth_damage.h"

#include <string_view>

#include "battle/battle_damage_constants.h"
#include "battle/smooth/smooth_unit.h"
#include "core/error_box.h"
#include "game/game.h"

namespace swd3e::smooth
{

namespace
{

constexpr int kSpellShadowShare = 3;
constexpr std::size_t kSpellShadowShearShift = 2;
constexpr int kSpellShadowTint = -6;
constexpr int kFaintSpellShadowTint = -1;
constexpr int kSpellShadowLeft = 5;
constexpr int kSpellPanCentre = 0x140;
constexpr int kSpellPan = 0x30;
constexpr u16 kDrainGainDelay = 8;  // the stamina gain shows 8 frames after the healing
constexpr i32 kEnemyAttackAction = 0x28;
constexpr i32 kEnemyAttackAction1 = 0x30;  // unk2A8C 1
constexpr i32 kEnemyAttackAction2 = 0x31;  // unk2A8C 2
constexpr u16 kSecondEnemyAttack = 2;
constexpr std::size_t kEnemyFlightAct = 1;  // the slot act a flying follow-up uses
constexpr u16 kEnemyStrikeBits = ActState::Strikes | ActState::UnkTarget10;
constexpr u16 kStatusEvade = 8;  // UnkStatus80: the target evades 8 times in 10
constexpr i16 kEnemyFadeEnd = -0x20;
constexpr i16 kEnemyFadeStep = 4;
constexpr u32 kGaugeRestRoll = 0x78;  // the gauge gains 10..129 once the attack is over
constexpr u16 kGaugeRestBase = 10;
constexpr u32 kStandsAsideFlag = 0x10000000;  // unk26B8: the enemy turns before it fades in at the target (swd3e)
constexpr u32 kStepsAway = 0x10000;           // unk26B8: the attack fells the enemy (swd3e)
constexpr i16 kStepAwayTintFrom = 0xF;
constexpr i16 kStepAwayFrames = 0x1F;
constexpr u8 kStepAwayPose = 2;
constexpr u32 kEvadeStatus = 0x80;         // status bit 7: the target evades with kStatusEvade
constexpr i16 kEvadedEnrageCause = 0x1B;   // an evaded strike checks this enrage cause
constexpr u32 kFliesMirrored = 0x1000000;  // unk26B8: the follow-up flies mirrored once
}  // namespace

DamageFrame::DamageFrame(Game& game)
    : game(game)
{
}

// What Game::DrainSpellFrame decides of the frame; the draws into `scene`.
int DamageFrame::DrainSpellFrameAdvance(BattleRole& role, BattleUnit& target, Scene& scene)
{
    Game& game = this->game;
    BattleState& battle = game.battleState;
    BattleServices& services = game.battleServices;
    ScenePainter painter(game.battleServices, scene);
    ActState& act = role.slotActs[BattleUnit::UnkSlot8];
    role.unk2AAC = 1;
    act.id = role.actId;
    act.action = role.magicAction;

    if (game.acts.Run(act) == 0)
    {
        return 0;
    }

    role.picture = game.archive.GetPicture(act.frame, act.picture);

    // A missing picture is read through a null pointer in the original.
    if (role.picture == nullptr)
    {
        return 0;
    }

    const Picture& picture = *role.picture;
    painter.SetSource(picture.data.data());
    role.actOffsetX = static_cast<u16>(act.x);
    u32 flags = act.flags;
    role.actAnchorX = act.anchorX;

    if (role.facing == 1)
    {
        flags ^= Painter::Mirror;
        role.actOffsetX = static_cast<u16>(picture.width - static_cast<u16>(act.x));

        if (act.anchorX != 0)
        {
            role.actAnchorX = static_cast<u16>(picture.width - act.anchorX);
        }
    }

    const int shadowHeight = picture.height / kSpellShadowShare;
    DrawOptions& options = painter.Options();
    options.scaleHeight = shadowHeight;
    options.shear = picture.height >> kSpellShadowShearShift;
    const int tint = role.autoControl == 1 ? kFaintSpellShadowTint : kSpellShadowTint;
    options.tintRed = tint;
    options.tintGreen = tint;
    options.tintBlue = tint;
    game.audio.PlaySound(act.view, game.config.soundVolume);
    const int x = role.placement.x - static_cast<i16>(role.actOffsetX);
    game.audio.SetSoundPan(act.view, x >= kSpellPanCentre ? kSpellPan : -kSpellPan);
    act.view = 0;
    role.shadowMode = (flags & (Painter::RleSource | Painter::Mirror | Painter::Flip)) | Painter::Shadow;
    painter.DrawPicture(
        x - kSpellShadowLeft,
        role.placement.y - shadowHeight,
        picture.width,
        picture.height,
        role.shadowMode
    );
    painter.DrawPicture(x, role.placement.y - act.y, picture.width, picture.height, flags);
    battle.darkRed = static_cast<i16>(act.red);
    battle.darkGreen = static_cast<i16>(act.green);
    battle.darkBlue = static_cast<i16>(act.blue);
    game.DarkenByTint();

    if (battle.darkRed != 0 || battle.darkGreen != 0 || battle.darkBlue != 0)
    {
        battle.unk53BF94 = 1;
    }

    if ((act.target & (ActState::Strikes | ActState::FollowsUp)) == 0)
    {
        return 0;
    }

    u16 targetX = 0;
    u16 targetY = 0;
    target.GetDrawPosition(targetX, targetY);
    ++role.absorbFrames;
    const int fromX = static_cast<i16>(role.actAnchorX) + x;
    const int fromY = role.placement.y + static_cast<i16>(act.anchorY) - act.y;

    if (UnitFrame(role).Fly(
            services,
            target,
            role.slotActs[0],
            0,
            role.command.castAct,
            fromX,
            fromY,
            static_cast<i16>(role.command.target),
            static_cast<i16>(role.command.unk28),
            1,
            scene
        ) != 1)
    {
        return 0;
    }

    target.RaiseFlag26B8(this->game.battleState);
    i16 spirit = 0;
    i16 stamina = 0;
    i32 damage = role.HitDamage(services, target, spirit, stamina);
    battle.lastDamage = damage;

    if (damage >= kSpellMaxDamage)
    {
        damage = kSpellMaxDamage;
        battle.lastDamage = damage;
    }

    battle.hitDamage += damage;
    target.SetDamageDigits(services, damage);
    target.StepTargetCount(services, 1);
    battle.lastDamage = -battle.lastDamage;
    role.PushTarget(BattleUnit::HealTarget);
    role.SetDamageDigits(services, battle.lastDamage);
    role.PushTargetAlt(0);
    role.StepTargetCount(services, 1);
    role.PushTarget(BattleUnit::StaminaGainTarget);
    role.SetDamageDigits(services, battle.hitDamage);
    role.PushTargetAlt(kDrainGainDelay);
    role.StepTargetCount(services, 1);
    role.Damage(services, battle.lastDamage, 0, static_cast<i16>(battle.hitDamage));
    role.absorbFrames = 0;
    role.slotActs[0] = ActState{};
    act = ActState{};
    return 1;
}

// What Game::MagicAttackFrame decides of the frame; the draws into `scene`.
int DamageFrame::MagicAttackFrameAdvance(BattleRole& role, BattleUnit& target, Scene& scene)
{
    Game& game = this->game;
    BattleState& battle = game.battleState;
    BattleServices& services = game.battleServices;
    ScenePainter painter(game.battleServices, scene);

    if (role.unk2A74 != 0 || role.unk2AD8 == 1)
    {
        return 0;
    }

    ActState& magic = role.slotActs[BattleUnit::UnkSlot8];
    role.unk2AAC = 1;
    magic.hold = 0;

    if ((magic.target & ActState::Holds) != 0)
    {
        magic.hold = 1;
    }

    magic.id = role.actId;
    magic.action = role.magicAction;
    UnitFrame(role).Action(services, target, magic, scene);

    if ((magic.target & ActState::Recoils) != 0)
    {
        if (magic.effectA != 0)
        {
            role.stage |= BattleUnit::Recoiling;
            role.recoilAct.id = magic.effectA;
            role.recoilAct.action = magic.effectQ;
        }

        if ((magic.target & ActState::Holds) != 0)
        {
            magic.hold = 1;
        }

        magic.target = static_cast<u16>(magic.target & ~ActState::Recoils);
        magic.effectA = 0;
        magic.effectQ = 0;
    }

    if ((role.stage & BattleUnit::Recoiling) != 0 &&
        UnitFrame(role).Recoil(services, role.recoilAct, static_cast<i16>(magic.anchorY), scene) == 1)
    {
        magic.target = 0;
        magic.hold = 0;
        role.stage &= ~BattleUnit::Recoiling;
    }

    if ((magic.target & ActState::Quakes) != 0)
    {
        battle.quaking = 1;
        battle.quakePhase = 0;
    }

    if ((magic.target & ActState::UnkTarget4) != 0)
    {
        magic.target = 0;
        role.stage |= BattleUnit::FollowingUp;
        target.RaiseFlag26B8(this->game.battleState);
    }

    ActState& follow = role.slotActs[0];

    if ((magic.target & ActState::FollowsUp) != 0)
    {
        if ((magic.target & ActState::FadesField) != 0)
        {
            battle.fadingField = 1;
            battle.StartFieldFade(
                static_cast<i16>(magic.lights[0]),
                static_cast<i16>(magic.lights[1]),
                static_cast<i16>(magic.lights[2]),
                static_cast<i16>(magic.lights[3]),
                static_cast<i16>(magic.lights[4]),
                static_cast<i16>(magic.lights[5]),
                static_cast<i16>(magic.lights[6])
            );
            magic.target = static_cast<u16>(magic.target & ~ActState::FadesField);
        }

        magic.target = static_cast<u16>(magic.target & ~ActState::FollowsUp);
        role.stage |= BattleUnit::FollowingUp;
        role.slotTint = 0;
        magic.target = 0;
        follow = ActState{};
    }

    if ((magic.target & ActState::Strikes) != 0)
    {
        role.stage |= BattleUnit::FollowingUp;
        role.slotTint = 0;
        magic.target = 0;
        role.Strike(services, target, 0);
        follow = ActState{};
    }

    battle.darkRed = static_cast<i16>(magic.red);
    battle.darkGreen = static_cast<i16>(magic.green);
    battle.darkBlue = static_cast<i16>(magic.blue);
    game.DarkenByTint();

    if (battle.darkRed != 0 || battle.darkGreen != 0 || battle.darkBlue != 0)
    {
        battle.unk53BF94 = 1;
    }

    if ((role.stage & BattleUnit::FollowingUp) == 0)
    {
        return 0;
    }

    follow.action = 0;
    follow.id = role.command.castAct;

    if (magic.effectA != 0)
    {
        follow.id = magic.effectA;
        follow.action = magic.effectQ;
    }

    if (follow.id == 0)
    {
        follow.waiting = 1;
    }
    else if ((role.command.flags & UnitCommand::TargetsUnit) != 0)
    {
        const int fromX = role.placement.x + static_cast<i16>(role.actAnchorX) - static_cast<i16>(role.actOffsetX);
        const int fromY = static_cast<i16>(role.standAct.anchorY) + role.placement.y - role.standAct.y;

        if (UnitFrame(role).Fly(
                services,
                target,
                follow,
                0,
                follow.id,
                fromX,
                fromY,
                static_cast<i16>(role.command.target),
                static_cast<i16>(role.command.unk28),
                0,
                scene
            ) != 1)
        {
            return 0;
        }

        follow.target |= ActState::Strikes;
        role.standAct.waiting = 1;
        follow.waiting = 1;
        role.slotTint = 0;
    }
    else
    {
        role.AimAction(services, target, follow, static_cast<i16>(magic.anchorX), static_cast<i16>(magic.anchorY));
    }

    if ((follow.target & ActState::Quakes) != 0)
    {
        battle.quaking = 1;
        battle.quakePhase = 0;
    }

    if ((follow.target & ActState::UnkTarget4) != 0)
    {
        target.RaiseFlag26B8(this->game.battleState);
        follow.target = 0;
    }

    if ((follow.target & ActState::Strikes) != 0)
    {
        role.slotTint = 0;
        battle.proficiencyBonus = 0;
        follow.target = 0;
        role.Strike(services, target, 0);
    }

    // The original reads the current or the shadow picture through a null pointer when there is none.
    if (role.picture != nullptr)
    {
        painter.SetSource(role.picture->data.data());
    }

    if (follow.waiting != 1)
    {
        if ((role.stage & BattleUnit::Recoiling) == 0 && role.picture != nullptr)
        {
            const DrawArgument argument =
                role.shadowPicture != nullptr ? PaletteArgument(*role.shadowPicture) : DrawArgument{};
            painter.DrawPicture(
                static_cast<i16>(role.aimX),
                static_cast<i16>(role.aimY),
                role.picture->width,
                role.picture->height,
                role.shadowMode,
                argument
            );
        }

        return 0;
    }

    if ((role.stage & BattleUnit::Recoiling) != 0)
    {
        return 0;
    }

    magic.effectA = 0;
    role.attackAct.effectA = 0;

    if (follow.id != 0 && (role.command.flags & UnitCommand::TargetsUnit) == 0)
    {
        if (role.slotTint > kMagicFadeEnd)
        {
            DrawOptions& options = painter.Options();
            options.tintRed = role.slotTint;
            options.tintGreen = role.slotTint;
            options.tintBlue = role.slotTint;

            if (role.picture != nullptr)
            {
                painter.DrawPicture(
                    static_cast<i16>(role.aimX),
                    static_cast<i16>(role.aimY),
                    role.picture->width,
                    role.picture->height,
                    role.shadowMode,
                    PaletteArgument(*role.picture)
                );
            }

            role.slotTint = static_cast<i16>(role.slotTint - kMagicFadeStep);

            if (role.held == 1)
            {
                role.slotTint = static_cast<i16>(role.slotTint + kMagicFadeStep);
            }

            return 0;
        }

        role.slotTint = 0;
    }

    if (magic.waiting != 1 || !role.images.Empty() || role.slotTint != 0)
    {
        return 0;
    }

    magic = ActState{};
    role.attackAct = ActState{};
    role.recoilAct = ActState{};
    follow = ActState{};
    role.trails = {};
    role.targets.fill(BattleUnit::NoTarget);
    role.slotTint = 0;
    role.unk2AB0 = 0;
    role.stage = 0;
    role.unk26D4 = 0;
    battle.criticalAttack = 0;
    ++battle.attacksSinceCritical;
    return 1;
}

// What Game::EnemyAttackFrame decides of the frame; the draws into `scene`.
int DamageFrame::EnemyAttackFrameAdvance(BattleUnit& enemy, BattleUnit& target, Scene& scene)
{
    Game& game = this->game;
    BattleState& battle = game.battleState;
    BattleServices& services = game.battleServices;
    ScenePainter painter(game.battleServices, scene);
    // The evade chance of the strikes (the drain kind of the first strike carries into the second, as in the original).
    u16 chance = 0;

    if (enemy.unk2A74 != 0)
    {
        return 0;
    }

    ActState& attack = enemy.standAct;
    ActState& follow = enemy.castAct;
    enemy.unk2AAC = 1;
    attack.hold = 0;

    if (enemy.held == 1)
    {
        attack.hold = 1;
    }

    if ((attack.target & ActState::Holds) != 0)
    {
        attack.hold = 1;
    }

    if ((enemy.command.flags & UnitCommand::UnkFlag8) != 0 && enemy.fadedIn == 0)
    {
        if (UnitFrame(enemy).DrawFadeOutAdvance(services, scene) == 0)
        {
            return 0;
        }

        if ((enemy.unk26B8 & kStandsAsideFlag) != 0)
        {
            enemy.SetFacing(1);
        }

        if (UnitFrame(enemy).DrawFadeInAdvance(services, target, scene) != 1)
        {
            return 0;
        }

        enemy.fadedIn = 1;
    }

    attack.id = enemy.actId;
    attack.action = kEnemyAttackAction;

    if (enemy.unk2A8C != 0)
    {
        attack.action = enemy.unk2A8C == kSecondEnemyAttack ? kEnemyAttackAction2 : kEnemyAttackAction1;
    }

    if (UnitFrame(enemy).Action(services, target, attack, scene) == 0)
    {
        const std::string_view name(enemy.displayName.data(), enemy.displayName.size());
        ErrorBox(
            services.errors,
            u8"Attack Act",
            u8"[%s]QQ:%d號錯誤!!",
            name.substr(0, name.find('\0')),
            attack.action
        );
        return 0;
    }

    auto clearAttack = [&enemy, &services]
    {
        enemy.standAct = ActState{};
        enemy.castAct = ActState{};
        enemy.recoilAct = ActState{};
        enemy.attackAct = ActState{};
        enemy.slotActs[kEnemyFlightAct] = ActState{};
        enemy.trails = {};
        enemy.targets.fill(BattleUnit::NoTarget);
        enemy.stage = 0;
        enemy.unk2AB0 = 0;
        enemy.fadedIn = 0;
        enemy.nudge = 0;
        enemy.gauge = static_cast<u16>(enemy.gauge + services.random.Below(kGaugeRestRoll) + kGaugeRestBase);
    };

    // swd3e: unk26B8 bit 16: the act shows once, then the enemy darkens away under itself and falls.
    if ((enemy.unk26B8 & kStepsAway) != 0)
    {
        attack = ActState{};

        // A missing picture is read through a null pointer in the original.
        if (enemy.picture != nullptr)
        {
            painter.SetSource(enemy.picture->data.data());
            enemy.shadowMode = enemy.drawFlags & (Painter::RleSource | Painter::Mirror | Painter::Flip);
            DrawOptions& options = painter.Options();
            options.tintRed = enemy.glowLevel;

            if (enemy.glowLevel >= kStepAwayTintFrom)
            {
                options.tintGreen = enemy.glowLevel - kStepAwayTintFrom;
                options.tintBlue = enemy.glowLevel - kStepAwayTintFrom;
            }

            painter.DrawPicture(
                static_cast<i16>(enemy.actX),
                static_cast<i16>(enemy.actY),
                enemy.picture->width,
                enemy.picture->height,
                enemy.shadowMode | Painter::TintUnder
            );
        }

        if (++enemy.glowLevel <= kStepAwayFrames)
        {
            return 0;
        }

        target.GetDrawPosition(enemy.aimX, enemy.aimY);
        enemy.SetPosition(static_cast<i16>(enemy.aimX), static_cast<i16>(enemy.aimY));
        enemy.glowLevel = 0;
        enemy.dying = 1;
        enemy.fixedPose = kStepAwayPose;
        enemy.data->data.hitPoints = 0;
        battle.unk577360 = 1;
        clearAttack();
        return 1;
    }

    if ((attack.target & ActState::Recoils) != 0)
    {
        if (attack.effectA != 0)
        {
            enemy.recoilAct = ActState{};
            enemy.stage |= BattleUnit::Recoiling;
            enemy.recoilAct.id = attack.effectA;
            enemy.recoilAct.action = attack.effectQ;
        }

        if ((attack.target & ActState::Holds) != 0)
        {
            attack.hold = 1;
        }

        attack.target = static_cast<u16>(attack.target & ~ActState::Recoils);
        attack.effectA = 0;
        attack.effectQ = 0;
    }

    if ((enemy.stage & BattleUnit::Recoiling) != 0 &&
        UnitFrame(enemy).Recoil(services, enemy.recoilAct, static_cast<i16>(attack.anchorY), scene) == 1)
    {
        attack.target = 0;
        attack.hold = 0;
        enemy.stage &= ~BattleUnit::Recoiling;
    }

    if ((attack.target & ActState::FollowsUp) != 0)
    {
        if ((attack.target & ActState::FadesField) != 0)
        {
            battle.fadingField = 1;
            battle.StartFieldFade(
                static_cast<i16>(attack.lights[0]),
                static_cast<i16>(attack.lights[1]),
                static_cast<i16>(attack.lights[2]),
                static_cast<i16>(attack.lights[3]),
                static_cast<i16>(attack.lights[4]),
                static_cast<i16>(attack.lights[5]),
                static_cast<i16>(attack.lights[6])
            );
            attack.target = static_cast<u16>(attack.target & ~ActState::FadesField);
        }

        attack.target = static_cast<u16>(attack.target & ~ActState::FollowsUp);
        enemy.stage |= BattleUnit::FollowingUp;

        // swd3e: a follow-up flying to the unit is queued at once.
        if ((enemy.command.flags & UnitCommand::TargetsUnit) != 0)
        {
            target.GetDrawPosition(enemy.aimX, enemy.aimY);
            enemy.AddQueuedAction(
                enemy.command.castAct,
                0,
                static_cast<i16>(enemy.placement.x + enemy.actAnchorX - enemy.actOffsetX),
                static_cast<i16>(attack.anchorY + enemy.placement.y - attack.y),
                static_cast<i16>(enemy.aimX),
                static_cast<i16>(enemy.aimY),
                static_cast<i16>(enemy.command.target),
                enemy.command.unk28,
                0,
                1
            );
        }

        follow = ActState{};
    }

    if ((attack.target & ActState::Quakes) != 0)
    {
        battle.quaking = 1;
        battle.quakePhase = 0;
    }

    if ((attack.target & ActState::SlidesTo) != 0)
    {
        battle.StartSlideTo(0);
        attack.target = static_cast<u16>(attack.target & ~ActState::SlidesTo);
        enemy.stage &= ~BattleUnit::FollowingUp;
    }

    if ((attack.target & ActState::SlidesBack) != 0)
    {
        battle.StartSlideBack(0);
        attack.target = static_cast<u16>(attack.target & ~ActState::SlidesBack);
        enemy.stage &= ~BattleUnit::FollowingUp;
    }

    if ((attack.target & ActState::UnkTarget4) != 0)
    {
        enemy.stage |= BattleUnit::FollowingUp;
        attack.target = 0;
        target.RaiseFlag26B8(battle);
    }

    // A strike of the act or its follow-up (inline twice in the original): the evasion roll unless already rolled,
    // the Strikes bit raising the target's flag 26B8; an evaded strike checks the enemy's enrage cause 0x1B, a hit
    // counts its damage (at most 9999) and shows it, a hit that drains at most hit points counting on the target. True
    // once the hit was shown with such a drain.
    auto strike = [&](u16 bits)
    {
        battle.lastDamage = 0;
        int evaded = 0;

        if ((enemy.status & kEvadeStatus) != 0)
        {
            chance = kStatusEvade;
        }

        if (enemy.evadeRolled == 0)
        {
            evaded = target.RollEvade(services, chance);
        }

        if ((bits & ActState::Strikes) != 0)
        {
            target.RaiseFlag26B8(battle);
        }

        if (evaded != 0)
        {
            enemy.EnrageCheck(services, kEvadedEnrageCause, 0);
            return false;
        }

        i32 damage = enemy.EnemyHitDamage(services, target);
        battle.lastDamage = damage;

        if (static_cast<i16>(damage) >= kSpellMaxDamage)
        {
            damage = kSpellMaxDamage;
            battle.lastDamage = damage;
        }

        battle.hitDamage += damage;

        if (damage == -1)
        {
            return false;
        }

        target.SetDamageDigits(services, damage);
        chance = enemy.DrainKind();

        if (chance > BattleUnit::DrainedHitPoints)
        {
            return false;
        }

        target.StepTargetCount(services, 1);
        return true;
    };

    if ((attack.target & kEnemyStrikeBits) != 0)
    {
        enemy.stage |= BattleUnit::FollowingUp;
        enemy.slotTint = 0;
        follow = ActState{};
        strike(attack.target);
        enemy.evadeRolled = 1;
        attack.target = 0;
    }

    battle.darkRed = static_cast<i16>(attack.red);
    battle.darkGreen = static_cast<i16>(attack.green);
    battle.darkBlue = static_cast<i16>(attack.blue);
    game.DarkenByTint();

    if (battle.darkRed != 0 || battle.darkGreen != 0 || battle.darkBlue != 0)
    {
        battle.unk53BF98 = 1;
    }

    if ((enemy.stage & BattleUnit::FollowingUp) == 0)
    {
        return 0;
    }

    if (follow.waiting == 1 && battle.HitPicturesDone())
    {
        if (follow.id == 0 || (enemy.command.flags & UnitCommand::TargetsUnit) != 0 || enemy.slotTint <= kEnemyFadeEnd)
        {
            if (attack.waiting == 1 && enemy.images.Empty())
            {
                clearAttack();
                return 1;
            }

            return 0;
        }

        // A missing picture is read through a null pointer in the original.
        if (enemy.picture != nullptr && enemy.shadowPicture != nullptr)
        {
            painter.SetSource(enemy.picture->data.data());
            DrawOptions& options = painter.Options();
            options.tintBlue = enemy.slotTint;
            options.tintGreen = enemy.slotTint;
            options.tintRed = enemy.slotTint;

            if ((enemy.shadowMode & ActState::NoBlend) != 0)
            {
                options.tintBlue = 0;
                options.tintGreen = 0;
                options.tintRed = 0;
                enemy.slotTint = kEnemyFadeEnd;
            }

            painter.DrawPicture(
                static_cast<i16>(enemy.aimX),
                static_cast<i16>(enemy.aimY),
                enemy.picture->width,
                enemy.picture->height,
                enemy.shadowMode,
                PaletteArgument(*enemy.shadowPicture)
            );
        }

        enemy.slotTint = static_cast<i16>(enemy.slotTint - kEnemyFadeStep);

        if (enemy.held == 1)
        {
            enemy.slotTint = static_cast<i16>(enemy.slotTint + kEnemyFadeStep);
        }

        return 0;
    }

    follow.action = 0;
    follow.id = enemy.command.castAct;

    if (attack.effectA != 0)
    {
        follow.id = attack.effectA;
    }

    if (follow.id == 0)
    {
        follow.waiting = 1;
        return 0;
    }

    if ((enemy.command.flags & UnitCommand::TargetsUnit) != 0)
    {
        if ((enemy.unk26B8 & kFliesMirrored) != 0)
        {
            enemy.facing = 1;
            const int flown = UnitFrame(enemy).Queued(services, target, 0, scene);
            enemy.facing = 0;

            if (flown != 1)
            {
                return 0;
            }

            follow.waiting = 1;
            enemy.unk26B8 &= ~kFliesMirrored;
        }
        else
        {
            if (UnitFrame(enemy).Queued(services, target, 0, scene) != 1)
            {
                return 0;
            }

            follow.waiting = 1;
        }
    }
    else
    {
        enemy.AimAction(services, target, follow, static_cast<i16>(attack.anchorX), static_cast<i16>(attack.anchorY));
    }

    // A missing picture is read through a null pointer in the original.
    if (enemy.picture != nullptr)
    {
        painter.SetSource(enemy.picture->data.data());
        painter.DrawPicture(
            static_cast<i16>(enemy.aimX),
            static_cast<i16>(enemy.aimY),
            enemy.picture->width,
            enemy.picture->height,
            enemy.shadowMode,
            PaletteArgument(*enemy.picture)
        );
    }

    if ((follow.target & ActState::AddsPicture) != 0)
    {
        follow.target = static_cast<u16>(follow.target & ~ActState::AddsPicture);
        battle.AddHitPicture(
            services.errors,
            follow.effectA,
            follow.effectQ,
            static_cast<i16>(enemy.aimX + follow.anchorX),
            static_cast<i16>(enemy.aimY + follow.anchorY),
            0
        );
    }

    DrawHitPictures(services, scene);
    follow.target = static_cast<u16>(follow.target | battle.HitBits());

    if ((follow.target & kEnemyStrikeBits) == 0)
    {
        return 0;
    }

    enemy.slotTint = 0;
    strike(follow.target);
    enemy.evadeRolled = 1;
    follow.target = 0;
    return 0;
}

}  // namespace swd3e::smooth
