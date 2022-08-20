#ifndef _MEM_H_
#define _MEM_H_
#include <cstdint>
#include "../allegro.h"
#include "cpu.h"

class SNES;
class MEM {
  public:
    SNES *snes = nullptr;

    uint16_t srammask;
    uint8_t *sram;
    uint8_t *ram;
    uint8_t *rom;

    int      lorom = 0;
    uint8_t *memlookup[2048];
    uint8_t  memread[2048], memwrite[2048];
    uint8_t  accessspeed[2048];

  public:
    MEM(SNES *_snes);

    void    allocmem();
    void    loadrom(char *fn);
    void    initmem();
    uint8_t readmeml(uint32_t addr);
    void    writememl(uint32_t addr, uint8_t val);
};
#endif
