#include <climits>
#include <cstring>
#include <span>
#include <string>
#include <string_view>

#include "core/debug_log.h"
#include "core/error_box.h"
#include "game/game.h"
#include "platform/display.h"

namespace swd3e
{

namespace
{

constexpr std::size_t kLegacyRoleBytes = 0x16;                 // LeaveParty: swd32's role records
constexpr std::u8string_view kLeavePartyTitle = u8"解除夥伴";  // 0x004B1058

constexpr i32 kTimerFrames = 30;  // the timers count frames, 30 a second
constexpr i32 kSecondsPerMinute = 60;

}  // namespace

// swd3e.exe 0x0040D4C0: the camera around (x, y): half the view (320 x 240 pixels) before it, held inside the map
void Field::CameraAround(int x, int y, CameraRect& rect) const
{
    const std::array<i32, 2> place = this->HoldCamera(x - this->viewWidth / 2, y - this->viewHeight / 2);
    rect.left = place[0];
    rect.top = place[1];
    rect.right = rect.left + this->viewWidth;
    rect.bottom = rect.top + this->viewHeight;
}

// swd3e.exe 0x0040D8D0: role `id` loaded from its record of maps.dat's role table (act, place in tiles, event, path,
// flags); its map is returned, 0xFFFFFFFF when the table lacks it
u32 Game::LoadRoleRecord(Role& role, u16 id)
{
    const std::vector<u8>& data = this->databases.mapsData;

    for (std::size_t at = this->scene.rolesTable; MapsWord(data, at) != MapsRecord::TableEnd;
         at += sizeof(u16) * MapsRecord::RoleWords)
    {
        if (MapsWord(data, at + sizeof(u16) * MapsRecord::RoleId) != id)
        {
            continue;
        }

        role.id = id;
        role.act.id = MapsWord(data, at + sizeof(u16) * MapsRecord::RoleAct);
        role.act.action = MapsWord(data, at + sizeof(u16) * MapsRecord::RoleAction);
        role.act.direction = MapsWord(data, at + sizeof(u16) * MapsRecord::RoleDirection);
        role.x = MapHeader::TileSize * MapsWord(data, at + sizeof(u16) * MapsRecord::RoleColumn);
        role.y = MapHeader::TileSize * MapsWord(data, at + sizeof(u16) * MapsRecord::RoleRow);
        role.event = MapsWord(data, at + sizeof(u16) * MapsRecord::RoleEvent);
        role.pathNumber = MapsWord(data, at + sizeof(u16) * MapsRecord::RolePath);
        role.pathStep = 0;
        role.flags = MapsWord(data, at + sizeof(u16) * MapsRecord::RoleFlags);
        return MapsWord(data, at);
    }

    DebugLog(u8"未定義角色[%d]", id);
    return GameState::NotFound;
}

// swd3e.exe 0x0040DB00: role `id` leaves the followers: a role between tiles is moved on to the next one along its
// last step, its follow move freed; it takes the act, path, event and flags given (its record too), takes its cells,
// and the followers after it move up. A role not on the map only loses the follow flag of its record.
void Game::LeaveParty(u16 id, u16 path, u16 event, i32 act, i32 action, i32 direction, u16 flags)
{
    GameState& game = this->state;
    Scene& view = this->scene;
    u32 index = 0;

    if (!game.FindRoleOrPlayer(id, index))
    {
        std::vector<u8>& data = this->databases.mapsData;

        // swd3e still steps through the table by swd32's records of 11 words (its records have 12), so after the
        // first it compares and clears the words at shifting places in the records (a bug kept).
        for (std::size_t at = view.rolesTable; at < data.size() && MapsWord(data, at) != MapsRecord::TableEnd;
             at += kLegacyRoleBytes)
        {
            if (MapsWord(data, at + sizeof(u16) * MapsRecord::RoleId) == id)
            {
                if (at + sizeof(u16) * MapsRecord::RoleFlags + 1 < data.size())
                {
                    data[at + sizeof(u16) * MapsRecord::RoleFlags] &= static_cast<u8>(~Role::Follower);
                }

                return;
            }
        }

        ErrorBox(this->errors, kLeavePartyTitle, u8"角色(%d)不存在", id);  // 0x004B1048
        return;
    }

    std::size_t follower = 0;

    while (follower < view.followers.size() && view.followers[follower] != static_cast<i32>(index))
    {
        ++follower;
    }

    if (follower == view.followers.size() || index >= GameState::Roles)
    {
        ErrorBox(this->errors, kLeavePartyTitle, u8"角色(%d)不是夥伴", id);  // 0x004B1064
        return;
    }

    Role& role = game.roles[index];
    MoveRecord& move = view.followerMoves[follower];
    const i32 leader = this->partyLeaderId;

    if (move.step < MoveRecord::StepMask &&
        ((role.x & MapHeader::TileFraction) != 0 || (role.y & MapHeader::TileFraction) != 0))
    {
        this->field.ClearRoleCells(role, leader);
        const u8 last = move.CurrentDirection();

        if (last != MoveRecord::EndOfPath && last < RolePath::Directions)
        {
            role.x = RolePath::TileStart(role.x, -RolePath::StepX[last]);
            role.y = RolePath::TileStart(role.y, -RolePath::StepY[last]);
        }

        this->field.RemoveRole(
            role.id,
            static_cast<int>(role.flags & Role::LayerMask),
            static_cast<i32>(static_cast<u32>(role.y) >> MapHeader::TileShift) - 1,
            0
        );
    }

    move.Clear();
    role.act.id = act;
    role.act.action = action;
    role.act.direction = direction;
    role.event = event;
    role.pathNumber = path;
    role.pathStep = 0;
    role.flags = flags;
    this->UpdateRoleRecord(
        role.id,
        Game::KeepValue,
        Game::KeepValue,
        Game::KeepValue,
        Game::KeepValue,
        Game::KeepValue,
        static_cast<i16>(event),
        static_cast<i16>(path),
        0,
        static_cast<i16>(~Role::Follower),
        Game::KeepValue
    );
    this->field.MarkRoleCells(role, leader);
    std::move(
        view.followers.begin() + static_cast<std::ptrdiff_t>(follower) + 1,
        view.followers.end(),
        view.followers.begin() + static_cast<std::ptrdiff_t>(follower)
    );
    std::move(
        view.followerMoves.begin() + static_cast<std::ptrdiff_t>(follower) + 1,
        view.followerMoves.end(),
        view.followerMoves.begin() + static_cast<std::ptrdiff_t>(follower)
    );
    --view.followerCount;
}

// swd3e.exe 0x00433490: a scripted change of map: the keys and steps stop, the map frame is hidden while LoadCity
// runs (Talk::Keep keeps the controlled role's tile, act, action or direction)
bool Game::ChangeMap(int map, int x, int y, int act, int action, int direction, int options)
{
    const Role& player = this->state.roles[static_cast<std::size_t>(this->state.playerRole)];
    this->scrollStepY = 0;
    this->controls.buttons = {};
    this->scrollStepX = 0;
    this->walkStepY = 0;
    this->walkStepX = 0;
    this->scene.screenFlags |= Scene::BlackScreen;
    const i32 newAct = act == Talk::Keep ? player.act.id : act;
    const i32 newAction = action == Talk::Keep ? player.act.action : action;
    const i32 newDirection = direction == Talk::Keep ? player.act.direction : direction;
    const int column = x == Talk::Keep ? static_cast<int>(static_cast<u32>(player.x) >> MapHeader::TileShift) : x;
    const int row = y == Talk::Keep ? static_cast<int>(static_cast<u32>(player.y) >> MapHeader::TileShift) : y;
    this->LoadCity(
        map,
        column,
        row,
        static_cast<i16>(newAct),
        static_cast<i16>(newAction),
        static_cast<i16>(newDirection),
        options
    );
    this->scene.screenFlags &= ~Scene::BlackScreen;
    return true;
}

// swd3e.exe 0x00435A60: a timed event in frames (30 a second): the second timer (`which` nonzero, flag 0x4A) or the
// first with its event (flags 0x10 and 0x12)
void Game::StartTimer(int minutes, int seconds, int event, int which)
{
    Scene& view = this->scene;

    if (which != 0)
    {
        view.unk4BAB78 = 0;
        view.unk4BAB7C = 0;
        view.secondTimerFrames = kTimerFrames * (seconds + kSecondsPerMinute * minutes);
        this->state.SetGameFlag(GameState::SecondTimer);
        return;
    }

    view.unk4C97E8 = 0;
    view.unk4C97EC = 0;
    view.timerFrames = kTimerFrames * (seconds + kSecondsPerMinute * minutes);
    view.timerEvent = event;
    this->state.SetGameFlag(GameState::Timer);
    this->state.SetGameFlag(GameState::TimerEvent);
}

// swd3e.exe 0x0049C600: the movie named in the script after the command word (up to "%Q", the event offset `pc`
// moving along) from the video folder (by the installation type the installation's Video\, the CD's swd3e\Video\
// after a CD check, or the name alone; the rebuild reads the installation's, see deviations.md), at volume 100; the
// frames then take the place of the map's (flag 0x20). A movie already open is closed here (the original leaves it
// open behind the new one, deviations.md).
bool Game::PlayVideo(std::size_t& cursor, u16& pc)
{
    constexpr int kVideoVolume = 0x64;
    const std::span<const u8> script(this->databases.script);
    std::string name;

    auto word = [&script](std::size_t at)
    {
        return at + 1 < script.size() ? static_cast<u16>(script[at] | script[at + 1] << CHAR_BIT) : u16{0};
    };

    pc = static_cast<u16>(pc + sizeof(u16));
    cursor += sizeof(u16);

    while (cursor < script.size() && word(cursor) != MapsRecord::NameEnd)
    {
        ++pc;
        name.push_back(static_cast<char>(script[cursor]));
        ++cursor;
    }

    pc = static_cast<u16>(pc + sizeof(u16));
    cursor += sizeof(u16);
    const std::u8string path = this->config.installDirectory + u8"Video\\" + PathFormat("%s", {name});
    LogInfo(u8"video %s", path);
    this->movie.Close();
    this->movie.Open(path, kVideoVolume);
    this->videoLoaded = true;
    this->systemFlags |= Game::VideoPlaying;
    this->graphics.PrimarySize(&this->videoPitch, nullptr);
    return true;
}

// swd3e.exe 0x0049C890: the frame of the movie, negated past its last; -1 without one
int Game::VideoPosition() const
{
    return this->videoLoaded ? this->movie.Position() : -1;
}

// swd3e.exe 0x0049C8B0: two values a script gives at a frame of the movie (talk command 0xC3); false without a movie.
// Nothing reads them.
bool Game::SetVideoMarks(i32 first, i32 second)
{
    if (!this->videoLoaded)
    {
        return false;
    }

    this->videoMarkA = first;
    this->videoMarkB = second;
    return true;
}

// swd3e.exe 0x0049C7B0: the movie closed and freed; a requested quit closes the window; flag 0x20 cleared
void Game::StopVideo()
{
    this->movie.Close();
    this->videoLoaded = false;

    if ((this->systemFlags & Game::QuitRequested) != 0)
    {
        this->Destroy();
    }

    this->systemFlags &= ~Game::VideoPlaying;
}

// swd3e.exe 0x0049C800: the next frame of the movie (unless it waits for its time) copied to the top left of the
// screen surface and shown; after its last frame the movie stops. The original crashes on a movie Bink could not open;
// the rebuild stops it (deviations.md).
void Game::VideoFrame()
{
    if (!this->videoLoaded)
    {
        return;
    }

    if (!this->movie.IsOpen())
    {
        this->StopVideo();
        return;
    }

    if (this->movie.Waiting() || this->offscreen == nullptr)
    {
        return;
    }

    const int left = this->movie.Frame(*this->offscreen);
    this->ShowFrame();

    if (left <= 0)
    {
        this->StopVideo();
    }
}

}  // namespace swd3e
