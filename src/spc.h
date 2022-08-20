#ifndef _SPC_H_
#define _SPC_H_
#include "cpu.h"
#include <cstdint>

class SNES;
class SPC {
  private:
    union
    {
        uint16_t w;
        struct
        {
            uint8_t a, y;
        } b;
    } ya;

    uint8_t  x;
    uint8_t  sp;
    uint16_t pc;

    struct
    {
        int n, v, p, b, h, i, z, c;
    } p;

  public:
    SNES *snes = nullptr;

    uint8_t *spcram      = nullptr;
    uint8_t *spcreadhigh = nullptr;

    uint8_t  dspaddr   = 0;
    double   spccycles = 0;
    int      dsptotal  = 0;
    uint16_t spc2 = 0, spc3 = 0;

  public:
    uint8_t spctocpu[4] = {};
    int     spctimer[3] = {}, spctimer2[3] = {}, spclimit[3] = {};

    uint8_t spcrom[64] = {0xCD, 0xEF, 0xBD, 0xE8, 0x00, 0xC6, 0x1D, 0xD0, 0xFC, 0x8F, 0xAA, 0xF4, 0x8F,
                          0xBB, 0xF5, 0x78, 0xCC, 0xF4, 0xD0, 0xFB, 0x2F, 0x19, 0xEB, 0xF4, 0xD0, 0xFC,
                          0x7E, 0xF4, 0xD0, 0x0B, 0xE4, 0xF5, 0xCB, 0xF4, 0xD7, 0x00, 0xFC, 0xD0, 0xF3,
                          0xAB, 0x01, 0x10, 0xEF, 0x7E, 0xF4, 0x10, 0xEB, 0xBA, 0xF6, 0xDA, 0x00, 0xBA,
                          0xF4, 0xC4, 0xF4, 0xDD, 0x5D, 0xD0, 0xDB, 0x1F, 0x00, 0x00, 0xC0, 0xFF};

  public:
    SPC(SNES *_snes);

    uint8_t readfromspc(uint16_t addr);
    void    writetospc(uint16_t addr, uint8_t val);
    void    writespcregs(uint16_t a, uint8_t v);

    uint16_t getspcpc();
    uint8_t  readspcregs(uint16_t a);

    void initspc();
    void resetspc();
    void execspc();

    void    getdp(uint16_t *addr);
    void    getdpx(uint16_t *addr);
    void    getdpy(uint16_t *addr);
    void    getabs(uint16_t *addr);
    void    setspczn(uint8_t v);
    void    setspczn16(uint16_t v);
    uint8_t readspc(uint16_t a);
    void    writespc(uint16_t a, uint8_t v);
    void    ADC(uint8_t *ac, uint8_t temp, uint16_t *tempw);
    void    SBC(uint8_t *ac, uint8_t temp, uint16_t *tempw);
};
#endif
