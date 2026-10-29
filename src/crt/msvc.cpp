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
// swd3e.exe 0x004A45C2: _math_entry, the control word saved and set for the function (inline here)
// swd3e.exe 0x004A4615: _handle_exc, the masked exceptions handled (inline here)
// swd3e.exe 0x004A46AD: _raise_exc, the IEEE exception record raised (inline here)
// swd3e.exe 0x004A4960: _umatherr, the user _matherr called (inline here)
// swd3e.exe 0x004A4B77: _87except, a floating exception of a math function (inline here)
// swd3e.exe 0x004A4BFF: _errcode, the _matherr type of the exception flags (inline here)
// swd3e.exe 0x004A4C25: _trandisp_lookup, the function's name and error table entry (inline here)
// swd3e.exe 0x004A4C4A: _get_fpe_type, the exception kind of the flags (inline here)
// swd3e.exe 0x004A4C77: _frnd, frndint in the current rounding mode (inline here)
// swd3e.exe 0x004A4C89: _set_exp, a double with its exponent replaced (inline here)
// swd3e.exe 0x004A4CB2: _sptype, infinity or NaN kind of a double (inline here)
// swd3e.exe 0x004A4D0C: _decomp, mantissa and exponent of a double (inline here)
// swd3e.exe 0x004A4DCD: _statfp, the FPU status word (inline here)
// swd3e.exe 0x004A4DDB: _clrfp, the FPU status word read and cleared (inline here)
// swd3e.exe 0x004A4DEA: _ctrlfp, the FPU control word set under a mask (inline here)
// swd3e.exe 0x004A4E0D: _set_statfp, an exception flag raised (inline here)
// swd3e.exe 0x004A7185: _matherr, the default handler (0: not handled) (inline here)
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
