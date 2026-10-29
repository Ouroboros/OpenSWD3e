#pragma once

#include <vector>

#include "core/types.h"

namespace swd3e
{

// The zeroed heap blocks of swd3e (0x0043DA70 allocates, 0x0043DAB0 frees), held in vectors.
class HeapBlock
{
public:
    // A block is rounded up to a multiple of 4 with at least 12 bytes over the request.
    static constexpr u32 Slack = 0xF;
    static constexpr u32 Align = ~3u;
    // The original 32-bit process gets no memory for requests of 1 GiB and more (as read from garbage records).
    static constexpr u32 AllocationLimit = 0x40000000;

    // A zeroed block for `size` bytes (as the original rounds it); none for sizes of 0 and below (as signed) or where
    // the allocation fails. True when the block is there.
    static bool Allocate(std::vector<u8>& block, u32 size);
    // Frees the block (a missing one is ignored).
    static void Free(std::vector<u8>& block);
    // The size the original's _msize reports for a block Allocate made for `size` bytes.
    static u32 ReportedSize(u32 size);
};

}  // namespace swd3e
