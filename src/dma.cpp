#include "snes.h"
#include "mem.h"
#include "dma.h"
#include "apu/apu.h"
#include "ppu.h"
#include "cpu.h"

#define ADD_CYCLES(n)                                                                                                  \
    {                                                                                                                  \
        snes->scpu->Cycles += (n);                                                                                     \
    }

//

SDMAS::SDMAS(SNESX *_snes)
{
    snes = _snes;
}
bool8 SDMAS::addCyclesInDMA(uint8 dma_channel)
{
    ADD_CYCLES(SLOW_ONE_CYCLE);
    while (snes->scpu->Cycles >= snes->scpu->NextEvent)
        snes->scpu->S9xDoHEventProcessing();
    if (snes->scpu->HDMARanInDMA & (1 << dma_channel)) {
        snes->scpu->HDMARanInDMA = 0;
        return (FALSE);
    }
    snes->scpu->HDMARanInDMA = 0;
    return (TRUE);
}
bool8 SDMAS::S9xDoDMA(uint8 Channel)
{
    snes->scpu->InDMA                   = TRUE;
    snes->scpu->InDMAorHDMA             = TRUE;
    snes->scpu->CurrentDMAorHDMAChannel = Channel;
    SDMA *d                             = &DMA[Channel];

    if ((d->ABank == 0x7E || d->ABank == 0x7F) && d->BAddress == 0x80 && !d->ReverseTransfer) {
        int32 c = d->DMACount_Or_HDMAIndirectAddress;
        if (c == 0)
            c = 0x10000;
        ADD_CYCLES(SLOW_ONE_CYCLE);
        while (c) {
            d->DMACount_Or_HDMAIndirectAddress--;
            d->AAddress++;
            c--;
            if (!addCyclesInDMA(Channel)) {
                snes->scpu->InDMA                   = FALSE;
                snes->scpu->InDMAorHDMA             = FALSE;
                snes->scpu->CurrentDMAorHDMAChannel = -1;
                return (FALSE);
            }
        }

        snes->scpu->InDMA                   = FALSE;
        snes->scpu->InDMAorHDMA             = FALSE;
        snes->scpu->CurrentDMAorHDMAChannel = -1;
        return (TRUE);
    }
    switch (d->BAddress) {
        case 0x18:
        case 0x19:
            if (snes->ppu->IPPU.RenderThisFrame)
                snes->ppu->FLUSH_REDRAW();
            break;
    }
    int32 inc   = d->AAddressFixed ? 0 : (!d->AAddressDecrement ? 1 : -1);
    int32 count = d->DMACount_Or_HDMAIndirectAddress;
    if (count == 0)
        count = 0x10000;
    uint8 *in_sdd1_dma = NULL;
    if (Settings.SDD1) {
        if (d->AAddressFixed && snes->mem->FillRAM[0x4801] > 0) {
            inc           = !d->AAddressDecrement ? 1 : -1;
            uint8 *in_ptr = snes->mem->S9xGetBasePointer(((d->ABank << 16) | d->AAddress));
            if (in_ptr) {
                in_ptr += d->AAddress;
            }

            in_sdd1_dma = sdd1_decode_buffer;
        }
        snes->mem->FillRAM[0x4801] = 0;
    }
    uint8 *spc7110_dma = NULL;


    bool8 in_sa1_dma = FALSE;

    uint8 Work;
    ADD_CYCLES(SLOW_ONE_CYCLE);
    if (!d->ReverseTransfer) {
        int32  b    = 0;
        uint16 p    = d->AAddress;
        uint8 *base = snes->mem->S9xGetBasePointer((d->ABank << 16) + d->AAddress);
        bool8  inWRAM_DMA;
        int32  rem = count;
        count      = d->AAddressFixed
                         ? rem
                         : (d->AAddressDecrement ? ((p & MEMMAP_MASK) + 1) : (MEMMAP_BLOCK_SIZE - (p & MEMMAP_MASK)));
        if (in_sa1_dma) {
            base  = &snes->mem->ROM[CMemory::MAX_ROM_SIZE - 0x10000];
            p     = 0;
            count = rem;
        } else if (in_sdd1_dma) {
            base  = in_sdd1_dma;
            p     = 0;
            count = rem;
        } else if (spc7110_dma) {
            base  = spc7110_dma;
            p     = 0;
            count = rem;
        }
        inWRAM_DMA = ((!in_sa1_dma && !in_sdd1_dma && !spc7110_dma) &&
                      (d->ABank == 0x7e || d->ABank == 0x7f || (!(d->ABank & 0x40) && d->AAddress < 0x2000)));
#define UPDATE_COUNTERS                                                                                                \
    d->DMACount_Or_HDMAIndirectAddress--;                                                                              \
    d->AAddress += inc;                                                                                                \
    p += inc;                                                                                                          \
    if (!addCyclesInDMA(Channel)) {                                                                                    \
        snes->scpu->InDMA                   = FALSE;                                                                   \
        snes->scpu->InDMAorHDMA             = FALSE;                                                                   \
        snes->scpu->InWRAMDMAorHDMA         = FALSE;                                                                   \
        snes->scpu->CurrentDMAorHDMAChannel = -1;                                                                      \
        return (FALSE);                                                                                                \
    }
        while (1) {
            if (count > rem)
                count = rem;
            rem -= count;
            snes->scpu->InWRAMDMAorHDMA = inWRAM_DMA;
            if (!base) {
                if (d->TransferMode == 0 || d->TransferMode == 2 || d->TransferMode == 6) {
                    do {
                        Work = snes->mem->S9xGetByte((d->ABank << 16) + p);
                        snes->ppu->S9xSetPPU(Work, 0x2100 + d->BAddress);
                        UPDATE_COUNTERS;
                    } while (--count > 0);
                } else if (d->TransferMode == 1 || d->TransferMode == 5) {
                    switch (b) {
                        default:
                            while (count > 1) {
                                Work = snes->mem->S9xGetByte((d->ABank << 16) + p);
                                snes->ppu->S9xSetPPU(Work, 0x2100 + d->BAddress);
                                UPDATE_COUNTERS;
                                count--;
                                case 1:
                                    Work = snes->mem->S9xGetByte((d->ABank << 16) + p);
                                    snes->ppu->S9xSetPPU(Work, 0x2101 + d->BAddress);
                                    UPDATE_COUNTERS;
                                    count--;
                            }
                    }
                    if (count == 1) {
                        Work = snes->mem->S9xGetByte((d->ABank << 16) + p);
                        snes->ppu->S9xSetPPU(Work, 0x2100 + d->BAddress);
                        UPDATE_COUNTERS;
                        b = 1;
                    } else
                        b = 0;
                } else if (d->TransferMode == 3 || d->TransferMode == 7) {
                    switch (b) {
                        default:
                            do {
                                Work = snes->mem->S9xGetByte((d->ABank << 16) + p);
                                snes->ppu->S9xSetPPU(Work, 0x2100 + d->BAddress);
                                UPDATE_COUNTERS;
                                if (--count <= 0) {
                                    b = 1;
                                    break;
                                }
                                case 1:
                                    Work = snes->mem->S9xGetByte((d->ABank << 16) + p);
                                    snes->ppu->S9xSetPPU(Work, 0x2100 + d->BAddress);
                                    UPDATE_COUNTERS;
                                    if (--count <= 0) {
                                        b = 2;
                                        break;
                                    }
                                case 2:
                                    Work = snes->mem->S9xGetByte((d->ABank << 16) + p);
                                    snes->ppu->S9xSetPPU(Work, 0x2101 + d->BAddress);
                                    UPDATE_COUNTERS;
                                    if (--count <= 0) {
                                        b = 3;
                                        break;
                                    }
                                case 3:
                                    Work = snes->mem->S9xGetByte((d->ABank << 16) + p);
                                    snes->ppu->S9xSetPPU(Work, 0x2101 + d->BAddress);
                                    UPDATE_COUNTERS;
                                    if (--count <= 0) {
                                        b = 0;
                                        break;
                                    }
                            } while (1);
                    }
                } else if (d->TransferMode == 4) {
                    switch (b) {
                        default:
                            do {
                                Work = snes->mem->S9xGetByte((d->ABank << 16) + p);
                                snes->ppu->S9xSetPPU(Work, 0x2100 + d->BAddress);
                                UPDATE_COUNTERS;
                                if (--count <= 0) {
                                    b = 1;
                                    break;
                                }
                                case 1:
                                    Work = snes->mem->S9xGetByte((d->ABank << 16) + p);
                                    snes->ppu->S9xSetPPU(Work, 0x2101 + d->BAddress);
                                    UPDATE_COUNTERS;
                                    if (--count <= 0) {
                                        b = 2;
                                        break;
                                    }
                                case 2:
                                    Work = snes->mem->S9xGetByte((d->ABank << 16) + p);
                                    snes->ppu->S9xSetPPU(Work, 0x2102 + d->BAddress);
                                    UPDATE_COUNTERS;
                                    if (--count <= 0) {
                                        b = 3;
                                        break;
                                    }
                                case 3:
                                    Work = snes->mem->S9xGetByte((d->ABank << 16) + p);
                                    snes->ppu->S9xSetPPU(Work, 0x2103 + d->BAddress);
                                    UPDATE_COUNTERS;
                                    if (--count <= 0) {
                                        b = 0;
                                        break;
                                    }
                            } while (1);
                    }
                }

            } else {
                if (d->TransferMode == 0 || d->TransferMode == 2 || d->TransferMode == 6) {
                    switch (d->BAddress) {
                        case 0x04:
                            do {
                                Work = *(base + p);
                                snes->ppu->REGISTER_2104(Work);
                                UPDATE_COUNTERS;
                            } while (--count > 0);
                            break;
                        case 0x18:
                            if (!snes->ppu->VMA.FullGraphicCount) {
                                do {
                                    Work = *(base + p);
                                    snes->ppu->REGISTER_2118_linear(Work);
                                    UPDATE_COUNTERS;
                                } while (--count > 0);
                            } else {
                                do {
                                    Work = *(base + p);
                                    snes->ppu->REGISTER_2118_tile(Work);
                                    UPDATE_COUNTERS;
                                } while (--count > 0);
                            }
                            break;
                        case 0x19:
                            if (!snes->ppu->VMA.FullGraphicCount) {
                                do {
                                    Work = *(base + p);
                                    snes->ppu->REGISTER_2119_linear(Work);
                                    UPDATE_COUNTERS;
                                } while (--count > 0);
                            } else {
                                do {
                                    Work = *(base + p);
                                    snes->ppu->REGISTER_2119_tile(Work);
                                    UPDATE_COUNTERS;
                                } while (--count > 0);
                            }
                            break;
                        case 0x22:
                            do {
                                Work = *(base + p);
                                snes->ppu->REGISTER_2122(Work);
                                UPDATE_COUNTERS;
                            } while (--count > 0);
                            break;
                        case 0x80:
                            if (!snes->scpu->InWRAMDMAorHDMA) {
                                do {
                                    Work = *(base + p);
                                    snes->ppu->REGISTER_2180(Work);
                                    UPDATE_COUNTERS;
                                } while (--count > 0);
                            } else {
                                do {
                                    UPDATE_COUNTERS;
                                } while (--count > 0);
                            }
                            break;
                        default:
                            do {
                                Work = *(base + p);
                                snes->ppu->S9xSetPPU(Work, 0x2100 + d->BAddress);
                                UPDATE_COUNTERS;
                            } while (--count > 0);
                            break;
                    }
                } else if (d->TransferMode == 1 || d->TransferMode == 5) {
                    if (d->BAddress == 0x18) {
                        if (!snes->ppu->VMA.FullGraphicCount) {
                            switch (b) {
                                default:
                                    while (count > 1) {
                                        Work = *(base + p);
                                        snes->ppu->REGISTER_2118_linear(Work);
                                        UPDATE_COUNTERS;
                                        count--;
                                        case 1:
                                            snes->OpenBus = *(base + p);
                                            snes->ppu->REGISTER_2119_linear(snes->OpenBus);
                                            UPDATE_COUNTERS;
                                            count--;
                                    }
                            }
                            if (count == 1) {
                                Work = *(base + p);
                                snes->ppu->REGISTER_2118_linear(Work);
                                UPDATE_COUNTERS;
                                b = 1;
                            } else
                                b = 0;
                        } else {
                            switch (b) {
                                default:
                                    while (count > 1) {
                                        Work = *(base + p);
                                        snes->ppu->REGISTER_2118_tile(Work);
                                        UPDATE_COUNTERS;
                                        count--;
                                        case 1:
                                            Work = *(base + p);
                                            snes->ppu->REGISTER_2119_tile(Work);
                                            UPDATE_COUNTERS;
                                            count--;
                                    }
                            }
                            if (count == 1) {
                                Work = *(base + p);
                                snes->ppu->REGISTER_2118_tile(Work);
                                UPDATE_COUNTERS;
                                b = 1;
                            } else
                                b = 0;
                        }
                    } else {
                        switch (b) {
                            default:
                                while (count > 1) {
                                    Work = *(base + p);
                                    snes->ppu->S9xSetPPU(Work, 0x2100 + d->BAddress);
                                    UPDATE_COUNTERS;
                                    count--;
                                    case 1:
                                        Work = *(base + p);
                                        snes->ppu->S9xSetPPU(Work, 0x2101 + d->BAddress);
                                        UPDATE_COUNTERS;
                                        count--;
                                }
                        }
                        if (count == 1) {
                            Work = *(base + p);
                            snes->ppu->S9xSetPPU(Work, 0x2100 + d->BAddress);
                            UPDATE_COUNTERS;
                            b = 1;
                        } else
                            b = 0;
                    }
                } else if (d->TransferMode == 3 || d->TransferMode == 7) {
                    switch (b) {
                        default:
                            do {
                                Work = *(base + p);
                                snes->ppu->S9xSetPPU(Work, 0x2100 + d->BAddress);
                                UPDATE_COUNTERS;
                                if (--count <= 0) {
                                    b = 1;
                                    break;
                                }
                                case 1:
                                    Work = *(base + p);
                                    snes->ppu->S9xSetPPU(Work, 0x2100 + d->BAddress);
                                    UPDATE_COUNTERS;
                                    if (--count <= 0) {
                                        b = 2;
                                        break;
                                    }
                                case 2:
                                    Work = *(base + p);
                                    snes->ppu->S9xSetPPU(Work, 0x2101 + d->BAddress);
                                    UPDATE_COUNTERS;
                                    if (--count <= 0) {
                                        b = 3;
                                        break;
                                    }
                                case 3:
                                    Work = *(base + p);
                                    snes->ppu->S9xSetPPU(Work, 0x2101 + d->BAddress);
                                    UPDATE_COUNTERS;
                                    if (--count <= 0) {
                                        b = 0;
                                        break;
                                    }
                            } while (1);
                    }
                } else if (d->TransferMode == 4) {
                    switch (b) {
                        default:
                            do {
                                Work = *(base + p);
                                snes->ppu->S9xSetPPU(Work, 0x2100 + d->BAddress);
                                UPDATE_COUNTERS;
                                if (--count <= 0) {
                                    b = 1;
                                    break;
                                }
                                case 1:
                                    Work = *(base + p);
                                    snes->ppu->S9xSetPPU(Work, 0x2101 + d->BAddress);
                                    UPDATE_COUNTERS;
                                    if (--count <= 0) {
                                        b = 2;
                                        break;
                                    }
                                case 2:
                                    Work = *(base + p);
                                    snes->ppu->S9xSetPPU(Work, 0x2102 + d->BAddress);
                                    UPDATE_COUNTERS;
                                    if (--count <= 0) {
                                        b = 3;
                                        break;
                                    }
                                case 3:
                                    Work = *(base + p);
                                    snes->ppu->S9xSetPPU(Work, 0x2103 + d->BAddress);
                                    UPDATE_COUNTERS;
                                    if (--count <= 0) {
                                        b = 0;
                                        break;
                                    }
                            } while (1);
                    }
                }
            }
            if (rem <= 0)
                break;
            base       = snes->mem->S9xGetBasePointer((d->ABank << 16) + d->AAddress);
            count      = MEMMAP_BLOCK_SIZE;
            inWRAM_DMA = ((!in_sa1_dma && !in_sdd1_dma && !spc7110_dma) &&
                          (d->ABank == 0x7e || d->ABank == 0x7f || (!(d->ABank & 0x40) && d->AAddress < 0x2000)));
        }
#undef UPDATE_COUNTERS
    } else {


#define UPDATE_COUNTERS                                                                                                \
    d->DMACount_Or_HDMAIndirectAddress--;                                                                              \
    d->AAddress += inc;                                                                                                \
    if (!addCyclesInDMA(Channel)) {                                                                                    \
        snes->scpu->InDMA                   = FALSE;                                                                   \
        snes->scpu->InDMAorHDMA             = FALSE;                                                                   \
        snes->scpu->InWRAMDMAorHDMA         = FALSE;                                                                   \
        snes->scpu->CurrentDMAorHDMAChannel = -1;                                                                      \
        return (FALSE);                                                                                                \
    }


        if (d->BAddress > 0x80 - 4 && d->BAddress <= 0x83 && !(d->ABank & 0x40)) {
            do {
                switch (d->TransferMode) {
                    case 0:
                    case 2:
                    case 6:
                        snes->scpu->InWRAMDMAorHDMA = (d->AAddress < 0x2000);
                        Work                        = snes->ppu->S9xGetPPU(0x2100 + d->BAddress);
                        snes->mem->S9xSetByte(Work, (d->ABank << 16) + d->AAddress);
                        UPDATE_COUNTERS;
                        count--;
                        break;
                    case 1:
                    case 5:
                        snes->scpu->InWRAMDMAorHDMA = (d->AAddress < 0x2000);
                        Work                        = snes->ppu->S9xGetPPU(0x2100 + d->BAddress);
                        snes->mem->S9xSetByte(Work, (d->ABank << 16) + d->AAddress);
                        UPDATE_COUNTERS;
                        if (!--count)
                            break;
                        snes->scpu->InWRAMDMAorHDMA = (d->AAddress < 0x2000);
                        Work                        = snes->ppu->S9xGetPPU(0x2101 + d->BAddress);
                        snes->mem->S9xSetByte(Work, (d->ABank << 16) + d->AAddress);
                        UPDATE_COUNTERS;
                        count--;
                        break;
                    case 3:
                    case 7:
                        snes->scpu->InWRAMDMAorHDMA = (d->AAddress < 0x2000);
                        Work                        = snes->ppu->S9xGetPPU(0x2100 + d->BAddress);
                        snes->mem->S9xSetByte(Work, (d->ABank << 16) + d->AAddress);
                        UPDATE_COUNTERS;
                        if (!--count)
                            break;
                        snes->scpu->InWRAMDMAorHDMA = (d->AAddress < 0x2000);
                        Work                        = snes->ppu->S9xGetPPU(0x2100 + d->BAddress);
                        snes->mem->S9xSetByte(Work, (d->ABank << 16) + d->AAddress);
                        UPDATE_COUNTERS;
                        if (!--count)
                            break;
                        snes->scpu->InWRAMDMAorHDMA = (d->AAddress < 0x2000);
                        Work                        = snes->ppu->S9xGetPPU(0x2101 + d->BAddress);
                        snes->mem->S9xSetByte(Work, (d->ABank << 16) + d->AAddress);
                        UPDATE_COUNTERS;
                        if (!--count)
                            break;
                        snes->scpu->InWRAMDMAorHDMA = (d->AAddress < 0x2000);
                        Work                        = snes->ppu->S9xGetPPU(0x2101 + d->BAddress);
                        snes->mem->S9xSetByte(Work, (d->ABank << 16) + d->AAddress);
                        UPDATE_COUNTERS;
                        count--;
                        break;
                    case 4:
                        snes->scpu->InWRAMDMAorHDMA = (d->AAddress < 0x2000);
                        Work                        = snes->ppu->S9xGetPPU(0x2100 + d->BAddress);
                        snes->mem->S9xSetByte(Work, (d->ABank << 16) + d->AAddress);
                        UPDATE_COUNTERS;
                        if (!--count)
                            break;
                        snes->scpu->InWRAMDMAorHDMA = (d->AAddress < 0x2000);
                        Work                        = snes->ppu->S9xGetPPU(0x2101 + d->BAddress);
                        snes->mem->S9xSetByte(Work, (d->ABank << 16) + d->AAddress);
                        UPDATE_COUNTERS;
                        if (!--count)
                            break;
                        snes->scpu->InWRAMDMAorHDMA = (d->AAddress < 0x2000);
                        Work                        = snes->ppu->S9xGetPPU(0x2102 + d->BAddress);
                        snes->mem->S9xSetByte(Work, (d->ABank << 16) + d->AAddress);
                        UPDATE_COUNTERS;
                        if (!--count)
                            break;
                        snes->scpu->InWRAMDMAorHDMA = (d->AAddress < 0x2000);
                        Work                        = snes->ppu->S9xGetPPU(0x2103 + d->BAddress);
                        snes->mem->S9xSetByte(Work, (d->ABank << 16) + d->AAddress);
                        UPDATE_COUNTERS;
                        count--;
                        break;
                    default:
                        while (count) {
                            UPDATE_COUNTERS;
                            count--;
                        }
                        break;
                }
            } while (count);
        } else {
            snes->scpu->InWRAMDMAorHDMA = (d->ABank == 0x7e || d->ABank == 0x7f);
            do {
                switch (d->TransferMode) {
                    case 0:
                    case 2:
                    case 6:
                        Work = snes->ppu->S9xGetPPU(0x2100 + d->BAddress);
                        snes->mem->S9xSetByte(Work, (d->ABank << 16) + d->AAddress);
                        UPDATE_COUNTERS;
                        count--;
                        break;
                    case 1:
                    case 5:
                        Work = snes->ppu->S9xGetPPU(0x2100 + d->BAddress);
                        snes->mem->S9xSetByte(Work, (d->ABank << 16) + d->AAddress);
                        UPDATE_COUNTERS;
                        if (!--count)
                            break;
                        Work = snes->ppu->S9xGetPPU(0x2101 + d->BAddress);
                        snes->mem->S9xSetByte(Work, (d->ABank << 16) + d->AAddress);
                        UPDATE_COUNTERS;
                        count--;
                        break;
                    case 3:
                    case 7:
                        Work = snes->ppu->S9xGetPPU(0x2100 + d->BAddress);
                        snes->mem->S9xSetByte(Work, (d->ABank << 16) + d->AAddress);
                        UPDATE_COUNTERS;
                        if (!--count)
                            break;
                        Work = snes->ppu->S9xGetPPU(0x2100 + d->BAddress);
                        snes->mem->S9xSetByte(Work, (d->ABank << 16) + d->AAddress);
                        UPDATE_COUNTERS;
                        if (!--count)
                            break;
                        Work = snes->ppu->S9xGetPPU(0x2101 + d->BAddress);
                        snes->mem->S9xSetByte(Work, (d->ABank << 16) + d->AAddress);
                        UPDATE_COUNTERS;
                        if (!--count)
                            break;
                        Work = snes->ppu->S9xGetPPU(0x2101 + d->BAddress);
                        snes->mem->S9xSetByte(Work, (d->ABank << 16) + d->AAddress);
                        UPDATE_COUNTERS;
                        count--;
                        break;
                    case 4:
                        Work = snes->ppu->S9xGetPPU(0x2100 + d->BAddress);
                        snes->mem->S9xSetByte(Work, (d->ABank << 16) + d->AAddress);
                        UPDATE_COUNTERS;
                        if (!--count)
                            break;
                        Work = snes->ppu->S9xGetPPU(0x2101 + d->BAddress);
                        snes->mem->S9xSetByte(Work, (d->ABank << 16) + d->AAddress);
                        UPDATE_COUNTERS;
                        if (!--count)
                            break;
                        Work = snes->ppu->S9xGetPPU(0x2102 + d->BAddress);
                        snes->mem->S9xSetByte(Work, (d->ABank << 16) + d->AAddress);
                        UPDATE_COUNTERS;
                        if (!--count)
                            break;
                        Work = snes->ppu->S9xGetPPU(0x2103 + d->BAddress);
                        snes->mem->S9xSetByte(Work, (d->ABank << 16) + d->AAddress);
                        UPDATE_COUNTERS;
                        count--;
                        break;
                    default:
                        while (count) {
                            UPDATE_COUNTERS;
                            count--;
                        }
                        break;
                }
            } while (count);
        }
    }
    if (snes->scpu->NMIPending && (Timings.NMITriggerPos != 0xffff)) {
        Timings.NMITriggerPos = snes->scpu->Cycles + Timings.NMIDMADelay;
    }
    if (Settings.SPC7110) {
        if (spc7110_dma)
            delete[] spc7110_dma;
    }

    snes->scpu->InDMA                   = FALSE;
    snes->scpu->InDMAorHDMA             = FALSE;
    snes->scpu->InWRAMDMAorHDMA         = FALSE;
    snes->scpu->CurrentDMAorHDMAChannel = -1;
    return (TRUE);
}
bool8 SDMAS::HDMAReadLineCount(int d)
{
    uint8 line;
    line = snes->mem->S9xGetByte((DMA[d].ABank << 16) + DMA[d].Address);
    ADD_CYCLES(SLOW_ONE_CYCLE);
    if (!line) {
        DMA[d].Repeat    = FALSE;
        DMA[d].LineCount = 128;
        if (DMA[d].HDMAIndirectAddressing) {
            if (snes->ppu->HDMA & (0xfe << d)) {
                DMA[d].Address++;
                ADD_CYCLES(SLOW_ONE_CYCLE << 1);
            } else
                ADD_CYCLES(SLOW_ONE_CYCLE);
            DMA[d].DMACount_Or_HDMAIndirectAddress = snes->mem->S9xGetWord((DMA[d].ABank << 16) + DMA[d].Address);
            DMA[d].Address++;
        }
        DMA[d].Address++;
        HDMAMemPointers[d] = NULL;
        return (FALSE);
    } else if (line == 0x80) {
        DMA[d].Repeat    = TRUE;
        DMA[d].LineCount = 128;
    } else {
        DMA[d].Repeat    = !(line & 0x80);
        DMA[d].LineCount = line & 0x7f;
    }
    DMA[d].Address++;
    DMA[d].DoTransfer = TRUE;
    if (DMA[d].HDMAIndirectAddressing) {
        ADD_CYCLES(SLOW_ONE_CYCLE << 1);
        DMA[d].DMACount_Or_HDMAIndirectAddress = snes->mem->S9xGetWord((DMA[d].ABank << 16) + DMA[d].Address);
        DMA[d].Address += 2;
        HDMAMemPointers[d] =
            snes->mem->S9xGetMemPointer((DMA[d].IndirectBank << 16) + DMA[d].DMACount_Or_HDMAIndirectAddress);
    } else
        HDMAMemPointers[d] = snes->mem->S9xGetMemPointer((DMA[d].ABank << 16) + DMA[d].Address);
    return (TRUE);
}
void SDMAS::S9xStartHDMA(void)
{
    snes->ppu->HDMA      = snes->mem->FillRAM[0x420c];
    snes->ppu->HDMAEnded = 0;
    int32 tmpch;
    snes->scpu->InHDMA      = TRUE;
    snes->scpu->InDMAorHDMA = TRUE;
    tmpch                   = snes->scpu->CurrentDMAorHDMAChannel;
    if (snes->ppu->HDMA != 0)
        ADD_CYCLES(Timings.DMACPUSync);
    for (uint8 i = 0; i < 8; i++) {
        if (snes->ppu->HDMA & (1 << i)) {
            snes->scpu->CurrentDMAorHDMAChannel = i;
            DMA[i].Address                      = DMA[i].AAddress;
            if (!HDMAReadLineCount(i)) {
                snes->ppu->HDMA &= ~(1 << i);
                snes->ppu->HDMAEnded |= (1 << i);
            }
        } else
            DMA[i].DoTransfer = FALSE;
    }
    snes->scpu->InHDMA                  = FALSE;
    snes->scpu->InDMAorHDMA             = snes->scpu->InDMA;
    snes->scpu->HDMARanInDMA            = snes->scpu->InDMA ? snes->ppu->HDMA : 0;
    snes->scpu->CurrentDMAorHDMAChannel = tmpch;
}
uint8 SDMAS::S9xDoHDMA(uint8 byte)
{
    struct SDMA *p;
    uint32       ShiftedIBank;
    uint16       IAddr;
    bool8        temp;
    int32        tmpch;
    int          d;
    uint8        mask;
    snes->scpu->InHDMA       = TRUE;
    snes->scpu->InDMAorHDMA  = TRUE;
    snes->scpu->HDMARanInDMA = snes->scpu->InDMA ? byte : 0;
    temp                     = snes->scpu->InWRAMDMAorHDMA;
    tmpch                    = snes->scpu->CurrentDMAorHDMAChannel;
    ADD_CYCLES(Timings.DMACPUSync);
    for (mask = 1, p = &DMA[0], d = 0; mask; mask <<= 1, p++, d++) {
        if (byte & mask) {
            snes->scpu->InWRAMDMAorHDMA         = FALSE;
            snes->scpu->CurrentDMAorHDMAChannel = d;
            if (p->HDMAIndirectAddressing) {
                ShiftedIBank = (p->IndirectBank << 16);
                IAddr        = p->DMACount_Or_HDMAIndirectAddress;
            } else {
                ShiftedIBank = (p->ABank << 16);
                IAddr        = p->Address;
            }
            if (!HDMAMemPointers[d])
                HDMAMemPointers[d] = snes->mem->S9xGetMemPointer(ShiftedIBank + IAddr);
            if (p->DoTransfer) {
                if (p->BAddress == 0x04) {
                    if (SNESGameFixes.Uniracers) {
                        snes->ppu->OAMAddr = 0x10c;
                        snes->ppu->OAMFlip = 0;
                    }
                }

                if (!p->ReverseTransfer) {
                    if ((IAddr & MEMMAP_MASK) + HDMA_ModeByteCounts[p->TransferMode] >= MEMMAP_BLOCK_SIZE) {
                        HDMAMemPointers[d] = NULL;


#define DOBYTE(Addr, RegOff)                                                                                           \
    snes->scpu->InWRAMDMAorHDMA = (ShiftedIBank == 0x7e0000 || ShiftedIBank == 0x7f0000 ||                             \
                                   (!(ShiftedIBank & 0x400000) && ((uint16)(Addr)) < 0x2000));                         \
    snes->ppu->S9xSetPPU(snes->mem->S9xGetByte(ShiftedIBank + ((uint16)(Addr))), 0x2100 + p->BAddress + (RegOff));


                        switch (p->TransferMode) {
                            case 0:
                                DOBYTE(IAddr, 0);
                                ADD_CYCLES(SLOW_ONE_CYCLE);
                                break;
                            case 5:
                                DOBYTE(IAddr + 0, 0);
                                ADD_CYCLES(SLOW_ONE_CYCLE);
                                DOBYTE(IAddr + 1, 1);
                                ADD_CYCLES(SLOW_ONE_CYCLE);
                                DOBYTE(IAddr + 2, 0);
                                ADD_CYCLES(SLOW_ONE_CYCLE);
                                DOBYTE(IAddr + 3, 1);
                                ADD_CYCLES(SLOW_ONE_CYCLE);
                                break;
                            case 1:
                                DOBYTE(IAddr + 0, 0);
                                ADD_CYCLES(SLOW_ONE_CYCLE);
                                DOBYTE(IAddr + 1, 1);
                                ADD_CYCLES(SLOW_ONE_CYCLE);
                                break;
                            case 2:
                            case 6:
                                DOBYTE(IAddr + 0, 0);
                                ADD_CYCLES(SLOW_ONE_CYCLE);
                                DOBYTE(IAddr + 1, 0);
                                ADD_CYCLES(SLOW_ONE_CYCLE);
                                break;
                            case 3:
                            case 7:
                                DOBYTE(IAddr + 0, 0);
                                ADD_CYCLES(SLOW_ONE_CYCLE);
                                DOBYTE(IAddr + 1, 0);
                                ADD_CYCLES(SLOW_ONE_CYCLE);
                                DOBYTE(IAddr + 2, 1);
                                ADD_CYCLES(SLOW_ONE_CYCLE);
                                DOBYTE(IAddr + 3, 1);
                                ADD_CYCLES(SLOW_ONE_CYCLE);
                                break;
                            case 4:
                                DOBYTE(IAddr + 0, 0);
                                ADD_CYCLES(SLOW_ONE_CYCLE);
                                DOBYTE(IAddr + 1, 1);
                                ADD_CYCLES(SLOW_ONE_CYCLE);
                                DOBYTE(IAddr + 2, 2);
                                ADD_CYCLES(SLOW_ONE_CYCLE);
                                DOBYTE(IAddr + 3, 3);
                                ADD_CYCLES(SLOW_ONE_CYCLE);
                                break;
                        }
#undef DOBYTE
                    } else {
                        snes->scpu->InWRAMDMAorHDMA = (ShiftedIBank == 0x7e0000 || ShiftedIBank == 0x7f0000 ||
                                                       (!(ShiftedIBank & 0x400000) && IAddr < 0x2000));
                        if (!HDMAMemPointers[d]) {
                            uint32 Addr = ShiftedIBank + IAddr;
                            switch (p->TransferMode) {
                                case 0:
                                    snes->ppu->S9xSetPPU(snes->mem->S9xGetByte(Addr), 0x2100 + p->BAddress);
                                    ADD_CYCLES(SLOW_ONE_CYCLE);
                                    break;
                                case 5:
                                    snes->ppu->S9xSetPPU(snes->mem->S9xGetByte(Addr + 0), 0x2100 + p->BAddress);
                                    ADD_CYCLES(SLOW_ONE_CYCLE);
                                    snes->ppu->S9xSetPPU(snes->mem->S9xGetByte(Addr + 1), 0x2101 + p->BAddress);
                                    ADD_CYCLES(SLOW_ONE_CYCLE);
                                    Addr += 2;
                                case 1:
                                    snes->ppu->S9xSetPPU(snes->mem->S9xGetByte(Addr + 0), 0x2100 + p->BAddress);
                                    ADD_CYCLES(SLOW_ONE_CYCLE);
                                    snes->ppu->S9xSetPPU(snes->mem->S9xGetByte(Addr + 1), 0x2101 + p->BAddress);
                                    ADD_CYCLES(SLOW_ONE_CYCLE);
                                    break;
                                case 2:
                                case 6:
                                    snes->ppu->S9xSetPPU(snes->mem->S9xGetByte(Addr + 0), 0x2100 + p->BAddress);
                                    ADD_CYCLES(SLOW_ONE_CYCLE);
                                    snes->ppu->S9xSetPPU(snes->mem->S9xGetByte(Addr + 1), 0x2100 + p->BAddress);
                                    ADD_CYCLES(SLOW_ONE_CYCLE);
                                    break;
                                case 3:
                                case 7:
                                    snes->ppu->S9xSetPPU(snes->mem->S9xGetByte(Addr + 0), 0x2100 + p->BAddress);
                                    ADD_CYCLES(SLOW_ONE_CYCLE);
                                    snes->ppu->S9xSetPPU(snes->mem->S9xGetByte(Addr + 1), 0x2100 + p->BAddress);
                                    ADD_CYCLES(SLOW_ONE_CYCLE);
                                    snes->ppu->S9xSetPPU(snes->mem->S9xGetByte(Addr + 2), 0x2101 + p->BAddress);
                                    ADD_CYCLES(SLOW_ONE_CYCLE);
                                    snes->ppu->S9xSetPPU(snes->mem->S9xGetByte(Addr + 3), 0x2101 + p->BAddress);
                                    ADD_CYCLES(SLOW_ONE_CYCLE);
                                    break;
                                case 4:
                                    snes->ppu->S9xSetPPU(snes->mem->S9xGetByte(Addr + 0), 0x2100 + p->BAddress);
                                    ADD_CYCLES(SLOW_ONE_CYCLE);
                                    snes->ppu->S9xSetPPU(snes->mem->S9xGetByte(Addr + 1), 0x2101 + p->BAddress);
                                    ADD_CYCLES(SLOW_ONE_CYCLE);
                                    snes->ppu->S9xSetPPU(snes->mem->S9xGetByte(Addr + 2), 0x2102 + p->BAddress);
                                    ADD_CYCLES(SLOW_ONE_CYCLE);
                                    snes->ppu->S9xSetPPU(snes->mem->S9xGetByte(Addr + 3), 0x2103 + p->BAddress);
                                    ADD_CYCLES(SLOW_ONE_CYCLE);
                                    break;
                            }
                        } else {
                            switch (p->TransferMode) {
                                case 0:
                                    snes->ppu->S9xSetPPU(*HDMAMemPointers[d]++, 0x2100 + p->BAddress);
                                    ADD_CYCLES(SLOW_ONE_CYCLE);
                                    break;
                                case 5:
                                    snes->ppu->S9xSetPPU(*(HDMAMemPointers[d] + 0), 0x2100 + p->BAddress);
                                    ADD_CYCLES(SLOW_ONE_CYCLE);
                                    snes->ppu->S9xSetPPU(*(HDMAMemPointers[d] + 1), 0x2101 + p->BAddress);
                                    ADD_CYCLES(SLOW_ONE_CYCLE);
                                    HDMAMemPointers[d] += 2;
                                case 1:
                                    snes->ppu->S9xSetPPU(*(HDMAMemPointers[d] + 0), 0x2100 + p->BAddress);
                                    ADD_CYCLES(SLOW_ONE_CYCLE);
                                    snes->OpenBus = *(HDMAMemPointers[d] + 1);
                                    snes->ppu->S9xSetPPU(snes->OpenBus, 0x2101 + p->BAddress);
                                    ADD_CYCLES(SLOW_ONE_CYCLE);
                                    HDMAMemPointers[d] += 2;
                                    break;
                                case 2:
                                case 6:
                                    snes->ppu->S9xSetPPU(*(HDMAMemPointers[d] + 0), 0x2100 + p->BAddress);
                                    ADD_CYCLES(SLOW_ONE_CYCLE);
                                    snes->ppu->S9xSetPPU(*(HDMAMemPointers[d] + 1), 0x2100 + p->BAddress);
                                    ADD_CYCLES(SLOW_ONE_CYCLE);
                                    HDMAMemPointers[d] += 2;
                                    break;
                                case 3:
                                case 7:
                                    snes->ppu->S9xSetPPU(*(HDMAMemPointers[d] + 0), 0x2100 + p->BAddress);
                                    ADD_CYCLES(SLOW_ONE_CYCLE);
                                    snes->ppu->S9xSetPPU(*(HDMAMemPointers[d] + 1), 0x2100 + p->BAddress);
                                    ADD_CYCLES(SLOW_ONE_CYCLE);
                                    snes->ppu->S9xSetPPU(*(HDMAMemPointers[d] + 2), 0x2101 + p->BAddress);
                                    ADD_CYCLES(SLOW_ONE_CYCLE);
                                    snes->ppu->S9xSetPPU(*(HDMAMemPointers[d] + 3), 0x2101 + p->BAddress);
                                    ADD_CYCLES(SLOW_ONE_CYCLE);
                                    HDMAMemPointers[d] += 4;
                                    break;
                                case 4:
                                    snes->ppu->S9xSetPPU(*(HDMAMemPointers[d] + 0), 0x2100 + p->BAddress);
                                    ADD_CYCLES(SLOW_ONE_CYCLE);
                                    snes->ppu->S9xSetPPU(*(HDMAMemPointers[d] + 1), 0x2101 + p->BAddress);
                                    ADD_CYCLES(SLOW_ONE_CYCLE);
                                    snes->ppu->S9xSetPPU(*(HDMAMemPointers[d] + 2), 0x2102 + p->BAddress);
                                    ADD_CYCLES(SLOW_ONE_CYCLE);
                                    snes->ppu->S9xSetPPU(*(HDMAMemPointers[d] + 3), 0x2103 + p->BAddress);
                                    ADD_CYCLES(SLOW_ONE_CYCLE);
                                    HDMAMemPointers[d] += 4;
                                    break;
                            }
                        }
                    }
                } else {
                    HDMAMemPointers[d] = NULL;


#define DOBYTE(Addr, RegOff)                                                                                           \
    snes->scpu->InWRAMDMAorHDMA = (ShiftedIBank == 0x7e0000 || ShiftedIBank == 0x7f0000 ||                             \
                                   (!(ShiftedIBank & 0x400000) && ((uint16)(Addr)) < 0x2000));                         \
    snes->mem->S9xSetByte(snes->ppu->S9xGetPPU(0x2100 + p->BAddress + (RegOff)), ShiftedIBank + ((uint16)(Addr)));



                    switch (p->TransferMode) {
                        case 0:
                            DOBYTE(IAddr, 0);
                            ADD_CYCLES(SLOW_ONE_CYCLE);
                            break;
                        case 5:
                            DOBYTE(IAddr + 0, 0);
                            ADD_CYCLES(SLOW_ONE_CYCLE);
                            DOBYTE(IAddr + 1, 1);
                            ADD_CYCLES(SLOW_ONE_CYCLE);
                            DOBYTE(IAddr + 2, 0);
                            ADD_CYCLES(SLOW_ONE_CYCLE);
                            DOBYTE(IAddr + 3, 1);
                            ADD_CYCLES(SLOW_ONE_CYCLE);
                            break;
                        case 1:
                            DOBYTE(IAddr + 0, 0);
                            ADD_CYCLES(SLOW_ONE_CYCLE);
                            DOBYTE(IAddr + 1, 1);
                            ADD_CYCLES(SLOW_ONE_CYCLE);
                            break;
                        case 2:
                        case 6:
                            DOBYTE(IAddr + 0, 0);
                            ADD_CYCLES(SLOW_ONE_CYCLE);
                            DOBYTE(IAddr + 1, 0);
                            ADD_CYCLES(SLOW_ONE_CYCLE);
                            break;
                        case 3:
                        case 7:
                            DOBYTE(IAddr + 0, 0);
                            ADD_CYCLES(SLOW_ONE_CYCLE);
                            DOBYTE(IAddr + 1, 0);
                            ADD_CYCLES(SLOW_ONE_CYCLE);
                            DOBYTE(IAddr + 2, 1);
                            ADD_CYCLES(SLOW_ONE_CYCLE);
                            DOBYTE(IAddr + 3, 1);
                            ADD_CYCLES(SLOW_ONE_CYCLE);
                            break;
                        case 4:
                            DOBYTE(IAddr + 0, 0);
                            ADD_CYCLES(SLOW_ONE_CYCLE);
                            DOBYTE(IAddr + 1, 1);
                            ADD_CYCLES(SLOW_ONE_CYCLE);
                            DOBYTE(IAddr + 2, 2);
                            ADD_CYCLES(SLOW_ONE_CYCLE);
                            DOBYTE(IAddr + 3, 3);
                            ADD_CYCLES(SLOW_ONE_CYCLE);
                            break;
                    }
#undef DOBYTE
                }
            }
        }
    }
    for (mask = 1, p = &DMA[0], d = 0; mask; mask <<= 1, p++, d++) {
        if (byte & mask) {
            if (p->DoTransfer) {
                if (p->HDMAIndirectAddressing)
                    p->DMACount_Or_HDMAIndirectAddress += HDMA_ModeByteCounts[p->TransferMode];
                else
                    p->Address += HDMA_ModeByteCounts[p->TransferMode];
            }
            p->DoTransfer = !p->Repeat;
            if (!--p->LineCount) {
                if (!HDMAReadLineCount(d)) {
                    byte &= ~mask;
                    snes->ppu->HDMAEnded |= mask;
                    p->DoTransfer = FALSE;
                }
            } else
                ADD_CYCLES(SLOW_ONE_CYCLE);
        }
    }
    snes->scpu->InHDMA                  = FALSE;
    snes->scpu->InDMAorHDMA             = snes->scpu->InDMA;
    snes->scpu->InWRAMDMAorHDMA         = temp;
    snes->scpu->CurrentDMAorHDMAChannel = tmpch;
    return (byte);
}
void SDMAS::S9xResetDMA(void)
{
    for (int d = 0; d < 8; d++) {
        DMA[d].ReverseTransfer                 = TRUE;
        DMA[d].HDMAIndirectAddressing          = TRUE;
        DMA[d].AAddressFixed                   = TRUE;
        DMA[d].AAddressDecrement               = TRUE;
        DMA[d].TransferMode                    = 7;
        DMA[d].BAddress                        = 0xff;
        DMA[d].AAddress                        = 0xffff;
        DMA[d].ABank                           = 0xff;
        DMA[d].DMACount_Or_HDMAIndirectAddress = 0xffff;
        DMA[d].IndirectBank                    = 0xff;
        DMA[d].Address                         = 0xffff;
        DMA[d].Repeat                          = FALSE;
        DMA[d].LineCount                       = 0x7f;
        DMA[d].UnknownByte                     = 0xff;
        DMA[d].DoTransfer                      = FALSE;
        DMA[d].UnusedBit43x0                   = 1;
    }
}
