
#ifndef _DMA_H_
#define _DMA_H_
#include "utils/port.h"

class SDMA {
  public:
    bool8  ReverseTransfer;
    bool8  HDMAIndirectAddressing;
    bool8  UnusedBit43x0;
    bool8  AAddressFixed;
    bool8  AAddressDecrement;
    uint8  TransferMode;
    uint8  BAddress;
    uint16 AAddress;
    uint8  ABank;
    uint16 DMACount_Or_HDMAIndirectAddress;
    uint8  IndirectBank;
    uint16 Address;
    uint8  Repeat;
    uint8  LineCount;
    uint8  UnknownByte;
    uint8  DoTransfer;
};

class SNESX;
class SDMAS {
  private:
    uint8 sdd1_decode_buffer[0x10000];
    int   HDMA_ModeByteCounts[8] = {1, 2, 2, 4, 4, 4, 2, 4};

  public:
    SNESX *snes = nullptr;

    SDMA   DMA[8];
    uint8 *HDMAMemPointers[8];

  public:
    SDMAS(SNESX *_snes);

    bool8 addCyclesInDMA(uint8 dma_channel);
    bool8 S9xDoDMA(uint8 Channel);
    bool8 HDMAReadLineCount(int d);
    void  S9xStartHDMA(void);
    uint8 S9xDoHDMA(uint8 byte);
    void  S9xResetDMA(void);
};
#endif
