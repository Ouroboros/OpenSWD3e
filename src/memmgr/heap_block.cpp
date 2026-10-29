#include "memmgr/heap_block.h"

#include <new>

namespace swd3e
{

namespace
{

constexpr u32 kSmallBlockLimit = 0x3F8;  // 0x004C0534: requests up to it go to the small-block heap
constexpr u32 kEntrySlack = 0x17;        // a small-block entry: the request, a header and a trailer, in paragraphs
constexpr u32 kEntryOverhead = 8;        // the header and trailer _msize leaves out
constexpr u32 kParagraphSlack = 0xF;     // a larger request is rounded up to 16 bytes for HeapAlloc
constexpr u32 kParagraphMask = ~0xFu;

}  // namespace

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

// swd3e.exe 0x004A207E: _msize of a block from 0x0043DA70 (the malloc request rounded as Allocate does). The
// small-block heap (0x004A1B1C, 0x004A3E17) keeps (request + 0x17) & ~0xF bytes and marks the size before the block, of
// which _msize reports 8 less; a larger request is HeapAlloc'ed rounded to 16 bytes and HeapSize reports that.
u32 HeapBlock::ReportedSize(u32 size)
{
    const u32 request = (size + HeapBlock::Slack) & HeapBlock::Align;

    if (request <= kSmallBlockLimit)
    {
        return ((request + kEntrySlack) & kParagraphMask) - kEntryOverhead;
    }

    return (request + kParagraphSlack) & kParagraphMask;
}

}  // namespace swd3e
