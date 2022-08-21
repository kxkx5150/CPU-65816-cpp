#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "apu.h"
#include "snes.h"
#include "spc.h"


Apu::Apu(Snes *_snes)
{
    snes = _snes;
    spc  = new SPC(this);
    dsp  = new Dsp(this);
}
Apu::~Apu()
{
    delete spc;
    delete dsp;
}
void Apu::apu_reset()
{
    romReadable = true;
    spc->spc_reset();
    dsp->dsp_reset();

    dspAdr = 0;
    cycles = 0;

    memset(ram, 0, sizeof(ram));
    memset(inPorts, 0, sizeof(inPorts));
    memset(outPorts, 0, sizeof(outPorts));

    for (int i = 0; i < 3; i++) {
        timer[i].cycles  = 0;
        timer[i].divider = 0;
        timer[i].target  = 0;
        timer[i].counter = 0;
        timer[i].enabled = false;
    }
    cpuCyclesLeft = 7;
}
void Apu::apu_cycle()
{
    if (cpuCyclesLeft == 0) {
        cpuCyclesLeft = spc->spc_runOpcode();
    }

    cpuCyclesLeft--;

    if ((cycles & 0x1f) == 0) {
        dsp->dsp_cycle();
    }

    for (int i = 0; i < 3; i++) {
        if (timer[i].cycles == 0) {
            timer[i].cycles = i == 2 ? 16 : 128;
            if (timer[i].enabled) {
                timer[i].divider++;
                if (timer[i].divider == timer[i].target) {
                    timer[i].divider = 0;
                    timer[i].counter++;
                    timer[i].counter &= 0xf;
                }
            }
        }
        timer[i].cycles--;
    }
    cycles++;
}
uint8_t Apu::apu_cpuRead(uint16_t adr)
{
    switch (adr) {
        case 0xf0:
        case 0xf1:
        case 0xfa:
        case 0xfb:
        case 0xfc: {
            return 0;
        }
        case 0xf2: {
            return dspAdr;
        }
        case 0xf3: {
            return dsp->dsp_read(dspAdr & 0x7f);
        }
        case 0xf4:
        case 0xf5:
        case 0xf6:
        case 0xf7:
        case 0xf8:
        case 0xf9: {
            return inPorts[adr - 0xf4];
        }
        case 0xfd:
        case 0xfe:
        case 0xff: {
            uint8_t ret               = timer[adr - 0xfd].counter;
            timer[adr - 0xfd].counter = 0;
            return ret;
        }
    }
    if (romReadable && adr >= 0xffc0) {
        return bootRom[adr - 0xffc0];
    }
    return ram[adr];
}
void Apu::apu_cpuWrite(uint16_t adr, uint8_t val)
{
    switch (adr) {
        case 0xf0: {
            break;
        }
        case 0xf1: {
            for (int i = 0; i < 3; i++) {
                if (!timer[i].enabled && (val & (1 << i))) {
                    timer[i].divider = 0;
                    timer[i].counter = 0;
                }
                timer[i].enabled = val & (1 << i);
            }
            if (val & 0x10) {
                inPorts[0] = 0;
                inPorts[1] = 0;
            }
            if (val & 0x20) {
                inPorts[2] = 0;
                inPorts[3] = 0;
            }
            romReadable = val & 0x80;
            break;
        }
        case 0xf2: {
            dspAdr = val;
            break;
        }
        case 0xf3: {
            if (dspAdr < 0x80)
                dsp->dsp_write(dspAdr, val);
            break;
        }
        case 0xf4:
        case 0xf5:
        case 0xf6:
        case 0xf7: {
            outPorts[adr - 0xf4] = val;
            break;
        }
        case 0xf8:
        case 0xf9: {
            inPorts[adr - 0xf4] = val;
            break;
        }
        case 0xfa:
        case 0xfb:
        case 0xfc: {
            timer[adr - 0xfa].target = val;
            break;
        }
    }
    ram[adr] = val;
}
