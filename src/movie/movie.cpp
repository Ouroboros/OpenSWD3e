#include "movie/movie.h"

#include <algorithm>

namespace swd3e
{

// swd3e.exe 0x0049C440: the decoder plays the sound on its own output and copies RGB565 frames
void Movie::Init(bool, bool)
{
}

// swd3e.exe 0x0049C480
Movie::Movie() = default;

// swd3e.exe 0x0049C490: a failed open keeps Bink's error text and a zero summary; the volume is held within 0..0x7F
void Movie::Open(const std::u8string& path, int volume)
{
    constexpr int kBinkVolumeShift = 15;

    if (!this->decoder.Open(path))
    {
        this->error = this->decoder.Error();
    }

    const int held = std::clamp(volume, 0, Movie::MaxVolume);
    this->decoder.SetVolume((held << kBinkVolumeShift) / Movie::MaxVolume);
}

// swd3e.exe 0x0049C520
bool Movie::Close()
{
    if (!this->decoder.IsOpen())
    {
        return false;
    }

    this->decoder.Close();
    return true;
}

// swd3e.exe 0x0049C550: the current frame copied to the top left of `surface` (its first 480 rows) and the next one
// taken: 1 while frames are left, -1 once the last one is shown
int Movie::Frame(Surface& surface)
{
    this->decoder.DecodeFrame();
    this->decoder.CopyTo(surface, 0, 0);
    int result = -1;

    if (this->decoder.Frames() > this->decoder.FrameNum())
    {
        result = 1;
        this->decoder.NextFrame();
    }

    return result;
}

// A movie is open (Bink gave a handle).
bool Movie::IsOpen() const
{
    return this->decoder.IsOpen();
}

// swd3e.exe 0x0049C5C0 (a chunk of 0x0049C890, Game::VideoPosition): the current frame, negated past the end; -1
// without a handle
int Movie::Position() const
{
    if (!this->decoder.IsOpen())
    {
        return -1;
    }

    const u32 frame = this->decoder.FrameNum();

    if (frame > this->decoder.Frames())
    {
        return -static_cast<int>(frame);
    }

    return static_cast<int>(frame);
}

// swd3e.exe 0x0049C5E0
bool Movie::Waiting() const
{
    return this->decoder.IsOpen() && this->decoder.Wait();
}

const std::string& Movie::Error() const
{
    return this->error;
}

}  // namespace swd3e
