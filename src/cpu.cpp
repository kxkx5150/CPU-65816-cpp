#include "cpu.h"
#include "snem.h"
#include <cstdint>

#define CALL_MEMBER_FN(object, ptrToMember) ((object).*(ptrToMember))

CPU::CPU(SNES *_snes)
{
    snes = _snes;
}
void CPU::exec()
{
    opcode = snes->readmem(pbr | pc);
    pc++;
    CALL_MEMBER_FN(*this, opcodes[opcode][cpumode])();
}
void CPU::setzn8(int v)
{
    p.z = !(v);
    p.n = (v)&0x80;
}
void CPU::setzn16(int v)
{
    p.z = !(v);
    p.n = (v)&0x8000;
}
void CPU::ADC8(uint16_t tempw, uint8_t temp)
{
    tempw = a.b.l + temp + ((p.c) ? 1 : 0);
    p.v   = (!((a.b.l ^ temp) & 0x80) && ((a.b.l ^ tempw) & 0x80));
    a.b.l = tempw & 0xFF;
    setzn8(a.b.l);
    p.c = tempw & 0x100;
}
void CPU::ADC16(uint16_t tempw, uint32_t templ)
{
    templ = a.w + tempw + ((p.c) ? 1 : 0);
    p.v   = (!((a.w ^ tempw) & 0x8000) && ((a.w ^ templ) & 0x8000));
    a.w   = templ & 0xFFFF;
    setzn16(a.w);
    p.c = templ & 0x10000;
}
void CPU::ADCBCD8(uint16_t tempw, uint8_t temp)
{
    tempw = (a.b.l & 0xF) + (temp & 0xF) + (p.c ? 1 : 0);
    if (tempw > 9) {
        tempw += 6;
    }
    tempw += ((a.b.l & 0xF0) + (temp & 0xF0));
    if (tempw > 0x9F) {
        tempw += 0x60;
    }
    p.v   = (!((a.b.l ^ temp) & 0x80) && ((a.b.l ^ tempw) & 0x80));
    a.b.l = tempw & 0xFF;
    setzn8(a.b.l);
    p.c = tempw > 0xFF;
    cycles -= 6;
    snes->clockspc(6);
}
void CPU::ADCBCD16(uint16_t tempw, uint32_t templ)
{
    templ = (a.w & 0xF) + (tempw & 0xF) + (p.c ? 1 : 0);
    if (templ > 9) {
        templ += 6;
    }
    templ += ((a.w & 0xF0) + (tempw & 0xF0));
    if (templ > 0x9F) {
        templ += 0x60;
    }
    templ += ((a.w & 0xF00) + (tempw & 0xF00));
    if (templ > 0x9FF) {
        templ += 0x600;
    }
    templ += ((a.w & 0xF000) + (tempw & 0xF000));
    if (templ > 0x9FFF) {
        templ += 0x6000;
    }
    p.v = (!((a.w ^ tempw) & 0x8000) && ((a.w ^ templ) & 0x8000));
    a.w = templ & 0xFFFF;
    setzn16(a.w);
    p.c = templ > 0xFFFF;
    cycles -= 6;
    snes->clockspc(6);
}
void CPU::SBC8(uint16_t tempw, uint8_t temp)
{
    tempw = a.b.l - temp - ((p.c) ? 0 : 1);
    p.v   = (((a.b.l ^ temp) & 0x80) && ((a.b.l ^ tempw) & 0x80));
    a.b.l = tempw & 0xFF;
    setzn8(a.b.l);
    p.c = tempw <= 0xFF;
}
void CPU::SBC16(uint16_t tempw, uint32_t templ)
{
    templ = a.w - tempw - ((p.c) ? 0 : 1);
    p.v   = (((a.w ^ tempw) & (a.w ^ templ)) & 0x8000);
    a.w   = templ & 0xFFFF;
    setzn16(a.w);
    p.c = templ <= 0xFFFF;
}
void CPU::SBCBCD8(uint16_t tempw, uint8_t temp)
{
    tempw = (a.b.l & 0xF) - (temp & 0xF) - (p.c ? 0 : 1);
    if (tempw > 9) {
        tempw -= 6;
    }
    tempw += ((a.b.l & 0xF0) - (temp & 0xF0));
    if (tempw > 0x9F) {
        tempw -= 0x60;
    }
    p.v   = (((a.b.l ^ temp) & 0x80) && ((a.b.l ^ tempw) & 0x80));
    a.b.l = tempw & 0xFF;
    setzn8(a.b.l);
    p.c = tempw <= 0xFF;
    cycles -= 6;
    snes->clockspc(6);
}
void CPU::SBCBCD16(uint16_t tempw, uint32_t templ)
{
    templ = (a.w & 0xF) - (tempw & 0xF) - (p.c ? 0 : 1);
    if (templ > 9) {
        templ -= 6;
    }
    templ += ((a.w & 0xF0) - (tempw & 0xF0));
    if (templ > 0x9F) {
        templ -= 0x60;
    }
    templ += ((a.w & 0xF00) - (tempw & 0xF00));
    if (templ > 0x9FF) {
        templ -= 0x600;
    }
    templ += ((a.w & 0xF000) - (tempw & 0xF000));
    if (templ > 0x9FFF) {
        templ -= 0x6000;
    }
    p.v = (((a.w ^ tempw) & 0x8000) && ((a.w ^ templ) & 0x8000));
    a.w = templ & 0xFFFF;
    setzn16(a.w);
    p.c = templ <= 0xFFFF;
    cycles -= 6;
    snes->clockspc(6);
}
void CPU::updatecpumode()
{
    if (p.e) {
        cpumode = 4;
        x.b.h = y.b.h = 0;
    } else {
        cpumode = 0;
        if (!p.m)
            cpumode |= 1;
        if (!p.x)
            cpumode |= 2;
        if (p.x)
            x.b.h = y.b.h = 0;
    }
}
uint32_t CPU::absolute()
{
    uint32_t temp = snes->readmemw(pbr | pc);
    pc += 2;
    return temp | dbr;
}
uint32_t CPU::absolutex()
{
    uint32_t temp = (snes->readmemw(pbr | pc)) + x.w + dbr;
    pc += 2;
    return temp;
}
uint32_t CPU::absolutey()
{
    uint32_t temp = (snes->readmemw(pbr | pc)) + y.w + dbr;
    pc += 2;
    return temp;
}
uint32_t CPU::absolutelong()
{
    uint32_t temp = snes->readmemw(pbr | pc);
    pc += 2;
    temp |= (snes->readmem(pbr | pc) << 16);
    pc++;
    return temp;
}
uint32_t CPU::absolutelongx()
{
    uint32_t temp = (snes->readmemw(pbr | pc)) + x.w;
    pc += 2;
    temp += (snes->readmem(pbr | pc) << 16);
    pc++;
    return temp;
}
uint32_t CPU::zeropage()
{
    uint32_t temp = snes->readmem(pbr | pc);
    pc++;
    temp += dp;
    if (dp & 0xFF) {
        cycles -= 6;
        snes->clockspc(6);
    }
    return temp & 0xFFFF;
}
uint32_t CPU::zeropagex()
{
    uint32_t temp = snes->readmem(pbr | pc) + x.w;
    pc++;
    if (p.e)
        temp &= 0xFF;
    temp += dp;
    if (dp & 0xFF) {
        cycles -= 6;
        snes->clockspc(6);
    }
    return temp & 0xFFFF;
}
uint32_t CPU::zeropagey()
{
    uint32_t temp = snes->readmem(pbr | pc) + y.w;
    pc++;
    if (p.e)
        temp &= 0xFF;
    temp += dp;
    if (dp & 0xFF) {
        cycles -= 6;
        snes->clockspc(6);
    }
    return temp & 0xFFFF;
}
uint32_t CPU::stack()
{
    uint32_t temp = snes->readmem(pbr | pc);
    pc++;
    temp += s.w;
    return temp & 0xFFFF;
}
uint32_t CPU::indirect()
{
    uint32_t temp = (snes->readmem(pbr | pc) + dp) & 0xFFFF;
    pc++;
    return (snes->readmemw(temp)) + dbr;
}
uint32_t CPU::indirectx()
{
    uint32_t temp = (snes->readmem(pbr | pc) + dp + x.w) & 0xFFFF;
    pc++;
    return (snes->readmemw(temp)) + dbr;
}
uint32_t CPU::jindirectx()
{
    uint32_t temp = (snes->readmem(pbr | pc) + (snes->readmem((pbr | pc) + 1) << 8) + x.w) + pbr;
    pc += 2;
    return temp;
}
uint32_t CPU::indirecty()
{
    uint32_t temp = (snes->readmem(pbr | pc) + dp) & 0xFFFF;
    pc++;
    return (snes->readmemw(temp)) + y.w + dbr;
}
uint32_t CPU::sindirecty()
{
    uint32_t temp = (snes->readmem(pbr | pc) + s.w) & 0xFFFF;
    pc++;
    return (snes->readmemw(temp)) + y.w + dbr;
}
uint32_t CPU::indirectl()
{
    uint32_t temp, addr;
    temp = (snes->readmem(pbr | pc) + dp) & 0xFFFF;
    pc++;
    addr = snes->readmemw(temp) | (snes->readmem(temp + 2) << 16);
    return addr;
}
uint32_t CPU::indirectly()
{
    uint32_t temp, addr;
    temp = (snes->readmem(pbr | pc) + dp) & 0xFFFF;
    pc++;
    addr = (snes->readmemw(temp) | (snes->readmem(temp + 2) << 16)) + y.w;
    return addr;
}
void CPU::inca8()
{
    snes->readmem(pbr | pc);
    a.b.l++;
    setzn8(a.b.l);
}
void CPU::inca16()
{
    snes->readmem(pbr | pc);
    a.w++;
    setzn16(a.w);
}
void CPU::inx8()
{
    snes->readmem(pbr | pc);
    x.b.l++;
    setzn8(x.b.l);
}
void CPU::inx16()
{
    snes->readmem(pbr | pc);
    x.w++;
    setzn16(x.w);
}
void CPU::iny8()
{
    snes->readmem(pbr | pc);
    y.b.l++;
    setzn8(y.b.l);
}
void CPU::iny16()
{
    snes->readmem(pbr | pc);
    y.w++;
    setzn16(y.w);
}
void CPU::deca8()
{
    snes->readmem(pbr | pc);
    a.b.l--;
    setzn8(a.b.l);
}
void CPU::deca16()
{
    snes->readmem(pbr | pc);
    a.w--;
    setzn16(a.w);
}
void CPU::dex8()
{
    snes->readmem(pbr | pc);
    x.b.l--;
    setzn8(x.b.l);
}
void CPU::dex16()
{
    snes->readmem(pbr | pc);
    x.w--;
    setzn16(x.w);
}
void CPU::dey8()
{
    snes->readmem(pbr | pc);
    y.b.l--;
    setzn8(y.b.l);
}
void CPU::dey16()
{
    snes->readmem(pbr | pc);
    y.w--;
    setzn16(y.w);
}
void CPU::incZp8()
{
    uint8_t temp;
    addr = zeropage();
    temp = snes->readmem(addr);
    cycles -= 6;
    snes->clockspc(6);
    temp++;
    setzn8(temp);
    snes->writemem(addr, temp);
}
void CPU::incZp16()
{
    uint16_t temp;
    addr = zeropage();
    temp = snes->readmemw(addr);
    cycles -= 6;
    snes->clockspc(6);
    temp++;
    setzn16(temp);
    snes->writememw2(addr, temp);
}
void CPU::incZpx8()
{
    uint8_t temp;
    addr = zeropagex();
    temp = snes->readmem(addr);
    cycles -= 6;
    snes->clockspc(6);
    temp++;
    setzn8(temp);
    snes->writemem(addr, temp);
}
void CPU::incZpx16()
{
    uint16_t temp;
    addr = zeropagex();
    temp = snes->readmemw(addr);
    cycles -= 6;
    snes->clockspc(6);
    temp++;
    setzn16(temp);
    snes->writememw2(addr, temp);
}
void CPU::incAbs8()
{
    uint8_t temp;
    addr = absolute();
    temp = snes->readmem(addr);
    cycles -= 6;
    snes->clockspc(6);
    temp++;
    setzn8(temp);
    snes->writemem(addr, temp);
}
void CPU::incAbs16()
{
    uint16_t temp;
    addr = absolute();
    temp = snes->readmemw(addr);
    cycles -= 6;
    snes->clockspc(6);
    temp++;
    setzn16(temp);
    snes->writememw2(addr, temp);
}
void CPU::incAbsx8()
{
    uint8_t temp;
    addr = absolutex();
    temp = snes->readmem(addr);
    cycles -= 6;
    snes->clockspc(6);
    temp++;
    setzn8(temp);
    snes->writemem(addr, temp);
}
void CPU::incAbsx16()
{
    uint16_t temp;
    addr = absolutex();
    temp = snes->readmemw(addr);
    cycles -= 6;
    snes->clockspc(6);
    temp++;
    setzn16(temp);
    snes->writememw2(addr, temp);
}
void CPU::decZp8()
{
    uint8_t temp;
    addr = zeropage();
    temp = snes->readmem(addr);
    cycles -= 6;
    snes->clockspc(6);
    temp--;
    setzn8(temp);
    snes->writemem(addr, temp);
}
void CPU::decZp16()
{
    uint16_t temp;
    addr = zeropage();
    temp = snes->readmemw(addr);
    cycles -= 6;
    snes->clockspc(6);
    temp--;
    setzn16(temp);
    snes->writememw2(addr, temp);
}
void CPU::decZpx8()
{
    uint8_t temp;
    addr = zeropagex();
    temp = snes->readmem(addr);
    cycles -= 6;
    snes->clockspc(6);
    temp--;
    setzn8(temp);
    snes->writemem(addr, temp);
}
void CPU::decZpx16()
{
    uint16_t temp;
    addr = zeropagex();
    temp = snes->readmemw(addr);
    cycles -= 6;
    snes->clockspc(6);
    temp--;
    setzn16(temp);
    snes->writememw2(addr, temp);
}
void CPU::decAbs8()
{
    uint8_t temp;
    addr = absolute();
    temp = snes->readmem(addr);
    cycles -= 6;
    snes->clockspc(6);
    temp--;
    setzn8(temp);
    snes->writemem(addr, temp);
}
void CPU::decAbs16()
{
    uint16_t temp;
    addr = absolute();
    temp = snes->readmemw(addr);
    cycles -= 6;
    snes->clockspc(6);
    temp--;
    setzn16(temp);
    snes->writememw2(addr, temp);
}
void CPU::decAbsx8()
{
    uint8_t temp;
    addr = absolutex();
    temp = snes->readmem(addr);
    cycles -= 6;
    snes->clockspc(6);
    temp--;
    setzn8(temp);
    snes->writemem(addr, temp);
}
void CPU::decAbsx16()
{
    uint16_t temp;
    addr = absolutex();
    temp = snes->readmemw(addr);
    cycles -= 6;
    snes->clockspc(6);
    temp--;
    setzn16(temp);
    snes->writememw2(addr, temp);
}
void CPU::clc()
{
    snes->readmem(pbr | pc);
    p.c = 0;
}
void CPU::cld()
{
    snes->readmem(pbr | pc);
    p.d = 0;
}
void CPU::cli()
{
    snes->readmem(pbr | pc);
    p.i = 0;
}
void CPU::clv()
{
    snes->readmem(pbr | pc);
    p.v = 0;
}
void CPU::sec()
{
    snes->readmem(pbr | pc);
    p.c = 1;
}
void CPU::sed()
{
    snes->readmem(pbr | pc);
    p.d = 1;
}
void CPU::sei()
{
    snes->readmem(pbr | pc);
    p.i = 1;
}
void CPU::xce()
{
    int temp = p.c;
    p.c      = p.e;
    p.e      = temp;
    snes->readmem(pbr | pc);
    updatecpumode();
}
void CPU::sep()
{
    uint8_t temp = snes->readmem(pbr | pc);
    pc++;
    if (temp & 1)
        p.c = 1;
    if (temp & 2)
        p.z = 1;
    if (temp & 4)
        p.i = 1;
    if (temp & 8)
        p.d = 1;
    if (temp & 0x40)
        p.v = 1;
    if (temp & 0x80)
        p.n = 1;
    if (!p.e) {
        if (temp & 0x10)
            p.x = 1;
        if (temp & 0x20)
            p.m = 1;
        updatecpumode();
    }
}
void CPU::rep()
{
    uint8_t temp = snes->readmem(pbr | pc);
    pc++;
    if (temp & 1)
        p.c = 0;
    if (temp & 2)
        p.z = 0;
    if (temp & 4)
        p.i = 0;
    if (temp & 8)
        p.d = 0;
    if (temp & 0x40)
        p.v = 0;
    if (temp & 0x80)
        p.n = 0;
    if (!p.e) {
        if (temp & 0x10)
            p.x = 0;
        if (temp & 0x20)
            p.m = 0;
        updatecpumode();
    }
}
void CPU::tax8()
{
    snes->readmem(pbr | pc);
    x.b.l = a.b.l;
    setzn8(x.b.l);
}
void CPU::tay8()
{
    snes->readmem(pbr | pc);
    y.b.l = a.b.l;
    setzn8(y.b.l);
}
void CPU::txa8()
{
    snes->readmem(pbr | pc);
    a.b.l = x.b.l;
    setzn8(a.b.l);
}
void CPU::tya8()
{
    snes->readmem(pbr | pc);
    a.b.l = y.b.l;
    setzn8(a.b.l);
}
void CPU::tsx8()
{
    snes->readmem(pbr | pc);
    x.b.l = s.b.l;
    setzn8(x.b.l);
}
void CPU::txs8()
{
    snes->readmem(pbr | pc);
    s.b.l = x.b.l;
}
void CPU::txy8()
{
    snes->readmem(pbr | pc);
    y.b.l = x.b.l;
    setzn8(y.b.l);
}
void CPU::tyx8()
{
    snes->readmem(pbr | pc);
    x.b.l = y.b.l;
    setzn8(x.b.l);
}
void CPU::tax16()
{
    snes->readmem(pbr | pc);
    x.w = a.w;
    setzn16(x.w);
}
void CPU::tay16()
{
    snes->readmem(pbr | pc);
    y.w = a.w;
    setzn16(y.w);
}
void CPU::txa16()
{
    snes->readmem(pbr | pc);
    a.w = x.w;
    setzn16(a.w);
}
void CPU::tya16()
{
    snes->readmem(pbr | pc);
    a.w = y.w;
    setzn16(a.w);
}
void CPU::tsx16()
{
    snes->readmem(pbr | pc);
    x.w = s.w;
    setzn16(x.w);
}
void CPU::txs16()
{
    snes->readmem(pbr | pc);
    s.w = x.w;
}
void CPU::txy16()
{
    snes->readmem(pbr | pc);
    y.w = x.w;
    setzn16(y.w);
}
void CPU::tyx16()
{
    snes->readmem(pbr | pc);
    x.w = y.w;
    setzn16(x.w);
}
void CPU::ldxImm8()
{
    x.b.l = snes->readmem(pbr | pc);
    pc++;
    setzn8(x.b.l);
}
void CPU::ldxZp8()
{
    addr  = zeropage();
    x.b.l = snes->readmem(addr);
    setzn8(x.b.l);
}
void CPU::ldxZpy8()
{
    addr  = zeropagey();
    x.b.l = snes->readmem(addr);
    setzn8(x.b.l);
}
void CPU::ldxAbs8()
{
    addr  = absolute();
    x.b.l = snes->readmem(addr);
    setzn8(x.b.l);
}
void CPU::ldxAbsy8()
{
    addr  = absolutey();
    x.b.l = snes->readmem(addr);
    setzn8(x.b.l);
}
void CPU::ldxImm16()
{
    x.w = snes->readmemw(pbr | pc);
    pc += 2;
    setzn16(x.w);
}
void CPU::ldxZp16()
{
    addr = zeropage();
    x.w  = snes->readmemw(addr);
    setzn16(x.w);
}
void CPU::ldxZpy16()
{
    addr = zeropagey();
    x.w  = snes->readmemw(addr);
    setzn16(x.w);
}
void CPU::ldxAbs16()
{
    addr = absolute();
    x.w  = snes->readmemw(addr);
    setzn16(x.w);
}
void CPU::ldxAbsy16()
{
    addr = absolutey();
    x.w  = snes->readmemw(addr);
    setzn16(x.w);
}
void CPU::ldyImm8()
{
    y.b.l = snes->readmem(pbr | pc);
    pc++;
    setzn8(y.b.l);
}
void CPU::ldyZp8()
{
    addr  = zeropage();
    y.b.l = snes->readmem(addr);
    setzn8(y.b.l);
}
void CPU::ldyZpx8()
{
    addr  = zeropagex();
    y.b.l = snes->readmem(addr);
    setzn8(y.b.l);
}
void CPU::ldyAbs8()
{
    addr  = absolute();
    y.b.l = snes->readmem(addr);
    setzn8(y.b.l);
}
void CPU::ldyAbsx8()
{
    addr  = absolutex();
    y.b.l = snes->readmem(addr);
    setzn8(y.b.l);
}
void CPU::ldyImm16()
{
    y.w = snes->readmemw(pbr | pc);
    pc += 2;
    setzn16(y.w);
}
void CPU::ldyZp16()
{
    addr = zeropage();
    y.w  = snes->readmemw(addr);
    setzn16(y.w);
}
void CPU::ldyZpx16()
{
    addr = zeropagex();
    y.w  = snes->readmemw(addr);
    setzn16(y.w);
}
void CPU::ldyAbs16()
{
    addr = absolute();
    y.w  = snes->readmemw(addr);
    setzn16(y.w);
}
void CPU::ldyAbsx16()
{
    addr = absolutex();
    y.w  = snes->readmemw(addr);
    setzn16(y.w);
}
void CPU::ldaImm8()
{
    a.b.l = snes->readmem(pbr | pc);
    pc++;
    setzn8(a.b.l);
}
void CPU::ldaZp8()
{
    addr  = zeropage();
    a.b.l = snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::ldaZpx8()
{
    addr  = zeropagex();
    a.b.l = snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::ldaSp8()
{
    addr  = stack();
    a.b.l = snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::ldaSIndirecty8()
{
    addr  = sindirecty();
    a.b.l = snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::ldaAbs8()
{
    addr  = absolute();
    a.b.l = snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::ldaAbsx8()
{
    addr  = absolutex();
    a.b.l = snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::ldaAbsy8()
{
    addr  = absolutey();
    a.b.l = snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::ldaLong8()
{
    addr  = absolutelong();
    a.b.l = snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::ldaLongx8()
{
    addr  = absolutelongx();
    a.b.l = snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::ldaIndirect8()
{
    addr  = indirect();
    a.b.l = snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::ldaIndirectx8()
{
    addr  = indirectx();
    a.b.l = snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::ldaIndirecty8()
{
    addr  = indirecty();
    a.b.l = snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::ldaIndirectLong8()
{
    addr  = indirectl();
    a.b.l = snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::ldaIndirectLongy8()
{
    addr  = indirectly();
    a.b.l = snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::ldaImm16()
{
    a.w = snes->readmemw(pbr | pc);
    pc += 2;
    setzn16(a.w);
}
void CPU::ldaZp16()
{
    addr = zeropage();
    a.w  = snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::ldaZpx16()
{
    addr = zeropagex();
    a.w  = snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::ldaSp16()
{
    addr = stack();
    a.w  = snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::ldaSIndirecty16()
{
    addr = sindirecty();
    a.w  = snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::ldaAbs16()
{
    addr = absolute();
    a.w  = snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::ldaAbsx16()
{
    addr = absolutex();
    a.w  = snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::ldaAbsy16()
{
    addr = absolutey();
    a.w  = snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::ldaLong16()
{
    addr = absolutelong();
    a.w  = snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::ldaLongx16()
{
    addr = absolutelongx();
    a.w  = snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::ldaIndirect16()
{
    addr = indirect();
    a.w  = snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::ldaIndirectx16()
{
    addr = indirectx();
    a.w  = snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::ldaIndirecty16()
{
    addr = indirecty();
    a.w  = snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::ldaIndirectLong16()
{
    addr = indirectl();
    a.w  = snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::ldaIndirectLongy16()
{
    addr = indirectly();
    a.w  = snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::staZp8()
{
    addr = zeropage();
    snes->writemem(addr, a.b.l);
}
void CPU::staZpx8()
{
    addr = zeropagex();
    snes->writemem(addr, a.b.l);
}
void CPU::staAbs8()
{
    addr = absolute();
    snes->writemem(addr, a.b.l);
}
void CPU::staAbsx8()
{
    addr = absolutex();
    snes->writemem(addr, a.b.l);
}
void CPU::staAbsy8()
{
    addr = absolutey();
    snes->writemem(addr, a.b.l);
}
void CPU::staLong8()
{
    addr = absolutelong();
    snes->writemem(addr, a.b.l);
}
void CPU::staLongx8()
{
    addr = absolutelongx();
    snes->writemem(addr, a.b.l);
}
void CPU::staIndirect8()
{
    addr = indirect();
    snes->writemem(addr, a.b.l);
}
void CPU::staIndirectx8()
{
    addr = indirectx();
    snes->writemem(addr, a.b.l);
}
void CPU::staIndirecty8()
{
    addr = indirecty();
    snes->writemem(addr, a.b.l);
}
void CPU::staIndirectLong8()
{
    addr = indirectl();
    snes->writemem(addr, a.b.l);
}
void CPU::staIndirectLongy8()
{
    addr = indirectly();
    snes->writemem(addr, a.b.l);
}
void CPU::staSp8()
{
    addr = stack();
    snes->writemem(addr, a.b.l);
}
void CPU::staSIndirecty8()
{
    addr = sindirecty();
    snes->writemem(addr, a.b.l);
}
void CPU::staZp16()
{
    addr = zeropage();
    snes->writememw(addr, a.w);
}
void CPU::staZpx16()
{
    addr = zeropagex();
    snes->writememw(addr, a.w);
}
void CPU::staAbs16()
{
    addr = absolute();
    snes->writememw(addr, a.w);
}
void CPU::staAbsx16()
{
    addr = absolutex();
    snes->writememw(addr, a.w);
}
void CPU::staAbsy16()
{
    addr = absolutey();
    snes->writememw(addr, a.w);
}
void CPU::staLong16()
{
    addr = absolutelong();
    snes->writememw(addr, a.w);
}
void CPU::staLongx16()
{
    addr = absolutelongx();
    snes->writememw(addr, a.w);
}
void CPU::staIndirect16()
{
    addr = indirect();
    snes->writememw(addr, a.w);
}
void CPU::staIndirectx16()
{
    addr = indirectx();
    snes->writememw(addr, a.w);
}
void CPU::staIndirecty16()
{
    addr = indirecty();
    snes->writememw(addr, a.w);
}
void CPU::staIndirectLong16()
{
    addr = indirectl();
    snes->writememw(addr, a.w);
}
void CPU::staIndirectLongy16()
{
    addr = indirectly();
    snes->writememw(addr, a.w);
}
void CPU::staSp16()
{
    addr = stack();
    snes->writememw(addr, a.w);
}
void CPU::staSIndirecty16()
{
    addr = sindirecty();
    snes->writememw(addr, a.w);
}
void CPU::stxZp8()
{
    addr = zeropage();
    snes->writemem(addr, x.b.l);
}
void CPU::stxZpy8()
{
    addr = zeropagey();
    snes->writemem(addr, x.b.l);
}
void CPU::stxAbs8()
{
    addr = absolute();
    snes->writemem(addr, x.b.l);
}
void CPU::stxZp16()
{
    addr = zeropage();
    snes->writememw(addr, x.w);
}
void CPU::stxZpy16()
{
    addr = zeropagey();
    snes->writememw(addr, x.w);
}
void CPU::stxAbs16()
{
    addr = absolute();
    snes->writememw(addr, x.w);
}
void CPU::styZp8()
{
    addr = zeropage();
    snes->writemem(addr, y.b.l);
}
void CPU::styZpx8()
{
    addr = zeropagex();
    snes->writemem(addr, y.b.l);
}
void CPU::styAbs8()
{
    addr = absolute();
    snes->writemem(addr, y.b.l);
}
void CPU::styZp16()
{
    addr = zeropage();
    snes->writememw(addr, y.w);
}
void CPU::styZpx16()
{
    addr = zeropagex();
    snes->writememw(addr, y.w);
}
void CPU::styAbs16()
{
    addr = absolute();
    snes->writememw(addr, y.w);
}
void CPU::stzZp8()
{
    addr = zeropage();
    snes->writemem(addr, 0);
}
void CPU::stzZpx8()
{
    addr = zeropagex();
    snes->writemem(addr, 0);
}
void CPU::stzAbs8()
{
    addr = absolute();
    snes->writemem(addr, 0);
}
void CPU::stzAbsx8()
{
    addr = absolutex();
    snes->writemem(addr, 0);
}
void CPU::stzZp16()
{
    addr = zeropage();
    snes->writememw(addr, 0);
}
void CPU::stzZpx16()
{
    addr = zeropagex();
    snes->writememw(addr, 0);
}
void CPU::stzAbs16()
{
    addr = absolute();
    snes->writememw(addr, 0);
}
void CPU::stzAbsx16()
{
    addr = absolutex();
    snes->writememw(addr, 0);
}
void CPU::adcImm8()
{
    uint16_t tempw;
    uint8_t  temp = snes->readmem(pbr | pc);
    pc++;
    if (p.d) {
        ADCBCD8(tempw, temp);
    } else {
        ADC8(tempw, temp);
    }
}
void CPU::adcZp8()
{
    uint16_t tempw;
    uint8_t  temp;
    addr = zeropage();
    temp = snes->readmem(addr);
    if (p.d) {
        ADCBCD8(tempw, temp);
    } else {
        ADC8(tempw, temp);
    }
}
void CPU::adcZpx8()
{
    uint16_t tempw;
    uint8_t  temp;
    addr = zeropagex();
    temp = snes->readmem(addr);
    if (p.d) {
        ADCBCD8(tempw, temp);
    } else {
        ADC8(tempw, temp);
    }
}
void CPU::adcSp8()
{
    uint16_t tempw;
    uint8_t  temp;
    addr = stack();
    temp = snes->readmem(addr);
    if (p.d) {
        ADCBCD8(tempw, temp);
    } else {
        ADC8(tempw, temp);
    }
}
void CPU::adcAbs8()
{
    uint16_t tempw;
    uint8_t  temp;
    addr = absolute();
    temp = snes->readmem(addr);
    if (p.d) {
        ADCBCD8(tempw, temp);
    } else {
        ADC8(tempw, temp);
    }
}
void CPU::adcAbsx8()
{
    uint16_t tempw;
    uint8_t  temp;
    addr = absolutex();
    temp = snes->readmem(addr);
    if (p.d) {
        ADCBCD8(tempw, temp);
    } else {
        ADC8(tempw, temp);
    }
}
void CPU::adcAbsy8()
{
    uint16_t tempw;
    uint8_t  temp;
    addr = absolutey();
    temp = snes->readmem(addr);
    if (p.d) {
        ADCBCD8(tempw, temp);
    } else {
        ADC8(tempw, temp);
    }
}
void CPU::adcLong8()
{
    uint16_t tempw;
    uint8_t  temp;
    addr = absolutelong();
    temp = snes->readmem(addr);
    if (p.d) {
        ADCBCD8(tempw, temp);
    } else {
        ADC8(tempw, temp);
    }
}
void CPU::adcLongx8()
{
    uint16_t tempw;
    uint8_t  temp;
    addr = absolutelongx();
    temp = snes->readmem(addr);
    if (p.d) {
        ADCBCD8(tempw, temp);
    } else {
        ADC8(tempw, temp);
    }
}
void CPU::adcIndirect8()
{
    uint16_t tempw;
    uint8_t  temp;
    addr = indirect();
    temp = snes->readmem(addr);
    if (p.d) {
        ADCBCD8(tempw, temp);
    } else {
        ADC8(tempw, temp);
    }
}
void CPU::adcIndirectx8()
{
    uint16_t tempw;
    uint8_t  temp;
    addr = indirectx();
    temp = snes->readmem(addr);
    if (p.d) {
        ADCBCD8(tempw, temp);
    } else {
        ADC8(tempw, temp);
    }
}
void CPU::adcIndirecty8()
{
    uint16_t tempw;
    uint8_t  temp;
    addr = indirecty();
    temp = snes->readmem(addr);
    if (p.d) {
        ADCBCD8(tempw, temp);
    } else {
        ADC8(tempw, temp);
    }
}
void CPU::adcsIndirecty8()
{
    uint16_t tempw;
    uint8_t  temp;
    addr = sindirecty();
    temp = snes->readmem(addr);
    if (p.d) {
        ADCBCD8(tempw, temp);
    } else {
        ADC8(tempw, temp);
    }
}
void CPU::adcIndirectLong8()
{
    uint16_t tempw;
    uint8_t  temp;
    addr = indirectl();
    temp = snes->readmem(addr);
    if (p.d) {
        ADCBCD8(tempw, temp);
    } else {
        ADC8(tempw, temp);
    }
}
void CPU::adcIndirectLongy8()
{
    uint16_t tempw;
    uint8_t  temp;
    addr = indirectly();
    temp = snes->readmem(addr);
    if (p.d) {
        ADCBCD8(tempw, temp);
    } else {
        ADC8(tempw, temp);
    }
}
void CPU::adcImm16()
{
    uint32_t templ;
    uint16_t tempw;
    tempw = snes->readmemw(pbr | pc);
    pc += 2;
    if (p.d) {
        ADCBCD16(tempw, templ);
    } else {
        ADC16(tempw, templ);
    }
}
void CPU::adcZp16()
{
    uint32_t templ;
    uint16_t tempw;
    addr  = zeropage();
    tempw = snes->readmemw(addr);
    if (p.d) {
        ADCBCD16(tempw, templ);
    } else {
        ADC16(tempw, templ);
    }
}
void CPU::adcZpx16()
{
    uint32_t templ;
    uint16_t tempw;
    addr  = zeropagex();
    tempw = snes->readmemw(addr);
    if (p.d) {
        ADCBCD16(tempw, templ);
    } else {
        ADC16(tempw, templ);
    }
}
void CPU::adcSp16()
{
    uint32_t templ;
    uint16_t tempw;
    addr  = stack();
    tempw = snes->readmemw(addr);
    if (p.d) {
        ADCBCD16(tempw, templ);
    } else {
        ADC16(tempw, templ);
    }
}
void CPU::adcAbs16()
{
    uint32_t templ;
    uint16_t tempw;
    addr  = absolute();
    tempw = snes->readmemw(addr);
    if (p.d) {
        ADCBCD16(tempw, templ);
    } else {
        ADC16(tempw, templ);
    }
}
void CPU::adcAbsx16()
{
    uint32_t templ;
    uint16_t tempw;
    addr  = absolutex();
    tempw = snes->readmemw(addr);
    if (p.d) {
        ADCBCD16(tempw, templ);
    } else {
        ADC16(tempw, templ);
    }
}
void CPU::adcAbsy16()
{
    uint32_t templ;
    uint16_t tempw;
    addr  = absolutey();
    tempw = snes->readmemw(addr);
    if (p.d) {
        ADCBCD16(tempw, templ);
    } else {
        ADC16(tempw, templ);
    }
}
void CPU::adcLong16()
{
    uint32_t templ;
    uint16_t tempw;
    addr  = absolutelong();
    tempw = snes->readmemw(addr);
    if (p.d) {
        ADCBCD16(tempw, templ);
    } else {
        ADC16(tempw, templ);
    }
}
void CPU::adcLongx16()
{
    uint32_t templ;
    uint16_t tempw;
    addr  = absolutelongx();
    tempw = snes->readmemw(addr);
    if (p.d) {
        ADCBCD16(tempw, templ);
    } else {
        ADC16(tempw, templ);
    }
}
void CPU::adcIndirect16()
{
    uint32_t templ;
    uint16_t tempw;
    addr  = indirect();
    tempw = snes->readmemw(addr);
    if (p.d) {
        ADCBCD16(tempw, templ);
    } else {
        ADC16(tempw, templ);
    }
}
void CPU::adcIndirectx16()
{
    uint32_t templ;
    uint16_t tempw;
    addr  = indirectx();
    tempw = snes->readmemw(addr);
    if (p.d) {
        ADCBCD16(tempw, templ);
    } else {
        ADC16(tempw, templ);
    }
}
void CPU::adcIndirecty16()
{
    uint32_t templ;
    uint16_t tempw;
    addr  = indirecty();
    tempw = snes->readmemw(addr);
    if (p.d) {
        ADCBCD16(tempw, templ);
    } else {
        ADC16(tempw, templ);
    }
}
void CPU::adcsIndirecty16()
{
    uint32_t templ;
    uint16_t tempw;
    addr  = sindirecty();
    tempw = snes->readmemw(addr);
    if (p.d) {
        ADCBCD16(tempw, templ);
    } else {
        ADC16(tempw, templ);
    }
}
void CPU::adcIndirectLong16()
{
    uint32_t templ;
    uint16_t tempw;
    addr  = indirectl();
    tempw = snes->readmemw(addr);
    if (p.d) {
        ADCBCD16(tempw, templ);
    } else {
        ADC16(tempw, templ);
    }
}
void CPU::adcIndirectLongy16()
{
    uint32_t templ;
    uint16_t tempw;
    addr  = indirectly();
    tempw = snes->readmemw(addr);
    if (p.d) {
        ADCBCD16(tempw, templ);
    } else {
        ADC16(tempw, templ);
    }
}
void CPU::sbcImm8()
{
    uint16_t tempw;
    uint8_t  temp = snes->readmem(pbr | pc);
    pc++;
    if (p.d) {
        SBCBCD8(tempw, temp);
    } else {
        SBC8(tempw, temp);
    }
}
void CPU::sbcZp8()
{
    uint16_t tempw;
    uint8_t  temp;
    addr = zeropage();
    temp = snes->readmem(addr);
    if (p.d) {
        SBCBCD8(tempw, temp);
    } else {
        SBC8(tempw, temp);
    }
}
void CPU::sbcZpx8()
{
    uint16_t tempw;
    uint8_t  temp;
    addr = zeropagex();
    temp = snes->readmem(addr);
    if (p.d) {
        SBCBCD8(tempw, temp);
    } else {
        SBC8(tempw, temp);
    }
}
void CPU::sbcSp8()
{
    uint16_t tempw;
    uint8_t  temp;
    addr = stack();
    temp = snes->readmem(addr);
    if (p.d) {
        SBCBCD8(tempw, temp);
    } else {
        SBC8(tempw, temp);
    }
}
void CPU::sbcAbs8()
{
    uint16_t tempw;
    uint8_t  temp;
    addr = absolute();
    temp = snes->readmem(addr);
    if (p.d) {
        SBCBCD8(tempw, temp);
    } else {
        SBC8(tempw, temp);
    }
}
void CPU::sbcAbsx8()
{
    uint16_t tempw;
    uint8_t  temp;
    addr = absolutex();
    temp = snes->readmem(addr);
    if (p.d) {
        SBCBCD8(tempw, temp);
    } else {
        SBC8(tempw, temp);
    }
}
void CPU::sbcAbsy8()
{
    uint16_t tempw;
    uint8_t  temp;
    addr = absolutey();
    temp = snes->readmem(addr);
    if (p.d) {
        SBCBCD8(tempw, temp);
    } else {
        SBC8(tempw, temp);
    }
}
void CPU::sbcLong8()
{
    uint16_t tempw;
    uint8_t  temp;
    addr = absolutelong();
    temp = snes->readmem(addr);
    if (p.d) {
        SBCBCD8(tempw, temp);
    } else {
        SBC8(tempw, temp);
    }
}
void CPU::sbcLongx8()
{
    uint16_t tempw;
    uint8_t  temp;
    addr = absolutelongx();
    temp = snes->readmem(addr);
    if (p.d) {
        SBCBCD8(tempw, temp);
    } else {
        SBC8(tempw, temp);
    }
}
void CPU::sbcIndirect8()
{
    uint16_t tempw;
    uint8_t  temp;
    addr = indirect();
    temp = snes->readmem(addr);
    if (p.d) {
        SBCBCD8(tempw, temp);
    } else {
        SBC8(tempw, temp);
    }
}
void CPU::sbcIndirectx8()
{
    uint16_t tempw;
    uint8_t  temp;
    addr = indirectx();
    temp = snes->readmem(addr);
    if (p.d) {
        SBCBCD8(tempw, temp);
    } else {
        SBC8(tempw, temp);
    }
}
void CPU::sbcIndirecty8()
{
    uint16_t tempw;
    uint8_t  temp;
    addr = indirecty();
    temp = snes->readmem(addr);
    if (p.d) {
        SBCBCD8(tempw, temp);
    } else {
        SBC8(tempw, temp);
    }
}
void CPU::sbcIndirectLong8()
{
    uint16_t tempw;
    uint8_t  temp;
    addr = indirectl();
    temp = snes->readmem(addr);
    if (p.d) {
        SBCBCD8(tempw, temp);
    } else {
        SBC8(tempw, temp);
    }
}
void CPU::sbcIndirectLongy8()
{
    uint16_t tempw;
    uint8_t  temp;
    addr = indirectly();
    temp = snes->readmem(addr);
    if (p.d) {
        SBCBCD8(tempw, temp);
    } else {
        SBC8(tempw, temp);
    }
}
void CPU::sbcImm16()
{
    uint32_t templ;
    uint16_t tempw;
    tempw = snes->readmemw(pbr | pc);
    pc += 2;
    if (p.d) {
        SBCBCD16(tempw, templ);
    } else {
        SBC16(tempw, templ);
    }
}
void CPU::sbcZp16()
{
    uint32_t templ;
    uint16_t tempw;
    addr  = zeropage();
    tempw = snes->readmemw(addr);
    if (p.d) {
        SBCBCD16(tempw, templ);
    } else {
        SBC16(tempw, templ);
    }
}
void CPU::sbcZpx16()
{
    uint32_t templ;
    uint16_t tempw;
    addr  = zeropagex();
    tempw = snes->readmemw(addr);
    if (p.d) {
        SBCBCD16(tempw, templ);
    } else {
        SBC16(tempw, templ);
    }
}
void CPU::sbcSp16()
{
    uint32_t templ;
    uint16_t tempw;
    addr  = stack();
    tempw = snes->readmemw(addr);
    if (p.d) {
        SBCBCD16(tempw, templ);
    } else {
        SBC16(tempw, templ);
    }
}
void CPU::sbcAbs16()
{
    uint32_t templ;
    uint16_t tempw;
    addr  = absolute();
    tempw = snes->readmemw(addr);
    if (p.d) {
        SBCBCD16(tempw, templ);
    } else {
        SBC16(tempw, templ);
    }
}
void CPU::sbcAbsx16()
{
    uint32_t templ;
    uint16_t tempw;
    addr  = absolutex();
    tempw = snes->readmemw(addr);
    if (p.d) {
        SBCBCD16(tempw, templ);
    } else {
        SBC16(tempw, templ);
    }
}
void CPU::sbcAbsy16()
{
    uint32_t templ;
    uint16_t tempw;
    addr  = absolutey();
    tempw = snes->readmemw(addr);
    if (p.d) {
        SBCBCD16(tempw, templ);
    } else {
        SBC16(tempw, templ);
    }
}
void CPU::sbcLong16()
{
    uint32_t templ;
    uint16_t tempw;
    addr  = absolutelong();
    tempw = snes->readmemw(addr);
    if (p.d) {
        SBCBCD16(tempw, templ);
    } else {
        SBC16(tempw, templ);
    }
}
void CPU::sbcLongx16()
{
    uint32_t templ;
    uint16_t tempw;
    addr  = absolutelongx();
    tempw = snes->readmemw(addr);
    if (p.d) {
        SBCBCD16(tempw, templ);
    } else {
        SBC16(tempw, templ);
    }
}
void CPU::sbcIndirect16()
{
    uint32_t templ;
    uint16_t tempw;
    addr  = indirect();
    tempw = snes->readmemw(addr);
    if (p.d) {
        SBCBCD16(tempw, templ);
    } else {
        SBC16(tempw, templ);
    }
}
void CPU::sbcIndirectx16()
{
    uint32_t templ;
    uint16_t tempw;
    addr  = indirectx();
    tempw = snes->readmemw(addr);
    if (p.d) {
        SBCBCD16(tempw, templ);
    } else {
        SBC16(tempw, templ);
    }
}
void CPU::sbcIndirecty16()
{
    uint32_t templ;
    uint16_t tempw;
    addr  = indirecty();
    tempw = snes->readmemw(addr);
    if (p.d) {
        SBCBCD16(tempw, templ);
    } else {
        SBC16(tempw, templ);
    }
}
void CPU::sbcIndirectLong16()
{
    uint32_t templ;
    uint16_t tempw;
    addr  = indirectl();
    tempw = snes->readmemw(addr);
    if (p.d) {
        SBCBCD16(tempw, templ);
    } else {
        SBC16(tempw, templ);
    }
}
void CPU::sbcIndirectLongy16()
{
    uint32_t templ;
    uint16_t tempw;
    addr  = indirectly();
    tempw = snes->readmemw(addr);
    if (p.d) {
        SBCBCD16(tempw, templ);
    } else {
        SBC16(tempw, templ);
    }
}
void CPU::eorImm8()
{
    a.b.l ^= snes->readmem(pbr | pc);
    pc++;
    setzn8(a.b.l);
}
void CPU::eorZp8()
{
    addr = zeropage();
    a.b.l ^= snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::eorZpx8()
{
    addr = zeropagex();
    a.b.l ^= snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::eorSp8()
{
    addr = stack();
    a.b.l ^= snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::eorAbs8()
{
    addr = absolute();
    a.b.l ^= snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::eorAbsx8()
{
    addr = absolutex();
    a.b.l ^= snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::eorAbsy8()
{
    addr = absolutey();
    a.b.l ^= snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::eorLong8()
{
    addr = absolutelong();
    a.b.l ^= snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::eorLongx8()
{
    addr = absolutelongx();
    a.b.l ^= snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::eorIndirect8()
{
    addr = indirect();
    a.b.l ^= snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::eorIndirectx8()
{
    addr = indirectx();
    a.b.l ^= snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::eorIndirecty8()
{
    addr = indirecty();
    a.b.l ^= snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::eorIndirectLong8()
{
    addr = indirectl();
    a.b.l ^= snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::eorIndirectLongy8()
{
    addr = indirectly();
    a.b.l ^= snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::eorImm16()
{
    a.w ^= snes->readmemw(pbr | pc);
    pc += 2;
    setzn16(a.w);
}
void CPU::eorZp16()
{
    addr = zeropage();
    a.w ^= snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::eorZpx16()
{
    addr = zeropagex();
    a.w ^= snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::eorSp16()
{
    addr = stack();
    a.w ^= snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::eorAbs16()
{
    addr = absolute();
    a.w ^= snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::eorAbsx16()
{
    addr = absolutex();
    a.w ^= snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::eorAbsy16()
{
    addr = absolutey();
    a.w ^= snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::eorLong16()
{
    addr = absolutelong();
    a.w ^= snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::eorLongx16()
{
    addr = absolutelongx();
    a.w ^= snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::eorIndirect16()
{
    addr = indirect();
    a.w ^= snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::eorIndirectx16()
{
    addr = indirectx();
    a.w ^= snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::eorIndirecty16()
{
    addr = indirecty();
    a.w ^= snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::eorIndirectLong16()
{
    addr = indirectl();
    a.w ^= snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::eorIndirectLongy16()
{
    addr = indirectly();
    a.w ^= snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::andImm8()
{
    a.b.l &= snes->readmem(pbr | pc);
    pc++;
    setzn8(a.b.l);
}
void CPU::andZp8()
{
    addr = zeropage();
    a.b.l &= snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::andZpx8()
{
    addr = zeropagex();
    a.b.l &= snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::andSp8()
{
    addr = stack();
    a.b.l &= snes->readmem(addr);
    setzn8(a.b.l);
}

// kxkx
void CPU::andSpII8()
{
    addr = stack();
}
void CPU::andSpII16()
{
    addr = stack();
}
//

void CPU::andAbs8()
{
    addr = absolute();
    a.b.l &= snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::andAbsx8()
{
    addr = absolutex();
    a.b.l &= snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::andAbsy8()
{
    addr = absolutey();
    a.b.l &= snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::andLong8()
{
    addr = absolutelong();
    a.b.l &= snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::andLongx8()
{
    addr = absolutelongx();
    a.b.l &= snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::andIndirect8()
{
    addr = indirect();
    a.b.l &= snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::andIndirectx8()
{
    addr = indirectx();
    a.b.l &= snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::andIndirecty8()
{
    addr = indirecty();
    a.b.l &= snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::andIndirectLong8()
{
    addr = indirectl();
    a.b.l &= snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::andIndirectLongy8()
{
    addr = indirectly();
    a.b.l &= snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::andImm16()
{
    a.w &= snes->readmemw(pbr | pc);
    pc += 2;
    setzn16(a.w);
}
void CPU::andZp16()
{
    addr = zeropage();
    a.w &= snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::andZpx16()
{
    addr = zeropagex();
    a.w &= snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::andSp16()
{
    addr = stack();
    a.w &= snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::andAbs16()
{
    addr = absolute();
    a.w &= snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::andAbsx16()
{
    addr = absolutex();
    a.w &= snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::andAbsy16()
{
    addr = absolutey();
    a.w &= snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::andLong16()
{
    addr = absolutelong();
    a.w &= snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::andLongx16()
{
    addr = absolutelongx();
    a.w &= snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::andIndirect16()
{
    addr = indirect();
    a.w &= snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::andIndirectx16()
{
    addr = indirectx();
    a.w &= snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::andIndirecty16()
{
    addr = indirecty();
    a.w &= snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::andIndirectLong16()
{
    addr = indirectl();
    a.w &= snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::andIndirectLongy16()
{
    addr = indirectly();
    a.w &= snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::oraImm8()
{
    a.b.l |= snes->readmem(pbr | pc);
    pc++;
    setzn8(a.b.l);
}
void CPU::oraZp8()
{
    addr = zeropage();
    a.b.l |= snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::oraZpx8()
{
    addr = zeropagex();
    a.b.l |= snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::oraSp8()
{
    addr = stack();
    a.b.l |= snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::oraAbs8()
{
    addr = absolute();
    a.b.l |= snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::oraAbsx8()
{
    addr = absolutex();
    a.b.l |= snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::oraAbsy8()
{
    addr = absolutey();
    a.b.l |= snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::oraLong8()
{
    addr = absolutelong();
    a.b.l |= snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::oraLongx8()
{
    addr = absolutelongx();
    a.b.l |= snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::oraIndirect8()
{
    addr = indirect();
    a.b.l |= snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::oraIndirectx8()
{
    addr = indirectx();
    a.b.l |= snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::oraIndirecty8()
{
    addr = indirecty();
    a.b.l |= snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::oraIndirectLong8()
{
    addr = indirectl();
    a.b.l |= snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::oraIndirectLongy8()
{
    addr = indirectly();
    a.b.l |= snes->readmem(addr);
    setzn8(a.b.l);
}
void CPU::oraImm16()
{
    a.w |= snes->readmemw(pbr | pc);
    pc += 2;
    setzn16(a.w);
}
void CPU::oraZp16()
{
    addr = zeropage();
    a.w |= snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::oraZpx16()
{
    addr = zeropagex();
    a.w |= snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::oraSp16()
{
    addr = stack();
    a.w |= snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::oraAbs16()
{
    addr = absolute();
    a.w |= snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::oraAbsx16()
{
    addr = absolutex();
    a.w |= snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::oraAbsy16()
{
    addr = absolutey();
    a.w |= snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::oraLong16()
{
    addr = absolutelong();
    a.w |= snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::oraLongx16()
{
    addr = absolutelongx();
    a.w |= snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::oraIndirect16()
{
    addr = indirect();
    a.w |= snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::oraIndirectx16()
{
    addr = indirectx();
    a.w |= snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::oraIndirecty16()
{
    addr = indirecty();
    a.w |= snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::oraIndirectLong16()
{
    addr = indirectl();
    a.w |= snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::oraIndirectLongy16()
{
    addr = indirectly();
    a.w |= snes->readmemw(addr);
    setzn16(a.w);
}
void CPU::bitImm8()
{
    uint8_t temp = snes->readmem(pbr | pc);
    pc++;
    p.z = !(temp & a.b.l);
}
void CPU::bitImm16()
{
    uint16_t temp = snes->readmemw(pbr | pc);
    pc += 2;
    p.z   = !(temp & a.w);
    setzf = 0;
}
void CPU::bitZp8()
{
    uint8_t temp;
    addr = zeropage();
    temp = snes->readmem(addr);
    p.z  = !(temp & a.b.l);
    p.v  = temp & 0x40;
    p.n  = temp & 0x80;
}
void CPU::bitZp16()
{
    uint16_t temp;
    addr = zeropage();
    temp = snes->readmemw(addr);
    p.z  = !(temp & a.w);
    p.v  = temp & 0x4000;
    p.n  = temp & 0x8000;
}
void CPU::bitZpx8()
{
    uint8_t temp;
    addr = zeropagex();
    temp = snes->readmem(addr);
    p.z  = !(temp & a.b.l);
    p.v  = temp & 0x40;
    p.n  = temp & 0x80;
}
void CPU::bitZpx16()
{
    uint16_t temp;
    addr = zeropagex();
    temp = snes->readmemw(addr);
    p.z  = !(temp & a.w);
    p.v  = temp & 0x4000;
    p.n  = temp & 0x8000;
}
void CPU::bitAbs8()
{
    uint8_t temp;
    addr = absolute();
    temp = snes->readmem(addr);
    p.z  = !(temp & a.b.l);
    p.v  = temp & 0x40;
    p.n  = temp & 0x80;
}
void CPU::bitAbs16()
{
    uint16_t temp;
    addr = absolute();
    temp = snes->readmemw(addr);
    p.z  = !(temp & a.w);
    p.v  = temp & 0x4000;
    p.n  = temp & 0x8000;
}
void CPU::bitAbsx8()
{
    uint8_t temp;
    addr = absolutex();
    temp = snes->readmem(addr);
    p.z  = !(temp & a.b.l);
    p.v  = temp & 0x40;
    p.n  = temp & 0x80;
}
void CPU::bitAbsx16()
{
    uint16_t temp;
    addr = absolutex();
    temp = snes->readmemw(addr);
    p.z  = !(temp & a.w);
    p.v  = temp & 0x4000;
    p.n  = temp & 0x8000;
}
void CPU::cmpImm8()
{
    uint8_t temp;
    temp = snes->readmem(pbr | pc);
    pc++;
    setzn8(a.b.l - temp);
    p.c = (a.b.l >= temp);
}
void CPU::cmpZp8()
{
    uint8_t temp;
    addr = zeropage();
    temp = snes->readmem(addr);
    setzn8(a.b.l - temp);
    p.c = (a.b.l >= temp);
}
void CPU::cmpZpx8()
{
    uint8_t temp;
    addr = zeropagex();
    temp = snes->readmem(addr);
    setzn8(a.b.l - temp);
    p.c = (a.b.l >= temp);
}
void CPU::cmpSp8()
{
    uint8_t temp;
    addr = stack();
    temp = snes->readmem(addr);
    setzn8(a.b.l - temp);
    p.c = (a.b.l >= temp);
}
void CPU::cmpAbs8()
{
    uint8_t temp;
    addr = absolute();
    temp = snes->readmem(addr);
    setzn8(a.b.l - temp);
    p.c = (a.b.l >= temp);
}
void CPU::cmpAbsx8()
{
    uint8_t temp;
    addr = absolutex();
    temp = snes->readmem(addr);
    setzn8(a.b.l - temp);
    p.c = (a.b.l >= temp);
}
void CPU::cmpAbsy8()
{
    uint8_t temp;
    addr = absolutey();
    temp = snes->readmem(addr);
    setzn8(a.b.l - temp);
    p.c = (a.b.l >= temp);
}
void CPU::cmpLong8()
{
    uint8_t temp;
    addr = absolutelong();
    temp = snes->readmem(addr);
    setzn8(a.b.l - temp);
    p.c = (a.b.l >= temp);
}
void CPU::cmpLongx8()
{
    uint8_t temp;
    addr = absolutelongx();
    temp = snes->readmem(addr);
    setzn8(a.b.l - temp);
    p.c = (a.b.l >= temp);
}
void CPU::cmpIndirect8()
{
    uint8_t temp;
    addr = indirect();
    temp = snes->readmem(addr);
    setzn8(a.b.l - temp);
    p.c = (a.b.l >= temp);
}
void CPU::cmpIndirectx8()
{
    uint8_t temp;
    addr = indirectx();
    temp = snes->readmem(addr);
    setzn8(a.b.l - temp);
    p.c = (a.b.l >= temp);
}
void CPU::cmpIndirecty8()
{
    uint8_t temp;
    addr = indirecty();
    temp = snes->readmem(addr);
    setzn8(a.b.l - temp);
    p.c = (a.b.l >= temp);
}
void CPU::cmpIndirectLong8()
{
    uint8_t temp;
    addr = indirectl();
    temp = snes->readmem(addr);
    setzn8(a.b.l - temp);
    p.c = (a.b.l >= temp);
}
void CPU::cmpIndirectLongy8()
{
    uint8_t temp;
    addr = indirectly();
    temp = snes->readmem(addr);
    setzn8(a.b.l - temp);
    p.c = (a.b.l >= temp);
}
void CPU::cmpImm16()
{
    uint16_t temp;
    temp = snes->readmemw(pbr | pc);
    pc += 2;
    setzn16(a.w - temp);
    p.c = (a.w >= temp);
}
void CPU::cmpZp16()
{
    uint16_t temp;
    addr = zeropage();
    temp = snes->readmemw(addr);
    setzn16(a.w - temp);
    p.c = (a.w >= temp);
}
void CPU::cmpSp16()
{
    uint16_t temp;
    addr = stack();
    temp = snes->readmemw(addr);
    setzn16(a.w - temp);
    p.c = (a.w >= temp);
}
void CPU::cmpZpx16()
{
    uint16_t temp;
    addr = zeropagex();
    temp = snes->readmemw(addr);
    setzn16(a.w - temp);
    p.c = (a.w >= temp);
}
void CPU::cmpAbs16()
{
    uint16_t temp;
    addr = absolute();
    temp = snes->readmemw(addr);
    setzn16(a.w - temp);
    p.c = (a.w >= temp);
}
void CPU::cmpAbsx16()
{
    uint16_t temp;
    addr = absolutex();
    temp = snes->readmemw(addr);
    setzn16(a.w - temp);
    p.c = (a.w >= temp);
}
void CPU::cmpAbsy16()
{
    uint16_t temp;
    addr = absolutey();
    temp = snes->readmemw(addr);
    setzn16(a.w - temp);
    p.c = (a.w >= temp);
}
void CPU::cmpLong16()
{
    uint16_t temp;
    addr = absolutelong();
    temp = snes->readmemw(addr);
    setzn16(a.w - temp);
    p.c = (a.w >= temp);
}
void CPU::cmpLongx16()
{
    uint16_t temp;
    addr = absolutelongx();
    temp = snes->readmemw(addr);
    setzn16(a.w - temp);
    p.c = (a.w >= temp);
}
void CPU::cmpIndirect16()
{
    uint16_t temp;
    addr = indirect();
    temp = snes->readmemw(addr);
    setzn16(a.w - temp);
    p.c = (a.w >= temp);
}
void CPU::cmpIndirectx16()
{
    uint16_t temp;
    addr = indirectx();
    temp = snes->readmemw(addr);
    setzn16(a.w - temp);
    p.c = (a.w >= temp);
}
void CPU::cmpIndirecty16()
{
    uint16_t temp;
    addr = indirecty();
    temp = snes->readmemw(addr);
    setzn16(a.w - temp);
    p.c = (a.w >= temp);
}
void CPU::cmpIndirectLong16()
{
    uint16_t temp;
    addr = indirectl();
    temp = snes->readmemw(addr);
    setzn16(a.w - temp);
    p.c = (a.w >= temp);
}
void CPU::cmpIndirectLongy16()
{
    uint16_t temp;
    addr = indirectly();
    temp = snes->readmemw(addr);
    setzn16(a.w - temp);
    p.c = (a.w >= temp);
}
void CPU::phb()
{
    snes->readmem(pbr | pc);
    snes->writemem(s.w, dbr >> 16);
    s.w--;
}
void CPU::phbe()
{
    snes->readmem(pbr | pc);
    snes->writemem(s.w, dbr >> 16);
    s.b.l--;
}
void CPU::phk()
{
    snes->readmem(pbr | pc);
    snes->writemem(s.w, pbr >> 16);
    s.w--;
}
void CPU::phke()
{
    snes->readmem(pbr | pc);
    snes->writemem(s.w, pbr >> 16);
    s.b.l--;
}
void CPU::pea()
{
    addr = snes->readmemw(pbr | pc);
    pc += 2;
    snes->writemem(s.w, addr >> 8);
    s.w--;
    snes->writemem(s.w, addr & 0xFF);
    s.w--;
}
void CPU::pei()
{
    addr = indirect();
    snes->writemem(s.w, addr >> 8);
    s.w--;
    snes->writemem(s.w, addr & 0xFF);
    s.w--;
}
void CPU::per()
{
    addr = snes->readmemw(pbr | pc);
    pc += 2;
    addr += pc;
    snes->writemem(s.w, addr >> 8);
    s.w--;
    snes->writemem(s.w, addr & 0xFF);
    s.w--;
}
void CPU::phd()
{
    snes->writemem(s.w, dp >> 8);
    s.w--;
    snes->writemem(s.w, dp & 0xFF);
    s.w--;
}
void CPU::pld()
{
    snes->readmem(pbr | pc);
    s.w++;
    cycles -= 6;
    snes->clockspc(6);
    dp = snes->readmem(s.w);
    s.w++;
    dp |= (snes->readmem(s.w) << 8);
}
void CPU::pha8()
{
    snes->readmem(pbr | pc);
    snes->writemem(s.w, a.b.l);
    s.w--;
}
void CPU::pha16()
{
    snes->readmem(pbr | pc);
    snes->writemem(s.w, a.b.h);
    s.w--;
    snes->writemem(s.w, a.b.l);
    s.w--;
}
void CPU::phx8()
{
    snes->readmem(pbr | pc);
    snes->writemem(s.w, x.b.l);
    s.w--;
}
void CPU::phx16()
{
    snes->readmem(pbr | pc);
    snes->writemem(s.w, x.b.h);
    s.w--;
    snes->writemem(s.w, x.b.l);
    s.w--;
}
void CPU::phy8()
{
    snes->readmem(pbr | pc);
    snes->writemem(s.w, y.b.l);
    s.w--;
}
void CPU::phy16()
{
    snes->readmem(pbr | pc);
    snes->writemem(s.w, y.b.h);
    s.w--;
    snes->writemem(s.w, y.b.l);
    s.w--;
}
void CPU::pla8()
{
    snes->readmem(pbr | pc);
    s.w++;
    cycles -= 6;
    snes->clockspc(6);
    a.b.l = snes->readmem(s.w);
    setzn8(a.b.l);
}
void CPU::pla16()
{
    snes->readmem(pbr | pc);
    s.w++;
    cycles -= 6;
    snes->clockspc(6);
    a.b.l = snes->readmem(s.w);
    s.w++;
    a.b.h = snes->readmem(s.w);
    setzn16(a.w);
}
void CPU::plx8()
{
    snes->readmem(pbr | pc);
    s.w++;
    cycles -= 6;
    snes->clockspc(6);
    x.b.l = snes->readmem(s.w);
    setzn8(x.b.l);
}
void CPU::plx16()
{
    snes->readmem(pbr | pc);
    s.w++;
    cycles -= 6;
    snes->clockspc(6);
    x.b.l = snes->readmem(s.w);
    s.w++;
    x.b.h = snes->readmem(s.w);
    setzn16(x.w);
}
void CPU::ply8()
{
    snes->readmem(pbr | pc);
    s.w++;
    cycles -= 6;
    snes->clockspc(6);
    y.b.l = snes->readmem(s.w);
    setzn8(y.b.l);
}
void CPU::ply16()
{
    snes->readmem(pbr | pc);
    s.w++;
    cycles -= 6;
    snes->clockspc(6);
    y.b.l = snes->readmem(s.w);
    s.w++;
    y.b.h = snes->readmem(s.w);
    setzn16(y.w);
}
void CPU::plb()
{
    snes->readmem(pbr | pc);
    s.w++;
    cycles -= 6;
    snes->clockspc(6);
    dbr = snes->readmem(s.w) << 16;
}
void CPU::plbe()
{
    snes->readmem(pbr | pc);
    s.b.l++;
    cycles -= 6;
    snes->clockspc(6);
    dbr = snes->readmem(s.w) << 16;
}
void CPU::plp()
{
    uint8_t temp = snes->readmem(s.w + 1);
    s.w++;
    p.c = temp & 1;
    p.z = temp & 2;
    p.i = temp & 4;
    p.d = temp & 8;
    p.x = temp & 0x10;
    p.m = temp & 0x20;
    p.v = temp & 0x40;
    p.n = temp & 0x80;
    cycles -= 12;
    snes->clockspc(12);
    updatecpumode();
}
void CPU::plpe()
{
    uint8_t temp;
    s.b.l++;
    temp = snes->readmem(s.w);
    p.c  = temp & 1;
    p.z  = temp & 2;
    p.i  = temp & 4;
    p.d  = temp & 8;
    p.v  = temp & 0x40;
    p.n  = temp & 0x80;
    cycles -= 12;
    snes->clockspc(12);
}
void CPU::php()
{
    uint8_t temp = (p.c) ? 1 : 0;
    if (p.z)
        temp |= 2;
    if (p.i)
        temp |= 4;
    if (p.d)
        temp |= 8;
    if (p.v)
        temp |= 0x40;
    if (p.n)
        temp |= 0x80;
    if (p.x)
        temp |= 0x10;
    if (p.m)
        temp |= 0x20;
    snes->readmem(pbr | pc);
    snes->writemem(s.w, temp);
    s.w--;
}
void CPU::phpe()
{
    uint8_t temp = (p.c) ? 1 : 0;
    if (p.z)
        temp |= 2;
    if (p.i)
        temp |= 4;
    if (p.d)
        temp |= 8;
    if (p.v)
        temp |= 0x40;
    if (p.n)
        temp |= 0x80;
    temp |= 0x30;
    snes->readmem(pbr | pc);
    snes->writemem(s.w, temp);
    s.b.l--;
}
void CPU::cpxImm8()
{
    uint8_t temp = snes->readmem(pbr | pc);
    pc++;
    setzn8(x.b.l - temp);
    p.c = (x.b.l >= temp);
}
void CPU::cpxImm16()
{
    uint16_t temp = snes->readmemw(pbr | pc);
    pc += 2;
    setzn16(x.w - temp);
    p.c = (x.w >= temp);
}
void CPU::cpxZp8()
{
    uint8_t temp;
    addr = zeropage();
    temp = snes->readmem(addr);
    setzn8(x.b.l - temp);
    p.c = (x.b.l >= temp);
}
void CPU::cpxZp16()
{
    uint16_t temp;
    addr = zeropage();
    temp = snes->readmemw(addr);
    setzn16(x.w - temp);
    p.c = (x.w >= temp);
}
void CPU::cpxAbs8()
{
    uint8_t temp;
    addr = absolute();
    temp = snes->readmem(addr);
    setzn8(x.b.l - temp);
    p.c = (x.b.l >= temp);
}
void CPU::cpxAbs16()
{
    uint16_t temp;
    addr = absolute();
    temp = snes->readmemw(addr);
    setzn16(x.w - temp);
    p.c = (x.w >= temp);
}
void CPU::cpyImm8()
{
    uint8_t temp = snes->readmem(pbr | pc);
    pc++;
    setzn8(y.b.l - temp);
    p.c = (y.b.l >= temp);
}
void CPU::cpyImm16()
{
    uint16_t temp = snes->readmemw(pbr | pc);
    pc += 2;
    setzn16(y.w - temp);
    p.c = (y.w >= temp);
}
void CPU::cpyZp8()
{
    uint8_t temp;
    addr = zeropage();
    temp = snes->readmem(addr);
    setzn8(y.b.l - temp);
    p.c = (y.b.l >= temp);
}
void CPU::cpyZp16()
{
    uint16_t temp;
    addr = zeropage();
    temp = snes->readmemw(addr);
    setzn16(y.w - temp);
    p.c = (y.w >= temp);
}
void CPU::cpyAbs8()
{
    uint8_t temp;
    addr = absolute();
    temp = snes->readmem(addr);
    setzn8(y.b.l - temp);
    p.c = (y.b.l >= temp);
}
void CPU::cpyAbs16()
{
    uint16_t temp;
    addr = absolute();
    temp = snes->readmemw(addr);
    setzn16(y.w - temp);
    p.c = (y.w >= temp);
}
void CPU::bcc()
{
    int8_t temp = (int8_t)snes->readmem(pbr | pc);
    pc++;
    if (!p.c) {
        pc += temp;
        cycles -= 6;
        snes->clockspc(6);
    }
}
void CPU::bcs()
{
    int8_t temp = (int8_t)snes->readmem(pbr | pc);
    pc++;
    if (p.c) {
        pc += temp;
        cycles -= 6;
        snes->clockspc(6);
    }
}
void CPU::beq()
{
    int8_t temp = (int8_t)snes->readmem(pbr | pc);
    pc++;
    if (setzf > 0)
        p.z = 0;
    if (setzf < 0)
        p.z = 1;
    setzf = 0;
    if (p.z) {
        pc += temp;
        cycles -= 6;
        snes->clockspc(6);
    }
}
void CPU::bne()
{
    int8_t temp = (int8_t)snes->readmem(pbr | pc);
    pc++;
    if (setzf > 0)
        p.z = 1;
    if (setzf < 0)
        p.z = 0;
    setzf = 0;
    if (!p.z) {
        pc += temp;
        cycles -= 6;
        snes->clockspc(6);
    }
}
void CPU::bpl()
{
    int8_t temp = (int8_t)snes->readmem(pbr | pc);
    pc++;
    if (!p.n) {
        pc += temp;
        cycles -= 6;
        snes->clockspc(6);
    }
}
void CPU::bmi()
{
    int8_t temp = (int8_t)snes->readmem(pbr | pc);
    pc++;
    if (p.n) {
        pc += temp;
        cycles -= 6;
        snes->clockspc(6);
    }
}
void CPU::bvc()
{
    int8_t temp = (int8_t)snes->readmem(pbr | pc);
    pc++;
    if (!p.v) {
        pc += temp;
        cycles -= 6;
        snes->clockspc(6);
    }
}
void CPU::bvs()
{
    int8_t temp = (int8_t)snes->readmem(pbr | pc);
    pc++;
    if (p.v) {
        pc += temp;
        cycles -= 6;
        snes->clockspc(6);
    }
}
void CPU::bra()
{
    int8_t temp = (int8_t)snes->readmem(pbr | pc);
    pc++;
    pc += temp;
    cycles -= 6;
    snes->clockspc(6);
}
void CPU::brl()
{
    uint16_t temp = snes->readmemw(pbr | pc);
    pc += 2;
    pc += temp;
    cycles -= 6;
    snes->clockspc(6);
}
void CPU::jmp()
{
    addr = snes->readmemw(pbr | pc);
    pc   = addr;
}
void CPU::jmplong()
{
    addr = snes->readmemw(pbr | pc) | (snes->readmem((pbr | pc) + 2) << 16);
    pc   = addr & 0xFFFF;
    pbr  = addr & 0xFF0000;
}
void CPU::jmpind()
{
    addr = snes->readmemw(pbr | pc);
    pc   = snes->readmemw(addr);
}
void CPU::jmpindx()
{
    addr = (snes->readmemw(pbr | pc)) + x.w + pbr;
    pc   = snes->readmemw(addr);
}
void CPU::jmlind()
{
    addr = snes->readmemw(pbr | pc);
    pc   = snes->readmemw(addr);
    pbr  = snes->readmem(addr + 2) << 16;
}
void CPU::jsr()
{
    addr = snes->readmemw(pbr | pc);
    pc++;
    snes->readmem(pbr | pc);
    snes->writemem(s.w, pc >> 8);
    s.w--;
    snes->writemem(s.w, pc & 0xFF);
    s.w--;
    pc = addr;
}
void CPU::jsre()
{
    addr = snes->readmemw(pbr | pc);
    pc++;
    snes->readmem(pbr | pc);
    snes->writemem(s.w, pc >> 8);
    s.b.l--;
    snes->writemem(s.w, pc & 0xFF);
    s.b.l--;
    pc = addr;
}
void CPU::jsrIndx()
{
    addr = jindirectx();
    pc--;
    snes->writemem(s.w, pc >> 8);
    s.w--;
    snes->writemem(s.w, pc & 0xFF);
    s.w--;
    pc = snes->readmemw(addr);
}
void CPU::jsrIndxe()
{
    addr = jindirectx();
    pc--;
    snes->writemem(s.w, pc >> 8);
    s.b.l--;
    snes->writemem(s.w, pc & 0xFF);
    s.b.l--;
    pc = snes->readmemw(addr);
}
void CPU::jsl()
{
    uint8_t temp;
    addr = snes->readmemw(pbr | pc);
    pc += 2;
    temp = snes->readmem(pbr | pc);
    snes->writemem(s.w, pbr >> 16);
    s.w--;
    snes->writemem(s.w, pc >> 8);
    s.w--;
    snes->writemem(s.w, pc & 0xFF);
    s.w--;
    pc  = addr;
    pbr = temp << 16;
}
void CPU::jsle()
{
    uint8_t temp;
    addr = snes->readmemw(pbr | pc);
    pc += 2;
    temp = snes->readmem(pbr | pc);
    snes->writemem(s.w, pbr >> 16);
    s.b.l--;
    snes->writemem(s.w, pc >> 8);
    s.b.l--;
    snes->writemem(s.w, pc & 0xFF);
    s.b.l--;
    pc  = addr;
    pbr = temp << 16;
}
void CPU::rtl()
{
    cycles -= 18;
    snes->clockspc(18);
    pc = snes->readmemw(s.w + 1);
    s.w += 2;
    pbr = snes->readmem(s.w + 1) << 16;
    s.w++;
    pc++;
}
void CPU::rtle()
{
    cycles -= 18;
    snes->clockspc(18);
    s.b.l++;
    pc = snes->readmem(s.w);
    s.b.l++;
    pc |= (snes->readmem(s.w) << 8);
    s.b.l++;
    pbr = snes->readmem(s.w) << 16;
}
void CPU::rts()
{
    cycles -= 18;
    snes->clockspc(18);
    pc = snes->readmemw(s.w + 1);
    s.w += 2;
    pc++;
}
void CPU::rtse()
{
    cycles -= 18;
    snes->clockspc(18);
    s.b.l++;
    pc = snes->readmem(s.w);
    s.b.l++;
    pc |= (snes->readmem(s.w) << 8);
}
void CPU::rti()
{
    uint8_t temp;
    cycles -= 6;
    s.w++;
    snes->clockspc(6);
    temp = snes->readmem(s.w);
    p.c  = temp & 1;
    p.z  = temp & 2;
    p.i  = temp & 4;
    p.d  = temp & 8;
    p.x  = temp & 0x10;
    p.m  = temp & 0x20;
    p.v  = temp & 0x40;
    p.n  = temp & 0x80;
    s.w++;
    pc = snes->readmem(s.w);
    s.w++;
    pc |= (snes->readmem(s.w) << 8);
    s.w++;
    pbr = snes->readmem(s.w) << 16;
    updatecpumode();
}
void CPU::asla8()
{
    snes->readmem(pbr | pc);
    p.c = a.b.l & 0x80;
    a.b.l <<= 1;
    setzn8(a.b.l);
}
void CPU::asla16()
{
    snes->readmem(pbr | pc);
    p.c = a.w & 0x8000;
    a.w <<= 1;
    setzn16(a.w);
}
void CPU::aslZp8()
{
    uint8_t temp;
    addr = zeropage();
    temp = snes->readmem(addr);
    cycles -= 6;
    snes->clockspc(6);
    p.c = temp & 0x80;
    temp <<= 1;
    setzn8(temp);
    snes->writemem(addr, temp);
}
void CPU::aslZp16()
{
    uint16_t temp;
    addr = zeropage();
    temp = snes->readmemw(addr);
    cycles -= 6;
    snes->clockspc(6);
    p.c = temp & 0x8000;
    temp <<= 1;
    setzn16(temp);
    snes->writememw2(addr, temp);
}
void CPU::aslZpx8()
{
    uint8_t temp;
    addr = zeropagex();
    temp = snes->readmem(addr);
    cycles -= 6;
    snes->clockspc(6);
    p.c = temp & 0x80;
    temp <<= 1;
    setzn8(temp);
    snes->writemem(addr, temp);
}
void CPU::aslZpx16()
{
    uint16_t temp;
    addr = zeropagex();
    temp = snes->readmemw(addr);
    cycles -= 6;
    snes->clockspc(6);
    p.c = temp & 0x8000;
    temp <<= 1;
    setzn16(temp);
    snes->writememw2(addr, temp);
}
void CPU::aslAbs8()
{
    uint8_t temp;
    addr = absolute();
    temp = snes->readmem(addr);
    cycles -= 6;
    snes->clockspc(6);
    p.c = temp & 0x80;
    temp <<= 1;
    setzn8(temp);
    snes->writemem(addr, temp);
}
void CPU::aslAbs16()
{
    uint16_t temp;
    addr = absolute();
    temp = snes->readmemw(addr);
    cycles -= 6;
    snes->clockspc(6);
    p.c = temp & 0x8000;
    temp <<= 1;
    setzn16(temp);
    snes->writememw2(addr, temp);
}
void CPU::aslAbsx8()
{
    uint8_t temp;
    addr = absolutex();
    temp = snes->readmem(addr);
    cycles -= 6;
    snes->clockspc(6);
    p.c = temp & 0x80;
    temp <<= 1;
    setzn8(temp);
    snes->writemem(addr, temp);
}
void CPU::aslAbsx16()
{
    uint16_t temp;
    addr = absolutex();
    temp = snes->readmemw(addr);
    cycles -= 6;
    snes->clockspc(6);
    p.c = temp & 0x8000;
    temp <<= 1;
    setzn16(temp);
    snes->writememw2(addr, temp);
}
void CPU::lsra8()
{
    snes->readmem(pbr | pc);
    p.c = a.b.l & 1;
    a.b.l >>= 1;
    setzn8(a.b.l);
}
void CPU::lsra16()
{
    snes->readmem(pbr | pc);
    p.c = a.w & 1;
    a.w >>= 1;
    setzn16(a.w);
}
void CPU::lsrZp8()
{
    uint8_t temp;
    addr = zeropage();
    temp = snes->readmem(addr);
    cycles -= 6;
    snes->clockspc(6);
    p.c = temp & 1;
    temp >>= 1;
    setzn8(temp);
    snes->writemem(addr, temp);
}
void CPU::lsrZp16()
{
    uint16_t temp;
    addr = zeropage();
    temp = snes->readmemw(addr);
    cycles -= 6;
    snes->clockspc(6);
    p.c = temp & 1;
    temp >>= 1;
    setzn16(temp);
    snes->writememw2(addr, temp);
}
void CPU::lsrZpx8()
{
    uint8_t temp;
    addr = zeropagex();
    temp = snes->readmem(addr);
    cycles -= 6;
    snes->clockspc(6);
    p.c = temp & 1;
    temp >>= 1;
    setzn8(temp);
    snes->writemem(addr, temp);
}
void CPU::lsrZpx16()
{
    uint16_t temp;
    addr = zeropagex();
    temp = snes->readmemw(addr);
    cycles -= 6;
    snes->clockspc(6);
    p.c = temp & 1;
    temp >>= 1;
    setzn16(temp);
    snes->writememw2(addr, temp);
}
void CPU::lsrAbs8()
{
    uint8_t temp;
    addr = absolute();
    temp = snes->readmem(addr);
    cycles -= 6;
    snes->clockspc(6);
    p.c = temp & 1;
    temp >>= 1;
    setzn8(temp);
    snes->writemem(addr, temp);
}
void CPU::lsrAbs16()
{
    uint16_t temp;
    addr = absolute();
    temp = snes->readmemw(addr);
    cycles -= 6;
    snes->clockspc(6);
    p.c = temp & 1;
    temp >>= 1;
    setzn16(temp);
    snes->writememw2(addr, temp);
}
void CPU::lsrAbsx8()
{
    uint8_t temp;
    addr = absolutex();
    temp = snes->readmem(addr);
    cycles -= 6;
    snes->clockspc(6);
    p.c = temp & 1;
    temp >>= 1;
    setzn8(temp);
    snes->writemem(addr, temp);
}
void CPU::lsrAbsx16()
{
    uint16_t temp;
    addr = absolutex();
    temp = snes->readmemw(addr);
    cycles -= 6;
    snes->clockspc(6);
    p.c = temp & 1;
    temp >>= 1;
    setzn16(temp);
    snes->writememw2(addr, temp);
}
void CPU::rola8()
{
    snes->readmem(pbr | pc);
    addr = p.c;
    p.c  = a.b.l & 0x80;
    a.b.l <<= 1;
    if (addr)
        a.b.l |= 1;
    setzn8(a.b.l);
}
void CPU::rola16()
{
    snes->readmem(pbr | pc);
    addr = p.c;
    p.c  = a.w & 0x8000;
    a.w <<= 1;
    if (addr)
        a.w |= 1;
    setzn16(a.w);
}
void CPU::rolZp8()
{
    uint8_t temp;
    int     tempc;
    addr = zeropage();
    temp = snes->readmem(addr);
    cycles -= 6;
    snes->clockspc(6);
    tempc = p.c;
    p.c   = temp & 0x80;
    temp <<= 1;
    if (tempc)
        temp |= 1;
    setzn8(temp);
    snes->writemem(addr, temp);
}
void CPU::rolZp16()
{
    uint16_t temp;
    int      tempc;
    addr = zeropage();
    temp = snes->readmemw(addr);
    cycles -= 6;
    snes->clockspc(6);
    tempc = p.c;
    p.c   = temp & 0x8000;
    temp <<= 1;
    if (tempc)
        temp |= 1;
    setzn16(temp);
    snes->writememw2(addr, temp);
}
void CPU::rolZpx8()
{
    uint8_t temp;
    int     tempc;
    addr = zeropagex();
    temp = snes->readmem(addr);
    cycles -= 6;
    snes->clockspc(6);
    tempc = p.c;
    p.c   = temp & 0x80;
    temp <<= 1;
    if (tempc)
        temp |= 1;
    setzn8(temp);
    snes->writemem(addr, temp);
}
void CPU::rolZpx16()
{
    uint16_t temp;
    int      tempc;
    addr = zeropagex();
    temp = snes->readmemw(addr);
    cycles -= 6;
    snes->clockspc(6);
    tempc = p.c;
    p.c   = temp & 0x8000;
    temp <<= 1;
    if (tempc)
        temp |= 1;
    setzn16(temp);
    snes->writememw2(addr, temp);
}
void CPU::rolAbs8()
{
    uint8_t temp;
    int     tempc;
    addr = absolute();
    temp = snes->readmem(addr);
    cycles -= 6;
    snes->clockspc(6);
    tempc = p.c;
    p.c   = temp & 0x80;
    temp <<= 1;
    if (tempc)
        temp |= 1;
    setzn8(temp);
    snes->writemem(addr, temp);
}
void CPU::rolAbs16()
{
    uint16_t temp;
    int      tempc;
    addr = absolute();
    temp = snes->readmemw(addr);
    cycles -= 6;
    snes->clockspc(6);
    tempc = p.c;
    p.c   = temp & 0x8000;
    temp <<= 1;
    if (tempc)
        temp |= 1;
    setzn16(temp);
    snes->writememw2(addr, temp);
}
void CPU::rolAbsx8()
{
    uint8_t temp;
    int     tempc;
    addr = absolutex();
    temp = snes->readmem(addr);
    cycles -= 6;
    snes->clockspc(6);
    tempc = p.c;
    p.c   = temp & 0x80;
    temp <<= 1;
    if (tempc)
        temp |= 1;
    setzn8(temp);
    snes->writemem(addr, temp);
}
void CPU::rolAbsx16()
{
    uint16_t temp;
    int      tempc;
    addr = absolutex();
    temp = snes->readmemw(addr);
    cycles -= 6;
    snes->clockspc(6);
    tempc = p.c;
    p.c   = temp & 0x8000;
    temp <<= 1;
    if (tempc)
        temp |= 1;
    setzn16(temp);
    snes->writememw2(addr, temp);
}
void CPU::rora8()
{
    snes->readmem(pbr | pc);
    addr = p.c;
    p.c  = a.b.l & 1;
    a.b.l >>= 1;
    if (addr)
        a.b.l |= 0x80;
    setzn8(a.b.l);
}
void CPU::rora16()
{
    snes->readmem(pbr | pc);
    addr = p.c;
    p.c  = a.w & 1;
    a.w >>= 1;
    if (addr)
        a.w |= 0x8000;
    setzn16(a.w);
}
void CPU::rorZp8()
{
    uint8_t temp;
    int     tempc;
    addr = zeropage();
    temp = snes->readmem(addr);
    cycles -= 6;
    snes->clockspc(6);
    tempc = p.c;
    p.c   = temp & 1;
    temp >>= 1;
    if (tempc)
        temp |= 0x80;
    setzn8(temp);
    snes->writemem(addr, temp);
}
void CPU::rorZp16()
{
    uint16_t temp;
    int      tempc;
    addr = zeropage();
    temp = snes->readmemw(addr);
    cycles -= 6;
    snes->clockspc(6);
    tempc = p.c;
    p.c   = temp & 1;
    temp >>= 1;
    if (tempc)
        temp |= 0x8000;
    setzn16(temp);
    snes->writememw2(addr, temp);
}
void CPU::rorZpx8()
{
    uint8_t temp;
    int     tempc;
    addr = zeropagex();
    temp = snes->readmem(addr);
    cycles -= 6;
    snes->clockspc(6);
    tempc = p.c;
    p.c   = temp & 1;
    temp >>= 1;
    if (tempc)
        temp |= 0x80;
    setzn8(temp);
    snes->writemem(addr, temp);
}
void CPU::rorZpx16()
{
    uint16_t temp;
    int      tempc;
    addr = zeropagex();
    temp = snes->readmemw(addr);
    cycles -= 6;
    snes->clockspc(6);
    tempc = p.c;
    p.c   = temp & 1;
    temp >>= 1;
    if (tempc)
        temp |= 0x8000;
    setzn16(temp);
    snes->writememw2(addr, temp);
}
void CPU::rorAbs8()
{
    uint8_t temp;
    int     tempc;
    addr = absolute();
    temp = snes->readmem(addr);
    cycles -= 6;
    snes->clockspc(6);
    tempc = p.c;
    p.c   = temp & 1;
    temp >>= 1;
    if (tempc)
        temp |= 0x80;
    setzn8(temp);
    snes->writemem(addr, temp);
}
void CPU::rorAbs16()
{
    uint16_t temp;
    int      tempc;
    addr = absolute();
    temp = snes->readmemw(addr);
    cycles -= 6;
    snes->clockspc(6);
    tempc = p.c;
    p.c   = temp & 1;
    temp >>= 1;
    if (tempc)
        temp |= 0x8000;
    setzn16(temp);
    snes->writememw2(addr, temp);
}
void CPU::rorAbsx8()
{
    uint8_t temp;
    int     tempc;
    addr = absolutex();
    temp = snes->readmem(addr);
    cycles -= 6;
    snes->clockspc(6);
    tempc = p.c;
    p.c   = temp & 1;
    temp >>= 1;
    if (tempc)
        temp |= 0x80;
    setzn8(temp);
    snes->writemem(addr, temp);
}
void CPU::rorAbsx16()
{
    uint16_t temp;
    int      tempc;
    addr = absolutex();
    temp = snes->readmemw(addr);
    cycles -= 6;
    snes->clockspc(6);
    tempc = p.c;
    p.c   = temp & 1;
    temp >>= 1;
    if (tempc)
        temp |= 0x8000;
    setzn16(temp);
    snes->writememw2(addr, temp);
}
void CPU::xba()
{
    snes->readmem(pbr | pc);
    a.w = (a.w >> 8) | (a.w << 8);
    setzn8(a.b.l);
}
void CPU::nop()
{
    cycles -= 6;
    snes->clockspc(6);
}
void CPU::tcd()
{
    snes->readmem(pbr | pc);
    dp = a.w;
    setzn16(dp);
}
void CPU::tdc()
{
    snes->readmem(pbr | pc);
    a.w = dp;
    setzn16(a.w);
}
void CPU::tcs()
{
    snes->readmem(pbr | pc);
    s.w = a.w;
}
void CPU::tsc()
{
    snes->readmem(pbr | pc);
    a.w = s.w;
    setzn16(a.w);
}
void CPU::trbZp8()
{
    uint8_t temp;
    addr = zeropage();
    temp = snes->readmem(addr);
    p.z  = !(a.b.l & temp);
    temp &= ~a.b.l;
    cycles -= 6;
    snes->clockspc(6);
    snes->writemem(addr, temp);
}
void CPU::trbZp16()
{
    uint16_t temp;
    addr = zeropage();
    temp = snes->readmemw(addr);
    p.z  = !(a.w & temp);
    temp &= ~a.w;
    cycles -= 6;
    snes->clockspc(6);
    snes->writememw2(addr, temp);
}
void CPU::trbAbs8()
{
    uint8_t temp;
    addr = absolute();
    temp = snes->readmem(addr);
    p.z  = !(a.b.l & temp);
    temp &= ~a.b.l;
    cycles -= 6;
    snes->clockspc(6);
    snes->writemem(addr, temp);
}
void CPU::trbAbs16()
{
    uint16_t temp;
    addr = absolute();
    temp = snes->readmemw(addr);
    p.z  = !(a.w & temp);
    temp &= ~a.w;
    cycles -= 6;
    snes->clockspc(6);
    snes->writememw2(addr, temp);
}
void CPU::tsbZp8()
{
    uint8_t temp;
    addr = zeropage();
    temp = snes->readmem(addr);
    p.z  = !(a.b.l & temp);
    temp |= a.b.l;
    cycles -= 6;
    snes->clockspc(6);
    snes->writemem(addr, temp);
}
void CPU::tsbZp16()
{
    uint16_t temp;
    addr = zeropage();
    temp = snes->readmemw(addr);
    p.z  = !(a.w & temp);
    temp |= a.w;
    cycles -= 6;
    snes->clockspc(6);
    snes->writememw2(addr, temp);
}
void CPU::tsbAbs8()
{
    uint8_t temp;
    addr = absolute();
    temp = snes->readmem(addr);
    p.z  = !(a.b.l & temp);
    temp |= a.b.l;
    cycles -= 6;
    snes->clockspc(6);
    snes->writemem(addr, temp);
}
void CPU::tsbAbs16()
{
    uint16_t temp;
    addr = absolute();
    temp = snes->readmemw(addr);
    p.z  = !(a.w & temp);
    temp |= a.w;
    cycles -= 6;
    snes->clockspc(6);
    snes->writememw2(addr, temp);
}
void CPU::wai()
{
    snes->readmem(pbr | pc);
    inwai = 1;
    pc--;
}
void CPU::mvp()
{
    uint8_t temp;
    dbr = (snes->readmem(pbr | pc)) << 16;
    pc++;
    addr = (snes->readmem(pbr | pc)) << 16;
    pc++;
    temp = snes->readmem(addr | x.w);
    snes->writemem(dbr | y.w, temp);
    x.w--;
    y.w--;
    a.w--;
    if (a.w != 0xFFFF)
        pc -= 3;
    cycles -= 12;
    snes->clockspc(12);
}
void CPU::mvn()
{
    uint8_t temp;
    dbr = (snes->readmem(pbr | pc)) << 16;
    pc++;
    addr = (snes->readmem(pbr | pc)) << 16;
    pc++;
    temp = snes->readmem(addr | x.w);
    snes->writemem(dbr | y.w, temp);
    x.w++;
    y.w++;
    a.w--;
    if (a.w != 0xFFFF)
        pc -= 3;
    cycles -= 12;
    snes->clockspc(12);
}
void CPU::brk()
{
    uint8_t temp = 0;
    snes->writemem(s.w, pbr >> 16);
    s.w--;
    snes->writemem(s.w, pc >> 8);
    s.w--;
    snes->writemem(s.w, pc & 0xFF);
    s.w--;
    if (p.c)
        temp |= 1;
    if (p.z)
        temp |= 2;
    if (p.i)
        temp |= 4;
    if (p.d)
        temp |= 8;
    if (p.x)
        temp |= 0x10;
    if (p.m)
        temp |= 0x20;
    if (p.v)
        temp |= 0x40;
    if (p.n)
        temp |= 0x80;
    snes->writemem(s.w, temp);
    s.w--;
    pc  = snes->readmemw(0xFFE6);
    pbr = 0;
    p.i = 1;
    p.d = 0;
}
void CPU::reset65c816()
{
    pbr = dbr = 0;
    s.w       = 0x1FF;
    cpumode   = 4;
    p.e       = 1;
    p.i       = 1;
    pc        = snes->readmemw(0xFFFC);
    a.w = x.w = y.w = 0;
    p.x = p.m = 1;
}
void CPU::badopcode()
{
    printf("CPU - Bad opcode %02X\n", opcode);
    // pc--;
    // exit(-1);
}
void CPU::nmi65c816()
{
    uint8_t temp = 0;
    snes->readmem(pbr | pc);
    cycles -= 6;
    snes->clockspc(6);
    if (inwai)
        pc++;
    inwai = 0;
    if (!p.e) {
        snes->writemem(s.w, pbr >> 16);
        s.w--;
        snes->writemem(s.w, pc >> 8);
        s.w--;
        snes->writemem(s.w, pc & 0xFF);
        s.w--;
        if (p.c)
            temp |= 1;
        if (p.z)
            temp |= 2;
        if (p.i)
            temp |= 4;
        if (p.d)
            temp |= 8;
        if (p.x)
            temp |= 0x10;
        if (p.m)
            temp |= 0x20;
        if (p.v)
            temp |= 0x40;
        if (p.n)
            temp |= 0x80;
        snes->writemem(s.w, temp);
        s.w--;
        pc  = snes->readmemw(0xFFEA);
        pbr = 0;
        p.i = 1;
        p.d = 0;
    } else {
        exit(-1);
    }
}
void CPU::irq65c816()
{
    uint8_t temp = 0;
    snes->readmem(pbr | pc);
    cycles -= 6;
    snes->clockspc(6);
    if (inwai && p.i) {
        pc++;
        inwai = 0;
        return;
    }
    if (inwai)
        pc++;
    inwai = 0;
    if (!p.e) {
        snes->writemem(s.w, pbr >> 16);
        s.w--;
        snes->writemem(s.w, pc >> 8);
        s.w--;
        snes->writemem(s.w, pc & 0xFF);
        s.w--;
        if (p.c)
            temp |= 1;
        if (p.z)
            temp |= 2;
        if (p.i)
            temp |= 4;
        if (p.d)
            temp |= 8;
        if (p.x)
            temp |= 0x10;
        if (p.m)
            temp |= 0x20;
        if (p.v)
            temp |= 0x40;
        if (p.n)
            temp |= 0x80;
        snes->writemem(s.w, temp);
        s.w--;
        pc  = snes->readmemw(0xFFEE);
        pbr = 0;
        p.i = 1;
        p.d = 0;
    } else {
        exit(-1);
    }
}
