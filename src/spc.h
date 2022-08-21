#ifndef SPC_H
#define SPC_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

class Apu;
class SPC {
  public:
    Apu *apu;

    // registers
    uint8_t  a  = 0;
    uint8_t  x  = 0;
    uint8_t  y  = 0;
    uint8_t  sp = 0;
    uint16_t pc = 0;

    // flags
    bool c = false;
    bool z = false;
    bool v = false;
    bool n = false;
    bool i = false;
    bool h = false;
    bool p = false;
    bool b = false;

    // stopping
    bool stopped = false;

    // internal use
    uint8_t cyclesUsed = 0;

  public:
    SPC(Apu *_apu);

  public:
    uint8_t spc_read(uint16_t adr);
    void    spc_write(uint16_t adr, uint8_t val);

    void     spc_reset();
    int      spc_runOpcode();
    uint8_t  spc_readOpcode();
    uint16_t spc_readOpcodeWord();
    uint8_t  spc_getFlags();
    void     spc_setFlags(uint8_t val);
    void     spc_setZN(uint8_t value);
    void     spc_doBranch(uint8_t value, bool check);
    uint8_t  spc_pullByte();
    void     spc_pushByte(uint8_t value);
    uint16_t spc_pullWord();
    void     spc_pushWord(uint16_t value);
    uint16_t spc_readWord(uint16_t adrl, uint16_t adrh);
    void     spc_writeWord(uint16_t adrl, uint16_t adrh, uint16_t value);

    // adressing modes;
    uint16_t spc_adrDp();
    uint16_t spc_adrAbs();
    uint16_t spc_adrInd();
    uint16_t spc_adrIdx();
    uint16_t spc_adrImm();
    uint16_t spc_adrDpx();
    uint16_t spc_adrDpy();
    uint16_t spc_adrAbx();
    uint16_t spc_adrAby();
    uint16_t spc_adrIdy();
    uint16_t spc_adrDpDp(uint16_t *src);
    uint16_t spc_adrDpImm(uint16_t *src);
    uint16_t spc_adrIndInd(uint16_t *src);
    uint8_t  spc_adrAbsBit(uint16_t *adr);
    uint16_t spc_adrDpWord(uint16_t *low);
    uint16_t spc_adrIndP();

    // opcode functions;
    void spc_and(uint16_t adr);
    void spc_andm(uint16_t dst, uint16_t src);
    void spc_or(uint16_t adr);
    void spc_orm(uint16_t dst, uint16_t src);
    void spc_eor(uint16_t adr);
    void spc_eorm(uint16_t dst, uint16_t src);
    void spc_adc(uint16_t adr);
    void spc_adcm(uint16_t dst, uint16_t src);
    void spc_sbc(uint16_t adr);
    void spc_sbcm(uint16_t dst, uint16_t src);
    void spc_cmp(uint16_t adr);
    void spc_cmpx(uint16_t adr);
    void spc_cmpy(uint16_t adr);
    void spc_cmpm(uint16_t dst, uint16_t src);
    void spc_mov(uint16_t adr);
    void spc_movx(uint16_t adr);
    void spc_movy(uint16_t adr);
    void spc_movs(uint16_t adr);
    void spc_movsx(uint16_t adr);
    void spc_movsy(uint16_t adr);
    void spc_asl(uint16_t adr);
    void spc_lsr(uint16_t adr);
    void spc_rol(uint16_t adr);
    void spc_ror(uint16_t adr);
    void spc_inc(uint16_t adr);
    void spc_dec(uint16_t adr);

    void spc_doOpcode(uint8_t opcode);

  public:
    const int cyclesPerOpcode[256] = {
        2, 8, 4, 5, 3, 4, 3, 6, 2, 6, 5,  4, 5, 4, 6, 8, 2, 8, 4, 5, 4, 5, 5, 6, 5, 5, 6, 5, 2, 2, 4, 6, 2, 8, 4, 5, 3,
        4, 3, 6, 2, 6, 5, 4, 5, 4, 5, 4,  2, 8, 4, 5, 4, 5, 5, 6, 5, 5, 6, 5, 2, 2, 3, 8, 2, 8, 4, 5, 3, 4, 3, 6, 2, 6,
        4, 4, 5, 4, 6, 6, 2, 8, 4, 5, 4,  5, 5, 6, 5, 5, 4, 5, 2, 2, 4, 3, 2, 8, 4, 5, 3, 4, 3, 6, 2, 6, 4, 4, 5, 4, 5,
        5, 2, 8, 4, 5, 4, 5, 5, 6, 5, 5,  5, 5, 2, 2, 3, 6, 2, 8, 4, 5, 3, 4, 3, 6, 2, 6, 5, 4, 5, 2, 4, 5, 2, 8, 4, 5,
        4, 5, 5, 6, 5, 5, 5, 5, 2, 2, 12, 5, 2, 8, 4, 5, 3, 4, 3, 6, 2, 6, 4, 4, 5, 2, 4, 4, 2, 8, 4, 5, 4, 5, 5, 6, 5,
        5, 5, 5, 2, 2, 3, 4, 2, 8, 4, 5,  4, 5, 4, 7, 2, 5, 6, 4, 5, 2, 4, 9, 2, 8, 4, 5, 5, 6, 6, 7, 4, 5, 5, 5, 2, 2,
        6, 3, 2, 8, 4, 5, 3, 4, 3, 6, 2,  4, 5, 3, 4, 3, 4, 3, 2, 8, 4, 5, 4, 5, 5, 6, 3, 4, 5, 4, 2, 2, 4, 3};
};


#endif
