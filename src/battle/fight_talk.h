#pragma once

#include <array>
#include <string>
#include <vector>

#include "core/types.h"
#include "file/file.h"

namespace swd3e
{

struct TalkWindow;

// The battle talk of figtalk.dat: a table of dword offsets from 0x200 (one per battle) and the script of each battle,
// read into 0x8000 bytes when the battle starts.
class FightTalk
{
public:
    static constexpr u32 HeaderBytes = 0x200;
    static constexpr u32 ScriptBytes = 0x8000;

    void Load(const std::u8string& installDirectory, u16 battle);
    void Close();
    void Seek(i32 offset);
    i16 Word(std::size_t index) const;
    u16 UnsignedWord(std::size_t index) const;

    i32 enabled = 1;         // 0x004BF908
    File file;               // 0x00578298 (the handle)
    bool open = false;       // 0x00578464
    std::vector<u8> script;  // 0x00578440 (the allocation)
    std::size_t cursor = 0;  // 0x0057843C the command read next (a pointer in the original)
    // The state of the talk (cleared by Reset).
    i32 windowTimeout = 0xFFFF;  // 0x004BF8FC the timeout of the next window (0xFFFF: none)
    i32 battleRuns = 1;          // 0x004BF904 1: the battle runs on while the talk waits
    i32 unk57828C = 0;           // 0x0057828C command 6: 0x8000 while it pauses, the low bits the frames left
    // 0x00578294 command 1: 0x8000 while it waits, the low bits the unit turns left (counted down by RoleFrame and
    // EnemyFrame)
    u16 waitCount = 0;
    i32 unk5782A4 = 0;         // 0x005782A4 a value of the command run (a y, the spirit, an enemy)
    i32 unk5782A8 = 0;         // 0x005782A8 a value of the command run (a target)
    i32 unk578414 = 0;         // 0x00578414
    i32 unk578418 = 0;         // 0x00578418
    i32 unk57841C = 0;         // 0x0057841C
    i32 unk578420 = 0;         // 0x00578420
    i32 savedUnit = 0;         // 0x00578424 the unit to give the turn back to
    i32 battleActingUnit = 0;  // 0x00578428 BattleState::actingUnit as the battle frame copied it
    u16 commandValue = 0;      // 0x0057842C the running command's working value (slide, curve step, list entry)
    u16 unk57842E = 0;         // 0x0057842E
    i32 unk578430 = 0;         // 0x00578430 the x (low word) and y (high word) of a unit or window
    i32 windowFlags = 0;       // 0x00578444 the flags of the next window, kWindowShown in the high word
    i32 unk578448 = 0;         // 0x00578448 command 0x10 count (low word); a word of the command run (high word)
    i32 unk57844C = 0;         // 0x0057844C two words of the list commands (count or index, place)
    u16 unk578450 = 0;         // 0x00578450
    u16 unk578452 = 0;         // 0x00578452
    u16 unk578454 = 0;         // 0x00578454
    u16 unk578456 = 0;         // 0x00578456 the units listed by commands 0xE and 0x3D
    // 0x00578458 (swd3e) the times the battle may be fought for the full rewards (talk opcode 0x54; battleRepeats)
    u16 repeatLimit = 0;
    i32 unk57845C = 0;  // 0x0057845C the choices counted by command 0x1F
    i32 offset = 0;     // 0x00578460 the place of the script read last (Seek)
    i32 wonByTalk = 0;  // 0x00578468 1: the battle ends as won (Game::CheckBattleEnd)
    // The state of the talk frames (Game::FightTalkFrame).
    i32 unk5782A0 = 0;        // 0x005782A0 a value of the command run (also the result of BattleFrame)
    std::size_t textAt = 0;   // 0x00578438 the text of a talk window in the script (a pointer in the original)
    i32 drawMovedAtOnce = 0;  // 0x0057846C 1: a moved unit is drawn at once
    std::array<char, 0x20> speaker = {};           // 0x005783F4 the name of the speaker of the next window
    TalkWindow* window = nullptr;                  // 0x00578284 the talk window opened last (kept by Talk)
    std::array<std::array<i16, 2>, 9> curve = {};  // 0x005782AC the points of the curve of command 0x27
};

}  // namespace swd3e
