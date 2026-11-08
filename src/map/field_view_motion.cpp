#include "map/field_view_internal.h"

namespace swd3e
{

namespace
{

// A gap longer than this many milliseconds (a pause, a slow frame) is skipped rather than run.
constexpr u32 kMostTicks = 0x100;
// A role drawn farther than this many tiles from where the frames left it was put there: it is shown there at once.
constexpr i32 kFarSteps = 2;
// A role drawn behind where the frames left it gains at most this part of its step on each step it takes.
constexpr i32 kCatchUpShare = 4;
// The steps of a walk measured for its pace (whole rounds of the walks whose steps take turns of two or four lengths),
// looked for over at most this many game frames.
constexpr i32 kPaceSteps = 8;
constexpr i32 kPaceMostFrames = 0x100;

i32 Sign(i32 value)
{
    return (value > 0) - (value < 0);
}

// The follower record (Scene::followerMoves) of role `index`, or nullptr.
const MoveRecord* FollowerMoveOf(const Scene& view, std::size_t index)
{
    for (u32 slot = 1; slot < static_cast<u32>(view.followerCount) && slot < view.followerMoves.size(); ++slot)
    {
        if (view.followerMoves[slot].role == index)
        {
            return &view.followerMoves[slot];
        }
    }

    return nullptr;
}

}  // namespace

// The end of the map's frame (0x004126A0) for the controlled role on a tile: its cells and trail follow its step and
// the step ends.
void FieldView::SettlePlayer()
{
    Game& owner = this->game;
    GameState& state = owner.state;
    Scene& view = owner.scene;
    Field& field = owner.field;
    Role& player = state.roles[static_cast<std::size_t>(state.playerRole)];
    const i32 leaderId = owner.partyLeaderId;
    const int width = state.map.width;
    field.RemoveRole(
        player.id,
        static_cast<int>(player.flags & Role::LayerMask),
        (static_cast<u32>(player.y) >> MapHeader::TileShift) - 1,
        0
    );
    field.ClearRoleCells(player, leaderId);

    if (player.cell != nullptr)
    {
        const std::ptrdiff_t at = player.cell - state.map.blocks.data() + owner.scrollStepX + owner.walkStepX +
                                  static_cast<std::ptrdiff_t>(width) * (owner.scrollStepY + owner.walkStepY);
        player.cell = at >= 0 && static_cast<std::size_t>(at) < state.map.blocks.size()
                          ? &state.map.blocks[static_cast<std::size_t>(at)]
                          : nullptr;
    }

    field.MarkRoleCells(player, leaderId);
    owner.scrollStepY = 0;
    owner.walkStepY = 0;
    owner.scrollStepX = 0;
    owner.walkStepX = 0;

    if (view.trailX[0] != player.x || view.trailY[0] != player.y)
    {
        std::copy_backward(view.trailX.begin(), view.trailX.end() - 1, view.trailX.end());
        std::copy_backward(view.trailY.begin(), view.trailY.end() - 1, view.trailY.end());
        std::copy_backward(view.trailDirection.begin(), view.trailDirection.end() - 1, view.trailDirection.end());
        view.trailX[0] = player.x;
        view.trailY[0] = player.y;
        view.trailDirection[0] = player.act.direction;
    }

    this->UpdateRoleCellFlags(player);
}

// The milliseconds of the map up to `now`, run while the map's frames run (no menu, battle, story page, video or
// pause): a gap after a pause starts the clock again.
void FieldView::AdvanceMotion(u32 now)
{
    Game& owner = this->game;

    if (!owner.smoothMotion)
    {
        // Smooth motion turned off: every motion under way ends where the game frames would have it (the roles' steps
        // are drawn only).
        if (this->playerMotion.active)
        {
            this->EndPlayerMotion(true);
        }

        this->roleMotions.clear();

        if (this->panMotion.active)
        {
            this->panMotion.elapsed = std::max(static_cast<i32>(owner.state.frameInterval), 1);
            this->PanTick();
        }

        this->motionClockSet = false;
        return;
    }

    // Another map drops the motions of the map left.
    if (owner.currentMap != this->motionMap)
    {
        this->motionMap = owner.currentMap;
        this->playerMotion.active = false;
        this->roleMotions.clear();
        this->panMotion.active = false;
    }

    const bool moving = owner.running != 0 && owner.inactive == 0 && owner.active != 0 &&
                        (owner.systemFlags & (Game::VideoPlaying | Game::UnkSystem1 | Game::QuitRequested)) == 0 &&
                        owner.storyState == Game::StoryRunning && owner.inBattle == 0 &&
                        owner.menuState == Game::NoMenu && owner.state.frameInterval > 0;

    if (!moving || !this->motionClockSet || now - this->motionClock > kMostTicks)
    {
        this->motionClock = now;
        this->motionClockSet = moving;
        return;
    }

    while (static_cast<i32>(now - this->motionClock) > 0)
    {
        ++this->motionClock;
        this->MotionTick();

        for (std::size_t index = 0; index < this->roleMotions.size(); ++index)
        {
            this->RoleTick(index);
        }

        this->PanTick();
    }
}

// A millisecond of the controlled role: a step ControlPlayer set in a frame moves it (and the camera when it follows)
// by its speed, a game frame's worth every frame interval; the step starts at the frame, so it reaches the next tile
// at a later frame's time, where the role settles as the end of the original's frame does (SettlePlayer) and that
// frame's ControlPlayer decides the next step.
void FieldView::MotionTick()
{
    Game& owner = this->game;
    GameState& state = owner.state;
    Field& field = owner.field;
    Role& player = state.roles[static_cast<std::size_t>(state.playerRole)];
    PlayerMotion& motion = this->playerMotion;
    const bool stepping =
        owner.scrollStepX != 0 || owner.scrollStepY != 0 || owner.walkStepX != 0 || owner.walkStepY != 0;

    if (motion.active)
    {
        const bool moved = player.x != motion.leftX || player.y != motion.leftY ||
                           field.cameraLeft != motion.leftCameraX || field.cameraTop != motion.leftCameraY;
        const bool same = motion.across == Sign(owner.scrollStepX + owner.walkStepX) &&
                          motion.down == Sign(owner.scrollStepY + owner.walkStepY) &&
                          motion.scrollAcross == owner.scrollStepX && motion.scrollDown == owner.scrollStepY;

        if (moved)
        {
            motion.active = false;
        }
        else if (!same)
        {
            this->EndPlayerMotion(true);
        }
    }

    if (!motion.active)
    {
        if (!stepping)
        {
            return;
        }

        this->BeginPlayerMotion();

        if (!motion.active)
        {
            return;
        }
    }

    ++motion.elapsed;
    const auto interval = static_cast<i32>(state.frameInterval);
    const i32 pixels = motion.speed * motion.elapsed / interval;
    const i32 cameraX = motion.cameraX + motion.scrollAcross * pixels;
    const i32 cameraY = motion.cameraY + motion.scrollDown * pixels;
    field.cameraRight += cameraX - field.cameraLeft;
    field.cameraBottom += cameraY - field.cameraTop;
    field.cameraLeft = cameraX;
    field.cameraTop = cameraY;
    const Point from{player.x, player.y};
    player.x = motion.startX + motion.across * pixels;
    player.y = motion.startY + motion.down * pixels;
    field.FollowOriginal({player.x - from.x, player.y - from.y});
    this->NoteStep(static_cast<std::size_t>(state.playerRole), player.x - from.x, player.y - from.y);
    motion.leftX = player.x;
    motion.leftY = player.y;
    motion.leftCameraX = field.cameraLeft;
    motion.leftCameraY = field.cameraTop;

    if (((player.x | player.y) & MapHeader::TileFraction) != 0 ||
        (player.x == motion.startX && player.y == motion.startY))
    {
        return;
    }

    motion.active = false;
    this->SettlePlayer();
}

// A step set by ControlPlayer starts from where the role and the camera are.
void FieldView::BeginPlayerMotion()
{
    Game& owner = this->game;
    const Field& field = owner.field;
    const Role& player = owner.state.roles[static_cast<std::size_t>(owner.state.playerRole)];
    PlayerMotion& motion = this->playerMotion;
    motion.across = Sign(owner.scrollStepX + owner.walkStepX);
    motion.down = Sign(owner.scrollStepY + owner.walkStepY);
    motion.scrollAcross = owner.scrollStepX;
    motion.scrollDown = owner.scrollStepY;
    motion.active = (motion.across != 0 || motion.down != 0) && owner.state.walkSpeed > 0;
    motion.startX = player.x;
    motion.startY = player.y;
    motion.cameraX = field.cameraLeft;
    motion.cameraY = field.cameraTop;
    motion.speed = owner.state.walkSpeed;
    motion.elapsed = 0;
    motion.leftX = player.x;
    motion.leftY = player.y;
    motion.leftCameraX = field.cameraLeft;
    motion.leftCameraY = field.cameraTop;
}

// A step left before its tile (its keys cleared, smooth motion turned off) ends where the game frames would have moved
// it (`atFrame`: a whole number of frames of its speed), so that they go on from a place of theirs.
void FieldView::EndPlayerMotion(bool atFrame)
{
    Game& owner = this->game;
    Field& field = owner.field;
    Role& player = owner.state.roles[static_cast<std::size_t>(owner.state.playerRole)];
    PlayerMotion& motion = this->playerMotion;
    motion.active = false;

    if (!atFrame || owner.state.frameInterval <= 0)
    {
        return;
    }

    const i32 pixels = motion.speed * (motion.elapsed / static_cast<i32>(owner.state.frameInterval));
    const i32 cameraX = motion.cameraX + motion.scrollAcross * pixels;
    const i32 cameraY = motion.cameraY + motion.scrollDown * pixels;
    field.cameraRight += cameraX - field.cameraLeft;
    field.cameraBottom += cameraY - field.cameraTop;
    field.cameraLeft = cameraX;
    field.cameraTop = cameraY;
    const Point from{player.x, player.y};
    player.x = motion.startX + motion.across * pixels;
    player.y = motion.startY + motion.down * pixels;
    field.FollowOriginal({player.x - from.x, player.y - from.y});
    this->NoteStep(static_cast<std::size_t>(owner.state.playerRole), player.x - from.x, player.y - from.y);
}

// The distance of the step under way in 1/scale pixels.
i32 FieldView::MotionPixels(int scale) const
{
    const PlayerMotion& motion = this->playerMotion;
    const auto interval = static_cast<i32>(this->game.state.frameInterval);

    if (!motion.active || interval <= 0)
    {
        return 0;
    }

    return motion.speed * motion.elapsed * scale / interval;
}

// The camera where the motion under way has it: following the step of the keys, centred on the controlled role an
// event walks (as CenterCamera keeps it), or panning.
std::array<i32, 2> FieldView::CameraFine(int scale) const
{
    const Game& owner = this->game;
    const Field& field = owner.field;
    const PlayerMotion& motion = this->playerMotion;

    if (motion.active)
    {
        const i32 pixels = this->MotionPixels(scale);
        return {
            motion.cameraX * scale + motion.scrollAcross * pixels,
            motion.cameraY * scale + motion.scrollDown * pixels,
        };
    }

    const auto playerIndex = static_cast<std::size_t>(owner.state.playerRole);
    const Role& player = owner.state.roles[playerIndex];

    if (playerIndex < this->roleMotions.size() && this->roleMotions[playerIndex].active &&
        (owner.scene.screenFlags & Scene::CameraHeld) == 0 &&
        field.CameraCentredOn(player.x, player.y) == std::array<i32, 2>{field.cameraLeft, field.cameraTop})
    {
        const std::array<i32, 2> place = this->RoleFine(playerIndex, scale);
        return field.CameraCentredOn(place[0], place[1], scale);
    }

    const PanMotion& pan = this->panMotion;
    const auto interval = static_cast<i32>(owner.state.frameInterval);

    if (pan.active && interval > 0)
    {
        return {
            pan.fromX * scale + pan.byX * pan.elapsed * scale / interval,
            pan.fromY * scale + pan.byY * pan.elapsed * scale / interval,
        };
    }

    return {field.cameraLeft * scale, field.cameraTop * scale};
}

// A role where its step under way has it (the controlled role where the step of the keys has it).
std::array<i32, 2> FieldView::RoleFine(std::size_t index, int scale) const
{
    const Game& owner = this->game;
    const Role& role = owner.state.roles[index];

    if (index == static_cast<std::size_t>(owner.state.playerRole) && this->playerMotion.active)
    {
        return this->PlayerFine(scale);
    }

    if (this->RoleOnStep(index))
    {
        const std::array<i32, 2> shown = this->MotionShown(this->roleMotions[index]);
        return {FloorDivide(shown[0] * scale, MotionFine), FloorDivide(shown[1] * scale, MotionFine)};
    }

    return {role.x * scale, role.y * scale};
}

// Where a role is drawn by its motion now, in 1/MotionFine pixels.
std::array<i32, 2> FieldView::MotionShown(const RoleMotion& step) const
{
    return {
        step.fromX + static_cast<i32>(static_cast<i64>(step.toX - step.fromX) * step.elapsed / step.length),
        step.fromY + static_cast<i32>(static_cast<i64>(step.toY - step.fromY) * step.elapsed / step.length),
    };
}

// Whether role `index` is drawn on a step that stops it, with the act it had where the step started.
// The steps role `index` walked since the frame kept its place (the frames' moves, the controlled role's motion).
void FieldView::NoteStep(std::size_t index, i32 x, i32 y)
{
    if (index < this->frameSteps.size())
    {
        this->frameSteps[index].x += x;
        this->frameSteps[index].y += y;
    }
}

bool FieldView::RoleStopping(std::size_t index) const
{
    return this->RoleOnStep(index) && this->roleMotions[index].stops &&
           this->roleMotions[index].elapsed < this->roleMotions[index].length;
}

// Whether role `index` is a follower drawn turning on its tile, with the act of its first turn there.
bool FieldView::RoleTurning(std::size_t index) const
{
    return this->RoleOnStep(index) && this->roleMotions[index].turning;
}

// Whether role `index` is drawn by its motion (still where the frames last left it).
bool FieldView::RoleOnStep(std::size_t index) const
{
    if (index >= this->roleMotions.size() || !this->roleMotions[index].active)
    {
        return false;
    }

    const RoleMotion& step = this->roleMotions[index];
    const Role& role = this->game.state.roles[index];
    return role.x == step.targetX && role.y == step.targetY;
}

// The roles' places as a game frame starts (the controlled role moved by its keys up to it): the steps the frame takes
// are drawn from them (CaptureRoleSteps).
void FieldView::KeepRolePlaces()
{
    const Game& owner = this->game;
    const i32 interval = std::max(static_cast<i32>(owner.state.frameInterval), 1);
    this->framePlaces.resize(GameState::Roles);

    for (std::size_t index = 0; index < GameState::Roles; ++index)
    {
        const Role& role = owner.state.roles[index];
        bool walking = false;

        if (index < this->roleMotions.size())
        {
            // A step drawn over its frames is walking until the frame after its last (a step of two frames ends as
            // the frame that stands the role starts).
            const RoleMotion& step = this->roleMotions[index];
            const u32 frames = static_cast<u32>(std::max((step.length + interval - 1) / interval, 1));
            walking =
                step.active && !step.frozen && (step.elapsed < step.length || owner.gameFrames <= step.frame + frames);
        }

        this->framePlaces[index] = RolePlace{
            .id = role.id,
            .shown = (role.flags & Role::Visible) != 0,
            .x = role.x,
            .y = role.y,
            .act = role.act,
            .walking = walking,
            .held = (role.flags & Role::Held) != 0,
        };

        this->frameSteps[index] = {};
    }

    this->framePlacesFrame = owner.gameFrames;
}

// The game frame's moves of the roles as they are drawn. A role the frame moved is drawn going from where it is drawn
// now to where the frame left it; a walking role whose action the frame changed (it stopped) with the act it had where
// it started until it gets there (the frame's picture is that of the place it stepped to: it stops, then stands);
// otherwise with the frame's act (a role starting off or turning faces its way from the step's start). A step takes the
// frames to the role's next step: its act's delay plus one, read once the frame's acts have all run; for a step that
// stops it, the delay its act at the start takes after a run, plus one, when fewer (it steps at the pace it walked);
// one past RoleStepMostFrames. On the straight end of a move (StepsToEnd) each step is drawn an even share of the way
// left to the move's end, so that the role gets there with the frame that stands it, at one pace along the straight
// (a step drawn there already stands at once). Any other role drawn behind (held while between tiles, the frames put
// it on the tile ahead) gains at most 1/kCatchUpShare of its step on each step, so that it catches up over a few steps
// at a pace near its own. A role the frame held for an event, or turned or changed the action of without moving it, has
// stopped: it stays drawn where it is (where the frame moved it on too) until the frames move it again, but for a
// walking follower turned with its next step set, which walks on; one the frame did not move goes on as it is drawn.
// One that came, went or changed, or is drawn more than kFarSteps tiles from where the frame left it (put there), is
// shown where it is, as are all after a gap in the frames or on another map.
void FieldView::CaptureRoleSteps(bool continuous)
{
    Game& owner = this->game;
    const bool kept =
        continuous && this->framePlacesFrame == owner.gameFrames && this->framePlaces.size() == GameState::Roles;
    const i32 interval = std::max(static_cast<i32>(owner.state.frameInterval), 1);
    this->roleMotions.resize(GameState::Roles);

    for (std::size_t index = 0; index < GameState::Roles; ++index)
    {
        const Role& role = owner.state.roles[index];
        RoleMotion& step = this->roleMotions[index];

        if (!kept)
        {
            step.active = false;
            continue;
        }

        const RolePlace& from = this->framePlaces[index];

        if (!from.shown || (role.flags & Role::Visible) == 0 || role.id != from.id)
        {
            step.active = false;
            continue;
        }

        const bool tracked = step.active && step.targetX == from.x && step.targetY == from.y;
        const std::array<i32, 2> shown =
            tracked ? this->MotionShown(step) : std::array<i32, 2>{from.x * MotionFine, from.y * MotionFine};
        const bool moved = role.x != from.x || role.y != from.y;
        const bool held = (role.flags & Role::Held) != 0 && !from.held;
        const bool turned = role.act.direction != from.act.direction || role.act.action != from.act.action;

        // The rebuild: what moved the role besides its steps put it there (an event's command, a path's place, a step
        // skipped off the camera, the map's start): it is drawn there at once. A role between tiles moved to the start
        // of a tile next to it was aligned (RolePath::TileStart, AlignPlayer: an event holding it, a move started from
        // it), which the drawing follows as below.
        const Point walked = this->frameSteps[index];
        const i32 putX = role.x - from.x - walked.x;
        const i32 putY = role.y - from.y - walked.y;
        const bool between = ((from.x | from.y) & MapHeader::TileFraction) != 0;
        const bool aligned = between && ((role.x | role.y) & MapHeader::TileFraction) == 0 &&
                             std::abs(putX) < MapHeader::TileSize && std::abs(putY) < MapHeader::TileSize;

        if ((putX != 0 || putY != 0) && !aligned)
        {
            step.active = false;
            continue;
        }

        const MoveRecord* followerMove = FollowerMoveOf(owner.scene, index);

        // The rebuild: a walking follower its frame turned on a tile, its next step set (RolePath::Follow faces it
        // along the step before its act's delay lets it take it, a frame or more later than a step straight on), walks
        // on: it is drawn from where it is to the end of that step over the frames to the step and as many again (what
        // is left of the step it is on, then the next), cutting the corner at about its pace, facing the new way.
        const bool walksOn = !moved && !held && followerMove != nullptr && role.act.action == from.act.action &&
                             (followerMove->step & MoveRecord::Done) == 0 &&
                             (followerMove->stepX != 0 || followerMove->stepY != 0) &&
                             static_cast<i32>(role.act.delay) < RoleStepMostFrames;

        if (walksOn)
        {
            // Turned again on the same tile (its path's first step flips as the leader's trail moves on): the drawing
            // keeps its first turn, its act going on while the role faces that way.
            if (tracked && step.turning)
            {
                if (role.act.direction == step.act.direction)
                {
                    step.act = role.act;
                }
                else
                {
                    // The rebuild: the act kept facing the first turn plays on as the role's does (a copy held still
                    // shows one picture until the role steps).
                    step.act = owner.acts.Peek(step.act);
                }

                continue;
            }

            const i32 walkToX = (role.x + followerMove->stepX) * MotionFine;
            const i32 walkToY = (role.y + followerMove->stepY) * MotionFine;
            i32 walkLength = 2 * (static_cast<i32>(role.act.delay) + 1) * interval;

            // The rebuild: a walk whose steps take frames of different lengths goes on at its mean pace (PaceOf), as
            // its steps are drawn.
            const WalkPace walkPace = this->PaceOf(role.act);
            const i32 walkStep = std::max(std::abs(followerMove->stepX), std::abs(followerMove->stepY)) * MotionFine;
            const bool walkPaced = walkPace.uneven && walkStep > 0;

            if (walkPaced)
            {
                const i32 aimed = std::max(std::abs(walkToX - shown[0]), std::abs(walkToY - shown[1]));
                walkLength = std::max(
                    static_cast<i32>(
                        static_cast<i64>(aimed) * walkPace.frames * interval /
                        (static_cast<i64>(walkPace.steps) * walkStep)
                    ),
                    1
                );
            }

            step = RoleMotion{
                .active = true,
                .targetX = role.x,
                .targetY = role.y,
                .fromX = shown[0],
                .fromY = shown[1],
                .toX = walkToX,
                .toY = walkToY,
                .length = walkLength,
                .elapsed = 0,
                .frame = owner.gameFrames,
                .stops = false,
                .frozen = false,
                .turning = true,
                .paceTime = walkPaced ? walkPace.frames * interval : 0,
                .paceDistance = walkPaced ? walkPace.steps * walkStep : 0,
                .act = role.act,
            };

            continue;
        }

        // The rebuild: a role drawn at its walk's mean pace (PaceOf) that the frame stopped facing the way it walked
        // goes to its tile at that pace from where it is drawn (short of it or past it), with the act it walked with;
        // one that turned as it stopped stands where it is drawn with its act, as any role turning.
        const bool stopped =
            !moved && !held && role.act.action != from.act.action && role.act.direction == from.act.direction;

        if (stopped && tracked && step.paceDistance > 0 && !step.frozen)
        {
            const i32 back =
                std::max(std::abs(role.x * MotionFine - shown[0]), std::abs(role.y * MotionFine - shown[1]));
            step = RoleMotion{
                .active = true,
                .targetX = role.x,
                .targetY = role.y,
                .fromX = shown[0],
                .fromY = shown[1],
                .toX = role.x * MotionFine,
                .toY = role.y * MotionFine,
                .length = std::max(static_cast<i32>(static_cast<i64>(back) * step.paceTime / step.paceDistance), 1),
                .elapsed = 0,
                .frame = step.frame,
                .stops = true,
                .frozen = false,
                .act = step.act,
            };

            continue;
        }

        if (held || (!moved && turned))
        {
            step = RoleMotion{
                .active = true,
                .targetX = role.x,
                .targetY = role.y,
                .fromX = shown[0],
                .fromY = shown[1],
                .toX = shown[0],
                .toY = shown[1],
                .length = 1,
                .elapsed = 1,
                .frame = step.frame,
                .stops = false,
                .frozen = true,
                .act = role.act,
            };

            continue;
        }

        const i32 toX = role.x * MotionFine;
        const i32 toY = role.y * MotionFine;
        const i32 gapX = toX - shown[0];
        const i32 gapY = toY - shown[1];
        const i32 gap = std::max(std::abs(gapX), std::abs(gapY));

        if (!moved)
        {
            // A turning follower faced its first turn's way again: its act goes on; facing another way, the act kept
            // plays on (the rebuild).
            if (tracked && step.turning)
            {
                step.act = role.act.direction == step.act.direction ? role.act : owner.acts.Peek(step.act);
            }

            // The rebuild: a role drawn walking on to where it stopped plays its walk on until it gets there (a copy
            // held still shows one picture, then the stand at once).
            if (tracked && step.stops && !step.frozen && step.elapsed < step.length)
            {
                step.act = owner.acts.Peek(step.act);
            }

            continue;
        }

        constexpr i32 kFar = kFarSteps * MapHeader::TileSize * MotionFine;

        if (std::abs(gapX) > kFar || std::abs(gapY) > kFar)
        {
            step.active = false;
            continue;
        }

        // On the straight end of a move the drawing covers an even share of the way left to the move's end on each of
        // the steps left, at one pace all along the straight; a short straight (one step after a turn) would take
        // twice the walk, so the share is held to the catch-up below and the stop walks the rest on.
        const i32 left = this->StepsToEnd(index, from);
        const i32 aimX =
            left >= 1 ? shown[0] + ((role.x + left * (role.x - from.x)) * MotionFine - shown[0]) / left : toX;
        const i32 aimY =
            left >= 1 ? shown[1] + ((role.y + left * (role.y - from.y)) * MotionFine - shown[1]) / left : toY;
        const i32 aimGapX = aimX - shown[0];
        const i32 aimGapY = aimY - shown[1];
        const i32 aimGap = std::max(std::abs(aimGapX), std::abs(aimGapY));

        // The step drawn gains at most a part of the frame's step on a role drawn behind.
        const i32 stepped = std::max(std::abs(role.x - from.x), std::abs(role.y - from.y)) * MotionFine;
        const i32 most = stepped + stepped / kCatchUpShare;
        i32 endX = aimX;
        i32 endY = aimY;

        if (aimGap > most)
        {
            endX = shown[0] + static_cast<i32>(static_cast<i64>(aimGapX) * most / aimGap);
            endY = shown[1] + static_cast<i32>(static_cast<i64>(aimGapY) * most / aimGap);
        }

        // A step that stops a walking role drawn short of where the frame left it takes the pace it walked at (its
        // stand's delay would hold it long); any other the frames to the role's next step. One drawn there already
        // (the step before took it on) stands at once.
        const bool stops = from.walking && role.act.action != from.act.action && gap != 0;

        i32 next = static_cast<i32>(role.act.delay) + 1;

        if (stops)
        {
            next = std::min(next, static_cast<i32>(owner.acts.Peek(from.act).delay) + 1);
        }

        const i32 frames = next > RoleStepMostFrames ? 1 : next;
        i32 length = frames * interval;

        // The rebuild: a walk whose steps take frames of different lengths (its act's frames hold for different
        // delays: 7 and 11, 9 and 5, ...) is drawn at its mean pace all along, the speed of its steps over a round of
        // the walk; the frames' steps are those of the original. It is drawn on toward the step after the one the
        // frame took (a long wait would have it reach its tile and stand): a stop takes it back to its tile. A role the
        // frame gave another action walks on no more (it stopped where the walk had drawn it already, and turned: its
        // new act, whose frames may hold unevenly too, would glide it a step past its tile).
        const bool keepsAction = role.act.action == from.act.action;
        const WalkPace pace = stops || !keepsAction ? WalkPace{} : this->PaceOf(role.act);
        const bool paced = pace.uneven && stepped > 0;

        if (paced)
        {
            endX = toX + (role.x - from.x) * MotionFine;
            endY = toY + (role.y - from.y) * MotionFine;
            const i32 aimed = std::max(std::abs(endX - shown[0]), std::abs(endY - shown[1]));
            length = std::max(
                static_cast<i32>(
                    static_cast<i64>(aimed) * pace.frames * interval / (static_cast<i64>(pace.steps) * stepped)
                ),
                1
            );
        }

        step = RoleMotion{
            .active = true,
            .targetX = role.x,
            .targetY = role.y,
            .fromX = shown[0],
            .fromY = shown[1],
            .toX = endX,
            .toY = endY,
            .length = length,
            .elapsed = 0,
            .frame = owner.gameFrames,
            .stops = stops,
            .frozen = false,
            .paceTime = paced ? pace.frames * interval : 0,
            .paceDistance = paced ? pace.steps * stepped : 0,
            .act = from.act,
        };
    }
}

// The steps like the one the game frame took (from `from`) that end role `index`'s move where it ends, in a straight
// line; 0 when its move does not end so (or it has none, or is at its end).
i32 FieldView::StepsToEnd(std::size_t index, const RolePlace& from) const
{
    const GameState& state = this->game.state;
    const Role& role = state.roles[index];
    const i32 byX = role.x - from.x;
    const i32 byY = role.y - from.y;

    if (byX == 0 && byY == 0)
    {
        return 0;
    }

    for (const MoveRecord& move : state.moves)
    {
        // A step on a tile marks the move Done until the next is taken: its end stands.
        if (move.role != index)
        {
            continue;
        }

        const i32 leftX = move.x - role.x;
        const i32 leftY = move.y - role.y;
        const i32 steps = byX != 0 ? leftX / byX : leftY / byY;
        return steps >= 1 && leftX == steps * byX && leftY == steps * byY ? steps : 0;
    }

    return 0;
}

// The pace of a role walking with `act` (its state after the frame): the frames to its next kPaceSteps steps, a step
// taken on each frame that starts with the act's delay at 0 (PathMove); uneven when the gaps between them differ.
FieldView::WalkPace FieldView::PaceOf(const ActState& act)
{
    ActState state = act;
    WalkPace pace;
    i32 first = -1;
    i32 gap = 0;

    for (i32 frame = 0; frame < kPaceMostFrames && pace.steps < kPaceSteps; ++frame)
    {
        if (state.delay == 0)
        {
            if (first < 0)
            {
                first = frame;
            }
            else
            {
                const i32 since = frame - first - pace.frames;
                pace.uneven = pace.uneven || (gap != 0 && since != gap);
                gap = since;
                pace.frames += since;
                ++pace.steps;
            }
        }

        state = this->game.acts.Peek(state);
    }

    pace.uneven = pace.uneven && pace.steps == kPaceSteps;
    return pace;
}

// A millisecond of a role's step as it is drawn.
void FieldView::RoleTick(std::size_t index)
{
    if (index >= this->roleMotions.size() || !this->roleMotions[index].active)
    {
        return;
    }

    RoleMotion& step = this->roleMotions[index];
    step.elapsed = std::min(step.elapsed + 1, step.length);
}

// The pan of a frame (0x00414570) moves the camera by its step over the frame.
void FieldView::StartPan(i32 byX, i32 byY)
{
    const Field& field = this->game.field;

    if (this->panMotion.active)
    {
        this->panMotion.elapsed = std::max(static_cast<i32>(this->game.state.frameInterval), 1);
        this->PanTick();
    }

    this->panMotion = PanMotion{
        byX != 0 || byY != 0,
        field.cameraLeft,
        field.cameraTop,
        byX,
        byY,
        0,
        field.cameraLeft,
        field.cameraTop
    };
}

// A millisecond of the pan: the camera moves by its share of the step; moved by anything else, the pan ends there.
void FieldView::PanTick()
{
    PanMotion& pan = this->panMotion;

    if (!pan.active)
    {
        return;
    }

    Field& field = this->game.field;

    if (field.cameraLeft != pan.leftX || field.cameraTop != pan.leftY)
    {
        pan.active = false;
        return;
    }

    const auto interval = std::max(static_cast<i32>(this->game.state.frameInterval), 1);
    pan.elapsed = std::min(pan.elapsed + 1, interval);
    const i32 cameraX = pan.fromX + pan.byX * pan.elapsed / interval;
    const i32 cameraY = pan.fromY + pan.byY * pan.elapsed / interval;
    field.cameraRight += cameraX - field.cameraLeft;
    field.cameraBottom += cameraY - field.cameraTop;
    field.cameraLeft = cameraX;
    field.cameraTop = cameraY;
    pan.leftX = cameraX;
    pan.leftY = cameraY;
    pan.active = pan.elapsed < interval;
}

std::array<i32, 2> FieldView::PlayerFine(int scale) const
{
    const Role& player = this->game.state.roles[static_cast<std::size_t>(this->game.state.playerRole)];
    const PlayerMotion& motion = this->playerMotion;

    if (!motion.active)
    {
        return {player.x * scale, player.y * scale};
    }

    const i32 pixels = this->MotionPixels(scale);
    return {motion.startX * scale + motion.across * pixels, motion.startY * scale + motion.down * pixels};
}

}  // namespace swd3e
