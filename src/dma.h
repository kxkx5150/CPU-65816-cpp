#ifndef DMA_H
#define DMA_H
#include <stdint.h>

typedef struct DmaChannel
{
    uint8_t  bAdr       = 0;
    uint16_t aAdr       = 0;
    uint8_t  aBank      = 0;
    uint16_t size       = 0;    // also indirect hdma adr
    uint8_t  indBank    = 0;    // hdma
    uint16_t tableAdr   = 0;    // hdma
    uint8_t  repCount   = 0;    // hdma
    uint8_t  unusedByte = 0;
    uint8_t  mode       = 0;
    uint8_t  offIndex   = 0;

    bool dmaActive  = false;
    bool hdmaActive = false;
    bool fixed      = false;
    bool decrement  = false;
    bool indirect   = false;    // hdma
    bool fromB      = false;
    bool unusedBit  = false;
    bool doTransfer = false;    // hdma
    bool terminated = false;    // hdma
} DmaChannel;

class Snes;
class Dma {
  public:
    Snes *snes = nullptr;

    DmaChannel channel[8] = {};
    uint16_t   hdmaTimer  = 0;
    uint32_t   dmaTimer   = 0;
    bool       dmaBusy    = false;

  public:
    Dma(Snes *_snes);

    void    dma_reset();
    uint8_t dma_read(uint16_t adr);
    void    dma_write(uint16_t adr, uint8_t val);
    void    dma_doDma();
    void    dma_initHdma();
    void    dma_doHdma();
    void    dma_transferByte(uint16_t aAdr, uint8_t aBank, uint8_t bAdr, bool fromB);
    bool    dma_cycle();
    void    dma_startDma(uint8_t val, bool hdma);

  public:
    const int bAdrOffsets[8][4] = {{0, 0, 0, 0}, {0, 1, 0, 1}, {0, 0, 0, 0}, {0, 0, 1, 1},
                                   {0, 1, 2, 3}, {0, 1, 0, 1}, {0, 0, 0, 0}, {0, 0, 1, 1}};
    const int transferLength[8] = {1, 2, 2, 4, 4, 4, 2, 4};
};
#endif
