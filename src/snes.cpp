#include <stdint.h>
#include <stdlib.h>
#include "../allegro.h"
#include "snes.h"


SNES::SNES()
{
    cpu = new CPU(this);
    ppu = new PPU(this);
    spc = new SPC(this);
    dsp = new DSP(this);
    mem = new MEM(this);
    io  = new IO(this);
}
SNES::~SNES()
{
    delete cpu;
    delete ppu;
    delete spc;
    delete dsp;
    delete mem;
    delete io;
}
void oncesec()
{
}
void hz60()
{
}
volatile int close_button_pressed = FALSE;

void close_button_handler(void)
{
    close_button_pressed = TRUE;
}
END_OF_FUNCTION(close_button_handler)
void SNES::execframe()
{
    nmi = vbl = 0;
    io->framenum++;

    for (lines = 0; lines < (262); lines++) {
        if ((irqenable == 2) && (lines == yirq)) {
            irq = 1;
        }
        if (lines < 225)
            ppu->drawline(lines);
        cpu->cycles += 1364;
        io->intthisline = 0;

        while (cpu->cycles > 0) {
            cpu->exec();
            if ((((irqenable == 3) && (lines == yirq)) || (irqenable == 1)) && !io->intthisline) {
                if (((1364 - cpu->cycles) >> 2) >= xirq) {
                    irq             = 1;
                    io->intthisline = 1;
                }
            }
            if (oldnmi != nmi && nmienable && nmi)
                cpu->nmi65c816();
            else if (irq && (!cpu->p.i || cpu->inwai))
                cpu->irq65c816();
            oldnmi = nmi;
        }
        if (lines == 0xE0)
            nmi = 1;
        if (lines == 0xE0) {
            vbl = joyscan = 1;
            io->readjoy();
        }
        if (lines == 0xE3)
            joyscan = 0;
        if (lines == 200 && key[KEY_ESC])
            break;
        if (close_button_pressed)
            exit(1);
    }
}
void SNES::initsnem()
{
    mem->allocmem();
    ppu->initppu();
    spc->initspc();
    cpu->makeopcodetable();
    install_keyboard();
    install_timer();
    install_int_ex(oncesec, MSEC_TO_TIMER(1000));
    LOCK_FUNCTION(close_button_handler);
    set_close_button_callback(close_button_handler);
    dsp->initdsp();
}
void SNES::resetsnem()
{
    ppu->resetppu();
    spc->resetspc();
    dsp->resetdsp();
    cpu->reset65c816();
    // install_int_ex(hz60, BPS_TO_TIMER(60));
}

void SNES::clockspc(int cyc)
{
    spc->spccycles += cyc;
    if (spc->spccycles > 0)
        spc->execspc();
}
uint8_t SNES::readmem(uint32_t a)
{
    cpu->cycles -= mem->accessspeed[(a >> 13) & 0x7FF];
    clockspc(mem->accessspeed[(a >> 13) & 0x7FF]);
    if (mem->memread[(a >> 13) & 0x7FF]) {
        return mem->memlookup[(a >> 13) & 0x7FF][a & 0x1FFF];
    }
    return mem->readmeml(a);
}
void SNES::writemem(uint32_t ad, uint8_t v)
{
    cpu->cycles -= mem->accessspeed[(ad >> 13) & 0x7FF];
    clockspc(mem->accessspeed[(ad >> 13) & 0x7FF]);
    if (mem->memwrite[(ad >> 13) & 0x7FF])
        mem->memlookup[(ad >> 13) & 0x7FF][(ad)&0x1FFF] = v;
    else
        mem->writememl(ad, v);
}
uint16_t SNES::readmemw(uint32_t a)
{
    return readmem(a) | ((readmem((a) + 1)) << 8);
}
void SNES::writememw(uint32_t ad, uint16_t v)
{
    writemem(ad, v & 0xFF);
    writemem(ad + 1, v >> 8);
}
void SNES::writememw2(uint32_t ad, uint16_t v)
{
    writemem(ad + 1, v >> 8);
    writemem(ad, (v)&0xFF);
}
void SNES::loadrom(char *fn)
{
    mem->loadrom(fn);
    resetsnem();
}