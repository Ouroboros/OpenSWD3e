#pragma once

#include "battle/smooth/smooth_draw.h"
#include "core/types.h"

namespace swd3e
{

class BattleUnit;
class Game;

}  // namespace swd3e

// The rebuild's smooth battle (deviations.md): the hit frames of the battle (battle_hit.cpp) split into the part
// deciding the frame and the part drawing it.
namespace swd3e::smooth
{

class HitFrame
{
public:
    explicit HitFrame(Game& game);

    int PlayHitActAdvance(BattleUnit& unit, u32 act, int index, Scene& scene);

    int FadeHitActAdvance(BattleUnit& unit, u32 act, i32 action, int index, Scene& scene);

    bool DrawHitPictureAdvance(BattleUnit& target, BattleUnit& attacker, u32 act, int index, Scene& scene);

    void RippleHitFrameAdvance(BattleUnit& target, BattleUnit& attacker, int index, Scene& scene);

    void AddAndDrawHitPictures(BattleUnit& target, BattleUnit& attacker, int index, Scene& scene);

    int HitTargetFrameAdvance(BattleUnit& target, BattleUnit& attacker, u32 act, int index, Scene& scene);

    int HitAllFrameAdvance(BattleUnit& target, BattleUnit& attacker, u32 act, int index, int all, Scene& scene);

private:
    Game& game;
};

}  // namespace swd3e::smooth
