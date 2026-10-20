#include "game/cheat_dialog.h"

#include "core/log.h"

namespace swd3e
{

// The dialogs of swd32.exe's resources are Win32 dialogs; none can be shown here (the game runs on).
int CheatDialog::Run()
{
    LogWarning(u8"the cheat dialog is only available on Windows");
    return 0;
}

}  // namespace swd3e
