#include <algorithm>
#include <bit>
#include <climits>
#include <cmath>
#include <cstring>
#include <span>
#include <string>

#include <SDL3/SDL.h>

#include "core/debug_log.h"
#include "core/error_box.h"
#include "core/fault.h"
#include "core/format.h"
#include "core/words.h"
#include "crt/msvc.h"
#include "game/game.h"
#include "platform/system_info.h"
#include "role/path_script.h"
#include "role/role_path.h"
#include "talk/talk_event.h"

namespace swd3e
{

namespace
{

// The effect bits of a map (maps.dat table 3, byte 5).
constexpr u8 kFireflies = 1;
constexpr u8 kSpotlight = 2;
constexpr u8 kLightning = 4;
constexpr u8 kHeatHaze = 8;
constexpr u8 kLeaves = 0x10;
constexpr u8 kShadeWeather = 0x20;
constexpr u8 kGlowWeather = 0x40;
constexpr u8 kOverlaysFirst = 0x80;

constexpr int kSceneFadeSteps = 0xF;  // MusicZone: the updates the playing piece fades over
constexpr int kSceneRepeatMask = 3;   // how the scene tunes repeat (MusicQueue::Switch), in the marks of a map
constexpr int kLoadingAction = 0x4F;
constexpr int kLoadingBarWidth = 0x174;
constexpr std::size_t kScreenBytes = sizeof(Pixel) * Display::Pixels;
constexpr u16 kPartyLeader = 1;

// The loading screen: the bar shades from (kBarRed, kBarGreen, kBarBlue) by the steps per column in 1/kColorFraction
// (8-bit channels, kChannelDrop bits dropped for the display); the runner walks at its end.
constexpr int kLoadingStart = -1;
constexpr int kLoadingSettings = 2;  // the progress of LoadCity
constexpr int kLoadingCleared = 10;
constexpr int kLoadingNamed = 85;
constexpr int kLoadingWeather = 90;
constexpr int kLoadingRoles = 95;
constexpr int kLoadingDone = 100;
constexpr u32 kRunners = 2;  // the runner is act 1 or 2
constexpr i32 kBarRed = 0x1F;
constexpr i32 kBarGreen = 0x73;
constexpr i32 kBarBlue = 0xFF;
constexpr int kBarRedStep = 0x268;
constexpr int kBarGreenStep = 0xE7;
constexpr int kBarBlueFall = 0x2BD;
constexpr int kColorFraction = 0x400;
constexpr int kChannelDrop = 3;
constexpr int kBarLeft = 0x7B;
constexpr int kBarTop = 0x1BD;
constexpr int kBarHeight = 0x1E;
constexpr u16 kLoadingPicture = 0x23AF;  // swd3e: the picture of the bar, drawn straight
constexpr int kPictureX = 0x78;
constexpr int kPictureY = 0x1BA;
constexpr int kStartPictureY = 0xCA;  // the picture before the screen is cleared
constexpr int kRunnerX = 0x6B;        // past the end of the bar
constexpr int kRunnerY = 0x1CC;

// Map settings (maps.dat table 3): the kind and speed bytes hold nibbles.
constexpr int kNibbleBits = 4;
constexpr u16 kNibbleMask = 0xF;
constexpr u8 kNoUnk4ACDB4 = 0x10;    // in the kind: townWarpAllowed is 0
constexpr u32 kMaxWalkSpeed = 0x10;  // the walk speed is a power of two up to this
constexpr i32 kDefaultWalkSpeed = 4;
constexpr u32 kStartSpeeds = 3;  // a weather particle starts at speed 1..kStartSpeeds
constexpr int kStartDrift = 2;

// The roles of maps.dat's role table.
constexpr u16 kEveryMapRole = 10000;  // records of these roles (and of none) belong to every map
constexpr u16 kEveryMapRole2 = 10001;
constexpr u16 kLeaderFlags = Role::Visible | Role::Solid | Role::StepsAct | Role::CellEffects;
constexpr u16 kStoryActMark = 0x8000;            // in mapStory: the party leader takes a story act
constexpr std::size_t kFirstVisitPairBytes = 4;  // table 0x19: a map and a story flag
constexpr i32 kGhostFirstAct = 0x5A;             // the talisman ghost's act: its variable added to it
constexpr i32 kGhostAction = 1;
constexpr float kGhostUnk14 = 2.0F;  // GhostFollower::Spawn's last two values
constexpr float kGhostUnk18 = 4.0F;
constexpr std::u8string_view kLoadMapTitle = u8"載入地圖";  // 0x004B0F7C
constexpr std::u8string_view kRolesTitle = u8"讀取路人";    // 0x004B0ED0

constexpr int kSoundRange = 0x200;  // pixels a map sound is heard within
constexpr int kSoundScale = 0x80;
constexpr int kPanScale = 0x40;
constexpr int kPlayOnce = 1;

// The encounter sets (maps.dat table 0x12): their count, then MapsRecord::GroupWords words each: kSetPairs pairs of a
// step threshold and a chance in percent.
constexpr int kSetPairs = 4;
constexpr std::size_t kPairWords = 2;
constexpr u32 kPercent = 100;
constexpr int kForcedEncounter = 1;  // RollEncounter `force`: the thresholds and the chance are skipped
constexpr i32 kTileMask = 0xF;       // ReleaseRole: the pixels within a tile

}  // namespace

// swd3e.exe 0x0040F0D0: the loading screen: a bar of `percent` (its colour shading along it) under picture 0x23AF, a
// random runner (1 or 2) at its end; -1 starts it, 100 ends it. Nothing while flag 0x46 is set.
void Game::ShowLoading(int percent)
{
    Scene& view = this->scene;
    int shown = percent;

    if (percent == kLoadingStart)
    {
        this->loadingPercent = 0;
        this->loadingUnusedA = 0;
        shown = 0;
        this->loadingRunner = static_cast<i32>(this->random.Below(kRunners)) + 1;
        this->loadingRed = kBarRed;
        this->loadingGreen = kBarGreen;
        this->loadingBlue = kBarBlue;
        this->loadingUnusedB = 0;
        ResetPlayback(view.loadingPicture);
        ResetPlayback(this->loadingAct);
    }

    if (!this->state.GameFlag(GameState::NoLoadingScreen))
    {
        this->drawSurface = this->painter.LockSurface(*this->offscreen);
        this->painter.SetTarget(this->drawSurface);
        this->painter.UnlockSurface(*this->offscreen);
        std::memset(this->drawSurface, 0, kScreenBytes);
        const int length = kLoadingBarWidth * shown / kLoadingDone;
        const int pitch = this->offscreen->Width();
        int red = 0;
        int green = 0;
        int blue = 0;

        for (int column = 0; column <= length; ++column)
        {
            this->loadingBlue = blue / kColorFraction + kBarBlue;
            this->loadingGreen = green / kColorFraction + kBarGreen;
            this->loadingRed = red / kColorFraction + kBarRed;
            const auto color = static_cast<u16>(this->painter.MakeColor(
                static_cast<u32>(this->loadingRed >> kChannelDrop),
                static_cast<u32>(this->loadingGreen >> kChannelDrop),
                static_cast<u32>(this->loadingBlue >> kChannelDrop)
            ));
            u16* pixel = this->drawSurface + pitch * kBarTop + column + kBarLeft;

            for (int row = 0; row < kBarHeight; ++row)
            {
                *pixel = color;
                pixel += pitch;
            }

            red += kBarRedStep;
            green += kBarGreenStep;
            blue -= kBarBlueFall;
        }

        this->loadingPercent = shown;
        this->actDrawer.DrawPicture(kLoadingPicture, 0, kPictureX, kPictureY);
        this->loadingAct.id = this->loadingRunner;
        this->loadingAct.action = Role::RunAction;
        this->loadingAct.direction = ActState::Right;
        this->actDrawer.Draw(this->loadingAct, length + kRunnerX, kRunnerY);
        this->ShowFrame();
    }

    if (percent == kLoadingDone)
    {
        this->state.ClearGameFlag(GameState::NoLoadingScreen);
    }
}

// swd3e.exe 0x0040D170: the music of map `map` (maps.dat's music table: its two tunes and how they repeat). The same
// tunes only take back the music from a story tune that ends (story repeat 1); new tunes become the first list of the
// music queue (<install>music\<name>) and play after the current piece fades over 15 updates, unless a story tune
// plays that does not end. A map without music clears the scene's tunes and its cue bits.
void Game::MusicZone(int map)
{
    Scene& view = this->scene;
    MusicQueue& queue = this->audio.Queue();
    const std::vector<u8>& data = this->databases.mapsData;
    const u32 lists = MapsTable(data, MapsDataTable::Music);
    u32 at = MapsDword(data, MapsDword(data, lists + sizeof(u32) * MapsRecord::SceneMusicItem));

    for (;; at += MapsRecord::MusicBytes)
    {
        const u16 entry = MapsWord(data, at);

        if (entry == 0 || at + 1 >= data.size())
        {
            view.musicTunes[Scene::SceneFirst] = 0;
            view.musicTunes[Scene::SceneSecond] = 0;
            view.musicCue &= ~(Scene::SceneLoops | Scene::SceneRepeatFirst);
            return;
        }

        if (static_cast<int>(entry) == map)
        {
            break;
        }
    }

    const i32 marks = MapsWord(data, at + MapsRecord::MusicMarksAt);
    const i32 first = MapsWord(data, at + MapsRecord::MusicFirstAt);
    const i32 second = MapsWord(data, at + MapsRecord::MusicSecondAt);
    const int repeat = marks & kSceneRepeatMask;

    auto endStory = [&view, &queue, repeat]
    {
        queue.Switch(MusicQueue::FirstList, repeat, kSceneFadeSteps);
        view.musicTunes[Scene::StoryFade] = 0;
        view.musicTunes[Scene::StoryFirst] = 0;
        view.musicTunes[Scene::StorySecond] = 0;
    };

    if (first == view.musicTunes[Scene::SceneFirst] && second == view.musicTunes[Scene::SceneSecond])
    {
        if (queue.Mode() == MusicQueue::SecondList && view.musicTunes[Scene::StoryFade] == 1)
        {
            endStory();
        }

        return;
    }

    view.musicTunes[Scene::SceneFade] = marks;
    view.musicTunes[Scene::SceneFirst] = first;
    view.musicTunes[Scene::SceneSecond] = second;

    for (std::size_t piece = 0; piece < MusicQueue::Pieces; ++piece)
    {
        const i32 tune = view.musicTunes[Scene::SceneFirst + piece];
        queue.SetSlot(this->MusicPath(tune), tune, piece, MusicQueue::FirstList);
    }

    view.musicCue = 0;

    if (queue.Mode() != MusicQueue::SecondList)
    {
        queue.Switch(MusicQueue::FirstList, repeat, kSceneFadeSteps);
    }
    else if (view.musicTunes[Scene::StoryFade] == 1)
    {
        endStory();
    }
}

// swd3e.exe 0x0040D3B0: the sound of a role and its period (in both halves of the timer) from maps.dat's sound table;
// false when it has none
bool Game::LoadRoleSound(Role& role)
{
    const std::vector<u8>& data = this->databases.mapsData;

    for (u32 at = MapsTable(data, MapsDataTable::RoleSounds); at + 1 < data.size(); at += MapsRecord::SoundBytes)
    {
        const u16 id = MapsWord(data, at);

        if (id == 0)
        {
            return false;
        }

        if (id == role.id)
        {
            role.sound = MapsWord(data, at + MapsRecord::SoundAt);
            const u16 period = MapsWord(data, at + MapsRecord::SoundPeriodAt);
            role.soundTimer = MakeWords(period, period);
            return true;
        }
    }

    return false;
}

// swd3e.exe 0x0040DD50: the encounter areas of the current map (maps.dat's table 7) put in front of the list
void Game::BuildTriggers(MapTrigger& head)
{
    const std::vector<u8>& data = this->databases.mapsData;

    for (u32 at = MapsTable(data, MapsDataTable::EncounterAreas); MapsWord(data, at) != MapsRecord::TableEnd;
         at += MapsRecord::AreaBytes)
    {
        if (static_cast<i32>(MapsWord(data, at)) == this->currentMap)
        {
            auto node = std::make_unique<MapTrigger>();
            node->left = MapsWord(data, at + MapsRecord::AreaLeftAt);
            node->top = MapsWord(data, at + MapsRecord::AreaTopAt);
            node->right = MapsWord(data, at + MapsRecord::AreaRightAt);
            node->bottom = MapsWord(data, at + MapsRecord::AreaBottomAt);
            node->table = MapsDword(data, at + MapsRecord::AreaTableAt);
            node->next = std::move(head.next);
            head.next = std::move(node);
        }
    }
}

// swd3e.exe 0x0040F5C0: a role entering the map: its action started, its cell of the grid and the marks of a mark
// cell (bit 11 sets flag 29, bits 12..15 go to flags 20..23); a solid role takes its cells
void Game::InitRole(Role& role)
{
    role.act.flags = 0;

    if (this->acts.Run(role.act) == 0)
    {
        ReportActError(this->errors, u8"Act Err(Misc:newRoleInit)", role);
    }

    role.flags &= ~(Role::Reflects | Role::CellShades);
    const auto column = static_cast<u32>(role.x) >> MapHeader::TileShift;
    const auto row = static_cast<u32>(role.y) >> MapHeader::TileShift;
    const std::size_t at = column + static_cast<std::size_t>(this->state.map.width) * static_cast<std::size_t>(row);
    std::vector<u32>& blocks = this->state.map.blocks;
    role.cell = at < blocks.size() ? &blocks[at] : nullptr;

    if ((role.flags & Role::CellEffects) != 0 && role.cell != nullptr)
    {
        const u32 cell = *role.cell;

        if ((cell & MapHeader::Reflecting) != 0)
        {
            role.flags |= Role::Reflects;
        }

        if ((cell & MapHeader::ShadeMask) != 0)
        {
            role.flags = (role.flags & ~Role::CellShades) | (cell & MapHeader::ShadeMask) << MapHeader::ShadeShift;
        }
    }

    if (role.act.id != 0 && (role.flags & (Role::Visible | Role::Hidden)) == Role::Visible)
    {
        this->field.MarkRoleCells(role, this->partyLeaderId);
    }
}

// swd3e.exe 0x0040F310: the name of map `map` (maps.dat's name table, Big5 ending with 0x5125); false when it has none
bool Game::MapName(int map, std::string& name) const
{
    const std::vector<u8>& data = this->databases.mapsData;
    std::size_t at = MapsTable(data, MapsDataTable::PlaceNames);
    u16 code = MapsWord(data, at);

    if (code == MapsRecord::TableEnd)
    {
        return false;
    }

    while (static_cast<int>(code) != map)
    {
        if (code != MapsRecord::NameEnd)
        {
            do
            {
                ++at;
            } while (at + 1 < data.size() && MapsWord(data, at) != MapsRecord::NameEnd);
        }

        code = MapsWord(data, at + MapsRecord::PlaceNameAt);
        at += sizeof(u16);

        if (code == MapsRecord::TableEnd || at + 1 >= data.size())
        {
            return false;
        }
    }

    name.clear();

    for (std::size_t character = at + MapsRecord::PlaceNameAt;
         character + 1 < data.size() && MapsWord(data, character) != MapsRecord::NameEnd;
         ++character)
    {
        name.push_back(static_cast<char>(data[character]));
    }

    return true;
}

// swd3e.exe 0x0040D7C0: changes the record of role `id` in maps.dat's role table (0xFFFF leaves a value, 0x8080 the
// role variables; the act's new action clears its step); false when the table lacks it
bool Game::UpdateRoleRecord(
    u16 id,
    i16 act,
    i16 action,
    i16 direction,
    i16 x,
    i16 y,
    i16 event,
    i16 talk,
    i16 setBits,
    i16 clearBits,
    i16 map,
    i16 variables
)
{
    std::vector<u8>& data = this->databases.mapsData;

    for (std::size_t at = this->scene.rolesTable;; at += static_cast<std::size_t>(sizeof(u16) * MapsRecord::RoleWords))
    {
        if (at + static_cast<std::size_t>(sizeof(u16) * MapsRecord::RoleWords) > data.size() ||
            MapsWord(data, at) == MapsRecord::TableEnd)
        {
            DebugLog(u8"未定義角色[%d]", id);
            return false;
        }

        if (MapsWord(data, at + sizeof(u16) * MapsRecord::RoleId) == id)
        {
            auto put = [&data, at](int word, i16 value)
            {
                const std::size_t offset = at + sizeof(u16) * static_cast<std::size_t>(word);
                data[offset] = static_cast<u8>(value);
                data[offset + 1] = static_cast<u8>(static_cast<u16>(value) >> CHAR_BIT);
            };

            auto get = [&data, at](int word)
            {
                return static_cast<i16>(MapsWord(data, at + sizeof(u16) * static_cast<std::size_t>(word)));
            };

            if (act != -1)
            {
                put(MapsRecord::RoleAct, act);
            }

            if (action != -1)
            {
                put(MapsRecord::RoleAction, action);
            }

            if (direction != -1)
            {
                put(MapsRecord::RoleDirection, direction);
            }

            if (x != -1)
            {
                put(MapsRecord::RoleColumn, x);
            }

            if (y != -1)
            {
                put(MapsRecord::RoleRow, y);
            }

            if (event != -1)
            {
                put(MapsRecord::RoleEvent, event);
            }

            if (talk != -1)
            {
                put(MapsRecord::RolePath, talk);
                put(MapsRecord::RoleStep, 0);
            }

            if (clearBits != -1)
            {
                put(MapsRecord::RoleFlags, static_cast<i16>(get(MapsRecord::RoleFlags) & clearBits));
            }

            if (setBits != -1)
            {
                put(MapsRecord::RoleFlags, static_cast<i16>(get(MapsRecord::RoleFlags) | setBits));
            }

            if (map != -1)
            {
                put(MapsRecord::RoleMap, map);
            }

            if (variables != Game::KeepVariables)
            {
                put(MapsRecord::RoleVariables, variables);
            }

            return true;
        }
    }
}

// swd3e.exe 0x0040D560: the roles of the map written back to maps.dat before it is left: the visible named roles not
// controlled (a role marked by bit 7 of its flags with its place, act and story values; the others go to their tile, a
// role on a path to the end of its move)
void Game::SaveRoleStates(i16 x, i16 y)
{
    for (int index = 1; index != this->state.roleCount; ++index)
    {
        Role copy = this->state.roles[static_cast<std::size_t>(index)];

        if ((copy.flags & (Role::Removed | Role::Unk8000000)) != 0 || copy.id == Role::NoId ||
            index == this->state.playerRole)
        {
            continue;
        }

        if ((copy.flags & Role::Follower) == 0)
        {
            if (copy.pathNumber != 0)
            {
                const u8* paths = this->databases.pathData;
                const std::span<const u8> pathBytes =
                    paths != nullptr ? std::span<const u8>(paths, this->databases.paths.Size()) : std::span<const u8>();
                const u32 path = PathScript::PathStart(pathBytes, static_cast<std::size_t>(copy.pathNumber));
                copy.x &= ~MapHeader::TileFraction;
                copy.y &= ~MapHeader::TileFraction;
                const std::size_t step =
                    PathScript::WordBytes * static_cast<std::size_t>(static_cast<u32>(copy.pathStep));

                if (PathScript::Word(pathBytes, PathScript::DataAt + path + step) ==
                    static_cast<u16>(PathCommand::Step))
                {
                    for (const MoveRecord& move : this->state.moves)
                    {
                        if (move.role == index)
                        {
                            copy.x = move.x;
                            copy.y = move.y;

                            if (static_cast<u32>(move.x) >
                                    static_cast<u32>(MapHeader::TileSize * this->state.map.width) ||
                                move.y > MapHeader::TileSize * this->state.map.height)
                            {
                                ErrorBox(
                                    this->errors,
                                    u8"角色回寫",
                                    u8"角色(%d)\n終點位置(%d,%d)\n不在地圖(%d,%d)內",
                                    copy.id,
                                    move.x,
                                    move.y,
                                    MapHeader::TileSize * this->state.map.width,
                                    MapHeader::TileSize * this->state.map.height
                                );
                            }

                            break;
                        }
                    }

                    ++copy.pathStep;
                }
            }

            this->StoreRole(copy);
        }
        else
        {
            this->UpdateRoleRecord(
                copy.id,
                static_cast<i16>(copy.act.id),
                static_cast<i16>(copy.act.action),
                static_cast<i16>(copy.act.direction),
                x,
                y,
                static_cast<i16>(copy.event),
                static_cast<i16>(copy.pathNumber),
                static_cast<i16>(copy.flags),
                0,
                static_cast<i16>(this->currentMap)
            );
        }
    }
}

// swd3e.exe 0x0040D980: role `index` joins the followers of the leader: a role on a path is moved back to the start of
// its tile by its last step and taken off its move; its record gets flag 0x80
void Game::FollowLeader(int index)
{
    Role& role = this->state.roles[static_cast<std::size_t>(index)];

    if (role.pathNumber != 0)
    {
        for (MoveRecord& move : this->state.moves)
        {
            if (move.role != index)
            {
                continue;
            }

            if ((role.x & MapHeader::TileFraction) != 0 || (role.y & MapHeader::TileFraction) != 0)
            {
                this->field.ClearRoleCells(role, this->partyLeaderId);
                const u8 direction = move.CurrentDirection();

                if (direction < RolePath::Directions)
                {
                    role.x = RolePath::TileStart(role.x, RolePath::StepX[direction]);
                    role.y = RolePath::TileStart(role.y, RolePath::StepY[direction]);
                }

                this->field.RemoveRole(
                    role.id,
                    static_cast<int>(role.flags & Role::LayerMask),
                    (role.y >> MapHeader::TileShift) - 1,
                    0
                );
            }

            this->UpdateRoleRecord(
                role.id,
                Game::KeepValue,
                Game::KeepValue,
                Game::KeepValue,
                Game::KeepValue,
                Game::KeepValue,
                Game::KeepValue,
                Game::KeepValue,
                Role::Follower,
                Game::KeepValue,
                Game::KeepValue
            );
            move.Clear();
            break;
        }
    }

    Scene& view = this->scene;
    const auto follower = static_cast<std::size_t>(view.followerCount);

    if (follower < view.followers.size())
    {
        view.followers[follower] = index;
        view.followerMoves[follower].Clear();
    }

    role.event = 0;
    role.flags = (role.flags & ~Role::Solid) | Role::Follower;
    ++view.followerCount;
}

// swd3e.exe 0x004121C0 (the wait for the disc of the minimal installation, the videos and the map movies) and
// 0x00411FD0 (the probe of the volume label "SWD3E_1" it uses): removed; the rebuild never looks for a disc and reads
// everything from the installation (see deviations.md)

// swd3e.exe 0x0040C570: enters map `map` with the leader at (x, y) tiles in act `act`, action and direction. The map's
// settings (table 3: its unit of HUGE.lmf, its kind, the story flags of its effects, its speeds and tints), the music,
// the talisman ghost removed, the story flags of table 0x19 for a first visit (with flag 0x19), the old map's roles
// written back (option bit 0), every scene list and role cleared, the map loaded (8-bit maps take their palette from
// the unit), its name, the weather, the encounter areas, then the roles of maps.dat's role table on this map: the
// leader moved to the entrance and made the controlled role with its trail (the ghost appears with it after flag
// 0x19), followers, bubbles; the roles enter, the camera is centred. A map missing from the tables ends the game.
void Game::LoadCity(int map, int x, int y, i16 act, i16 action, i16 direction, int options)
{
    Scene& view = this->scene;
    GameState& game = this->state;
    LogInfo(u8"map %d at (%d, %d) from map %d", map, x, y, this->currentMap);
    game.ClearFrameInterval();

    if (this->drawSurface != nullptr)
    {
        view.loadingPicture.id = Scene::InterfaceAct;
        view.loadingPicture.action = kLoadingAction;
        view.loadingPicture.direction = 0;
        view.loadingPicture.x = 0;
        view.loadingPicture.y = 0;
        this->actDrawer.Draw(view.loadingPicture, kPictureX, kStartPictureY);
        this->ShowLoading(kLoadingStart);
    }

    view.screenFlags = 0;
    game.ClearGameFlag(GameState::Weather);
    game.ClearGameFlag(GameState::Leaves);

    view.weather.fill({MapParticle::Filled, MapParticle::Filled, MapParticle::Filled, MapParticle::Filled});

    ResetPlayback(view.effectAct);
    view.ResetLeaves();
    game.ClearGameFlag(GameState::HeatHaze);
    game.ClearGameFlag(GameState::Lightning);
    std::fill_n(view.bolts.begin(), Scene::ClearedBolts, Streak{});
    view.lightningShown = 0;
    view.lightningCount = 0;
    game.ClearGameFlag(GameState::Fireflies);
    std::fill_n(view.bolts.begin(), Scene::ClearedBolts, Streak{});
    view.firefliesShown = 0;
    view.spotlightX = Display::Width / 2;
    view.spotlightTargetX = Display::Width / 2;
    view.fireflyCount = 0;
    view.spotlightY = Display::Height / 2;
    view.spotlightTargetY = Display::Height / 2;
    view.spotlightStepX = 0;
    view.spotlightStepY = 0;
    this->acts.CloseAll();
    this->archive.Flush();
    this->audio.StopAllSounds();
    std::vector<u8>& data = this->databases.mapsData;
    u16 unit = MapsRecord::TableEnd;

    for (u32 at = MapsTable(data, MapsDataTable::MapSettings); MapsWord(data, at) != MapsRecord::TableEnd;
         at += MapsRecord::SettingsBytes)
    {
        if (MapsWord(data, at) != static_cast<u16>(map))
        {
            continue;
        }

        unit = MapsWord(data, at + MapsRecord::SettingsUnitAt);
        const u8 kind = data[at + MapsRecord::SettingsKindAt];
        const u8 effects = data[at + MapsRecord::SettingsEffectsAt];
        const u16 speeds = MapsWord(data, at + MapsRecord::SettingsSpeedsAt);
        this->field.shadowKind = kind & kNibbleMask;
        view.mapWalkSpeed = data[at + MapsRecord::SettingsSpeedsAt] & kNibbleMask;
        view.layerFrames = MapsWord(data, at + MapsRecord::SettingsUnk49E0BCAt);
        view.encounterGroup = MapsWord(data, at + MapsRecord::SettingsUnk4B896CAt);
        view.mapStory = MapsWord(data, at + MapsRecord::SettingsUnk4B88C0At);
        view.townWarpAllowed = (kind & kNoUnk4ACDB4) == 0 ? 1 : 0;
        game.ClearGameFlag(GameState::OverlaysFirst);

        if ((effects & kOverlaysFirst) != 0)
        {
            game.SetGameFlag(GameState::OverlaysFirst);
        }

        view.weatherAction = Scene::GlowWeatherFirstAction;
        view.weatherActions = Scene::GlowWeatherActions;
        game.ClearGameFlag(GameState::Weather);

        if ((effects & kGlowWeather) != 0)
        {
            game.SetGameFlag(GameState::Weather);
        }

        if ((effects & kShadeWeather) != 0)
        {
            game.SetGameFlag(GameState::Weather);
            view.weatherAction = Scene::ShadeWeatherFirstAction;
            view.weatherActions = Scene::ShadeWeatherActions;
        }

        constexpr std::array<std::pair<u8, int>, 5> kEffectFlags = {{
            {kLeaves, GameState::Leaves},
            {kHeatHaze, GameState::HeatHaze},
            {kLightning, GameState::Lightning},
            {kSpotlight, GameState::Spotlight},
            {kFireflies, GameState::Fireflies},
        }};

        for (const auto& [bit, flag] : kEffectFlags)
        {
            game.ClearGameFlag(flag);

            if ((effects & bit) != 0)
            {
                game.SetGameFlag(flag);
            }
        }

        view.cellTintRed = Game::SpeedNibbles[speeds >> (3 * kNibbleBits)];
        view.cellTintGreen = Game::SpeedNibbles[(speeds >> (2 * kNibbleBits)) & kNibbleMask];
        view.cellTintBlue = Game::SpeedNibbles[data[at + MapsRecord::SettingsSpeedsAt] >> kNibbleBits];

        if (!std::has_single_bit(static_cast<u32>(view.mapWalkSpeed)) ||
            static_cast<u32>(view.mapWalkSpeed) > kMaxWalkSpeed)
        {
            view.mapWalkSpeed = kDefaultWalkSpeed;
        }

        if (view.layerFrames < 1)
        {
            view.layerFrames = 1;
        }

        if (!view.encounterGroups.empty() &&
            static_cast<i16>(view.encounterGroup) > static_cast<i16>(view.encounterGroups.front()))
        {
            ErrorBox(this->errors, kLoadMapTitle, u8"錯誤的戰鬥機率組[%d]", view.encounterGroup);  // 0x004B0F88
            view.encounterGroup = 1;
        }

        break;
    }

    this->MusicZone(map);
    this->ghostFollower.Remove();
    game.ClearGameFlag(GameState::GhostAppears);

    // Table 0x19 (swd3e): pairs of a map and a story flag, ended by map 0. A first visit sets the flag and 0x19.
    for (u32 at = MapsTable(data, MapsDataTable::FirstVisitFlags); MapsWord(data, at) != 0; at += kFirstVisitPairBytes)
    {
        const u16 flag = MapsWord(data, at + sizeof(u16));

        if (MapsWord(data, at) == static_cast<u16>(map) && !game.GameFlag(flag))
        {
            game.SetGameFlag(flag);
            game.SetGameFlag(GameState::GhostAppears);
        }
    }

    if (unit == MapsRecord::TableEnd)
    {
        ErrorBox(this->errors, u8"遊戲初始", u8"找不到設定的起始地圖碼 (%d)", map);  // 0x004B0F54, 0x004B0F60
        this->Destroy();
        return;
    }

    this->ShowLoading(kLoadingSettings);
    this->currentMap = map;
    view.layerDirection = 1;
    game.SetGameFlag(map + GameState::MapEntered);

    if ((options & Game::SaveRoles) != 0)
    {
        this->SaveRoleStates(static_cast<i16>(x), static_cast<i16>(y));
    }

    view.encounterSteps = 0;
    view.soundRise = {};
    view.triggers.next.reset();
    view.FreeAnimations();
    view.FreeEffects();
    view.FreeSpritesA();
    view.FreeSpritesB();
    game.clickRegions.Free();
    view.FreeNodes();
    view.FreeMarks();
    view.ResetLeaves();
    view.FreeObjects();
    this->controls.buttons = {};
    game.ClearRoles();

    for (MoveRecord& move : game.moves)
    {
        move.Clear();
    }

    view.followerCount = 0;
    view.followers.fill(-1);

    for (MoveRecord& move : view.followerMoves)
    {
        move.Clear();
    }

    this->field.overlays.reset();
    this->notices.head.next.reset();
    this->ShowLoading(kLoadingCleared);
    game.roleCount = 1;
    this->field.palette.clear();

    if (!this->field.LoadMap(unit, this->systemFlags))
    {
        ErrorBox(this->errors, kLoadMapTitle, u8"過程發生錯誤");  // 0x004B0F44
        this->Destroy();
        return;
    }

    MapHeader& header = game.map;

    if (header.bitsPerPixel == MapHeader::IndexedBits && header.pixels != nullptr)
    {
        this->field.palette.assign(MapHeader::PaletteBytes / sizeof(u16), 0);
        std::memcpy(this->field.palette.data(), header.pixels, MapHeader::PaletteBytes);
        this->painter.ConvertPixels(this->field.palette);
    }

    if (header.pixels == nullptr)
    {
        ErrorLog(u8"LoadCity ,gMap.Pixel==NULL");
    }

    view.layerTick = 1;
    view.layerIndex = 0;
    view.layerOffset = 0;
    view.layerTiles = header.width * header.height;
    std::string name;

    if (!this->MapName(this->currentMap, name))
    {
        ErrorBox(this->errors, u8"讀取地圖名稱", u8"MapName.txt中找不到地圖[%d]名稱", unit);  // 0x004B0EF8, 0x004B0F08
        name = Big5(Game::UnknownPlace);
    }

    header.name = {};
    std::copy_n(name.begin(), std::min(name.size(), header.name.size() - 1), header.name.begin());
    this->ShowLoading(kLoadingNamed);
    this->ShowLoading(kLoadingWeather);

    if (game.GameFlag(GameState::Weather))
    {
        for (std::size_t particle = 0; particle < view.weather.size(); ++particle)
        {
            view.weatherMotion[particle].speedTarget = static_cast<i32>(this->random.Below(kStartSpeeds)) + 1;
            const u32 spread = this->random.Below(view.weatherActions);
            view.weatherShades[particle].tintTarget = 0;
            view.weatherMotion[particle].action = view.weatherAction + static_cast<i32>(spread);
            view.weather[particle].x =
                static_cast<i32>(this->random.Below(static_cast<u32>(MapHeader::TileSize * header.width)));
            view.weather[particle].y =
                static_cast<i32>(this->random.Below(static_cast<u32>(MapHeader::TileSize * header.height)));
            view.weather[particle].stepX = static_cast<i32>(this->random.Below(kStartDrift)) - kStartDrift;
            view.weather[particle].stepY = static_cast<i32>(this->random.Below(kStartDrift)) - kStartDrift;
        }
    }

    view.mapTop = 0;
    view.mapLeft = 0;
    view.mapRight = MapHeader::TileSize * header.width;
    view.mapBottom = MapHeader::TileSize * header.height;
    this->BuildTriggers(view.triggers);
    const std::size_t table = view.rolesTable;
    this->ShowLoading(kLoadingRoles);
    view.followerCount = 1;

    auto word = [&data, table](std::size_t index)
    {
        return MapsWord(data, table + sizeof(u16) * index);
    };

    auto put = [&data, table](std::size_t index, u16 value)
    {
        const std::size_t at = table + sizeof(u16) * index;

        if (at + 1 < data.size())
        {
            data[at] = static_cast<u8>(value);
            data[at + 1] = static_cast<u8>(value >> CHAR_BIT);
        }
    };

    std::size_t record = 0;
    int placed = 0;
    std::size_t mark = 0;

    while (word(record) != MapsRecord::TableEnd)
    {
        for (std::size_t earlier = 0; earlier < record / MapsRecord::RoleWords; ++earlier)
        {
            if (word(record + MapsRecord::RoleId) == word(earlier * MapsRecord::RoleWords + MapsRecord::RoleId))
            {
                ErrorBox(
                    this->errors,
                    kRolesTitle,
                    u8"重複定義角色(%d)",
                    word(earlier * MapsRecord::RoleWords + MapsRecord::RoleId)
                );  // 0x004B0EDC
                record += MapsRecord::RoleWords;
            }
        }

        const u16 id = word(record + MapsRecord::RoleId);

        if (id == 0 || id == kEveryMapRole || id == kEveryMapRole2)
        {
            put(record + MapsRecord::RoleMap, static_cast<u16>(map));
        }

        if (static_cast<i32>(word(record + MapsRecord::RoleId)) == this->partyLeaderId)
        {
            put(record + MapsRecord::RoleMap, static_cast<u16>(map));
            put(record + MapsRecord::RoleAct, static_cast<u16>(act));
            put(record + MapsRecord::RoleAction, static_cast<u16>(action));
            put(record + MapsRecord::RoleDirection, static_cast<u16>(direction));
            put(record + MapsRecord::RoleColumn, static_cast<u16>(x));
            put(record + MapsRecord::RoleRow, static_cast<u16>(y));
            put(record + MapsRecord::RoleEvent, 0);
            put(record + MapsRecord::RolePath, 0);
            put(record + MapsRecord::RoleStep, 0);
            put(record + MapsRecord::RoleFlags, kLeaderFlags);
            game.playerRole = game.roleCount + placed;
            view.followers[0] = game.roleCount + placed;
            view.trailY.fill(MapHeader::TileSize * y);
            view.trailX.fill(MapHeader::TileSize * x);
        }

        if (static_cast<int>(word(record + MapsRecord::RoleMap)) != map)
        {
            record += MapsRecord::RoleWords;
            continue;
        }

        const u16 column = word(record + MapsRecord::RoleColumn);
        const u16 row = word(record + MapsRecord::RoleRow);
        const int slot = game.roleCount + placed;

        // The rebuild also leaves out a role past the last slot (deviations.md).
        if (column > static_cast<u32>(header.width) || row > static_cast<u32>(header.height) ||
            slot >= GameState::Roles)
        {
            ErrorBox(
                this->errors,
                kRolesTitle,
                u8"地圖(%d)啟動對話(%d)\n路人(%d)位置超出地圖\n地圖(%d,%d)\n路人(%d,%d)",
                map,
                game.leader.event,
                word(record + MapsRecord::RoleId),
                header.width,
                header.height,
                column,
                row
            );  // 0x004B0E68
            put(record + MapsRecord::RoleColumn, static_cast<u16>(header.width - 1));
            put(record + MapsRecord::RoleRow, static_cast<u16>(header.height - 1));
            record += MapsRecord::RoleWords;
            continue;
        }

        Role& role = game.roles[static_cast<std::size_t>(slot)];
        role.id = word(record + MapsRecord::RoleId);
        role.act.id = word(record + MapsRecord::RoleAct);
        role.act.action = word(record + MapsRecord::RoleAction);
        role.act.direction = word(record + MapsRecord::RoleDirection);
        role.act.wait = 0;
        role.act.drawLevel = 0;
        role.act.signal = 0;
        role.x = MapHeader::TileSize * column;
        role.y = MapHeader::TileSize * row;
        role.event = word(record + MapsRecord::RoleEvent);
        role.pathNumber = word(record + MapsRecord::RolePath);
        role.pathStep = static_cast<i16>(word(record + MapsRecord::RoleStep));
        role.flags = word(record + MapsRecord::RoleFlags);
        role.talkState = Role::NoTalk;
        role.eventOffset = 0;
        role.eventCursor = 0;
        this->LoadRoleSound(role);
        this->field.InsertRole(role, static_cast<int>(role.flags & Role::LayerMask));

        if (role.id == kPartyLeader && game.GameFlag(GameState::GhostAppears) &&
            this->party.variables[Party::GhostVariable] != 0)
        {
            this->ghostFollower.Spawn(
                this->party.variables[Party::GhostVariable] + kGhostFirstAct,
                kGhostAction,
                static_cast<float>(static_cast<u32>(role.x)),
                static_cast<float>(static_cast<u32>(role.y)),
                kGhostUnk14,
                kGhostUnk18
            );
        }

        if ((role.flags & Role::Follower) != 0)
        {
            this->FollowLeader(slot);
            role.flags |= Role::Visible;

            if (this->currentMap == Field::DemonRefiningPot || (view.mapStory & kStoryActMark) != 0)
            {
                role.flags &= ~Role::Visible;
            }
        }

        if ((role.flags & Role::Bubbles) != 0)
        {
            if (mark >= view.marks.size())
            {
                ErrorBox(this->errors, kRolesTitle, u8"地圖(%d)定義煙囪物件數量超過四個", map);  // 0x004B0EAC
            }
            else
            {
                RoleMark& entry = view.marks[mark++];
                entry.role = role.id;
                entry.x = static_cast<u16>(role.x);
                entry.y = static_cast<u16>(role.y);
                entry.unk08 = 0;
                entry.flags = 0;
                entry.unk0E = 0;
            }
        }

        record += MapsRecord::RoleWords;
        ++placed;
    }

    game.roleCount += placed;
    this->ShowLoading(kLoadingDone);

    for (int index = 1; index < game.roleCount; ++index)
    {
        this->InitRole(game.roles[static_cast<std::size_t>(index)]);
    }

    this->field.CenterCamera();
    DebugLog(
        u8"\n"
        u8"MapName[%s] id[%d]",
        std::string_view(header.name.data(), header.name.size()),
        this->currentMap
    );
    DebugLog(
        u8"\n"
        u8"TotalRole[%d]",
        game.roleCount
    );
    game.SetFrameInterval(Game::FrameInterval);
}

// swd3e.exe 0x0040D720: a role written into its record of maps.dat's role table (its act, place in tiles, event, talk,
// step and flags). The original searches past the end of the table unless story flag 2 is set.
void Game::StoreRole(const Role& role)
{
    std::vector<u8>& data = this->databases.mapsData;

    for (std::size_t at = this->scene.rolesTable;
         at + static_cast<std::size_t>(sizeof(u16) * MapsRecord::RoleWords) <= data.size();
         at += static_cast<std::size_t>(sizeof(u16) * MapsRecord::RoleWords))
    {
        if (MapsWord(data, at) == MapsRecord::TableEnd)
        {
            if (this->state.GameFlag(GameState::DebugReports))
            {
                ErrorBox(
                    this->errors,
                    u8"!!! 警告 !!!",
                    u8"角色編號(%d)\n資料歸鍵時\n找不到符合的位置\n可能是定義錯誤或是角色不存在了",
                    role.id
                );  // 0x004B0FE0, 0x004B0FF0
            }

            return;
        }

        if (MapsWord(data, at + sizeof(u16) * MapsRecord::RoleId) != role.id)
        {
            continue;
        }

        auto put = [&data, at](int word, u32 value)
        {
            const std::size_t offset = at + sizeof(u16) * static_cast<std::size_t>(word);
            data[offset] = static_cast<u8>(value);
            data[offset + 1] = static_cast<u8>(value >> CHAR_BIT);
        };

        put(MapsRecord::RoleAct, static_cast<u32>(role.act.id));
        put(MapsRecord::RoleAction, static_cast<u32>(role.act.action));
        put(MapsRecord::RoleDirection, static_cast<u32>(role.act.direction));
        put(MapsRecord::RoleColumn, static_cast<u16>(role.x) >> MapHeader::TileShift);
        put(MapsRecord::RoleRow, static_cast<u16>(role.y) >> MapHeader::TileShift);
        put(MapsRecord::RoleEvent, role.event);
        put(MapsRecord::RolePath, role.pathNumber);
        put(MapsRecord::RoleStep, static_cast<u32>(role.pathStep));
        put(MapsRecord::RoleFlags, role.flags);
        return;
    }
}

// swd3e.exe 0x004270B0: sound `sound` from (x, y) of the map when within 512 pixels of the controlled role, quieter and
// panned by the distance
void Game::PlaySoundAt(u16 sound, int x, int y)
{
    const Role& player = this->state.roles[static_cast<std::size_t>(this->state.playerRole)];
    const int squared = (player.x - x) * (player.x - x) + (player.y - y) * (player.y - y);
    const auto distance = static_cast<int>(crt::Ftol(std::sqrt(static_cast<double>(squared))));

    if (distance >= kSoundRange)
    {
        return;
    }

    SoundEffects& effects = this->audio.Effects();
    effects.Play(sound, 0, 0, kPlayOnce);
    effects.SetVolume(
        sound,
        this->config.soundVolume * (kSoundScale - distance * kSoundScale / kSoundRange) / Config::MaxVolume
    );
    effects.SetPan(sound, ((x - player.x) * kPanScale) / kSoundRange);
}

// swd3e.exe 0x0040DDD0: a random encounter after `steps` steps: the map's encounter set (from maps.dat's table 0x12)
// gives up to four step thresholds with their chances in percent; once one is passed and its roll succeeds (or with
// `force` 1) the encounter area under the controlled role picks one of its battles at random. 0 for none.
u32 Game::RollEncounter(int steps, int force)
{
    Scene& view = this->scene;

    if (view.encounterGroup == 0)
    {
        return 0;
    }

    const Role& player = this->state.roles[static_cast<std::size_t>(this->state.playerRole)];
    const auto roll = static_cast<int>(this->random.Below(kPercent));
    const std::vector<u16>& sets = view.encounterGroups;
    std::size_t at = MapsRecord::GroupsAt + MapsRecord::GroupWords * static_cast<std::size_t>(view.encounterGroup - 1);
    int threshold = 0;
    int pair = kSetPairs;

    auto word = [&sets](std::size_t index) -> i16
    {
        return index < sets.size() ? static_cast<i16>(sets[index]) : i16{0};
    };

    for (;;)
    {
        threshold += word(at);

        if (steps < threshold || force == kForcedEncounter)
        {
            break;
        }

        at += kPairWords;

        if (--pair <= 0)
        {
            return 0;
        }
    }

    if (word(at + 1) <= roll && force != kForcedEncounter)
    {
        return 0;
    }

    // logical shifts (shr) of the place, compared signed with the areas
    const auto column = static_cast<i32>(static_cast<u32>(player.x) >> MapHeader::TileShift);
    const auto row = static_cast<i32>(static_cast<u32>(player.y) >> MapHeader::TileShift);
    const MapTrigger* area = view.triggers.next.get();

    while (area != nullptr && (column < area->left || column > area->right || row < area->top || row > area->bottom))
    {
        area = area->next.get();
    }

    if (area == nullptr)
    {
        return 0;
    }

    const std::vector<u8>& data = this->databases.mapsData;
    const u16 count = MapsWord(data, area->table);
    const u32 pick = this->random.Below(count);
    view.encounterSteps = 0;
    return MapsWord(data, area->table + sizeof(u16) * (pick + 1));
}

// swd3e.exe 0x0040FA20: role `index` let go by an event: while held, each scripted move of it ends: a role between
// tiles leaves its cells and (unless controlled) steps back along the move until it stands on a tile, its stepping
// and fast bits and its act's speed cleared; its path move resumes, its act returns to the action and direction it had
// before turning to the talker, and it is no longer held. The original steps forever where the step cannot reach a
// tile (or reads past the step tables at the end of the path); here that is Hang().
void Game::ReleaseRole(int index)
{
    Role& role = this->state.roles[static_cast<std::size_t>(index)];

    if ((role.flags & Role::Held) == 0)
    {
        return;
    }

    for (MoveRecord& move : this->state.moves)
    {
        if (static_cast<int>(move.role) != index)
        {
            continue;
        }

        if ((role.x & kTileMask) != 0 || (role.y & kTileMask) != 0)
        {
            this->field.ClearRoleCells(role, this->partyLeaderId);

            if (index != this->state.playerRole)
            {
                const u8 direction = move.CurrentDirection();

                if (direction >= RolePath::StepX.size())
                {
                    Hang();
                }

                role.x = RolePath::TileStart(role.x, RolePath::StepX[direction]);
                role.y = RolePath::TileStart(role.y, RolePath::StepY[direction]);
            }
        }

        role.flags &= ~(Role::Stepping | Role::FastMove);
        role.act.speed = 0;
    }

    this->rolePath.ResumeMove(index);

    if (index >= GameState::Roles)
    {
        // Two values for three %d: manMax takes what is on the stack (0 here, see deviations.md).
        ErrorBox(
            this->errors,
            u8"對話系統(TalkEnd)",
            u8"GUID(%d)\n陣列索引錯誤(crID %d) (manMax %d) ",
            index,
            this->state.roleCount,
            0
        );  // 0x004B11E0, 0x004B11F4
    }

    if ((role.flags & Role::FacesTalker) != 0)
    {
        if (role.act.savedAction != ActState::NotSaved)
        {
            role.act.action = role.act.savedAction;
        }

        if (role.act.savedDirection != ActState::NotSaved)
        {
            role.act.direction = role.act.savedDirection;
        }
    }

    role.act.savedAction = ActState::NotSaved;
    role.act.savedDirection = ActState::NotSaved;

    if (this->acts.Run(role.act) == 0)
    {
        ReportActError(this->errors, u8"Act Err(Talk:Talkend)", role);
    }

    role.act.savedAction = ActState::NotSaved;
    role.act.savedDirection = ActState::NotSaved;
    role.flags &= ~Role::Held;
    role.act.delay = 0;
}
}  // namespace swd3e
