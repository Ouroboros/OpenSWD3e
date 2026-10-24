#pragma once

#include "battle/smooth/smooth_draw.h"
#include "core/types.h"

namespace swd3e
{

class Game;
class BattleRole;
class BattleEnemy;
class BattleUnit;

}  // namespace swd3e

// The rebuild's smooth battle (deviations.md): the spell and attack frames of battle_damage.cpp split into the part
// deciding the frame and the part drawing it.
namespace swd3e::smooth
{

class DamageFrame
{
public:
    explicit DamageFrame(Game& game);

    int DrainSpellFrameAdvance(BattleRole& role, BattleUnit& target, Scene& scene);

    int MagicAttackFrameAdvance(BattleRole& role, BattleUnit& target, Scene& scene);

    int EnemyAttackFrameAdvance(BattleUnit& enemy, BattleUnit& target, Scene& scene);
    int EnemySkillFrameAdvance(BattleUnit& enemy, BattleUnit& target, Scene& scene);

private:
    Game& game;
};

}  // namespace swd3e::smooth
