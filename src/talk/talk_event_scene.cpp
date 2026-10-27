#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

#include "core/debug_log.h"
#include "core/error_box.h"
#include "core/fault.h"
#include "core/format.h"
#include "file/file.h"
#include "game/direction.h"
#include "game/game.h"
#include "platform/display.h"
#include "platform/system_info.h"
#include "talk/script_text.h"
#include "talk/talk.h"
#include "talk/talk_event.h"

namespace swd3e
{

namespace
{

constexpr u16 kListEnd = 0xFF00;
constexpr double kFadeLimit = 31.0;  // FadeColors (swd3e): the levels and targets are kept within these
constexpr i32 kFadeTargetLimit = 0x1F;
constexpr std::u8string_view kCaptionTitle = u8"對話系統(Caption2)";  // 0x004B26F0
constexpr u16 kPlaceAtTarget = 0x8000;  // in the y of HeadIn: the head is placed at its target
constexpr u16 kEvenPixels = 0xFFFE;     // the line effects keep to even pixels

// The nodes of a list for which `match` holds are freed (`first`: only the first of them).
template <typename Node, typename Match>
bool EraseNodes(std::unique_ptr<Node>& head, Match match, bool first)
{
    bool erased = false;
    std::unique_ptr<Node>* link = &head;

    while (*link != nullptr)
    {
        if (match(**link))
        {
            std::unique_ptr<Node> gone = std::move(*link);
            *link = std::move(gone->next);
            erased = true;

            if (first)
            {
                return true;
            }

            continue;
        }

        link = &(*link)->next;
    }

    return erased;
}

}  // namespace

// Commands 0x32, 0x46 and 0x49: the camera pans by tiles (0x32), to a tile (0x46) or to a role (0x49) by steps of
// whole pixels that must divide the distance (otherwise 4), kept inside the map.
bool Talk::PanCamera(EventRun& run)
{
    Game& owner = this->game;
    GameState& state = owner.state;
    Scene& view = owner.scene;
    const Field& field = owner.field;
    constexpr i32 kDefaultPanStep = 4;  // a step that does not divide the distance becomes this
    i32 across = view.panAcross;
    i32 down = view.panDown;
    Operands operands = this->OperandsOf(run);

    if ((across | down) != 0)
    {
        ErrorBox(this->game.errors, u8"對話系統(Scroll_map)", u8"前一個捲動指令還沒有結束");  // 0x004B2A5C, 0x004B2A74
    }

    if (run.code == EventCommand::PanCamera)
    {
        across = static_cast<i16>(operands.Word());
        view.panAcross = across;
        down = static_cast<i16>(operands.Word());
        view.panDown = down;
    }
    else if (run.code == EventCommand::PanCameraTo)
    {
        // The tiles are those of the 640x480 screen's corner (the rebuild's wider view keeps it in the middle).
        run.valueA = static_cast<i16>(operands.Word());
        across = run.valueA - ((field.cameraLeft + field.screenX) >> MapHeader::TileShift);
        view.panAcross = across;
        down = static_cast<i16>(operands.Word()) - ((field.cameraTop + field.screenY) >> MapHeader::TileShift);
        view.panDown = down;
    }
    else
    {
        state.FindRoleOrPlayer(operands.Word(), run.found);
        const Role* role = this->FoundRole(run);
        CameraRect rect = {field.cameraLeft, field.cameraTop, field.cameraRight, field.cameraBottom};
        field.CameraAround(role != nullptr ? role->x : 0, role != nullptr ? role->y : 0, rect);
        across = (rect.left - field.cameraLeft) >> MapHeader::TileShift;
        down = (rect.top - field.cameraTop) >> MapHeader::TileShift;
        view.panAcross = across;
        view.panDown = down;
    }

    run.valueB = static_cast<i32>(operands.Index());  // the operand the steps start at
    i32 stepX = operands.Word();
    i32 stepY = operands.Word();
    view.panStepX = stepX;
    view.panStepY = stepY;

    if (across == 0)
    {
        stepX = 0;
        view.panStepX = 0;
    }

    if (down == 0)
    {
        stepY = 0;
        view.panStepY = 0;
    }

    i32 pixelsDown = MapHeader::TileSize * down;
    view.panAcross = MapHeader::TileSize * across;
    view.panDown = pixelsDown;

    if (view.panAcross != 0 && stepX == 0)
    {
        Fault(owner.errors, u8"Integer divide by zero (0x0042D482 pan)");
    }

    if (view.panAcross != 0 && view.panAcross % stepX != 0)
    {
        ErrorBox(
            this->game.errors,
            u8"對話系統(Scroll_map)",
            u8"捲動間距請設為二的次方倍(X..%d)",
            stepX
        );  // 0x004B2A5C, 0x004B2A3C
        pixelsDown = view.panDown;
        stepY = view.panStepY;
        stepX = kDefaultPanStep;
        view.panStepX = kDefaultPanStep;
    }

    if (pixelsDown != 0 && stepY == 0)
    {
        Fault(owner.errors, u8"Integer divide by zero (0x0042D4C0 pan)");
    }

    if (pixelsDown != 0 && pixelsDown % stepY != 0)
    {
        ErrorBox(
            this->game.errors,
            u8"對話系統(Scroll_map)",
            u8"捲動間距請設為二的次方倍(Y..%d)",
            stepY
        );  // 0x004B2A5C, 0x004B2A1C
        pixelsDown = view.panDown;
        stepX = view.panStepX;
        stepY = kDefaultPanStep;
        view.panStepY = kDefaultPanStep;
    }

    i32 pixelsAcross = view.panAcross;

    if (view.panAcross < 0)
    {
        view.panStepX = -stepX;
    }

    if (pixelsDown < 0)
    {
        view.panStepY = -stepY;
    }

    bool clamped = false;

    if (view.panAcross + field.cameraLeft < 0)
    {
        clamped = true;
        pixelsAcross = -field.cameraLeft;
        view.panAcross = -field.cameraLeft;
    }

    if (field.cameraRight + pixelsAcross > MapHeader::TileSize * state.map.width)
    {
        clamped = true;
        view.panAcross = MapHeader::TileSize * state.map.width - field.cameraRight;
    }

    if (pixelsDown + field.cameraTop < 0)
    {
        clamped = true;
        pixelsDown = -field.cameraTop;
        view.panDown = -field.cameraTop;
    }

    if (field.cameraBottom + pixelsDown > MapHeader::TileSize * state.map.height)
    {
        view.panDown = MapHeader::TileSize * state.map.height - field.cameraBottom;
        clamped = true;
    }

    // The rebuild (deviations.md): the wide view holds a map smaller than it in its middle, the camera before the map's
    // start and past its end; the limits above, made for a camera inside the map, would turn the pan against its step
    // and it would never end. On such an axis the camera does not pan.
    const std::array<bool, 2> inside = field.MapInsideView();

    if (inside[0])
    {
        view.panAcross = 0;
        view.panStepX = 0;
    }

    if (inside[1])
    {
        view.panDown = 0;
        view.panStepY = 0;
    }

    if (clamped)
    {
        // 0x0042D5B2: the four operand words as they stand (sign-extended) and the camera in tiles
        ErrorBox(
            this->game.errors,
            u8"對話系統(Scroll_map)",
            u8"捲動將超出螢幕範圍\n(捲動值) L%d.T%d.R%d.B%d\n(目前螢幕位置) L%d.T%d.R%d.B%d",
            static_cast<i16>(this->Word(run, kFirstOperand)),
            static_cast<i16>(this->Word(run, kFirstOperand + 1)),
            static_cast<i16>(this->Word(run, kFirstOperand + 2)),
            static_cast<i16>(this->Word(run, kFirstOperand + 3)),
            field.cameraLeft >> MapHeader::TileShift,
            field.cameraTop >> MapHeader::TileShift,
            field.cameraRight >> MapHeader::TileShift,
            field.cameraBottom >> MapHeader::TileShift
        );  // 0x004B2A5C, 0x004B29D0
    }

    const int length = CommandBytes(run.code);
    run.valueB = length;
    this->Advance(run, length);
    return true;
}

// Command 0x3E: role `id` put on a map (its record's act, place, event and talk set; 0xFFFF keeps the map or takes
// the controlled role's tile). A role on the map leaves it first (its moves freed); a role put on the current map is
// loaded from maps.dat and placed (replacing a role of the same id), and a role with flag 0x200 takes the empty
// bubble marks. The command number is left 0xFFFF (it becomes the last command, 0x0042B0AE); every way out
// advances 0x12 bytes whatever it holds (0x004299CA add ebx, 12h; 0x004299CD add word ptr [ebp+0], 12h).
bool Talk::PlaceRole(EventRun& run)
{
    Game& owner = this->game;
    GameState& state = owner.state;
    const Role& player = state.roles[static_cast<std::size_t>(state.playerRole)];
    Operands operands = this->OperandsOf(run);
    const u16 operand = operands.Word();
    const u16 mapOperand = operands.Word();
    const auto talk = static_cast<i16>(operands.Word());
    const u16 column = operands.Word();
    const u16 row = operands.Word();
    const auto act = static_cast<i16>(operands.Word());
    const auto action = static_cast<i16>(operands.Word());
    const auto direction = static_cast<i16>(operands.Word());
    const u16 id = this->EventRoleId(run, operand);
    i16 setBits = 0;
    i16 event = -1;

    if (state.FindRoleOrPlayer(id, run.found))
    {
        for (MoveRecord& move : state.moves)
        {
            if (move.role == run.found)
            {
                move.Clear();
            }
        }

        Role& role = state.roles[run.found];
        setBits = static_cast<i16>(role.flags);
        role.flags &= ~(Role::Visible | Role::Solid);
        owner.field.ClearRoleCells(role, owner.partyLeaderId);
        role.flags |= Role::Removed;
        event = static_cast<i16>(role.event);
        // swd3e also takes the role off its line of the map (0x00411830).
        owner.field.UnlinkRole(role, static_cast<int>(role.flags & Role::LayerMask));
    }

    run.code = EventCommand::None;
    const i16 map = mapOperand == Talk::Keep ? static_cast<i16>(owner.currentMap) : static_cast<i16>(mapOperand);
    const i16 tileY = row == Talk::Keep ? static_cast<i16>(static_cast<u32>(player.y) >> MapHeader::TileShift)
                                        : static_cast<i16>(row);
    const i16 tileX = column == Talk::Keep ? static_cast<i16>(static_cast<u32>(player.x) >> MapHeader::TileShift)
                                           : static_cast<i16>(column);

    if (!owner.UpdateRoleRecord(id, act, action, direction, tileX, tileY, event, talk, setBits, Game::KeepValue, map))
    {
        // 0x0042DC22: the error box, then the text formatted again and logged alone
        ErrorBox(this->game.errors, u8"!!! 警告 !!!", u8"Maps沒有定義角色[%d]", operand);  // 0x004B0FE0, 0x004B2988
        DebugLog(u8"Maps沒有定義角色[%d]", operand);
        this->Advance(run, CommandBytes(EventCommand::PlaceRoleOnMap));
        return true;
    }

    const u16 target = mapOperand == Talk::Keep ? static_cast<u16>(owner.currentMap) : mapOperand;

    if (static_cast<u16>(owner.currentMap) == target)
    {
        Role loaded;
        owner.LoadRoleRecord(loaded, operand);
        owner.InitRole(loaded);
        Role* placed = nullptr;
        int index = 1;

        while (index < state.roleCount && state.roles[static_cast<std::size_t>(index)].id != loaded.id)
        {
            ++index;
        }

        if (index >= state.roleCount)
        {
            if (state.roleCount < GameState::Roles)
            {
                placed = &state.roles[static_cast<std::size_t>(state.roleCount)];
                *placed = loaded;
                owner.field.InsertRole(*placed, static_cast<int>(placed->flags & Role::LayerMask));
                ++state.roleCount;
            }
        }
        else
        {
            placed = owner.field.RemoveRole(loaded.id, static_cast<int>(loaded.flags & Role::LayerMask), 0, 1);

            if (placed == nullptr)
            {
                placed = &state.roles[static_cast<std::size_t>(index)];
            }

            *placed = loaded;
            owner.field.InsertRole(*placed, static_cast<int>(placed->flags & Role::LayerMask));
        }

        if (placed != nullptr && (placed->flags & Role::Bubbles) != 0)
        {
            // The loop's count (0x0042DD30) lies where the later commands read valueA.
            run.valueA = static_cast<i32>(owner.scene.marks.size());

            for (RoleMark& mark : owner.scene.marks)
            {
                if (mark.role == 0)
                {
                    mark.role = placed->id;
                    mark.x = static_cast<u16>(placed->x);
                    mark.y = static_cast<u16>(placed->y);
                    mark.unk08 = 0;
                    mark.flags = 0;
                    mark.unk0E = 0;
                }
            }
        }
    }

    this->Advance(run, CommandBytes(EventCommand::PlaceRoleOnMap));
    return true;
}

// swd3e.exe 0x004120A0: name plate `index` of namex.dat (its offset at 0x200 + 4 * index, the text after the 0x200
// header) read into `plate` and cut at its "%Q"; the length up to there. The file is opened as OPEN_ALWAYS (a missing
// one is created empty and the plate keeps its bytes). A plate without "%Q" has the original write to address 0;
// here it stays as read.
int Talk::ReadNamePlate(u32 index, std::span<char> plate)
{
    constexpr i32 kHeaderBytes = 0x200;
    constexpr std::string_view kPlateEnd = "%Q";  // 0x004B2720
    const std::u8string path = PathFormat("%snamex.dat", {this->game.config.installDirectory});
    File file;
    file.Open(path, File::Disposition::OpenAlways, File::Access::Read);
    u32 bytes = sizeof(u32);
    u32 offset = 0;
    file.SeekBegin(static_cast<i32>(sizeof(u32) * index) + kHeaderBytes);
    file.Read(&offset, bytes);
    bytes = static_cast<u32>(plate.size());
    file.SeekBegin(static_cast<i32>(offset) + kHeaderBytes);
    file.Read(plate.data(), bytes);
    file.Close();
    const std::string_view text(plate.data(), strnlen(plate.data(), plate.size()));
    const std::size_t end = text.find(kPlateEnd);

    if (end == std::string_view::npos)
    {
        return static_cast<int>(text.size());
    }

    plate[end] = '\0';
    return static_cast<int>(end);
}

// Commands 0xA2 and 0x5B: the name plate of the next talk window from namex.dat (0xA2: by script variable 0xB or
// 0xC, which must be set; 0x5B: by role), its names put in by Talk::ExpandText.
bool Talk::LoadName(EventRun& run)
{
    Game& owner = this->game;
    u32 index = 0;

    if (run.code == EventCommand::RoleNamePlate)
    {
        index = this->EventRoleId(run, this->OperandsOf(run).Word());
        run.found = index;
    }
    else
    {
        const u16 variable = this->OperandsOf(run).Word();
        run.found = variable;

        if (variable != Party::FirstNameVariable && variable != Party::SecondNameVariable)
        {
            ErrorBox(owner.errors, kCaptionTitle, u8"設定的變數(%d)和系統衝突", variable);  // 0x004B2704
            this->Next(run);
            return true;
        }

        index = static_cast<u32>(owner.party.variables[variable]);
        run.found = index;

        if (index == 0)
        {
            ErrorBox(owner.errors, kCaptionTitle, u8"設定的變數(0)和系統衝突");  // 0x004B26D8
            this->Next(run);
            return true;
        }
    }

    run.valueA = this->ReadNamePlate(index, this->dialogName);
    // The original expands in place, the text and its moves reaching past the 0x20 bytes of the plate; here the
    // expansion is made on a copy and its first 0x20 bytes kept.
    std::string name(this->dialogName.data(), this->dialogName.size());
    this->ExpandText(name);
    std::fill(this->dialogName.begin(), this->dialogName.end(), '\0');
    std::copy_n(name.begin(), std::min(name.size(), this->dialogName.size()), this->dialogName.begin());
    this->Next(run);
    return true;
}

// Command 0x60: a map movie from Video\ of the installation (the original falls back to the CD): "%" plays
// it without the fade in, "*" fades it out darkening. Its header gives the frames, the first chunk and the palettes
// (an 8-bit movie: the frames they change at and the palettes from the file's end); the first frame is read and the
// main loop slows to 70 ms a frame.
bool Talk::OpenMapMovie(EventRun& run)
{
    Game& owner = this->game;
    GameState& state = owner.state;
    MapMovie& film = owner.fieldView.movie;
    const std::vector<u8>& script = owner.databases.script;
    constexpr int kMovieFrameInterval = 0x46;  // milliseconds a frame while a map movie plays
    // swd3e closes HUGE.lmf while the movie plays; command 0x61 opens it again.
    owner.mapCache.lmf.Close();
    state.ClearFrameInterval();
    state.SetFrameInterval(kMovieFrameInterval);
    film.options = 0;
    this->Advance(run, kOpcodeBytes);

    if (this->Byte(run, 0) == '%')
    {
        ++run.cursor;
        film.options |= MapMovie::NoFadeIn;
        ++run.context.eventCursor;
    }

    if (this->Byte(run, 0) == '*')
    {
        ++run.cursor;
        film.options |= MapMovie::DarkenOut;
        ++run.context.eventCursor;
    }

    constexpr std::size_t kNameBytes = 0x20;  // copied from the script; the copy is 0x400 bytes
    constexpr std::size_t kCopyBytes = 0x400;
    std::array<u8, kCopyBytes> copy = {};

    for (std::size_t index = 0; index < kNameBytes; ++index)
    {
        copy[index] = run.cursor + index < script.size() ? script[run.cursor + index] : u8{0};
    }

    std::size_t end = 0;

    while (end + 1 < copy.size() && TextWord(copy, end) != kTextEnd)
    {
        ++run.context.eventCursor;
        ++end;
        ++run.cursor;
    }

    run.cursor += kWordBytes;
    run.context.eventCursor = static_cast<u16>(run.context.eventCursor + kWordBytes);
    copy[end] = 0;
    const std::string name(reinterpret_cast<const char*>(copy.data()), end);
    // By the installation type the original reads the installation's Video\, the CD's swd3e\Video\ (after a CD
    // check) or the name alone; the rebuild reads the installation's (see deviations.md).
    const std::u8string path = PathFormat("%sVideo\\%s", {owner.config.installDirectory, name});
    ErrorLog(u8"Talk:Ani_Open::%s", path);

    if (!film.file.Open(path, File::Disposition::OpenExisting, File::Access::Read))
    {
        ErrorBox(this->game.errors, u8"MAni Open Failure!", u8"MAni File Error!\n%s", name);  // 0x004B27BC, 0x004B27D0
        return false;
    }

    film.chunks.next.reset();
    constexpr u32 kReadRetryWait = 0xFA;  // milliseconds
    std::array<u8, MovieHeader::Bytes> bytes = {};
    u32 read = static_cast<u32>(bytes.size());
    film.file.Read(bytes.data(), read);

    if (read != bytes.size())
    {
        ErrorLog(u8"Talk:Ani_Open Header read fail ,%s", film.file.ErrorText());
        SleepMilliseconds(kReadRetryWait);
    }

    const MovieHeader header = MovieHeader::Read(bytes);
    MovieChunk& head = film.chunks;
    head.offset = 0;
    head.unk0C = MovieHeader::FirstChunkBytes;
    head.rows = 1;
    head.size = MovieHeader::FirstChunkBytes;
    head.nextUnk0C = header.nextUnk0C;
    head.nextRows = header.nextRows;
    head.nextSize = header.nextSize;
    head.frame = 0;
    head.next.reset();
    film.lastFrame = static_cast<i32>(header.lastFrame);
    film.active = header.active;
    film.height = header.height;
    film.frame.assign(MapMovie::FrameBufferBytes, 0);
    film.buffer.assign(MapMovie::FrameBufferBytes, 0);
    film.palette.clear();
    film.paletteFrames.clear();
    film.paletteCount = header.paletteCount;
    film.paletteIndex = 0;

    if (header.bitsPerPixel == MapMovie::IndexedBits)
    {
        if (film.paletteCount != 0)
        {
            film.paletteFrames.assign(MapMovie::PaletteTailBytes / sizeof(u16), 0);
            u32 frames = MapMovie::PaletteTailBytes;
            film.file.SeekEnd(-static_cast<i32>(MapMovie::PaletteTailBytes));
            film.file.Read(film.paletteFrames.data(), frames);
        }

        auto back = static_cast<i32>(MapMovie::PaletteBytes);

        if (film.paletteCount != 0)
        {
            back = static_cast<i32>(
                MapMovie::PaletteBytes * (film.paletteCount - film.paletteIndex++) + MapMovie::PaletteTailBytes
            );
        }

        std::array<u8, MapMovie::PaletteBytes> colors = {};
        film.file.SeekEnd(-back);
        u32 colorBytes = MapMovie::PaletteBytes;
        film.file.Read(colors.data(), colorBytes);
        film.palette.assign(MapMovie::PaletteColors, 0);
        // The conversion loop's count (0x0042F023) lies where the later commands read valueA.
        run.valueA = static_cast<i32>(MapMovie::PaletteColors);

        for (std::size_t index = 0; index < MapMovie::PaletteColors; ++index)
        {
            const u8* color = colors.data() + MapMovie::PaletteBytes / MapMovie::PaletteColors * index;
            film.palette[index] =
                MapMovie::Rgb555(color[MapMovie::RedAt], color[MapMovie::GreenAt], color[MapMovie::BlueAt]);
        }

        owner.painter.ConvertPixels(film.palette);
    }

    film.frameIndex = 1;
    owner.fieldView.ReadMovieFrame();
    film.frameIndex = MapMovie::FadeInStart;

    if ((film.options & MapMovie::NoFadeIn) != 0)
    {
        film.frameIndex = 1;
        owner.ShowFrame();
    }

    film.saved = 0;
    owner.systemFlags |= Game::MovieOpened;
    return false;
}

// Commands 0x32..0x62: the camera, the colour fade, the roles' acts, layers and followers, the quake, the sprites and
// line effects over the screen, the movies and the battles.
bool Talk::RunSceneCommand(EventRun& run)
{
    Game& owner = this->game;
    GameState& state = owner.state;
    Scene& view = owner.scene;
    const Field& field = owner.field;
    const Role blank;

    switch (run.code)
    {
        case EventCommand::PanCamera:
        case EventCommand::PanCameraTo:
        case EventCommand::PanCameraToRole:
            return this->PanCamera(run);

        case EventCommand::WaitPan:
            if (view.panAcross == 0 && view.panDown == 0 && view.panStepX == 0 && view.panStepY == 0)
            {
                this->Next(run);
                return true;
            }

            return false;

        case EventCommand::FadeColors:
        {
            ColorFade& fade = view.fade;
            Operands operands = this->OperandsOf(run);

            // swd3e keeps both the levels and the targets within -31..31.
            auto level = [](i16 value)
            {
                const auto shade = static_cast<double>(value);
                return static_cast<float>(std::clamp(shade, -kFadeLimit, kFadeLimit));
            };

            auto target = [](i16 value)
            {
                return std::clamp(static_cast<i32>(value), -kFadeTargetLimit, kFadeTargetLimit);
            };

            fade.red = level(static_cast<i16>(operands.Word()));
            fade.green = level(static_cast<i16>(operands.Word()));
            fade.blue = level(static_cast<i16>(operands.Word()));
            const i32 red = target(static_cast<i16>(operands.Word()));
            const i32 green = target(static_cast<i16>(operands.Word()));
            const i32 blue = target(static_cast<i16>(operands.Word()));
            run.valueA = red;
            run.valueB = green;
            fade.startRed = static_cast<float>(red);
            fade.startGreen = static_cast<float>(green);
            fade.startBlue = static_cast<float>(blue);
            fade.frames = operands.Word();
            const auto frames = static_cast<double>(fade.frames);
            fade.stepRed =
                static_cast<float>(static_cast<double>(red - static_cast<i32>(crt::Ftol(fade.red))) / frames);
            fade.stepGreen =
                static_cast<float>(static_cast<double>(green - static_cast<i32>(crt::Ftol(fade.green))) / frames);
            fade.stepBlue =
                static_cast<float>(static_cast<double>(blue - static_cast<i32>(crt::Ftol(fade.blue))) / frames);
            this->Next(run);
            return true;
        }

        case EventCommand::WaitFade:
            if (view.fade.frames <= 0)
            {
                this->Next(run);
                return true;
            }

            return false;

        case EventCommand::SeekAct:
        {
            Operands operands = this->OperandsOf(run);
            const u16 id = this->EventRoleId(run, operands.Word());
            const auto steps = static_cast<i16>(operands.Word());
            state.FindRoleOrPlayer(id, run.found);
            run.valueB = steps;
            run.valueA = 0;
            Role* role = this->FoundRole(run);

            if (role != nullptr)
            {
                role->act.delay = 0;
                role->act.position = 0;

                if (owner.acts.Run(role->act) == 0)
                {
                    ReportActError(owner.errors, u8"Act Err(Talk:SeekAct1)", *role);
                }
            }

            // The count is left behind for the commands after it even when the role is missing (the original steps
            // role -1 as often).
            for (int step = 0; step < run.valueB; ++step)
            {
                if (role != nullptr)
                {
                    role->act.delay = 0;

                    if (owner.acts.Run(role->act) == 0)
                    {
                        ReportActError(owner.errors, u8"Act Err(Talk:SeekAct2)", *role);
                    }

                    role->act.view = 0;
                }

                run.valueA = step + 1;
            }

            this->Next(run);
            return true;
        }

        case EventCommand::LayerFront:
        case EventCommand::LayerMiddle:
        case EventCommand::LayerBack:
        {
            Operands operands = this->OperandsOf(run);
            const u16 id = this->EventRoleId(run, operands.Word());
            state.FindRoleOrPlayer(id, run.found);
            run.valueA = 0;  // the layer of a missing role (role -1, read as all zero)

            if (Role* role = this->FoundRole(run))
            {
                const u32 layer = role->flags & Role::LayerMask;
                run.valueA = static_cast<i32>(layer);
                role->flags &= ~Role::LayerMask;

                if (run.code == EventCommand::LayerFront)
                {
                    role->flags |= Role::FrontLayer;
                }

                if (run.code == EventCommand::LayerBack)
                {
                    role->flags |= Role::BackLayer;
                }

                owner.field.RemoveRole(
                    role->id,
                    static_cast<int>(layer),
                    static_cast<int>(static_cast<u32>(role->y) >> MapHeader::TileShift) - 1,
                    0
                );
            }

            this->Next(run);
            return false;
        }

        case EventCommand::ScreenAnimation:
        {
            auto animation = std::make_unique<ScriptAnimation>();
            ResetPlayback(animation->act);
            Operands operands = this->OperandsOf(run);
            animation->x = static_cast<i16>(operands.Word());
            animation->y = static_cast<i16>(operands.Word());
            animation->act.id = operands.Word();
            animation->act.action = operands.Word();
            animation->next = std::move(view.animationsA);
            view.animationsA = std::move(animation);
            this->Next(run);
            return false;
        }

        case EventCommand::Sound:
        {
            Operands operands = this->OperandsOf(run);
            const u16 sound = operands.Word();
            owner.audio.PlaySound(sound, owner.config.soundVolume);
            this->Next(run);
            return false;
        }

        case EventCommand::ShowScreen:
        case EventCommand::BlackScreen:
            view.screenFlags &= ~Scene::BlackScreen;

            if (run.code == EventCommand::BlackScreen)
            {
                if (owner.offscreen != nullptr)
                {
                    owner.offscreen->Fill(0);
                }

                view.screenFlags |= Scene::BlackScreen;
            }

            this->Next(run);
            return false;

        case EventCommand::PlaceRoleOnMap:
            return this->PlaceRole(run);

        case EventCommand::Quake:
        {
            constexpr std::size_t kShakesAt = 2;  // after the frames: the offsets, ended by kListEnd
            constexpr std::size_t kMaxShakes = 0x38;
            const u16 frames = this->Word(run, kFirstOperand);
            std::size_t count = 0;

            while (this->Word(run, kShakesAt + count) != kListEnd &&
                   run.cursor + kWordBytes * (kShakesAt + count) + 1 < owner.databases.script.size())
            {
                ++count;
            }

            run.valueA = static_cast<i32>(count);

            if (count > kMaxShakes)
            {
                ErrorBox(this->game.errors, u8"對話系統(Shack_screen)", u8"參數太長");  // 0x004B2964, 0x004B297C
                return false;
            }

            view.shake.fill(Scene::ShakeEnd);

            for (std::size_t index = 0; index < count; ++index)
            {
                view.shake[index] = static_cast<i16>(this->Word(run, kShakesAt + index));
            }

            const auto length = static_cast<int>(kWordBytes * (kShakesAt + count + 1));
            run.valueA = length;
            view.shakeHold = frames;
            view.shakeFrames = frames;
            view.quakeCameraTop = field.cameraTop;
            view.quakeCameraLeft = field.cameraLeft;
            this->Advance(run, length);
            return true;
        }

        case EventCommand::StopQuake:
            view.shake.fill(Scene::ShakeEnd);
            this->Next(run);
            return true;

        case EventCommand::JoinFollowers:
        {
            Operands operands = this->OperandsOf(run);
            const u16 id = operands.Word();

            if (state.FindRoleOrPlayer(id, run.found))
            {
                owner.FollowLeader(static_cast<int>(run.found));
            }

            this->Next(run);
            return false;
        }

        case EventCommand::LeaveFollowers:
        {
            // swd3e's operands: the role, its act, action and direction, then its event and path, then its flags.
            Operands operands = this->OperandsOf(run);
            const u16 id = operands.Word();
            const u16 act = operands.Word();
            const u16 action = operands.Word();
            const u16 direction = operands.Word();
            const u16 event = operands.Word();
            const u16 path = operands.Word();
            const u16 flags = operands.Word();
            owner.LeaveParty(id, path, event, act, action, direction, flags);
            this->Next(run);
            return false;
        }

        case EventCommand::Wait:
        {
            constexpr u16 kWaiting = 0x8000;  // the command marks its length while it waits
            const u16 length = this->Word(run, kFirstOperand);
            run.found = length;

            if ((length & kWaiting) == 0)
            {
                this->waitLength = length;
                this->SetWord(run, kFirstOperand, static_cast<u16>(length | kWaiting));
                this->waitStart = owner.frameStart;
                return false;
            }

            if (owner.frameStart - this->waitStart <= this->waitLength)
            {
                return false;
            }

            this->SetWord(run, kFirstOperand, static_cast<u16>(length & ~kWaiting));
            this->Next(run);
            return true;
        }

        case EventCommand::ClearRoleHidden:
        case EventCommand::SetRoleHidden:
        {
            Operands operands = this->OperandsOf(run);
            const u16 id = this->EventRoleId(run, operands.Word());

            if (state.FindRoleOrPlayer(id, run.found))
            {
                Role& role = state.roles[run.found];
                role.flags =
                    run.code == EventCommand::ClearRoleHidden ? role.flags & ~Role::Hidden : role.flags | Role::Hidden;
            }
            else if (run.code == EventCommand::ClearRoleHidden)
            {
                owner.UpdateRoleRecord(
                    id,
                    Game::KeepValue,
                    Game::KeepValue,
                    Game::KeepValue,
                    Game::KeepValue,
                    Game::KeepValue,
                    Game::KeepValue,
                    Game::KeepValue,
                    0,
                    static_cast<i16>(~Role::Hidden),
                    Game::KeepValue
                );
            }
            else
            {
                owner.UpdateRoleRecord(
                    id,
                    Game::KeepValue,
                    Game::KeepValue,
                    Game::KeepValue,
                    Game::KeepValue,
                    Game::KeepValue,
                    Game::KeepValue,
                    Game::KeepValue,
                    Role::Hidden,
                    Game::KeepValue,
                    Game::KeepValue
                );
            }

            this->Next(run);
            return false;
        }

        case EventCommand::AttachAct:
        {
            Operands operands = this->OperandsOf(run);
            const u16 id = operands.Word();
            const u16 act = operands.Word();

            if (state.FindRoleOrPlayer(id, run.found))
            {
                state.roles[run.found].attached = act < view.screenActs.size() ? &view.screenActs[act] : nullptr;
            }

            this->Next(run);
            return false;
        }

        case EventCommand::DetachAct:
        {
            Operands operands = this->OperandsOf(run);
            const u16 id = operands.Word();

            if (state.FindRoleOrPlayer(id, run.found))
            {
                state.roles[run.found].attached = nullptr;
            }

            this->Next(run);
            return false;
        }

        case EventCommand::StopFade:
            view.fade.stepRed = 0;
            view.fade.stepGreen = 0;
            view.fade.stepBlue = 0;
            view.fade.frames = 0;
            this->Next(run);
            return true;

        case EventCommand::HoldRole:
            state.FindRoleOrPlayer(this->OperandsOf(run).Word(), run.found);

            if (run.found < GameState::Roles)
            {
                owner.rolePath.HoldRole(static_cast<int>(run.found));
            }

            this->Next(run);
            return true;

        case EventCommand::TurnToFace:
        {
            constexpr i32 kTurnDistance = 4;  // nearer than this the role faces direction 0
            Operands operands = this->OperandsOf(run);
            const u16 id = this->EventRoleId(run, operands.Word());
            const u16 otherId = operands.Word();

            // Both lookups report the first role (role -1, before the roles, when it is missing; the blank role here).
            const bool found = state.FindRoleOrPlayer(id, run.found);
            Role* role = this->FoundRole(run);
            const Role& turning = role != nullptr ? *role : blank;

            if (!found)
            {
                ReportActError(owner.errors, u8"Act Err(Talk:TurnFace[1])", turning);
            }

            u32 other = 0;

            if (!state.FindRoleOrPlayer(otherId, other))
            {
                ReportActError(owner.errors, u8"Act Err(Talk:TurnFace[2])", turning);
            }

            const Role& faced = other < GameState::Roles ? state.roles[other] : blank;
            i32 distance = 0;
            const int angle = AngleBetween(
                faced.x + MapHeader::TileSize / 2 * faced.act.width,
                faced.y + MapHeader::TileSize / 2 * faced.act.height,
                turning.x + MapHeader::TileSize / 2 * turning.act.width,
                turning.y + MapHeader::TileSize / 2 * turning.act.height,
                distance
            );
            const int direction = AngleToDirection(angle);
            run.valueA = direction;
            run.valueB = distance;

            if (role != nullptr)
            {
                role->act.action = 0;
                role->act.direction = 0;

                if (distance >= kTurnDistance)
                {
                    role->act.direction = direction;
                }

                role->act.delay = 0;

                if (owner.acts.Run(role->act) == 0)
                {
                    ReportActError(owner.errors, u8"Act Err(Talk:TurnFace[3])", *role);
                }

                owner.rolePath.HoldRole(static_cast<int>(run.found));
            }

            this->Next(run);
            return true;
        }

        case EventCommand::FixSpeed:
        case EventCommand::FreeSpeed:
        {
            // swd3e: nothing happens for a role not on the map (swd32 showed a message).
            Operands operands = this->OperandsOf(run);
            const u16 id = this->EventRoleId(run, operands.Word());
            run.valueB = CommandBytes(run.code);

            if (state.FindRoleOrPlayer(id, run.found))
            {
                Role& role = state.roles[run.found];

                if (run.code == EventCommand::FixSpeed)
                {
                    const u16 delay = operands.Word();
                    role.act.speed = static_cast<i16>(delay | static_cast<u16>(ActState::FixedDelay));
                }
                else
                {
                    role.act.speed = 0;
                }

                role.act.delay = 0;

                if (owner.acts.Run(role.act) == 0)
                {
                    ReportActError(owner.errors, u8"Act Err(Talk:SetRoleSpd)", role);
                }
            }

            this->Advance(run, run.valueB);
            return true;
        }

        case EventCommand::FlyingSprite:
        {
            auto sprite = std::make_unique<ScriptSprite>();
            ResetPlayback(sprite->act);
            Operands operands = this->OperandsOf(run);
            sprite->act.id = operands.Word();
            sprite->act.direction = 0;
            sprite->act.action = operands.Word();
            sprite->headX = static_cast<i16>(MapHeader::TileSize * operands.Word());
            sprite->headSpeed = static_cast<i16>(MapHeader::TileSize * operands.Word());
            sprite->targetX = static_cast<i16>(MapHeader::TileSize * operands.Word());
            sprite->targetY = static_cast<i16>(MapHeader::TileSize * operands.Word());
            const i32 fromX = sprite->headX;
            const i32 fromY = sprite->headSpeed;
            const auto speed = static_cast<double>(static_cast<i16>(operands.Word()));
            const i32 across = sprite->targetX - fromX;
            const i32 down = sprite->targetY - fromY;
            // The squares are summed by 32-bit imul and add, wrapping, then read as signed (0x0042E5BB).
            const auto squares = static_cast<i32>(
                static_cast<u32>(down) * static_cast<u32>(down) + static_cast<u32>(across) * static_cast<u32>(across)
            );
            const double scale = speed / std::sqrt(static_cast<double>(squares));
            sprite->stepX = static_cast<float>(static_cast<double>(across) * scale);
            sprite->stepY = static_cast<float>(static_cast<double>(down) * scale);
            sprite->x = static_cast<float>(fromX);
            sprite->y = static_cast<float>(fromY);
            sprite->next = std::move(view.spritesA);
            view.spritesA = std::move(sprite);
            this->Next(run);
            return true;
        }

        case EventCommand::DialogOverlay:
            this->dialogOptions &= ~Talk::Overlay;
            this->Next(run);
            return true;

        case EventCommand::HeadIn:
        {
            auto sprite = std::make_unique<ScriptSprite>();
            ResetPlayback(sprite->act);
            Operands operands = this->OperandsOf(run);
            sprite->act.id = operands.Word();
            sprite->act.action = operands.Word();
            const u16 target = operands.Word();
            const u16 place = operands.Word();
            sprite->targetX = static_cast<i16>(target);
            sprite->headX = static_cast<i16>(-ScriptSprite::HeadMargin);
            sprite->targetY = static_cast<i16>(place & (kPlaceAtTarget - 1));

            if (static_cast<i16>(target) > Display::Width / 2)  // a signed compare (0x0042E6A8)
            {
                sprite->headX = static_cast<i16>(Display::Width + ScriptSprite::HeadMargin);
            }

            sprite->headSpeed = 0;

            if ((place & kPlaceAtTarget) != 0)
            {
                sprite->headX = static_cast<i16>(target);
                sprite->headSpeed = ScriptSprite::HeadPlaced;
            }

            sprite->next = std::move(view.spritesB);
            view.spritesB = std::move(sprite);
            this->Next(run);
            return true;
        }

        case EventCommand::HeadOut:
        {
            Operands operands = this->OperandsOf(run);
            const u16 actId = operands.Word();
            const u16 action = operands.Word();

            for (ScriptSprite* sprite = view.spritesB.get(); sprite != nullptr; sprite = sprite->next.get())
            {
                if (sprite->act.id == actId && sprite->act.action == action)
                {
                    if (sprite->headSpeed >= 0)
                    {
                        sprite->headSpeed =
                            sprite->headX <= Display::Width / 2 ? i16{-1} : i16{1};  // signed (0x0042E76E)
                    }
                    else
                    {
                        sprite->headSpeed = ScriptSprite::HeadDropSpeed;
                    }

                    break;
                }
            }

            this->Next(run);
            return true;
        }

        case EventCommand::LineEffect:
        {
            Operands operands = this->OperandsOf(run);
            const u16 mode = operands.Word();

            if (mode > ScriptEffect::NumberMask)
            {
                ErrorBox(
                    this->game.errors,
                    u8"對話系統(GAON)",
                    u8"Talk(%d) ,代碼值太大(%d)",
                    run.context.event,
                    mode
                );  // 0x004B28C8, 0x004B28D8
                this->Next(run);
                return true;
            }

            run.valueA = 0;

            if (EraseNodes(
                    view.effects,
                    [mode](const ScriptEffect& effect)
                    {
                        return static_cast<u8>(effect.mode) == mode;
                    },
                    false
                ))
            {
                run.valueA = 1;
            }

            constexpr u16 kRisingLines = 1;  // the line kinds of the command (0: falling)
            constexpr u16 kStillLines = 2;
            constexpr int kSpareColumns = 4;  // past the count in the column buffers
            constexpr i16 kLineStart = 2;     // pixels a line starts with
            const u16 color = operands.Word();
            const u16 lines = operands.Word();
            const u16 x = operands.Word();
            const u16 y = operands.Word();
            const u16 limit = operands.Word();
            const u16 count = operands.Word();
            auto effect = std::make_unique<ScriptEffect>();
            effect->mode = mode;
            effect->color = static_cast<i16>(color);
            effect->x = static_cast<i16>(x & kEvenPixels);
            effect->y = static_cast<i16>(y & kEvenPixels);
            effect->limit = static_cast<i16>(limit & kEvenPixels);
            effect->count = static_cast<i16>(count & kEvenPixels);

            if (effect->x < 0 || effect->y < 0 || effect->limit + effect->x > Display::Width ||
                effect->count + effect->y > Display::Height)
            {
                ErrorBox(
                    this->game.errors,
                    u8"對話系統(GAON)",
                    u8"Talk(%d) ,位置超出螢幕(x%d ,y%d ,w%d ,h%d)",
                    effect->x,
                    effect->y,
                    effect->limit,
                    effect->count
                );  // 0x004B28C8, 0x004B289C
                this->Next(run);
                return true;
            }

            const auto columns = static_cast<std::size_t>(std::max<int>(effect->count, 0) + kSpareColumns);
            effect->offsets.assign(columns, 0);
            effect->lengths.assign(columns, 0);
            run.valueB = 0;
            u16 kind = ScriptEffect::Falling;

            if (lines == kRisingLines)
            {
                run.valueB = effect->limit - kLineStart;
                kind = ScriptEffect::Rising;
            }

            if (lines == kStillLines)
            {
                kind = ScriptEffect::Still;
                run.valueB = 0;
            }

            effect->mode |= kind;
            run.valueA = 0;  // the column count left behind (0 without columns, whatever the search found)

            for (int column = 0; column < effect->count; ++column)
            {
                effect->offsets[static_cast<std::size_t>(column)] = static_cast<i16>(run.valueB);
                effect->lengths[static_cast<std::size_t>(column)] = kLineStart;
                run.valueA = column + 1;
            }

            effect->next = std::move(view.effects);
            view.effects = std::move(effect);
            this->Next(run);
            return true;
        }

        case EventCommand::LineEffectMode:
        {
            Operands operands = this->OperandsOf(run);
            const u16 mode = operands.Word();

            if (mode > ScriptEffect::NumberMask)
            {
                ErrorBox(
                    this->game.errors,
                    u8"對話系統(G_AREA_OFF)",
                    u8"Talk(%d)代碼值太大(%d)",
                    run.context.id,
                    mode
                );  // 0x004B286C, 0x004B2884
                this->Next(run);
                return true;
            }

            ScriptEffect* found = nullptr;

            for (ScriptEffect* effect = view.effects.get(); effect != nullptr; effect = effect->next.get())
            {
                if (static_cast<u8>(effect->mode) == mode)
                {
                    found = effect;
                    break;
                }
            }

            if (found == nullptr)
            {
                this->Next(run);
                return true;
            }

            constexpr u16 kOpen = 0;  // the changes of LineEffectMode
            constexpr u16 kClose = 1;
            constexpr u16 kRemove = 2;
            const u16 change = operands.Word();

            if (change == kOpen)
            {
                run.valueA = ScriptEffect::Opening;
            }

            if (change == kClose)
            {
                run.valueA = ScriptEffect::Closing;
            }

            if (change == kRemove)
            {
                EraseNodes(
                    view.effects,
                    [found](const ScriptEffect& effect)
                    {
                        return &effect == found;
                    },
                    true
                );
            }
            else
            {
                found->mode = static_cast<u16>(run.valueA | static_cast<u8>(found->mode));
            }

            this->Next(run);
            return true;
        }

        case EventCommand::ObsoleteVideo:
            // swd3e: an obsolete command; the error box shows the scratch text buffer (0x004C4658) as it was left,
            // here nothing (deviations.md), and the script stays on the command.
            ErrorBox(this->game.errors, u8"對話系統(Play_avi) ,過期的指令", u8"");  // 0x004B284C
            return false;

        case EventCommand::ChangeHead:
        {
            Operands operands = this->OperandsOf(run);
            const u16 actId = operands.Word();
            const u16 action = operands.Word();
            const u16 newActId = operands.Word();
            const u16 newAction = operands.Word();

            for (ScriptSprite* sprite = view.spritesB.get(); sprite != nullptr; sprite = sprite->next.get())
            {
                if (sprite->act.id == actId && sprite->act.action == action)
                {
                    sprite->act.id = newActId;
                    sprite->act.action = newAction;
                    break;
                }
            }

            this->Next(run);
            return true;
        }

        case EventCommand::RandomJump:
        {
            auto targetAt = [](u32 index)
            {
                return kOpcodeBytes + sizeof(u32) * static_cast<std::size_t>(index);
            };

            u32 count = 0;

            while (this->Dword(run, targetAt(count)) != kTargetsEnd &&
                   run.cursor + targetAt(count) < owner.databases.script.size())
            {
                ++count;
            }

            run.valueA = static_cast<i32>(count);

            if (count == 0)
            {
                Fault(owner.errors, u8"Integer divide by zero (0x0043DB9D)");
            }

            run.valueB = static_cast<i32>(owner.random.Below(count));
            this->Jump(run, this->Dword(run, targetAt(static_cast<u32>(run.valueB))));
            return true;
        }

        case EventCommand::StartBattle:
            view.FreeEffects();

            view.FreeSpritesB();
            {
                const auto battle = static_cast<i16>(this->OperandsOf(run).Word());
                owner.battleRequest = battle | static_cast<i32>(Game::ScriptedBattle);
                this->Next(run);
                return false;
            }

        case EventCommand::RecordDialogWait:
        case EventCommand::RecordDialog:
            return this->OpenDialog(run);

        case EventCommand::RoleNamePlate:
            return this->LoadName(run);

        case EventCommand::SetInParty:
        case EventCommand::ClearInParty:
        {
            Operands operands = this->OperandsOf(run);
            run.found = static_cast<u32>(operands.Word() - 1);  // the roles count from 1

            if (run.found >= Party::Roles)
            {
                ErrorBox(
                    this->game.errors,
                    u8"對話系統(Addd_FMan)",
                    u8"人物號碼不對",
                    run.found
                );  // 0x004B2828, 0x004B283C
            }

            this->SetStoryFlag(static_cast<int>(run.found + GameState::InParty), run.code == EventCommand::SetInParty);
            this->Next(run);
            return true;
        }

        case EventCommand::CameraHold:
            view.screenFlags |= Scene::CameraHeld;
            this->Next(run);
            return false;

        case EventCommand::CameraFollow:
            view.screenFlags &= ~Scene::CameraHeld;
            this->Next(run);
            return false;

        case EventCommand::MapMovie:
            return this->OpenMapMovie(run);

        case EventCommand::WaitMapMovie:
            if (owner.fieldView.movie.active != 0)
            {
                return false;
            }

            this->Next(run);
            state.ClearFrameInterval();
            state.SetFrameInterval(Game::FrameInterval);

            if (!this->keepArchiveClosed)
            {
                owner.field.ReopenArchive(owner.currentMap);
            }

            this->keepArchiveClosed = false;
            return true;

        case EventCommand::SetMovieFrame:
            owner.fieldView.movie.frameIndex = this->OperandsOf(run).Word();
            this->Next(run);
            return false;

        default:
            return this->UnknownCommand(run);
    }
}

}  // namespace swd3e
