// scripts/bestiary/dump <game dir> data | portrait <action> <first> <last>
// data: one JSON line per mon.dat object 1..500 (the bestiary's range) with the fields the page shows.
// portrait: each act's action at its first frame as "id width height" and its RGB rows.
// Build after `cmake --build --preset linux`, linking build/linux/libswd3e_core.a and the libraries of swd3e_tests
// (third_party/install/linux/lib: SDL3, avformat, avcodec, swscale, swresample, avutil) with -static -pthread.
// The page: dump <dir> data > monsters.jsonl; dump <dir> portrait 59 1 500 > portraits.raw;
// python3 scripts/bestiary/build.py monsters.jsonl portraits.raw docs/bestiary.html
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

std::string Name(swd3e::Database& database, swd3e::u16 id)
{
    if (id == 0)
    {
        return std::string();
    }

    swd3e::ObjectRecord record;

    if (!database.LoadObject(record, id))
    {
        return "#" + std::to_string(id);
    }

    return swd3e::Utf8FromBig5(swd3e::ObjectName(record));
}

int Data(const std::u8string& directory)
{
    swd3e::Database database(directory);

    for (swd3e::u32 id = 1; id <= 500; ++id)
    {
        swd3e::ObjectRecord record;

        if (!database.LoadObject(record, static_cast<swd3e::u16>(id)))
        {
            continue;
        }

        const swd3e::ObjectData& d = record.data;
        // Menu::BestiaryChoose (0x004427B0)
        const int level = d.level;
        int spirit = 0;
        spirit += d.itemA != 0 ? 2 * level : 0;
        spirit += d.itemB != 0 ? 3 * level : 0;
        spirit += d.itemC != 0 ? 5 * level : 0;
        spirit += d.healItemA != 0 ? 2 * level : 0;
        spirit += d.healItemB != 0 ? 4 * level : 0;
        int stamina = 0;
        stamina += d.skillA != 0 ? 3 * level : 0;
        stamina += d.skillB != 0 ? 5 * level : 0;
        std::string resist;

        for (std::size_t at = 0; at < d.resistances.size(); ++at)
        {
            resist += (at != 0 ? "," : "") + std::to_string(static_cast<int>(static_cast<swd3e::i8>(d.resistances[at])));
        }

        const std::string description = swd3e::Utf8FromBig5(record.description.value_or(std::string()));
        std::printf(
            "{\"id\":%u,\"name\":%s,\"category\":%u,\"kind\":%u,\"level\":%d,\"hp\":%d,\"spirit\":%d,\"stamina\":%d,"
            "\"attack\":%u,\"defense\":%u,\"agility\":%u,\"act\":%u,"
            "\"items\":[%s,%s,%s],\"itemCounts\":[%u,%u,%u],\"skills\":[%s,%s],\"skillCounts\":[%u,%u],"
            "\"heals\":[%s,%s],\"healCounts\":[%u,%u],\"itemChance\":%u,\"skillChance\":%u,"
            "\"experience\":%u,\"gold\":%u,\"resistances\":[%s],\"ailmentChance\":%u,\"fleeChance\":%u,"
            "\"steal\":%s,\"stealDifficulty\":%u,\"drop\":%s,\"dropChance\":%u,"
            "\"enrageCondition\":%u,\"enrageEffect\":%u,\"enrageSkill\":%s,\"battleLevel\":%u,\"skill\":%u,\"wisdom\":%u,"
            "\"description\":%s}\n",
            id,
            Json(swd3e::Utf8FromBig5(swd3e::ObjectName(record))).c_str(),
            d.category,
            d.kind,
            level,
            d.hitPoints,
            spirit,
            stamina,
            d.attack,
            d.defense,
            d.agility,
            d.act,
            Json(Name(database, d.itemA)).c_str(),
            Json(Name(database, d.itemB)).c_str(),
            Json(Name(database, d.itemC)).c_str(),
            d.itemACount,
            d.itemBCount,
            d.itemCCount,
            Json(Name(database, d.skillA)).c_str(),
            Json(Name(database, d.skillB)).c_str(),
            d.skillACount,
            d.skillBCount,
            Json(Name(database, d.healItemA)).c_str(),
            Json(Name(database, d.healItemB)).c_str(),
            d.healCountA,
            d.healCountB,
            d.itemChance,
            d.skillChance,
            d.unk66,
            d.unk90,
            resist.c_str(),
            d.ailmentChance,
            d.catchChance,
            Json(Name(database, d.stealItem)).c_str(),
            d.stealDifficulty,
            Json(Name(database, d.dropItem)).c_str(),
            d.dropChance,
            d.enrageCondition,
            d.enrageEffect,
            Json(d.enrageEffect != 0 && d.enrageEffect < 1001 ? Name(database, d.enrageEffect) : std::string()).c_str(),
            d.battleLevel,
            d.skill,
            d.unk60,
            Json(description).c_str()
        );
    }

    return 0;
}

int Portrait(const std::u8string& directory, int action, int first, int last)
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
        std::fprintf(stderr, "usage: dump <game dir> data | portrait <action> <first> <last>\n");
        return 2;
    }

    const std::string text = argv[1];
    const std::u8string directory(text.begin(), text.end());

    if (std::strcmp(argv[2], "data") == 0)
    {
        return Data(directory);
    }

    if (std::strcmp(argv[2], "portrait") == 0 && argc >= 6)
    {
        return Portrait(directory, std::atoi(argv[3]), std::atoi(argv[4]), std::atoi(argv[5]));
    }

    return 2;
}
