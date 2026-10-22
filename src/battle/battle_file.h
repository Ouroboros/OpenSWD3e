#pragma once

#include <array>
#include <span>
#include <string_view>
#include <utility>

#include "core/types.h"

namespace swd3e
{

// A battle of battle.ffd (0x1EC bytes, read to 0x0050EDE0): the background and up to eight animations over it, and up
// to 24 enemy slots with their places.
struct BattleFormation
{
    static constexpr std::size_t UnkSlots = 7;
    static constexpr std::size_t Animations = 8;
    static constexpr std::size_t MaxEnemies = 24;  // the slots of a record (a battle has BattleState::Enemies units)
    static constexpr std::size_t Unk48Bytes = 0x10;

    i32 unk00 = 0;                                // +0x00
    i32 background = 0;                           // +0x04 the background (picture background + 15000)
    std::array<i32, UnkSlots> unk08 = {};         // +0x08
    u16 animationCount = 0;                       // +0x24 the animations over the background
    u16 unk26 = 0;                                // +0x26
    std::array<u32, Animations> animations = {};  // +0x28 the low words: their acts
    std::array<u8, Unk48Bytes> unk48 = {};        // +0x48
    std::array<i32, Animations> animationX = {};  // +0x58
    std::array<i32, Animations> animationY = {};  // +0x78
    u16 enemyCount = 0;                           // +0x98
    u16 unk9A = 0;                                // +0x9A
    std::array<u32, MaxEnemies> enemies = {};     // +0x9C the low words: the mon.dat objects
    std::array<u16, MaxEnemies> guarding = {};    // +0xFC 1: the enemy starts guarding
    std::array<u32, MaxEnemies> enemyX = {};      // +0x12C the low words
    std::array<u32, MaxEnemies> enemyY = {};      // +0x18C

    void Load(std::span<const u8> bytes);
};

class Projector;

// battle.ffd (the object at 0x0050F620): a header of 0x2714 bytes (a table of record numbers from +4 and, from
// +0x1F44, the number of formations of each battle) followed by the 0x1EC-byte formations.
class BattleFile
{
public:
    static constexpr u32 HeaderBytes = 0x2714;
    static constexpr u32 RecordBytes = 0x1EC;
    static constexpr u32 RecordsAt = 4;  // the table of record numbers
    static constexpr u32 CountsAt = 0x1F44;
    static constexpr std::size_t Steps = 0x1E;

    explicit BattleFile(const Projector* projector = nullptr);

    bool ReadHeader(std::u8string_view path);
    bool ReadFormation(std::u8string_view path, BattleFormation& formation, u16 battle, i8 choice);
    std::span<const u8> Counts() const;

    const Projector* projector = nullptr;      // +0x00 the battle's projector (never read)
    std::array<u8, HeaderBytes> header = {};   // +0x04
    std::array<u8, RecordBytes> unk2718 = {};  // +0x2718 a formation buffer cleared by the constructor (never read)
    // +0x3C24: the numbers 0..29 with five quarters of each (never read).
    std::array<std::pair<i32, i32>, Steps> unk3C24 = {};
};

}  // namespace swd3e
