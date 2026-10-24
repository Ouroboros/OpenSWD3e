#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>

#include "core/debug_log.h"
#include "core/error_box.h"
#include "core/format.h"
#include "game/game.h"
#include "item/item_list.h"
#include "talk/script_text.h"
#include "talk/talk.h"
#include "talk/talk_event.h"
#include "text/text_input.h"

namespace swd3e
{

namespace
{

constexpr int kMusicRepeatMask = 3;  // how a list of tunes repeats (MusicQueue::Switch), in the marks
constexpr u16 kMusicGivesBack = 4;   // ChangeMusic: the map takes the music back when its tunes come again
// WaitVideoFrame clears the bottom rows of the drawing surface: 0x86600 bytes in, 0xFA00 bytes (rows 430..479).
constexpr std::size_t kVideoClearFrom = 0x86600 / sizeof(Pixel);
constexpr std::size_t kVideoClearPixels = 0xFA00 / sizeof(Pixel);
constexpr u16 kFadeNoLength = 1;              // MusicFade: the fade mode whose length is taken as 0
constexpr u16 kCommandStarted = 0x8000;       // GatherParty marks its own command word while the party gathers
constexpr std::size_t kFileNameBytes = 0x33;  // the longest name SetFileNameA/B keeps
// The range the spotlight command keeps the spotlight in (screen pixels).
constexpr int kSpotlightLeft = 0xD0;
constexpr int kSpotlightRight = 0x1B0;
constexpr int kSpotlightTop = 0xD0;
constexpr int kSpotlightBottom = 0x110;

// The role flag commands 0x66, 0x67, 0x75, 0x88, 0x8C, 0x91, 0x92 and 0xAE set or clear.
u32 RoleFlagOf(EventCommand code)
{
    switch (code)
    {
        case EventCommand::RoleExamine:
            return Role::Examine;

        case EventCommand::RoleNoTalkPointer:
            return Role::NoTalkPointer;

        case EventCommand::RoleUnk10:
            return Role::Unk10;

        case EventCommand::RoleStepsAct:
            return Role::StepsAct;

        case EventCommand::RoleFacesTalker:
            return Role::FacesTalker;

        case EventCommand::RoleTouchEvent:
            return Role::TouchEvent;

        case EventCommand::RoleCellEffects:
            return Role::CellEffects;

        default:
            return Role::Solid;
    }
}

}  // namespace

// Command 0xD1 (inline in 0x0042BAF0, swd3e): the name boxes of role `operand` (0..3; a larger one is reported and
// its boxes start empty, where the original reads past the names): the boxes start with the role's surname, given
// name and nickname, the keyboard goes to them (flag 0x50) and the operand is marked as started; the script waits
// until Game::NameBoxFrame has closed them, then clears the mark and goes on.
bool Talk::OpenNameBoxes(EventRun& run)
{
    constexpr u16 kStarted = 0x8000;
    Game& owner = this->game;
    const u16 operand = this->Word(run, kFirstOperand);

    if ((operand & kStarted) != 0)
    {
        if ((owner.systemFlags & Game::NameBoxesOpen) == 0)
        {
            this->SetWord(run, kFirstOperand, static_cast<u16>(operand & ~kStarted));
            owner.nameRole = 0;
            this->Next(run);
        }

        return false;
    }

    if (operand >= Party::Roles)
    {
        ErrorBox(owner.errors, u8"對話系統(NAME_BOX)", u8"號碼超過範圍");  // 0x004B2424, 0x004B2438
    }

    run.valueA = operand;
    owner.nameRole = operand;
    owner.systemFlags |= Game::NameBoxesOpen;
    owner.state.SetGameFlag(GameState::TextBox);
    this->SetWord(run, kFirstOperand, static_cast<u16>(operand | kStarted));
    std::array<std::string, Game::NameBoxCount> parts;

    if (operand < Party::Roles)
    {
        const RoleName& name = owner.party.names[operand];
        parts = {name.Surname(), name.Given(), name.Nickname()};
    }

    for (std::size_t box = 0; box < Game::NameBoxCount; ++box)
    {
        const Game::NameBoxPlace& place = Game::NameBoxPlaces[box];
        owner.nameBoxes[box] = TextInput::FromBig5(parts[box], place.capacity, place.x, place.y);
    }

    owner.currentNameBox = owner.nameBoxes[Game::SurnameBox].get();
    return false;
}

// swd3e.exe 0x00433620: where role `id`'s record lies in maps.dat's role table (records of 0x18 bytes from
// Scene::rolesTable up to one starting with a dword of -1), NoRecord when it has none
std::size_t Talk::FindRoleRecord(u32 id) const
{
    constexpr u32 kTableEnd = 0xFFFFFFFFu;
    constexpr std::size_t kRecordBytes = sizeof(u16) * MapsRecord::RoleWords;
    const std::vector<u8>& data = this->game.databases.mapsData;
    std::size_t at = this->game.scene.rolesTable;

    if (MapsDword(data, at) == kTableEnd)
    {
        return Talk::NoRecord;
    }

    while (MapsWord(data, at + sizeof(u16) * MapsRecord::RoleId) != id)
    {
        at += kRecordBytes;

        // The rebuild: the data ending without the end mark ends the search.
        if (at >= data.size() || MapsDword(data, at) == kTableEnd)
        {
            return Talk::NoRecord;
        }
    }

    return at;
}

// swd3e.exe 0x00433590: variable `variable` (1: the low byte, 2: the high byte of the record's RoleVariables word) of
// role `id` read into `value` or set from its low byte by `mode`; 0 when the role has no record, else `id`
u32 Talk::RoleVariable(u32 id, int variable, i32& value, int mode)
{
    constexpr int kLowByte = 1;
    constexpr int kHighByte = 2;
    const std::size_t record = this->FindRoleRecord(id);

    if (record == Talk::NoRecord)
    {
        return 0;
    }

    std::vector<u8>& data = this->game.databases.mapsData;
    const std::size_t at = record + sizeof(u16) * MapsRecord::RoleVariables;

    // The rebuild: a record cut by the end of the data reads as 0 and is not written.
    if (at + 1 >= data.size())
    {
        if (mode == Talk::ReadVariable && (variable == kLowByte || variable == kHighByte))
        {
            value = 0;
        }

        return id;
    }

    if (mode == Talk::ReadVariable)
    {
        if (variable == kLowByte)
        {
            value = data[at];
        }
        else if (variable == kHighByte)
        {
            value = data[at + 1];
        }
    }
    else if (mode == Talk::WriteVariable)
    {
        if (variable == kLowByte)
        {
            data[at] = static_cast<u8>(value);
        }
        else if (variable == kHighByte)
        {
            data[at + 1] = static_cast<u8>(value);
        }
    }

    return id;
}

// Commands 0x64..0x8F: the roles' events and flags, the talk window settings, waits for windows and animations, the
// music, the talk log, the items and skills, the party's state and the timers.
bool Talk::RunStateCommand(EventRun& run)
{
    Game& owner = this->game;
    GameState& state = owner.state;
    Scene& view = owner.scene;
    Party& party = owner.party;
    const i32 leader = owner.partyLeaderId;

    switch (run.code)
    {
        case EventCommand::SetEvent:
        {
            Operands operands = this->OperandsOf(run);
            const u16 id = this->EventRoleId(run, operands.Word());
            const u16 event = operands.Word();

            if (state.FindRoleOrPlayer(id, run.found))
            {
                state.roles[run.found].event = event;
            }
            else
            {
                owner.UpdateRoleRecord(
                    id,
                    Game::KeepValue,
                    Game::KeepValue,
                    Game::KeepValue,
                    Game::KeepValue,
                    Game::KeepValue,
                    static_cast<i16>(event),
                    Game::KeepValue,
                    0,
                    Game::KeepValue,
                    Game::KeepValue
                );
            }

            this->Next(run);
            return true;
        }

        case EventCommand::SetRoleFastMove:
        {
            Operands operands = this->OperandsOf(run);
            const u16 id = this->EventRoleId(run, operands.Word());

            if (state.FindRoleOrPlayer(id, run.found))
            {
                state.roles[run.found].flags |= Role::FastMove;
            }

            this->Next(run);
            return true;
        }

        case EventCommand::RoleExamine:
        case EventCommand::RoleNoTalkPointer:
        case EventCommand::RoleUnk10:
        case EventCommand::RoleStepsAct:
        case EventCommand::RoleFacesTalker:
        case EventCommand::RoleTouchEvent:
        case EventCommand::RoleCellEffects:
        case EventCommand::RoleSolid:
        {
            const u32 bit = RoleFlagOf(run.code);
            Operands operands = this->OperandsOf(run);
            const u16 operand = operands.Word();
            const u16 id = operand == Talk::Current ? static_cast<u16>(state.playerRole) : operand;
            const bool set = operands.Word() != 0;

            if (state.FindRoleOrPlayer(id, run.found))
            {
                Role& role = state.roles[run.found];
                role.flags &= ~bit;

                if (set)
                {
                    role.flags |= bit;
                }

                owner.field.ClearRoleCells(role, leader);
                owner.field.MarkRoleCells(role, leader);
            }
            else
            {
                DebugLog(
                    u8"(Sar_**)角色()不在地圖上"
                );  // wsprintfA of 0x004B2538 (no conversion in it), then nullsub_1
                run.valueA = set ? static_cast<i32>(bit) : 0;
                run.valueB = set ? -1 : static_cast<i32>(~bit);
                owner.UpdateRoleRecord(
                    id,
                    Game::KeepValue,
                    Game::KeepValue,
                    Game::KeepValue,
                    Game::KeepValue,
                    Game::KeepValue,
                    Game::KeepValue,
                    Game::KeepValue,
                    static_cast<i16>(set ? bit : 0),
                    static_cast<i16>(set ? ~0u : ~bit),
                    Game::KeepValue
                );
            }

            this->Next(run);
            return true;
        }

        case EventCommand::DialogOffset:
        {
            Operands operands = this->OperandsOf(run);
            const auto x = static_cast<i16>(operands.Word());
            const auto y = static_cast<i16>(operands.Word());
            this->dialogOptions &= ~Talk::Offset;
            this->dialogOffsetX = x;
            this->dialogOffsetY = y;
            this->Next(run);
            return true;
        }

        case EventCommand::DialogSingleLine:
            this->dialogOptions &= ~Talk::SingleLine;
            this->Next(run);
            return true;

        case EventCommand::WaitAnimationA:
        case EventCommand::WaitAnimationB:
        {
            Operands operands = this->OperandsOf(run);
            const u16 rounds = operands.Word();
            const ScriptAnimation* first =
                run.code == EventCommand::WaitAnimationB ? view.animationsB.get() : view.animationsA.get();

            if (first == nullptr || (first->act.counter >> ActState::RoundShift) > rounds)
            {
                this->Next(run);
                return true;
            }

            return false;
        }

        case EventCommand::WaitActRounds:
        {
            Operands operands = this->OperandsOf(run);
            const u16 id = this->EventRoleId(run, operands.Word());
            const u16 wanted = operands.Word();

            if (!state.FindRoleOrPlayer(id, run.found))
            {
                ErrorBox(
                    this->game.errors,
                    u8"Talk(tilpa1)",
                    u8"不存在地圖上的角色(GUID:%d)",
                    wanted
                );  // 0x004B2768, 0x004B274C
                this->Next(run);
                return true;
            }

            const u16 rounds = state.roles[run.found].act.counter;

            if ((rounds & ActState::RoundsMask) < wanted)
            {
                ErrorBox(
                    this->game.errors,
                    u8"Talk(tilpa1)",
                    u8"指定的張數(%d) 超過act容量(%d)",
                    wanted,
                    rounds & ActState::RoundsMask
                );  // 0x004B2768, 0x004B2778
                this->Next(run);
                return true;
            }

            if ((rounds >> ActState::RoundShift) >= wanted)
            {
                this->Next(run);
                return true;
            }

            return false;
        }

        case EventCommand::DialogOrigin:
        {
            Operands operands = this->OperandsOf(run);
            u32 x = operands.Word();
            u32 y = operands.Word();

            if (x > Display::Width - 1)
            {
                x = Talk::OriginEdge;
            }

            if (y > Display::Height - 1)
            {
                y = Talk::OriginEdge;
            }

            this->dialogOrigin = x | y << Talk::OriginYShift;
            this->Next(run);
            return true;
        }

        case EventCommand::StepRoles:
        {
            // swd3e handles it after its switch of commands (a role not on the map is passed over).
            constexpr std::size_t kRolesAt = kFirstOperand + 1;  // the roles follow the count
            const u16 count = this->Word(run, kFirstOperand);

            for (u16 index = 0; index < count; ++index)
            {
                if (state.FindRoleOrPlayer(this->Word(run, kRolesAt + index), run.found))
                {
                    owner.rolePath.ScriptStep(static_cast<int>(run.found));
                }
            }

            this->Advance(run, static_cast<int>(kWordBytes * (kRolesAt + count)));
            return false;
        }

        case EventCommand::JumpIfNoneMoving:
        case EventCommand::JumpIfAnyMoving:
        {
            bool moving = false;

            for (int index = 1; index < state.roleCount && index < GameState::Roles; ++index)
            {
                if ((state.roles[static_cast<std::size_t>(index)].flags & Role::Stepping) != 0)
                {
                    moving = true;
                    break;
                }
            }

            if (run.code == EventCommand::JumpIfAnyMoving)
            {
                moving = !moving;
            }

            if (moving)
            {
                this->Next(run);
            }
            else
            {
                Operands operands = this->OperandsOf(run);
                this->Jump(run, operands.Dword());
            }

            return true;
        }

        case EventCommand::WaitEffects:
            if (view.effects != nullptr)
            {
                return false;
            }

            if (view.spritesB == nullptr)
            {
                this->Next(run);
            }

            return false;

        case EventCommand::SoundLong:
            owner.audio.PlaySound(this->OperandsOf(run).Word(), owner.config.soundVolume);
            this->Next(run);
            return false;

        case EventCommand::ChangeMusic:
        {
            // swd3e: the story's two tunes become the second list of the music queue, which follows the playing piece
            // after the fade the last MusicFade gave (then cleared); bit 2 of the marks lets the map take the music
            // back (Game::MusicZone).
            Operands operands = this->OperandsOf(run);
            auto& tunes = view.musicTunes;
            tunes[Scene::StoryFirst] = operands.Word();
            tunes[Scene::StorySecond] = operands.Word();
            const u16 marks = operands.Word();
            MusicQueue& queue = owner.audio.Queue();

            for (std::size_t piece = 0; piece < MusicQueue::Pieces; ++piece)
            {
                const i32 tune = tunes[Scene::StoryFirst + piece];
                queue.SetSlot(owner.MusicPath(tune), tune, piece, MusicQueue::SecondList);
            }

            run.valueA = marks & kMusicRepeatMask;

            if (run.valueA == 0)
            {
                run.valueA = MusicQueue::RepeatList;
            }

            queue.Switch(MusicQueue::SecondList, run.valueA, owner.audio.FadeLength());
            owner.audio.SetFadeMode(0);
            owner.audio.SetFadeLength(0);
            tunes[Scene::StoryFade] = (marks & kMusicGivesBack) != 0 ? 1 : 0;
            this->Next(run);
            DebugLog(u8"S_M[%d,%d]", tunes[Scene::StoryFirst], tunes[Scene::StorySecond]);
            return true;
        }

        case EventCommand::MusicVolume:
            // swd3e passes over the command (swd32 set the music volume).
            this->Next(run);
            return false;

        case EventCommand::PutRoles:
        {
            constexpr std::size_t kRolesAt = kFirstOperand + 1;  // the entries follow the count
            constexpr std::size_t kEntryWords = 3;               // a role, a column and a row
            constexpr std::size_t kColumnWord = 1;
            constexpr std::size_t kRowWord = 2;
            const u16 count = this->Word(run, kFirstOperand);
            run.valueA = 0;

            for (u16 index = 0; index < count; ++index)
            {
                const std::size_t entry = kRolesAt + kEntryWords * index;
                const u16 id = this->EventRoleId(run, this->Word(run, entry));
                const u16 column = this->Word(run, entry + kColumnWord);
                const u16 row = this->Word(run, entry + kRowWord);
                state.FindRoleOrPlayer(id, run.found);

                if (run.found < GameState::Roles)
                {
                    owner.rolePath.MoveRoleTo(
                        static_cast<int>(run.found),
                        static_cast<u16>(MapHeader::TileSize * column),
                        static_cast<u16>(MapHeader::TileSize * row),
                        RolePath::WalkThere,
                        RolePath::Keep,
                        RolePath::Keep,
                        RolePath::Keep
                    );
                }

                if (static_cast<i32>(run.found) == state.playerRole)
                {
                    view.playerHold |= Scene::HoldLocked;
                }
            }

            this->Advance(run, static_cast<int>(kWordBytes * (kRolesAt + kEntryWords * count)));
            return true;
        }

        case EventCommand::CloseDialog:
        {
            Operands operands = this->OperandsOf(run);
            const u16 id = this->EventRoleId(run, operands.Word());
            std::unique_ptr<TalkWindow>* link = &view.talkWindows;

            while (*link != nullptr)
            {
                TalkWindow& window = **link;

                if (owner.rolePath.RoleId(window.role) != id)
                {
                    link = &window.next;
                    continue;
                }

                if (window.role != TalkWindow::NoRole && window.role < GameState::Roles)
                {
                    state.roles[window.role].talkState = Role::NoTalk;
                }

                std::unique_ptr<TalkWindow> gone = std::move(*link);
                *link = std::move(gone->next);
                run.valueA = view.playerHold & Scene::HoldLocked;
                run.valueB = std::max((view.playerHold & Scene::HoldCountMask) - 1, 0);
                view.playerHold = run.valueA | run.valueB;
            }

            this->Next(run);
            return true;
        }

        case EventCommand::WaitDialogRead:
        case EventCommand::WaitDialogTyped:
        {
            Operands operands = this->OperandsOf(run);
            const u16 id = this->EventRoleId(run, operands.Word());
            run.found = TalkWindow::NoRole;

            if (id != TalkWindow::NoRole && !state.FindRoleOrPlayer(id, run.found))
            {
                ErrorBox(
                    this->game.errors,
                    u8"Talk(wait_dlg2)",
                    u8"角色號碼(GUID:%d)不存在此地圖(%d)",
                    id,
                    owner.currentMap
                );  // 0x004B26BC, 0x004B2B88
                this->Next(run);
                return true;
            }

            const TalkWindow* window = view.talkWindows.get();

            while (window != nullptr && static_cast<u32>(window->role) != run.found)
            {
                window = window->next.get();
            }

            if (window == nullptr)
            {
                this->Next(run);
                return true;
            }

            const u32 done = run.code == EventCommand::WaitDialogRead ? window->flags & TalkWindow::Read
                                                                      : window->flags & TalkWindow::Typed;

            if (done != 0)
            {
                this->Next(run);
                return true;
            }

            return false;
        }

        case EventCommand::SetActData:
        {
            Operands operands = this->OperandsOf(run);
            const u16 id = this->EventRoleId(run, operands.Word());
            const u16 act = operands.Word();
            const u16 action = operands.Word();
            const u16 direction = operands.Word();

            if (state.FindRoleOrPlayer(id, run.found))
            {
                Role& role = state.roles[run.found];

                if (act != Talk::Keep)
                {
                    role.act.id = static_cast<i16>(act);
                }

                if (action != Talk::Keep)
                {
                    role.act.action = static_cast<i16>(action);
                }

                if (direction != Talk::Keep)
                {
                    role.act.direction = direction;
                }

                role.act.delay = 0;

                if (owner.acts.Run(role.act) == 0)
                {
                    ReportActError(owner.errors, u8"TakeActData(Talk,TakeActData)", role);
                }

                role.flags |= Role::StepsAct;
            }
            else
            {
                owner.UpdateRoleRecord(
                    id,
                    static_cast<i16>(act),
                    static_cast<i16>(action),
                    static_cast<i16>(direction),
                    Game::KeepValue,
                    Game::KeepValue,
                    Game::KeepValue,
                    Game::KeepValue,
                    Role::StepsAct,
                    Game::KeepValue,
                    Game::KeepValue
                );
            }

            this->Next(run);
            return true;
        }

        case EventCommand::DialogBubble:
            this->dialogOptions &= ~Talk::ShowMark;
            this->Next(run);
            return true;

        case EventCommand::StopSkipping:
            this->skipAll = 0;
            this->Next(run);
            return true;

        case EventCommand::SceneMusic:
        {
            constexpr std::size_t kRecordWords = 3;  // the map and its two tunes; the marks stay
            Operands operands = this->OperandsOf(run);
            const u16 operand = operands.Word();
            const u16 firstTune = operands.Word();
            const u16 secondTune = operands.Word();
            const u16 marks = operands.Word();
            const std::array<u16, kRecordWords> record = {operand, firstTune, secondTune};
            const u16 map = operand == Talk::Current ? static_cast<u16>(owner.currentMap) : operand;
            std::vector<u8>& data = owner.databases.mapsData;
            std::size_t at = MapsDword(
                data,
                MapsDword(data, MapsTable(data, MapsDataTable::Music) + sizeof(u32) * MapsRecord::SceneMusicItem)
            );
            run.valueA = 0;

            while (MapsWord(data, at) != 0 && at + 1 < data.size() && MapsWord(data, at) != map)
            {
                at += MapsRecord::MusicBytes;
            }

            if (MapsWord(data, at) != 0 && at + MapsRecord::MusicMarksAt - 1 < data.size())
            {
                for (std::size_t index = 0; index < kRecordWords; ++index)
                {
                    const u16 value = record[index];
                    data[at + sizeof(u16) * index] = static_cast<u8>(value);
                    data[at + sizeof(u16) * index + 1] = static_cast<u8>(value >> CHAR_BIT);
                }
            }

            // swd3e: on the map shown the scene's tunes change at once, from the operands one word too early (the
            // map, the first and the second tune go to the first tune, the second and the fade mode; a bug kept).
            if (map == static_cast<u16>(owner.currentMap))
            {
                view.musicTunes[Scene::SceneFirst] = operand;
                view.musicTunes[Scene::SceneSecond] = firstTune;
                view.musicTunes[Scene::SceneFade] = secondTune;
            }

            if (MapsWord(data, at) == 0)
            {
                ErrorBox(
                    this->game.errors,
                    u8"TalkOp:Scene_Music",
                    u8"找不到地圖音樂定義\n[%d,%d,%d,%d]",
                    operand,
                    firstTune,
                    secondTune,
                    marks
                );  // 0x004B2664, 0x004B2678
            }

            this->Next(run);
            return true;
        }

        case EventCommand::DialogLogged:
            this->dialogOptions &= ~Talk::Logged;
            this->Next(run);
            return true;

        case EventCommand::AddLog:
        {
            constexpr std::size_t kLogLineBytes = 0xFD;  // a log line before its "%Q"
            const std::vector<u8>& script = owner.databases.script;
            const std::size_t start = run.cursor + kOpcodeBytes;
            const std::size_t end = TextEndAt(script, start);
            std::string text = start < end ? std::string(script.begin() + start, script.begin() + end) : std::string();
            run.valueA = static_cast<i32>(text.size());

            if (text.size() > kLogLineBytes)
            {
                text.resize(kLogLineBytes);
            }

            this->history.push_back(text + "%Q");
            run.cursor = end + kWordBytes;
            this->SyncOffset(run);
            return false;
        }

        case EventCommand::JumpIfActionNot:
        case EventCommand::JumpIfAction:
        {
            Operands operands = this->OperandsOf(run);
            const u16 operand = operands.Word();
            const u16 id = operand == Talk::Current ? static_cast<u16>(state.playerRole) : operand;
            const u16 action = operands.Word();
            const u32 target = operands.Dword();
            Role loaded;
            const Role* role = &loaded;

            if (state.FindRoleOrPlayer(id, run.found))
            {
                role = &state.roles[run.found];
            }
            else
            {
                owner.LoadRoleRecord(loaded, id);
            }

            run.valueA = role->act.action;

            if ((run.code == EventCommand::JumpIfAction) != (run.valueA == action))
            {
                this->Jump(run, target);
            }
            else
            {
                this->Next(run);
            }

            return true;
        }

        case EventCommand::AddItem:
        {
            Operands operands = this->OperandsOf(run);
            const u16 id = operands.Word();
            const auto count = static_cast<i16>(operands.Word());
            const ItemNode* item = AddItem(party.inventory, id, count, ItemCountMode::Pending, owner.database);

            if (item != nullptr)
            {
                DebugLog(u8"%s,%d", ObjectName(item->record), count);  // the name at +0xC
            }
            else
            {
                DebugLog(u8"%d,%d", id, count);
            }

            this->Next(run);
            return false;
        }

        case EventCommand::JumpIfItem:
        case EventCommand::JumpIfNoItem:
        case EventCommand::JumpIfWorn:
        case EventCommand::JumpIfNotWorn:
        {
            Operands operands = this->OperandsOf(run);
            const auto id = static_cast<i16>(operands.Word());
            const ItemNode* carried = FindItemOfKind(party.inventory, id);
            const ItemNode* worn = FindRoleItem(party.equipment, id);
            const bool wanted = run.code == EventCommand::JumpIfNoItem || run.code == EventCommand::JumpIfNotWorn;

            if (run.code == EventCommand::JumpIfWorn || run.code == EventCommand::JumpIfNotWorn)
            {
                carried = nullptr;
            }

            // The original leaves the two item pointers behind (deviations.md).
            run.valueA = 0;
            run.valueB = 0;

            if (wanted == (carried != nullptr || worn != nullptr))
            {
                this->Next(run);
                return true;
            }

            this->Jump(run, operands.Dword());
            return true;
        }

        case EventCommand::LearnSkill:
        {
            Operands operands = this->OperandsOf(run);
            const u16 role = operands.Word();
            const u16 skill = operands.Word();
            run.found = role;

            if (role >= Party::Roles)
            {
                ErrorBox(
                    this->game.errors,
                    u8"對話系統(Mag_Learn)",
                    u8"角色序號超過四(%d)",
                    role
                );  // 0x004B262C, 0x004B2640
                this->Next(run);
                return true;
            }

            ItemLink& skills = party.skills[role]->next;

            if (FindItemOfKind(skills, static_cast<i16>(skill)) == nullptr)
            {
                ItemNode* learned = AddItem(skills, skill, 1, ItemCountMode::Count, owner.database);

                if (learned != nullptr)
                {
                    learned->record.data.kind &= ~ObjectData::UnkKind8000;

                    if ((learned->record.data.usableBy & (ObjectData::FirstRoleBit >> role)) == 0)
                    {
                        ErrorBox(
                            this->game.errors,
                            u8"對話系統(Mag_Learn)",
                            u8"角色(%d)\n不應該學到這種法術%s",
                            role,
                            ObjectName(learned->record)
                        );  // 0x004B262C, 0x004B260C
                        AddItem(skills, learned->id, -1, ItemCountMode::Pending, owner.database);
                    }
                }
            }

            this->Next(run);
            return true;
        }

        case EventCommand::Equip:
        {
            constexpr u16 kEquipSlots = 0xC;  // of the Party::SlotsPerRole a role has
            Operands operands = this->OperandsOf(run);
            const u16 role = operands.Word();
            const u16 slot = operands.Word();
            const auto kind = static_cast<i16>(operands.Word());
            run.found = role;

            if (role >= Party::Roles || slot >= kEquipSlots)
            {
                ErrorBox(
                    this->game.errors,
                    u8"對話系統(Mag_Learn)",
                    u8"角色序號或部位超過有效範圍\nrs(%d) bp(%d)",
                    role,
                    slot
                );  // 0x004B262C, 0x004B25BC
                this->Next(run);
                return true;
            }

            const ItemNode* item = FindItemOfKind(party.inventory, kind);

            if (item == nullptr)
            {
                ErrorBox(
                    this->game.errors,
                    u8"對話系統(Equip)",
                    u8"隊伍沒有物品(%d)",
                    static_cast<u16>(kind)
                );  // 0x004B25E8, 0x004B25F8
                this->Next(run);
                return true;
            }

            std::unique_ptr<ItemNode>& worn =
                party.equipment[static_cast<std::size_t>(Party::SlotsPerRole * role + slot)];
            const u16 oldId = worn != nullptr ? worn->id : u16{0};
            const u16 newId = item->id;

            if (worn == nullptr)
            {
                worn = std::make_unique<ItemNode>();
            }

            worn->id = item->id;
            worn->record = item->record;
            worn->count = 1;
            worn->pending = 0;
            worn->unk06 = 0;
            AddItem(party.inventory, oldId, 1, ItemCountMode::Pending, owner.database);
            AddItem(party.inventory, newId, -1, ItemCountMode::Pending, owner.database);
            this->Next(run);
            return true;
        }

        case EventCommand::ItemMenu:
        {
            constexpr std::size_t kItemsAt = 1;  // the items follow the command, ended by 0
            constexpr std::size_t kMaxItems = 0x7F;
            std::size_t count = 0;

            while (this->Word(run, kItemsAt + count) != 0 &&
                   run.cursor + kWordBytes * (kItemsAt + count) + 1 < owner.databases.script.size())
            {
                ++count;
            }

            owner.shopGoods.clear();

            for (std::size_t index = 0; index < count && index < kMaxItems; ++index)
            {
                owner.shopGoods.push_back(this->Word(run, kItemsAt + index));
            }

            owner.shopGoods.push_back(0);

            owner.menuState = Game::MenuEntering | Game::ItemChoiceMenu;
            this->Advance(run, static_cast<int>(kWordBytes * (kItemsAt + count + 1)));
            return false;
        }

        case EventCommand::ChangeVitals:
        {
            Operands operands = this->OperandsOf(run);
            const auto index = static_cast<u16>(operands.Word() - 1);  // the roles count from 1
            const u16 hitPoints = operands.Word();
            const u16 spirit = operands.Word();
            const u16 stamina = operands.Word();

            if (index >= Party::Roles)
            {
                ErrorBox(
                    this->game.errors,
                    u8"對話系統(Healing)",
                    u8"角色序號[%d]超過有效範圍 4.",
                    index
                );  // 0x004B258C, 0x004B25A0
                this->Next(run);
                return true;
            }

            RoleRecord& record = party.roles[index];
            record.hitPoints = static_cast<i16>(record.hitPoints + hitPoints);
            record.spirit = static_cast<i16>(record.spirit + spirit);
            record.stamina = static_cast<i16>(record.stamina + stamina);
            record.hitPoints = std::min(record.hitPoints, record.maxHitPoints);
            record.spirit = std::min(record.spirit, record.maxSpirit);
            record.stamina = std::min(record.stamina, record.maxStamina);
            record.condition = 0;

            // swd3e: a role brought to 0 is defeated; only when no role in the party has hit points left does the
            // command after this one become EndGame.
            if (record.hitPoints <= 0)
            {
                record.hitPoints = 0;
                record.condition |= RoleRecord::Defeated;
                bool standing = false;
                run.valueA = 0;  // the role the search stops at is left behind (Party::Roles when none stands)

                for (std::size_t role = 0; role < Party::Roles; ++role)
                {
                    run.valueA = static_cast<i32>(role);

                    if (state.GameFlag(GameState::InParty + static_cast<int>(role)) && party.roles[role].hitPoints > 0)
                    {
                        standing = true;
                        break;
                    }

                    run.valueA = static_cast<i32>(role + 1);
                }

                if (!standing)
                {
                    this->SetWord(run, CommandBytes(run.code) / kWordBytes, static_cast<u16>(EventCommand::EndGame));
                }
            }

            record.spirit = std::max<i16>(record.spirit, 0);
            record.stamina = std::max<i16>(record.stamina, 0);
            this->Next(run);
            return true;
        }

        case EventCommand::SaveMenu:
            owner.menuMode = Menu::ScriptSaveMode;
            owner.saveScreenPage = 1;
            owner.slotsLoad = 0;
            owner.storyState = Game::SaveSlotsState;
            owner.ResetSaveSlots();
            this->Next(run);
            return false;

        case EventCommand::StopMusic:
        {
            // swd3e: the map's tunes (the first list) follow the playing piece after the fade the last MusicFade gave;
            // the story's tunes are forgotten.
            auto& tunes = view.musicTunes;
            const int repeat = tunes[Scene::SceneFade] & kMusicRepeatMask;
            owner.audio.Queue().Switch(MusicQueue::FirstList, repeat, owner.audio.FadeLength());
            owner.audio.SetFadeMode(0);
            owner.audio.SetFadeLength(0);
            tunes[Scene::StoryFade] = 0;
            tunes[Scene::StoryFirst] = 0;
            tunes[Scene::StorySecond] = 0;
            this->Next(run);
            DebugLog(u8"S_M_off[%d,%d]", tunes[Scene::SceneFirst], tunes[Scene::SceneSecond]);
            return true;
        }

        case EventCommand::JumpIfFar:
        {
            Operands operands = this->OperandsOf(run);
            const u16 column = operands.Word();
            const u16 row = operands.Word();
            const u16 operand = operands.Word();
            const u16 tiles = operands.Word();
            const u32 target = operands.Dword();
            const u16 id = operand == Talk::Current ? static_cast<u16>(state.playerRole) : operand;

            if (state.FindRoleOrPlayer(id, run.found))
            {
                const Role& role = state.roles[run.found];
                run.valueA = role.x - MapHeader::TileSize * column + MapHeader::TileSize / 2;
                run.valueB = role.y - MapHeader::TileSize * row;
                const i32 square = run.valueA * run.valueA + run.valueB * run.valueB;
                const auto distance = static_cast<i32>(crt::Ftol(std::sqrt(static_cast<double>(square))));

                if (distance > MapHeader::TileSize * tiles)
                {
                    this->Jump(run, target);
                    return true;
                }
            }
            else
            {
                ErrorBox(
                    this->game.errors,
                    u8"對話系統(DISTANCE)",
                    u8"GUID(%d)\n不在地圖上",
                    id
                );  // 0x004B2554, 0x004B2568
            }

            this->Next(run);
            return true;
        }

        case EventCommand::MusicFade:
        {
            Operands operands = this->OperandsOf(run);
            const u16 mode = operands.Word();
            // swd3e: mode 1 has no length.
            const u16 length = mode == kFadeNoLength ? u16{0} : operands.Word();
            owner.audio.SetFadeMode(mode);
            owner.audio.SetFadeLength(length);
            this->Next(run);
            DebugLog(u8"Fmode[%d,%d]", mode, length);
            return true;
        }

        case EventCommand::StartTimer:
        {
            Operands operands = this->OperandsOf(run);
            const u16 minutes = operands.Word();
            const u16 seconds = operands.Word();
            const u16 event = operands.Word();
            owner.StartTimer(minutes, seconds, event, 0);
            this->Next(run);
            return false;
        }

        case EventCommand::StopTimer:
            view.timerFrames = -1;
            state.ClearGameFlag(GameState::Timer);
            state.ClearGameFlag(GameState::HideTimer);
            state.ClearGameFlag(GameState::TimerEvent);
            this->Next(run);
            return false;

        default:
            return this->UnknownCommand(run);
    }
}

// Commands 0x90..0xC1: the end of the game, story flags, the camera's target, animations, the second map, media
// files, scripts, names, item counts, moves, file names, the movie pause, the followers, ripples, variables, the
// first party member's attributes and waits for the camera, the music and the movie.
bool Talk::RunSystemCommand(EventRun& run)
{
    Game& owner = this->game;
    GameState& state = owner.state;
    Scene& view = owner.scene;
    Party& party = owner.party;
    const Field& field = owner.field;
    Role& player = state.roles[static_cast<std::size_t>(state.playerRole)];

    switch (run.code)
    {
        case EventCommand::EndGame:
            owner.menuState =
                Game::MenuEntering | (this->Byte(run, kOpcodeBytes) == 1 ? Game::SystemMenuB : Game::SystemMenuA);
            owner.storyState = Game::StoryRunning;
            owner.ResetSaveSlots();
            owner.saveScreenPage = 0;
            owner.slotsLoad = 0;
            owner.menuMode = Menu::NoSlotsMode;
            this->Next(run);
            return false;

        case EventCommand::RoleTouchEvent:
        case EventCommand::RoleCellEffects:
        case EventCommand::RoleSolid:
            return this->RunStateCommand(run);

        case EventCommand::HideLoadingScreen:
            state.SetGameFlag(GameState::NoLoadingScreen);
            this->Next(run);
            return false;

        case EventCommand::SpotlightOn:
            state.SetGameFlag(GameState::Spotlight);
            this->Next(run);
            return false;

        case EventCommand::SpotlightOff:
            state.ClearGameFlag(GameState::Spotlight);
            this->Next(run);
            return false;

        case EventCommand::SpotlightAt:
        {
            Operands operands = this->OperandsOf(run);
            i32 x = MapHeader::TileSize * static_cast<i16>(operands.Word());
            view.spotlightX = x;
            const i32 y = MapHeader::TileSize * static_cast<i16>(operands.Word());
            view.spotlightY = y;

            if (x == -1)
            {
                x = Display::Width / 2;
                view.spotlightX = Display::Width / 2;
            }

            if (y == -1)
            {
                // The original stores the centre row in x (and the scaled -1 values never occur).
                x = Display::Height / 2;
                view.spotlightX = Display::Height / 2;
                view.spotlightY = kSpotlightTop;
            }
            else
            {
                if (x > kSpotlightRight || x < kSpotlightLeft)
                {
                    x = x > kSpotlightRight ? kSpotlightRight : kSpotlightLeft;
                    view.spotlightX = x;
                }

                if (y > kSpotlightBottom)
                {
                    view.spotlightY = kSpotlightBottom;
                }
                else if (y < kSpotlightTop)
                {
                    view.spotlightY = kSpotlightTop;
                }
            }

            view.spotlightTargetX = x;
            view.spotlightStepX = 0;
            view.spotlightStepY = 0;
            this->Next(run);
            return false;
        }

        case EventCommand::SpotlightMove:
        {
            Operands operands = this->OperandsOf(run);
            view.spotlightTargetX = MapHeader::TileSize * static_cast<i16>(operands.Word());
            view.spotlightTargetY = MapHeader::TileSize * static_cast<i16>(operands.Word());
            view.spotlightStepX = static_cast<i16>(operands.Word());
            view.spotlightStepY = static_cast<i16>(operands.Word());
            this->Next(run);
            return false;
        }

        case EventCommand::WaitSpotlight:
            if (view.spotlightX == view.spotlightTargetX && view.spotlightY == view.spotlightTargetY)
            {
                this->Next(run);
            }

            return false;

        case EventCommand::ScreenAnimationB:
        {
            Operands operands = this->OperandsOf(run);
            const auto x = static_cast<i16>(operands.Word());
            const auto y = static_cast<i16>(operands.Word());
            const u16 act = operands.Word();
            const u16 action = operands.Word();
            auto animation = std::make_unique<ScriptAnimation>();
            ResetPlayback(animation->act);
            animation->x = x;
            animation->y = y;
            animation->act.id = act;
            animation->act.action = action;
            animation->next = std::move(view.animationsB);
            view.animationsB = std::move(animation);
            this->Next(run);
            return false;
        }

        case EventCommand::WaitAnimationB:
            return this->RunStateCommand(run);

        case EventCommand::EnterDemonPot:
        case EventCommand::LeaveDemonPot:
            // swd3e passes over the commands of the demon refining pot (swd32 moved the party into and out of it).
            this->Advance(run, kOpcodeBytes);
            return false;

        case EventCommand::SetReturnMap:
            // swd3e passes over the place to come back to from the pot.
            this->Advance(run, CommandBytes(run.code));
            return true;

        case EventCommand::CopyMedia:
        case EventCommand::DeleteMedia:
        case EventCommand::Unk0CD:
            // swd3e: the script stays on these commands for good (swd32 copied or deleted a file of the CD).
            return false;

        case EventCommand::DialogCentred:
            this->dialogCentred = 1;
            this->Next(run);
            return true;

        case EventCommand::OpenScript:
            run.valueA = static_cast<i16>(this->OperandsOf(run).Word());
            this->OpenScript(state.leader, static_cast<u32>(run.valueA));
            run.cursor = 0;
            return true;

        case EventCommand::VariableNamePlate:
            return this->LoadName(run);

        case EventCommand::JumpIfNotMap:
        case EventCommand::JumpIfMap:
        {
            Operands operands = this->OperandsOf(run);
            const auto map = static_cast<i16>(operands.Word());
            const u32 target = operands.Dword();
            run.valueA = map;

            if ((run.code == EventCommand::JumpIfNotMap) == (run.valueA == owner.currentMap))
            {
                this->Next(run);
                return true;
            }

            this->Jump(run, target);
            return true;
        }

        case EventCommand::JumpIfItemsAtLeast:
        case EventCommand::JumpIfItemsAtMost:
        {
            Operands operands = this->OperandsOf(run);
            const auto id = static_cast<i16>(operands.Word());
            i32 count = 0;

            if (const ItemNode* carried = FindItemOfKind(party.inventory, id))
            {
                count = carried->count + carried->pending;
            }

            if (const ItemNode* worn = FindRoleItem(party.equipment, id))
            {
                count += worn->pending + worn->count;
            }

            run.valueB = count;
            const i32 wanted = static_cast<i16>(operands.Word());
            run.valueA = wanted;
            const bool jumps = count == 0 ? run.code == EventCommand::JumpIfItemsAtMost
                               : run.code == EventCommand::JumpIfItemsAtLeast ? count >= wanted
                                                                              : count <= wanted;

            if (jumps)
            {
                this->Jump(run, operands.Dword());
                return true;
            }

            this->Next(run);
            return true;
        }

        case EventCommand::JumpIfWorn:
        case EventCommand::JumpIfNotWorn:
            return this->RunStateCommand(run);

        case EventCommand::MoveRolesActing:
            return this->MoveRoles(run);

        case EventCommand::ClearFileNameA:
        case EventCommand::SetFileNameA:
        case EventCommand::ClearFileNameB:
        case EventCommand::SetFileNameB:
        {
            std::string name;

            if (SetsFileName(run.code))
            {
                const std::vector<u8>& script = owner.databases.script;
                const std::size_t start = run.cursor + kOpcodeBytes;
                const std::size_t end = TextEndAt(script, start);

                if (start < end)
                {
                    name.assign(script.begin() + start, script.begin() + end);
                }

                run.cursor = end + kWordBytes;
                this->SyncOffset(run);
            }
            else
            {
                this->Next(run);
            }

            const bool second = run.code > EventCommand::SetFileNameA;
            run.valueA = second ? 1 : 0;
            std::optional<std::string>& slot = second ? view.noExitWarpText : view.noTownWarpText;

            if (SetsFileName(run.code))
            {
                slot = name.substr(0, kFileNameBytes);
                state.SetGameFlag(run.valueA + GameState::FileNameA);
            }
            else
            {
                slot.reset();
                state.ClearGameFlag(run.valueA + GameState::FileNameA);
            }

            return false;
        }

        case EventCommand::PauseMapMovie:
            this->Next(run);
            owner.fieldView.movie.options |= MapMovie::Paused;
            return true;

        case EventCommand::ResumeMapMovie:
            this->Next(run);
            owner.fieldView.movie.options &= ~MapMovie::Paused;
            return false;

        case EventCommand::GatherParty:
        {
            const u16 command = this->Word(run, kCodeWord);

            if ((command & kCommandStarted) == 0)
            {
                run.valueA = 1;
                this->SetWord(run, kCodeWord, static_cast<u16>(command | kCommandStarted));
                view.playerHold |= Scene::HoldLocked;
                player.act.action = 0;
                view.trailY.fill(player.y);
                view.trailX.fill(player.x);

                for (int index = 1; index < state.roleCount && index < GameState::Roles; ++index)
                {
                    Role& role = state.roles[static_cast<std::size_t>(index)];

                    if ((role.flags & Role::Follower) != 0)
                    {
                        role.flags |= Role::FastMove;
                        role.act.speed = ActState::FixedDelay;  // a fixed delay of 0 frames
                    }

                    run.valueA = state.roleCount;
                }

                return false;
            }

            // swd3e: the trail starts again at the controlled role while it stands on a tile.
            if (((player.x | player.y) & MapHeader::TileFraction) == 0)
            {
                view.trailY.fill(player.y);
                view.trailX.fill(player.x);
            }

            run.valueB = 0;
            run.valueA = 1;

            for (int index = 1; index < state.roleCount && index < GameState::Roles; ++index)
            {
                Role& role = state.roles[static_cast<std::size_t>(index)];

                if ((role.flags & Role::Follower) != 0 && role.x == player.x && role.y == player.y)
                {
                    ++run.valueB;
                    role.flags &= ~Role::FastMove;
                    role.act.speed = 0;
                }

                run.valueA = state.roleCount;
            }

            if (run.valueB + 1 == view.followerCount)
            {
                this->SetWord(run, kCodeWord, static_cast<u16>(command & ~kCommandStarted));
                this->Next(run);
            }

            return false;
        }

        case EventCommand::PassWalls:
        {
            Operands operands = this->OperandsOf(run);
            const u16 operand = operands.Word();
            const u16 id = operand == Talk::Current ? static_cast<u16>(state.playerRole) : operand;

            if (state.FindRoleOrPlayer(id, run.found))
            {
                state.roles[run.found].flags |= Role::PassWalls;
            }

            this->Next(run);
            return true;
        }

        case EventCommand::Ripple:
        {
            Operands operands = this->OperandsOf(run);
            const i32 x = static_cast<i16>(operands.Word());
            const i32 y = static_cast<i16>(operands.Word());
            const i32 radius = static_cast<i16>(operands.Word());
            const i32 depth = static_cast<i16>(operands.Word());
            run.valueA = x;
            run.valueB = y;
            constexpr int kSplashRadius = 0x18;
            // On the map's canvas, the place counted from the 640x480 screen in it (deviations.md).
            const Field& field = owner.field;
            auto ripple = std::make_unique<ScriptObject>(
                field.viewWidth,
                field.viewHeight,
                x - radius + field.screenX,
                y - radius + field.screenY,
                2 * radius,
                2 * radius
            );
            ripple->Splash(radius, radius, kSplashRadius, depth, owner.crtRandom);
            ripple->next = std::move(view.objects);
            view.objects = std::move(ripple);
            this->Next(run);
            return true;
        }

        case EventCommand::PauseGame:
            owner.running = 0;
            this->Next(run);
            return false;

        case EventCommand::SetVariableWide:
        case EventCommand::AddVariableWide:
        case EventCommand::SubtractVariableWide:
        case EventCommand::JumpIfVariableAtLeastWide:
        case EventCommand::JumpIfVariableAtMostWide:
            return this->ChangeVariable(run);

        case EventCommand::JumpIfStatAtLeast:
        case EventCommand::JumpIfStatAtMost:
        {
            Operands operands = this->OperandsOf(run);
            const i32 field = static_cast<i16>(operands.Word());
            run.valueB = field;

            if (field > Party::LastStat)
            {
                ErrorBox(
                    this->game.errors,
                    u8"對話系統(attb)",
                    u8"屬性代號不合法(%d)\n",
                    field
                );  // 0x004B2504, 0x004B2514
                return false;
            }

            const int value = party.RoleStat(field, 1);
            run.valueA = value;
            const i32 wanted = operands.Word();

            if (run.code == EventCommand::JumpIfStatAtLeast ? value < wanted : value > wanted)
            {
                this->Next(run);
                return true;
            }

            this->Jump(run, operands.Dword());
            return false;
        }

        case EventCommand::SetStat:
        case EventCommand::AddStat:
        case EventCommand::SubtractStat:
        {
            Operands operands = this->OperandsOf(run);
            const i32 field = static_cast<i16>(operands.Word());
            const i32 value = static_cast<i16>(operands.Word());
            run.valueB = field;

            if (field > Party::LastStat)
            {
                ErrorBox(
                    this->game.errors,
                    u8"對話系統(attb)",
                    u8"屬性代號不合法(%d)\n",
                    field
                );  // 0x004B2504, 0x004B2514
                return false;
            }

            if (run.code == EventCommand::SetStat)
            {
                party.SetRoleStat(field, value, 1, owner.database);
            }
            else if (run.code == EventCommand::AddStat)
            {
                party.SetRoleStat(field, value + party.RoleStat(field, 1), 1, owner.database);
            }
            else
            {
                party.SetRoleStat(field, party.RoleStat(field, 1) - value, 1, owner.database);
            }

            this->Next(run);
            return true;
        }

        case EventCommand::WaitCameraY:
            if ((view.panAcross == 0 && view.panDown == 0) ||
                field.cameraTop + field.screenY == static_cast<i16>(this->OperandsOf(run).Word()))
            {
                this->Next(run);
                return true;
            }

            return false;

        case EventCommand::WaitMusicFade:
            // swd3e: the playing piece has stopped.
            if (owner.audio.Queue().IsStopped())
            {
                this->Next(run);
            }

            return false;

        case EventCommand::JumpIfRoleAtTile:
        {
            // The role is a slot of the roles, not an id.
            Operands operands = this->OperandsOf(run);
            const u16 slot = operands.Word();
            const u16 column = operands.Word();
            const u16 row = operands.Word();
            const u32 target = operands.Dword();
            const Role& role = state.roles[std::min<std::size_t>(slot, GameState::Roles - 1)];

            if (static_cast<u32>(role.x) >> MapHeader::TileShift == column &&
                static_cast<u32>(role.y) >> MapHeader::TileShift == row)
            {
                this->Jump(run, target);
                return true;
            }

            this->Next(run);
            return true;
        }

        case EventCommand::PlayVideo:
            // The original clears the drawing surface and shows the offscreen surface first; HUGE.lmf is closed until
            // WaitVideo.
            owner.mapCache.lmf.Close();
            std::fill_n(owner.drawSurface, Display::Pixels, Pixel{0});
            owner.ShowFrame();
            owner.PlayVideo(run.cursor, run.context.eventCursor);
            return false;

        case EventCommand::WaitVideoFrame:
        {
            Operands operands = this->OperandsOf(run);
            run.valueA = owner.VideoPosition();

            if (run.valueA != operands.Word())
            {
                return false;
            }

            const auto first = static_cast<i32>(operands.Word());
            const auto second = static_cast<i32>(operands.Word());
            owner.SetVideoMarks(first, second);
            std::fill_n(owner.drawSurface + kVideoClearFrom, kVideoClearPixels, Pixel{0});
            this->Next(run);
            return true;
        }

        case EventCommand::WaitVideo:
            run.valueA = owner.VideoPosition();

            if (run.valueA >= 0)
            {
                return false;
            }

            this->Next(run);

            if (!this->keepArchiveClosed)
            {
                owner.field.ReopenArchive();
            }

            this->keepArchiveClosed = false;
            return true;

        case EventCommand::SetRoleVariable:
        {
            Operands operands = this->OperandsOf(run);
            const u16 id = operands.Word();
            const u16 variable = operands.Word();
            i32 value = operands.Word();

            if (this->RoleVariable(id, variable, value, Talk::WriteVariable) == 0)
            {
                ErrorBox(owner.errors, u8"Err(Talk:rv_set)", u8"找不到角色 %d", id);  // 0x004B24E0, 0x004B24F4
            }

            this->Next(run);
            return true;
        }

        case EventCommand::AddRoleVariable:
        case EventCommand::SubtractRoleVariable:
        {
            Operands operands = this->OperandsOf(run);
            const u16 id = operands.Word();
            const u16 variable = operands.Word();
            const u16 amount = operands.Word();
            i32 value = 0;

            if (this->RoleVariable(id, variable, value, Talk::ReadVariable) != 0)
            {
                value = run.code == EventCommand::AddRoleVariable ? value + amount : value - amount;
                this->RoleVariable(id, variable, value, Talk::WriteVariable);
            }
            else
            {
                ErrorBox(owner.errors, u8"Err(Talk:rv_add)", u8"找不到角色 %d", id);  // 0x004B24CC, 0x004B24F4
            }

            this->Next(run);
            return true;
        }

        case EventCommand::JumpIfRoleVariableAtLeast:
        case EventCommand::JumpIfRoleVariableAtMost:
        case EventCommand::JumpIfRoleVariableEquals:
        {
            Operands operands = this->OperandsOf(run);
            const u16 id = operands.Word();
            const u16 variable = operands.Word();
            const u16 limit = operands.Word();
            const u32 target = operands.Dword();
            i32 value = 0;

            if (this->RoleVariable(id, variable, value, Talk::ReadVariable) == 0)
            {
                // The report keeps the title of AddRoleVariable.
                ErrorBox(owner.errors, u8"Err(Talk:rv_add)", u8"找不到角色 %d", id);  // 0x004B24CC, 0x004B24F4
                this->Next(run);
                return true;
            }

            const auto read = static_cast<u32>(value);
            bool jumps = read == limit;

            if (run.code == EventCommand::JumpIfRoleVariableAtLeast)
            {
                jumps = read >= limit;
            }
            else if (run.code == EventCommand::JumpIfRoleVariableAtMost)
            {
                jumps = read <= limit;
            }

            if (jumps)
            {
                this->Jump(run, target);
                return true;
            }

            this->Next(run);
            return true;
        }

        case EventCommand::SwitchRoleVariable:
        {
            // The targets (dwords) follow the role and the variable up to kTargetsEnd; the command is passed by the
            // targets without their end mark (a bug kept).
            constexpr std::size_t kTargetsAt = kWordBytes * 3;
            const std::vector<u8>& script = owner.databases.script;
            const u16 id = this->Word(run, kFirstOperand);
            const u16 variable = this->Word(run, kFirstOperand + 1);
            u32 count = 0;

            while (run.cursor + kTargetsAt + sizeof(u32) * count + sizeof(u32) <= script.size() &&
                   this->Dword(run, kTargetsAt + sizeof(u32) * count) != kTargetsEnd)
            {
                ++count;
            }

            const auto passed = static_cast<int>(kTargetsAt + sizeof(u32) * count);
            i32 value = 0;
            const u32 found = this->RoleVariable(id, variable, value, Talk::ReadVariable);

            if (found == 0)
            {
                ErrorBox(owner.errors, u8"Err(Talk:rv_switch)", u8"找不到角色 %d", id);  // 0x004B247C, 0x004B24F4
                this->Advance(run, passed);
                return true;
            }

            if (static_cast<u32>(value) >= count)
            {
                ErrorBox(owner.errors, u8"Err(Talk:rv_switch)", u8"數值太大(%d) 沒有定義 %d", found, id);  // 0x004B2460
                this->Advance(run, passed);
                return true;
            }

            this->Jump(run, this->Dword(run, kTargetsAt + sizeof(u32) * static_cast<u32>(value)));
            return true;
        }

        case EventCommand::JumpIfRolesApart:
        {
            // Talk::Current stands for the slot of the controlled role, taken as an id (a bug kept).
            Operands operands = this->OperandsOf(run);
            u16 first = operands.Word();
            u16 second = operands.Word();
            const u16 tiles = operands.Word();
            const u32 target = operands.Dword();
            first = first == Talk::Current ? static_cast<u16>(state.playerRole) : first;
            u32 firstSlot = 0;
            u32 secondSlot = 0;

            if (!state.FindRoleOrPlayer(first, firstSlot))
            {
                ErrorBox(owner.errors, u8"對話系統(DISTANCE_role)", u8"GUID(%d)\n不在地圖上", first);  // 0x004B2448
                this->Next(run);
                return true;
            }

            second = second == Talk::Current ? static_cast<u16>(state.playerRole) : second;

            if (!state.FindRoleOrPlayer(second, secondSlot))
            {
                ErrorBox(owner.errors, u8"對話系統(DISTANCE_role)", u8"GUID(%d)\n不在地圖上", second);  // 0x004B2568
                this->Next(run);
                return true;
            }

            const Role& one = state.roles[firstSlot];
            const Role& other = state.roles[secondSlot];
            const i32 dx = one.x - other.x;
            const i32 dy = one.y - other.y;
            run.valueA = dx;
            run.valueB = dy;
            const auto distance = static_cast<i32>(crt::Ftol(std::sqrt(static_cast<double>(dx * dx + dy * dy))));

            if (distance > MapHeader::TileSize * tiles)
            {
                this->Jump(run, target);
                return true;
            }

            this->Next(run);
            return true;
        }

        case EventCommand::SetControlledRole:
        {
            // The slots up to roleCount itself are compared (one past the roles in use).
            const u16 id = this->OperandsOf(run).Word();

            for (int index = 0; index <= state.roleCount && index < GameState::Roles; ++index)
            {
                if (state.roles[static_cast<std::size_t>(index)].id == id)
                {
                    state.playerRole = index;
                    owner.partyLeaderId = id;
                }
            }

            this->Next(run);
            return true;
        }

        case EventCommand::JumpIfAnyFollower:
            for (int index = 0; index <= state.roleCount && index < GameState::Roles; ++index)
            {
                if ((state.roles[static_cast<std::size_t>(index)].flags & Role::Follower) != 0)
                {
                    this->Jump(run, this->OperandsOf(run).Dword());
                    return true;
                }
            }

            this->Next(run);
            return true;

        case EventCommand::WaitSprite:
        {
            Operands operands = this->OperandsOf(run);
            const u16 act = operands.Word();
            const u16 action = operands.Word();

            for (const ScriptSprite* sprite = view.spritesA.get(); sprite != nullptr; sprite = sprite->next.get())
            {
                if (sprite->act.id == act && sprite->act.action == action)
                {
                    return false;
                }
            }

            this->Next(run);
            return true;
        }

        case EventCommand::NameBox:
            return this->OpenNameBoxes(run);

        case EventCommand::StartAscentGame:
            owner.systemFlags |= Game::AscentRunning;
            owner.ascentGame.Start(
                AscentGame::SceneUnk00,
                AscentGame::SceneAct,
                AscentGame::BackgroundAction,
                AscentGame::SceneAct,
                AscentGame::PlayerAction,
                AscentGame::SceneAct,
                AscentGame::LeaderAction,
                AscentGame::FullStamina
            );

            for (u32 kind = 0; kind < AscentGame::ObstacleActions.size(); ++kind)
            {
                owner.ascentGame.SetObstacleAct(kind, AscentGame::SceneAct, AscentGame::ObstacleActions[kind]);
            }

            this->Next(run);
            return false;

        case EventCommand::ShowFollowers:
        case EventCommand::HideFollowers:
            run.valueA = std::max(state.roleCount, 1);  // the loop's end is left behind

            for (int index = 1; index < state.roleCount && index < GameState::Roles; ++index)
            {
                Role& role = state.roles[static_cast<std::size_t>(index)];

                if ((role.flags & Role::Follower) == 0)
                {
                    continue;
                }

                if (run.code == EventCommand::ShowFollowers)
                {
                    role.flags &= ~Role::Hidden;
                }
                else
                {
                    role.flags |= Role::Hidden;
                }
            }

            this->Next(run);
            return true;

        case EventCommand::JumpIfRoleVariableNotBelow:
        {
            Operands operands = this->OperandsOf(run);
            const u16 firstId = operands.Word();
            const u16 firstVariable = operands.Word();
            const u16 secondId = operands.Word();
            const u16 secondVariable = operands.Word();
            const u32 target = operands.Dword();
            i32 first = 0;
            i32 second = 0;

            if (this->RoleVariable(firstId, firstVariable, first, Talk::ReadVariable) == 0)
            {
                ErrorBox(owner.errors, u8"Err(Talk:rv_comp)", u8"找不到角色 %d(1)", firstId);  // 0x004B24A4, 0x004B24B8
                this->Next(run);
                return true;
            }

            if (this->RoleVariable(secondId, secondVariable, second, Talk::ReadVariable) == 0)
            {
                ErrorBox(owner.errors, u8"Err(Talk:rv_comp)", u8"找不到角色 %d(2)", secondId);  // 0x004B2490
            }
            else if (static_cast<u32>(first) >= static_cast<u32>(second))
            {
                this->Jump(run, target);
                return true;
            }

            this->Next(run);
            return true;
        }

        case EventCommand::KeepArchiveClosed:
            this->keepArchiveClosed = true;
            this->Next(run);
            return true;

        default:
            return this->UnknownCommand(run);
    }
}

}  // namespace swd3e
