//------------------------------------------------------------------------------
//  atom.cc
//------------------------------------------------------------------------------
#include "atom.h"

#include <stdio.h>

namespace YAKC {

atom* atom::self = nullptr;

//------------------------------------------------------------------------------
void
atom::init(breadboard* b, rom_images* r) {
    YAKC_ASSERT(b && r);
    self = this;
    this->board = b;
    this->roms = r;
    this->vdg = &(this->board->mc6847);
}

//------------------------------------------------------------------------------
bool
atom::check_roms(const rom_images& roms, device model, os_rom os) {
    if (device::acorn_atom == model) {
        return roms.has(rom_images::atom_kernel) &&
               roms.has(rom_images::atom_float) &&
               roms.has(rom_images::atom_basic) &&
               roms.has(rom_images::atom_dos);
    }
    else {
        return false;
    }
}

//------------------------------------------------------------------------------
void
atom::on_context_switched() {
    // FIXME
}

//------------------------------------------------------------------------------
void
atom::poweron() {
    YAKC_ASSERT(this->board && this->roms);
    YAKC_ASSERT(!this->on);
    
    this->on = true;

    // map memory
    fill_random(this->board->ram[1], sizeof(this->board->ram[1]));
    fill_random(this->board->ram[2], sizeof(this->board->ram[2]));
    auto& mem = this->board->mos6502.mem;
    mem.unmap_all();
    mem.map(0, 0x0000, 0x0400, this->board->ram[0], true);
    mem.map(0, 0x2000, 0x8000, this->board->ram[1], true);
    mem.map_io(0, 0xB000, 0x1000, memio);
    mem.map(0, 0xC000, 0x1000, this->roms->ptr(rom_images::atom_basic), false);
    mem.map(0, 0xD000, 0x1000, this->roms->ptr(rom_images::atom_float), false);
    mem.map(0, 0xE000, 0x1000, this->roms->ptr(rom_images::atom_dos), false);
    mem.map(0, 0xF000, 0x1000, this->roms->ptr(rom_images::atom_kernel), false);
    this->vidmem_base = mem.read_ptr(0x8000);

    this->board->clck.init(1000);
    this->board->mos6502.init(this);
    this->board->mos6502.reset();
    this->board->i8255.init(0);
    this->vdg->init(read_vidmem, this->board->rgba8_buffer, 1000);
}

//------------------------------------------------------------------------------
void
atom::poweroff() {
    YAKC_ASSERT(this->on);
    this->board->mos6502.mem.unmap_all();
    this->on = false;
}

//------------------------------------------------------------------------------
void
atom::reset() {
    this->board->mos6502.reset();
    this->board->i8255.reset();
}

//------------------------------------------------------------------------------
uint64_t
atom::step(uint64_t start_tick, uint64_t end_tick) {
    auto& cpu = this->board->mos6502;
    auto& dbg = this->board->dbg;
    uint64_t cur_tick = start_tick;
    while (cur_tick < end_tick) {
        if (dbg.check_break(cpu.PC)) {
            dbg.paused = true;
            return end_tick;
        }
        dbg.store_pc_history(cpu.PC); // FIXME: only if debug window open?    
        cur_tick += cpu.step();
    }
    return cur_tick;
}

//------------------------------------------------------------------------------
void
atom::put_input(uint8_t ascii, uint8_t joy0_mask) {
    // FIXME
}

//------------------------------------------------------------------------------
void
atom::cpu_tick() {
    this->vdg->step();
}

//------------------------------------------------------------------------------
uint8_t
atom::memio(bool write, uint16_t addr, uint8_t inval) {
    if ((addr >= 0xB000) && (addr < 0xB400)) {
        if (write) {
            self->board->i8255.write(self, addr & 0x0003, inval);
        }
        else {
            return self->board->i8255.read(self, addr & 0x0003);
        }
    }
    else if ((addr >= 0xB800) && (addr < 0xBC00)) {
        printf("VIA: addr=%04X %s %02X\n", addr, write ? "write":"read", inval);
    }
    else {
        printf("UNKNOWN: addr=%04X %s %02X\n", addr, write ? "write":"read", inval);
    }

    // FIXME
    return 0x00;
}

//------------------------------------------------------------------------------
uint8_t
atom::read_vidmem(uint16_t addr) {
    uint8_t chr = self->vidmem_base[addr];
    self->vdg->inv(chr & (1<<7));
    self->vdg->as(chr & (1<<6));
    self->vdg->int_ext(chr & (1<<6));
    return chr;
}

//------------------------------------------------------------------------------
void
atom::pio_out(int pio_id, int port_id, uint8_t val) {
    /*
    FROM Atom Theory and Praxis (and MAME)

    The  8255  Programmable  Peripheral  Interface  Adapter  contains  three
    8-bit ports, and all but one of these lines is used by the ATOM.

    Port A - #B000
           Output bits:      Function:
                O -- 3     Keyboard row
                4 -- 7     Graphics mode (4: A/G, 5..7: GM0..2)

    Port B - #B001
           Input bits:       Function:
                O -- 5     Keyboard column
                  6        CTRL key (low when pressed)
                  7        SHIFT keys {low when pressed)

    Port C - #B002
           Output bits:      Function:
                O          Tape output
                1          Enable 2.4 kHz to cassette output
                2          Loudspeaker
                3          Not used

           Input bits:       Function:
                4          2.4 kHz input
                5          Cassette input
                6          REPT key (low when pressed)
                7          60 Hz sync signal (low during flyback)

    The port C output lines, bits O to 3, may be used for user
    applications when the cassette interface is not being used.
    */
    if (0 == port_id) {
        // PPIA Port A
        this->vdg->ag(val & (1<<4));
        this->vdg->gm0(val & (1<<5));
        this->vdg->gm1(val & (1<<6));
        this->vdg->gm2(val & (1<<7));
    }

//    printf("PPIA OUT: port %d val %02X\n", port_id, val);
}

//------------------------------------------------------------------------------
uint8_t
atom::pio_in(int pio_id, int port_id) {
    // quick'n'dirty vsync flag impl
    if (port_id == 2) {
        uint8_t val = 0x00;
        // fsync flag, this is off during the FSYNC, and on otherwise
        if (!this->board->mc6847.test(mc6847::FSYNC)) {
            val |= (1<<7);
        }
        return val;
    }
    else {
        return 0xFF;
    }
}

//------------------------------------------------------------------------------
const void*
atom::framebuffer(int& out_width, int& out_height) {
    out_width = mc6847::disp_width;
    out_height = mc6847::disp_height;
    return this->board->mc6847.rgba8_buffer;
}

//------------------------------------------------------------------------------
const char*
atom::system_info() const {
    return "FIXME!";
}

} // namespace YAKC
