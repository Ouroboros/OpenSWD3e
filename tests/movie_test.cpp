#include <SDL3/SDL.h>

#include <cstdio>

#include "game_data.h"
#include "movie/movie.h"
#include "platform/display.h"
#include "platform/surface.h"
#include "test.h"

using swd3e::Display;

namespace
{

constexpr int kVolume = 0x90;  // past Movie::MaxVolume
constexpr int kLowVolume = 100;
constexpr int kSecondFrame = 2;  // Position counts the frames from 1

}  // namespace

TEST_CASE("movie: Bink frames are decoded, copied and counted")
{
    const std::u8string path = swd3etest::GameFile("Video/Parade.bik");

    if (swd3etest::ReadGameFile("Video/Parade.bik").empty())
    {
        std::printf("  (Parade.bik not found, skipped)\n");
        return;
    }

    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    swd3e::Movie movie;
    movie.Open(path, kVolume);
    CHECK(movie.IsOpen());
    CHECK_EQ(movie.Position(), 1);
    swd3e::Surface surface(swd3e::Display::Width, swd3e::Display::Height);
    surface.Fill(0);
    CHECK_EQ(movie.Frame(surface), 1);
    CHECK_EQ(movie.Position(), kSecondFrame);
    int lit = 0;

    for (int row = 0; row < Display::Height; ++row)
    {
        for (int column = 0; column < Display::Width; ++column)
        {
            lit += surface.Row(row)[column] != 0 ? 1 : 0;
        }
    }

    CHECK(lit > 0);
    int last = 1;
    int shown = 1;

    while (last > 0)
    {
        last = movie.Frame(surface);
        ++shown;
    }

    CHECK_EQ(last, -1);
    CHECK_EQ(movie.Position(), shown);
    CHECK(movie.Close());
    CHECK(!movie.Close());
    CHECK_EQ(movie.Position(), -1);
}

TEST_CASE("movie: a missing file keeps the error and is not open")
{
    swd3e::Movie movie;
    movie.Open(swd3etest::GameFile("Video/missing.bik"), kLowVolume);
    CHECK(!movie.Error().empty());
    CHECK(!movie.IsOpen());
    CHECK(!movie.Waiting());
    CHECK_EQ(movie.Position(), -1);
}
