
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "spc.h"
#include "apu.h"

SPC::SPC(Apu *_apu)
{
    apu = _apu;
}
uint8_t SPC::spc_read(uint16_t adr)
{
    return apu->apu_cpuRead(adr);
}
void SPC::spc_write(uint16_t adr, uint8_t val)
{
    apu->apu_cpuWrite(adr, val);
}
void SPC::spc_reset()
{
    a  = 0;
    x  = 0;
    y  = 0;
    sp = 0;
    pc = spc_read(0xfffe) | (spc_read(0xffff) << 8);

    c = false;
    z = false;
    v = false;
    n = false;
    i = false;
    h = false;
    p = false;
    b = false;

    stopped    = false;
    cyclesUsed = 0;
}
int SPC::spc_runOpcode()
{
    cyclesUsed = 0;
    if (stopped)
        return 1;
    uint8_t opcode = spc_readOpcode();
    cyclesUsed     = cyclesPerOpcode[opcode];
    spc_doOpcode(opcode);
    return cyclesUsed;
}
uint8_t SPC::spc_readOpcode()
{
    return spc_read(pc++);
}
uint16_t SPC::spc_readOpcodeWord()
{
    uint8_t low = spc_readOpcode();
    return low | (spc_readOpcode() << 8);
}
uint8_t SPC::spc_getFlags()
{
    uint8_t val = n << 7;
    val |= v << 6;
    val |= p << 5;
    val |= b << 4;
    val |= h << 3;
    val |= i << 2;
    val |= z << 1;
    val |= c;
    return val;
}
void SPC::spc_setFlags(uint8_t val)
{
    n = val & 0x80;
    v = val & 0x40;
    p = val & 0x20;
    b = val & 0x10;
    h = val & 8;
    i = val & 4;
    z = val & 2;
    c = val & 1;
}
void SPC::spc_setZN(uint8_t value)
{
    z = value == 0;
    n = value & 0x80;
}
void SPC::spc_doBranch(uint8_t value, bool check)
{
    if (check) {
        cyclesUsed += 2;
        pc += (int8_t)value;
    }
}
uint8_t SPC::spc_pullByte()
{
    sp++;
    return spc_read(0x100 | sp);
}
void SPC::spc_pushByte(uint8_t value)
{
    spc_write(0x100 | sp, value);
    sp--;
}
uint16_t SPC::spc_pullWord()
{
    uint8_t value = spc_pullByte();
    return value | (spc_pullByte() << 8);
}
void SPC::spc_pushWord(uint16_t value)
{
    spc_pushByte(value >> 8);
    spc_pushByte(value & 0xff);
}
uint16_t SPC::spc_readWord(uint16_t adrl, uint16_t adrh)
{
    uint8_t value = spc_read(adrl);
    return value | (spc_read(adrh) << 8);
}
void SPC::spc_writeWord(uint16_t adrl, uint16_t adrh, uint16_t value)
{
    spc_write(adrl, value & 0xff);
    spc_write(adrh, value >> 8);
}
// adressing modes
uint16_t SPC::spc_adrDp()
{
    return spc_readOpcode() | (p << 8);
}
uint16_t SPC::spc_adrAbs()
{
    return spc_readOpcodeWord();
}
uint16_t SPC::spc_adrInd()
{
    return x | (p << 8);
}
uint16_t SPC::spc_adrIdx()
{
    uint8_t pointer = spc_readOpcode();
    return spc_readWord(((pointer + x) & 0xff) | (p << 8), ((pointer + x + 1) & 0xff) | (p << 8));
}
uint16_t SPC::spc_adrImm()
{
    return pc++;
}
uint16_t SPC::spc_adrDpx()
{
    return ((spc_readOpcode() + x) & 0xff) | (p << 8);
}
uint16_t SPC::spc_adrDpy()
{
    return ((spc_readOpcode() + y) & 0xff) | (p << 8);
}
uint16_t SPC::spc_adrAbx()
{
    return (spc_readOpcodeWord() + x) & 0xffff;
}
uint16_t SPC::spc_adrAby()
{
    return (spc_readOpcodeWord() + y) & 0xffff;
}
uint16_t SPC::spc_adrIdy()
{
    uint8_t  pointer = spc_readOpcode();
    uint16_t adr     = spc_readWord(pointer | (p << 8), ((pointer + 1) & 0xff) | (p << 8));
    return (adr + y) & 0xffff;
}
uint16_t SPC::spc_adrDpDp(uint16_t *src)
{
    *src = spc_readOpcode() | (p << 8);
    return spc_readOpcode() | (p << 8);
}
uint16_t SPC::spc_adrDpImm(uint16_t *src)
{
    *src = pc++;
    return spc_readOpcode() | (p << 8);
}
uint16_t SPC::spc_adrIndInd(uint16_t *src)
{
    *src = y | (p << 8);
    return x | (p << 8);
}
uint8_t SPC::spc_adrAbsBit(uint16_t *adr)
{
    uint16_t adrBit = spc_readOpcodeWord();
    *adr            = adrBit & 0x1fff;
    return adrBit >> 13;
}
uint16_t SPC::spc_adrDpWord(uint16_t *low)
{
    uint8_t adr = spc_readOpcode();
    *low        = adr | (p << 8);
    return ((adr + 1) & 0xff) | (p << 8);
}
uint16_t SPC::spc_adrIndP()
{
    return x++ | (p << 8);
}
// opcode functions
void SPC::spc_and(uint16_t adr)
{
    a &= spc_read(adr);
    spc_setZN(a);
}
void SPC::spc_andm(uint16_t dst, uint16_t src)
{
    uint8_t value  = spc_read(src);
    uint8_t result = spc_read(dst) & value;
    spc_write(dst, result);
    spc_setZN(result);
}
void SPC::spc_or(uint16_t adr)
{
    a |= spc_read(adr);
    spc_setZN(a);
}
void SPC::spc_orm(uint16_t dst, uint16_t src)
{
    uint8_t value  = spc_read(src);
    uint8_t result = spc_read(dst) | value;
    spc_write(dst, result);
    spc_setZN(result);
}
void SPC::spc_eor(uint16_t adr)
{
    a ^= spc_read(adr);
    spc_setZN(a);
}
void SPC::spc_eorm(uint16_t dst, uint16_t src)
{
    uint8_t value  = spc_read(src);
    uint8_t result = spc_read(dst) ^ value;
    spc_write(dst, result);
    spc_setZN(result);
}
void SPC::spc_adc(uint16_t adr)
{
    uint8_t value  = spc_read(adr);
    int     result = a + value + c;
    v              = (a & 0x80) == (value & 0x80) && (value & 0x80) != (result & 0x80);
    h              = ((a & 0xf) + (value & 0xf) + c) > 0xf;
    c              = result > 0xff;
    a              = result;
    spc_setZN(a);
}
void SPC::spc_adcm(uint16_t dst, uint16_t src)
{
    uint8_t value   = spc_read(src);
    uint8_t applyOn = spc_read(dst);
    int     result  = applyOn + value + c;
    v               = (applyOn & 0x80) == (value & 0x80) && (value & 0x80) != (result & 0x80);
    h               = ((applyOn & 0xf) + (value & 0xf) + c) > 0xf;
    c               = result > 0xff;
    spc_write(dst, result);
    spc_setZN(result);
}
void SPC::spc_sbc(uint16_t adr)
{
    uint8_t value  = spc_read(adr) ^ 0xff;
    int     result = a + value + c;
    v              = (a & 0x80) == (value & 0x80) && (value & 0x80) != (result & 0x80);
    h              = ((a & 0xf) + (value & 0xf) + c) > 0xf;
    c              = result > 0xff;
    a              = result;
    spc_setZN(a);
}
void SPC::spc_sbcm(uint16_t dst, uint16_t src)
{
    uint8_t value   = spc_read(src) ^ 0xff;
    uint8_t applyOn = spc_read(dst);
    int     result  = applyOn + value + c;
    v               = (applyOn & 0x80) == (value & 0x80) && (value & 0x80) != (result & 0x80);
    h               = ((applyOn & 0xf) + (value & 0xf) + c) > 0xf;
    c               = result > 0xff;
    spc_write(dst, result);
    spc_setZN(result);
}
void SPC::spc_cmp(uint16_t adr)
{
    uint8_t value  = spc_read(adr) ^ 0xff;
    int     result = a + value + 1;
    c              = result > 0xff;
    spc_setZN(result);
}
void SPC::spc_cmpx(uint16_t adr)
{
    uint8_t value  = spc_read(adr) ^ 0xff;
    int     result = x + value + 1;
    c              = result > 0xff;
    spc_setZN(result);
}
void SPC::spc_cmpy(uint16_t adr)
{
    uint8_t value  = spc_read(adr) ^ 0xff;
    int     result = y + value + 1;
    c              = result > 0xff;
    spc_setZN(result);
}
void SPC::spc_cmpm(uint16_t dst, uint16_t src)
{
    uint8_t value  = spc_read(src) ^ 0xff;
    int     result = spc_read(dst) + value + 1;
    c              = result > 0xff;
    spc_setZN(result);
}
void SPC::spc_mov(uint16_t adr)
{
    a = spc_read(adr);
    spc_setZN(a);
}
void SPC::spc_movx(uint16_t adr)
{
    x = spc_read(adr);
    spc_setZN(x);
}
void SPC::spc_movy(uint16_t adr)
{
    y = spc_read(adr);
    spc_setZN(y);
}
void SPC::spc_movs(uint16_t adr)
{
    spc_read(adr);
    spc_write(adr, a);
}
void SPC::spc_movsx(uint16_t adr)
{
    spc_read(adr);
    spc_write(adr, x);
}
void SPC::spc_movsy(uint16_t adr)
{
    spc_read(adr);
    spc_write(adr, y);
}
void SPC::spc_asl(uint16_t adr)
{
    uint8_t val = spc_read(adr);
    c           = val & 0x80;
    val <<= 1;
    spc_write(adr, val);
    spc_setZN(val);
}
void SPC::spc_lsr(uint16_t adr)
{
    uint8_t val = spc_read(adr);
    c           = val & 1;
    val >>= 1;
    spc_write(adr, val);
    spc_setZN(val);
}
void SPC::spc_rol(uint16_t adr)
{
    uint8_t val  = spc_read(adr);
    bool    newC = val & 0x80;
    val          = (val << 1) | c;
    c            = newC;
    spc_write(adr, val);
    spc_setZN(val);
}
void SPC::spc_ror(uint16_t adr)
{
    uint8_t val  = spc_read(adr);
    bool    newC = val & 1;
    val          = (val >> 1) | (c << 7);
    c            = newC;
    spc_write(adr, val);
    spc_setZN(val);
}
void SPC::spc_inc(uint16_t adr)
{
    uint8_t val = spc_read(adr) + 1;
    spc_write(adr, val);
    spc_setZN(val);
}
void SPC::spc_dec(uint16_t adr)
{
    uint8_t val = spc_read(adr) - 1;
    spc_write(adr, val);
    spc_setZN(val);
}
void SPC::spc_doOpcode(uint8_t opcode)
{
    switch (opcode) {
        case 0x00: {    // nop imp
            // no operation
            break;
        }
        case 0x01:
        case 0x11:
        case 0x21:
        case 0x31:
        case 0x41:
        case 0x51:
        case 0x61:
        case 0x71:
        case 0x81:
        case 0x91:
        case 0xa1:
        case 0xb1:
        case 0xc1:
        case 0xd1:
        case 0xe1:
        case 0xf1: {    // tcall imp
            spc_pushWord(pc);
            uint16_t adr = 0xffde - (2 * (opcode >> 4));
            pc           = spc_readWord(adr, adr + 1);
            break;
        }
        case 0x02:
        case 0x22:
        case 0x42:
        case 0x62:
        case 0x82:
        case 0xa2:
        case 0xc2:
        case 0xe2: {    // set1 dp
            uint16_t adr = spc_adrDp();
            spc_write(adr, spc_read(adr) | (1 << (opcode >> 5)));
            break;
        }
        case 0x12:
        case 0x32:
        case 0x52:
        case 0x72:
        case 0x92:
        case 0xb2:
        case 0xd2:
        case 0xf2: {    // clr1 dp
            uint16_t adr = spc_adrDp();
            spc_write(adr, spc_read(adr) & ~(1 << (opcode >> 5)));
            break;
        }
        case 0x03:
        case 0x23:
        case 0x43:
        case 0x63:
        case 0x83:
        case 0xa3:
        case 0xc3:
        case 0xe3: {    // bbs dp, rel
            uint8_t val = spc_read(spc_adrDp());
            spc_doBranch(spc_readOpcode(), val & (1 << (opcode >> 5)));
            break;
        }
        case 0x13:
        case 0x33:
        case 0x53:
        case 0x73:
        case 0x93:
        case 0xb3:
        case 0xd3:
        case 0xf3: {    // bbc dp, rel
            uint8_t val = spc_read(spc_adrDp());
            spc_doBranch(spc_readOpcode(), (val & (1 << (opcode >> 5))) == 0);
            break;
        }
        case 0x04: {    // or  dp
            spc_or(spc_adrDp());
            break;
        }
        case 0x05: {    // or  abs
            spc_or(spc_adrAbs());
            break;
        }
        case 0x06: {    // or  ind
            spc_or(spc_adrInd());
            break;
        }
        case 0x07: {    // or  idx
            spc_or(spc_adrIdx());
            break;
        }
        case 0x08: {    // or  imm
            spc_or(spc_adrImm());
            break;
        }
        case 0x09: {    // orm dp, dp
            uint16_t src = 0;
            uint16_t dst = spc_adrDpDp(&src);
            spc_orm(dst, src);
            break;
        }
        case 0x0a: {    // or1 abs.bit
            uint16_t adr = 0;
            uint8_t  bit = spc_adrAbsBit(&adr);
            c            = c | ((spc_read(adr) >> bit) & 1);
            break;
        }
        case 0x0b: {    // asl dp
            spc_asl(spc_adrDp());
            break;
        }
        case 0x0c: {    // asl abs
            spc_asl(spc_adrAbs());
            break;
        }
        case 0x0d: {    // pushp imp
            spc_pushByte(spc_getFlags());
            break;
        }
        case 0x0e: {    // tset1 abs
            uint16_t adr    = spc_adrAbs();
            uint8_t  val    = spc_read(adr);
            uint8_t  result = a + (val ^ 0xff) + 1;
            spc_setZN(result);
            spc_write(adr, val | a);
            break;
        }
        case 0x0f: {    // brk imp
            spc_pushWord(pc);
            spc_pushByte(spc_getFlags());
            i  = false;
            b  = true;
            pc = spc_readWord(0xffde, 0xffdf);
            break;
        }
        case 0x10: {    // bpl rel
            spc_doBranch(spc_readOpcode(), !n);
            break;
        }
        case 0x14: {    // or  dpx
            spc_or(spc_adrDpx());
            break;
        }
        case 0x15: {    // or  abx
            spc_or(spc_adrAbx());
            break;
        }
        case 0x16: {    // or  aby
            spc_or(spc_adrAby());
            break;
        }
        case 0x17: {    // or  idy
            spc_or(spc_adrIdy());
            break;
        }
        case 0x18: {    // orm dp, imm
            uint16_t src = 0;
            uint16_t dst = spc_adrDpImm(&src);
            spc_orm(dst, src);
            break;
        }
        case 0x19: {    // orm ind, ind
            uint16_t src = 0;
            uint16_t dst = spc_adrIndInd(&src);
            spc_orm(dst, src);
            break;
        }
        case 0x1a: {    // decw dp
            uint16_t low   = 0;
            uint16_t high  = spc_adrDpWord(&low);
            uint16_t value = spc_readWord(low, high) - 1;
            z              = value == 0;
            n              = value & 0x8000;
            spc_writeWord(low, high, value);
            break;
        }
        case 0x1b: {    // asl dpx
            spc_asl(spc_adrDpx());
            break;
        }
        case 0x1c: {    // asla imp
            c = a & 0x80;
            a <<= 1;
            spc_setZN(a);
            break;
        }
        case 0x1d: {    // decx imp
            x--;
            spc_setZN(x);
            break;
        }
        case 0x1e: {    // cmpx abs
            spc_cmpx(spc_adrAbs());
            break;
        }
        case 0x1f: {    // jmp iax
            uint16_t pointer = spc_readOpcodeWord();
            pc               = spc_readWord((pointer + x) & 0xffff, (pointer + x + 1) & 0xffff);
            break;
        }
        case 0x20: {    // clrp imp
            p = false;
            break;
        }
        case 0x24: {    // and dp
            spc_and(spc_adrDp());
            break;
        }
        case 0x25: {    // and abs
            spc_and(spc_adrAbs());
            break;
        }
        case 0x26: {    // and ind
            spc_and(spc_adrInd());
            break;
        }
        case 0x27: {    // and idx
            spc_and(spc_adrIdx());
            break;
        }
        case 0x28: {    // and imm
            spc_and(spc_adrImm());
            break;
        }
        case 0x29: {    // andm dp, dp
            uint16_t src = 0;
            uint16_t dst = spc_adrDpDp(&src);
            spc_andm(dst, src);
            break;
        }
        case 0x2a: {    // or1n abs.bit
            uint16_t adr = 0;
            uint8_t  bit = spc_adrAbsBit(&adr);
            c            = c | (~(spc_read(adr) >> bit) & 1);
            break;
        }
        case 0x2b: {    // rol dp
            spc_rol(spc_adrDp());
            break;
        }
        case 0x2c: {    // rol abs
            spc_rol(spc_adrAbs());
            break;
        }
        case 0x2d: {    // pusha imp
            spc_pushByte(a);
            break;
        }
        case 0x2e: {    // cbne dp, rel
            uint8_t val    = spc_read(spc_adrDp()) ^ 0xff;
            uint8_t result = a + val + 1;
            spc_doBranch(spc_readOpcode(), result != 0);
            break;
        }
        case 0x2f: {    // bra rel
            pc += (int8_t)spc_readOpcode();
            break;
        }
        case 0x30: {    // bmi rel
            spc_doBranch(spc_readOpcode(), n);
            break;
        }
        case 0x34: {    // and dpx
            spc_and(spc_adrDpx());
            break;
        }
        case 0x35: {    // and abx
            spc_and(spc_adrAbx());
            break;
        }
        case 0x36: {    // and aby
            spc_and(spc_adrAby());
            break;
        }
        case 0x37: {    // and idy
            spc_and(spc_adrIdy());
            break;
        }
        case 0x38: {    // andm dp, imm
            uint16_t src = 0;
            uint16_t dst = spc_adrDpImm(&src);
            spc_andm(dst, src);
            break;
        }
        case 0x39: {    // andm ind, ind
            uint16_t src = 0;
            uint16_t dst = spc_adrIndInd(&src);
            spc_andm(dst, src);
            break;
        }
        case 0x3a: {    // incw dp
            uint16_t low   = 0;
            uint16_t high  = spc_adrDpWord(&low);
            uint16_t value = spc_readWord(low, high) + 1;
            z              = value == 0;
            n              = value & 0x8000;
            spc_writeWord(low, high, value);
            break;
        }
        case 0x3b: {    // rol dpx
            spc_rol(spc_adrDpx());
            break;
        }
        case 0x3c: {    // rola imp
            bool newC = a & 0x80;
            a         = (a << 1) | c;
            c         = newC;
            spc_setZN(a);
            break;
        }
        case 0x3d: {    // incx imp
            x++;
            spc_setZN(x);
            break;
        }
        case 0x3e: {    // cmpx dp
            spc_cmpx(spc_adrDp());
            break;
        }
        case 0x3f: {    // call abs
            uint16_t dst = spc_readOpcodeWord();
            spc_pushWord(pc);
            pc = dst;
            break;
        }
        case 0x40: {    // setp imp
            p = true;
            break;
        }
        case 0x44: {    // eor dp
            spc_eor(spc_adrDp());
            break;
        }
        case 0x45: {    // eor abs
            spc_eor(spc_adrAbs());
            break;
        }
        case 0x46: {    // eor ind
            spc_eor(spc_adrInd());
            break;
        }
        case 0x47: {    // eor idx
            spc_eor(spc_adrIdx());
            break;
        }
        case 0x48: {    // eor imm
            spc_eor(spc_adrImm());
            break;
        }
        case 0x49: {    // eorm dp, dp
            uint16_t src = 0;
            uint16_t dst = spc_adrDpDp(&src);
            spc_eorm(dst, src);
            break;
        }
        case 0x4a: {    // and1 abs.bit
            uint16_t adr = 0;
            uint8_t  bit = spc_adrAbsBit(&adr);
            c            = c & ((spc_read(adr) >> bit) & 1);
            break;
        }
        case 0x4b: {    // lsr dp
            spc_lsr(spc_adrDp());
            break;
        }
        case 0x4c: {    // lsr abs
            spc_lsr(spc_adrAbs());
            break;
        }
        case 0x4d: {    // pushx imp
            spc_pushByte(x);
            break;
        }
        case 0x4e: {    // tclr1 abs
            uint16_t adr    = spc_adrAbs();
            uint8_t  val    = spc_read(adr);
            uint8_t  result = a + (val ^ 0xff) + 1;
            spc_setZN(result);
            spc_write(adr, val & ~a);
            break;
        }
        case 0x4f: {    // pcall dp
            uint8_t dst = spc_readOpcode();
            spc_pushWord(pc);
            pc = 0xff00 | dst;
            break;
        }
        case 0x50: {    // bvc rel
            spc_doBranch(spc_readOpcode(), !v);
            break;
        }
        case 0x54: {    // eor dpx
            spc_eor(spc_adrDpx());
            break;
        }
        case 0x55: {    // eor abx
            spc_eor(spc_adrAbx());
            break;
        }
        case 0x56: {    // eor aby
            spc_eor(spc_adrAby());
            break;
        }
        case 0x57: {    // eor idy
            spc_eor(spc_adrIdy());
            break;
        }
        case 0x58: {    // eorm dp, imm
            uint16_t src = 0;
            uint16_t dst = spc_adrDpImm(&src);
            spc_eorm(dst, src);
            break;
        }
        case 0x59: {    // eorm ind, ind
            uint16_t src = 0;
            uint16_t dst = spc_adrIndInd(&src);
            spc_eorm(dst, src);
            break;
        }
        case 0x5a: {    // cmpw dp
            uint16_t low    = 0;
            uint16_t high   = spc_adrDpWord(&low);
            uint16_t value  = spc_readWord(low, high) ^ 0xffff;
            uint16_t ya     = a | (y << 8);
            int      result = ya + value + 1;
            c               = result > 0xffff;
            z               = (result & 0xffff) == 0;
            n               = result & 0x8000;
            break;
        }
        case 0x5b: {    // lsr dpx
            spc_lsr(spc_adrDpx());
            break;
        }
        case 0x5c: {    // lsra imp
            c = a & 1;
            a >>= 1;
            spc_setZN(a);
            break;
        }
        case 0x5d: {    // movxa imp
            x = a;
            spc_setZN(x);
            break;
        }
        case 0x5e: {    // cmpy abs
            spc_cmpy(spc_adrAbs());
            break;
        }
        case 0x5f: {    // jmp abs
            pc = spc_readOpcodeWord();
            break;
        }
        case 0x60: {    // clrc imp
            c = false;
            break;
        }
        case 0x64: {    // cmp dp
            spc_cmp(spc_adrDp());
            break;
        }
        case 0x65: {    // cmp abs
            spc_cmp(spc_adrAbs());
            break;
        }
        case 0x66: {    // cmp ind
            spc_cmp(spc_adrInd());
            break;
        }
        case 0x67: {    // cmp idx
            spc_cmp(spc_adrIdx());
            break;
        }
        case 0x68: {    // cmp imm
            spc_cmp(spc_adrImm());
            break;
        }
        case 0x69: {    // cmpm dp, dp
            uint16_t src = 0;
            uint16_t dst = spc_adrDpDp(&src);
            spc_cmpm(dst, src);
            break;
        }
        case 0x6a: {    // and1n abs.bit
            uint16_t adr = 0;
            uint8_t  bit = spc_adrAbsBit(&adr);
            c            = c & (~(spc_read(adr) >> bit) & 1);
            break;
        }
        case 0x6b: {    // ror dp
            spc_ror(spc_adrDp());
            break;
        }
        case 0x6c: {    // ror abs
            spc_ror(spc_adrAbs());
            break;
        }
        case 0x6d: {    // pushy imp
            spc_pushByte(y);
            break;
        }
        case 0x6e: {    // dbnz dp, rel
            uint16_t adr    = spc_adrDp();
            uint8_t  result = spc_read(adr) - 1;
            spc_write(adr, result);
            spc_doBranch(spc_readOpcode(), result != 0);
            break;
        }
        case 0x6f: {    // ret imp
            pc = spc_pullWord();
            break;
        }
        case 0x70: {    // bvs rel
            spc_doBranch(spc_readOpcode(), v);
            break;
        }
        case 0x74: {    // cmp dpx
            spc_cmp(spc_adrDpx());
            break;
        }
        case 0x75: {    // cmp abx
            spc_cmp(spc_adrAbx());
            break;
        }
        case 0x76: {    // cmp aby
            spc_cmp(spc_adrAby());
            break;
        }
        case 0x77: {    // cmp idy
            spc_cmp(spc_adrIdy());
            break;
        }
        case 0x78: {    // cmpm dp, imm
            uint16_t src = 0;
            uint16_t dst = spc_adrDpImm(&src);
            spc_cmpm(dst, src);
            break;
        }
        case 0x79: {    // cmpm ind, ind
            uint16_t src = 0;
            uint16_t dst = spc_adrIndInd(&src);
            spc_cmpm(dst, src);
            break;
        }
        case 0x7a: {    // addw dp
            uint16_t low    = 0;
            uint16_t high   = spc_adrDpWord(&low);
            uint16_t value  = spc_readWord(low, high);
            uint16_t ya     = a | (y << 8);
            int      result = ya + value;
            v               = (ya & 0x8000) == (value & 0x8000) && (value & 0x8000) != (result & 0x8000);
            h               = ((ya & 0xfff) + (value & 0xfff) + 1) > 0xfff;
            c               = result > 0xffff;
            z               = (result & 0xffff) == 0;
            n               = result & 0x8000;
            a               = result & 0xff;
            y               = result >> 8;
            break;
        }
        case 0x7b: {    // ror dpx
            spc_ror(spc_adrDpx());
            break;
        }
        case 0x7c: {    // rora imp
            bool newC = a & 1;
            a         = (a >> 1) | (c << 7);
            c         = newC;
            spc_setZN(a);
            break;
        }
        case 0x7d: {    // movax imp
            a = x;
            spc_setZN(a);
            break;
        }
        case 0x7e: {    // cmpy dp
            spc_cmpy(spc_adrDp());
            break;
        }
        case 0x7f: {    // reti imp
            spc_setFlags(spc_pullByte());
            pc = spc_pullWord();
            break;
        }
        case 0x80: {    // setc imp
            c = true;
            break;
        }
        case 0x84: {    // adc dp
            spc_adc(spc_adrDp());
            break;
        }
        case 0x85: {    // adc abs
            spc_adc(spc_adrAbs());
            break;
        }
        case 0x86: {    // adc ind
            spc_adc(spc_adrInd());
            break;
        }
        case 0x87: {    // adc idx
            spc_adc(spc_adrIdx());
            break;
        }
        case 0x88: {    // adc imm
            spc_adc(spc_adrImm());
            break;
        }
        case 0x89: {    // adcm dp, dp
            uint16_t src = 0;
            uint16_t dst = spc_adrDpDp(&src);
            spc_adcm(dst, src);
            break;
        }
        case 0x8a: {    // eor1 abs.bit
            uint16_t adr = 0;
            uint8_t  bit = spc_adrAbsBit(&adr);
            c            = c ^ ((spc_read(adr) >> bit) & 1);
            break;
        }
        case 0x8b: {    // dec dp
            spc_dec(spc_adrDp());
            break;
        }
        case 0x8c: {    // dec abs
            spc_dec(spc_adrAbs());
            break;
        }
        case 0x8d: {    // movy imm
            spc_movy(spc_adrImm());
            break;
        }
        case 0x8e: {    // popp imp
            spc_setFlags(spc_pullByte());
            break;
        }
        case 0x8f: {    // movm dp, imm
            uint16_t src = 0;
            uint16_t dst = spc_adrDpImm(&src);
            uint8_t  val = spc_read(src);
            spc_read(dst);
            spc_write(dst, val);
            break;
        }
        case 0x90: {    // bcc rel
            spc_doBranch(spc_readOpcode(), !c);
            break;
        }
        case 0x94: {    // adc dpx
            spc_adc(spc_adrDpx());
            break;
        }
        case 0x95: {    // adc abx
            spc_adc(spc_adrAbx());
            break;
        }
        case 0x96: {    // adc aby
            spc_adc(spc_adrAby());
            break;
        }
        case 0x97: {    // adc idy
            spc_adc(spc_adrIdy());
            break;
        }
        case 0x98: {    // adcm dp, imm
            uint16_t src = 0;
            uint16_t dst = spc_adrDpImm(&src);
            spc_adcm(dst, src);
            break;
        }
        case 0x99: {    // adcm ind, ind
            uint16_t src = 0;
            uint16_t dst = spc_adrIndInd(&src);
            spc_adcm(dst, src);
            break;
        }
        case 0x9a: {    // subw dp
            uint16_t low    = 0;
            uint16_t high   = spc_adrDpWord(&low);
            uint16_t value  = spc_readWord(low, high) ^ 0xffff;
            uint16_t ya     = a | (y << 8);
            int      result = ya + value + 1;
            v               = (ya & 0x8000) == (value & 0x8000) && (value & 0x8000) != (result & 0x8000);
            h               = ((ya & 0xfff) + (value & 0xfff) + 1) > 0xfff;
            c               = result > 0xffff;
            z               = (result & 0xffff) == 0;
            n               = result & 0x8000;
            a               = result & 0xff;
            y               = result >> 8;
            break;
        }
        case 0x9b: {    // dec dpx
            spc_dec(spc_adrDpx());
            break;
        }
        case 0x9c: {    // deca imp
            a--;
            spc_setZN(a);
            break;
        }
        case 0x9d: {    // movxp imp
            x = sp;
            spc_setZN(x);
            break;
        }
        case 0x9e: {    // div imp
            // TODO: proper division algorithm
            uint16_t value  = a | (y << 8);
            int      result = 0xffff;
            int      mod    = a;
            if (x != 0) {
                result = value / x;
                mod    = value % x;
            }
            v = result > 0xff;
            h = (x & 0xf) <= (y & 0xf);
            a = result;
            y = mod;
            spc_setZN(a);
            break;
        }
        case 0x9f: {    // xcn imp
            a = (a >> 4) | (a << 4);
            spc_setZN(a);
            break;
        }
        case 0xa0: {    // ei  imp
            i = true;
            break;
        }
        case 0xa4: {    // sbc dp
            spc_sbc(spc_adrDp());
            break;
        }
        case 0xa5: {    // sbc abs
            spc_sbc(spc_adrAbs());
            break;
        }
        case 0xa6: {    // sbc ind
            spc_sbc(spc_adrInd());
            break;
        }
        case 0xa7: {    // sbc idx
            spc_sbc(spc_adrIdx());
            break;
        }
        case 0xa8: {    // sbc imm
            spc_sbc(spc_adrImm());
            break;
        }
        case 0xa9: {    // sbcm dp, dp
            uint16_t src = 0;
            uint16_t dst = spc_adrDpDp(&src);
            spc_sbcm(dst, src);
            break;
        }
        case 0xaa: {    // mov1 abs.bit
            uint16_t adr = 0;
            uint8_t  bit = spc_adrAbsBit(&adr);
            c            = (spc_read(adr) >> bit) & 1;
            break;
        }
        case 0xab: {    // inc dp
            spc_inc(spc_adrDp());
            break;
        }
        case 0xac: {    // inc abs
            spc_inc(spc_adrAbs());
            break;
        }
        case 0xad: {    // cmpy imm
            spc_cmpy(spc_adrImm());
            break;
        }
        case 0xae: {    // popa imp
            a = spc_pullByte();
            break;
        }
        case 0xaf: {    // movs ind+
            uint16_t adr = spc_adrIndP();
            spc_write(adr, a);
            break;
        }
        case 0xb0: {    // bcs rel
            spc_doBranch(spc_readOpcode(), c);
            break;
        }
        case 0xb4: {    // sbc dpx
            spc_sbc(spc_adrDpx());
            break;
        }
        case 0xb5: {    // sbc abx
            spc_sbc(spc_adrAbx());
            break;
        }
        case 0xb6: {    // sbc aby
            spc_sbc(spc_adrAby());
            break;
        }
        case 0xb7: {    // sbc idy
            spc_sbc(spc_adrIdy());
            break;
        }
        case 0xb8: {    // sbcm dp, imm
            uint16_t src = 0;
            uint16_t dst = spc_adrDpImm(&src);
            spc_sbcm(dst, src);
            break;
        }
        case 0xb9: {    // sbcm ind, ind
            uint16_t src = 0;
            uint16_t dst = spc_adrIndInd(&src);
            spc_sbcm(dst, src);
            break;
        }
        case 0xba: {    // movw dp
            uint16_t low  = 0;
            uint16_t high = spc_adrDpWord(&low);
            uint16_t val  = spc_readWord(low, high);
            a             = val & 0xff;
            y             = val >> 8;
            z             = val == 0;
            n             = val & 0x8000;
            break;
        }
        case 0xbb: {    // inc dpx
            spc_inc(spc_adrDpx());
            break;
        }
        case 0xbc: {    // inca imp
            a++;
            spc_setZN(a);
            break;
        }
        case 0xbd: {    // movpx imp
            sp = x;
            break;
        }
        case 0xbe: {    // das imp
            if (a > 0x99 || !c) {
                a -= 0x60;
                c = false;
            }
            if ((a & 0xf) > 9 || !h) {
                a -= 6;
            }
            spc_setZN(a);
            break;
        }
        case 0xbf: {    // mov ind+
            uint16_t adr = spc_adrIndP();
            a            = spc_read(adr);
            spc_setZN(a);
            break;
        }
        case 0xc0: {    // di  imp
            i = false;
            break;
        }
        case 0xc4: {    // movs dp
            spc_movs(spc_adrDp());
            break;
        }
        case 0xc5: {    // movs abs
            spc_movs(spc_adrAbs());
            break;
        }
        case 0xc6: {    // movs ind
            spc_movs(spc_adrInd());
            break;
        }
        case 0xc7: {    // movs idx
            spc_movs(spc_adrIdx());
            break;
        }
        case 0xc8: {    // cmpx imm
            spc_cmpx(spc_adrImm());
            break;
        }
        case 0xc9: {    // movsx abs
            spc_movsx(spc_adrAbs());
            break;
        }
        case 0xca: {    // mov1s abs.bit
            uint16_t adr    = 0;
            uint8_t  bit    = spc_adrAbsBit(&adr);
            uint8_t  result = (spc_read(adr) & (~(1 << bit))) | (c << bit);
            spc_write(adr, result);
            break;
        }
        case 0xcb: {    // movsy dp
            spc_movsy(spc_adrDp());
            break;
        }
        case 0xcc: {    // movsy abs
            spc_movsy(spc_adrAbs());
            break;
        }
        case 0xcd: {    // movx imm
            spc_movx(spc_adrImm());
            break;
        }
        case 0xce: {    // popx imp
            x = spc_pullByte();
            break;
        }
        case 0xcf: {    // mul imp
            uint16_t result = a * y;
            a               = result & 0xff;
            y               = result >> 8;
            spc_setZN(y);
            break;
        }
        case 0xd0: {    // bne rel
            spc_doBranch(spc_readOpcode(), !z);
            break;
        }
        case 0xd4: {    // movs dpx
            spc_movs(spc_adrDpx());
            break;
        }
        case 0xd5: {    // movs abx
            spc_movs(spc_adrAbx());
            break;
        }
        case 0xd6: {    // movs aby
            spc_movs(spc_adrAby());
            break;
        }
        case 0xd7: {    // movs idy
            spc_movs(spc_adrIdy());
            break;
        }
        case 0xd8: {    // movsx dp
            spc_movsx(spc_adrDp());
            break;
        }
        case 0xd9: {    // movsx dpy
            spc_movsx(spc_adrDpy());
            break;
        }
        case 0xda: {    // movws dp
            uint16_t low  = 0;
            uint16_t high = spc_adrDpWord(&low);
            spc_read(low);
            spc_write(low, a);
            spc_write(high, y);
            break;
        }
        case 0xdb: {    // movsy dpx
            spc_movsy(spc_adrDpx());
            break;
        }
        case 0xdc: {    // decy imp
            y--;
            spc_setZN(y);
            break;
        }
        case 0xdd: {    // movay imp
            a = y;
            spc_setZN(a);
            break;
        }
        case 0xde: {    // cbne dpx, rel
            uint8_t val    = spc_read(spc_adrDpx()) ^ 0xff;
            uint8_t result = a + val + 1;
            spc_doBranch(spc_readOpcode(), result != 0);
            break;
        }
        case 0xdf: {    // daa imp
            if (a > 0x99 || c) {
                a += 0x60;
                c = true;
            }
            if ((a & 0xf) > 9 || h) {
                a += 6;
            }
            spc_setZN(a);
            break;
        }
        case 0xe0: {    // clrv imp
            v = false;
            h = false;
            break;
        }
        case 0xe4: {    // mov dp
            spc_mov(spc_adrDp());
            break;
        }
        case 0xe5: {    // mov abs
            spc_mov(spc_adrAbs());
            break;
        }
        case 0xe6: {    // mov ind
            spc_mov(spc_adrInd());
            break;
        }
        case 0xe7: {    // mov idx
            spc_mov(spc_adrIdx());
            break;
        }
        case 0xe8: {    // mov imm
            spc_mov(spc_adrImm());
            break;
        }
        case 0xe9: {    // movx abs
            spc_movx(spc_adrAbs());
            break;
        }
        case 0xea: {    // not1 abs.bit
            uint16_t adr    = 0;
            uint8_t  bit    = spc_adrAbsBit(&adr);
            uint8_t  result = spc_read(adr) ^ (1 << bit);
            spc_write(adr, result);
            break;
        }
        case 0xeb: {    // movy dp
            spc_movy(spc_adrDp());
            break;
        }
        case 0xec: {    // movy abs
            spc_movy(spc_adrAbs());
            break;
        }
        case 0xed: {    // notc imp
            c = !c;
            break;
        }
        case 0xee: {    // popy imp
            y = spc_pullByte();
            break;
        }
        case 0xef: {           // sleep imp
            stopped = true;    // no interrupts, so sleeping stops as well
            break;
        }
        case 0xf0: {    // beq rel
            spc_doBranch(spc_readOpcode(), z);
            break;
        }
        case 0xf4: {    // mov dpx
            spc_mov(spc_adrDpx());
            break;
        }
        case 0xf5: {    // mov abx
            spc_mov(spc_adrAbx());
            break;
        }
        case 0xf6: {    // mov aby
            spc_mov(spc_adrAby());
            break;
        }
        case 0xf7: {    // mov idy
            spc_mov(spc_adrIdy());
            break;
        }
        case 0xf8: {    // movx dp
            spc_movx(spc_adrDp());
            break;
        }
        case 0xf9: {    // movx dpy
            spc_movx(spc_adrDpy());
            break;
        }
        case 0xfa: {    // movm dp, dp
            uint16_t src = 0;
            uint16_t dst = spc_adrDpDp(&src);
            uint8_t  val = spc_read(src);
            spc_write(dst, val);
            break;
        }
        case 0xfb: {    // movy dpx
            spc_movy(spc_adrDpx());
            break;
        }
        case 0xfc: {    // incy imp
            y++;
            spc_setZN(y);
            break;
        }
        case 0xfd: {    // movya imp
            y = a;
            spc_setZN(y);
            break;
        }
        case 0xfe: {    // dbnzy rel
            y--;
            spc_doBranch(spc_readOpcode(), y != 0);
            break;
        }
        case 0xff: {    // stop imp
            stopped = true;
            break;
        }
        default: {
            printf("spc op error : %X", opcode);
            exit(1);
        }
    }
}
