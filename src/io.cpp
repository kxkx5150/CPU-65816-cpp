#include "../allegro.h"
#include <stdlib.h>
#include "cpu.h"
#include "mem.h"
#include "ppu.h"
#include "snem.h"
#include "io.h"


#define CONTINUOUS 1
#define INDIRECT   2


IO::IO(SNES *_snes)
{
    snes = _snes;
}
void IO::readjoy()
{
    pad[0] = 0;
    if (key[KEY_X])
        pad[0] |= 0x8000;
    if (key[KEY_Z])
        pad[0] |= 0x4000;
    if (key[KEY_RSHIFT])
        pad[0] |= 0x2000;
    if (key[KEY_ENTER])
        pad[0] |= 0x1000;
    if (key[KEY_UP])
        pad[0] |= 0x0800;
    if (key[KEY_DOWN])
        pad[0] |= 0x0400;
    if (key[KEY_LEFT])
        pad[0] |= 0x0200;
    if (key[KEY_RIGHT])
        pad[0] |= 0x0100;
    if (key[KEY_S])
        pad[0] |= 0x0080;
    if (key[KEY_A])
        pad[0] |= 0x0040;
    if (key[KEY_Q])
        pad[0] |= 0x0020;
    if (key[KEY_W])
        pad[0] |= 0x0010;
    padpos = 16;
}
uint8_t IO::readjoyold(uint16_t addr)
{
    int temp;
    if (addr == 0x4016) {
        temp = pad[0] >> (padpos ^ 15);
        if (!(padstat & 1))
            padpos++;
        if (padpos > 15)
            temp = 1;
        return temp & 1;
    }
    return 0xFF;
}
void IO::writejoyold(uint16_t addr, uint8_t val)
{
    if (addr == 0x4016) {
        if ((val & 1) && !(padstat & 1)) {
            padpos = 0;
        }
        padstat = val;
    }
}
void IO::writeio(uint16_t addr, uint8_t val)
{
    int     c, d = 0, offset = 0, speed;
    uint8_t temp;
    switch (addr & 0x1FF) {
        case 0x00:
            snes->nmienable = val & 0x80;
            snes->irqenable = (val >> 4) & 3;
            if (!snes->irqenable)
                snes->irq = 0;
            break;
        case 0x02:
            mula = val;
            return;
        case 0x03:
            mulb = val;
            mulr = mula * mulb;
            return;
        case 0x04:
            divc &= 0xFF00;
            divc |= val;
            return;
        case 0x05:
            divc &= 0xFF;
            divc |= (val << 8);
            return;
        case 0x06:
            divb = val;
            if (divb) {
                divr = divc / divb;
                mulr = divc % divb;
            } else {
                divr = 0xFFFF;
                mulr = divc;
            }
            return;
        case 0x07:
            snes->xirq  = (snes->xirq & 0x100) | val;
            intthisline = 0;
            break;
        case 0x08:
            snes->xirq  = (snes->xirq & 0xFF) | (val & 0x100);
            intthisline = 0;
            break;
        case 0x09:
            snes->yirq = (snes->yirq & 0x100) | val;
            break;
        case 0x0A:
            snes->yirq = (snes->yirq & 0xFF) | (val & 0x100);
            break;
        case 0x0B:
            for (c = 1; c < 0x100; c <<= 1) {
                if (val & c) {
                    do {
                        if (dmactrl[d] & 0x80) {
                            temp = snes->ppu->readppu(dmadest[d] + offset);
                            snes->writemem((dmabank[d] << 16) | dmasrc[d], temp);
                        } else {
                            temp = snes->readmem((dmabank[d] << 16) | dmasrc[d]);
                            snes->ppu->writeppu(dmadest[d] + offset, temp);
                        }
                        if (!(dmactrl[d] & 8)) {
                            if (dmactrl[d] & 0x10)
                                dmasrc[d]--;
                            else
                                dmasrc[d]++;
                        }
                        switch (dmactrl[d] & 7) {
                            case 0:
                            case 2:
                                break;
                            case 1:
                                offset++;
                                offset &= 1;
                                break;
                            default:
                                printf("Bad DMA mode %i\n", dmactrl[d] & 7);
                                exit(-1);
                        }
                        dmalen[d]--;
                    } while (dmalen[d] != 0);
                }
                d++;
            }
            break;
        case 0x0C:
            hdmaena = val;
            break;
        case 0x0D:
            if (val & 1)
                speed = 6;
            else
                speed = 8;
            for (c = 192; c < 256; c++) {
                for (d = 0; d < 8; d++) {
                    snes->mem->accessspeed[(c << 3) | d] = speed;
                }
            }
            for (c = 128; c < 192; c++) {
                snes->mem->accessspeed[(c << 3) | 4] = snes->mem->accessspeed[(c << 3) | 5] = speed;
                snes->mem->accessspeed[(c << 3) | 6] = snes->mem->accessspeed[(c << 3) | 7] = speed;
            }
            break;
        case 0x100:
        case 0x110:
        case 0x120:
        case 0x130:
        case 0x140:
        case 0x150:
        case 0x160:
        case 0x170:
            dmactrl[(addr >> 4) & 7] = val;
            break;
        case 0x101:
        case 0x111:
        case 0x121:
        case 0x131:
        case 0x141:
        case 0x151:
        case 0x161:
        case 0x171:
            dmadest[(addr >> 4) & 7] = val | 0x2100;
            break;
        case 0x102:
        case 0x112:
        case 0x122:
        case 0x132:
        case 0x142:
        case 0x152:
        case 0x162:
        case 0x172:
            dmasrc[(addr >> 4) & 7] = (dmasrc[(addr >> 4) & 7] & 0xFF00) | val;
            break;
        case 0x103:
        case 0x113:
        case 0x123:
        case 0x133:
        case 0x143:
        case 0x153:
        case 0x163:
        case 0x173:
            dmasrc[(addr >> 4) & 7] = (dmasrc[(addr >> 4) & 7] & 0xFF) | (val << 8);
            break;
        case 0x104:
        case 0x114:
        case 0x124:
        case 0x134:
        case 0x144:
        case 0x154:
        case 0x164:
        case 0x174:
            dmabank[(addr >> 4) & 7] = val;
            break;
        case 0x105:
        case 0x115:
        case 0x125:
        case 0x135:
        case 0x145:
        case 0x155:
        case 0x165:
        case 0x175:
            dmalen[(addr >> 4) & 7] = (dmalen[(addr >> 4) & 7] & 0xFF00) | val;
            break;
        case 0x106:
        case 0x116:
        case 0x126:
        case 0x136:
        case 0x146:
        case 0x156:
        case 0x166:
        case 0x176:
            dmalen[(addr >> 4) & 7] = (dmalen[(addr >> 4) & 7] & 0xFF) | (val << 8);
            break;
        case 0x107:
        case 0x117:
        case 0x127:
        case 0x137:
        case 0x147:
        case 0x157:
        case 0x167:
        case 0x177:
            dmaibank[(addr >> 4) & 7] = val;
            break;
        case 0x108:
        case 0x118:
        case 0x128:
        case 0x138:
        case 0x148:
        case 0x158:
        case 0x168:
        case 0x178:
            hdmaaddr[(addr >> 4) & 7] = (hdmaaddr[(addr >> 4) & 7] & 0xFF00) | val;
            break;
        case 0x109:
        case 0x119:
        case 0x129:
        case 0x139:
        case 0x149:
        case 0x159:
        case 0x169:
        case 0x179:
            hdmaaddr[(addr >> 4) & 7] = (hdmaaddr[(addr >> 4) & 7] & 0xFF) | (val << 8);
            break;
        case 0x10A:
        case 0x11A:
        case 0x12A:
        case 0x13A:
        case 0x14A:
        case 0x15A:
        case 0x16A:
        case 0x17A:
            hdmacount[(addr >> 4) & 7] = val;
            break;
    }
}
uint8_t IO::readio(uint16_t addr)
{
    int temp = 0;
    if (addr == 0x4016 || addr == 0x4017) {
    }
    switch (addr & 0x1FF) {
        case 0:
            return 0;
        case 0xB:
            return 0;
        case 0x0C:
            return hdmaena;
        case 0x10:
            if (snes->nmi)
                temp = 0x80;
            snes->nmi = snes->oldnmi = 0;
            return temp;
        case 0x11:
            if (snes->irq)
                temp = 0x80;
            snes->irq = 0;
            return temp;
        case 0x12:
            if (snes->vbl)
                temp |= 0x80;
            if (snes->joyscan)
                temp |= 1;
            if (snes->cpu->cycles < 340)
                temp |= 0x40;
            return temp;
        case 0x13:
            return 0;
        case 0x14:
            return divr;
        case 0x15:
            return divr >> 8;
        case 0x16:
            return mulr;
        case 0x17:
            return mulr >> 8;
        case 0x18:
            return pad[0] & 0xFF;
        case 0x19:
            return pad[0] >> 8;
        case 0x1A:
        case 0x1B:
        case 0x1C:
        case 0x1D:
        case 0x1E:
        case 0x1F:
            return 0;
        case 0x100:
        case 0x110:
        case 0x120:
        case 0x130:
        case 0x140:
        case 0x150:
        case 0x160:
        case 0x170:
            return dmactrl[(addr >> 4) & 7];
            break;
        case 0x101:
        case 0x111:
        case 0x121:
        case 0x131:
        case 0x141:
        case 0x151:
        case 0x161:
        case 0x171:
            return dmadest[(addr >> 4) & 7] & 0xFF;
            break;
        case 0x102:
        case 0x112:
        case 0x122:
        case 0x132:
        case 0x142:
        case 0x152:
        case 0x162:
        case 0x172:
            return dmasrc[(addr >> 4) & 7] & 0xFF;
            break;
        case 0x103:
        case 0x113:
        case 0x123:
        case 0x133:
        case 0x143:
        case 0x153:
        case 0x163:
        case 0x173:
            return dmasrc[(addr >> 4) & 7] >> 8;
            break;
        case 0x104:
        case 0x114:
        case 0x124:
        case 0x134:
        case 0x144:
        case 0x154:
        case 0x164:
        case 0x174:
            return dmabank[(addr >> 4) & 7];
            break;
        case 0x105:
        case 0x115:
        case 0x125:
        case 0x135:
        case 0x145:
        case 0x155:
        case 0x165:
        case 0x175:
            return dmalen[(addr >> 4) & 7] & 0xFF;
            break;
        case 0x106:
        case 0x116:
        case 0x126:
        case 0x136:
        case 0x146:
        case 0x156:
        case 0x166:
        case 0x176:
            return dmalen[(addr >> 4) & 7] >> 8;
            break;
        case 0x107:
        case 0x117:
        case 0x127:
        case 0x137:
        case 0x147:
        case 0x157:
        case 0x167:
        case 0x177:
            return dmaibank[(addr >> 4) & 7] >> 8;
            break;
        default:
            return 0;
    }
}
void IO::dohdma_macro(int c, int no)
{
    if (hdmastat[c] & INDIRECT)
        hdmadat[c] = snes->readmem((dmaibank[c] << 16) | (hdmaaddr2[c]++));
    else
        hdmadat[c] = snes->readmem((dmabank[c] << 16) | (hdmaaddr[c]++));

    snes->ppu->writeppu(dmadest[c] + no, hdmadat[c]);
}
void IO::dohdma(int line)
{
    int c;
    for (c = 0; c < 8; c++) {
        if (!line) {
            hdmaaddr[c]  = dmasrc[c];
            hdmacount[c] = 0;
        }
        if (hdmaena & (1 << c) && hdmacount[c] != -1) {
            if (hdmacount[c] <= 0) {
                hdmacount[c] = snes->readmem((dmabank[c] << 16) | (hdmaaddr[c]++));
                if (!hdmacount[c])
                    goto finishhdma;
                hdmastat[c] = 0;
                if (hdmacount[c] & 0x80) {
                    if (hdmacount[c] != 0x80) {
                        hdmacount[c] &= 0x7F;
                    }
                    hdmastat[c] |= CONTINUOUS;
                }
                if (dmactrl[c] & 0x40) {
                    hdmastat[c] |= INDIRECT;
                    hdmaaddr2[c] = snes->readmemw((dmabank[c] << 16) | hdmaaddr[c]);
                    hdmaaddr[c] += 2;
                }
            }

            switch (dmactrl[c] & 7) {
                case 1:
                    dohdma_macro(c, 0);
                    dohdma_macro(c, 1);
                    break;
                case 6:
                case 2:
                    dohdma_macro(c, 0);
                case 0:
                    dohdma_macro(c, 0);
                    break;
                case 7:
                case 3:
                    dohdma_macro(c, 0);
                    dohdma_macro(c, 0);
                    dohdma_macro(c, 1);
                    dohdma_macro(c, 1);
                    break;
                case 4:
                    dohdma_macro(c, 0);
                    dohdma_macro(c, 1);
                    dohdma_macro(c, 2);
                    dohdma_macro(c, 3);
                    break;
                case 5:
                    dohdma_macro(c, 0);
                    dohdma_macro(c, 1);
                    dohdma_macro(c, 0);
                    dohdma_macro(c, 1);
                    break;
                default:
                    if (hdmastat[c] & CONTINUOUS)
                        printf("Bad HDMA2 transfer mode %i\n", dmactrl[c] & 7);
                    else
                        printf("Bad HDMA transfer mode %i %02X %i\n", dmactrl[c] & 7, dmadest[c], hdmastat[c]);
                    exit(-1);
            }
        finishhdma:
            hdmacount[c]--;
        }
    }
}