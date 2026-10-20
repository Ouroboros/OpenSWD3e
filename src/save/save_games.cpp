#include "save/save_games.h"

#include "core/format.h"
#include "file/file.h"

namespace swd3e
{

std::u8string SaveGames::SlotPath(std::u8string_view saveDirectory, int slot)
{
    return PathFormat(SaveGames::NameFormat, {saveDirectory, slot});
}

// swd3e.exe 0x00409AC0: whether any slot opens for reading and writing
// swd3e.exe EH funclet (tail) of 0x00409AC0, swd32 0x00498090 (EH unwind: the cFile, closed by its destructor)
// swd3e.exe EH funclet (tail) of 0x00409AC0, swd32 0x00498098 (EH handler of 0x00409C10)
bool SaveGames::AnyExists(std::u8string_view saveDirectory)
{
    File file;
    int found = 0;

    for (int slot = 0; slot <= SaveGames::LastSlot; ++slot)
    {
        if (file.Open(
                SaveGames::SlotPath(saveDirectory, slot),
                File::Disposition::OpenExisting,
                File::Access::ReadWrite
            ))
        {
            ++found;
        }

        file.Close();
    }

    return found != 0;
}

// swd3e.exe 0x00409320: whether save `slot` does not open for reading and writing (a free slot)
// swd3e.exe EH funclet (tail) of 0x00409320, swd32 0x00498030 (EH unwind: the cFile, closed by its destructor)
// swd3e.exe EH funclet (tail) of 0x00409320, swd32 0x00498038 (EH handler of 0x004094A0)
bool SaveGames::IsFree(std::u8string_view saveDirectory, int slot)
{
    File file;
    const bool free =
        !file.Open(SaveGames::SlotPath(saveDirectory, slot), File::Disposition::OpenExisting, File::Access::ReadWrite);
    file.Close();
    return free;
}

}  // namespace swd3e
