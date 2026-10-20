#pragma once

#include <array>
#include <memory>

#include "core/types.h"
#include "res/act_state.h"

namespace swd3e
{

class Game;

// A sprite of the ascent (0x14 bytes in the original): its place on the course, its speed and its act.
struct AscentSprite
{
    static constexpr i32 Parked = -0x400;  // `x` and `y` of an obstacle slot not in use

    i32 x = 0;                      // +0x00
    i32 y = 0;                      // +0x04 course pixels, 0 at the top
    i32 velocityX = 0;              // +0x08
    i32 velocityY = 0;              // +0x0C
    std::unique_ptr<ActState> act;  // +0x10
};

// The act of one kind of obstacle (8 bytes in the original, set by AscentGame::SetObstacleAct).
struct AscentObstacleAct
{
    i32 act = 0;     // +0x00
    i32 action = 0;  // +0x04
};

// The state of a running ascent (0x90 bytes, 0x00579A3C).
struct AscentState
{
    static constexpr std::size_t ObstacleKinds = 8;
    static constexpr std::size_t Obstacles = 8;

    // Bits of `flags`.
    enum Flag : u32
    {
        Running = 0x80000000u,  // set at the start; nothing reads it
        Ending = 0x40000000u,   // the screen fades out (Escape, or the top reached)
        Done = 0x10000000u,     // the fade is over: the ascent ends
    };

    i32 unk00 = 0;    // +0x00 the first argument of AscentGame::Start; nothing reads it
    i32 unk04 = 0;    // +0x04 0; nothing reads it
    i32 wind = 0;     // +0x08 the sideways drift (eighths of a pixel), a random walk within -16..16
    i32 current = 0;  // +0x0C the downward drift (eighths), a random walk within 0..16
    i32 scrollX = 0;  // +0x10 the background's offset, moved by the drifts
    i32 scrollY = 0;  // +0x14
    i32 stamina = 0;  // +0x18 0..100: rises while the role rests, swimming costs it
    std::array<AscentObstacleAct, ObstacleKinds> obstacleActs = {};  // +0x1C
    i32 fadeFrames = 0;                                              // +0x5C frames of the fade left (Ending)
    u32 flags = 0;                                                   // +0x60 AscentState::Flag
    std::unique_ptr<AscentSprite> player;   // +0x64 the controlled role, from the bottom of the course
    std::unique_ptr<AscentSprite> leader;   // +0x68 a role at the top, keeping to the player's column
    std::unique_ptr<AscentSprite> partner;  // +0x6C a role beside it (act 0x2332 action 11), moving with it
    std::array<std::unique_ptr<AscentSprite>, Obstacles> obstacles;  // +0x70 drifting down the course
};

// The ascent game of swd3e (talk command 210 and the debug key F7; 0x0049EFF0..0x004A0027): the controlled role
// climbs a course of 800 pixels against a downward current and a sideways wind, past obstacles drifting down, with
// two roles waiting at the top. Up swims up (holding it rests), Down backs off, Left and Right move across, Escape
// gives up. Its name is an inference from that behaviour; what the scene shows in the story is not identified yet.
class AscentGame
{
public:
    static constexpr int CourseHeight = 0x320;  // 0x00579A30

    explicit AscentGame(Game& game);
    ~AscentGame();

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
    void DrawSprite(const AscentSprite& sprite);
    void DrawGauges();
    bool Steer(AscentSprite& sprite, int moveX, int moveY, bool player);
    static void PushAway(AscentSprite& sprite, const AscentSprite& obstacle);
    const AscentState* FollowPlayer();

    Game& game;
    std::unique_ptr<AscentState> state;    // 0x00579A3C
    int courseHeight = 0;                  // 0x00579A30
    std::unique_ptr<ActState> background;  // 0x00579A34 the act tiled behind the course
    std::unique_ptr<ActState> leaderCopy;  // 0x00579A2C the leader's act once more; nothing draws it
    i32 cameraTop = 0;                     // 0x00579A40 the course line at the top of the screen
};

}  // namespace swd3e
