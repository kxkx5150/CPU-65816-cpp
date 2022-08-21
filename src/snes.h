
#ifndef SNES_H
#define SNES_H
#include "cpu.h"
#include "apu.h"
#include "dma.h"
#include "ppu.h"
#include "cart.h"
#include "input.h"

class Snes {
    const double apuCyclesPerMaster = (32040 * 32) / (1364 * 262 * 60.0);

  public:
    CPU   *cpu    = nullptr;
    Apu   *apu    = nullptr;
    Ppu   *ppu    = nullptr;
    Dma   *dma    = nullptr;
    Cart  *cart   = nullptr;
    Input *input1 = nullptr;
    Input *input2 = nullptr;

    // ram
    uint8_t  ram[0x20000] = {};
    uint32_t ramAdr       = 0;

    // frame timing
    uint16_t hPos   = 0;
    uint16_t vPos   = 0;
    uint32_t frames = 0;

    // cpu handling
    uint8_t cpuCyclesLeft    = 0;
    uint8_t cpuMemOps        = 0;
    double  apuCatchupCycles = 0;

    // nmi / irq
    bool     hIrqEnabled = false;
    bool     vIrqEnabled = false;
    bool     nmiEnabled  = false;
    uint16_t hTimer      = 0;
    uint16_t vTimer      = 0;
    bool     inNmi       = false;
    bool     inIrq       = false;
    bool     inVblank    = false;

    // joypad handling
    uint16_t portAutoRead[4] = {};
    bool     autoJoyRead     = false;
    uint16_t autoJoyTimer    = 0;
    bool     ppuLatch        = false;

    // multiplication/division
    uint8_t  multiplyA      = 0;
    uint16_t multiplyResult = 0;
    uint16_t divideA        = 0;
    uint16_t divideResult   = 0;

    // misc
    bool    fastMem = false;
    uint8_t openBus = 0;

  public:
    Snes();
    ~Snes();

    void    snes_reset(bool hard);
    void    snes_runFrame();
    void    snes_runCycle();
    void    snes_runCpu();
    void    snes_catchupApu();
    void    snes_doAutoJoypad();
    uint8_t snes_readBBus(uint8_t adr);
    void    snes_writeBBus(uint8_t adr, uint8_t val);
    uint8_t snes_readReg(uint16_t adr);
    void    snes_writeReg(uint16_t adr, uint8_t val);
    uint8_t snes_rread(uint32_t adr);
    void    snes_write(uint32_t adr, uint8_t val);
    int     snes_getAccessTime(uint32_t adr);
    uint8_t snes_read(uint32_t adr);
    uint8_t snes_cpuRead(uint32_t adr);
    void    snes_cpuWrite(uint32_t adr, uint8_t val);

  public:
};
#endif
