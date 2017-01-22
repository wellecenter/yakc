#pragma once
//------------------------------------------------------------------------------
/**
    @class YAKC::breadboard
    @brief houses all the common chips required by emulated systems
*/
#include "yakc/core/core.h"
#include "yakc/core/clock.h"
#include "yakc/devices/z80.h"
#include "yakc/devices/mos6502.h"
#include "yakc/devices/cpudbg.h"
#include "yakc/devices/z80pio.h"
#include "yakc/devices/z80ctc.h"
#include "yakc/devices/i8255.h"
#include "yakc/devices/mc6845.h"
#include "yakc/devices/beeper.h"
#include "yakc/devices/speaker.h"
#include "yakc/devices/ay8910.h"
#include "yakc/devices/crt.h"

namespace YAKC {

class breadboard {
public:
    clock clck;
    z80 z80cpu;
    mos6502 m6502cpu;
    class z80pio z80pio;
    class z80pio z80pio2;
    class z80ctc z80ctc;
    class i8255 i8255;
    class mc6845 mc6845;
    class beeper beeper;
    class speaker speaker;
    class ay8910 ay8910;
    cpudbg dbg;
    class crt crt;          // this is not a chip, but a cathode-ray-tube emulation
    ubyte ram[8][0x4000];
};

} // namespace YAKC
