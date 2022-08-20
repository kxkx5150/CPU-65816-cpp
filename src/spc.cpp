#include <cstdint>
#include <stdlib.h>
#include <string.h>
#include "dsp.h"
#include "snem.h"
#include "spc.h"

SPC::SPC(SNES *_snes)
{
    snes = _snes;
}
void SPC::getdp(uint16_t *addr)
{
    *addr = readspc(pc) | p.p;
    pc++;
}
void SPC::getdpx(uint16_t *addr)
{
    *addr = ((readspc(pc) + x) & 0xFF) | p.p;
    pc++;
}
void SPC::getdpy(uint16_t *addr)
{
    *addr = ((readspc(pc) + ya.b.y) & 0xFF) | p.p;
    pc++;
}
void SPC::getabs(uint16_t *addr)
{
    *addr = readspc(pc) | (readspc(pc + 1) << 8);
    pc += 2;
}
void SPC::setspczn(uint8_t v)
{
    p.z = !(v);
    p.n = (v)&0x80;
}
void SPC::setspczn16(uint16_t v)
{
    p.z = !(v);
    p.n = (v)&0x8000;
}
uint8_t SPC::readspc(uint16_t a)
{
    return (((a)&0xFFC0) == 0xFFC0) ? spcreadhigh[(a)&63] : ((((a)&0xFFF0) == 0xF0) ? readspcregs(a) : spcram[a]);
}
void SPC::writespc(uint16_t a, uint8_t v)
{
    if (((a)&0xFFF0) == 0xF0)
        writespcregs(a, v);
    else
        spcram[a] = v;
}
void SPC::ADC(uint8_t *ac, uint8_t temp, uint16_t *tempw)
{
    *tempw = *ac + temp + ((p.c) ? 1 : 0);
    p.v    = (!((*ac ^ temp) & 0x80) && ((*ac ^ *tempw) & 0x80));
    *ac    = *tempw & 0xFF;
    setspczn(*ac);
    p.c = *tempw & 0x100;
}
void SPC::SBC(uint8_t *ac, uint8_t temp, uint16_t *tempw)
{
    *tempw = *ac - temp - ((p.c) ? 0 : 1);
    p.v    = (((*ac ^ temp) & 0x80) && ((*ac ^ *tempw) & 0x80));
    *ac    = *tempw & 0xFF;
    setspczn(*ac);
    p.c = *tempw <= 0xFF;
}
uint8_t SPC::readfromspc(uint16_t addr)
{
    return spctocpu[addr & 3];
}
void SPC::writetospc(uint16_t addr, uint8_t val)
{
    spcram[(addr & 3) + 0xF4] = val;
}
void SPC::writespcregs(uint16_t a, uint8_t v)
{
    switch (a) {
        case 0xF1:
            if (v & 0x10)
                spcram[0xF4] = spcram[0xF5] = 0;
            if (v & 0x20)
                spcram[0xF6] = spcram[0xF7] = 0;
            spcram[0xF1] = v;
            if (v & 0x80)
                spcreadhigh = spcrom;
            else
                spcreadhigh = spcram + 0xFFC0;
            break;
        case 0xF2:
        case 0xF3:
            snes->dsp->writedsp(a, v);
            break;
        case 0xF4:
        case 0xF5:
        case 0xF6:
        case 0xF7:
            spctocpu[a & 3] = v;
            break;
        case 0xFA:
        case 0xFB:
        case 0xFC:
            spclimit[a - 0xFA] = v;
            break;
        case 0xFD:
        case 0xFE:
        case 0xFF:
            spcram[a] = v;
            break;
    }
}
uint16_t SPC::getspcpc()
{
    return pc;
}
uint8_t SPC::readspcregs(uint16_t a)
{
    uint8_t v;
    switch (a) {
        case 0xF2:
        case 0xF3:
            return snes->dsp->readdsp(a);
        case 0xFD:
        case 0xFE:
        case 0xFF:
            v         = spcram[a];
            spcram[a] = 0;
            return v;
    }
    return spcram[a];
}
void SPC::initspc()
{
    spcram = (uint8_t *)malloc(65536);
    memset(spcram, 0, 65536);
}
void SPC::resetspc()
{
    pc          = 0xFFC0;
    spcreadhigh = spcrom;
    x = ya.w    = 0;
    spccycles   = 0;
    p.p         = 0;
    spctimer[0] = spctimer[1] = spctimer[2] = 0;
    spctimer2[0] = spctimer2[1] = spctimer2[2] = 0;
    spcram[0xF1]                               = 0x80;
}
void SPC::execspc()
{
    uint8_t  opcode, temp, temp2;
    uint16_t addr, addr2, tempw;
    uint32_t templ;
    int      spccount;
    while (spccycles > 0) {
        spc3     = spc2;
        spc2     = pc;
        spccount = 0;
        opcode   = readspc(pc);
        pc++;
        switch (opcode) {
            case 0x00:
                spccount += 2;
                break;
            case 0x02:
                getdp(&addr);
                temp = readspc(addr) | 0x01;
                writespc(addr, temp);
                spccount += 4;
                break;
            case 0x22:
                getdp(&addr);
                temp = readspc(addr) | 0x02;
                writespc(addr, temp);
                spccount += 4;
                break;
            case 0x42:
                getdp(&addr);
                temp = readspc(addr) | 0x04;
                writespc(addr, temp);
                spccount += 4;
                break;
            case 0x62:
                getdp(&addr);
                temp = readspc(addr) | 0x08;
                writespc(addr, temp);
                spccount += 4;
                break;
            case 0x82:
                getdp(&addr);
                temp = readspc(addr) | 0x10;
                writespc(addr, temp);
                spccount += 4;
                break;
            case 0xA2:
                getdp(&addr);
                temp = readspc(addr) | 0x20;
                writespc(addr, temp);
                spccount += 4;
                break;
            case 0xC2:
                getdp(&addr);
                temp = readspc(addr) | 0x40;
                writespc(addr, temp);
                spccount += 4;
                break;
            case 0xE2:
                getdp(&addr);
                temp = readspc(addr) | 0x80;
                writespc(addr, temp);
                spccount += 4;
                break;
            case 0x12:
                getdp(&addr);
                temp = readspc(addr) & ~0x01;
                writespc(addr, temp);
                spccount += 4;
                break;
            case 0x32:
                getdp(&addr);
                temp = readspc(addr) & ~0x02;
                writespc(addr, temp);
                spccount += 4;
                break;
            case 0x52:
                getdp(&addr);
                temp = readspc(addr) & ~0x04;
                writespc(addr, temp);
                spccount += 4;
                break;
            case 0x72:
                getdp(&addr);
                temp = readspc(addr) & ~0x08;
                writespc(addr, temp);
                spccount += 4;
                break;
            case 0x92:
                getdp(&addr);
                temp = readspc(addr) & ~0x10;
                writespc(addr, temp);
                spccount += 4;
                break;
            case 0xB2:
                getdp(&addr);
                temp = readspc(addr) & ~0x20;
                writespc(addr, temp);
                spccount += 4;
                break;
            case 0xD2:
                getdp(&addr);
                temp = readspc(addr) & ~0x40;
                writespc(addr, temp);
                spccount += 4;
                break;
            case 0xF2:
                getdp(&addr);
                temp = readspc(addr) & ~0x80;
                writespc(addr, temp);
                spccount += 4;
                break;
            case 0x04:
                getdp(&addr);
                ya.b.a |= readspc(addr);
                setspczn(ya.b.a);
                spccount += 3;
                break;
            case 0x05:
                getabs(&addr);
                ya.b.a |= readspc(addr);
                setspczn(ya.b.a);
                spccount += 4;
                break;
            case 0x06:
                addr = x + p.p;
                ya.b.a |= readspc(addr);
                setspczn(ya.b.a);
                spccount += 3;
                break;
            case 0x07:
                getdpx(&addr);
                addr2 = readspc(addr) + (readspc(addr + 1) << 8);
                ya.b.a |= readspc(addr2);
                setspczn(ya.b.a);
                spccount += 6;
                break;
            case 0x08:
                ya.b.a |= readspc(pc);
                pc++;
                setspczn(ya.b.a);
                spccount += 2;
                break;
            case 0x09:
                addr = readspc(pc) + p.p;
                pc++;
                addr2 = readspc(pc) + p.p;
                pc++;
                temp = readspc(addr) | readspc(addr2);
                setspczn(temp);
                writespc(addr2, temp);
                spccount += 6;
                break;
            case 0x0B:
                getdp(&addr);
                temp = readspc(addr);
                p.c  = temp & 0x80;
                temp <<= 1;
                setspczn(temp);
                writespc(addr, temp);
                spccount += 4;
                break;
            case 0x0C:
                getabs(&addr);
                temp = readspc(addr);
                p.c  = temp & 0x80;
                temp <<= 1;
                setspczn(temp);
                writespc(addr, temp);
                spccount += 4;
                break;
            case 0x0D:
                temp = (p.c) ? 1 : 0;
                if (p.z)
                    temp |= 0x02;
                if (p.i)
                    temp |= 0x04;
                if (p.h)
                    temp |= 0x08;
                if (p.b)
                    temp |= 0x10;
                if (p.p)
                    temp |= 0x20;
                if (p.v)
                    temp |= 0x40;
                if (p.n)
                    temp |= 0x80;
                writespc(sp + 0x100, temp);
                sp--;
                spccount += 4;
                break;
            case 0x0E:
                getabs(&addr);
                temp = readspc(addr);
                setspczn(ya.b.a & temp);
                temp |= ya.b.a;
                writespc(addr, temp);
                spccount += 6;
                break;
            case 0x10:
                addr = readspc(pc);
                pc++;
                if (addr & 0x80)
                    addr |= 0xFF00;
                if (!p.n) {
                    pc += addr;
                    spccount += 2;
                }
                spccount += 2;
                break;
            case 0x14:
                getdpx(&addr);
                ya.b.a |= readspc(addr);
                setspczn(ya.b.a);
                spccount += 4;
                break;
            case 0x15:
                addr = readspc(pc) + (readspc(pc + 1) << 8) + x;
                pc += 2;
                ya.b.a |= readspc(addr);
                setspczn(ya.b.a);
                spccount += 5;
                break;
            case 0x16:
                addr = readspc(pc) + (readspc(pc + 1) << 8) + ya.b.y;
                pc += 2;
                ya.b.a |= readspc(addr);
                setspczn(ya.b.a);
                spccount += 5;
                break;
            case 0x17:
                getdp(&addr);
                addr2 = readspc(addr) + (readspc(addr + 1) << 8) + ya.b.y;
                ya.b.a |= readspc(addr2);
                setspczn(ya.b.a);
                spccount += 6;
                break;
            case 0x18:
                temp2 = readspc(pc);
                pc++;
                getdp(&addr);
                temp2 |= readspc(addr);
                setspczn(temp2);
                writespc(addr, temp2);
                spccount += 5;
                break;
            case 0x1A:
                getdp(&addr);
                tempw = (readspc(addr) + (readspc(addr + 1) << 8)) - 1;
                writespc(addr, tempw & 0xFF);
                writespc(addr + 1, tempw >> 8);
                p.z = !tempw;
                p.n = tempw & 0x8000;
                spccount += 6;
                break;
            case 0x1B:
                getdpx(&addr);
                temp = readspc(addr);
                p.c  = temp & 0x80;
                temp <<= 1;
                setspczn(temp);
                writespc(addr, temp);
                spccount += 4;
                break;
            case 0x1C:
                p.c = ya.b.a & 0x80;
                ya.b.a <<= 1;
                setspczn(ya.b.a);
                spccount += 2;
                break;
            case 0x1D:
                x--;
                setspczn(x);
                spccount += 2;
                break;
            case 0x1E:
                getabs(&addr);
                temp = readspc(addr);
                setspczn(x - temp);
                p.c = (x >= temp);
                spccount += 4;
                break;
            case 0x1F:
                addr = readspc(pc) + (readspc(pc + 1) << 8) + x;
                pc += 2;
                pc = readspc(addr) | (readspc(addr + 1) << 8);
                spccount += 6;
                break;
            case 0x20:
                p.p = 0;
                spccount += 2;
                break;
            case 0x24:
                getdp(&addr);
                ya.b.a &= readspc(addr);
                setspczn(ya.b.a);
                spccount += 3;
                break;
            case 0x25:
                getabs(&addr);
                ya.b.a &= readspc(addr);
                setspczn(ya.b.a);
                spccount += 4;
                break;
            case 0x26:
                addr = x + p.p;
                ya.b.a &= readspc(addr);
                setspczn(ya.b.a);
                spccount += 3;
                break;
            case 0x27:
                getdpx(&addr);
                addr2 = readspc(addr) + (readspc(addr + 1) << 8);
                ya.b.a &= readspc(addr2);
                setspczn(ya.b.a);
                spccount += 6;
                break;
            case 0x28:
                ya.b.a &= readspc(pc);
                pc++;
                setspczn(ya.b.a);
                spccount += 2;
                break;
            case 0x29:
                addr = readspc(pc) + p.p;
                pc++;
                addr2 = readspc(pc) + p.p;
                pc++;
                temp = readspc(addr) & readspc(addr2);
                setspczn(temp);
                writespc(addr2, temp);
                spccount += 6;
                break;
            case 0x2B:
                getdp(&addr);
                temp  = readspc(addr);
                templ = p.c;
                p.c   = temp & 0x80;
                temp <<= 1;
                if (templ)
                    temp |= 1;
                setspczn(temp);
                writespc(addr, temp);
                spccount += 4;
                break;
            case 0x2D:
                writespc(sp + 0x100, ya.b.a);
                sp--;
                spccount += 4;
                break;
            case 0x2E:
                getdp(&addr);
                temp = readspc(addr);
                addr = readspc(pc);
                pc++;
                if (addr & 0x80)
                    addr |= 0xFF00;
                if (ya.b.a != temp) {
                    pc += addr;
                    spccount += 2;
                }
                spccount += 5;
                break;
            case 0x2F:
                addr = readspc(pc);
                pc++;
                if (addr & 0x80)
                    addr |= 0xFF00;
                pc += addr;
                spccount += 4;
                break;
            case 0x30:
                addr = readspc(pc);
                pc++;
                if (addr & 0x80)
                    addr |= 0xFF00;
                if (p.n) {
                    pc += addr;
                    spccount += 2;
                }
                spccount += 2;
                break;
            case 0x35:
                addr = readspc(pc) + (readspc(pc + 1) << 8) + x;
                pc += 2;
                ya.b.a &= readspc(addr);
                setspczn(ya.b.a);
                spccount += 5;
                break;
            case 0x36:
                addr = readspc(pc) + (readspc(pc + 1) << 8) + ya.b.y;
                pc += 2;
                ya.b.a &= readspc(addr);
                setspczn(ya.b.a);
                spccount += 5;
                break;
            case 0x38:
                temp2 = readspc(pc);
                pc++;
                getdp(&addr);
                temp2 &= readspc(addr);
                setspczn(temp2);
                writespc(addr, temp2);
                spccount += 5;
                break;
            case 0x3A:
                getdp(&addr);
                tempw = readspc(addr) + (readspc(addr + 1) << 8) + 1;
                writespc(addr, tempw & 0xFF);
                writespc(addr + 1, tempw >> 8);
                p.z = !tempw;
                p.n = tempw & 0x8000;
                spccount += 6;
                break;
            case 0x3C:
                templ = p.c;
                p.c   = ya.b.a & 0x80;
                ya.b.a <<= 1;
                if (templ)
                    ya.b.a |= 1;
                setspczn(ya.b.a);
                spccount += 2;
                break;
            case 0x3D:
                x++;
                setspczn(x);
                spccount += 2;
                break;
            case 0x3E:
                getdp(&addr);
                temp = readspc(addr);
                setspczn(x - temp);
                p.c = (x >= temp);
                spccount += 3;
                break;
            case 0x3F:
                addr = readspc(pc) + (readspc(pc + 1) << 8);
                pc += 2;
                writespc(sp + 0x100, pc >> 8);
                sp--;
                writespc(sp + 0x100, pc & 0xFF);
                sp--;
                pc = addr;
                spccount += 8;
                break;
            case 0x40:
                p.p = 0x100;
                spccount += 2;
                break;
            case 0x44:
                getdp(&addr);
                ya.b.a ^= readspc(addr);
                setspczn(ya.b.a);
                spccount += 3;
                break;
            case 0x45:
                getabs(&addr);
                ya.b.a ^= readspc(addr);
                setspczn(ya.b.a);
                spccount += 4;
                break;
            case 0x46:
                addr = x + p.p;
                ya.b.a ^= readspc(addr);
                setspczn(ya.b.a);
                spccount += 3;
                break;
            case 0x47:
                getdpx(&addr);
                addr2 = readspc(addr) + (readspc(addr + 1) << 8);
                ya.b.a ^= readspc(addr2);
                setspczn(ya.b.a);
                spccount += 6;
                break;
            case 0x48:
                ya.b.a ^= readspc(pc);
                pc++;
                setspczn(ya.b.a);
                spccount += 2;
                break;
            case 0x49:
                addr = readspc(pc) + p.p;
                pc++;
                addr2 = readspc(pc) + p.p;
                pc++;
                temp = readspc(addr) ^ readspc(addr2);
                setspczn(temp);
                writespc(addr2, temp);
                spccount += 6;
                break;
            case 0x4B:
                getdp(&addr);
                temp = readspc(addr);
                p.c  = temp & 1;
                temp >>= 1;
                setspczn(temp);
                writespc(addr, temp);
                spccount += 4;
                break;
            case 0x4C:
                getabs(&addr);
                temp = readspc(addr);
                p.c  = temp & 1;
                temp >>= 1;
                setspczn(temp);
                writespc(addr, temp);
                spccount += 5;
                break;
            case 0x4D:
                writespc(sp + 0x100, x);
                sp--;
                spccount += 4;
                break;
            case 0x4E:
                getabs(&addr);
                temp = readspc(addr);
                setspczn(ya.b.a & temp);
                temp &= ~ya.b.a;
                writespc(addr, temp);
                spccount += 6;
                break;
            case 0x4F:
                temp = readspc(pc);
                pc++;
                writespc(sp + 0x100, pc >> 8);
                sp--;
                writespc(sp + 0x100, pc & 0xFF);
                sp--;
                pc = 0xFF00 | temp;
                spccount += 6;
                break;
            case 0x50:
                addr = readspc(pc);
                pc++;
                if (addr & 0x80)
                    addr |= 0xFF00;
                if (!p.v) {
                    pc += addr;
                    spccount += 2;
                }
                spccount += 2;
                break;
            case 0x55:
                addr = readspc(pc) + (readspc(pc + 1) << 8) + x;
                pc += 2;
                ya.b.a ^= readspc(addr);
                setspczn(ya.b.a);
                spccount += 5;
                break;
            case 0x56:
                addr = readspc(pc) + (readspc(pc + 1) << 8) + ya.b.y;
                pc += 2;
                ya.b.a ^= readspc(addr);
                setspczn(ya.b.a);
                spccount += 5;
                break;
            case 0x58:
                temp2 = readspc(pc);
                pc++;
                getdp(&addr);
                temp2 ^= readspc(addr);
                setspczn(temp2);
                writespc(addr, temp2);
                spccount += 5;
                break;
            case 0x5A:
                getdp(&addr);
                tempw = readspc(addr) | (readspc(addr + 1) << 8);
                setspczn16(ya.w - tempw);
                spccount += 4;
                break;
            case 0x5B:
                getdpx(&addr);
                temp = readspc(addr);
                p.c  = temp & 1;
                temp >>= 1;
                setspczn(temp);
                writespc(addr, temp);
                spccount += 5;
                break;
            case 0x5C:
                p.c = ya.b.a & 1;
                ya.b.a >>= 1;
                setspczn(ya.b.a);
                spccount += 2;
                break;
            case 0x5D:
                x = ya.b.a;
                setspczn(x);
                spccount += 2;
                break;
            case 0x5E:
                getabs(&addr);
                temp = readspc(addr);
                setspczn(ya.b.y - temp);
                p.c = (ya.b.y >= temp);
                spccount += 4;
                break;
            case 0x5F:
                getabs(&addr);
                pc = addr;
                spccount += 3;
                break;
            case 0x60:
                p.c = 0;
                spccount += 2;
                break;
            case 0x64:
                getdp(&addr);
                temp = readspc(addr);
                setspczn(ya.b.a - temp);
                p.c = (ya.b.a >= temp);
                spccount += 3;
                break;
            case 0x65:
                getabs(&addr);
                temp = readspc(addr);
                setspczn(ya.b.a - temp);
                p.c = (ya.b.a >= temp);
                spccount += 4;
                break;
            case 0x66:
                addr = x + p.p;
                temp = readspc(addr);
                setspczn(ya.b.a - temp);
                p.c = (ya.b.a >= temp);
                spccount += 3;
                break;
            case 0x68:
                temp = readspc(pc);
                pc++;
                setspczn(ya.b.a - temp);
                p.c = (ya.b.a >= temp);
                spccount += 2;
                break;
            case 0x69:
                getdp(&addr);
                addr2 = addr;
                getdp(&addr);
                temp  = readspc(addr2);
                temp2 = readspc(addr);
                p.c   = (temp >= temp2);
                setspczn(temp - temp2);
                spccount += 6;
                break;
            case 0x6B:
                getdp(&addr);
                temp  = readspc(addr);
                templ = p.c;
                p.c   = temp & 1;
                temp >>= 1;
                if (templ)
                    temp |= 0x80;
                setspczn(temp);
                writespc(addr, temp);
                spccount += 4;
                break;
            case 0x6D:
                writespc(sp + 0x100, ya.b.y);
                sp--;
                spccount += 4;
                break;
            case 0x6E:
                getdp(&addr);
                temp = readspc(addr) - 1;
                writespc(addr, temp);
                addr = readspc(pc);
                pc++;
                if (addr & 0x80)
                    addr |= 0xFF00;
                if (temp) {
                    pc += addr;
                    spccount += 2;
                }
                spccount += 5;
                break;
            case 0x6F:
                sp++;
                pc = readspc(sp + 0x100);
                sp++;
                pc |= (readspc(sp + 0x100) << 8);
                spccount += 5;
                break;
            case 0x70:
                addr = readspc(pc);
                pc++;
                if (addr & 0x80)
                    addr |= 0xFF00;
                if (p.v) {
                    pc += addr;
                    spccount += 2;
                }
                spccount += 2;
                break;
            case 0x74:
                getdpx(&addr);
                temp = readspc(addr);
                setspczn(ya.b.a - temp);
                p.c = (ya.b.a >= temp);
                spccount += 5;
                break;
            case 0x75:
                addr = readspc(pc) + (readspc(pc + 1) << 8) + x;
                pc += 2;
                temp = readspc(addr);
                setspczn(ya.b.a - temp);
                p.c = (ya.b.a >= temp);
                spccount += 5;
                break;
            case 0x76:
                addr = readspc(pc) + (readspc(pc + 1) << 8) + ya.b.y;
                pc += 2;
                temp = readspc(addr);
                setspczn(ya.b.a - temp);
                p.c = (ya.b.a >= temp);
                spccount += 5;
                break;
            case 0x77:
                addr = readspc(pc) + p.p;
                pc++;
                addr2 = readspc(addr) + (readspc(addr + 1) << 8) + ya.b.y;
                temp  = readspc(addr2);
                setspczn(ya.b.a - temp);
                p.c = (ya.b.a >= temp);
                spccount += 6;
                break;
            case 0x78:
                temp = readspc(pc);
                pc++;
                getdp(&addr);
                temp2 = readspc(addr);
                setspczn(temp2 - temp);
                p.c = (temp2 >= temp);
                spccount += 5;
                break;
            case 0x7A:
                addr = readspc(pc) + p.p;
                pc++;
                tempw = readspc(addr) | (readspc(addr + 1) << 8);
                templ = ya.w + tempw;
                p.v   = (!((ya.w ^ tempw) & 0x8000) && ((ya.w ^ templ) & 0x8000));
                ya.w  = templ & 0xFFFF;
                setspczn16(ya.w);
                p.c = templ & 0x10000;
                spccount += 5;
                break;
            case 0x7B:
                getdpx(&addr);
                temp  = readspc(addr);
                templ = p.c;
                p.c   = temp & 1;
                temp >>= 1;
                if (templ)
                    temp |= 0x80;
                setspczn(temp);
                writespc(addr, temp);
                spccount += 4;
                break;
            case 0x7C:
                templ = p.c;
                p.c   = ya.b.a & 1;
                ya.b.a >>= 1;
                if (templ)
                    ya.b.a |= 0x80;
                setspczn(ya.b.a);
                spccount += 2;
                break;
            case 0x7D:
                ya.b.a = x;
                setspczn(ya.b.a);
                spccount += 2;
                break;
            case 0x7E:
                getdp(&addr);
                temp = readspc(addr);
                setspczn(ya.b.y - temp);
                p.c = (ya.b.y >= temp);
                spccount += 3;
                break;
            case 0x80:
                p.c = 1;
                spccount += 2;
                break;
            case 0x84:
                getdp(&addr);
                temp = readspc(addr);
                ADC(&ya.b.a, temp, &tempw);
                spccount += 3;
                break;
            case 0x85:
                getabs(&addr);
                temp = readspc(addr);
                ADC(&ya.b.a, temp, &tempw);
                spccount += 4;
                break;
            case 0x88:
                temp = readspc(pc);
                pc++;
                ADC(&ya.b.a, temp, &tempw);
                spccount += 2;
                break;
            case 0x89:
                addr = readspc(pc) + p.p;
                pc++;
                addr2 = readspc(pc) + p.p;
                pc++;
                temp  = readspc(addr);
                temp2 = readspc(addr2);
                ADC(&temp2, temp, &tempw);
                writespc(addr2, temp2);
                spccount += 6;
                break;
            case 0x8A:
                addr = readspc(pc) | (readspc(pc + 1) << 8);
                pc += 2;
                tempw = addr >> 13;
                addr &= 0x1FFF;
                temp = readspc(addr);
                p.c  = (p.c ? 1 : 0) ^ ((temp & (1 << tempw)) ? 1 : 0);
                spccount += 5;
                break;
            case 0x8B:
                getdp(&addr);
                temp = readspc(addr) - 1;
                setspczn(temp);
                writespc(addr, temp);
                spccount += 4;
                break;
            case 0x8C:
                getabs(&addr);
                temp = readspc(addr) - 1;
                setspczn(temp);
                writespc(addr, temp);
                spccount += 5;
                break;
            case 0x8D:
                ya.b.y = readspc(pc);
                pc++;
                setspczn(ya.b.y);
                spccount += 2;
                break;
            case 0x8E:
                sp++;
                temp = readspc(sp + 0x100);
                p.c  = temp & 0x01;
                p.z  = temp & 0x02;
                p.i  = temp & 0x04;
                p.h  = temp & 0x08;
                p.b  = temp & 0x10;
                p.p  = (temp & 0x20) ? 0x100 : 0;
                p.v  = temp & 0x40;
                p.n  = temp & 0x80;
                spccount += 4;
                break;
            case 0x8F:
                temp = readspc(pc);
                pc++;
                getdp(&addr);
                writespc(addr, temp);
                spccount += 5;
                break;
            case 0x90:
                addr = readspc(pc);
                pc++;
                if (addr & 0x80)
                    addr |= 0xFF00;
                if (!p.c) {
                    pc += addr;
                    spccount += 2;
                }
                spccount += 2;
                break;
            case 0x94:
                getdpx(&addr);
                temp = readspc(addr);
                ADC(&ya.b.a, temp, &tempw);
                spccount += 4;
                break;
            case 0x95:
                addr = readspc(pc) + (readspc(pc + 1) << 8) + x;
                pc += 2;
                temp = readspc(addr);
                ADC(&ya.b.a, temp, &tempw);
                spccount += 5;
                break;
            case 0x96:
                addr = readspc(pc) + (readspc(pc + 1) << 8) + ya.b.y;
                pc += 2;
                temp = readspc(addr);
                ADC(&ya.b.a, temp, &tempw);
                spccount += 5;
                break;
            case 0x97:
                addr = readspc(pc) + p.p;
                pc++;
                addr2 = readspc(addr) + (readspc(addr + 1) << 8) + ya.b.y;
                temp  = readspc(addr2);
                ADC(&ya.b.a, temp, &tempw);
                spccount += 6;
                break;
            case 0x98:
                temp2 = readspc(pc);
                pc++;
                getdp(&addr);
                temp = readspc(addr);
                ADC(&temp, temp2, &tempw);
                writespc(addr, temp);
                spccount += 5;
                break;
            case 0x9A:
                addr = readspc(pc) + p.p;
                pc++;
                tempw = readspc(addr) | (readspc(addr + 1) << 8);
                templ = ya.w - tempw;
                p.v   = (((ya.w ^ tempw) & 0x8000) && ((ya.w ^ templ) & 0x8000));
                ya.w  = templ & 0xFFFF;
                p.c   = (ya.w >= tempw);
                setspczn16(ya.w);
                spccount += 5;
                break;
            case 0x9B:
                getdpx(&addr);
                temp = readspc(addr) - 1;
                setspczn(temp);
                writespc(addr, temp);
                spccount += 5;
                break;
            case 0x9C:
                ya.b.a--;
                setspczn(ya.b.a);
                spccount += 2;
                break;
            case 0x9D:
                x = sp;
                setspczn(x);
                spccount += 2;
                break;
            case 0x9E:
                if (x) {
                    temp   = ya.w / x;
                    ya.b.y = ya.w % x;
                    ya.b.a = temp;
                    setspczn(temp);
                    p.v = 0;
                } else {
                    ya.w = 0xFFFF;
                    p.v  = 1;
                }
                spccount += 12;
                break;
            case 0x9F:
                ya.b.a = (ya.b.a >> 4) | (ya.b.a << 4);
                setspczn(ya.b.a);
                spccount += 5;
                break;
            case 0xA4:
                getdp(&addr);
                temp = readspc(addr);
                SBC(&ya.b.a, temp, &tempw);
                spccount += 3;
                break;
            case 0xA5:
                getabs(&addr);
                temp = readspc(addr);
                SBC(&ya.b.a, temp, &tempw);
                spccount += 4;
                break;
            case 0xA8:
                temp = readspc(pc);
                pc++;
                SBC(&ya.b.a, temp, &tempw);
                spccount += 2;
                break;
            case 0xA9:
                addr = readspc(pc) + p.p;
                pc++;
                addr2 = readspc(pc) + p.p;
                pc++;
                temp  = readspc(addr);
                temp2 = readspc(addr2);
                SBC(&temp2, temp, &tempw);
                writespc(addr2, temp2);
                spccount += 6;
                break;
            case 0xAA:
                getabs(&addr);
                tempw = addr >> 13;
                addr &= 0x1FFF;
                temp = readspc(addr);
                p.c  = temp & (1 << tempw);
                spccount += 4;
                break;
            case 0xAB:
                getdp(&addr);
                temp = readspc(addr) + 1;
                setspczn(temp);
                writespc(addr, temp);
                spccount += 4;
                break;
            case 0xAC:
                getabs(&addr);
                temp = readspc(addr) + 1;
                setspczn(temp);
                writespc(addr, temp);
                spccount += 5;
                break;
            case 0xAD:
                temp = readspc(pc);
                pc++;
                setspczn(ya.b.y - temp);
                p.c = (ya.b.y >= temp);
                spccount += 2;
                break;
            case 0xAE:
                sp++;
                ya.b.a = readspc(sp + 0x100);
                spccount += 4;
                break;
            case 0xAF:
                writespc(x + p.p, ya.b.a);
                x++;
                spccount += 4;
                break;
            case 0xB0:
                addr = readspc(pc);
                pc++;
                if (addr & 0x80)
                    addr |= 0xFF00;
                if (p.c) {
                    pc += addr;
                    spccount += 2;
                }
                spccount += 2;
                break;
            case 0xB4:
                getdpx(&addr);
                temp = readspc(addr);
                SBC(&ya.b.a, temp, &tempw);
                spccount += 4;
                break;
            case 0xB5:
                addr = readspc(pc) + (readspc(pc + 1) << 8) + x;
                pc += 2;
                temp = readspc(addr);
                SBC(&ya.b.a, temp, &tempw);
                spccount += 5;
                break;
            case 0xB6:
                addr = readspc(pc) + (readspc(pc + 1) << 8) + ya.b.y;
                pc += 2;
                temp = readspc(addr);
                SBC(&ya.b.a, temp, &tempw);
                spccount += 5;
                break;
            case 0xB7:
                addr = readspc(pc) + p.p;
                pc++;
                addr2 = readspc(addr) + (readspc(addr + 1) << 8) + ya.b.y;
                temp  = readspc(addr2);
                SBC(&ya.b.a, temp, &tempw);
                spccount += 6;
                break;
            case 0xB8:
                temp2 = readspc(pc);
                pc++;
                getdp(&addr);
                temp = readspc(addr);
                SBC(&temp, temp2, &tempw);
                writespc(addr, temp);
                spccount += 5;
                break;
            case 0xBA:
                getdp(&addr);
                ya.b.a = readspc(addr);
                ya.b.y = readspc(addr + 1);
                setspczn16(ya.w);
                spccount += 5;
                break;
            case 0xBB:
                getdpx(&addr);
                temp = readspc(addr) + 1;
                setspczn(temp);
                writespc(addr, temp);
                spccount += 5;
                break;
            case 0xBC:
                ya.b.a++;
                setspczn(ya.b.a);
                spccount += 2;
                break;
            case 0xBD:
                sp = x;
                spccount += 2;
                break;
            case 0xC0:
                p.i = 0;
                spccount += 3;
                break;
            case 0xC4:
                getdp(&addr);
                writespc(addr, ya.b.a);
                spccount += 4;
                break;
            case 0xC5:
                getabs(&addr);
                writespc(addr, ya.b.a);
                spccount += 5;
                break;
            case 0xC6:
                writespc(x + p.p, ya.b.a);
                spccount += 4;
                break;
            case 0xC7:
                getdpx(&addr);
                addr2 = readspc(addr) + (readspc(addr + 1) << 8);
                writespc(addr2, ya.b.a);
                spccount += 7;
                break;
            case 0xC8:
                temp = readspc(pc);
                pc++;
                setspczn(x - temp);
                p.c = (x >= temp);
                spccount += 2;
                break;
            case 0xC9:
                getabs(&addr);
                writespc(addr, x);
                spccount += 5;
                break;
            case 0xCA:
                addr = readspc(pc) | (readspc(pc + 1) << 8);
                pc += 2;
                tempw = addr >> 13;
                addr &= 0x1FFF;
                temp = readspc(addr);
                if (p.c)
                    temp |= (1 << tempw);
                else
                    temp &= ~(1 << tempw);
                writespc(addr, temp);
                spccount += 6;
                break;
            case 0xCB:
                getdp(&addr);
                writespc(addr, ya.b.y);
                spccount += 4;
                break;
            case 0xCC:
                getabs(&addr);
                writespc(addr, ya.b.y);
                spccount += 5;
                break;
            case 0xCD:
                x = readspc(pc);
                pc++;
                setspczn(x);
                spccount += 2;
                break;
            case 0xCE:
                sp++;
                x = readspc(sp + 0x100);
                spccount += 4;
                break;
            case 0xCF:
                ya.w = ya.b.a * ya.b.y;
                setspczn(ya.b.y);
                spccount += 9;
                break;
            case 0xD0:
                addr = readspc(pc);
                pc++;
                if (addr & 0x80)
                    addr |= 0xFF00;
                if (!p.z) {
                    pc += addr;
                    spccount += 2;
                }
                spccount += 2;
                break;
            case 0xD4:
                getdpx(&addr);
                writespc(addr, ya.b.a);
                spccount += 5;
                break;
            case 0xD5:
                addr = readspc(pc) + (readspc(pc + 1) << 8) + x;
                pc += 2;
                writespc(addr, ya.b.a);
                spccount += 6;
                break;
            case 0xD6:
                addr = readspc(pc) + (readspc(pc + 1) << 8) + ya.b.y;
                pc += 2;
                writespc(addr, ya.b.a);
                spccount += 6;
                break;
            case 0xD7:
                addr = readspc(pc) + p.p;
                pc++;
                addr2 = readspc(addr) + (readspc(addr + 1) << 8) + ya.b.y;
                writespc(addr2, ya.b.a);
                spccount += 7;
                break;
            case 0xD8:
                getdp(&addr);
                writespc(addr, x);
                spccount += 4;
                break;
            case 0xD9:
                addr = ((readspc(pc) + ya.b.y) & 0xFF) + p.p;
                pc++;
                writespc(addr, x);
                spccount += 5;
                break;
            case 0xDA:
                getdp(&addr);
                writespc(addr, ya.b.a);
                writespc(addr + 1, ya.b.y);
                spccount += 5;
                break;
            case 0xDB:
                getdpx(&addr);
                writespc(addr, ya.b.y);
                spccount += 5;
                break;
            case 0xDC:
                ya.b.y--;
                setspczn(ya.b.y);
                spccount += 2;
                break;
            case 0xDD:
                ya.b.a = ya.b.y;
                setspczn(ya.b.a);
                spccount += 2;
                break;
            case 0xDE:
                getdpx(&addr);
                temp = readspc(addr);
                addr = readspc(pc);
                pc++;
                if (addr & 0x80)
                    addr |= 0xFF00;
                if (ya.b.a != temp) {
                    pc += addr;
                    spccount += 2;
                }
                spccount += 6;
                break;
            case 0xE0:
                p.v = p.h = 0;
                spccount += 2;
                break;
            case 0xE4:
                getdp(&addr);
                ya.b.a = readspc(addr);
                setspczn(ya.b.a);
                spccount += 3;
                break;
            case 0xE5:
                getabs(&addr);
                ya.b.a = readspc(addr);
                setspczn(ya.b.a);
                spccount += 4;
                break;
            case 0xE6:
                ya.b.a = readspc(x + p.p);
                setspczn(ya.b.a);
                spccount += 3;
                break;
            case 0xE7:
                getdpx(&addr);
                addr2  = readspc(addr) | (readspc(addr + 1) << 8);
                ya.b.a = readspc(addr2);
                setspczn(ya.b.a);
                spccount += 6;
                break;
            case 0xE8:
                ya.b.a = readspc(pc);
                pc++;
                setspczn(ya.b.a);
                spccount += 2;
                break;
            case 0xE9:
                getabs(&addr);
                x = readspc(addr);
                setspczn(x);
                spccount += 4;
                break;
            case 0xEA:
                addr = readspc(pc) | (readspc(pc + 1) << 8);
                pc += 2;
                tempw = addr >> 13;
                addr &= 0x1FFF;
                temp = readspc(addr);
                temp ^= (1 << tempw);
                writespc(addr, temp);
                spccount += 5;
                break;
            case 0xEB:
                getdp(&addr);
                ya.b.y = readspc(addr);
                setspczn(ya.b.y);
                spccount += 3;
                break;
            case 0xEC:
                getabs(&addr);
                ya.b.y = readspc(addr);
                setspczn(ya.b.y);
                spccount += 4;
                break;
            case 0xED:
                p.c = !p.c;
                spccount += 3;
                break;
            case 0xEE:
                sp++;
                ya.b.y = readspc(sp + 0x100);
                spccount += 4;
                break;
            case 0xF0:
                addr = readspc(pc);
                pc++;
                if (addr & 0x80)
                    addr |= 0xFF00;
                if (p.z) {
                    pc += addr;
                    spccount += 2;
                }
                spccount += 2;
                break;
            case 0xF4:
                getdpx(&addr);
                ya.b.a = readspc(addr);
                setspczn(ya.b.a);
                spccount += 4;
                break;
            case 0xF5:
                addr = readspc(pc) + (readspc(pc + 1) << 8) + x;
                pc += 2;
                ya.b.a = readspc(addr);
                setspczn(ya.b.a);
                spccount += 5;
                break;
            case 0xF6:
                addr = readspc(pc) + (readspc(pc + 1) << 8) + ya.b.y;
                pc += 2;
                ya.b.a = readspc(addr);
                setspczn(ya.b.a);
                spccount += 5;
                break;
            case 0xF7:
                addr = readspc(pc) + p.p;
                pc++;
                addr2  = readspc(addr) + (readspc(addr + 1) << 8) + ya.b.y;
                ya.b.a = readspc(addr2);
                setspczn(ya.b.a);
                spccount += 6;
                break;
            case 0xF8:
                getdp(&addr);
                x = readspc(addr);
                setspczn(x);
                spccount += 3;
                break;
            case 0xF9:
                getdpy(&addr);
                x = readspc(addr);
                setspczn(x);
                spccount += 3;
                break;
            case 0xFA:
                addr = readspc(pc) + p.p;
                pc++;
                addr2 = readspc(pc) + p.p;
                pc++;
                temp = readspc(addr);
                writespc(addr2, temp);
                spccount += 5;
                break;
            case 0xFB:
                getdpx(&addr);
                ya.b.y = readspc(addr);
                setspczn(ya.b.y);
                spccount += 4;
                break;
            case 0xFC:
                ya.b.y++;
                setspczn(ya.b.y);
                spccount += 2;
                break;
            case 0xFD:
                ya.b.y = ya.b.a;
                setspczn(ya.b.y);
                spccount += 2;
                break;
            case 0xFE:
                addr = readspc(pc);
                pc++;
                if (addr & 0x80)
                    addr |= 0xFF00;
                ya.b.y--;
                if (ya.b.y) {
                    pc += addr;
                    spccount += 2;
                }
                spccount += 4;
                break;
            case 0x13:
                getdp(&addr);
                temp = readspc(addr);
                addr = readspc(pc);
                pc++;
                if (addr & 0x80)
                    addr |= 0xFF00;
                if (!(temp & 0x01)) {
                    pc += addr;
                    spccount += 5;
                }
                spccount += 2;
                break;
            case 0x33:
                getdp(&addr);
                temp = readspc(addr);
                addr = readspc(pc);
                pc++;
                if (addr & 0x80)
                    addr |= 0xFF00;
                if (!(temp & 0x02)) {
                    pc += addr;
                    spccount += 5;
                }
                spccount += 2;
                break;
            case 0x53:
                getdp(&addr);
                temp = readspc(addr);
                addr = readspc(pc);
                pc++;
                if (addr & 0x80)
                    addr |= 0xFF00;
                if (!(temp & 0x04)) {
                    pc += addr;
                    spccount += 5;
                }
                spccount += 2;
                break;
            case 0x73:
                getdp(&addr);
                temp = readspc(addr);
                addr = readspc(pc);
                pc++;
                if (addr & 0x80)
                    addr |= 0xFF00;
                if (!(temp & 0x08)) {
                    pc += addr;
                    spccount += 5;
                }
                spccount += 2;
                break;
            case 0x93:
                getdp(&addr);
                temp = readspc(addr);
                addr = readspc(pc);
                pc++;
                if (addr & 0x80)
                    addr |= 0xFF00;
                if (!(temp & 0x10)) {
                    pc += addr;
                    spccount += 5;
                }
                spccount += 2;
                break;
            case 0xB3:
                getdp(&addr);
                temp = readspc(addr);
                addr = readspc(pc);
                pc++;
                if (addr & 0x80)
                    addr |= 0xFF00;
                if (!(temp & 0x20)) {
                    pc += addr;
                    spccount += 5;
                }
                spccount += 2;
                break;
            case 0xD3:
                getdp(&addr);
                temp = readspc(addr);
                addr = readspc(pc);
                pc++;
                if (addr & 0x80)
                    addr |= 0xFF00;
                if (!(temp & 0x40)) {
                    pc += addr;
                    spccount += 5;
                }
                spccount += 2;
                break;
            case 0xF3:
                getdp(&addr);
                temp = readspc(addr);
                addr = readspc(pc);
                pc++;
                if (addr & 0x80)
                    addr |= 0xFF00;
                if (!(temp & 0x80)) {
                    pc += addr;
                    spccount += 5;
                }
                spccount += 2;
                break;
            case 0x03:
                getdp(&addr);
                temp = readspc(addr);
                addr = readspc(pc);
                pc++;
                if (addr & 0x80)
                    addr |= 0xFF00;
                if ((temp & 0x01)) {
                    pc += addr;
                    spccount += 5;
                }
                spccount += 2;
                break;
            case 0x23:
                getdp(&addr);
                temp = readspc(addr);
                addr = readspc(pc);
                pc++;
                if (addr & 0x80)
                    addr |= 0xFF00;
                if ((temp & 0x02)) {
                    pc += addr;
                    spccount += 5;
                }
                spccount += 2;
                break;
            case 0x43:
                getdp(&addr);
                temp = readspc(addr);
                addr = readspc(pc);
                pc++;
                if (addr & 0x80)
                    addr |= 0xFF00;
                if ((temp & 0x04)) {
                    pc += addr;
                    spccount += 5;
                }
                spccount += 2;
                break;
            case 0x63:
                getdp(&addr);
                temp = readspc(addr);
                addr = readspc(pc);
                pc++;
                if (addr & 0x80)
                    addr |= 0xFF00;
                if ((temp & 0x08)) {
                    pc += addr;
                    spccount += 5;
                }
                spccount += 2;
                break;
            case 0x83:
                getdp(&addr);
                temp = readspc(addr);
                addr = readspc(pc);
                pc++;
                if (addr & 0x80)
                    addr |= 0xFF00;
                if ((temp & 0x10)) {
                    pc += addr;
                    spccount += 5;
                }
                spccount += 2;
                break;
            case 0xA3:
                getdp(&addr);
                temp = readspc(addr);
                addr = readspc(pc);
                pc++;
                if (addr & 0x80)
                    addr |= 0xFF00;
                if ((temp & 0x20)) {
                    pc += addr;
                    spccount += 5;
                }
                spccount += 2;
                break;
            case 0xC3:
                getdp(&addr);
                temp = readspc(addr);
                addr = readspc(pc);
                pc++;
                if (addr & 0x80)
                    addr |= 0xFF00;
                if ((temp & 0x40)) {
                    pc += addr;
                    spccount += 5;
                }
                spccount += 2;
                break;
            case 0xE3:
                getdp(&addr);
                temp = readspc(addr);
                addr = readspc(pc);
                pc++;
                if (addr & 0x80)
                    addr |= 0xFF00;
                if ((temp & 0x80)) {
                    pc += addr;
                    spccount += 5;
                }
                spccount += 2;
                break;
            case 0x21:
                // writespc(sp + 0x100, ya.b.a);
                // sp--;
                // spccount += 4;

                // push(br[PC] >> 8);
                // push(br[PC] & 0xff);
                // int64_t padr = 0xffc0 + ((15 - (instr >> 4)) << 1);
                // br[PC]       = mem->read(padr) | (mem->read(padr + 1) << 8);
                // break;
            default:
                printf("SPC - Bad opcode %02X at %04X\n", opcode, pc);
                // pc--;
                // exit(-1);
        }
        spccycles -= (spccount * 20.9395313f);
        spctimer[0] -= spccount;
        if (spctimer[0] <= 0) {
            spctimer[0] += 128;
            spctimer2[0]++;
            if (spctimer2[0] == spclimit[0]) {
                spctimer2[0] = 0;
                spcram[0xFD]++;
            }
            spctimer2[0] &= 255;
        }
        spctimer[1] -= spccount;
        if (spctimer[1] <= 0) {
            spctimer[1] += 128;
            spctimer2[1]++;
            if (spctimer2[1] == spclimit[1]) {
                spctimer2[1] = 0;
                spcram[0xFE]++;
            }
            spctimer2[1] &= 255;
        }
        spctimer[2] -= spccount;
        if (spctimer[2] <= 0) {
            spctimer[2] += 16;
            spctimer2[2]++;
            if (spctimer2[2] == spclimit[2]) {
                spctimer2[2] = 0;
                spcram[0xFF]++;
            }
            spctimer2[2] &= 255;
        }
        dsptotal -= spccount;
        if (dsptotal <= 0) {
            dsptotal += 32;
            snes->dsp->polldsp();
        }
    }
}
