#pragma once

#include <array>
#include <memory>
#include <span>
#include <vector>

#include "core/types.h"
#include "game/game_state.h"

namespace swd3e
{

class ErrorReporter;
struct BlockingMap;

// An A* search node (0x58 bytes in the original pool; swd32's had two more words, at +0x00 and +0x14).
struct PathNode
{
    i32 x = 0;                               // +0x00 pixels
    i32 y = 0;                               // +0x04
    i32 tile = 0;                            // +0x08
    i32 direction = 0;                       // +0x0C the direction from the parent
    double f = 0;                            // +0x10 g + h
    double h = 0;                            // +0x18 the distance to the target
    double g = 0;                            // +0x20 the length from the search origin
    PathNode* poolNext = nullptr;            // +0x28 the free pool, or the nodes owned by a search
    PathNode* parent = nullptr;              // +0x2C
    PathNode* listNext = nullptr;            // +0x30 the open or closed list
    std::array<PathNode*, 8> children = {};  // +0x34
    i32 unk54 = 0;                           // +0x54 nothing reads it
};

// The node pool shared by every path finder (0x004CB1F0..0x004CB1FC): a free list growing on demand.
class PathNodePool
{
public:
    static constexpr int Initial = 8;

    void Init();
    bool Release();
    PathNode* Alloc();
    int Free(PathNode* node);

    i32 unused = 0;  // 0x004C15BC
    i32 free = 0;    // 0x004C15C0
    i32 peak = 0;    // 0x004C15C4

private:
    PathNode* NewNode();

    std::vector<std::unique_ptr<PathNode>> storage;
    PathNode* head = nullptr;  // 0x004C15C8
};

// An A* path finder over the blocking map for a footprint of width x height tiles (0x4C bytes in the original). It
// searches from the target back to the origin; when the origin cannot be reached the path leads to the node of the
// search nearest it.
class PathFinder
{
public:
    static constexpr int MaxSteps = 0x1FE;
    static constexpr u8 End = 0xFF;
    // The direction byte of a step, by DeltaY[dy + 1] + DeltaX[dx + 1] + 4 (swd3e.exe 0x004AA320, DeltaX 0x004AA32C,
    // DeltaY 0x004AA338).
    static constexpr std::array<i8, 9> DirectionByDelta = {0, 1, 2, 7, 0, 3, 6, 5, 4};
    static constexpr std::array<i32, 3> DeltaX = {-1, 0, 1};
    static constexpr std::array<i32, 3> DeltaY = {-3, 0, 3};
    // swd3e.exe 0x004AA344: the direction a child stores back to its parent (TryNeighbour)
    static constexpr std::array<u8, 8> OppositeDirection = {4, 5, 6, 7, 0, 1, 2, 3};

    PathFinder(PathNodePool& pool, const BlockingMap& map, ErrorReporter& reporter);
    ~PathFinder();
    PathFinder(const PathFinder&) = delete;
    PathFinder& operator=(const PathFinder&) = delete;

    int FindPath(int fromX, int fromY, int toX, int toY, int width, int height, std::span<u8> directions);
    int IsUsable() const;
    int TileIndex(int x, int y) const;
    int IsWalkable(int x, int y) const;
    void Reset();
    PathNode* Search(int fromX, int fromY, int toX, int toY);
    PathNode* PopBest();
    void Expand(PathNode& node, int targetX, int targetY);
    void TryNeighbour(PathNode& parent, int x, int y, int targetX, int targetY, int direction);
    PathNode* FindOpen(int tile) const;
    PathNode* FindClosed(int tile) const;
    void InsertOpen(PathNode& node);
    void Propagate(PathNode& node);
    void Push(PathNode& node);
    PathNode* Pop();
    void UpdateBlocked(int x, int y);
    int SetMask(int mask);

    PathNode* open = nullptr;      // +0x00 sentinel; its poolNext chains every node of the search
    PathNode* closed = nullptr;    // +0x04 sentinel
    PathNode* result = nullptr;    // +0x08 the node reached at the search origin (or `closest`)
    PathNode* closest = nullptr;   // +0x0C the node of the search nearest the origin (smallest h)
    std::vector<PathNode*> stack;  // +0x10
    i32 found = 0;                 // +0x14 1 after a successful FindPath, 0 after an impossible one
    i32 unk18 = 0;
    i32 unk1C = 0;
    i32 width = 0;                                      // +0x20 tiles
    i32 height = 0;                                     // +0x24
    u32 mask = MapHeader::Wall | MapHeader::Unk800000;  // +0x28 blocking bits
    std::array<u32, 8> blocked = {};                    // +0x2C N, NE, E, SE, S, SW, W, NW

private:
    PathNodePool& pool;
    const BlockingMap& map;
    ErrorReporter& reporter;
};

}  // namespace swd3e
