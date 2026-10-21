#pragma once

#include <string>

#include "core/types.h"
#include "platform/media_decoder.h"

namespace swd3e
{

class Surface;

// A Bink movie of the game (0x484 bytes in the original: the Bink handle, its 0x7C-byte summary and the error text).
class Movie
{
public:
    static constexpr int MaxVolume = 0x7F;  // Open's volume (Bink's 0x8000 at 0x7F)

    // Bink's sound output (DirectSound or the Miles digital driver) and the copy format of the back buffer.
    static void Init(bool directSound, bool digitalDriver);

    Movie();

    void Open(const std::u8string& path, int volume);
    bool Close();
    int Frame(Surface& surface);
    int Position() const;
    bool Waiting() const;
    bool IsOpen() const;
    const std::string& Error() const;

private:
    MediaDecoder decoder;
    std::string error;
};

}  // namespace swd3e
