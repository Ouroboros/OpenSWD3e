#include <array>
#include <cstring>
#include <string_view>

#include "battle/battle_frame_constants.h"
#include "battle/smooth/smooth_battle.h"
#include "crt/msvc.h"
#include "draw/paint_record.h"
#include "game/game.h"
#include "ui/ui_draw.h"

namespace swd3e
{

// swd3e.exe 0x00464240: the battle panels. The talisman ghost gets its own (portrait, spirit, turn gauge and its
// order); every other role in battle that is neither auto-controlled, a guest nor guarding gets its panel by place:
// the highlight of the current role (once its name grew in), the second gauge with the debug overlay, the name or its
// gauge (growing in once its turn comes), the portrait and effect icons, the block gauge (with the arrow at a full
// fury), and the hit point, spirit and stamina gauges and numbers moving towards their values. Then the meter; with
// the automatic battle on, its box slides in.
void Game::DrawRolePanels()
{
    smooth::Scene scene;
    smooth::BattleFrame(*this).DrawRolePanelsAdvance(scene);
    scene.Show(this->battleServices);
}

}  // namespace swd3e
