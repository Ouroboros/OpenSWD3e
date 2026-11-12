#pragma once

#include <array>
#include <memory>

#include "core/types.h"
#include "res/act_state.h"

namespace swd3e
{

class Game;

// A sprite of the swim (0x14 bytes in the original): its place on the course, its speed and its act.
struct SwimSprite
{
    static constexpr i32 Parked = -0x400;  // `x` and `y` of an obstacle slot not in use

    i32 x = 0;                      // +0x00
    i32 y = 0;                      // +0x04 course pixels, 0 at the top
    i32 velocityX = 0;              // +0x08
    i32 velocityY = 0;              // +0x0C
    std::unique_ptr<ActState> act;  // +0x10
};

// The act of one kind of obstacle (8 bytes in the original, set by SwimGame::SetObstacleAct).
struct SwimObstacleAct
{
    i32 act = 0;     // +0x00
    i32 action = 0;  // +0x04
};

// The state of a running swim (0x90 bytes, 0x00579A3C).
struct SwimState
{
    static constexpr std::size_t ObstacleKinds = 8;
    static constexpr std::size_t Obstacles = 8;

    // Bits of `flags`.
    enum Flag : u32
    {
        Running = 0x80000000u,  // set at the start; nothing reads it
        Ending = 0x40000000u,   // the screen fades out (Escape, or the top reached)
        Done = 0x10000000u,     // the fade is over: the swim ends
    };

    i32 unk00 = 0;    // +0x00 the first argument of SwimGame::Start; nothing reads it
    i32 unk04 = 0;    // +0x04 0; nothing reads it
    i32 wind = 0;     // +0x08 the sideways drift (eighths of a pixel), a random walk within -16..16
    i32 current = 0;  // +0x0C the downward drift (eighths), a random walk within 0..16
    i32 scrollX = 0;  // +0x10 the background's offset, moved by the drifts
    i32 scrollY = 0;  // +0x14
    i32 stamina = 0;  // +0x18 0..100: rises while the role rests, swimming costs it
    std::array<SwimObstacleAct, ObstacleKinds> obstacleActs = {};  // +0x1C
    i32 fadeFrames = 0;                                            // +0x5C frames of the fade left (Ending)
    u32 flags = 0;                                                 // +0x60 SwimState::Flag
    std::unique_ptr<SwimSprite> player;   // +0x64 the controlled role, from the bottom of the course
    std::unique_ptr<SwimSprite> leader;   // +0x68 a role at the top, keeping to the player's column
    std::unique_ptr<SwimSprite> partner;  // +0x6C a role beside it (act 0x2332 action 11), moving with it
    std::array<std::unique_ptr<SwimSprite>, Obstacles> obstacles;  // +0x70 drifting down the course
};

// The swim game of swd3e (talk command 210 and the debug key F7; 0x0049EFF0..0x004A0027): the controlled role,
// jumped from the dragon boat into the river, swims up a course of 800 pixels to the shore against a downward
// current and a sideways wind, past obstacles drifting down, with two roles waiting at the top. Up swims up (holding
// it rests), Down backs off, Left and Right move across, Escape gives up. The scene as the user identified it; no
// event of the Talk files uses command 210, so only F7 starts it.
class SwimGame
{
public:
    static constexpr int CourseHeight = 0x320;  // 0x00579A30
    // How talk command 210 and the debug key F7 start it: all its pictures from act 0x2332 (actions 0 the background,
    // 1 the player, 2 the leader, 3..6 the obstacles, two kinds each), full stamina.
    static constexpr i32 SceneAct = 0x2332;
    static constexpr i32 SceneUnk00 = 0x64;
    static constexpr i32 BackgroundAction = 0;
    static constexpr i32 PlayerAction = 1;
    static constexpr i32 LeaderAction = 2;
    static constexpr i32 FullStamina = 0x64;
    static constexpr std::array<i32, 8> ObstacleActions = {3, 4, 5, 6, 3, 4, 5, 6};

    explicit SwimGame(Game& game);
    ~SwimGame();

    bool Start(
        i32 unk00,
        i32 backgroundAct,
        i32 backgroundAction,
        i32 playerAct,
        i32 playerAction,
        i32 leaderAct,
        i32 leaderAction,
        i32 stamina
    );
    bool SetObstacleAct(u32 kind, i32 act, i32 action);
    bool Stop();
    bool Frame();

private:
    void ReadControls(int& moveX, int& moveY) const;
    void Update(int moveX, int moveY);
    void Draw();
    void DrawBackground(int left, int top);
    void DrawSprites();
    void DrawSprite(const SwimSprite& sprite);
    void DrawGauges();
    bool Steer(SwimSprite& sprite, int moveX, int moveY, bool player);
    static void PushAway(SwimSprite& sprite, const SwimSprite& obstacle);
    const SwimState* FollowPlayer();

    Game& game;
    std::unique_ptr<SwimState> state;      // 0x00579A3C
    int courseHeight = 0;                  // 0x00579A30
    std::unique_ptr<ActState> background;  // 0x00579A34 the act tiled behind the course
    std::unique_ptr<ActState> leaderCopy;  // 0x00579A2C the leader's act once more; nothing draws it
    i32 cameraTop = 0;                     // 0x00579A40 the course line at the top of the screen
};

}  // namespace swd3e
