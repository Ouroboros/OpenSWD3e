// scripts/items/dump <game dir> data | picture <action> <first> <last>
// data: one JSON line per mon.dat object 1..1199 with a name (the range the debug key F6 walks, 0x00402AA0), with the
// fields the items and altar pages show (the altar takes objects of any kind among 501..899).
// picture: each act's action at its first frame as "id width height" and its RGB rows (the item pictures, act 0x3B).
// Build after `cmake --build --preset linux`, linking build/linux/libswd3e_core.a and the libraries of swd3e_tests
// (third_party/install/linux/lib: SDL3, avformat, avcodec, swscale, swresample, avutil) with -static -pthread.
// The game directory ends in a slash. The pages:
//   dump <dir> data > objects.jsonl; dump <dir> picture 59 1 1199 > pictures.raw;
//   python3 scripts/sound/extract.py <dir>all.snd 0x2E move.wav;
//   python3 scripts/items/build.py objects.jsonl pictures.raw monsters.jsonl docs/items.html move.wav
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#include "draw/painter.h"
#include "org/database.h"
#include "org/object_record.h"
#include "pic/picture.h"
#include "pic/rle.h"
#include "platform/message_box.h"
#include "platform/surface.h"
#include "res/act_archive.h"
#include "res/act_state.h"
#include "res/tsw_archive.h"
#include "text/big5.h"

namespace
{

constexpr swd3e::u32 kLastObject = 0x4AF;

class QuietReporter : public swd3e::ErrorReporter
{
public:
    void ShowError(std::u8string_view, std::u8string_view text) override
    {
        std::fprintf(stderr, "error: %s\n", reinterpret_cast<const char*>(std::u8string(text).c_str()));
    }

    swd3e::AssertChoice AskAssertion(std::u8string_view, std::u8string_view) override
    {
        return swd3e::AssertChoice::Ignore;
    }
};

std::string Json(const std::string& text)
{
    std::string out = "\"";

    for (const char c : text)
    {
        if (c == '"' || c == '\\')
        {
            out += '\\';
            out += c;
        }
        else if (static_cast<unsigned char>(c) < 0x20)
        {
            char escape[8];
            std::snprintf(escape, sizeof(escape), "\\u%04x", static_cast<unsigned char>(c));
            out += escape;
        }
        else
        {
            out += c;
        }
    }

    return out + "\"";
}

int Data(const std::u8string& directory)
{
    swd3e::Database database(directory);

    for (swd3e::u32 id = 1; id <= kLastObject; ++id)
    {
        swd3e::ObjectRecord record;

        if (!database.LoadObject(record, static_cast<swd3e::u16>(id)))
        {
            continue;
        }

        const swd3e::ObjectData& d = record.data;
        const std::string name = swd3e::Utf8FromBig5(swd3e::ObjectName(record));

        if (name.empty())
        {
            continue;
        }

        std::string resist;

        for (std::size_t at = 0; at < d.resistances.size(); ++at)
        {
            resist += (at != 0 ? "," : "") + std::to_string(static_cast<int>(static_cast<swd3e::i8>(d.resistances[at])));
        }

        swd3e::SkillScript skill;

        if (d.skillScript == 0 || !database.LoadSkill(skill, d.skillScript))
        {
            skill = swd3e::SkillScript{};
        }

        const std::string description = swd3e::Utf8FromBig5(record.description.value_or(std::string()));
        std::printf(
            "{\"id\":%u,\"name\":%s,\"kind\":%u,\"category\":%u,\"level\":%u,\"act\":%u,\"price\":%u,"
            "\"usableBy\":%u,\"useKind\":%u,\"attackBonusUp\":%u,\"defenseBonusUp\":%u,\"strengthUp\":%u,"
            "\"enduranceUp\":%u,\"agilityUp\":%u,\"wisdomUp\":%u,\"rapportUp\":%u,\"evasionUp\":%u,"
            "\"hitPointsUp\":%u,\"spiritUp\":%u,\"staminaUp\":%u,\"costBits\":%u,\"counterMax\":%u,\"unk42\":%u,\"unk48\":%u,"
            "\"skillScript\":%u,\"skillFlags\":%u,\"skillAilment\":%u,\"altar\":%u,\"unkA2\":%u,\"resistances\":[%s],\"description\":%s}\n",
            id,
            Json(name).c_str(),
            d.kind,
            d.category,
            d.level,
            d.act,
            d.price,
            d.usableBy,
            d.useKind,
            d.attackBonusUp,
            d.defenseBonusUp,
            d.strengthUp,
            d.enduranceUp,
            d.agilityUp,
            d.wisdomUp,
            d.rapportUp,
            d.evasionUp,
            d.hitPointsUp,
            d.spiritUp,
            d.staminaUp,
            d.costBits,
            d.counterMax,
            d.unk42,
            d.unk48,
            d.skillScript,
            skill.flags,
            skill.ailment,
            d.altar,
            d.unkA2,
            resist.c_str(),
            Json(description).c_str()
        );
    }

    return 0;
}

int Picture(const std::u8string& directory, int action, int first, int last)
{
    QuietReporter quiet;
    swd3e::Surface scratch(64, 64);
    swd3e::Painter painter;
    painter.Init(scratch);
    swd3e::RleCodec codec(painter);
    swd3e::TswArchive pictures(codec, quiet);
    pictures.SetGameDirectory(directory);
    pictures.OpenArchives();
    pictures.SetCacheLimit(32 << 20);
    swd3e::ActArchive acts(quiet);
    acts.SetGameDirectory(directory);
    acts.SetCacheLimit(16 << 20);

    for (int id = first; id <= last; ++id)
    {
        swd3e::ActState state;
        state.id = id;
        state.action = action;

        if (acts.Run(state) == 0)
        {
            continue;
        }

        const swd3e::Picture* picture = pictures.GetPicture(state.frame, state.picture);

        if (picture == nullptr)
        {
            continue;
        }

        const std::optional<swd3e::RawPicture> raw = codec.Decode(picture->data);

        if (!raw.has_value() || raw->width <= 0 || raw->height <= 0)
        {
            continue;
        }

        std::vector<unsigned char> rgb(static_cast<std::size_t>(raw->width) * raw->height * 3);

        for (int at = 0; at < raw->width * raw->height; ++at)
        {
            unsigned pixel = 0;

            if (raw->format == 8)
            {
                const swd3e::u8 entry = raw->pixels[static_cast<std::size_t>(at)];
                pixel = entry < picture->palette.size() ? picture->palette[entry] : 0;
            }
            else
            {
                const std::size_t from = 2 * static_cast<std::size_t>(at);
                pixel = raw->pixels[from] | (raw->pixels[from + 1] << 8);
            }

            unsigned char* out = &rgb[3 * static_cast<std::size_t>(at)];
            out[0] = static_cast<unsigned char>(((pixel >> 11) & 0x1F) << 3);
            out[1] = static_cast<unsigned char>(((pixel >> 5) & 0x3F) << 2);
            out[2] = static_cast<unsigned char>((pixel & 0x1F) << 3);
        }

        std::printf("%d %d %d\n", id, raw->width, raw->height);
        std::fwrite(rgb.data(), 1, rgb.size(), stdout);
        std::fflush(stdout);
    }

    return 0;
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::fprintf(stderr, "usage: dump <game dir> data | picture <action> <first> <last>\n");
        return 2;
    }

    const std::string text = argv[1];
    const std::u8string directory(text.begin(), text.end());

    if (std::strcmp(argv[2], "data") == 0)
    {
        return Data(directory);
    }

    if (std::strcmp(argv[2], "picture") == 0 && argc >= 6)
    {
        return Picture(directory, std::atoi(argv[3]), std::atoi(argv[4]), std::atoi(argv[5]));
    }

    return 2;
}
