#pragma once

#if DYNO_ARCH_X86 == 32
#include "x86_detour.h"
#elif DYNO_ARCH_X86 == 64
#include "x64_detour.h"
#endif

namespace dyno {
#if DYNO_ARCH_X86 == 32
    using NatDetour = x86Detour;
#elif DYNO_ARCH_X86 == 64
    using NatDetour = x64Detour;
#endif
}