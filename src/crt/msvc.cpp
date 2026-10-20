#include "crt/msvc.h"

#include <cmath>
#include <cstdlib>
#include <ctime>

namespace swd3e::crt
{

namespace
{

constexpr int kRandMask = 0x7FFF;

}  // namespace

// swd3e.exe 0x004A10E8: _ftol
i64 Ftol(double value)
{
    return static_cast<i64>(std::llrint(std::trunc(value)));
}

// swd3e.exe 0x004A1B81: floor with its math error handlers below
// swd32.exe 0x0048F9A0 (unmatched in swd3e): _frnd, frndint in the current rounding mode (inline here)
// swd32.exe 0x0048F9C0 (unmatched in swd3e): _handle_qnan1, a quiet NaN argument (EDOM) (inline here)
// swd32.exe 0x0048FA20 (unmatched in swd3e): _except1, a floating exception of a math function (inline here)
// swd3e.exe 0x004A46AD: _raise_exc, the IEEE exception record raised (inline here)
// swd32.exe 0x0048FFB0 (unmatched in swd3e): _handle_exc, the masked exceptions handled (inline here)
// swd32.exe 0x00490330 (unmatched in swd3e): _umatherr, the user _matherr called (inline here)
// swd32.exe 0x004903D0 (unmatched in swd3e): _set_errno, EDOM or ERANGE (inline here)
// swd32.exe 0x00490410 (unmatched in swd3e): _getfpename, the function name from the table at 0x004A85E0 (inline here)
// swd32.exe 0x00490450 (unmatched in swd3e): _errcode, the _matherr type of the exception flags (inline here)
// swd32.exe 0x004904D0 (unmatched in swd3e): _set_exp, a double with its exponent replaced (inline here)
// swd32.exe 0x00490510 (unmatched in swd3e): _sptype, infinity or NaN kind of a double (inline here)
// swd32.exe 0x00490590 (unmatched in swd3e): _decomp, mantissa and exponent of a double (inline here)
// swd32.exe 0x004906D0 (unmatched in swd3e): _statfp, the FPU status word (inline here)
// swd32.exe 0x004906F0 (unmatched in swd3e): _clrfp, the FPU status word read and cleared (inline here)
// swd32.exe 0x00490710 (unmatched in swd3e): _ctrlfp, the FPU control word set under a mask (inline here)
// swd3e.exe 0x004A4E0D: _set_statfp, an exception flag raised (inline here)
// swd32.exe 0x00494800 (unmatched in swd3e): _matherr, the default handler (0: not handled) (inline here)
double Floor(double value)
{
    return std::floor(value);
}

// swd3e.exe 0x004A150A: difftime
double Difftime(i32 end, i32 start)
{
    return std::difftime(static_cast<std::time_t>(end), static_cast<std::time_t>(start));
}

// swd3e.exe 0x004A151B: time
i32 Time()
{
    return static_cast<i32>(std::time(nullptr));
}

// swd3e.exe 0x004A14E2: srand
void Random::Seed(u32 seed)
{
    std::srand(seed);
}

// swd3e.exe 0x004A14EC: rand, in the 15 bits the game's arithmetic assumes (RAND_MAX of the Windows runtime)
int Random::Next()
{
    return std::rand() & kRandMask;
}

}  // namespace swd3e::crt
