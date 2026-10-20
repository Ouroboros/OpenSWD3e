#include "minigame/ascent_game.h"

#include <cmath>
#include <span>

#include "crt/msvc.h"
#include "draw/painter.h"
#include "game/game.h"
#include "input/controls.h"
#include "pic/picture.h"
#include "platform/display.h"
#include "res/act_archive.h"
#include "res/tsw_archive.h"
#include "ui/act_draw.h"

namespace swd3e
{

namespace
{

constexpr int kCameraOffset = 0xF0;      // the player is kept this far below the top of the screen
constexpr i32 kStartX = 0x140;           // all sprites start in the middle column
constexpr i32 kPlayerFromBottom = 0x14;  // the player starts this far above the bottom of the course
constexpr i32 kLeaderY = -0x48;          // the two roles at the top, above the course
constexpr i32 kPartnerY = -0x3C;
constexpr i32 kPartnerAct = 0x2332;  // the partner's act and action are fixed
constexpr i32 kPartnerAction = 0xB;
constexpr u32 kStartWinds = 3;     // the wind starts at -1..1
constexpr u32 kStartCurrents = 8;  // the current at 0..7
constexpr u32 kWindSteps = 3;      // the wind changes by -1..1 a frame
constexpr u32 kCurrentSteps = 4;   // the current by -1..2
constexpr i32 kDriftLimit = 0x10;  // both drifts stay within their limit of 16
constexpr int kDriftShift = 8;     // the drifts move by eighths of a pixel
constexpr i32 kStaminaMax = 0x64;
constexpr i32 kStrokeCost = 4;       // a stroke up or back
constexpr i32 kStrokeUp = -4;        // Up: the speed of a stroke
constexpr i32 kStrokeBack = 2;       // Down
constexpr i32 kStrokeUpStamina = 5;  // Up strokes only above it
constexpr i32 kRestHeld = 3;         // Up held this many frames rests instead
constexpr i32 kRest = -0x400;        // Steer: a vertical move that stops the sprite and rests
constexpr i32 kSideStep = 3;         // Left and Right
constexpr u8 kPressMask = 0xF;       // ButtonState::press: the click count
constexpr i32 kFadeFrames = 0x1E;
constexpr int kSpawnChance = 0x2EE;  // an obstacle appears when rand() % 750 is 1
constexpr int kSpawnRoll = 1;
constexpr i32 kSpawnMargin = 0x3C;  // obstacles appear this far above the screen, 60..579 across
constexpr u32 kSpawnColumns = 0x208;
constexpr i32 kObstacleDepth = 0x230;  // an obstacle this far below the top of the screen is gone
constexpr double kPushRadius = 60.0;   // 0x004AAE38: an obstacle closer than this pushes the player
constexpr int kTileSize = 0x40;        // the background tiles are scrolled within 64 pixels
constexpr int kFadeStep = -1;          // the fade darkens all channels by one step a frame
constexpr u16 kMarkerFile = 0x232F;    // the player's marker on the course gauge
constexpr u16 kMarkerPicture = 0;
constexpr int kMarkerX = 0x16;
constexpr int kMarkerTop = 0x32;
// The two gauges: 200 lines from line 64, the course at column 32, the stamina 24 columns to the right of it.
constexpr int kGaugeTop = 0x40;
constexpr int kGaugeLeft = 0x20;
constexpr int kGaugeLines = 0xC8;
constexpr int kStaminaGaugeLeft = kGaugeLeft + 0x18;
constexpr int kCapInset = 1;  // the caps above and below a gauge, a column in
constexpr int kCapWidth = 0xC;

// A band of a gauge line: its columns from the gauge.s left and its TintPixels levels.
struct GaugeBand
{
    int column = 0;
    int width = 0;
    int red = 0;
    int green = 0;
    int blue = 0;
};

constexpr GaugeBand kCap = {kCapInset, kCapWidth, 8, -8, -8};
constexpr std::array<GaugeBand, 5> kCourseBands = {{
    {0, 2, 8, -8, -8},
    {2, 2, 4, 4, 4},
    {4, 6, 0xC, 0xC, 0xC},
    {10, 2, 4, 4, 4},
    {12, 2, 8, -8, -8},
}};

constexpr GaugeBand kStaminaSpent = {2, 10, -4, -8, 0x10};
constexpr std::array<GaugeBand, 3> kStaminaLeft = {{
    {2, 2, -4, -8, 0x10},
    {4, 6, -4, -4, 0x18},
    {10, 2, -4, -8, 0x10},
}};

constexpr std::array<GaugeBand, 2> kStaminaEdges = {{
    {0, 2, 8, -8, -8},
    {12, 2, 8, -8, -8},
}};

}  // namespace

AscentGame::AscentGame(Game& game)
    : game(game)
{
}

AscentGame::~AscentGame() = default;

// swd3e.exe 0x0049EFF0: a new ascent (the one running is stopped first): the background act, the player's act at the
// bottom of the course, the leader's act at the top (and a copy nobody draws), the partner beside the leader, eight
// parked obstacles, the stamina; the wind -1..1 and the current 0..7, the buttons cleared and the camera on the player
// (`unk00` is kept unread).
bool AscentGame::Start(
    i32 unk00,
    i32 backgroundAct,
    i32 backgroundAction,
    i32 playerAct,
    i32 playerAction,
    i32 leaderAct,
    i32 leaderAction,
    i32 stamina
)
{
    if (this->state != nullptr)
    {
        this->Stop();
    }

    Random& random = this->game.random;
    this->courseHeight = AscentGame::CourseHeight;
    this->state = std::make_unique<AscentState>();
    AscentState& ascent = *this->state;
    ascent.unk00 = unk00;
    ascent.unk04 = 0;
    ascent.wind = static_cast<i32>(random.Below(kStartWinds)) - 1;
    ascent.current = static_cast<i32>(random.Below(kStartCurrents));
    ascent.scrollX = 0;
    ascent.scrollY = 0;
    ascent.flags = AscentState::Running;

    auto newAct = [](i32 act, i32 action)
    {
        auto state = std::make_unique<ActState>();
        ResetPlayback(*state);
        state->id = act;
        state->action = action;
        state->direction = 0;
        return state;
    };

    auto newSprite = [&newAct](i32 x, i32 y, i32 act, i32 action)
    {
        auto sprite = std::make_unique<AscentSprite>();
        sprite->x = x;
        sprite->y = y;
        sprite->velocityX = 0;
        sprite->velocityY = 0;
        sprite->act = newAct(act, action);
        return sprite;
    };

    this->background = newAct(backgroundAct, backgroundAction);
    this->leaderCopy = newAct(leaderAct, leaderAction);
    ascent.player = newSprite(kStartX, this->courseHeight - kPlayerFromBottom, playerAct, playerAction);
    ascent.leader = newSprite(kStartX, kLeaderY, leaderAct, leaderAction);
    ascent.partner = newSprite(kStartX, kPartnerY, kPartnerAct, kPartnerAction);

    for (std::size_t kind = 0; kind < AscentState::Obstacles; ++kind)
    {
        auto obstacle = std::make_unique<AscentSprite>();
        obstacle->x = AscentSprite::Parked;
        obstacle->y = AscentSprite::Parked;
        obstacle->act = std::make_unique<ActState>();
        ResetPlayback(*obstacle->act);
        ascent.obstacles[kind] = std::move(obstacle);
        ascent.obstacleActs[kind] = {};
    }

    ascent.stamina = stamina;
    this->game.controls.buttons = {};
    this->cameraTop = ascent.player->y - kCameraOffset;
    this->FollowPlayer();
    return true;
}

// swd3e.exe 0x0049F4D0: obstacles of `kind` (0..7) take this act; false without an ascent
bool AscentGame::SetObstacleAct(u32 kind, i32 act, i32 action)
{
    if (this->state == nullptr || kind >= AscentState::ObstacleKinds)
    {
        return false;
    }

    this->state->obstacleActs[kind] = {act, action};
    return true;
}

// swd3e.exe 0x0049F3D0: the sprites and the state freed (the background act and the copy stay until the next start)
bool AscentGame::Stop()
{
    this->state.reset();
    return true;
}

// swd3e.exe 0x0049F510: a frame of the ascent: the buttons, the moves and the screen; true once it has ended (the
// fade over), or when none runs
bool AscentGame::Frame()
{
    if (this->state == nullptr)
    {
        return false;
    }

    int moveX = 0;
    int moveY = 0;
    this->ReadControls(moveX, moveY);
    this->Update(moveX, moveY);
    this->Draw();

    if ((this->state->flags & AscentState::Done) != 0)
    {
        this->Stop();
    }

    return this->state == nullptr;
}

// swd3e.exe 0x0049F580: Up strokes up (above 5 stamina) or, held three frames, rests; Down at its first frame backs
// off (with stamina left), overriding Up; Left and Right move across; Escape at its first frame starts the fade
void AscentGame::ReadControls(int& moveX, int& moveY) const
{
    if (this->state == nullptr)
    {
        return;
    }

    const Controls& controls = this->game.controls;
    const ButtonState& up = controls.At(Button::Up);
    const ButtonState& down = controls.At(Button::Down);
    const ButtonState& escape = controls.At(Button::Escape);

    if (up.press != 0)
    {
        if (up.heldFrames >= kRestHeld)
        {
            if (up.heldFrames != 0)
            {
                moveY = kRest;
            }
        }
        else if (this->state->stamina > kStrokeUpStamina)
        {
            moveY = kStrokeUp;
        }
    }

    if (down.heldFrames == 1 && this->state->stamina > 0)
    {
        moveY = kStrokeBack;
    }

    if ((static_cast<u8>(controls.At(Button::Left).press) & kPressMask) != 0)
    {
        moveX = -kSideStep;
    }

    if ((static_cast<u8>(controls.At(Button::Right).press) & kPressMask) != 0)
    {
        moveX = kSideStep;
    }

    if (escape.heldFrames == 1 && (static_cast<u8>(escape.press) & kPressMask) == 1)
    {
        this->state->flags |= AscentState::Ending;
        this->state->fadeFrames = kFadeFrames;
    }
}

// swd3e.exe 0x0049F640: unless fading: the wind and current wander, the player drifts with them and moves by the
// buttons, the roles at the top step towards the player's column, each obstacle drifts and pushes the player away or
// is parked once below the screen, a parked slot may take a new obstacle above the screen (one rand() a slot, then
// its kind and column), and the top reached starts the fade
void AscentGame::Update(int moveX, int moveY)
{
    if (this->state == nullptr || (this->state->flags & AscentState::Ending) != 0)
    {
        return;
    }

    AscentState& ascent = *this->state;
    Random& random = this->game.random;
    ascent.wind += static_cast<i32>(random.Below(kWindSteps)) - 1;

    if (ascent.wind >= kDriftLimit)
    {
        ascent.wind = kDriftLimit;
    }

    if (ascent.wind <= -kDriftLimit)
    {
        ascent.wind = -kDriftLimit;
    }

    ascent.current += static_cast<i32>(random.Below(kCurrentSteps)) - 1;

    if (ascent.current <= 0)
    {
        ascent.current = 0;
    }

    if (ascent.current >= kDriftLimit)
    {
        ascent.current = kDriftLimit;
    }

    ascent.scrollX += ascent.wind / kDriftShift;
    ascent.scrollY += ascent.current / kDriftShift;
    AscentSprite& player = *ascent.player;
    this->Steer(player, ascent.wind / kDriftShift, ascent.current / kDriftShift, false);
    this->Steer(player, moveX, moveY, true);
    player.x += player.velocityX;
    player.y += player.velocityY;
    this->FollowPlayer();
    AscentSprite& leader = *ascent.leader;
    AscentSprite& partner = *ascent.partner;

    if (player.x > leader.x)
    {
        ++leader.x;
        ++partner.x;
    }
    else if (player.x < leader.x)
    {
        --leader.x;
        --partner.x;
    }

    for (std::size_t slot = 0; slot < AscentState::Obstacles; ++slot)
    {
        AscentSprite& obstacle = *ascent.obstacles[slot];

        if (obstacle.y <= AscentSprite::Parked)
        {
            if (this->game.crtRandom.Next() % kSpawnChance == kSpawnRoll && this->cameraTop >= kSpawnMargin)
            {
                const auto kind = static_cast<std::size_t>(random.Below(AscentState::ObstacleKinds));
                obstacle.x = static_cast<i32>(random.Below(kSpawnColumns)) + kSpawnMargin;
                obstacle.y = this->cameraTop - kSpawnMargin;
                obstacle.velocityX = 0;
                obstacle.velocityY = 0;
                obstacle.act->id = ascent.obstacleActs[kind].act;
                obstacle.act->action = ascent.obstacleActs[kind].action;
            }

            continue;
        }

        this->Steer(obstacle, ascent.wind / kDriftShift, ascent.current / kDriftShift, false);
        obstacle.x += obstacle.velocityX;
        obstacle.y += obstacle.velocityY;
        AscentGame::PushAway(player, obstacle);

        if (obstacle.y > this->cameraTop + kObstacleDepth)
        {
            obstacle.x = AscentSprite::Parked;
            obstacle.y = AscentSprite::Parked;
        }
    }

    if (player.y <= 0)
    {
        ascent.flags |= AscentState::Ending;
        ascent.fadeFrames = kFadeFrames;
    }
}

// swd3e.exe 0x0049F920: the course drawn on the frame (or, fading, the frame darkened a step; the last step marks the
// ascent done) and the frame shown
void AscentGame::Draw()
{
    if (this->state == nullptr)
    {
        return;
    }

    Game& owner = this->game;
    owner.drawSurface = owner.painter.LockSurface(*owner.offscreen);
    owner.painter.SetTarget(owner.drawSurface);
    owner.painter.UnlockSurface(*owner.offscreen);
    AscentState& ascent = *this->state;

    if ((ascent.flags & AscentState::Ending) != 0)
    {
        const std::size_t pixels = static_cast<std::size_t>(Display::Width) * Display::Height;
        owner.painter.TintPixels(std::span<u16>(owner.drawSurface, pixels), kFadeStep, kFadeStep, kFadeStep);

        if (--ascent.fadeFrames == 0)
        {
            ascent.flags |= AscentState::Done;
        }
    }
    else
    {
        this->DrawBackground(
            ascent.scrollX % kTileSize - kTileSize,
            (ascent.scrollY - ascent.player->y) % kTileSize - kTileSize
        );
        this->DrawSprites();
        this->DrawGauges();
    }

    owner.ShowFrame();
}

// swd3e.exe 0x0049FA20: the background act's picture tiled over the screen from (left, top)
void AscentGame::DrawBackground(int left, int top)
{
    Game& owner = this->game;
    owner.acts.Run(*this->background);
    const Picture* picture = owner.archive.GetPicture(this->background->frame, this->background->picture);

    if (picture == nullptr || picture->width == 0 || picture->height == 0)
    {
        return;
    }

    for (int y = top; y < Display::Height; y += picture->height)
    {
        for (int x = left; x < Display::Width; x += picture->width)
        {
            owner.painter.SetSource(picture->data.data());
            owner.painter.DrawPicture(x, y, picture->width, picture->height, 0);
        }
    }
}

// swd3e.exe 0x0049FAB0: the obstacles in use, the leader, the partner, then the player
void AscentGame::DrawSprites()
{
    AscentState& ascent = *this->state;

    for (const std::unique_ptr<AscentSprite>& obstacle : ascent.obstacles)
    {
        if (obstacle->y > AscentSprite::Parked)
        {
            this->DrawSprite(*obstacle);
        }
    }

    this->DrawSprite(*ascent.leader);
    this->DrawSprite(*ascent.partner);
    this->DrawSprite(*ascent.player);
}

// Part of 0x0049FAB0: a sprite's act stepped and its picture drawn at its place less the act's anchor, the camera's
// line at the top of the screen.
void AscentGame::DrawSprite(const AscentSprite& sprite)
{
    Game& owner = this->game;
    ActState& act = *sprite.act;
    owner.acts.Run(act);
    const Picture* picture = owner.archive.GetPicture(act.frame, act.picture);

    if (picture == nullptr)
    {
        return;
    }

    owner.painter.SetSource(picture->data.data());
    owner.painter.DrawPicture(sprite.x - act.x, sprite.y - act.y - this->cameraTop, picture->width, picture->height, 0);
}

// swd3e.exe 0x0049FCA0: the course gauge (tinted bands, a cap above and below, the player's marker at its height on
// the course) and the stamina gauge (the spent part from the top in one band, the rest in three)
void AscentGame::DrawGauges()
{
    Game& owner = this->game;
    const Painter& painter = owner.painter;
    const int pitch = painter.ScreenPitch();
    u16* const surface = owner.drawSurface;

    auto tint = [&painter, surface, pitch](int line, int left, const GaugeBand& band)
    {
        u16* const at = surface + static_cast<std::ptrdiff_t>(pitch) * line + left + band.column;
        painter.TintPixels(std::span<u16>(at, static_cast<std::size_t>(band.width)), band.red, band.green, band.blue);
    };

    auto caps = [&tint](int left)
    {
        tint(kGaugeTop - 1, left, kCap);
        tint(kGaugeTop + kGaugeLines, left, kCap);
    };

    caps(kGaugeLeft);

    for (int line = 0; line < kGaugeLines; ++line)
    {
        for (const GaugeBand& band : kCourseBands)
        {
            tint(kGaugeTop + line, kGaugeLeft, band);
        }
    }

    owner.actDrawer.DrawPicture(
        kMarkerFile,
        kMarkerPicture,
        kMarkerX,
        kGaugeLines * this->state->player->y / this->courseHeight + kMarkerTop
    );
    caps(kStaminaGaugeLeft);

    for (int line = 0; line < kGaugeLines; ++line)
    {
        const int row = kGaugeTop + line;

        if (2 * (kStaminaMax - this->state->stamina) > line)
        {
            tint(row, kStaminaGaugeLeft, kStaminaSpent);
        }
        else
        {
            for (const GaugeBand& band : kStaminaLeft)
            {
                tint(row, kStaminaGaugeLeft, band);
            }
        }

        for (const GaugeBand& band : kStaminaEdges)
        {
            tint(row, kStaminaGaugeLeft, band);
        }
    }
}

// swd3e.exe 0x0049FE60: the sprite's speeds ease a step towards 0, then take `moveX` and `moveY` up to their size
// (kRest stops it vertically). For the player (`player`): no vertical move or a rest gains a point of stamina (to
// 100), a stroke costs 4 (to 0). Always true.
bool AscentGame::Steer(AscentSprite& sprite, int moveX, int moveY, bool player)
{
    if (sprite.velocityX != 0)
    {
        sprite.velocityX += sprite.velocityX <= 0 ? 1 : -1;
    }

    if (sprite.velocityY != 0)
    {
        sprite.velocityY += sprite.velocityY <= 0 ? 1 : -1;
    }

    if (moveX != 0)
    {
        sprite.velocityX += moveX;

        if (moveX > 0 ? sprite.velocityX > moveX : sprite.velocityX < moveX)
        {
            sprite.velocityX = moveX;
        }
    }

    auto rest = [this, player]()
    {
        if (player && ++this->state->stamina > kStaminaMax)
        {
            this->state->stamina = kStaminaMax;
        }
    };

    if (moveY == 0)
    {
        rest();
        return true;
    }

    if (moveY == kRest)
    {
        sprite.velocityY = 0;
        rest();
        return true;
    }

    sprite.velocityY += moveY;

    if (moveY > 0 ? sprite.velocityY > moveY : sprite.velocityY < moveY)
    {
        sprite.velocityY = moveY;
    }

    if (player)
    {
        this->state->stamina -= kStrokeCost;

        if (this->state->stamina <= 0)
        {
            this->state->stamina = 0;
        }
    }

    return true;
}

// swd3e.exe 0x0049FF60: an obstacle within 60 pixels pushes the sprite a pixel across (away from it, rightwards when
// level) and up or down by the distance short of 60 (the distance kept as a float there, truncated)
void AscentGame::PushAway(AscentSprite& sprite, const AscentSprite& obstacle)
{
    const auto offsetX = static_cast<double>(sprite.x - obstacle.x);
    const auto offsetY = static_cast<double>(sprite.y - obstacle.y);
    const double distance = std::sqrt(offsetY * offsetY + offsetX * offsetX);

    if (distance > kPushRadius)
    {
        return;
    }

    sprite.x += static_cast<i32>(crt::Ftol(offsetX < 0.0 ? -1.0 : 1.0));
    const double short60 = static_cast<double>(static_cast<float>(distance)) - kPushRadius;

    if (sprite.y <= obstacle.y)
    {
        sprite.y += static_cast<i32>(crt::Ftol(short60));
    }
    else
    {
        sprite.y -= static_cast<i32>(crt::Ftol(short60));
    }
}

// swd3e.exe 0x004A0010: the camera kept on the player
const AscentState* AscentGame::FollowPlayer()
{
    this->cameraTop = this->state->player->y - kCameraOffset;
    return this->state.get();
}

}  // namespace swd3e
