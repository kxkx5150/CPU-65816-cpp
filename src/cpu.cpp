
#include "cpu.h"
#include "snes.h"
#include <cstdio>


CPU::CPU(Snes *_snes)
{
    snes = _snes;
}
uint8_t CPU::cpu_read(uint32_t adr)
{
    return snes->snes_cpuRead(adr);
}
void CPU::cpu_write(uint32_t adr, uint8_t val)
{
    snes->snes_cpuWrite(adr, val);
}
CPU *CPU::cpu_init(void *mem, int memType)
{
    return this;
}
void CPU::cpu_reset()
{
    a          = 0;
    x          = 0;
    y          = 0;
    sp         = 0x100;
    pc         = cpu_read(0xfffc) | (cpu_read(0xfffd) << 8);
    dp         = 0;
    k          = 0;
    db         = 0;
    c          = false;
    z          = false;
    v          = false;
    n          = false;
    i          = true;
    d          = false;
    xf         = true;
    mf         = true;
    e          = true;
    irqWanted  = false;
    nmiWanted  = false;
    waiting    = false;
    stopped    = false;
    cyclesUsed = 0;
}
int CPU::cpu_runOpcode()
{
    cyclesUsed = 0;
    if (stopped)
        return 1;

    if (waiting) {
        if (irqWanted || nmiWanted) {
            waiting = false;
        }
        return 1;
    }

    if ((!i && irqWanted) || nmiWanted) {
        cyclesUsed = 7;
        if (nmiWanted) {
            nmiWanted = false;
            cpu_doInterrupt(false);
        } else {
            cpu_doInterrupt(true);
        }
    } else {
        uint8_t opcode = cpu_readOpcode();
        cyclesUsed     = cyclesPerOpcode[opcode];
        cpu_doOpcode(opcode);
    }

    return cyclesUsed;
}
uint8_t CPU::cpu_readOpcode()
{
    return cpu_read((k << 16) | pc++);
}
uint16_t CPU::cpu_readOpcodeWord()
{
    uint8_t low = cpu_readOpcode();
    return low | (cpu_readOpcode() << 8);
}
uint8_t CPU::cpu_getFlags()
{
    uint8_t val = n << 7;
    val |= v << 6;
    val |= mf << 5;
    val |= xf << 4;
    val |= d << 3;
    val |= i << 2;
    val |= z << 1;
    val |= c;
    return val;
}
void CPU::cpu_setFlags(uint8_t val)
{
    n  = val & 0x80;
    v  = val & 0x40;
    mf = val & 0x20;
    xf = val & 0x10;
    d  = val & 8;
    i  = val & 4;
    z  = val & 2;
    c  = val & 1;

    if (e) {
        mf = true;
        xf = true;
        sp = (sp & 0xff) | 0x100;
    }

    if (xf) {
        x &= 0xff;
        y &= 0xff;
    }
}
void CPU::cpu_setZN(uint16_t value, bool byte)
{
    if (byte) {
        z = (value & 0xff) == 0;
        n = value & 0x80;
    } else {
        z = value == 0;
        n = value & 0x8000;
    }
}
void CPU::cpu_doBranch(uint8_t value, bool check)
{
    if (check) {
        cyclesUsed++;
        pc += (int8_t)value;
    }
}
uint8_t CPU::cpu_pullByte()
{
    sp++;
    if (e)
        sp = (sp & 0xff) | 0x100;

    return cpu_read(sp);
}
void CPU::cpu_pushByte(uint8_t value)
{
    cpu_write(sp, value);
    sp--;
    if (e)
        sp = (sp & 0xff) | 0x100;
}
uint16_t CPU::cpu_pullWord()
{
    uint8_t value = cpu_pullByte();
    return value | (cpu_pullByte() << 8);
}
void CPU::cpu_pushWord(uint16_t value)
{
    cpu_pushByte(value >> 8);
    cpu_pushByte(value & 0xff);
}
uint16_t CPU::cpu_readWord(uint32_t adrl, uint32_t adrh)
{
    uint8_t value = cpu_read(adrl);
    return value | (cpu_read(adrh) << 8);
}
void CPU::cpu_writeWord(uint32_t adrl, uint32_t adrh, uint16_t value, bool reversed)
{
    if (reversed) {
        cpu_write(adrh, value >> 8);
        cpu_write(adrl, value & 0xff);
    } else {
        cpu_write(adrl, value & 0xff);
        cpu_write(adrh, value >> 8);
    }
}
void CPU::cpu_doInterrupt(bool irq)
{
    cpu_pushByte(k);
    cpu_pushWord(pc);
    cpu_pushByte(cpu_getFlags());
    cyclesUsed++;
    i = true;
    d = false;
    k = 0;

    if (irq) {
        pc = cpu_readWord(0xffee, 0xffef);
    } else {
        pc = cpu_readWord(0xffea, 0xffeb);
    }
}
uint32_t CPU::cpu_adrImm(uint32_t *low, bool xFlag)
{
    if ((xFlag && xf) || (!xFlag && mf)) {
        *low = (k << 16) | pc++;
        return 0;
    } else {
        *low = (k << 16) | pc++;
        return (k << 16) | pc++;
    }
}
uint32_t CPU::cpu_adrDp(uint32_t *low)
{
    uint8_t adr = cpu_readOpcode();
    if (dp & 0xff)
        cyclesUsed++;
    *low = (dp + adr) & 0xffff;
    return (dp + adr + 1) & 0xffff;
}
uint32_t CPU::cpu_adrDpx(uint32_t *low)
{
    uint8_t adr = cpu_readOpcode();
    if (dp & 0xff)
        cyclesUsed++;
    *low = (dp + adr + x) & 0xffff;
    return (dp + adr + x + 1) & 0xffff;
}
uint32_t CPU::cpu_adrDpy(uint32_t *low)
{
    uint8_t adr = cpu_readOpcode();
    if (dp & 0xff)
        cyclesUsed++;
    *low = (dp + adr + y) & 0xffff;
    return (dp + adr + y + 1) & 0xffff;
}
uint32_t CPU::cpu_adrIdp(uint32_t *low)
{
    uint8_t adr = cpu_readOpcode();
    if (dp & 0xff)
        cyclesUsed++;
    uint16_t pointer = cpu_readWord((dp + adr) & 0xffff, (dp + adr + 1) & 0xffff);
    *low             = (db << 16) + pointer;
    return ((db << 16) + pointer + 1) & 0xffffff;
}
uint32_t CPU::cpu_adrIdx(uint32_t *low)
{
    uint8_t adr = cpu_readOpcode();
    if (dp & 0xff)
        cyclesUsed++;
    uint16_t pointer = cpu_readWord((dp + adr + x) & 0xffff, (dp + adr + x + 1) & 0xffff);
    *low             = (db << 16) + pointer;
    return ((db << 16) + pointer + 1) & 0xffffff;
}
uint32_t CPU::cpu_adrIdy(uint32_t *low, bool write)
{
    uint8_t adr = cpu_readOpcode();
    if (dp & 0xff)
        cyclesUsed++;
    uint16_t pointer = cpu_readWord((dp + adr) & 0xffff, (dp + adr + 1) & 0xffff);
    if (write && (!xf || ((pointer >> 8) != ((pointer + y) >> 8))))
        cyclesUsed++;
    *low = ((db << 16) + pointer + y) & 0xffffff;
    return ((db << 16) + pointer + y + 1) & 0xffffff;
}
uint32_t CPU::cpu_adrIdl(uint32_t *low)
{
    uint8_t adr = cpu_readOpcode();
    if (dp & 0xff)
        cyclesUsed++;
    uint32_t pointer = cpu_readWord((dp + adr) & 0xffff, (dp + adr + 1) & 0xffff);
    pointer |= cpu_read((dp + adr + 2) & 0xffff) << 16;
    *low = pointer;
    return (pointer + 1) & 0xffffff;
}
uint32_t CPU::cpu_adrIly(uint32_t *low)
{
    uint8_t adr = cpu_readOpcode();
    if (dp & 0xff)
        cyclesUsed++;
    uint32_t pointer = cpu_readWord((dp + adr) & 0xffff, (dp + adr + 1) & 0xffff);
    pointer |= cpu_read((dp + adr + 2) & 0xffff) << 16;
    *low = (pointer + y) & 0xffffff;
    return (pointer + y + 1) & 0xffffff;
}
uint32_t CPU::cpu_adrSr(uint32_t *low)
{
    uint8_t adr = cpu_readOpcode();
    *low        = (sp + adr) & 0xffff;
    return (sp + adr + 1) & 0xffff;
}
uint32_t CPU::cpu_adrIsy(uint32_t *low)
{
    uint8_t  adr     = cpu_readOpcode();
    uint16_t pointer = cpu_readWord((sp + adr) & 0xffff, (sp + adr + 1) & 0xffff);
    *low             = ((db << 16) + pointer + y) & 0xffffff;
    return ((db << 16) + pointer + y + 1) & 0xffffff;
}
uint32_t CPU::cpu_adrAbs(uint32_t *low)
{
    uint16_t adr = cpu_readOpcodeWord();
    *low         = (db << 16) + adr;
    return ((db << 16) + adr + 1) & 0xffffff;
}
uint32_t CPU::cpu_adrAbx(uint32_t *low, bool write)
{
    uint16_t adr = cpu_readOpcodeWord();
    if (write && (!xf || ((adr >> 8) != ((adr + x) >> 8))))
        cyclesUsed++;
    *low = ((db << 16) + adr + x) & 0xffffff;
    return ((db << 16) + adr + x + 1) & 0xffffff;
}
uint32_t CPU::cpu_adrAby(uint32_t *low, bool write)
{
    uint16_t adr = cpu_readOpcodeWord();
    if (write && (!xf || ((adr >> 8) != ((adr + y) >> 8))))
        cyclesUsed++;
    *low = ((db << 16) + adr + y) & 0xffffff;
    return ((db << 16) + adr + y + 1) & 0xffffff;
}
uint32_t CPU::cpu_adrAbl(uint32_t *low)
{
    uint32_t adr = cpu_readOpcodeWord();
    adr |= cpu_readOpcode() << 16;
    *low = adr;
    return (adr + 1) & 0xffffff;
}
uint32_t CPU::cpu_adrAlx(uint32_t *low)
{
    uint32_t adr = cpu_readOpcodeWord();
    adr |= cpu_readOpcode() << 16;
    *low = (adr + x) & 0xffffff;
    return (adr + x + 1) & 0xffffff;
}
uint16_t CPU::cpu_adrIax()
{
    uint16_t adr = cpu_readOpcodeWord();
    return cpu_readWord((k << 16) | ((adr + x) & 0xffff), (k << 16) | ((adr + x + 1) & 0xffff));
}
void CPU::cpu_and(uint32_t low, uint32_t high)
{
    if (mf) {
        uint8_t value = cpu_read(low);
        a             = (a & 0xff00) | ((a & value) & 0xff);
    } else {
        cyclesUsed++;
        uint16_t value = cpu_readWord(low, high);
        a &= value;
    }
    cpu_setZN(a, mf);
}
void CPU::cpu_ora(uint32_t low, uint32_t high)
{
    if (mf) {
        uint8_t value = cpu_read(low);
        a             = (a & 0xff00) | ((a | value) & 0xff);
    } else {
        cyclesUsed++;
        uint16_t value = cpu_readWord(low, high);
        a |= value;
    }
    cpu_setZN(a, mf);
}
void CPU::cpu_eor(uint32_t low, uint32_t high)
{
    if (mf) {
        uint8_t value = cpu_read(low);
        a             = (a & 0xff00) | ((a ^ value) & 0xff);
    } else {
        cyclesUsed++;
        uint16_t value = cpu_readWord(low, high);
        a ^= value;
    }
    cpu_setZN(a, mf);
}
void CPU::cpu_adc(uint32_t low, uint32_t high)
{
    if (mf) {
        uint8_t value  = cpu_read(low);
        int     result = 0;
        if (d) {
            result = (a & 0xf) + (value & 0xf) + c;
            if (result > 0x9)
                result = ((result + 0x6) & 0xf) + 0x10;
            result = (a & 0xf0) + (value & 0xf0) + result;
        } else {
            result = (a & 0xff) + value + c;
        }

        v = (a & 0x80) == (value & 0x80) && (value & 0x80) != (result & 0x80);

        if (d && result > 0x9f)
            result += 0x60;

        c = result > 0xff;
        a = (a & 0xff00) | (result & 0xff);
    } else {
        cyclesUsed++;
        uint16_t value  = cpu_readWord(low, high);
        int      result = 0;
        if (d) {
            result = (a & 0xf) + (value & 0xf) + c;
            if (result > 0x9)
                result = ((result + 0x6) & 0xf) + 0x10;
            result = (a & 0xf0) + (value & 0xf0) + result;
            if (result > 0x9f)
                result = ((result + 0x60) & 0xff) + 0x100;
            result = (a & 0xf00) + (value & 0xf00) + result;
            if (result > 0x9ff)
                result = ((result + 0x600) & 0xfff) + 0x1000;
            result = (a & 0xf000) + (value & 0xf000) + result;
        } else {
            result = a + value + c;
        }

        v = (a & 0x8000) == (value & 0x8000) && (value & 0x8000) != (result & 0x8000);

        if (d && result > 0x9fff)
            result += 0x6000;
        c = result > 0xffff;
        a = result;
    }
    cpu_setZN(a, mf);
}
void CPU::cpu_sbc(uint32_t low, uint32_t high)
{
    if (mf) {
        uint8_t value  = cpu_read(low) ^ 0xff;
        int     result = 0;

        if (d) {
            result = (a & 0xf) + (value & 0xf) + c;
            if (result < 0x10)
                result = (result - 0x6) & ((result - 0x6 < 0) ? 0xf : 0x1f);
            result = (a & 0xf0) + (value & 0xf0) + result;
        } else {
            result = (a & 0xff) + value + c;
        }

        v = (a & 0x80) == (value & 0x80) && (value & 0x80) != (result & 0x80);

        if (d && result < 0x100)
            result -= 0x60;

        c = result > 0xff;
        a = (a & 0xff00) | (result & 0xff);
    } else {
        cyclesUsed++;
        uint16_t value  = cpu_readWord(low, high) ^ 0xffff;
        int      result = 0;
        if (d) {
            result = (a & 0xf) + (value & 0xf) + c;
            if (result < 0x10)
                result = (result - 0x6) & ((result - 0x6 < 0) ? 0xf : 0x1f);

            result = (a & 0xf0) + (value & 0xf0) + result;
            if (result < 0x100)
                result = (result - 0x60) & ((result - 0x60 < 0) ? 0xff : 0x1ff);

            result = (a & 0xf00) + (value & 0xf00) + result;
            if (result < 0x1000)
                result = (result - 0x600) & ((result - 0x600 < 0) ? 0xfff : 0x1fff);

            result = (a & 0xf000) + (value & 0xf000) + result;
        } else {
            result = a + value + c;
        }

        v = (a & 0x8000) == (value & 0x8000) && (value & 0x8000) != (result & 0x8000);
        if (d && result < 0x10000)
            result -= 0x6000;

        c = result > 0xffff;
        a = result;
    }
    cpu_setZN(a, mf);
}
void CPU::cpu_cmp(uint32_t low, uint32_t high)
{
    int result = 0;
    if (mf) {
        uint8_t value = cpu_read(low) ^ 0xff;
        result        = (a & 0xff) + value + 1;
        c             = result > 0xff;
    } else {
        cyclesUsed++;
        uint16_t value = cpu_readWord(low, high) ^ 0xffff;
        result         = a + value + 1;
        c              = result > 0xffff;
    }
    cpu_setZN(result, mf);
}
void CPU::cpu_cpx(uint32_t low, uint32_t high)
{
    int result = 0;
    if (xf) {
        uint8_t value = cpu_read(low) ^ 0xff;
        result        = (x & 0xff) + value + 1;
        c             = result > 0xff;
    } else {
        cyclesUsed++;
        uint16_t value = cpu_readWord(low, high) ^ 0xffff;
        result         = x + value + 1;
        c              = result > 0xffff;
    }
    cpu_setZN(result, xf);
}
void CPU::cpu_cpy(uint32_t low, uint32_t high)
{
    int result = 0;
    if (xf) {
        uint8_t value = cpu_read(low) ^ 0xff;
        result        = (y & 0xff) + value + 1;
        c             = result > 0xff;
    } else {
        cyclesUsed++;
        uint16_t value = cpu_readWord(low, high) ^ 0xffff;
        result         = y + value + 1;
        c              = result > 0xffff;
    }
    cpu_setZN(result, xf);
}
void CPU::cpu_bit(uint32_t low, uint32_t high)
{
    if (mf) {
        uint8_t value  = cpu_read(low);
        uint8_t result = (a & 0xff) & value;
        z              = result == 0;
        n              = value & 0x80;
        v              = value & 0x40;
    } else {
        cyclesUsed++;
        uint16_t value  = cpu_readWord(low, high);
        uint16_t result = a & value;
        z               = result == 0;
        n               = value & 0x8000;
        v               = value & 0x4000;
    }
}
void CPU::cpu_lda(uint32_t low, uint32_t high)
{
    if (mf) {
        a = (a & 0xff00) | cpu_read(low);
    } else {
        cyclesUsed++;
        a = cpu_readWord(low, high);
    }
    cpu_setZN(a, mf);
}
void CPU::cpu_ldx(uint32_t low, uint32_t high)
{
    if (xf) {
        x = cpu_read(low);
    } else {
        cyclesUsed++;
        x = cpu_readWord(low, high);
    }
    cpu_setZN(x, xf);
}
void CPU::cpu_ldy(uint32_t low, uint32_t high)
{
    if (xf) {
        y = cpu_read(low);
    } else {
        cyclesUsed++;
        y = cpu_readWord(low, high);
    }
    cpu_setZN(y, xf);
}
void CPU::cpu_sta(uint32_t low, uint32_t high)
{
    if (mf) {
        cpu_write(low, a);
    } else {
        cyclesUsed++;
        cpu_writeWord(low, high, a, false);
    }
}
void CPU::cpu_stx(uint32_t low, uint32_t high)
{
    if (xf) {
        cpu_write(low, x);
    } else {
        cyclesUsed++;
        cpu_writeWord(low, high, x, false);
    }
}
void CPU::cpu_sty(uint32_t low, uint32_t high)
{
    if (xf) {
        cpu_write(low, y);
    } else {
        cyclesUsed++;
        cpu_writeWord(low, high, y, false);
    }
}
void CPU::cpu_stz(uint32_t low, uint32_t high)
{
    if (mf) {
        cpu_write(low, 0);
    } else {
        cyclesUsed++;
        cpu_writeWord(low, high, 0, false);
    }
}
void CPU::cpu_ror(uint32_t low, uint32_t high)
{
    bool carry  = false;
    int  result = 0;
    if (mf) {
        uint8_t value = cpu_read(low);
        carry         = value & 1;
        result        = (value >> 1) | (c << 7);
        cpu_write(low, result);
    } else {
        cyclesUsed += 2;
        uint16_t value = cpu_readWord(low, high);
        carry          = value & 1;
        result         = (value >> 1) | (c << 15);
        cpu_writeWord(low, high, result, true);
    }
    cpu_setZN(result, mf);
    c = carry;
}
void CPU::cpu_rol(uint32_t low, uint32_t high)
{
    int result = 0;
    if (mf) {
        result = (cpu_read(low) << 1) | c;
        c      = result & 0x100;
        cpu_write(low, result);
    } else {
        cyclesUsed += 2;
        result = (cpu_readWord(low, high) << 1) | c;
        c      = result & 0x10000;
        cpu_writeWord(low, high, result, true);
    }
    cpu_setZN(result, mf);
}
void CPU::cpu_lsr(uint32_t low, uint32_t high)
{
    int result = 0;
    if (mf) {
        uint8_t value = cpu_read(low);
        c             = value & 1;
        result        = value >> 1;
        cpu_write(low, result);
    } else {
        cyclesUsed += 2;
        uint16_t value = cpu_readWord(low, high);
        c              = value & 1;
        result         = value >> 1;
        cpu_writeWord(low, high, result, true);
    }
    cpu_setZN(result, mf);
}
void CPU::cpu_asl(uint32_t low, uint32_t high)
{
    int result = 0;
    if (mf) {
        result = cpu_read(low) << 1;
        c      = result & 0x100;
        cpu_write(low, result);
    } else {
        cyclesUsed += 2;
        result = cpu_readWord(low, high) << 1;
        c      = result & 0x10000;
        cpu_writeWord(low, high, result, true);
    }
    cpu_setZN(result, mf);
}
void CPU::cpu_inc(uint32_t low, uint32_t high)
{
    int result = 0;
    if (mf) {
        result = cpu_read(low) + 1;
        cpu_write(low, result);
    } else {
        cyclesUsed += 2;
        result = cpu_readWord(low, high) + 1;
        cpu_writeWord(low, high, result, true);
    }
    cpu_setZN(result, mf);
}
void CPU::cpu_dec(uint32_t low, uint32_t high)
{
    int result = 0;
    if (mf) {
        result = cpu_read(low) - 1;
        cpu_write(low, result);
    } else {
        cyclesUsed += 2;
        result = cpu_readWord(low, high) - 1;
        cpu_writeWord(low, high, result, true);
    }
    cpu_setZN(result, mf);
}
void CPU::cpu_tsb(uint32_t low, uint32_t high)
{
    if (mf) {
        uint8_t value = cpu_read(low);
        z             = ((a & 0xff) & value) == 0;
        cpu_write(low, value | (a & 0xff));
    } else {
        cyclesUsed += 2;
        uint16_t value = cpu_readWord(low, high);
        z              = (a & value) == 0;
        cpu_writeWord(low, high, value | a, true);
    }
}
void CPU::cpu_trb(uint32_t low, uint32_t high)
{
    if (mf) {
        uint8_t value = cpu_read(low);
        z             = ((a & 0xff) & value) == 0;
        cpu_write(low, value & ~(a & 0xff));
    } else {
        cyclesUsed += 2;
        uint16_t value = cpu_readWord(low, high);
        z              = (a & value) == 0;
        cpu_writeWord(low, high, value & ~a, true);
    }
}
void CPU::cpu_doOpcode(uint8_t opcode)
{
    switch (opcode) {
        case 0x00: {
            cpu_pushByte(k);
            cpu_pushWord(pc + 1);
            cpu_pushByte(cpu_getFlags());
            cyclesUsed++;
            i  = true;
            d  = false;
            k  = 0;
            pc = cpu_readWord(0xffe6, 0xffe7);
            break;
        }
        case 0x01: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIdx(&low);
            cpu_ora(low, high);
            break;
        }
        case 0x02: {
            cpu_readOpcode();
            cpu_pushByte(k);
            cpu_pushWord(pc);
            cpu_pushByte(cpu_getFlags());
            cyclesUsed++;
            i  = true;
            d  = false;
            k  = 0;
            pc = cpu_readWord(0xffe4, 0xffe5);
            break;
        }
        case 0x03: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrSr(&low);
            cpu_ora(low, high);
            break;
        }
        case 0x04: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDp(&low);
            cpu_tsb(low, high);
            break;
        }
        case 0x05: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDp(&low);
            cpu_ora(low, high);
            break;
        }
        case 0x06: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDp(&low);
            cpu_asl(low, high);
            break;
        }
        case 0x07: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIdl(&low);
            cpu_ora(low, high);
            break;
        }
        case 0x08: {
            cpu_pushByte(cpu_getFlags());
            break;
        }
        case 0x09: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrImm(&low, false);
            cpu_ora(low, high);
            break;
        }
        case 0x0a: {
            if (mf) {
                c = a & 0x80;
                a = (a & 0xff00) | ((a << 1) & 0xff);
            } else {
                c = a & 0x8000;
                a <<= 1;
            }
            cpu_setZN(a, mf);
            break;
        }
        case 0x0b: {
            cpu_pushWord(dp);
            break;
        }
        case 0x0c: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbs(&low);
            cpu_tsb(low, high);
            break;
        }
        case 0x0d: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbs(&low);
            cpu_ora(low, high);
            break;
        }
        case 0x0e: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbs(&low);
            cpu_asl(low, high);
            break;
        }
        case 0x0f: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbl(&low);
            cpu_ora(low, high);
            break;
        }
        case 0x10: {
            cpu_doBranch(cpu_readOpcode(), !n);
            break;
        }
        case 0x11: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIdy(&low, false);
            cpu_ora(low, high);
            break;
        }
        case 0x12: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIdp(&low);
            cpu_ora(low, high);
            break;
        }
        case 0x13: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIsy(&low);
            cpu_ora(low, high);
            break;
        }
        case 0x14: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDp(&low);
            cpu_trb(low, high);
            break;
        }
        case 0x15: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDpx(&low);
            cpu_ora(low, high);
            break;
        }
        case 0x16: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDpx(&low);
            cpu_asl(low, high);
            break;
        }
        case 0x17: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIly(&low);
            cpu_ora(low, high);
            break;
        }
        case 0x18: {
            c = false;
            break;
        }
        case 0x19: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAby(&low, false);
            cpu_ora(low, high);
            break;
        }
        case 0x1a: {
            if (mf) {
                a = (a & 0xff00) | ((a + 1) & 0xff);
            } else {
                a++;
            }
            cpu_setZN(a, mf);
            break;
        }
        case 0x1b: {
            sp = a;
            break;
        }
        case 0x1c: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbs(&low);
            cpu_trb(low, high);
            break;
        }
        case 0x1d: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbx(&low, false);
            cpu_ora(low, high);
            break;
        }
        case 0x1e: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbx(&low, true);
            cpu_asl(low, high);
            break;
        }
        case 0x1f: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAlx(&low);
            cpu_ora(low, high);
            break;
        }
        case 0x20: {
            uint16_t value = cpu_readOpcodeWord();
            cpu_pushWord(pc - 1);
            pc = value;
            break;
        }
        case 0x21: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIdx(&low);
            cpu_and(low, high);
            break;
        }
        case 0x22: {
            uint16_t value = cpu_readOpcodeWord();
            uint8_t  newK  = cpu_readOpcode();
            cpu_pushByte(k);
            cpu_pushWord(pc - 1);
            pc = value;
            k  = newK;
            break;
        }
        case 0x23: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrSr(&low);
            cpu_and(low, high);
            break;
        }
        case 0x24: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDp(&low);
            cpu_bit(low, high);
            break;
        }
        case 0x25: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDp(&low);
            cpu_and(low, high);
            break;
        }
        case 0x26: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDp(&low);
            cpu_rol(low, high);
            break;
        }
        case 0x27: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIdl(&low);
            cpu_and(low, high);
            break;
        }
        case 0x28: {
            cpu_setFlags(cpu_pullByte());
            break;
        }
        case 0x29: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrImm(&low, false);
            cpu_and(low, high);
            break;
        }
        case 0x2a: {
            int result = (a << 1) | c;
            if (mf) {
                c = result & 0x100;
                a = (a & 0xff00) | (result & 0xff);
            } else {
                c = result & 0x10000;
                a = result;
            }
            cpu_setZN(a, mf);
            break;
        }
        case 0x2b: {
            dp = cpu_pullWord();
            cpu_setZN(dp, false);
            break;
        }
        case 0x2c: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbs(&low);
            cpu_bit(low, high);
            break;
        }
        case 0x2d: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbs(&low);
            cpu_and(low, high);
            break;
        }
        case 0x2e: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbs(&low);
            cpu_rol(low, high);
            break;
        }
        case 0x2f: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbl(&low);
            cpu_and(low, high);
            break;
        }
        case 0x30: {
            cpu_doBranch(cpu_readOpcode(), n);
            break;
        }
        case 0x31: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIdy(&low, false);
            cpu_and(low, high);
            break;
        }
        case 0x32: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIdp(&low);
            cpu_and(low, high);
            break;
        }
        case 0x33: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIsy(&low);
            cpu_and(low, high);
            break;
        }
        case 0x34: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDpx(&low);
            cpu_bit(low, high);
            break;
        }
        case 0x35: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDpx(&low);
            cpu_and(low, high);
            break;
        }
        case 0x36: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDpx(&low);
            cpu_rol(low, high);
            break;
        }
        case 0x37: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIly(&low);
            cpu_and(low, high);
            break;
        }
        case 0x38: {
            c = true;
            break;
        }
        case 0x39: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAby(&low, false);
            cpu_and(low, high);
            break;
        }
        case 0x3a: {
            if (mf) {
                a = (a & 0xff00) | ((a - 1) & 0xff);
            } else {
                a--;
            }
            cpu_setZN(a, mf);
            break;
        }
        case 0x3b: {
            a = sp;
            cpu_setZN(a, false);
            break;
        }
        case 0x3c: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbx(&low, false);
            cpu_bit(low, high);
            break;
        }
        case 0x3d: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbx(&low, false);
            cpu_and(low, high);
            break;
        }
        case 0x3e: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbx(&low, true);
            cpu_rol(low, high);
            break;
        }
        case 0x3f: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAlx(&low);
            cpu_and(low, high);
            break;
        }
        case 0x40: {
            cpu_setFlags(cpu_pullByte());
            cyclesUsed++;
            pc = cpu_pullWord();
            k  = cpu_pullByte();
            break;
        }
        case 0x41: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIdx(&low);
            cpu_eor(low, high);
            break;
        }
        case 0x42: {
            cpu_readOpcode();
            break;
        }
        case 0x43: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrSr(&low);
            cpu_eor(low, high);
            break;
        }
        case 0x44: {
            uint8_t dest = cpu_readOpcode();
            uint8_t src  = cpu_readOpcode();
            db           = dest;
            cpu_write((dest << 16) | y, cpu_read((src << 16) | x));
            a--;
            x--;
            y--;
            if (a != 0xffff) {
                pc -= 3;
            }
            if (xf) {
                x &= 0xff;
                y &= 0xff;
            }
            break;
        }
        case 0x45: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDp(&low);
            cpu_eor(low, high);
            break;
        }
        case 0x46: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDp(&low);
            cpu_lsr(low, high);
            break;
        }
        case 0x47: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIdl(&low);
            cpu_eor(low, high);
            break;
        }
        case 0x48: {
            if (mf) {
                cpu_pushByte(a);
            } else {
                cyclesUsed++;
                cpu_pushWord(a);
            }
            break;
        }
        case 0x49: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrImm(&low, false);
            cpu_eor(low, high);
            break;
        }
        case 0x4a: {
            c = a & 1;
            if (mf) {
                a = (a & 0xff00) | ((a >> 1) & 0x7f);
            } else {
                a >>= 1;
            }
            cpu_setZN(a, mf);
            break;
        }
        case 0x4b: {
            cpu_pushByte(k);
            break;
        }
        case 0x4c: {
            pc = cpu_readOpcodeWord();
            break;
        }
        case 0x4d: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbs(&low);
            cpu_eor(low, high);
            break;
        }
        case 0x4e: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbs(&low);
            cpu_lsr(low, high);
            break;
        }
        case 0x4f: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbl(&low);
            cpu_eor(low, high);
            break;
        }
        case 0x50: {
            cpu_doBranch(cpu_readOpcode(), !v);
            break;
        }
        case 0x51: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIdy(&low, false);
            cpu_eor(low, high);
            break;
        }
        case 0x52: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIdp(&low);
            cpu_eor(low, high);
            break;
        }
        case 0x53: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIsy(&low);
            cpu_eor(low, high);
            break;
        }
        case 0x54: {
            uint8_t dest = cpu_readOpcode();
            uint8_t src  = cpu_readOpcode();
            db           = dest;
            cpu_write((dest << 16) | y, cpu_read((src << 16) | x));
            a--;
            x++;
            y++;
            if (a != 0xffff) {
                pc -= 3;
            }
            if (xf) {
                x &= 0xff;
                y &= 0xff;
            }
            break;
        }
        case 0x55: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDpx(&low);
            cpu_eor(low, high);
            break;
        }
        case 0x56: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDpx(&low);
            cpu_lsr(low, high);
            break;
        }
        case 0x57: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIly(&low);
            cpu_eor(low, high);
            break;
        }
        case 0x58: {
            i = false;
            break;
        }
        case 0x59: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAby(&low, false);
            cpu_eor(low, high);
            break;
        }
        case 0x5a: {
            if (xf) {
                cpu_pushByte(y);
            } else {
                cyclesUsed++;
                cpu_pushWord(y);
            }
            break;
        }
        case 0x5b: {
            dp = a;
            cpu_setZN(dp, false);
            break;
        }
        case 0x5c: {
            uint16_t value = cpu_readOpcodeWord();
            k              = cpu_readOpcode();
            pc             = value;
            break;
        }
        case 0x5d: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbx(&low, false);
            cpu_eor(low, high);
            break;
        }
        case 0x5e: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbx(&low, true);
            cpu_lsr(low, high);
            break;
        }
        case 0x5f: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAlx(&low);
            cpu_eor(low, high);
            break;
        }
        case 0x60: {
            pc = cpu_pullWord() + 1;
            break;
        }
        case 0x61: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIdx(&low);
            cpu_adc(low, high);
            break;
        }
        case 0x62: {
            uint16_t value = cpu_readOpcodeWord();
            cpu_pushWord(pc + (int16_t)value);
            break;
        }
        case 0x63: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrSr(&low);
            cpu_adc(low, high);
            break;
        }
        case 0x64: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDp(&low);
            cpu_stz(low, high);
            break;
        }
        case 0x65: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDp(&low);
            cpu_adc(low, high);
            break;
        }
        case 0x66: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDp(&low);
            cpu_ror(low, high);
            break;
        }
        case 0x67: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIdl(&low);
            cpu_adc(low, high);
            break;
        }
        case 0x68: {
            if (mf) {
                a = (a & 0xff00) | cpu_pullByte();
            } else {
                cyclesUsed++;
                a = cpu_pullWord();
            }
            cpu_setZN(a, mf);
            break;
        }
        case 0x69: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrImm(&low, false);
            cpu_adc(low, high);
            break;
        }
        case 0x6a: {
            bool carry = a & 1;
            if (mf) {
                a = (a & 0xff00) | ((a >> 1) & 0x7f) | (c << 7);
            } else {
                a = (a >> 1) | (c << 15);
            }
            c = carry;
            cpu_setZN(a, mf);
            break;
        }
        case 0x6b: {
            pc = cpu_pullWord() + 1;
            k  = cpu_pullByte();
            break;
        }
        case 0x6c: {
            uint16_t adr = cpu_readOpcodeWord();
            pc           = cpu_readWord(adr, (adr + 1) & 0xffff);
            break;
        }
        case 0x6d: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbs(&low);
            cpu_adc(low, high);
            break;
        }
        case 0x6e: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbs(&low);
            cpu_ror(low, high);
            break;
        }
        case 0x6f: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbl(&low);
            cpu_adc(low, high);
            break;
        }
        case 0x70: {
            cpu_doBranch(cpu_readOpcode(), v);
            break;
        }
        case 0x71: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIdy(&low, false);
            cpu_adc(low, high);
            break;
        }
        case 0x72: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIdp(&low);
            cpu_adc(low, high);
            break;
        }
        case 0x73: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIsy(&low);
            cpu_adc(low, high);
            break;
        }
        case 0x74: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDpx(&low);
            cpu_stz(low, high);
            break;
        }
        case 0x75: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDpx(&low);
            cpu_adc(low, high);
            break;
        }
        case 0x76: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDpx(&low);
            cpu_ror(low, high);
            break;
        }
        case 0x77: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIly(&low);
            cpu_adc(low, high);
            break;
        }
        case 0x78: {
            i = true;
            break;
        }
        case 0x79: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAby(&low, false);
            cpu_adc(low, high);
            break;
        }
        case 0x7a: {
            if (xf) {
                y = cpu_pullByte();
            } else {
                cyclesUsed++;
                y = cpu_pullWord();
            }
            cpu_setZN(y, xf);
            break;
        }
        case 0x7b: {
            a = dp;
            cpu_setZN(a, false);
            break;
        }
        case 0x7c: {
            pc = cpu_adrIax();
            break;
        }
        case 0x7d: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbx(&low, false);
            cpu_adc(low, high);
            break;
        }
        case 0x7e: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbx(&low, true);
            cpu_ror(low, high);
            break;
        }
        case 0x7f: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAlx(&low);
            cpu_adc(low, high);
            break;
        }
        case 0x80: {
            pc += (int8_t)cpu_readOpcode();
            break;
        }
        case 0x81: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIdx(&low);
            cpu_sta(low, high);
            break;
        }
        case 0x82: {
            pc += (int16_t)cpu_readOpcodeWord();
            break;
        }
        case 0x83: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrSr(&low);
            cpu_sta(low, high);
            break;
        }
        case 0x84: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDp(&low);
            cpu_sty(low, high);
            break;
        }
        case 0x85: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDp(&low);
            cpu_sta(low, high);
            break;
        }
        case 0x86: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDp(&low);
            cpu_stx(low, high);
            break;
        }
        case 0x87: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIdl(&low);
            cpu_sta(low, high);
            break;
        }
        case 0x88: {
            if (xf) {
                y = (y - 1) & 0xff;
            } else {
                y--;
            }
            cpu_setZN(y, xf);
            break;
        }
        case 0x89: {
            if (mf) {
                uint8_t result = (a & 0xff) & cpu_readOpcode();
                z              = result == 0;
            } else {
                cyclesUsed++;
                uint16_t result = a & cpu_readOpcodeWord();
                z               = result == 0;
            }
            break;
        }
        case 0x8a: {
            if (mf) {
                a = (a & 0xff00) | (x & 0xff);
            } else {
                a = x;
            }
            cpu_setZN(a, mf);
            break;
        }
        case 0x8b: {
            cpu_pushByte(db);
            break;
        }
        case 0x8c: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbs(&low);
            cpu_sty(low, high);
            break;
        }
        case 0x8d: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbs(&low);
            cpu_sta(low, high);
            break;
        }
        case 0x8e: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbs(&low);
            cpu_stx(low, high);
            break;
        }
        case 0x8f: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbl(&low);
            cpu_sta(low, high);
            break;
        }
        case 0x90: {
            cpu_doBranch(cpu_readOpcode(), !c);
            break;
        }
        case 0x91: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIdy(&low, true);
            cpu_sta(low, high);
            break;
        }
        case 0x92: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIdp(&low);
            cpu_sta(low, high);
            break;
        }
        case 0x93: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIsy(&low);
            cpu_sta(low, high);
            break;
        }
        case 0x94: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDpx(&low);
            cpu_sty(low, high);
            break;
        }
        case 0x95: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDpx(&low);
            cpu_sta(low, high);
            break;
        }
        case 0x96: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDpy(&low);
            cpu_stx(low, high);
            break;
        }
        case 0x97: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIly(&low);
            cpu_sta(low, high);
            break;
        }
        case 0x98: {
            if (mf) {
                a = (a & 0xff00) | (y & 0xff);
            } else {
                a = y;
            }
            cpu_setZN(a, mf);
            break;
        }
        case 0x99: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAby(&low, true);
            cpu_sta(low, high);
            break;
        }
        case 0x9a: {
            sp = x;
            break;
        }
        case 0x9b: {
            if (xf) {
                y = x & 0xff;
            } else {
                y = x;
            }
            cpu_setZN(y, xf);
            break;
        }
        case 0x9c: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbs(&low);
            cpu_stz(low, high);
            break;
        }
        case 0x9d: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbx(&low, true);
            cpu_sta(low, high);
            break;
        }
        case 0x9e: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbx(&low, true);
            cpu_stz(low, high);
            break;
        }
        case 0x9f: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAlx(&low);
            cpu_sta(low, high);
            break;
        }
        case 0xa0: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrImm(&low, true);
            cpu_ldy(low, high);
            break;
        }
        case 0xa1: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIdx(&low);
            cpu_lda(low, high);
            break;
        }
        case 0xa2: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrImm(&low, true);
            cpu_ldx(low, high);
            break;
        }
        case 0xa3: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrSr(&low);
            cpu_lda(low, high);
            break;
        }
        case 0xa4: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDp(&low);
            cpu_ldy(low, high);
            break;
        }
        case 0xa5: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDp(&low);
            cpu_lda(low, high);
            break;
        }
        case 0xa6: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDp(&low);
            cpu_ldx(low, high);
            break;
        }
        case 0xa7: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIdl(&low);
            cpu_lda(low, high);
            break;
        }
        case 0xa8: {
            if (xf) {
                y = a & 0xff;
            } else {
                y = a;
            }
            cpu_setZN(y, xf);
            break;
        }
        case 0xa9: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrImm(&low, false);
            cpu_lda(low, high);
            break;
        }
        case 0xaa: {
            if (xf) {
                x = a & 0xff;
            } else {
                x = a;
            }
            cpu_setZN(x, xf);
            break;
        }
        case 0xab: {
            db = cpu_pullByte();
            cpu_setZN(db, true);
            break;
        }
        case 0xac: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbs(&low);
            cpu_ldy(low, high);
            break;
        }
        case 0xad: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbs(&low);
            cpu_lda(low, high);
            break;
        }
        case 0xae: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbs(&low);
            cpu_ldx(low, high);
            break;
        }
        case 0xaf: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbl(&low);
            cpu_lda(low, high);
            break;
        }
        case 0xb0: {
            cpu_doBranch(cpu_readOpcode(), c);
            break;
        }
        case 0xb1: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIdy(&low, false);
            cpu_lda(low, high);
            break;
        }
        case 0xb2: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIdp(&low);
            cpu_lda(low, high);
            break;
        }
        case 0xb3: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIsy(&low);
            cpu_lda(low, high);
            break;
        }
        case 0xb4: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDpx(&low);
            cpu_ldy(low, high);
            break;
        }
        case 0xb5: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDpx(&low);
            cpu_lda(low, high);
            break;
        }
        case 0xb6: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDpy(&low);
            cpu_ldx(low, high);
            break;
        }
        case 0xb7: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIly(&low);
            cpu_lda(low, high);
            break;
        }
        case 0xb8: {
            v = false;
            break;
        }
        case 0xb9: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAby(&low, false);
            cpu_lda(low, high);
            break;
        }
        case 0xba: {
            if (xf) {
                x = sp & 0xff;
            } else {
                x = sp;
            }
            cpu_setZN(x, xf);
            break;
        }
        case 0xbb: {
            if (xf) {
                x = y & 0xff;
            } else {
                x = y;
            }
            cpu_setZN(x, xf);
            break;
        }
        case 0xbc: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbx(&low, false);
            cpu_ldy(low, high);
            break;
        }
        case 0xbd: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbx(&low, false);
            cpu_lda(low, high);
            break;
        }
        case 0xbe: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAby(&low, false);
            cpu_ldx(low, high);
            break;
        }
        case 0xbf: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAlx(&low);
            cpu_lda(low, high);
            break;
        }
        case 0xc0: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrImm(&low, true);
            cpu_cpy(low, high);
            break;
        }
        case 0xc1: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIdx(&low);
            cpu_cmp(low, high);
            break;
        }
        case 0xc2: {
            cpu_setFlags(cpu_getFlags() & ~cpu_readOpcode());
            break;
        }
        case 0xc3: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrSr(&low);
            cpu_cmp(low, high);
            break;
        }
        case 0xc4: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDp(&low);
            cpu_cpy(low, high);
            break;
        }
        case 0xc5: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDp(&low);
            cpu_cmp(low, high);
            break;
        }
        case 0xc6: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDp(&low);
            cpu_dec(low, high);
            break;
        }
        case 0xc7: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIdl(&low);
            cpu_cmp(low, high);
            break;
        }
        case 0xc8: {
            if (xf) {
                y = (y + 1) & 0xff;
            } else {
                y++;
            }
            cpu_setZN(y, xf);
            break;
        }
        case 0xc9: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrImm(&low, false);
            cpu_cmp(low, high);
            break;
        }
        case 0xca: {
            if (xf) {
                x = (x - 1) & 0xff;
            } else {
                x--;
            }
            cpu_setZN(x, xf);
            break;
        }
        case 0xcb: {
            waiting = true;
            break;
        }
        case 0xcc: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbs(&low);
            cpu_cpy(low, high);
            break;
        }
        case 0xcd: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbs(&low);
            cpu_cmp(low, high);
            break;
        }
        case 0xce: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbs(&low);
            cpu_dec(low, high);
            break;
        }
        case 0xcf: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbl(&low);
            cpu_cmp(low, high);
            break;
        }
        case 0xd0: {
            cpu_doBranch(cpu_readOpcode(), !z);
            break;
        }
        case 0xd1: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIdy(&low, false);
            cpu_cmp(low, high);
            break;
        }
        case 0xd2: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIdp(&low);
            cpu_cmp(low, high);
            break;
        }
        case 0xd3: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIsy(&low);
            cpu_cmp(low, high);
            break;
        }
        case 0xd4: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDp(&low);
            cpu_pushWord(cpu_readWord(low, high));
            break;
        }
        case 0xd5: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDpx(&low);
            cpu_cmp(low, high);
            break;
        }
        case 0xd6: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDpx(&low);
            cpu_dec(low, high);
            break;
        }
        case 0xd7: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIly(&low);
            cpu_cmp(low, high);
            break;
        }
        case 0xd8: {
            d = false;
            break;
        }
        case 0xd9: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAby(&low, false);
            cpu_cmp(low, high);
            break;
        }
        case 0xda: {
            if (xf) {
                cpu_pushByte(x);
            } else {
                cyclesUsed++;
                cpu_pushWord(x);
            }
            break;
        }
        case 0xdb: {
            stopped = true;
            break;
        }
        case 0xdc: {
            uint16_t adr = cpu_readOpcodeWord();
            pc           = cpu_readWord(adr, (adr + 1) & 0xffff);
            k            = cpu_read((adr + 2) & 0xffff);
            break;
        }
        case 0xdd: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbx(&low, false);
            cpu_cmp(low, high);
            break;
        }
        case 0xde: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbx(&low, true);
            cpu_dec(low, high);
            break;
        }
        case 0xdf: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAlx(&low);
            cpu_cmp(low, high);
            break;
        }
        case 0xe0: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrImm(&low, true);
            cpu_cpx(low, high);
            break;
        }
        case 0xe1: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIdx(&low);
            cpu_sbc(low, high);
            break;
        }
        case 0xe2: {
            cpu_setFlags(cpu_getFlags() | cpu_readOpcode());
            break;
        }
        case 0xe3: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrSr(&low);
            cpu_sbc(low, high);
            break;
        }
        case 0xe4: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDp(&low);
            cpu_cpx(low, high);
            break;
        }
        case 0xe5: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDp(&low);
            cpu_sbc(low, high);
            break;
        }
        case 0xe6: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDp(&low);
            cpu_inc(low, high);
            break;
        }
        case 0xe7: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIdl(&low);
            cpu_sbc(low, high);
            break;
        }
        case 0xe8: {
            if (xf) {
                x = (x + 1) & 0xff;
            } else {
                x++;
            }
            cpu_setZN(x, xf);
            break;
        }
        case 0xe9: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrImm(&low, false);
            cpu_sbc(low, high);
            break;
        }
        case 0xea: {
            break;
        }
        case 0xeb: {
            uint8_t low  = a & 0xff;
            uint8_t high = a >> 8;
            a            = (low << 8) | high;
            cpu_setZN(high, true);
            break;
        }
        case 0xec: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbs(&low);
            cpu_cpx(low, high);
            break;
        }
        case 0xed: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbs(&low);
            cpu_sbc(low, high);
            break;
        }
        case 0xee: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbs(&low);
            cpu_inc(low, high);
            break;
        }
        case 0xef: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbl(&low);
            cpu_sbc(low, high);
            break;
        }
        case 0xf0: {
            cpu_doBranch(cpu_readOpcode(), z);
            break;
        }
        case 0xf1: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIdy(&low, false);
            cpu_sbc(low, high);
            break;
        }
        case 0xf2: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIdp(&low);
            cpu_sbc(low, high);
            break;
        }
        case 0xf3: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIsy(&low);
            cpu_sbc(low, high);
            break;
        }
        case 0xf4: {
            cpu_pushWord(cpu_readOpcodeWord());
            break;
        }
        case 0xf5: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDpx(&low);
            cpu_sbc(low, high);
            break;
        }
        case 0xf6: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrDpx(&low);
            cpu_inc(low, high);
            break;
        }
        case 0xf7: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrIly(&low);
            cpu_sbc(low, high);
            break;
        }
        case 0xf8: {
            d = true;
            break;
        }
        case 0xf9: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAby(&low, false);
            cpu_sbc(low, high);
            break;
        }
        case 0xfa: {
            if (xf) {
                x = cpu_pullByte();
            } else {
                cyclesUsed++;
                x = cpu_pullWord();
            }
            cpu_setZN(x, xf);
            break;
        }
        case 0xfb: {
            bool temp = c;
            c         = e;
            e         = temp;
            cpu_setFlags(cpu_getFlags());
            break;
        }
        case 0xfc: {
            uint16_t value = cpu_adrIax();
            cpu_pushWord(pc - 1);
            pc = value;
            break;
        }
        case 0xfd: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbx(&low, false);
            cpu_sbc(low, high);
            break;
        }
        case 0xfe: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAbx(&low, true);
            cpu_inc(low, high);
            break;
        }
        case 0xff: {
            uint32_t low  = 0;
            uint32_t high = cpu_adrAlx(&low);
            cpu_sbc(low, high);
            break;
        }
        default: {
            printf("cpu op error : %X", opcode);
            exit(1);
        }
    }
}
