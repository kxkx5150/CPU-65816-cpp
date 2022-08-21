#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "dma.h"
#include "snes.h"


Dma::Dma(Snes *_snes)
{
    snes = _snes;
}
void Dma::dma_reset()
{
    for (int i = 0; i < 8; i++) {
        channel[i].bAdr       = 0xff;
        channel[i].aAdr       = 0xffff;
        channel[i].aBank      = 0xff;
        channel[i].size       = 0xffff;
        channel[i].indBank    = 0xff;
        channel[i].tableAdr   = 0xffff;
        channel[i].repCount   = 0xff;
        channel[i].unusedByte = 0xff;
        channel[i].dmaActive  = false;
        channel[i].hdmaActive = false;
        channel[i].mode       = 7;
        channel[i].fixed      = true;
        channel[i].decrement  = true;
        channel[i].indirect   = true;
        channel[i].fromB      = true;
        channel[i].unusedBit  = true;
        channel[i].doTransfer = false;
        channel[i].terminated = false;
        channel[i].offIndex   = 0;
    }

    hdmaTimer = 0;
    dmaTimer  = 0;
    dmaBusy   = false;
}
uint8_t Dma::dma_read(uint16_t adr)
{
    uint8_t c = (adr & 0x70) >> 4;
    switch (adr & 0xf) {
        case 0x0: {
            uint8_t val = channel[c].mode;
            val |= channel[c].fixed << 3;
            val |= channel[c].decrement << 4;
            val |= channel[c].unusedBit << 5;
            val |= channel[c].indirect << 6;
            val |= channel[c].fromB << 7;
            return val;
        }
        case 0x1: {
            return channel[c].bAdr;
        }
        case 0x2: {
            return channel[c].aAdr & 0xff;
        }
        case 0x3: {
            return channel[c].aAdr >> 8;
        }
        case 0x4: {
            return channel[c].aBank;
        }
        case 0x5: {
            return channel[c].size & 0xff;
        }
        case 0x6: {
            return channel[c].size >> 8;
        }
        case 0x7: {
            return channel[c].indBank;
        }
        case 0x8: {
            return channel[c].tableAdr & 0xff;
        }
        case 0x9: {
            return channel[c].tableAdr >> 8;
        }
        case 0xa: {
            return channel[c].repCount;
        }
        case 0xb:
        case 0xf: {
            return channel[c].unusedByte;
        }
        default: {
            return snes->openBus;
        }
    }
}
void Dma::dma_write(uint16_t adr, uint8_t val)
{
    uint8_t c = (adr & 0x70) >> 4;
    switch (adr & 0xf) {
        case 0x0: {
            channel[c].mode      = val & 0x7;
            channel[c].fixed     = val & 0x8;
            channel[c].decrement = val & 0x10;
            channel[c].unusedBit = val & 0x20;
            channel[c].indirect  = val & 0x40;
            channel[c].fromB     = val & 0x80;
            break;
        }
        case 0x1: {
            channel[c].bAdr = val;
            break;
        }
        case 0x2: {
            channel[c].aAdr = (channel[c].aAdr & 0xff00) | val;
            break;
        }
        case 0x3: {
            channel[c].aAdr = (channel[c].aAdr & 0xff) | (val << 8);
            break;
        }
        case 0x4: {
            channel[c].aBank = val;
            break;
        }
        case 0x5: {
            channel[c].size = (channel[c].size & 0xff00) | val;
            break;
        }
        case 0x6: {
            channel[c].size = (channel[c].size & 0xff) | (val << 8);
            break;
        }
        case 0x7: {
            channel[c].indBank = val;
            break;
        }
        case 0x8: {
            channel[c].tableAdr = (channel[c].tableAdr & 0xff00) | val;
            break;
        }
        case 0x9: {
            channel[c].tableAdr = (channel[c].tableAdr & 0xff) | (val << 8);
            break;
        }
        case 0xa: {
            channel[c].repCount = val;
            break;
        }
        case 0xb:
        case 0xf: {
            channel[c].unusedByte = val;
            break;
        }
        default: {
            break;
        }
    }
}
void Dma::dma_doDma()
{
    if (dmaTimer > 0) {
        dmaTimer -= 2;
        return;
    }

    int i = 0;

    for (i = 0; i < 8; i++) {
        if (channel[i].dmaActive) {
            break;
        }
    }
    if (i == 8) {
        dmaBusy = false;
        return;
    }
    dma_transferByte(channel[i].aAdr, channel[i].aBank,
                     channel[i].bAdr + bAdrOffsets[channel[i].mode][channel[i].offIndex++], channel[i].fromB);

    channel[i].offIndex &= 3;
    dmaTimer += 6;
    if (!channel[i].fixed) {
        channel[i].aAdr += channel[i].decrement ? -1 : 1;
    }

    channel[i].size--;
    if (channel[i].size == 0) {
        channel[i].offIndex  = 0;
        channel[i].dmaActive = false;
        dmaTimer += 8;
    }
}
void Dma::dma_initHdma()
{
    hdmaTimer         = 0;
    bool hdmaHappened = false;
    for (int i = 0; i < 8; i++) {
        if (channel[i].hdmaActive) {
            hdmaHappened         = true;
            channel[i].dmaActive = false;
            channel[i].offIndex  = 0;
            channel[i].tableAdr  = channel[i].aAdr;
            channel[i].repCount  = snes->snes_read((channel[i].aBank << 16) | channel[i].tableAdr++);
            hdmaTimer += 8;

            if (channel[i].indirect) {
                channel[i].size = snes->snes_read((channel[i].aBank << 16) | channel[i].tableAdr++);
                channel[i].size |= snes->snes_read((channel[i].aBank << 16) | channel[i].tableAdr++) << 8;
                hdmaTimer += 16;
            }
            channel[i].doTransfer = true;
        } else {
            channel[i].doTransfer = false;
        }
        channel[i].terminated = false;
    }
    if (hdmaHappened)
        hdmaTimer += 16;
}
void Dma::dma_doHdma()
{
    hdmaTimer         = 0;
    bool hdmaHappened = false;
    for (int i = 0; i < 8; i++) {
        if (channel[i].hdmaActive && !channel[i].terminated) {
            hdmaHappened         = true;
            channel[i].dmaActive = false;
            channel[i].offIndex  = 0;
            hdmaTimer += 8;

            if (channel[i].doTransfer) {
                for (int j = 0; j < transferLength[channel[i].mode]; j++) {
                    hdmaTimer += 8;
                    if (channel[i].indirect) {
                        dma_transferByte(channel[i].size++, channel[i].indBank,
                                         channel[i].bAdr + bAdrOffsets[channel[i].mode][j], channel[i].fromB);
                    } else {
                        dma_transferByte(channel[i].tableAdr++, channel[i].aBank,
                                         channel[i].bAdr + bAdrOffsets[channel[i].mode][j], channel[i].fromB);
                    }
                }
            }
            channel[i].repCount--;
            channel[i].doTransfer = channel[i].repCount & 0x80;

            if ((channel[i].repCount & 0x7f) == 0) {
                channel[i].repCount = snes->snes_read((channel[i].aBank << 16) | channel[i].tableAdr++);
                if (channel[i].indirect) {
                    channel[i].size = snes->snes_read((channel[i].aBank << 16) | channel[i].tableAdr++);
                    channel[i].size |= snes->snes_read((channel[i].aBank << 16) | channel[i].tableAdr++) << 8;
                    hdmaTimer += 16;
                }
                if (channel[i].repCount == 0)
                    channel[i].terminated = true;
                channel[i].doTransfer = true;
            }
        }
    }
    if (hdmaHappened)
        hdmaTimer += 16;
}
void Dma::dma_transferByte(uint16_t aAdr, uint8_t aBank, uint8_t bAdr, bool fromB)
{
    if (fromB) {
        snes->snes_write((aBank << 16) | aAdr, snes->snes_readBBus(bAdr));
    } else {
        snes->snes_writeBBus(bAdr, snes->snes_read((aBank << 16) | aAdr));
    }
}
bool Dma::dma_cycle()
{
    if (hdmaTimer > 0) {
        hdmaTimer -= 2;
        return true;
    } else if (dmaBusy) {
        dma_doDma();
        return true;
    }
    return false;
}
void Dma::dma_startDma(uint8_t val, bool hdma)
{
    for (int i = 0; i < 8; i++) {
        if (hdma) {
            channel[i].hdmaActive = val & (1 << i);
        } else {
            channel[i].dmaActive = val & (1 << i);
        }
    }
    if (!hdma) {
        dmaBusy = val;
        dmaTimer += dmaBusy ? 16 : 0;
    }
}
