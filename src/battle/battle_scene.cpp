#include <algorithm>
#include <array>
#include <span>

#include "battle/battle_frame_constants.h"
#include "battle/smooth/smooth_battle.h"
#include "game/game.h"
#include "pic/picture_ops.h"

namespace swd3e
{

namespace
{

}  // namespace

// The background shifted by `shake` (right for a positive value), with its animation shifted along.
void Game::ShakeBattleBackground(int shake)
{
    BattleState& battle = this->battleState;

    if (shake > 0)
    {
        ShiftPicture(battle.background.data, ShiftMode::Right, shake);
    }
    else
    {
        ShiftPicture(battle.background.data, ShiftMode::Left, -shake);
    }
}

// swd3e.exe 0x0045B8D0: the battle background of the frame. Unless the field is darkened it is drawn (shifted by
// `shake` with its animations, or with them in place and the grey band of the battle start growing from BandRow), the
// quake is cleared and the flash applied. While the field is darkened it is drawn darker (a darkened copy a level a few
// frames, or the background and its animations with the tint stepping down), and when the darkening ends it is undone
// (the tint stepping back up, or a copy a level lighter) until a tint channel reaches 0.
void Game::DrawBattleBackground(int shake)
{
    smooth::Scene scene;
    smooth::BattleFrame(*this).DrawBattleBackgroundAdvance(shake, scene);
    scene.Show(this->battleServices);
}

// swd3e.exe 0x004655A0: the field is darkened by tint steps (swd32 drew darkened copies of it here)
void Game::DarkenByTint()
{
    this->battleState.darkenByTint = 1;
}
}  // namespace swd3e
