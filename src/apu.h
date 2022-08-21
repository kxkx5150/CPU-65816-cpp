#ifndef APU_H
#define APU_H
#include "spc.h"
#include "dsp.h"


typedef struct Timer
{
    uint8_t cycles  = 0;
    uint8_t divider = 0;
    uint8_t target  = 0;
    uint8_t counter = 0;
    bool    enabled = 0;
} Timer;


struct Snes;
class Apu {
  public:
    Snes   *snes = nullptr;
    SPC    *spc  = nullptr;
    Dsp    *dsp  = nullptr;
    uint8_t ram[0x10000];

    uint8_t  dspAdr        = 0;
    uint32_t cycles        = 0;
    uint8_t  cpuCyclesLeft = 0;

    bool romReadable = false;

    uint8_t inPorts[6]  = {};
    uint8_t outPorts[4] = {};
    Timer   timer[3]    = {};

  public:
    Apu(Snes *_snes);
    ~Apu();

    void    apu_reset();
    void    apu_cycle();
    uint8_t apu_cpuRead(uint16_t adr);
    void    apu_cpuWrite(uint16_t adr, uint8_t val);

  public:
    const uint8_t bootRom[0x40] = {0xcd, 0xef, 0xbd, 0xe8, 0x00, 0xc6, 0x1d, 0xd0, 0xfc, 0x8f, 0xaa, 0xf4, 0x8f,
                                   0xbb, 0xf5, 0x78, 0xcc, 0xf4, 0xd0, 0xfb, 0x2f, 0x19, 0xeb, 0xf4, 0xd0, 0xfc,
                                   0x7e, 0xf4, 0xd0, 0x0b, 0xe4, 0xf5, 0xcb, 0xf4, 0xd7, 0x00, 0xfc, 0xd0, 0xf3,
                                   0xab, 0x01, 0x10, 0xef, 0x7e, 0xf4, 0x10, 0xeb, 0xba, 0xf6, 0xda, 0x00, 0xba,
                                   0xf4, 0xc4, 0xf4, 0xdd, 0x5d, 0xd0, 0xdb, 0x1f, 0x00, 0x00, 0xc0, 0xff};
};
#endif
