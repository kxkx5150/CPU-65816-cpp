
#ifndef CPU_H
#define CPU_H
#include <cstdint>

class Snes;
class CPU {
  public:
    Snes *snes = nullptr;

    void   *mem     = nullptr;
    uint8_t memType = 0;

    uint16_t a  = 0;
    uint16_t x  = 0;
    uint16_t y  = 0;
    uint16_t sp = 0;
    uint16_t pc = 0;
    uint16_t dp = 0;
    uint8_t  k  = 0;
    uint8_t  db = 0;

    bool c  = false;
    bool z  = false;
    bool v  = false;
    bool n  = false;
    bool i  = false;
    bool d  = false;
    bool xf = false;
    bool mf = false;
    bool e  = false;

    bool irqWanted = false;
    bool nmiWanted = false;
    bool waiting   = false;
    bool stopped   = false;

    uint8_t cyclesUsed = 0;

  public:
    CPU(Snes *_snes);
    CPU *cpu_init(void *mem, int memType);

    uint8_t cpu_read(uint32_t adr);
    void    cpu_write(uint32_t adr, uint8_t val);
    void    cpu_reset();

    int cpu_runOpcode();

    uint8_t  cpu_readOpcode();
    uint16_t cpu_readOpcodeWord();

    uint8_t cpu_getFlags();
    void    cpu_setFlags(uint8_t val);

    void cpu_setZN(uint16_t value, bool byte);
    void cpu_doBranch(uint8_t value, bool check);

    uint8_t  cpu_pullByte();
    void     cpu_pushByte(uint8_t value);
    uint16_t cpu_pullWord();
    void     cpu_pushWord(uint16_t value);
    uint16_t cpu_readWord(uint32_t adrl, uint32_t adrh);
    void     cpu_writeWord(uint32_t adrl, uint32_t adrh, uint16_t value, bool reversed);

    void cpu_doInterrupt(bool irq);

    uint32_t cpu_adrImm(uint32_t *low, bool xFlag);
    uint32_t cpu_adrDp(uint32_t *low);
    uint32_t cpu_adrDpx(uint32_t *low);
    uint32_t cpu_adrDpy(uint32_t *low);
    uint32_t cpu_adrIdp(uint32_t *low);
    uint32_t cpu_adrIdx(uint32_t *low);
    uint32_t cpu_adrIdy(uint32_t *low, bool write);
    uint32_t cpu_adrIdl(uint32_t *low);
    uint32_t cpu_adrIly(uint32_t *low);
    uint32_t cpu_adrSr(uint32_t *low);
    uint32_t cpu_adrIsy(uint32_t *low);
    uint32_t cpu_adrAbs(uint32_t *low);
    uint32_t cpu_adrAbx(uint32_t *low, bool write);
    uint32_t cpu_adrAby(uint32_t *low, bool write);
    uint32_t cpu_adrAbl(uint32_t *low);
    uint32_t cpu_adrAlx(uint32_t *low);

    uint16_t cpu_adrIax();

    void cpu_and(uint32_t low, uint32_t high);
    void cpu_ora(uint32_t low, uint32_t high);
    void cpu_eor(uint32_t low, uint32_t high);
    void cpu_adc(uint32_t low, uint32_t high);
    void cpu_sbc(uint32_t low, uint32_t high);
    void cpu_cmp(uint32_t low, uint32_t high);
    void cpu_cpx(uint32_t low, uint32_t high);
    void cpu_cpy(uint32_t low, uint32_t high);
    void cpu_bit(uint32_t low, uint32_t high);
    void cpu_lda(uint32_t low, uint32_t high);
    void cpu_ldx(uint32_t low, uint32_t high);
    void cpu_ldy(uint32_t low, uint32_t high);
    void cpu_sta(uint32_t low, uint32_t high);
    void cpu_stx(uint32_t low, uint32_t high);
    void cpu_sty(uint32_t low, uint32_t high);
    void cpu_stz(uint32_t low, uint32_t high);
    void cpu_ror(uint32_t low, uint32_t high);
    void cpu_rol(uint32_t low, uint32_t high);
    void cpu_lsr(uint32_t low, uint32_t high);
    void cpu_asl(uint32_t low, uint32_t high);
    void cpu_inc(uint32_t low, uint32_t high);
    void cpu_dec(uint32_t low, uint32_t high);
    void cpu_tsb(uint32_t low, uint32_t high);
    void cpu_trb(uint32_t low, uint32_t high);

    void cpu_doOpcode(uint8_t opcode);

  public:
    const int cyclesPerOpcode[256] = {
        7, 6, 7, 4, 5, 3, 5, 6, 3, 2, 2, 4, 6, 4, 6, 5, 2, 5, 5, 7, 5, 4, 6, 6, 2, 4, 2, 2, 6, 4, 7, 5, 6, 6, 8, 4, 3,
        3, 5, 6, 4, 2, 2, 5, 4, 4, 6, 5, 2, 5, 5, 7, 4, 4, 6, 6, 2, 4, 2, 2, 4, 4, 7, 5, 6, 6, 2, 4, 7, 3, 5, 6, 3, 2,
        2, 3, 3, 4, 6, 5, 2, 5, 5, 7, 7, 4, 6, 6, 2, 4, 3, 2, 4, 4, 7, 5, 6, 6, 6, 4, 3, 3, 5, 6, 4, 2, 2, 6, 5, 4, 6,
        5, 2, 5, 5, 7, 4, 4, 6, 6, 2, 4, 4, 2, 6, 4, 7, 5, 3, 6, 4, 4, 3, 3, 3, 6, 2, 2, 2, 3, 4, 4, 4, 5, 2, 6, 5, 7,
        4, 4, 4, 6, 2, 5, 2, 2, 4, 5, 5, 5, 2, 6, 2, 4, 3, 3, 3, 6, 2, 2, 2, 4, 4, 4, 4, 5, 2, 5, 5, 7, 4, 4, 4, 6, 2,
        4, 2, 2, 4, 4, 4, 5, 2, 6, 3, 4, 3, 3, 5, 6, 2, 2, 2, 3, 4, 4, 6, 5, 2, 5, 5, 7, 6, 4, 6, 6, 2, 4, 3, 3, 6, 4,
        7, 5, 2, 6, 3, 4, 3, 3, 5, 6, 2, 2, 2, 3, 4, 4, 6, 5, 2, 5, 5, 7, 5, 4, 6, 6, 2, 4, 4, 2, 8, 4, 7, 5};
};
#endif
