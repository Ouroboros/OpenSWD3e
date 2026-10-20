#include "memmgr/heap_block.h"

#include <new>

namespace swd3e
{

// swd3e.exe 0x0043DA70: malloc((size + 0xF) & ~3) cleared; NULL for a size of 0 and below (new in swd3e)
bool HeapBlock::Allocate(std::vector<u8>& block, u32 size)
{
    if (static_cast<i32>(size) <= 0 || size >= HeapBlock::AllocationLimit)
    {
        block.clear();
        return false;
    }

    try
    {
        block.assign((size + HeapBlock::Slack) & HeapBlock::Align, 0);
    }
    catch (const std::bad_alloc&)
    {
        block.clear();
        return false;
    }

    return true;
}

// swd3e.exe 0x0043DAB0: free unless NULL (new in swd3e; its swd32 pairing with 0x00485910 by similarity is wrong)
void HeapBlock::Free(std::vector<u8>& block)
{
    block.clear();
    block.shrink_to_fit();
}

}  // namespace swd3e
