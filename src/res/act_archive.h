#pragma once

#include <array>
#include <functional>
#include <list>
#include <string>
#include <vector>

#include "core/types.h"
#include "file/file.h"
#include "res/act_state.h"
#include "text/big5.h"

namespace swd3e
{

class ErrorReporter;

// The act archives (0x00436B40-0x00437FC0) and the action script interpreter (0x00436C80): six files addressed by
// id / 3000, opened at the first use, whose directory records are cached in sixteen chains, a byte-limited cache of
// whole entries in sixteen chains, and direct loading. An entry holds a count and a table of offsets to its scripts
// (sub entries).
class ActArchive
{
public:
    // Texts of the archive (Big5 in the executable; u8 here, converted where drawn).
    static constexpr std::u8string_view MemoryError = u8"ACT記憶體管理有誤!!";  // 0x004B7128
    static constexpr std::u8string_view MemoryErrorTitle = u8"ERR";             // 0x004B713C
    static constexpr std::array<std::string_view, 6> FileNames = {
        "all_char.act",
        "all_item.act",
        "all_magic.act",
        "all_sys.act",
        "all_map1.act",
        "all_map2.act",
    };

    static constexpr int FileCount = 6;
    static constexpr int IndicesPerFile = 3000;
    static constexpr int Chains = 16;
    static constexpr u32 ChainMask = 0xF;
    static constexpr int MapAct = 0xFFFF;
    static constexpr u16 FlagZoom = 0x80;  // ActState::flags: 'CS' set the zoom (Painter::ZoomModes)

    // Painter::SetZoom (0x00423630) called by 'CS': the state's x and y as the anchor and the opcode's value twice.
    using ZoomStart = std::function<void(i32 x, i32 y, i32 zoomX, i32 zoomY)>;

    explicit ActArchive(ErrorReporter& errors);

    // The directory the archives are opened from at the first use (the original reads 0x004C2B94 then).
    void SetGameDirectory(std::u8string directory);
    void SetZoomStart(ZoomStart start);
    void SetCacheLimit(int bytes);
    ActScript Get(int id, int sub);
    ActScript LoadDirect(int id, int sub);
    int CloseAll();
    void SetUseCache(int value);
    void SetZoomEnabled(int value);
    int CacheBytes() const;
    bool ScriptRestarted() const;
    // The rebuild's smooth motion: the flag put back after a menu frame is painted again.
    void SetScriptRestarted(bool value);
    int Run(ActState& state);
    // The rebuild's smooth battle: `state` one frame on (a copy run without setting the zoom; what Run leaves in the
    // archive is kept).
    ActState Peek(const ActState& state);

private:
    struct IndexEntry
    {
        int file = 0;
        int entry = 0;
        u32 offset = 0;
        u32 size = 0;
    };

    struct CacheEntry
    {
        int id = 0;
        u32 size = 0;
        std::shared_ptr<const std::vector<u8>> data;
    };

    void OpenArchives();
    bool OpenFile(File& file, const std::u8string& name);
    ActScript Extract(const std::shared_ptr<const std::vector<u8>>& entry, int sub) const;
    std::shared_ptr<const std::vector<u8>> LoadEntry(File* file, int fileIndex, int entry, u32& size);
    bool CacheLoad(int id, int sub);
    bool CacheFind(int id, int sub);
    void CacheEvict();
    void CacheClear();
    void IndexInsert(File* file, int fileIndex, int entry, u32& offset, u32& size);
    void IndexLookup(int fileIndex, int entry, u32& offset, u32& size);
    void IndexClear();
    void Restart(ActState& state, u32 keptFlags);
    File* ArchiveOf(int id);

    ErrorReporter& errors;
    ZoomStart zoomStart;
    std::u8string gameDirectory;
    std::array<File, FileCount> files;
    bool opened = false;                               // 0x004F5170
    int cacheLimit = 0x100000;                         // 0x004B70C0
    u32 cacheBytes = 0;                                // 0x004F5168
    int useCache = 0;                                  // 0x004F516C
    int zoomEnabled = 0;                               // 0x004F5400
    bool restarted = false;                            // 0x004F5160
    bool peeking = false;                              // the rebuild's Peek
    u32 reads = 0;                                     // 0x004F5164: counts the file reads, read by nothing
    u16 lastZoom = 0;                                  // 0x004F5120: the last 'CS' value, read by nothing
    u16 lastZoomX = 0;                                 // 0x004F517C, read by nothing
    u16 lastZoomY = 0;                                 // 0x004F517E, read by nothing
    std::array<std::list<CacheEntry>, Chains> chains;  // 0x004F5180
    std::array<u16, Chains> chainCounts{};             // 0x004F4FBC
    std::array<std::list<IndexEntry>, Chains> index;   // 0x004F4FE0
    std::array<u16, Chains> indexCounts{};             // 0x004F5140
    ActScript current;                                 // 0x004F513C
    ActScript direct;                                  // 0x004F5174
};

}  // namespace swd3e
