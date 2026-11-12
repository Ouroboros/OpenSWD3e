// scripts/events/dump <game dir>: the events page's data as JSON lines, the game directory ending in a slash:
//   {"kind":"map","map":N,"name":"..."}                                   the place names (maps.dat table 0x14)
//   {"kind":"role","map":M,"id":I,"act":A,"x":X,"y":Y,"event":E}          the roles of maps.dat table 1 with an event
//   {"kind":"region","map":M,"region":R,"event":E,"name":"..."}          the regions of HUGE.lmf's maps with an event
//   {"kind":"event","event":E,"commands":[[code,word...],...],"texts":["...",...]}
//                                     every event of Talk1..4.dat: its own commands (the scripts it opens apart), the
//                                     operand words of each (none for the dialogs), and its dialogs' texts (UTF-8,
//                                     control codes kept)
// Build after `cmake --build --preset linux` with -Isrc -Ibuild/linux/generated, linking build/linux/libswd3e_core.a and
// the libraries of swd3e_tests (third_party/install/linux/lib: SDL3, avformat, avcodec, swscale, swresample, avutil)
// with -static -pthread. The page:
//   dump <dir> > events.jsonl;
//   (scripts/items/dump <dir> data > objects.jsonl; scripts/bestiary/dump <dir> data > monsters.jsonl;
//    scripts/bestiary/dump <dir> formations > formations.jsonl)
//   python3 scripts/events/build.py events.jsonl objects.jsonl monsters.jsonl formations.jsonl docs/events.html
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "core/types.h"
#include "game/databases.h"
#include "talk/event_command.h"
#include "talk/event_scan.h"
#include "talk/script_text.h"
#include "talk/talk.h"
#include "text/big5.h"

namespace
{

using swd3e::u16;
using swd3e::u32;
using swd3e::u8;

constexpr std::size_t kMapsHeader = 0x200;  // maps.dat's tables follow its header
constexpr std::size_t kTableBytes = 4;      // a dword offset per table
constexpr u32 kRolesTable = 1;
constexpr u32 kPlaceNamesTable = 0x14;

// HUGE.lmf (Field::LoadMap): the directory's offset first, at the end one entry per map (its start, its number);
// each map: its magic, then a header whose region section is an offset from the map's start and a size; the
// regions follow their count, each its id, event, flag, block flag and name.
constexpr std::size_t kDirectoryEntryBytes = 0x10;
constexpr std::size_t kEntryStartAt = 0;
constexpr std::size_t kEntryMapAt = 8;
constexpr std::size_t kMagicBytes = 4;
constexpr std::size_t kRegionsAt = 0x10;
constexpr std::size_t kRegionsBytesAt = 0x14;
constexpr std::size_t kCountBytes = 4;
constexpr std::size_t kRegionBytes = 0x30;
constexpr std::size_t kRegionEventAt = 4;
constexpr std::size_t kRegionNameAt = 0x10;
constexpr std::size_t kRegionNameBytes = 0x20;

std::vector<u8> ReadFile(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    return std::vector<u8>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

u16 Word(const std::vector<u8>& bytes, std::size_t at)
{
    return at + 1 < bytes.size() ? static_cast<u16>(bytes[at] | bytes[at + 1] << 8) : u16{0};
}

u32 Dword(const std::vector<u8>& bytes, std::size_t at)
{
    return Word(bytes, at) | static_cast<u32>(Word(bytes, at + 2)) << 16;
}

// A Big5 text as a JSON string (UTF-8, escaped).
std::string Json(std::string_view big5)
{
    std::string out = "\"";

    for (const char character : swd3e::Utf8FromBig5(big5))
    {
        const auto byte = static_cast<u8>(character);

        if (character == '"' || character == '\\')
        {
            out += '\\';
            out += character;
        }
        else if (byte < 0x20)
        {
            char escaped[8];
            std::snprintf(escaped, sizeof(escaped), "\\u%04x", byte);
            out += escaped;
        }
        else
        {
            out += character;
        }
    }

    return out + "\"";
}

void PlaceNames(const std::vector<u8>& maps)
{
    std::size_t at = Dword(maps, kTableBytes * kPlaceNamesTable);

    while (at + 1 < maps.size() && Word(maps, at) != swd3e::MapsRecord::TableEnd)
    {
        const u16 map = Word(maps, at);
        const std::size_t name = at + 2;
        std::size_t end = name;

        while (end + 1 < maps.size() && Word(maps, end) != swd3e::MapsRecord::NameEnd)
        {
            ++end;
        }

        const std::string_view text(reinterpret_cast<const char*>(maps.data()) + name, end - name);
        std::printf("{\"kind\":\"map\",\"map\":%u,\"name\":%s}\n", map, Json(text).c_str());
        at = end + 2;
    }
}

void Roles(const std::vector<u8>& maps)
{
    using swd3e::MapsRecord;
    constexpr std::size_t kRoleBytes = 2 * MapsRecord::RoleWords;

    for (std::size_t at = Dword(maps, kTableBytes * kRolesTable);
         at + kRoleBytes <= maps.size() && Word(maps, at) != MapsRecord::TableEnd;
         at += kRoleBytes)
    {
        auto word = [&maps, at](std::size_t index)
        {
            return Word(maps, at + 2 * index);
        };

        if (word(MapsRecord::RoleEvent) == 0)
        {
            continue;
        }

        std::printf(
            "{\"kind\":\"role\",\"map\":%u,\"id\":%u,\"act\":%u,\"x\":%u,\"y\":%u,\"event\":%u}\n",
            word(MapsRecord::RoleMap),
            word(MapsRecord::RoleId),
            word(MapsRecord::RoleAct),
            word(MapsRecord::RoleColumn),
            word(MapsRecord::RoleRow),
            word(MapsRecord::RoleEvent)
        );
    }
}

void Regions(const std::vector<u8>& lmf)
{
    const u32 directory = Dword(lmf, 0);

    for (std::size_t entry = directory; entry + kDirectoryEntryBytes <= lmf.size(); entry += kDirectoryEntryBytes)
    {
        const u32 start = Dword(lmf, entry + kEntryStartAt);
        const u32 map = Dword(lmf, entry + kEntryMapAt);
        const std::size_t head = start + kMagicBytes;
        const std::size_t section = start + Dword(lmf, head + kRegionsAt);

        if (Dword(lmf, head + kRegionsBytesAt) < kCountBytes || section + kCountBytes > lmf.size())
        {
            continue;
        }

        const u32 count = Dword(lmf, section);

        for (u32 region = 0; region < count; ++region)
        {
            const std::size_t at = section + kCountBytes + kRegionBytes * region;

            if (at + kRegionBytes > lmf.size())
            {
                break;
            }

            const u32 event = Dword(lmf, at + kRegionEventAt);

            if (event == 0)
            {
                continue;
            }

            const char* name = reinterpret_cast<const char*>(lmf.data()) + at + kRegionNameAt;
            std::printf(
                "{\"kind\":\"region\",\"map\":%u,\"region\":%u,\"event\":%u,\"name\":%s}\n",
                map,
                Dword(lmf, at),
                event,
                Json(std::string_view(name, strnlen(name, kRegionNameBytes))).c_str()
            );
        }
    }
}

// The words before a dialog's text (Talk::SetupDialog: a speech's, a sized window's, a record's); 0 for no dialog.
std::size_t DialogTextWords(swd3e::EventCommand code)
{
    using swd3e::EventCommand;
    constexpr std::size_t kSpeechWords = 2;
    constexpr std::size_t kRecordWords = 4;
    constexpr std::size_t kSizedWords = 6;

    switch (code)
    {
        case EventCommand::DialogWait:
        case EventCommand::Dialog:
            return kSpeechWords;

        case EventCommand::SizedDialogWait:
        case EventCommand::SizedDialog:
        case EventCommand::PlainDialogWait:
        case EventCommand::PlainDialog:
            return kSizedWords;

        case EventCommand::RecordDialogWait:
        case EventCommand::RecordDialog:
            return kRecordWords;

        default:
            return 0;
    }
}

void Events(const std::u8string& directory)
{
    swd3e::EventScan scan;

    constexpr u32 kTalkFiles = 4;  // Talk1.dat..Talk4.dat

    for (u32 event = 1; event < kTalkFiles * swd3e::Talk::EventsPerFile; ++event)
    {
        std::string commands;
        std::string texts;
        scan.Scan(
            directory,
            event,
            swd3e::EventScan::OpenDepth,
            [&commands, &texts](swd3e::EventCommand code, std::span<const u8> command)
            {
                std::string one = "[" + std::to_string(static_cast<int>(code));

                if (const std::size_t words = DialogTextWords(code); words != 0)
                {
                    const std::size_t start = std::min<std::size_t>(swd3e::kOpcodeBytes + 2 * words, command.size());
                    const std::size_t end = swd3e::TextEndAt(command, start);
                    const std::string_view text(
                        reinterpret_cast<const char*>(command.data()) + start,
                        end - start
                    );
                    texts += (texts.empty() ? "" : ",") + Json(text);
                }
                else
                {
                    for (std::size_t at = swd3e::kOpcodeBytes; at + 1 < command.size(); at += 2)
                    {
                        one += "," + std::to_string(swd3e::TextWord(command, at));
                    }
                }

                commands += (commands.empty() ? "" : ",") + one + "]";
                return false;
            }
        );

        if (commands.empty())
        {
            continue;
        }

        std::printf(
            "{\"kind\":\"event\",\"event\":%u,\"commands\":[%s],\"texts\":[%s]}\n",
            event,
            commands.c_str(),
            texts.c_str()
        );
    }
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: dump <game dir/>\n");
        return 2;
    }

    const std::string text = argv[1];
    const std::vector<u8> all = ReadFile(text + "MAPS.dat");

    if (all.size() <= kMapsHeader)
    {
        std::fprintf(stderr, "MAPS.dat missing\n");
        return 1;
    }

    const std::vector<u8> maps(all.begin() + static_cast<std::ptrdiff_t>(kMapsHeader), all.end());
    PlaceNames(maps);
    Roles(maps);
    Regions(ReadFile(text + "HUGE.lmf"));
    Events(std::u8string(text.begin(), text.end()));
    return 0;
}
