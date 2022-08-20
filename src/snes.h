#ifndef _SNEM_H_
#define _SNEM_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "cpu.h"
#include "ppu.h"
#include "spc.h"
#include "dsp.h"
#include "mem.h"
#include "io.h"

class SNES {
  public:
    CPU *cpu = nullptr;
    PPU *ppu = nullptr;
    SPC *spc = nullptr;
    DSP *dsp = nullptr;
    MEM *mem = nullptr;
    IO  *io  = nullptr;

    int nmi, vbl, joyscan;
    int yirq, xirq, irqenable, irq;
    int lines;
    int oldnmi = 0;
    int nmienable;

  public:
    SNES();
    ~SNES();

    void loadrom(char *fn);

    void execframe();
    void initsnem();
    void resetsnem();

    void clockspc(int cyc);

    uint8_t  readmem(uint32_t a);
    void     writemem(uint32_t ad, uint8_t v);
    uint16_t readmemw(uint32_t a);
    void     writememw(uint32_t ad, uint16_t v);
    void     writememw2(uint32_t ad, uint16_t v);
};
#endif
