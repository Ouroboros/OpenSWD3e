#include "game/cheat_dialog.h"

#include <windows.h>

#include <commctrl.h>

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "core/error_box.h"
#include "core/format.h"
#include "core/log.h"
#include "game/game.h"
#include "item/item_list.h"
#include "menu/menu_list.h"
#include "org/object_record.h"
#include "party/party.h"
#include "platform/exe_resources.h"
#include "talk/talk.h"
#include "text/big5.h"

namespace swd3e
{

namespace
{

// The dialogs of swd3e.exe's resources.
constexpr WORD kCheatDialogId = 0x6A;

// The controls of the cheat dialog.
constexpr int kCloseButton = 0x3E8;
constexpr int kTopUpButton = 0x3EB;
constexpr int kAddButton = 0x3EC;
constexpr int kIdField = 0x3ED;
constexpr int kDeleteButton = 0x3EE;
constexpr int kTabControl = 0x3F1;
constexpr int kValueField = 0x3F4;
constexpr int kCountField = 0x3F5;
constexpr int kSetButton = 0x3F6;
constexpr int kList = 0x3F9;
constexpr std::array<int, 4> kEntryButtons = {kValueField, kTopUpButton, kDeleteButton, kAddButton};
constexpr int kFieldBytes = 0xA;  // GetDlgItemTextA reads at most nine bytes
constexpr std::array<int, 4> kColumnWidths = {0x78, 0x40, 0x40, 0x60};
constexpr int kColumnTextSize = 4;

// The name dialog.

std::wstring Wide(std::string_view big5)
{
    const std::string text = Utf8FromBig5(big5);
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), length);
    return wide;
}

std::wstring Wide(std::u8string_view text)
{
    return Wide(Big5(text));
}

// GetDlgItemTextA(`control`, `bytes`): the text of a field in Big5, at most `bytes` - 1 bytes; false when empty.
bool FieldText(HWND dialog, int control, int bytes, std::string& text)
{
    std::array<wchar_t, 0x100> wide = {};
    const int length = GetDlgItemTextW(dialog, control, wide.data(), static_cast<int>(wide.size()));
    const int size = WideCharToMultiByte(CP_UTF8, 0, wide.data(), length, nullptr, 0, nullptr, nullptr);
    std::string utf8(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), length, utf8.data(), size, nullptr, nullptr);
    text = Big5FromUtf8(utf8);
    text.resize(std::min(text.size(), static_cast<std::size_t>(bytes - 1)));
    return !text.empty();
}

// The three fields of the dialog (FieldText each; empty when empty).
CheatDialog::Fields DialogFields(HWND dialog)
{
    CheatDialog::Fields fields;
    FieldText(dialog, kIdField, kFieldBytes, fields.id);
    FieldText(dialog, kCountField, kFieldBytes, fields.count);
    FieldText(dialog, kValueField, kFieldBytes, fields.value);
    return fields;
}

void SetItemText(HWND list, int row, int column, const std::string& big5)
{
    std::wstring text = Wide(big5);
    // mask, item, sub item, state, state mask, text (positional: the Win32 field names do not follow the style)
    LVITEMW item{0, 0, column, 0, 0, text.data()};
    SendMessageW(list, LVM_SETITEMTEXTW, static_cast<WPARAM>(row), reinterpret_cast<LPARAM>(&item));
}

INT_PTR CALLBACK CheatProcedure(HWND dialog, UINT message, WPARAM word, LPARAM data);

// The window of the game, the owner of the dialogs.
HWND GameWindow(const CheatDialog& cheat)
{
    SDL_Window* window = cheat.Window();

    if (window == nullptr)
    {
        return nullptr;
    }

    return static_cast<HWND>(
        SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr)
    );
}

// The dialog `id` of swd3e.exe's resources run with `procedure` and `context`; 0 when it cannot be.
int RunDialog(CheatDialog& cheat, WORD id, DLGPROC procedure)
{
    const std::vector<u8> stored = cheat.resources.ReadDialog(id);

    if (stored.empty())
    {
        LogWarning(u8"dialog 0x%X: %d.dlg missing from the res directory", static_cast<i32>(id), static_cast<i32>(id));
        return 0;
    }

    // A dialog template is read in DWORDs.
    std::vector<DWORD> dialog((stored.size() + sizeof(DWORD) - 1) / sizeof(DWORD));
    std::memcpy(dialog.data(), stored.data(), stored.size());
    INITCOMMONCONTROLSEX controls{sizeof(INITCOMMONCONTROLSEX), ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES};
    InitCommonControlsEx(&controls);
    const INT_PTR result = DialogBoxIndirectParamW(
        GetModuleHandleW(nullptr),
        reinterpret_cast<const DLGTEMPLATE*>(dialog.data()),
        GameWindow(cheat),
        procedure,
        reinterpret_cast<LPARAM>(&cheat)
    );
    LogInfo(u8"dialog 0x%X result=%d", static_cast<i32>(id), static_cast<i32>(result));
    return result < 0 ? 0 : static_cast<int>(result);
}

}  // namespace

int CheatDialog::Run()
{
    return RunDialog(*this, kCheatDialogId, &CheatProcedure);
}

namespace
{

// swd3e.exe 0x00410710: the columns of the list (the value column only on the lists of objects)
void AddColumns(const CheatDialog& cheat, HWND list)
{
    const int columns = cheat.ColumnCount();

    for (int column = 0; column < columns; ++column)
    {
        std::wstring text = Wide(CheatDialog::ColumnLabels[static_cast<std::size_t>(column)]);
        // mask, format, width, text, text size, sub item
        LVCOLUMNW entry{
            LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM,
            LVCFMT_LEFT,
            kColumnWidths[static_cast<std::size_t>(column)],
            text.data(),
            kColumnTextSize,
            column
        };

        SendMessageW(list, LVM_INSERTCOLUMNW, static_cast<WPARAM>(column), reinterpret_cast<LPARAM>(&entry));
    }
}

// swd3e.exe 0x00410950: row `row` written: the name, the count, the id and the value (CheatDialog::ValueText)
void SetRow(HWND list, int row, std::string_view name, int count, int id, i32 value, i32 maximum)
{
    SetItemText(list, row, 0, std::string(name));
    SetItemText(list, row, 1, WinFormat("%d", {count}));
    const std::string idText = WinFormat("%d", {id});
    SetItemText(list, row, 2, idText);
    SetItemText(list, row, 3, CheatDialog::ValueText(value, maximum, idText));
}

// swd3e.exe 0x004107E0: row `row` inserted, then written as SetRow does
void InsertRow(HWND list, int row, std::string_view name, int count, int id, i32 value, i32 maximum)
{
    LVITEMW item{};
    item.mask = LVIF_TEXT | LVIF_STATE | LVIF_PARAM;
    item.iItem = row;
    SendMessageW(list, LVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&item));
    SetRow(list, row, name, count, id, value, maximum);
}

// swd3e.exe 0x00410A80: the list of the tab: the objects (with their counters on the items tab) or skills, the
// seventeen attributes of the role, or the 64 variables (the rows of CheatDialog::Rows)
void Fill(CheatDialog& cheat, HWND list)
{
    SendMessageW(list, LVM_DELETEALLITEMS, 0, 0);
    int index = 0;

    for (const CheatDialog::Row& row : cheat.Rows())
    {
        InsertRow(list, index, row.name, row.count, row.id, row.value, row.maximum);
        ++index;
    }
}

// The fields cleared after an entry changed (part of 0x0040FBE0)
void ClearFields(HWND dialog)
{
    for (const int field : {kIdField, kCountField, kValueField})
    {
        SetDlgItemTextW(dialog, field, L"");
    }
}

// swd3e.exe 0x0040FBE0: the cheat dialog. A tab chosen shows its list (the buttons off on the attributes and
// variables); the buttons: close (the game runs again), top up the row chosen to 90 (items only), add the id of the
// field with its count and value (a skill tab takes skill ids, one, no value), delete the row chosen, set the count and
// value of the id (always looked up in the items), an attribute or a variable.
INT_PTR CALLBACK CheatProcedure(HWND dialog, UINT message, WPARAM word, LPARAM data)
{
    if (message == WM_INITDIALOG)
    {
        SetWindowLongPtrW(dialog, DWLP_USER, data);
    }

    auto* cheat = reinterpret_cast<CheatDialog*>(GetWindowLongPtrW(dialog, DWLP_USER));

    if (cheat == nullptr)
    {
        return FALSE;
    }

    HWND list = GetDlgItem(dialog, kList);

    switch (message)
    {
        case WM_NOTIFY:
        {
            const auto* header = reinterpret_cast<const NMHDR*>(data);

            if (header->code != static_cast<UINT>(TCN_SELCHANGE))
            {
                return FALSE;
            }

            cheat->tab = static_cast<int>(SendMessageW(header->hwndFrom, TCM_GETCURSEL, 0, 0));

            for (int column = 3; column >= 0; --column)
            {
                SendMessageW(list, LVM_DELETECOLUMN, static_cast<WPARAM>(column), 0);
            }

            AddColumns(*cheat, list);
            Fill(*cheat, list);

            for (const int button : kEntryButtons)
            {
                EnableWindow(GetDlgItem(dialog, button), cheat->EntriesEnabled() ? TRUE : FALSE);
            }

            return FALSE;
        }

        case WM_INITDIALOG:
        {
            HWND tabs = GetDlgItem(dialog, kTabControl);

            for (int index = 0; index < CheatDialog::Tabs; ++index)
            {
                std::wstring text = Wide(cheat->TabLabel(index));
                // mask, state, state mask, text
                TCITEMW item{TCIF_TEXT, 0, 0, text.data()};
                SendMessageW(tabs, TCM_INSERTITEMW, static_cast<WPARAM>(index), reinterpret_cast<LPARAM>(&item));
            }

            cheat->tab = CheatDialog::ItemsTab;
            AddColumns(*cheat, list);
            SendMessageW(list, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT);
            Fill(*cheat, list);
            SendMessageW(GetDlgItem(dialog, kIdField), EM_LIMITTEXT, CheatDialog::IdLimit, 0);
            SendMessageW(GetDlgItem(dialog, kCountField), EM_LIMITTEXT, CheatDialog::CountLimit, 0);
            SendMessageW(GetDlgItem(dialog, kValueField), EM_LIMITTEXT, CheatDialog::ValueLimit, 0);
            return TRUE;
        }

        case WM_COMMAND:
            break;

        default:
            return FALSE;
    }

    switch (LOWORD(word))
    {
        case kCloseButton:
            cheat->running = 1;
            EndDialog(dialog, 1);
            SDL_HideCursor();
            return FALSE;

        case kTopUpButton:
        {
            const auto row = static_cast<int>(SendMessageW(list, LVM_GETSELECTIONMARK, 0, 0));
            CheatDialog::Row shown;

            if (cheat->TopUp(row, shown) == CheatDialog::EditResult::Applied)
            {
                SetRow(list, row, shown.name, shown.count, shown.id, shown.value, shown.maximum);
            }

            return FALSE;
        }

        case kAddButton:
        {
            const CheatDialog::EditResult result = cheat->Add(DialogFields(dialog));

            if (result == CheatDialog::EditResult::Ignored)
            {
                return FALSE;
            }

            if (result == CheatDialog::EditResult::InsertFailed)
            {
                ErrorBox(cheat->errors, u8"CheatDialog", CheatDialog::InsertFailedText);
            }

            Fill(*cheat, list);
            ClearFields(dialog);
            return FALSE;
        }

        case kDeleteButton:
        {
            const auto row = static_cast<int>(SendMessageW(list, LVM_GETSELECTIONMARK, 0, 0));
            const CheatDialog::EditResult result = cheat->Delete(row);

            if (result == CheatDialog::EditResult::Ignored)
            {
                return FALSE;
            }

            if (result == CheatDialog::EditResult::DeleteFailed)
            {
                ErrorBox(cheat->errors, u8"CheatDialog", CheatDialog::DeleteFailedText);
            }

            Fill(*cheat, list);
            return FALSE;
        }

        case kSetButton:
        {
            if (cheat->Set(DialogFields(dialog)) == CheatDialog::EditResult::Ignored)
            {
                return FALSE;
            }

            Fill(*cheat, list);
            ClearFields(dialog);
            return FALSE;
        }

        default:
            return FALSE;
    }
}

}  // namespace

}  // namespace swd3e
