#ifndef _IO_H_
#define _IO_H_
#include "cpu.h"
#include <cstdint>

class SNES;
class IO {
  public:
    SNES *snes = nullptr;

    int      intthisline;
    uint8_t  hdmaena;
    int      padpos, padstat;
    int      dmaops   = 0;
    int      framenum = 0;
    uint16_t mulr, divc, divr;
    uint8_t  mula, mulb, divb;

  public:
    uint16_t dmadest[8], dmasrc[8], dmalen[8];
    uint32_t hdmaaddr[8], hdmaaddr2[8];
    uint8_t  dmabank[8], dmaibank[8], dmactrl[8], hdmastat[8], hdmadat[8];
    int      hdmacount[8];
    uint16_t pad[4];

  public:
    IO(SNES *_snes);

    void    readjoy();
    uint8_t readjoyold(uint16_t addr);
    void    writejoyold(uint16_t addr, uint8_t val);
    void    writeio(uint16_t addr, uint8_t val);
    uint8_t readio(uint16_t addr);

    void dohdma_macro(int c, int no);
    void dohdma(int line);
};
#endif
