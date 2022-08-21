#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "cart.h"
#include "input.h"
#include "snes.h"
#include "spc.h"


Snes::Snes()
{
    cpu    = new CPU(this);
    dma    = new Dma(this);
    cart   = new Cart(this);
    apu    = new Apu(this);
    input1 = new Input(this);
    input2 = new Input(this);
    ppu    = new Ppu(this);
}
Snes::~Snes()
{
    delete cpu;
    delete dma;
    delete cart;
    delete apu;
    delete input1;
    delete input2;
    delete ppu;
}
void Snes::snes_reset(bool hard)
{
    cart->cart_reset();
    cpu->cpu_reset();
    apu->apu_reset();
    dma->dma_reset();
    ppu->ppu_reset();
    input1->input_reset();
    input2->input_reset();

    if (hard)
        memset(ram, 0, sizeof(ram));

    ramAdr           = 0;
    hPos             = 0;
    vPos             = 0;
    frames           = 0;
    cpuCyclesLeft    = 52;
    cpuMemOps        = 0;
    apuCatchupCycles = 0.0;
    hIrqEnabled      = false;
    vIrqEnabled      = false;
    nmiEnabled       = false;
    hTimer           = 0x1ff;
    vTimer           = 0x1ff;
    inNmi            = false;
    inIrq            = false;
    inVblank         = false;

    memset(portAutoRead, 0, sizeof(portAutoRead));

    autoJoyRead    = false;
    autoJoyTimer   = 0;
    ppuLatch       = false;
    multiplyA      = 0xff;
    multiplyResult = 0xfe01;
    divideA        = 0xffff;
    divideResult   = 0x101;
    fastMem        = false;
    openBus        = 0;
}
void Snes::snes_runFrame()
{
    do {
        snes_runCycle();
    } while (!(hPos == 0 && vPos == 0));
}
void Snes::snes_runCycle()
{
    apuCatchupCycles += apuCyclesPerMaster * 2.0;
    input1->input_cycle();
    input2->input_cycle();

    if (hPos < 536 || hPos >= 576) {
        if (!dma->dma_cycle()) {
            snes_runCpu();
        }
    }

    if (vIrqEnabled && hIrqEnabled) {
        if (vPos == vTimer && hPos == (4 * hTimer)) {
            inIrq          = true;
            cpu->irqWanted = true;
        }
    } else if (vIrqEnabled && !hIrqEnabled) {
        if (vPos == vTimer && hPos == 0) {
            inIrq          = true;
            cpu->irqWanted = true;
        }
    } else if (!vIrqEnabled && hIrqEnabled) {
        if (hPos == (4 * hTimer)) {
            inIrq          = true;
            cpu->irqWanted = true;
        }
    }

    if (hPos == 0) {
        bool startingVblank = false;
        if (vPos == 0) {
            inVblank = false;
            inNmi    = false;
            dma->dma_initHdma();
        } else if (vPos == 225) {
            startingVblank = !ppu->ppu_checkOverscan();
        } else if (vPos == 240) {
            if (!inVblank)
                startingVblank = true;
        }

        if (startingVblank) {
            ppu->ppu_handleVblank();
            inVblank = true;
            inNmi    = true;
            if (autoJoyRead) {
                autoJoyTimer = 4224;
                snes_doAutoJoypad();
            }
            if (nmiEnabled) {
                cpu->nmiWanted = true;
            }
        }
    } else if (hPos == 512) {
        if (!inVblank)
            ppu->ppu_runLine(vPos);
    } else if (hPos == 1024) {
        if (!inVblank)
            dma->dma_doHdma();
    }
    if (autoJoyTimer > 0)
        autoJoyTimer -= 2;

    hPos += 2;

    if (hPos == 1364) {
        hPos = 0;
        vPos++;

        if (vPos == 262) {
            vPos = 0;
            frames++;
            snes_catchupApu();
        }
    }
}
void Snes::snes_runCpu()
{
    if (cpuCyclesLeft == 0) {
        cpuMemOps  = 0;
        int cycles = cpu->cpu_runOpcode();
        cpuCyclesLeft += (cycles - cpuMemOps) * 6;
    }

    cpuCyclesLeft -= 2;
}
void Snes::snes_catchupApu()
{
    int catchupCycles = (int)apuCatchupCycles;
    for (int i = 0; i < catchupCycles; i++) {
        apu->apu_cycle();
    }

    apuCatchupCycles -= (double)catchupCycles;
}
void Snes::snes_doAutoJoypad()
{
    memset(portAutoRead, 0, sizeof(portAutoRead));
    input1->latchLine = true;
    input2->latchLine = true;
    input1->input_cycle();
    input1->input_cycle();
    input1->latchLine = false;
    input2->latchLine = false;

    for (int i = 0; i < 16; i++) {
        uint8_t val = input1->input_read();
        portAutoRead[0] |= ((val & 1) << (15 - i));
        portAutoRead[2] |= (((val >> 1) & 1) << (15 - i));
        val = input2->input_read();
        portAutoRead[1] |= ((val & 1) << (15 - i));
        portAutoRead[3] |= (((val >> 1) & 1) << (15 - i));
    }
}
uint8_t Snes::snes_readBBus(uint8_t adr)
{
    if (adr < 0x40) {
        return ppu->ppu_read(adr);
    }

    if (adr < 0x80) {
        snes_catchupApu();
        return apu->outPorts[adr & 0x3];
    }

    if (adr == 0x80) {
        uint8_t ret = ram[ramAdr++];
        ramAdr &= 0x1ffff;
        return ret;
    }

    return openBus;
}
void Snes::snes_writeBBus(uint8_t adr, uint8_t val)
{
    if (adr < 0x40) {
        ppu->ppu_write(adr, val);
        return;
    }

    if (adr < 0x80) {
        snes_catchupApu();
        apu->inPorts[adr & 0x3] = val;
        return;
    }

    switch (adr) {
        case 0x80: {
            ram[ramAdr++] = val;
            ramAdr &= 0x1ffff;
            break;
        }
        case 0x81: {
            ramAdr = (ramAdr & 0x1ff00) | val;
            break;
        }
        case 0x82: {
            ramAdr = (ramAdr & 0x100ff) | (val << 8);
            break;
        }
        case 0x83: {
            ramAdr = (ramAdr & 0x0ffff) | ((val & 1) << 16);
            break;
        }
    }
}
uint8_t Snes::snes_readReg(uint16_t adr)
{
    switch (adr) {
        case 0x4210: {
            uint8_t val = 0x2;
            val |= inNmi << 7;
            inNmi = false;
            return val | (openBus & 0x70);
        }
        case 0x4211: {
            uint8_t val    = inIrq << 7;
            inIrq          = false;
            cpu->irqWanted = false;
            return val | (openBus & 0x7f);
        }
        case 0x4212: {
            uint8_t val = (autoJoyTimer > 0);
            val |= (hPos >= 1024) << 6;
            val |= inVblank << 7;
            return val | (openBus & 0x3e);
        }
        case 0x4213: {
            return ppuLatch << 7;
        }
        case 0x4214: {
            return divideResult & 0xff;
        }
        case 0x4215: {
            return divideResult >> 8;
        }
        case 0x4216: {
            return multiplyResult & 0xff;
        }
        case 0x4217: {
            return multiplyResult >> 8;
        }
        case 0x4218:
        case 0x421a:
        case 0x421c:
        case 0x421e: {
            return portAutoRead[(adr - 0x4218) / 2] & 0xff;
        }
        case 0x4219:
        case 0x421b:
        case 0x421d:
        case 0x421f: {
            return portAutoRead[(adr - 0x4219) / 2] >> 8;
        }
        default: {
            return openBus;
        }
    }
}
void Snes::snes_writeReg(uint16_t adr, uint8_t val)
{
    switch (adr) {
        case 0x4200: {
            autoJoyRead = val & 0x1;
            if (!autoJoyRead)
                autoJoyTimer = 0;
            hIrqEnabled = val & 0x10;
            vIrqEnabled = val & 0x20;
            nmiEnabled  = val & 0x80;
            if (!hIrqEnabled && !vIrqEnabled) {
                inIrq          = false;
                cpu->irqWanted = false;
            }
            break;
        }
        case 0x4201: {
            if (!(val & 0x80) && ppuLatch) {
                ppu->ppu_read(0x37);
            }
            ppuLatch = val & 0x80;
            break;
        }
        case 0x4202: {
            multiplyA = val;
            break;
        }
        case 0x4203: {
            multiplyResult = multiplyA * val;
            break;
        }
        case 0x4204: {
            divideA = (divideA & 0xff00) | val;
            break;
        }
        case 0x4205: {
            divideA = (divideA & 0x00ff) | (val << 8);
            break;
        }
        case 0x4206: {
            if (val == 0) {
                divideResult   = 0xffff;
                multiplyResult = divideA;
            } else {
                divideResult   = divideA / val;
                multiplyResult = divideA % val;
            }
            break;
        }
        case 0x4207: {
            hTimer = (hTimer & 0x100) | val;
            break;
        }
        case 0x4208: {
            hTimer = (hTimer & 0x0ff) | ((val & 1) << 8);
            break;
        }
        case 0x4209: {
            vTimer = (vTimer & 0x100) | val;
            break;
        }
        case 0x420a: {
            vTimer = (vTimer & 0x0ff) | ((val & 1) << 8);
            break;
        }
        case 0x420b: {
            dma->dma_startDma(val, false);
            break;
        }
        case 0x420c: {
            dma->dma_startDma(val, true);
            break;
        }
        case 0x420d: {
            fastMem = val & 0x1;
            break;
        }
        default: {
            break;
        }
    }
}
uint8_t Snes::snes_rread(uint32_t adr)
{
    uint8_t bank = adr >> 16;
    adr &= 0xffff;

    if (bank == 0x7e || bank == 0x7f) {
        return ram[((bank & 1) << 16) | adr];
    }

    if (bank < 0x40 || (bank >= 0x80 && bank < 0xc0)) {
        if (adr < 0x2000) {
            return ram[adr];
        }
        if (adr >= 0x2100 && adr < 0x2200) {
            return snes_readBBus(adr & 0xff);
        }
        if (adr == 0x4016) {
            return input1->input_read() | (openBus & 0xfc);
        }
        if (adr == 0x4017) {
            return input2->input_read() | (openBus & 0xe0) | 0x1c;
        }
        if (adr >= 0x4200 && adr < 0x4220) {
            return snes_readReg(adr);
        }
        if (adr >= 0x4300 && adr < 0x4380) {
            return dma->dma_read(adr);
        }
    }

    return cart->cart_read(bank, adr);
}
void Snes::snes_write(uint32_t adr, uint8_t val)
{
    openBus      = val;
    uint8_t bank = adr >> 16;
    adr &= 0xffff;

    if (bank == 0x7e || bank == 0x7f) {
        ram[((bank & 1) << 16) | adr] = val;
    }

    if (bank < 0x40 || (bank >= 0x80 && bank < 0xc0)) {
        if (adr < 0x2000) {
            ram[adr] = val;
        }

        if (adr >= 0x2100 && adr < 0x2200) {
            snes_writeBBus(adr & 0xff, val);
        }

        if (adr == 0x4016) {
            input1->latchLine = val & 1;
            input2->latchLine = val & 1;
        }

        if (adr >= 0x4200 && adr < 0x4220) {
            snes_writeReg(adr, val);
        }

        if (adr >= 0x4300 && adr < 0x4380) {
            dma->dma_write(adr, val);
        }
    }

    cart->cart_write(bank, adr, val);
}
int Snes::snes_getAccessTime(uint32_t adr)
{
    uint8_t bank = adr >> 16;
    adr &= 0xffff;

    if (bank >= 0x40 && bank < 0x80) {
        return 8;
    }

    if (bank >= 0xc0) {
        return fastMem ? 6 : 8;
    }

    if (adr < 0x2000) {
        return 8;
    }

    if (adr < 0x4000) {
        return 6;
    }

    if (adr < 0x4200) {
        return 12;
    }

    if (adr < 0x6000) {
        return 6;
    }

    if (adr < 0x8000) {
        return 8;
    }

    return (fastMem && bank >= 0x80) ? 6 : 8;
}
uint8_t Snes::snes_read(uint32_t adr)
{
    uint8_t val = snes_rread(adr);
    openBus     = val;
    return val;
}
uint8_t Snes::snes_cpuRead(uint32_t adr)
{
    cpuMemOps++;
    cpuCyclesLeft += snes_getAccessTime(adr);
    return snes_read(adr);
}
void Snes::snes_cpuWrite(uint32_t adr, uint8_t val)
{
    cpuMemOps++;
    cpuCyclesLeft += snes_getAccessTime(adr);
    snes_write(adr, val);
}
