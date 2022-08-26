#include "cpu.h"
#include "snes.h"
#include "dma.h"
#include "apu/apu.h"
#include "mem.h"
#include "ppu.h"
#include <cstdio>


#define CALL_MEMBER_FN(object, ptrToMember) ((object).*(ptrToMember))


#define PushW(w)                                                                                                       \
    snes->mem->S9xSetWord(w, Registers.S.W - 1, WRAP_BANK, WRITE_10);                                                  \
    Registers.S.W -= 2;
#define PushWE(w)                                                                                                      \
    Registers.SL--;                                                                                                    \
    snes->mem->S9xSetWord(w, Registers.S.W, WRAP_PAGE, WRITE_10);                                                      \
    Registers.SL--;
#define PushBE(b)                                                                                                      \
    snes->mem->S9xSetByte(b, Registers.S.W);                                                                           \
    Registers.SL--;
#define PullW(w)                                                                                                       \
    w = snes->mem->S9xGetWord(Registers.S.W + 1, WRAP_BANK);                                                           \
    Registers.S.W += 2;
#define PullWE(w)                                                                                                      \
    Registers.SL++;                                                                                                    \
    w = snes->mem->S9xGetWord(Registers.S.W, WRAP_PAGE);                                                               \
    Registers.SL++;
#define PullB(b) b = snes->mem->S9xGetByte(++Registers.S.W);
#define PullBE(b)                                                                                                      \
    Registers.SL++;                                                                                                    \
    b = snes->mem->S9xGetByte(Registers.S.W);
#define AddCycles(n)                                                                                                   \
    {                                                                                                                  \
        Cycles += (n);                                                                                                 \
        while (Cycles >= NextEvent)                                                                                    \
            S9xDoHEventProcessing();                                                                                   \
    }
#define PushB(b) snes->mem->S9xSetByte(b, Registers.S.W--);

//

SCPUState::SCPUState(SNESX *_snes)
{
    snes = _snes;
    create_op_table();
}
void SCPUState::S9xResetCPU(void)
{
    S9xSoftResetCPU();
    Registers.SL  = 0xff;
    Registers.P.W = 0;
    Registers.A.W = 0;
    Registers.X.W = 0;
    Registers.Y.W = 0;
    SetFlags(MemoryFlag | IndexFlag | IRQ | Emulation);
    ClearFlags(Decimal);
}
void SCPUState::S9xSoftResetCPU(void)
{
    Cycles                  = 182;
    PrevCycles              = Cycles;
    V_Counter               = 0;
    Flags                   = Flags & (DEBUG_MODE_FLAG | TRACE_FLAG);
    PCBase                  = NULL;
    NMIPending              = FALSE;
    IRQLine                 = FALSE;
    IRQTransition           = FALSE;
    IRQExternal             = FALSE;
    MemSpeed                = SLOW_ONE_CYCLE;
    MemSpeedx2              = SLOW_ONE_CYCLE * 2;
    FastROMSpeed            = SLOW_ONE_CYCLE;
    InDMA                   = FALSE;
    InHDMA                  = FALSE;
    InDMAorHDMA             = FALSE;
    InWRAMDMAorHDMA         = FALSE;
    HDMARanInDMA            = 0;
    CurrentDMAorHDMAChannel = -1;
    WhichEvent              = HC_RENDER_EVENT;
    NextEvent               = Timings.RenderPos;
    WaitingForInterrupt     = FALSE;
    AutoSaveTimer           = 0;
    SRAMModified            = FALSE;
    Registers.PBPC          = 0;
    Registers.PB            = 0;
    Registers.PCw           = snes->mem->S9xGetWord(0xfffc);
    snes->OpenBus                 = Registers.PCh;
    Registers.D.W           = 0;
    Registers.DB            = 0;
    Registers.SH            = 1;
    Registers.SL -= 3;
    Registers.XH   = 0;
    Registers.YH   = 0;
    ICPU.ShiftedPB = 0;
    ICPU.ShiftedDB = 0;
    SetFlags(MemoryFlag | IndexFlag | IRQ | Emulation);
    ClearFlags(Decimal);
    Timings.InterlaceField  = FALSE;
    Timings.H_Max           = Timings.H_Max_Master;
    Timings.V_Max           = Timings.V_Max_Master;
    Timings.NMITriggerPos   = 0xffff;
    Timings.NextIRQTimer    = 0x0fffffff;
    Timings.IRQFlagChanging = IRQ_NONE;
    if (snes->Model->_5A22 == 2)
        Timings.WRAMRefreshPos = SNES_WRAM_REFRESH_HC_v2;
    else
        Timings.WRAMRefreshPos = SNES_WRAM_REFRESH_HC_v1;
    snes->mem->S9xSetPCBase(Registers.PBPC);
    ICPU.S9xOpcodes   = &S9xOpcodesE1;
    ICPU.S9xOpLengths = S9xOpLengthsM1X1;
    snes->mem->S9xUnpackStatus();
}
void SCPUState::S9xReset(void)
{
    memset(snes->mem->RAM, 0x55, 0x20000);
    memset(snes->mem->VRAM, 0x00, 0x10000);
    memset(snes->mem->FillRAM, 0, 0x8000);
    S9xResetCPU();
    snes->ppu->S9xResetPPU();
    snes->dma->S9xResetDMA();
    S9xResetAPU();
    S9xResetMSU();
}
void SCPUState::S9xSoftReset(void)
{
    memset(snes->mem->FillRAM, 0, 0x8000);
    S9xSoftResetCPU();
    snes->ppu->S9xSoftResetPPU();
    snes->dma->S9xResetDMA();
    S9xSoftResetAPU();
    S9xResetMSU();
}
void SCPUState::S9xMainLoop(void)
{
    if (Flags & SCAN_KEYS_FLAG) {
        Flags &= ~SCAN_KEYS_FLAG;
    }
    for (;;) {
        if (NMIPending) {
            if (Timings.NMITriggerPos <= Cycles) {
                NMIPending            = FALSE;
                Timings.NMITriggerPos = 0xffff;
                if (WaitingForInterrupt) {
                    WaitingForInterrupt = FALSE;
                    Registers.PCw++;
                    Cycles += TWO_CYCLES + ONE_DOT_CYCLE / 2;
                    while (Cycles >= NextEvent)
                        S9xDoHEventProcessing();
                }
                CHECK_FOR_IRQ_CHANGE();
                S9xOpcode_NMI();
            }
        }
        if (Cycles >= Timings.NextIRQTimer) {
            snes->ppu->S9xUpdateIRQPositions(false);
            IRQLine = TRUE;
        }
        if (IRQLine || IRQExternal) {
            if (WaitingForInterrupt) {
                WaitingForInterrupt = FALSE;
                Registers.PCw++;
                Cycles += TWO_CYCLES + ONE_DOT_CYCLE / 2;
                while (Cycles >= NextEvent)
                    S9xDoHEventProcessing();
            }
            if (!CheckFlag(IRQ)) {
                CHECK_FOR_IRQ_CHANGE();
                S9xOpcode_IRQ();
            }
        }
        CHECK_FOR_IRQ_CHANGE();
        if (Flags & SCAN_KEYS_FLAG) {
            snes->S9xSyncSpeed();
            break;
        }
        uint8                  Op;
        std::vector<op_hndls> *Opcodes;
        if (PCBase) {
            Op = PCBase[Registers.PCw];
            Cycles += MemSpeed;
            Opcodes = ICPU.S9xOpcodes;
        } else {
            Op      = snes->mem->S9xGetByte(Registers.PBPC);
            snes->OpenBus = Op;
            Opcodes = &S9xOpcodesSlow;
        }
        if ((Registers.PCw & MEMMAP_MASK) + ICPU.S9xOpLengths[Op] >= MEMMAP_BLOCK_SIZE) {
            uint8 *oldPCBase = PCBase;
            PCBase           = snes->mem->S9xGetBasePointer(ICPU.ShiftedPB + ((uint16)(Registers.PCw + 4)));
            if (oldPCBase != PCBase || (Registers.PCw & ~MEMMAP_MASK) == (0xffff & ~MEMMAP_MASK))
                Opcodes = &S9xOpcodesSlow;
        }
        Registers.PCw++;
        auto _Opcodes = *Opcodes;
        CALL_MEMBER_FN(*this, _Opcodes[Op].opcode_handler)();
    }
    snes->mem->S9xPackStatus();
}
bool SCPUState::CheckEmulation(void)
{
    return Registers.P.W & Emulation;
}
void SCPUState::S9xFixCycles(void)
{
    if (CheckEmulation()) {
        ICPU.S9xOpcodes   = &S9xOpcodesE1;
        ICPU.S9xOpLengths = S9xOpLengthsM1X1;
    } else if (CheckMemory()) {
        if (CheckIndex()) {
            ICPU.S9xOpcodes   = &S9xOpcodesM1X1;
            ICPU.S9xOpLengths = S9xOpLengthsM1X1;
        } else {
            ICPU.S9xOpcodes   = &S9xOpcodesM1X0;
            ICPU.S9xOpLengths = S9xOpLengthsM1X0;
        }
    } else {
        if (CheckIndex()) {
            ICPU.S9xOpcodes   = &S9xOpcodesM0X1;
            ICPU.S9xOpLengths = S9xOpLengthsM0X1;
        } else {
            ICPU.S9xOpcodes   = &S9xOpcodesM0X0;
            ICPU.S9xOpLengths = S9xOpLengthsM0X0;
        }
    }
}
void SCPUState::S9xReschedule(void)
{
    switch (WhichEvent) {
        case HC_HBLANK_START_EVENT:
            WhichEvent = HC_HDMA_START_EVENT;
            NextEvent  = Timings.HDMAStart;
            break;
        case HC_HDMA_START_EVENT:
            WhichEvent = HC_HCOUNTER_MAX_EVENT;
            NextEvent  = Timings.H_Max;
            break;
        case HC_HCOUNTER_MAX_EVENT:
            WhichEvent = HC_HDMA_INIT_EVENT;
            NextEvent  = Timings.HDMAInit;
            break;
        case HC_HDMA_INIT_EVENT:
            WhichEvent = HC_RENDER_EVENT;
            NextEvent  = Timings.RenderPos;
            break;
        case HC_RENDER_EVENT:
            WhichEvent = HC_WRAM_REFRESH_EVENT;
            NextEvent  = Timings.WRAMRefreshPos;
            break;
        case HC_WRAM_REFRESH_EVENT:
            WhichEvent = HC_HBLANK_START_EVENT;
            NextEvent  = Timings.HBlankStart;
            break;
    }
}
void SCPUState::S9xDoHEventProcessing(void)
{
    switch (WhichEvent) {
        case HC_HBLANK_START_EVENT:
            S9xReschedule();
            break;
        case HC_HDMA_START_EVENT:
            S9xReschedule();
            if (snes->ppu->HDMA && V_Counter <= snes->ppu->ScreenHeight) {
                snes->ppu->HDMA = snes->dma->S9xDoHDMA(snes->ppu->HDMA);
            }
            break;
        case HC_HCOUNTER_MAX_EVENT:
            S9xAPUEndScanline();
            Cycles -= Timings.H_Max;
            if (Timings.NMITriggerPos != 0xffff)
                Timings.NMITriggerPos -= Timings.H_Max;
            if (Timings.NextIRQTimer != 0x0fffffff)
                Timings.NextIRQTimer -= Timings.H_Max;
            S9xAPUSetReferenceTime(Cycles);
            V_Counter++;
            if (V_Counter >= Timings.V_Max) {
                V_Counter = 0;
                Timings.InterlaceField ^= 1;
                if (snes->ppu->IPPU.Interlace && !Timings.InterlaceField)
                    Timings.V_Max = Timings.V_Max_Master + 1;
                else
                    Timings.V_Max = Timings.V_Max_Master;
                snes->mem->FillRAM[0x213F] ^= 0x80;
                snes->ppu->RangeTimeOver   = 0;
                snes->mem->FillRAM[0x4210] = snes->Model->_5A22;
                ICPU.Frame++;
                snes->ppu->HVBeamCounterLatched = 0;
            }
            if (V_Counter == 240 && !snes->ppu->IPPU.Interlace && Timings.InterlaceField)
                Timings.H_Max = Timings.H_Max_Master - ONE_DOT_CYCLE;
            else
                Timings.H_Max = Timings.H_Max_Master;
            if (snes->Model->_5A22 == 2) {
                if (V_Counter != 240 || snes->ppu->IPPU.Interlace || !Timings.InterlaceField) {
                    if (Timings.WRAMRefreshPos == SNES_WRAM_REFRESH_HC_v2 - ONE_DOT_CYCLE)
                        Timings.WRAMRefreshPos = SNES_WRAM_REFRESH_HC_v2;
                    else
                        Timings.WRAMRefreshPos = SNES_WRAM_REFRESH_HC_v2 - ONE_DOT_CYCLE;
                }
            } else
                Timings.WRAMRefreshPos = SNES_WRAM_REFRESH_HC_v1;
            if (V_Counter == snes->ppu->ScreenHeight + FIRST_VISIBLE_LINE) {
                snes->renderer->S9xEndScreenRefresh();
                Flags |= SCAN_KEYS_FLAG;
                snes->ppu->HDMA               = 0;
                snes->ppu->IPPU.MaxBrightness = snes->ppu->Brightness;
                snes->ppu->ForcedBlanking     = (snes->mem->FillRAM[0x2100] >> 7) & 1;
                if (!snes->ppu->ForcedBlanking) {
                    snes->ppu->OAMAddr = snes->ppu->SavedOAMAddr;
                    uint8 tmp          = 0;
                    if (snes->ppu->OAMPriorityRotation)
                        tmp = (snes->ppu->OAMAddr & 0xFE) >> 1;
                    if ((snes->ppu->OAMFlip & 1) || snes->ppu->FirstSprite != tmp) {
                        snes->ppu->FirstSprite     = tmp;
                        snes->ppu->IPPU.OBJChanged = TRUE;
                    }
                    snes->ppu->OAMFlip = 0;
                }
                snes->mem->FillRAM[0x4210] = 0x80 | snes->Model->_5A22;
                if (snes->mem->FillRAM[0x4200] & 0x80) {
                    NMIPending            = TRUE;
                    Timings.NMITriggerPos = 6 + 6;
                }
            }
            if (V_Counter == snes->ppu->ScreenHeight + 3) {
            }
            if (V_Counter == FIRST_VISIBLE_LINE)
                snes->renderer->S9xStartScreenRefresh();

            S9xReschedule();
            break;
        case HC_HDMA_INIT_EVENT:
            S9xReschedule();
            if (V_Counter == 0) {
                snes->dma->S9xStartHDMA();
            }
            break;
        case HC_RENDER_EVENT:
            if (V_Counter >= FIRST_VISIBLE_LINE && V_Counter <= snes->ppu->ScreenHeight)
                snes->renderer->RenderLine((uint8)(V_Counter - FIRST_VISIBLE_LINE));
            S9xReschedule();
            break;
        case HC_WRAM_REFRESH_EVENT:
            Cycles += SNES_WRAM_REFRESH_CYCLES;
            S9xReschedule();
            break;
    }
}
void SCPUState::S9xOpcode_IRQ(void)
{
    AddCycles(MemSpeed + ONE_CYCLE);
    if (!CheckEmulation()) {
        PushB(Registers.PB);
        PushW(Registers.PCw);
        snes->mem->S9xPackStatus();
        PushB(Registers.PL);
        snes->OpenBus = Registers.PL;
        ClearDecimal();
        SetIRQ();
        if (Settings.SA1 && (snes->mem->FillRAM[0x2209] & 0x40)) {
            snes->OpenBus = snes->mem->FillRAM[0x220f];
            AddCycles(2 * ONE_CYCLE);
            snes->mem->S9xSetPCBase(snes->mem->FillRAM[0x220e] | (snes->mem->FillRAM[0x220f] << 8));
        } else {
            uint16 addr = snes->mem->S9xGetWord(0xFFEE);
            snes->OpenBus     = addr >> 8;
            snes->mem->S9xSetPCBase(addr);
        }
    } else {
        PushWE(Registers.PCw);
        snes->mem->S9xPackStatus();
        PushBE(Registers.PL);
        snes->OpenBus = Registers.PL;
        ClearDecimal();
        SetIRQ();
        if (Settings.SA1 && (snes->mem->FillRAM[0x2209] & 0x40)) {
            snes->OpenBus = snes->mem->FillRAM[0x220f];
            AddCycles(2 * ONE_CYCLE);
            snes->mem->S9xSetPCBase(snes->mem->FillRAM[0x220e] | (snes->mem->FillRAM[0x220f] << 8));
        } else {
            uint16 addr = snes->mem->S9xGetWord(0xFFFE);
            snes->OpenBus     = addr >> 8;
            snes->mem->S9xSetPCBase(addr);
        }
    }
}
void SCPUState::S9xOpcode_NMI(void)
{
    AddCycles(MemSpeed + ONE_CYCLE);
    if (!CheckEmulation()) {
        PushB(Registers.PB);
        PushW(Registers.PCw);
        snes->mem->S9xPackStatus();
        PushB(Registers.PL);
        snes->OpenBus = Registers.PL;
        ClearDecimal();
        SetIRQ();
        if (Settings.SA1 && (snes->mem->FillRAM[0x2209] & 0x10)) {
            snes->OpenBus = snes->mem->FillRAM[0x220d];
            AddCycles(2 * ONE_CYCLE);
            snes->mem->S9xSetPCBase(snes->mem->FillRAM[0x220c] | (snes->mem->FillRAM[0x220d] << 8));
        } else {
            uint16 addr = snes->mem->S9xGetWord(0xFFEA);
            snes->OpenBus     = addr >> 8;
            snes->mem->S9xSetPCBase(addr);
        }
    } else {
        PushWE(Registers.PCw);
        snes->mem->S9xPackStatus();
        PushBE(Registers.PL);
        snes->OpenBus = Registers.PL;
        ClearDecimal();
        SetIRQ();
        if (Settings.SA1 && (snes->mem->FillRAM[0x2209] & 0x10)) {
            snes->OpenBus = snes->mem->FillRAM[0x220d];
            AddCycles(2 * ONE_CYCLE);
            snes->mem->S9xSetPCBase(snes->mem->FillRAM[0x220c] | (snes->mem->FillRAM[0x220d] << 8));
        } else {
            uint16 addr = snes->mem->S9xGetWord(0xFFFA);
            snes->OpenBus     = addr >> 8;
            snes->mem->S9xSetPCBase(addr);
        }
    }
}
void SCPUState::SetZN(uint16 Work16)
{
    ICPU._Zero     = Work16 != 0;
    ICPU._Negative = (uint8)(Work16 >> 8);
}
void SCPUState::SetZN(uint8 Work8)
{
    ICPU._Zero     = Work8;
    ICPU._Negative = Work8;
}
void SCPUState::ADC(uint16 Work16)
{
    if (CheckDecimal()) {
        uint32 result;
        uint32 carry = CheckCarry();
        result       = (Registers.A.W & 0x000F) + (Work16 & 0x000F) + carry;
        if (result > 0x0009)
            result += 0x0006;
        carry  = (result > 0x000F);
        result = (Registers.A.W & 0x00F0) + (Work16 & 0x00F0) + (result & 0x000F) + carry * 0x10;
        if (result > 0x009F)
            result += 0x0060;
        carry  = (result > 0x00FF);
        result = (Registers.A.W & 0x0F00) + (Work16 & 0x0F00) + (result & 0x00FF) + carry * 0x100;
        if (result > 0x09FF)
            result += 0x0600;
        carry  = (result > 0x0FFF);
        result = (Registers.A.W & 0xF000) + (Work16 & 0xF000) + (result & 0x0FFF) + carry * 0x1000;
        if ((Registers.A.W & 0x8000) == (Work16 & 0x8000) && (Registers.A.W & 0x8000) != (result & 0x8000))
            SetOverflow();
        else
            ClearOverflow();
        if (result > 0x9FFF)
            result += 0x6000;
        if (result > 0xFFFF)
            SetCarry();
        else
            ClearCarry();
        Registers.A.W = result & 0xFFFF;
        SetZN(Registers.A.W);
    } else {
        uint32 Ans32 = Registers.A.W + Work16 + CheckCarry();
        ICPU._Carry  = Ans32 >= 0x10000;
        if (~(Registers.A.W ^ Work16) & (Work16 ^ (uint16)Ans32) & 0x8000)
            SetOverflow();
        else
            ClearOverflow();
        Registers.A.W = (uint16)Ans32;
        SetZN(Registers.A.W);
    }
}
void SCPUState::ADC(uint8 Work8)
{
    if (CheckDecimal()) {
        uint32 result;
        uint32 carry = CheckCarry();
        result       = (Registers.AL & 0x0F) + (Work8 & 0x0F) + carry;
        if (result > 0x09)
            result += 0x06;
        carry  = (result > 0x0F);
        result = (Registers.AL & 0xF0) + (Work8 & 0xF0) + (result & 0x0F) + (carry * 0x10);
        if ((Registers.AL & 0x80) == (Work8 & 0x80) && (Registers.AL & 0x80) != (result & 0x80))
            SetOverflow();
        else
            ClearOverflow();
        if (result > 0x9F)
            result += 0x60;
        if (result > 0xFF)
            SetCarry();
        else
            ClearCarry();
        Registers.AL = result & 0xFF;
        SetZN(Registers.AL);
    } else {
        uint16 Ans16 = Registers.AL + Work8 + CheckCarry();
        ICPU._Carry  = Ans16 >= 0x100;
        if (~(Registers.AL ^ Work8) & (Work8 ^ (uint8)Ans16) & 0x80)
            SetOverflow();
        else
            ClearOverflow();
        Registers.AL = (uint8)Ans16;
        SetZN(Registers.AL);
    }
}
void SCPUState::AND(uint16 Work16)
{
    Registers.A.W &= Work16;
    SetZN(Registers.A.W);
}
void SCPUState::AND(uint8 Work8)
{
    Registers.AL &= Work8;
    SetZN(Registers.AL);
}
void SCPUState::ASL16(uint32 OpAddress, s9xwrap_t w)
{
    uint16 Work16 = snes->mem->S9xGetWord(OpAddress, w);
    ICPU._Carry   = (Work16 & 0x8000) != 0;
    Work16 <<= 1;
    AddCycles(ONE_CYCLE);
    snes->mem->S9xSetWord(Work16, OpAddress, w, WRITE_10);
    snes->OpenBus = Work16 & 0xff;
    SetZN(Work16);
}
void SCPUState::ASL8(uint32 OpAddress)
{
    uint8 Work8 = snes->mem->S9xGetByte(OpAddress);
    ICPU._Carry = (Work8 & 0x80) != 0;
    Work8 <<= 1;
    AddCycles(ONE_CYCLE);
    snes->mem->S9xSetByte(Work8, OpAddress);
    snes->OpenBus = Work8;
    SetZN(Work8);
}
void SCPUState::BIT(uint16 Work16)
{
    ICPU._Overflow = (Work16 & 0x4000) != 0;
    ICPU._Negative = (uint8)(Work16 >> 8);
    ICPU._Zero     = (Work16 & Registers.A.W) != 0;
}
void SCPUState::BIT(uint8 Work8)
{
    ICPU._Overflow = (Work8 & 0x40) != 0;
    ICPU._Negative = Work8;
    ICPU._Zero     = Work8 & Registers.AL;
}
void SCPUState::CMP(uint16 val)
{
    int32 Int32 = (int32)Registers.A.W - (int32)val;
    ICPU._Carry = Int32 >= 0;
    SetZN((uint16)Int32);
}
void SCPUState::CMP(uint8 val)
{
    int16 Int16 = (int16)Registers.AL - (int16)val;
    ICPU._Carry = Int16 >= 0;
    SetZN((uint8)Int16);
}
void SCPUState::CPX(uint16 val)
{
    int32 Int32 = (int32)Registers.X.W - (int32)val;
    ICPU._Carry = Int32 >= 0;
    SetZN((uint16)Int32);
}
void SCPUState::CPX(uint8 val)
{
    int16 Int16 = (int16)Registers.XL - (int16)val;
    ICPU._Carry = Int16 >= 0;
    SetZN((uint8)Int16);
}
void SCPUState::CPY(uint16 val)
{
    int32 Int32 = (int32)Registers.Y.W - (int32)val;
    ICPU._Carry = Int32 >= 0;
    SetZN((uint16)Int32);
}
void SCPUState::CPY(uint8 val)
{
    int16 Int16 = (int16)Registers.YL - (int16)val;
    ICPU._Carry = Int16 >= 0;
    SetZN((uint8)Int16);
}
void SCPUState::DEC16(uint32 OpAddress, s9xwrap_t w)
{
    uint16 Work16 = snes->mem->S9xGetWord(OpAddress, w) - 1;
    AddCycles(ONE_CYCLE);
    snes->mem->S9xSetWord(Work16, OpAddress, w, WRITE_10);
    snes->OpenBus = Work16 & 0xff;
    SetZN(Work16);
}
void SCPUState::DEC8(uint32 OpAddress)
{
    uint8 Work8 = snes->mem->S9xGetByte(OpAddress) - 1;
    AddCycles(ONE_CYCLE);
    snes->mem->S9xSetByte(Work8, OpAddress);
    snes->OpenBus = Work8;
    SetZN(Work8);
}
void SCPUState::EOR(uint16 val)
{
    Registers.A.W ^= val;
    SetZN(Registers.A.W);
}
void SCPUState::EOR(uint8 val)
{
    Registers.AL ^= val;
    SetZN(Registers.AL);
}
void SCPUState::INC16(uint32 OpAddress, s9xwrap_t w)
{
    uint16 Work16 = snes->mem->S9xGetWord(OpAddress, w) + 1;
    AddCycles(ONE_CYCLE);
    snes->mem->S9xSetWord(Work16, OpAddress, w, WRITE_10);
    snes->OpenBus = Work16 & 0xff;
    SetZN(Work16);
}
void SCPUState::INC8(uint32 OpAddress)
{
    uint8 Work8 = snes->mem->S9xGetByte(OpAddress) + 1;
    AddCycles(ONE_CYCLE);
    snes->mem->S9xSetByte(Work8, OpAddress);
    snes->OpenBus = Work8;
    SetZN(Work8);
}
void SCPUState::LDA(uint16 val)
{
    Registers.A.W = val;
    SetZN(Registers.A.W);
}
void SCPUState::LDA(uint8 val)
{
    Registers.AL = val;
    SetZN(Registers.AL);
}
void SCPUState::LDX(uint16 val)
{
    Registers.X.W = val;
    SetZN(Registers.X.W);
}
void SCPUState::LDX(uint8 val)
{
    Registers.XL = val;
    SetZN(Registers.XL);
}
void SCPUState::LDY(uint16 val)
{
    Registers.Y.W = val;
    SetZN(Registers.Y.W);
}
void SCPUState::LDY(uint8 val)
{
    Registers.YL = val;
    SetZN(Registers.YL);
}
void SCPUState::LSR16(uint32 OpAddress, s9xwrap_t w)
{
    uint16 Work16 = snes->mem->S9xGetWord(OpAddress, w);
    ICPU._Carry   = Work16 & 1;
    Work16 >>= 1;
    AddCycles(ONE_CYCLE);
    snes->mem->S9xSetWord(Work16, OpAddress, w, WRITE_10);
    snes->OpenBus = Work16 & 0xff;
    SetZN(Work16);
}
void SCPUState::LSR8(uint32 OpAddress)
{
    uint8 Work8 = snes->mem->S9xGetByte(OpAddress);
    ICPU._Carry = Work8 & 1;
    Work8 >>= 1;
    AddCycles(ONE_CYCLE);
    snes->mem->S9xSetByte(Work8, OpAddress);
    snes->OpenBus = Work8;
    SetZN(Work8);
}
void SCPUState::ORA(uint16 val)
{
    Registers.A.W |= val;
    SetZN(Registers.A.W);
}
void SCPUState::ORA(uint8 val)
{
    Registers.AL |= val;
    SetZN(Registers.AL);
}
void SCPUState::ROL16(uint32 OpAddress, s9xwrap_t w)
{
    uint32 Work32 = (((uint32)snes->mem->S9xGetWord(OpAddress, w)) << 1) | CheckCarry();
    ICPU._Carry   = Work32 >= 0x10000;
    AddCycles(ONE_CYCLE);
    snes->mem->S9xSetWord((uint16)Work32, OpAddress, w, WRITE_10);
    snes->OpenBus = Work32 & 0xff;
    SetZN((uint16)Work32);
}
void SCPUState::ROL8(uint32 OpAddress)
{
    uint16 Work16 = (((uint16)snes->mem->S9xGetByte(OpAddress)) << 1) | CheckCarry();
    ICPU._Carry   = Work16 >= 0x100;
    AddCycles(ONE_CYCLE);
    snes->mem->S9xSetByte((uint8)Work16, OpAddress);
    snes->OpenBus = Work16 & 0xff;
    SetZN((uint8)Work16);
}
void SCPUState::ROR16(uint32 OpAddress, s9xwrap_t w)
{
    uint32 Work32 = ((uint32)snes->mem->S9xGetWord(OpAddress, w)) | (((uint32)CheckCarry()) << 16);
    ICPU._Carry   = Work32 & 1;
    Work32 >>= 1;
    AddCycles(ONE_CYCLE);
    snes->mem->S9xSetWord((uint16)Work32, OpAddress, w, WRITE_10);
    snes->OpenBus = Work32 & 0xff;
    SetZN((uint16)Work32);
}
void SCPUState::ROR8(uint32 OpAddress)
{
    uint16 Work16 = ((uint16)snes->mem->S9xGetByte(OpAddress)) | (((uint16)CheckCarry()) << 8);
    ICPU._Carry   = Work16 & 1;
    Work16 >>= 1;
    AddCycles(ONE_CYCLE);
    snes->mem->S9xSetByte((uint8)Work16, OpAddress);
    snes->OpenBus = Work16 & 0xff;
    SetZN((uint8)Work16);
}
void SCPUState::SBC(uint16 Work16)
{
    if (CheckDecimal()) {
        int result;
        int carry = CheckCarry();
        Work16 ^= 0xFFFF;
        result = (Registers.A.W & 0x000F) + (Work16 & 0x000F) + carry;
        if (result < 0x0010)
            result -= 0x0006;
        carry  = (result > 0x000F);
        result = (Registers.A.W & 0x00F0) + (Work16 & 0x00F0) + (result & 0x000F) + carry * 0x10;
        if (result < 0x0100)
            result -= 0x0060;
        carry  = (result > 0x00FF);
        result = (Registers.A.W & 0x0F00) + (Work16 & 0x0F00) + (result & 0x00FF) + carry * 0x100;
        if (result < 0x1000)
            result -= 0x0600;
        carry  = (result > 0x0FFF);
        result = (Registers.A.W & 0xF000) + (Work16 & 0xF000) + (result & 0x0FFF) + carry * 0x1000;
        if (((Registers.A.W ^ Work16) & 0x8000) == 0 && ((Registers.A.W ^ result) & 0x8000))
            SetOverflow();
        else
            ClearOverflow();
        if (result < 0x10000)
            result -= 0x6000;
        if (result > 0xFFFF)
            SetCarry();
        else
            ClearCarry();
        Registers.A.W = result & 0xFFFF;
        SetZN(Registers.A.W);
    } else {
        int32 Int32 = (int32)Registers.A.W - (int32)Work16 + (int32)CheckCarry() - 1;
        ICPU._Carry = Int32 >= 0;
        if ((Registers.A.W ^ Work16) & (Registers.A.W ^ (uint16)Int32) & 0x8000)
            SetOverflow();
        else
            ClearOverflow();
        Registers.A.W = (uint16)Int32;
        SetZN(Registers.A.W);
    }
}
void SCPUState::SBC(uint8 Work8)
{
    if (CheckDecimal()) {
        int result;
        int carry = CheckCarry();
        Work8 ^= 0xFF;
        result = (Registers.AL & 0x0F) + (Work8 & 0x0F) + carry;
        if (result < 0x10)
            result -= 0x06;
        carry  = (result > 0x0F);
        result = (Registers.AL & 0xF0) + (Work8 & 0xF0) + (result & 0x0F) + carry * 0x10;
        if ((Registers.AL & 0x80) == (Work8 & 0x80) && (Registers.AL & 0x80) != (result & 0x80))
            SetOverflow();
        else
            ClearOverflow();
        if (result < 0x100)
            result -= 0x60;
        if (result > 0xFF)
            SetCarry();
        else
            ClearCarry();
        Registers.AL = result & 0xFF;
        SetZN(Registers.AL);
    } else {
        int16 Int16 = (int16)Registers.AL - (int16)Work8 + (int16)CheckCarry() - 1;
        ICPU._Carry = Int16 >= 0;
        if ((Registers.AL ^ Work8) & (Registers.AL ^ (uint8)Int16) & 0x80)
            SetOverflow();
        else
            ClearOverflow();
        Registers.AL = (uint8)Int16;
        SetZN(Registers.AL);
    }
}
void SCPUState::STA16(uint32 OpAddress, enum s9xwrap_t w)
{
    snes->mem->S9xSetWord(Registers.A.W, OpAddress, w);
    snes->OpenBus = Registers.AH;
}
void SCPUState::STA8(uint32 OpAddress)
{
    snes->mem->S9xSetByte(Registers.AL, OpAddress);
    snes->OpenBus = Registers.AL;
}
void SCPUState::STX16(uint32 OpAddress, enum s9xwrap_t w)
{
    snes->mem->S9xSetWord(Registers.X.W, OpAddress, w);
    snes->OpenBus = Registers.XH;
}
void SCPUState::STX8(uint32 OpAddress)
{
    snes->mem->S9xSetByte(Registers.XL, OpAddress);
    snes->OpenBus = Registers.XL;
}
void SCPUState::STY16(uint32 OpAddress, enum s9xwrap_t w)
{
    snes->mem->S9xSetWord(Registers.Y.W, OpAddress, w);
    snes->OpenBus = Registers.YH;
}
void SCPUState::STY8(uint32 OpAddress)
{
    snes->mem->S9xSetByte(Registers.YL, OpAddress);
    snes->OpenBus = Registers.YL;
}
void SCPUState::STZ16(uint32 OpAddress, enum s9xwrap_t w)
{
    snes->mem->S9xSetWord(0, OpAddress, w);
    snes->OpenBus = 0;
}
void SCPUState::STZ8(uint32 OpAddress)
{
    snes->mem->S9xSetByte(0, OpAddress);
    snes->OpenBus = 0;
}
void SCPUState::TSB16(uint32 OpAddress, enum s9xwrap_t w)
{
    uint16 Work16 = snes->mem->S9xGetWord(OpAddress, w);
    ICPU._Zero    = (Work16 & Registers.A.W) != 0;
    Work16 |= Registers.A.W;
    AddCycles(ONE_CYCLE);
    snes->mem->S9xSetWord(Work16, OpAddress, w, WRITE_10);
    snes->OpenBus = Work16 & 0xff;
}
void SCPUState::TSB8(uint32 OpAddress)
{
    uint8 Work8 = snes->mem->S9xGetByte(OpAddress);
    ICPU._Zero  = Work8 & Registers.AL;
    Work8 |= Registers.AL;
    AddCycles(ONE_CYCLE);
    snes->mem->S9xSetByte(Work8, OpAddress);
    snes->OpenBus = Work8;
}
void SCPUState::TRB16(uint32 OpAddress, enum s9xwrap_t w)
{
    uint16 Work16 = snes->mem->S9xGetWord(OpAddress, w);
    ICPU._Zero    = (Work16 & Registers.A.W) != 0;
    Work16 &= ~Registers.A.W;
    AddCycles(ONE_CYCLE);
    snes->mem->S9xSetWord(Work16, OpAddress, w, WRITE_10);
    snes->OpenBus = Work16 & 0xff;
}
void SCPUState::TRB8(uint32 OpAddress)
{
    uint8 Work8 = snes->mem->S9xGetByte(OpAddress);
    ICPU._Zero  = Work8 & Registers.AL;
    Work8 &= ~Registers.AL;
    AddCycles(ONE_CYCLE);
    snes->mem->S9xSetByte(Work8, OpAddress);
    snes->OpenBus = Work8;
}
uint8 SCPUState::Immediate8Slow(AccessMode a)
{
    uint8 val = snes->mem->S9xGetByte(Registers.PBPC);
    if (a & READ)
        snes->OpenBus = val;
    Registers.PCw++;
    return (val);
}
uint8 SCPUState::Immediate8(AccessMode a)
{
    uint8 val = PCBase[Registers.PCw];
    if (a & READ)
        snes->OpenBus = val;
    AddCycles(MemSpeed);
    Registers.PCw++;
    return (val);
}
uint16 SCPUState::Immediate16Slow(AccessMode a)
{
    uint16 val = snes->mem->S9xGetWord(Registers.PBPC, WRAP_BANK);
    if (a & READ)
        snes->OpenBus = (uint8)(val >> 8);
    Registers.PCw += 2;
    return (val);
}
uint16 SCPUState::Immediate16(AccessMode a)
{
    uint16 val = READ_WORD(PCBase + Registers.PCw);
    if (a & READ)
        snes->OpenBus = (uint8)(val >> 8);
    AddCycles(MemSpeedx2);
    Registers.PCw += 2;
    return (val);
}
uint32 SCPUState::RelativeSlow(AccessMode a)
{
    int8 offset = Immediate8Slow(a);
    return ((int16)Registers.PCw + offset) & 0xffff;
}
uint32 SCPUState::Relative(AccessMode a)
{
    int8 offset = Immediate8(a);
    return ((int16)Registers.PCw + offset) & 0xffff;
}
uint32 SCPUState::RelativeLongSlow(AccessMode a)
{
    int16 offset = Immediate16Slow(a);
    return ((int32)Registers.PCw + offset) & 0xffff;
}
uint32 SCPUState::RelativeLong(AccessMode a)
{
    int16 offset = Immediate16(a);
    return ((int32)Registers.PCw + offset) & 0xffff;
}
uint32 SCPUState::AbsoluteIndexedIndirectSlow(AccessMode a)
{
    uint16 addr;
    if (a & JSR) {
        addr = Immediate8Slow(READ);
        if (a == JSR)
            snes->OpenBus = Registers.PCl;
        addr |= Immediate8Slow(READ) << 8;
    } else
        addr = Immediate16Slow(READ);
    AddCycles(ONE_CYCLE);
    addr += Registers.X.W;
    uint16 addr2 = snes->mem->S9xGetWord(ICPU.ShiftedPB | addr, WRAP_BANK);
    snes->OpenBus      = addr2 >> 8;
    return (addr2);
}
uint32 SCPUState::AbsoluteIndexedIndirect(AccessMode a)
{
    uint16 addr = Immediate16Slow(READ);
    AddCycles(ONE_CYCLE);
    addr += Registers.X.W;
    uint16 addr2 = snes->mem->S9xGetWord(ICPU.ShiftedPB | addr, WRAP_BANK);
    snes->OpenBus      = addr2 >> 8;
    return (addr2);
}
uint32 SCPUState::AbsoluteIndirectLongSlow(AccessMode a)
{
    uint16 addr  = Immediate16Slow(READ);
    uint32 addr2 = snes->mem->S9xGetWord(addr);
    snes->OpenBus      = addr2 >> 8;
    addr2 |= (snes->OpenBus = snes->mem->S9xGetByte(addr + 2)) << 16;
    return (addr2);
}
uint32 SCPUState::AbsoluteIndirectLong(AccessMode a)
{
    uint16 addr  = Immediate16(READ);
    uint32 addr2 = snes->mem->S9xGetWord(addr);
    snes->OpenBus      = addr2 >> 8;
    addr2 |= (snes->OpenBus = snes->mem->S9xGetByte(addr + 2)) << 16;
    return (addr2);
}
uint32 SCPUState::AbsoluteIndirectSlow(AccessMode a)
{
    uint16 addr2 = snes->mem->S9xGetWord(Immediate16Slow(READ));
    snes->OpenBus      = addr2 >> 8;
    return (addr2);
}
uint32 SCPUState::AbsoluteIndirect(AccessMode a)
{
    uint16 addr2 = snes->mem->S9xGetWord(Immediate16(READ));
    snes->OpenBus      = addr2 >> 8;
    return (addr2);
}
uint32 SCPUState::AbsoluteSlow(AccessMode a)
{
    return (ICPU.ShiftedDB | Immediate16Slow(a));
}
uint32 SCPUState::Absolute(AccessMode a)
{
    return (ICPU.ShiftedDB | Immediate16(a));
}
uint32 SCPUState::AbsoluteLongSlow(AccessMode a)
{
    uint32 addr = Immediate16Slow(READ);
    if (a == JSR)
        snes->OpenBus = Registers.PB;
    addr |= Immediate8Slow(a) << 16;
    return (addr);
}
uint32 SCPUState::AbsoluteLong(AccessMode a)
{
    uint32 addr = READ_3WORD(PCBase + Registers.PCw);
    AddCycles(MemSpeedx2 + MemSpeed);
    if (a & READ)
        snes->OpenBus = addr >> 16;
    Registers.PCw += 3;
    return (addr);
}
uint32 SCPUState::DirectSlow(AccessMode a)
{
    uint16 addr = Immediate8Slow(a) + Registers.D.W;
    if (Registers.DL != 0)
        AddCycles(ONE_CYCLE);
    return (addr);
}
uint32 SCPUState::Direct(AccessMode a)
{
    uint16 addr = Immediate8(a) + Registers.D.W;
    if (Registers.DL != 0)
        AddCycles(ONE_CYCLE);
    return (addr);
}
uint32 SCPUState::DirectIndirectSlow(AccessMode a)
{
    uint32 addr = snes->mem->S9xGetWord(DirectSlow(READ), (!CheckEmulation() || Registers.DL) ? WRAP_BANK : WRAP_PAGE);
    if (a & READ)
        snes->OpenBus = (uint8)(addr >> 8);
    addr |= ICPU.ShiftedDB;
    return (addr);
}
uint32 SCPUState::DirectIndirectE0(AccessMode a)
{
    uint32 addr = snes->mem->S9xGetWord(Direct(READ));
    if (a & READ)
        snes->OpenBus = (uint8)(addr >> 8);
    addr |= ICPU.ShiftedDB;
    return (addr);
}
uint32 SCPUState::DirectIndirectE1(AccessMode a)
{
    uint32 addr = snes->mem->S9xGetWord(DirectSlow(READ), Registers.DL ? WRAP_BANK : WRAP_PAGE);
    if (a & READ)
        snes->OpenBus = (uint8)(addr >> 8);
    addr |= ICPU.ShiftedDB;
    return (addr);
}
uint32 SCPUState::DirectIndirectIndexedSlow(AccessMode a)
{
    uint32 addr = DirectIndirectSlow(a);
    if (a & WRITE || !CheckIndex() || (addr & 0xff) + Registers.YL >= 0x100)
        AddCycles(ONE_CYCLE);
    return (addr + Registers.Y.W);
}
uint32 SCPUState::DirectIndirectIndexedE0X0(AccessMode a)
{
    uint32 addr = DirectIndirectE0(a);
    AddCycles(ONE_CYCLE);
    return (addr + Registers.Y.W);
}
uint32 SCPUState::DirectIndirectIndexedE0X1(AccessMode a)
{
    uint32 addr = DirectIndirectE0(a);
    if (a & WRITE || (addr & 0xff) + Registers.YL >= 0x100)
        AddCycles(ONE_CYCLE);
    return (addr + Registers.Y.W);
}
uint32 SCPUState::DirectIndirectIndexedE1(AccessMode a)
{
    uint32 addr = DirectIndirectE1(a);
    if (a & WRITE || (addr & 0xff) + Registers.YL >= 0x100)
        AddCycles(ONE_CYCLE);
    return (addr + Registers.Y.W);
}
uint32 SCPUState::DirectIndirectLongSlow(AccessMode a)
{
    uint16 addr  = DirectSlow(READ);
    uint32 addr2 = snes->mem->S9xGetWord(addr);
    snes->OpenBus      = addr2 >> 8;
    addr2 |= (snes->OpenBus = snes->mem->S9xGetByte(addr + 2)) << 16;
    return (addr2);
}
uint32 SCPUState::DirectIndirectLong(AccessMode a)
{
    uint16 addr  = Direct(READ);
    uint32 addr2 = snes->mem->S9xGetWord(addr);
    snes->OpenBus      = addr2 >> 8;
    addr2 |= (snes->OpenBus = snes->mem->S9xGetByte(addr + 2)) << 16;
    return (addr2);
}
uint32 SCPUState::DirectIndirectIndexedLongSlow(AccessMode a)
{
    return (DirectIndirectLongSlow(a) + Registers.Y.W);
}
uint32 SCPUState::DirectIndirectIndexedLong(AccessMode a)
{
    return (DirectIndirectLong(a) + Registers.Y.W);
}
uint32 SCPUState::DirectIndexedXSlow(AccessMode a)
{
    pair addr;
    addr.W = DirectSlow(a);
    if (!CheckEmulation() || Registers.DL)
        addr.W += Registers.X.W;
    else
        addr.B.l += Registers.XL;
    AddCycles(ONE_CYCLE);
    return (addr.W);
}
uint32 SCPUState::DirectIndexedXE0(AccessMode a)
{
    uint16 addr = Direct(a) + Registers.X.W;
    AddCycles(ONE_CYCLE);
    return (addr);
}
uint32 SCPUState::DirectIndexedXE1(AccessMode a)
{
    if (Registers.DL)
        return (DirectIndexedXE0(a));
    else {
        pair addr;
        addr.W = Direct(a);
        addr.B.l += Registers.XL;
        AddCycles(ONE_CYCLE);
        return (addr.W);
    }
}
uint32 SCPUState::DirectIndexedYSlow(AccessMode a)
{
    pair addr;
    addr.W = DirectSlow(a);
    if (!CheckEmulation() || Registers.DL)
        addr.W += Registers.Y.W;
    else
        addr.B.l += Registers.YL;
    AddCycles(ONE_CYCLE);
    return (addr.W);
}
uint32 SCPUState::DirectIndexedYE0(AccessMode a)
{
    uint16 addr = Direct(a) + Registers.Y.W;
    AddCycles(ONE_CYCLE);
    return (addr);
}
uint32 SCPUState::DirectIndexedYE1(AccessMode a)
{
    if (Registers.DL)
        return (DirectIndexedYE0(a));
    else {
        pair addr;
        addr.W = Direct(a);
        addr.B.l += Registers.YL;
        AddCycles(ONE_CYCLE);
        return (addr.W);
    }
}
uint32 SCPUState::DirectIndexedIndirectSlow(AccessMode a)
{
    uint32 addr =
        snes->mem->S9xGetWord(DirectIndexedXSlow(READ), (!CheckEmulation() || Registers.DL) ? WRAP_BANK : WRAP_PAGE);
    if (a & READ)
        snes->OpenBus = (uint8)(addr >> 8);
    return (ICPU.ShiftedDB | addr);
}
uint32 SCPUState::DirectIndexedIndirectE0(AccessMode a)
{
    uint32 addr = snes->mem->S9xGetWord(DirectIndexedXE0(READ));
    if (a & READ)
        snes->OpenBus = (uint8)(addr >> 8);
    return (ICPU.ShiftedDB | addr);
}
uint32 SCPUState::DirectIndexedIndirectE1(AccessMode a)
{
    uint32 addr = snes->mem->S9xGetWord(DirectIndexedXE1(READ), Registers.DL ? WRAP_BANK : WRAP_PAGE);
    if (a & READ)
        snes->OpenBus = (uint8)(addr >> 8);
    return (ICPU.ShiftedDB | addr);
}
uint32 SCPUState::AbsoluteIndexedXSlow(AccessMode a)
{
    uint32 addr = AbsoluteSlow(a);
    if (a & WRITE || !CheckIndex() || (addr & 0xff) + Registers.XL >= 0x100)
        AddCycles(ONE_CYCLE);
    return (addr + Registers.X.W);
}
uint32 SCPUState::AbsoluteIndexedXX0(AccessMode a)
{
    uint32 addr = Absolute(a);
    AddCycles(ONE_CYCLE);
    return (addr + Registers.X.W);
}
uint32 SCPUState::AbsoluteIndexedXX1(AccessMode a)
{
    uint32 addr = Absolute(a);
    if (a & WRITE || (addr & 0xff) + Registers.XL >= 0x100)
        AddCycles(ONE_CYCLE);
    return (addr + Registers.X.W);
}
uint32 SCPUState::AbsoluteIndexedYSlow(AccessMode a)
{
    uint32 addr = AbsoluteSlow(a);
    if (a & WRITE || !CheckIndex() || (addr & 0xff) + Registers.YL >= 0x100)
        AddCycles(ONE_CYCLE);
    return (addr + Registers.Y.W);
}
uint32 SCPUState::AbsoluteIndexedYX0(AccessMode a)
{
    uint32 addr = Absolute(a);
    AddCycles(ONE_CYCLE);
    return (addr + Registers.Y.W);
}
uint32 SCPUState::AbsoluteIndexedYX1(AccessMode a)
{
    uint32 addr = Absolute(a);
    if (a & WRITE || (addr & 0xff) + Registers.YL >= 0x100)
        AddCycles(ONE_CYCLE);
    return (addr + Registers.Y.W);
}
uint32 SCPUState::AbsoluteLongIndexedXSlow(AccessMode a)
{
    return (AbsoluteLongSlow(a) + Registers.X.W);
}
uint32 SCPUState::AbsoluteLongIndexedX(AccessMode a)
{
    return (AbsoluteLong(a) + Registers.X.W);
}
uint32 SCPUState::StackRelativeSlow(AccessMode a)
{
    uint16 addr = Immediate8Slow(a) + Registers.S.W;
    AddCycles(ONE_CYCLE);
    return (addr);
}
uint32 SCPUState::StackRelative(AccessMode a)
{
    uint16 addr = Immediate8(a) + Registers.S.W;
    AddCycles(ONE_CYCLE);
    return (addr);
}
uint32 SCPUState::StackRelativeIndirectIndexedSlow(AccessMode a)
{
    uint32 addr = snes->mem->S9xGetWord(StackRelativeSlow(READ));
    if (a & READ)
        snes->OpenBus = (uint8)(addr >> 8);
    addr = (addr + Registers.Y.W + ICPU.ShiftedDB) & 0xffffff;
    AddCycles(ONE_CYCLE);
    return (addr);
}
uint32 SCPUState::StackRelativeIndirectIndexed(AccessMode a)
{
    uint32 addr = snes->mem->S9xGetWord(StackRelative(READ));
    if (a & READ)
        snes->OpenBus = (uint8)(addr >> 8);
    addr = (addr + Registers.Y.W + ICPU.ShiftedDB) & 0xffffff;
    AddCycles(ONE_CYCLE);
    return (addr);
}
void SCPUState::Op69M1(void)
{
    ADC(Immediate8(READ));
}
void SCPUState::Op69M0(void)
{
    ADC(Immediate16(READ));
}
void SCPUState::Op69Slow(void)
{
    if (CheckMemory())
        ADC(Immediate8Slow(READ));
    else
        ADC(Immediate16Slow(READ));
}
void SCPUState::Op65M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(Direct(READ));
    ADC(val);
}
void SCPUState::Op65M0(void)
{
    uint16 val = snes->mem->S9xGetWord(Direct(READ), WRAP_BANK);
    snes->OpenBus    = (uint8)(val >> 8);
    ADC(val);
}
void SCPUState::Op65Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectSlow(READ));
        ADC(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectSlow(READ), WRAP_BANK);
        snes->OpenBus    = (uint8)(val >> 8);
        ADC(val);
    }
}
void SCPUState::Op75E1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedXE1(READ));
    ADC(val);
}
void SCPUState::Op75E0M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedXE0(READ));
    ADC(val);
}
void SCPUState::Op75E0M0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndexedXE0(READ), WRAP_BANK);
    snes->OpenBus    = (uint8)(val >> 8);
    ADC(val);
}
void SCPUState::Op75Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedXSlow(READ));
        ADC(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndexedXSlow(READ), WRAP_BANK);
        snes->OpenBus    = (uint8)(val >> 8);
        ADC(val);
    }
}
void SCPUState::Op72E1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectE1(READ));
    ADC(val);
}
void SCPUState::Op72E0M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectE0(READ));
    ADC(val);
}
void SCPUState::Op72E0M0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndirectE0(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    ADC(val);
}
void SCPUState::Op72Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectSlow(READ));
        ADC(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndirectSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        ADC(val);
    }
}
void SCPUState::Op61E1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedIndirectE1(READ));
    ADC(val);
}
void SCPUState::Op61E0M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedIndirectE0(READ));
    ADC(val);
}
void SCPUState::Op61E0M0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndexedIndirectE0(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    ADC(val);
}
void SCPUState::Op61Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedIndirectSlow(READ));
        ADC(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndexedIndirectSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        ADC(val);
    }
}
void SCPUState::Op71E1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedE1(READ));
    ADC(val);
}
void SCPUState::Op71E0M1X1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedE0X1(READ));
    ADC(val);
}
void SCPUState::Op71E0M0X1(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndirectIndexedE0X1(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    ADC(val);
}
void SCPUState::Op71E0M1X0(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedE0X0(READ));
    ADC(val);
}
void SCPUState::Op71E0M0X0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndirectIndexedE0X0(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    ADC(val);
}
void SCPUState::Op71Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedSlow(READ));
        ADC(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndirectIndexedSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        ADC(val);
    }
}
void SCPUState::Op67M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectLong(READ));
    ADC(val);
}
void SCPUState::Op67M0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndirectLong(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    ADC(val);
}
void SCPUState::Op67Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectLongSlow(READ));
        ADC(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndirectLongSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        ADC(val);
    }
}
void SCPUState::Op77M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedLong(READ));
    ADC(val);
}
void SCPUState::Op77M0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndirectIndexedLong(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    ADC(val);
}
void SCPUState::Op77Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedLongSlow(READ));
        ADC(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndirectIndexedLongSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        ADC(val);
    }
}
void SCPUState::Op6DM1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(Absolute(READ));
    ADC(val);
}
void SCPUState::Op6DM0(void)
{
    uint16 val = snes->mem->S9xGetWord(Absolute(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    ADC(val);
}
void SCPUState::Op6DSlow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteSlow(READ));
        ADC(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        ADC(val);
    }
}
void SCPUState::Op7DM1X1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedXX1(READ));
    ADC(val);
}
void SCPUState::Op7DM0X1(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedXX1(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    ADC(val);
}
void SCPUState::Op7DM1X0(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedXX0(READ));
    ADC(val);
}
void SCPUState::Op7DM0X0(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedXX0(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    ADC(val);
}
void SCPUState::Op7DSlow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedXSlow(READ));
        ADC(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedXSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        ADC(val);
    }
}
void SCPUState::Op79M1X1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedYX1(READ));
    ADC(val);
}
void SCPUState::Op79M0X1(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedYX1(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    ADC(val);
}
void SCPUState::Op79M1X0(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedYX0(READ));
    ADC(val);
}
void SCPUState::Op79M0X0(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedYX0(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    ADC(val);
}
void SCPUState::Op79Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedYSlow(READ));
        ADC(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedYSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        ADC(val);
    }
}
void SCPUState::Op6FM1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteLong(READ));
    ADC(val);
}
void SCPUState::Op6FM0(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteLong(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    ADC(val);
}
void SCPUState::Op6FSlow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteLongSlow(READ));
        ADC(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteLongSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        ADC(val);
    }
}
void SCPUState::Op7FM1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteLongIndexedX(READ));
    ADC(val);
}
void SCPUState::Op7FM0(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteLongIndexedX(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    ADC(val);
}
void SCPUState::Op7FSlow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteLongIndexedXSlow(READ));
        ADC(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteLongIndexedXSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        ADC(val);
    }
}
void SCPUState::Op63M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(StackRelative(READ));
    ADC(val);
}
void SCPUState::Op63M0(void)
{
    uint16 val = snes->mem->S9xGetWord(StackRelative(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    ADC(val);
}
void SCPUState::Op63Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(StackRelativeSlow(READ));
        ADC(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(StackRelativeSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        ADC(val);
    }
}
void SCPUState::Op73M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(StackRelativeIndirectIndexed(READ));
    ADC(val);
}
void SCPUState::Op73M0(void)
{
    uint16 val = snes->mem->S9xGetWord(StackRelativeIndirectIndexed(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    ADC(val);
}
void SCPUState::Op73Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(StackRelativeIndirectIndexedSlow(READ));
        ADC(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(StackRelativeIndirectIndexedSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        ADC(val);
    }
}
void SCPUState::Op29M1(void)
{
    Registers.A.B.l &= Immediate8(READ);
    SetZN(Registers.A.B.l);
}
void SCPUState::Op29M0(void)
{
    Registers.A.W &= Immediate16(READ);
    SetZN(Registers.A.W);
}
void SCPUState::Op29Slow(void)
{
    if (CheckMemory()) {
        Registers.AL &= Immediate8Slow(READ);
        SetZN(Registers.AL);
    } else {
        Registers.A.W &= Immediate16Slow(READ);
        SetZN(Registers.A.W);
    }
}
void SCPUState::Op25M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(Direct(READ));
    AND(val);
}
void SCPUState::Op25M0(void)
{
    uint16 val = snes->mem->S9xGetWord(Direct(READ), WRAP_BANK);
    snes->OpenBus    = (uint8)(val >> 8);
    AND(val);
}
void SCPUState::Op25Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectSlow(READ));
        AND(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectSlow(READ), WRAP_BANK);
        snes->OpenBus    = (uint8)(val >> 8);
        AND(val);
    }
}
void SCPUState::Op35E1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedXE1(READ));
    AND(val);
}
void SCPUState::Op35E0M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedXE0(READ));
    AND(val);
}
void SCPUState::Op35E0M0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndexedXE0(READ), WRAP_BANK);
    snes->OpenBus    = (uint8)(val >> 8);
    AND(val);
}
void SCPUState::Op35Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedXSlow(READ));
        AND(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndexedXSlow(READ), WRAP_BANK);
        snes->OpenBus    = (uint8)(val >> 8);
        AND(val);
    }
}
void SCPUState::Op32E1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectE1(READ));
    AND(val);
}
void SCPUState::Op32E0M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectE0(READ));
    AND(val);
}
void SCPUState::Op32E0M0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndirectE0(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    AND(val);
}
void SCPUState::Op32Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectSlow(READ));
        AND(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndirectSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        AND(val);
    }
}
void SCPUState::Op21E1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedIndirectE1(READ));
    AND(val);
}
void SCPUState::Op21E0M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedIndirectE0(READ));
    AND(val);
}
void SCPUState::Op21E0M0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndexedIndirectE0(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    AND(val);
}
void SCPUState::Op21Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedIndirectSlow(READ));
        AND(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndexedIndirectSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        AND(val);
    }
}
void SCPUState::Op31E1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedE1(READ));
    AND(val);
}
void SCPUState::Op31E0M1X1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedE0X1(READ));
    AND(val);
}
void SCPUState::Op31E0M0X1(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndirectIndexedE0X1(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    AND(val);
}
void SCPUState::Op31E0M1X0(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedE0X0(READ));
    AND(val);
}
void SCPUState::Op31E0M0X0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndirectIndexedE0X0(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    AND(val);
}
void SCPUState::Op31Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedSlow(READ));
        AND(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndirectIndexedSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        AND(val);
    }
}
void SCPUState::Op27M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectLong(READ));
    AND(val);
}
void SCPUState::Op27M0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndirectLong(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    AND(val);
}
void SCPUState::Op27Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectLongSlow(READ));
        AND(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndirectLongSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        AND(val);
    }
}
void SCPUState::Op37M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedLong(READ));
    AND(val);
}
void SCPUState::Op37M0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndirectIndexedLong(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    AND(val);
}
void SCPUState::Op37Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedLongSlow(READ));
        AND(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndirectIndexedLongSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        AND(val);
    }
}
void SCPUState::Op2DM1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(Absolute(READ));
    AND(val);
}
void SCPUState::Op2DM0(void)
{
    uint16 val = snes->mem->S9xGetWord(Absolute(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    AND(val);
}
void SCPUState::Op2DSlow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteSlow(READ));
        AND(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        AND(val);
    }
}
void SCPUState::Op3DM1X1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedXX1(READ));
    AND(val);
}
void SCPUState::Op3DM0X1(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedXX1(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    AND(val);
}
void SCPUState::Op3DM1X0(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedXX0(READ));
    AND(val);
}
void SCPUState::Op3DM0X0(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedXX0(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    AND(val);
}
void SCPUState::Op3DSlow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedXSlow(READ));
        AND(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedXSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        AND(val);
    }
}
void SCPUState::Op39M1X1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedYX1(READ));
    AND(val);
}
void SCPUState::Op39M0X1(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedYX1(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    AND(val);
}
void SCPUState::Op39M1X0(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedYX0(READ));
    AND(val);
}
void SCPUState::Op39M0X0(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedYX0(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    AND(val);
}
void SCPUState::Op39Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedYSlow(READ));
        AND(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedYSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        AND(val);
    }
}
void SCPUState::Op2FM1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteLong(READ));
    AND(val);
}
void SCPUState::Op2FM0(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteLong(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    AND(val);
}
void SCPUState::Op2FSlow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteLongSlow(READ));
        AND(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteLongSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        AND(val);
    }
}
void SCPUState::Op3FM1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteLongIndexedX(READ));
    AND(val);
}
void SCPUState::Op3FM0(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteLongIndexedX(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    AND(val);
}
void SCPUState::Op3FSlow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteLongIndexedXSlow(READ));
        AND(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteLongIndexedXSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        AND(val);
    }
}
void SCPUState::Op23M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(StackRelative(READ));
    AND(val);
}
void SCPUState::Op23M0(void)
{
    uint16 val = snes->mem->S9xGetWord(StackRelative(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    AND(val);
}
void SCPUState::Op23Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(StackRelativeSlow(READ));
        AND(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(StackRelativeSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        AND(val);
    }
}
void SCPUState::Op33M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(StackRelativeIndirectIndexed(READ));
    AND(val);
}
void SCPUState::Op33M0(void)
{
    uint16 val = snes->mem->S9xGetWord(StackRelativeIndirectIndexed(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    AND(val);
}
void SCPUState::Op33Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(StackRelativeIndirectIndexedSlow(READ));
        AND(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(StackRelativeIndirectIndexedSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        AND(val);
    }
}
void SCPUState::Op0AM1(void)
{
    {
        Cycles += (6);
        while (Cycles >= NextEvent)
            S9xDoHEventProcessing();
    };
    ICPU._Carry = (Registers.A.B.l & 0x80) != 0;
    Registers.A.B.l <<= 1;
    SetZN(Registers.A.B.l);
}
void SCPUState::Op0AM0(void)
{
    AddCycles(ONE_CYCLE);
    ICPU._Carry = (Registers.AH & 0x80) != 0;
    Registers.A.W <<= 1;
    SetZN(Registers.A.W);
}
void SCPUState::Op0ASlow(void)
{
    AddCycles(ONE_CYCLE);
    if (CheckMemory()) {
        ICPU._Carry = (Registers.AL & 0x80) != 0;
        Registers.AL <<= 1;
        SetZN(Registers.AL);
    } else {
        ICPU._Carry = (Registers.AH & 0x80) != 0;
        Registers.A.W <<= 1;
        SetZN(Registers.A.W);
    }
}
void SCPUState::Op06M1(void)
{
    ASL8(Direct(MODIFY));
}
void SCPUState::Op06M0(void)
{
    ASL16(Direct(MODIFY), WRAP_BANK);
}
void SCPUState::Op06Slow(void)
{
    if ((Registers.P.B.l & 32))
        ASL8(DirectSlow(MODIFY));
    else
        ASL16(DirectSlow(MODIFY), WRAP_BANK);
}
void SCPUState::Op16E1(void)
{
    ASL8(DirectIndexedXE1(MODIFY));
}
void SCPUState::Op16E0M1(void)
{
    ASL8(DirectIndexedXE0(MODIFY));
}
void SCPUState::Op16E0M0(void)
{
    ASL16(DirectIndexedXE0(MODIFY), WRAP_BANK);
}
void SCPUState::Op16Slow(void)
{
    if ((Registers.P.B.l & 32))
        ASL8(DirectIndexedXSlow(MODIFY));
    else
        ASL16(DirectIndexedXSlow(MODIFY), WRAP_BANK);
}
void SCPUState::Op0EM1(void)
{
    ASL8(Absolute(MODIFY));
}
void SCPUState::Op0EM0(void)
{
    ASL16(Absolute(MODIFY), WRAP_NONE);
}
void SCPUState::Op0ESlow(void)
{
    if ((Registers.P.B.l & 32))
        ASL8(AbsoluteSlow(MODIFY));
    else
        ASL16(AbsoluteSlow(MODIFY), WRAP_NONE);
}
void SCPUState::Op1EM1X1(void)
{
    ASL8(AbsoluteIndexedXX1(MODIFY));
}
void SCPUState::Op1EM0X1(void)
{
    ASL16(AbsoluteIndexedXX1(MODIFY), WRAP_NONE);
}
void SCPUState::Op1EM1X0(void)
{
    ASL8(AbsoluteIndexedXX0(MODIFY));
}
void SCPUState::Op1EM0X0(void)
{
    ASL16(AbsoluteIndexedXX0(MODIFY), WRAP_NONE);
}
void SCPUState::Op1ESlow(void)
{
    if ((Registers.P.B.l & 32))
        ASL8(AbsoluteIndexedXSlow(MODIFY));
    else
        ASL16(AbsoluteIndexedXSlow(MODIFY), WRAP_NONE);
}
void SCPUState::Op89M1(void)
{
    ICPU._Zero = Registers.A.B.l & Immediate8(READ);
}
void SCPUState::Op89M0(void)
{
    ICPU._Zero = (Registers.A.W & Immediate16(READ)) != 0;
}
void SCPUState::Op89Slow(void)
{
    if (CheckMemory())
        ICPU._Zero = Registers.AL & Immediate8Slow(READ);
    else
        ICPU._Zero = (Registers.A.W & Immediate16Slow(READ)) != 0;
}
void SCPUState::Op24M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(Direct(READ));
    BIT(val);
}
void SCPUState::Op24M0(void)
{
    uint16 val = snes->mem->S9xGetWord(Direct(READ), WRAP_BANK);
    snes->OpenBus    = (uint8)(val >> 8);
    BIT(val);
}
void SCPUState::Op24Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectSlow(READ));
        BIT(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectSlow(READ), WRAP_BANK);
        snes->OpenBus    = (uint8)(val >> 8);
        BIT(val);
    }
}
void SCPUState::Op34E1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedXE1(READ));
    BIT(val);
}
void SCPUState::Op34E0M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedXE0(READ));
    BIT(val);
}
void SCPUState::Op34E0M0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndexedXE0(READ), WRAP_BANK);
    snes->OpenBus    = (uint8)(val >> 8);
    BIT(val);
}
void SCPUState::Op34Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedXSlow(READ));
        BIT(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndexedXSlow(READ), WRAP_BANK);
        snes->OpenBus    = (uint8)(val >> 8);
        BIT(val);
    }
}
void SCPUState::Op2CM1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(Absolute(READ));
    BIT(val);
}
void SCPUState::Op2CM0(void)
{
    uint16 val = snes->mem->S9xGetWord(Absolute(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    BIT(val);
}
void SCPUState::Op2CSlow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteSlow(READ));
        BIT(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        BIT(val);
    }
}
void SCPUState::Op3CM1X1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedXX1(READ));
    BIT(val);
}
void SCPUState::Op3CM0X1(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedXX1(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    BIT(val);
}
void SCPUState::Op3CM1X0(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedXX0(READ));
    BIT(val);
}
void SCPUState::Op3CM0X0(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedXX0(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    BIT(val);
}
void SCPUState::Op3CSlow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedXSlow(READ));
        BIT(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedXSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        BIT(val);
    }
}
void SCPUState::OpC9M1(void)
{
    int16 Int16 = (int16)Registers.A.B.l - (int16)Immediate8(READ);
    ICPU._Carry = Int16 >= 0;
    SetZN((uint8)Int16);
}
void SCPUState::OpC9M0(void)
{
    int32 Int32 = (int32)Registers.A.W - (int32)Immediate16(READ);
    ICPU._Carry = Int32 >= 0;
    SetZN((uint16)Int32);
}
void SCPUState::OpC9Slow(void)
{
    if (CheckMemory()) {
        int16 Int16 = (int16)Registers.AL - (int16)Immediate8Slow(READ);
        ICPU._Carry = Int16 >= 0;
        SetZN((uint8)Int16);
    } else {
        int32 Int32 = (int32)Registers.A.W - (int32)Immediate16Slow(READ);
        ICPU._Carry = Int32 >= 0;
        SetZN((uint16)Int32);
    }
}
void SCPUState::OpC5M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(Direct(READ));
    CMP(val);
}
void SCPUState::OpC5M0(void)
{
    uint16 val = snes->mem->S9xGetWord(Direct(READ), WRAP_BANK);
    snes->OpenBus    = (uint8)(val >> 8);
    CMP(val);
}
void SCPUState::OpC5Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectSlow(READ));
        CMP(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectSlow(READ), WRAP_BANK);
        snes->OpenBus    = (uint8)(val >> 8);
        CMP(val);
    }
}
void SCPUState::OpD5E1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedXE1(READ));
    CMP(val);
}
void SCPUState::OpD5E0M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedXE0(READ));
    CMP(val);
}
void SCPUState::OpD5E0M0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndexedXE0(READ), WRAP_BANK);
    snes->OpenBus    = (uint8)(val >> 8);
    CMP(val);
}
void SCPUState::OpD5Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedXSlow(READ));
        CMP(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndexedXSlow(READ), WRAP_BANK);
        snes->OpenBus    = (uint8)(val >> 8);
        CMP(val);
    }
}
void SCPUState::OpD2E1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectE1(READ));
    CMP(val);
}
void SCPUState::OpD2E0M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectE0(READ));
    CMP(val);
}
void SCPUState::OpD2E0M0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndirectE0(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    CMP(val);
}
void SCPUState::OpD2Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectSlow(READ));
        CMP(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndirectSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        CMP(val);
    }
}
void SCPUState::OpC1E1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedIndirectE1(READ));
    CMP(val);
}
void SCPUState::OpC1E0M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedIndirectE0(READ));
    CMP(val);
}
void SCPUState::OpC1E0M0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndexedIndirectE0(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    CMP(val);
}
void SCPUState::OpC1Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedIndirectSlow(READ));
        CMP(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndexedIndirectSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        CMP(val);
    }
}
void SCPUState::OpD1E1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedE1(READ));
    CMP(val);
}
void SCPUState::OpD1E0M1X1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedE0X1(READ));
    CMP(val);
}
void SCPUState::OpD1E0M0X1(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndirectIndexedE0X1(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    CMP(val);
}
void SCPUState::OpD1E0M1X0(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedE0X0(READ));
    CMP(val);
}
void SCPUState::OpD1E0M0X0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndirectIndexedE0X0(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    CMP(val);
}
void SCPUState::OpD1Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedSlow(READ));
        CMP(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndirectIndexedSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        CMP(val);
    }
}
void SCPUState::OpC7M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectLong(READ));
    CMP(val);
}
void SCPUState::OpC7M0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndirectLong(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    CMP(val);
}
void SCPUState::OpC7Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectLongSlow(READ));
        CMP(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndirectLongSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        CMP(val);
    }
}
void SCPUState::OpD7M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedLong(READ));
    CMP(val);
}
void SCPUState::OpD7M0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndirectIndexedLong(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    CMP(val);
}
void SCPUState::OpD7Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedLongSlow(READ));
        CMP(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndirectIndexedLongSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        CMP(val);
    }
}
void SCPUState::OpCDM1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(Absolute(READ));
    CMP(val);
}
void SCPUState::OpCDM0(void)
{
    uint16 val = snes->mem->S9xGetWord(Absolute(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    CMP(val);
}
void SCPUState::OpCDSlow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteSlow(READ));
        CMP(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        CMP(val);
    }
}
void SCPUState::OpDDM1X1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedXX1(READ));
    CMP(val);
}
void SCPUState::OpDDM0X1(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedXX1(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    CMP(val);
}
void SCPUState::OpDDM1X0(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedXX0(READ));
    CMP(val);
}
void SCPUState::OpDDM0X0(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedXX0(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    CMP(val);
}
void SCPUState::OpDDSlow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedXSlow(READ));
        CMP(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedXSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        CMP(val);
    }
}
void SCPUState::OpD9M1X1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedYX1(READ));
    CMP(val);
}
void SCPUState::OpD9M0X1(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedYX1(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    CMP(val);
}
void SCPUState::OpD9M1X0(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedYX0(READ));
    CMP(val);
}
void SCPUState::OpD9M0X0(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedYX0(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    CMP(val);
}
void SCPUState::OpD9Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedYSlow(READ));
        CMP(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedYSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        CMP(val);
    }
}
void SCPUState::OpCFM1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteLong(READ));
    CMP(val);
}
void SCPUState::OpCFM0(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteLong(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    CMP(val);
}
void SCPUState::OpCFSlow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteLongSlow(READ));
        CMP(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteLongSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        CMP(val);
    }
}
void SCPUState::OpDFM1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteLongIndexedX(READ));
    CMP(val);
}
void SCPUState::OpDFM0(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteLongIndexedX(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    CMP(val);
}
void SCPUState::OpDFSlow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteLongIndexedXSlow(READ));
        CMP(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteLongIndexedXSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        CMP(val);
    }
}
void SCPUState::OpC3M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(StackRelative(READ));
    CMP(val);
}
void SCPUState::OpC3M0(void)
{
    uint16 val = snes->mem->S9xGetWord(StackRelative(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    CMP(val);
}
void SCPUState::OpC3Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(StackRelativeSlow(READ));
        CMP(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(StackRelativeSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        CMP(val);
    }
}
void SCPUState::OpD3M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(StackRelativeIndirectIndexed(READ));
    CMP(val);
}
void SCPUState::OpD3M0(void)
{
    uint16 val = snes->mem->S9xGetWord(StackRelativeIndirectIndexed(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    CMP(val);
}
void SCPUState::OpD3Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(StackRelativeIndirectIndexedSlow(READ));
        CMP(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(StackRelativeIndirectIndexedSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        CMP(val);
    }
}
void SCPUState::OpE0X1(void)
{
    int16 Int16 = (int16)Registers.X.B.l - (int16)Immediate8(READ);
    ICPU._Carry = Int16 >= 0;
    SetZN((uint8)Int16);
}
void SCPUState::OpE0X0(void)
{
    int32 Int32 = (int32)Registers.X.W - (int32)Immediate16(READ);
    ICPU._Carry = Int32 >= 0;
    SetZN((uint16)Int32);
}
void SCPUState::OpE0Slow(void)
{
    if (CheckIndex()) {
        int16 Int16 = (int16)Registers.XL - (int16)Immediate8Slow(READ);
        ICPU._Carry = Int16 >= 0;
        SetZN((uint8)Int16);
    } else {
        int32 Int32 = (int32)Registers.X.W - (int32)Immediate16Slow(READ);
        ICPU._Carry = Int32 >= 0;
        SetZN((uint16)Int32);
    }
}
void SCPUState::OpE4X1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(Direct(READ));
    CPX(val);
}
void SCPUState::OpE4X0(void)
{
    uint16 val = snes->mem->S9xGetWord(Direct(READ), WRAP_BANK);
    snes->OpenBus    = (uint8)(val >> 8);
    CPX(val);
}
void SCPUState::OpE4Slow(void)
{
    if ((Registers.P.B.l & 16)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectSlow(READ));
        CPX(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectSlow(READ), WRAP_BANK);
        snes->OpenBus    = (uint8)(val >> 8);
        CPX(val);
    }
}
void SCPUState::OpECX1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(Absolute(READ));
    CPX(val);
}
void SCPUState::OpECX0(void)
{
    uint16 val = snes->mem->S9xGetWord(Absolute(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    CPX(val);
}
void SCPUState::OpECSlow(void)
{
    if ((Registers.P.B.l & 16)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteSlow(READ));
        CPX(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        CPX(val);
    }
}
void SCPUState::OpC0X1(void)
{
    int16 Int16 = (int16)Registers.Y.B.l - (int16)Immediate8(READ);
    ICPU._Carry = Int16 >= 0;
    SetZN((uint8)Int16);
}
void SCPUState::OpC0X0(void)
{
    int32 Int32 = (int32)Registers.Y.W - (int32)Immediate16(READ);
    ICPU._Carry = Int32 >= 0;
    SetZN((uint16)Int32);
}
void SCPUState::OpC0Slow(void)
{
    if (CheckIndex()) {
        int16 Int16 = (int16)Registers.YL - (int16)Immediate8Slow(READ);
        ICPU._Carry = Int16 >= 0;
        SetZN((uint8)Int16);
    } else {
        int32 Int32 = (int32)Registers.Y.W - (int32)Immediate16Slow(READ);
        ICPU._Carry = Int32 >= 0;
        SetZN((uint16)Int32);
    }
}
void SCPUState::OpC4X1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(Direct(READ));
    CPY(val);
}
void SCPUState::OpC4X0(void)
{
    uint16 val = snes->mem->S9xGetWord(Direct(READ), WRAP_BANK);
    snes->OpenBus    = (uint8)(val >> 8);
    CPY(val);
}
void SCPUState::OpC4Slow(void)
{
    if ((Registers.P.B.l & 16)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectSlow(READ));
        CPY(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectSlow(READ), WRAP_BANK);
        snes->OpenBus    = (uint8)(val >> 8);
        CPY(val);
    }
}
void SCPUState::OpCCX1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(Absolute(READ));
    CPY(val);
}
void SCPUState::OpCCX0(void)
{
    uint16 val = snes->mem->S9xGetWord(Absolute(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    CPY(val);
}
void SCPUState::OpCCSlow(void)
{
    if ((Registers.P.B.l & 16)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteSlow(READ));
        CPY(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        CPY(val);
    }
}
void SCPUState::Op3AM1(void)
{
    {
        Cycles += (6);
        while (Cycles >= NextEvent)
            S9xDoHEventProcessing();
    };
    Registers.A.B.l--;
    SetZN(Registers.A.B.l);
}
void SCPUState::Op3AM0(void)
{
    AddCycles(ONE_CYCLE);
    Registers.A.W--;
    SetZN(Registers.A.W);
}
void SCPUState::Op3ASlow(void)
{
    AddCycles(ONE_CYCLE);
    if (CheckMemory()) {
        Registers.AL--;
        SetZN(Registers.AL);
    } else {
        Registers.A.W--;
        SetZN(Registers.A.W);
    }
}
void SCPUState::OpC6M1(void)
{
    DEC8(Direct(MODIFY));
}
void SCPUState::OpC6M0(void)
{
    DEC16(Direct(MODIFY), WRAP_BANK);
}
void SCPUState::OpC6Slow(void)
{
    if ((Registers.P.B.l & 32))
        DEC8(DirectSlow(MODIFY));
    else
        DEC16(DirectSlow(MODIFY), WRAP_BANK);
}
void SCPUState::OpD6E1(void)
{
    DEC8(DirectIndexedXE1(MODIFY));
}
void SCPUState::OpD6E0M1(void)
{
    DEC8(DirectIndexedXE0(MODIFY));
}
void SCPUState::OpD6E0M0(void)
{
    DEC16(DirectIndexedXE0(MODIFY), WRAP_BANK);
}
void SCPUState::OpD6Slow(void)
{
    if ((Registers.P.B.l & 32))
        DEC8(DirectIndexedXSlow(MODIFY));
    else
        DEC16(DirectIndexedXSlow(MODIFY), WRAP_BANK);
}
void SCPUState::OpCEM1(void)
{
    DEC8(Absolute(MODIFY));
}
void SCPUState::OpCEM0(void)
{
    DEC16(Absolute(MODIFY), WRAP_NONE);
}
void SCPUState::OpCESlow(void)
{
    if ((Registers.P.B.l & 32))
        DEC8(AbsoluteSlow(MODIFY));
    else
        DEC16(AbsoluteSlow(MODIFY), WRAP_NONE);
}
void SCPUState::OpDEM1X1(void)
{
    DEC8(AbsoluteIndexedXX1(MODIFY));
}
void SCPUState::OpDEM0X1(void)
{
    DEC16(AbsoluteIndexedXX1(MODIFY), WRAP_NONE);
}
void SCPUState::OpDEM1X0(void)
{
    DEC8(AbsoluteIndexedXX0(MODIFY));
}
void SCPUState::OpDEM0X0(void)
{
    DEC16(AbsoluteIndexedXX0(MODIFY), WRAP_NONE);
}
void SCPUState::OpDESlow(void)
{
    if ((Registers.P.B.l & 32))
        DEC8(AbsoluteIndexedXSlow(MODIFY));
    else
        DEC16(AbsoluteIndexedXSlow(MODIFY), WRAP_NONE);
}
void SCPUState::Op49M1(void)
{
    Registers.A.B.l ^= Immediate8(READ);
    SetZN(Registers.A.B.l);
}
void SCPUState::Op49M0(void)
{
    Registers.A.W ^= Immediate16(READ);
    SetZN(Registers.A.W);
}
void SCPUState::Op49Slow(void)
{
    if (CheckMemory()) {
        Registers.AL ^= Immediate8Slow(READ);
        SetZN(Registers.AL);
    } else {
        Registers.A.W ^= Immediate16Slow(READ);
        SetZN(Registers.A.W);
    }
}
void SCPUState::Op45M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(Direct(READ));
    EOR(val);
}
void SCPUState::Op45M0(void)
{
    uint16 val = snes->mem->S9xGetWord(Direct(READ), WRAP_BANK);
    snes->OpenBus    = (uint8)(val >> 8);
    EOR(val);
}
void SCPUState::Op45Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectSlow(READ));
        EOR(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectSlow(READ), WRAP_BANK);
        snes->OpenBus    = (uint8)(val >> 8);
        EOR(val);
    }
}
void SCPUState::Op55E1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedXE1(READ));
    EOR(val);
}
void SCPUState::Op55E0M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedXE0(READ));
    EOR(val);
}
void SCPUState::Op55E0M0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndexedXE0(READ), WRAP_BANK);
    snes->OpenBus    = (uint8)(val >> 8);
    EOR(val);
}
void SCPUState::Op55Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedXSlow(READ));
        EOR(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndexedXSlow(READ), WRAP_BANK);
        snes->OpenBus    = (uint8)(val >> 8);
        EOR(val);
    }
}
void SCPUState::Op52E1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectE1(READ));
    EOR(val);
}
void SCPUState::Op52E0M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectE0(READ));
    EOR(val);
}
void SCPUState::Op52E0M0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndirectE0(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    EOR(val);
}
void SCPUState::Op52Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectSlow(READ));
        EOR(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndirectSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        EOR(val);
    }
}
void SCPUState::Op41E1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedIndirectE1(READ));
    EOR(val);
}
void SCPUState::Op41E0M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedIndirectE0(READ));
    EOR(val);
}
void SCPUState::Op41E0M0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndexedIndirectE0(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    EOR(val);
}
void SCPUState::Op41Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedIndirectSlow(READ));
        EOR(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndexedIndirectSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        EOR(val);
    }
}
void SCPUState::Op51E1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedE1(READ));
    EOR(val);
}
void SCPUState::Op51E0M1X1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedE0X1(READ));
    EOR(val);
}
void SCPUState::Op51E0M0X1(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndirectIndexedE0X1(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    EOR(val);
}
void SCPUState::Op51E0M1X0(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedE0X0(READ));
    EOR(val);
}
void SCPUState::Op51E0M0X0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndirectIndexedE0X0(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    EOR(val);
}
void SCPUState::Op51Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedSlow(READ));
        EOR(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndirectIndexedSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        EOR(val);
    }
}
void SCPUState::Op47M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectLong(READ));
    EOR(val);
}
void SCPUState::Op47M0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndirectLong(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    EOR(val);
}
void SCPUState::Op47Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectLongSlow(READ));
        EOR(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndirectLongSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        EOR(val);
    }
}
void SCPUState::Op57M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedLong(READ));
    EOR(val);
}
void SCPUState::Op57M0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndirectIndexedLong(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    EOR(val);
}
void SCPUState::Op57Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedLongSlow(READ));
        EOR(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndirectIndexedLongSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        EOR(val);
    }
}
void SCPUState::Op4DM1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(Absolute(READ));
    EOR(val);
}
void SCPUState::Op4DM0(void)
{
    uint16 val = snes->mem->S9xGetWord(Absolute(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    EOR(val);
}
void SCPUState::Op4DSlow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteSlow(READ));
        EOR(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        EOR(val);
    }
}
void SCPUState::Op5DM1X1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedXX1(READ));
    EOR(val);
}
void SCPUState::Op5DM0X1(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedXX1(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    EOR(val);
}
void SCPUState::Op5DM1X0(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedXX0(READ));
    EOR(val);
}
void SCPUState::Op5DM0X0(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedXX0(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    EOR(val);
}
void SCPUState::Op5DSlow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedXSlow(READ));
        EOR(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedXSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        EOR(val);
    }
}
void SCPUState::Op59M1X1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedYX1(READ));
    EOR(val);
}
void SCPUState::Op59M0X1(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedYX1(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    EOR(val);
}
void SCPUState::Op59M1X0(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedYX0(READ));
    EOR(val);
}
void SCPUState::Op59M0X0(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedYX0(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    EOR(val);
}
void SCPUState::Op59Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedYSlow(READ));
        EOR(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedYSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        EOR(val);
    }
}
void SCPUState::Op4FM1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteLong(READ));
    EOR(val);
}
void SCPUState::Op4FM0(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteLong(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    EOR(val);
}
void SCPUState::Op4FSlow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteLongSlow(READ));
        EOR(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteLongSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        EOR(val);
    }
}
void SCPUState::Op5FM1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteLongIndexedX(READ));
    EOR(val);
}
void SCPUState::Op5FM0(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteLongIndexedX(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    EOR(val);
}
void SCPUState::Op5FSlow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteLongIndexedXSlow(READ));
        EOR(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteLongIndexedXSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        EOR(val);
    }
}
void SCPUState::Op43M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(StackRelative(READ));
    EOR(val);
}
void SCPUState::Op43M0(void)
{
    uint16 val = snes->mem->S9xGetWord(StackRelative(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    EOR(val);
}
void SCPUState::Op43Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(StackRelativeSlow(READ));
        EOR(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(StackRelativeSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        EOR(val);
    }
}
void SCPUState::Op53M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(StackRelativeIndirectIndexed(READ));
    EOR(val);
}
void SCPUState::Op53M0(void)
{
    uint16 val = snes->mem->S9xGetWord(StackRelativeIndirectIndexed(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    EOR(val);
}
void SCPUState::Op53Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(StackRelativeIndirectIndexedSlow(READ));
        EOR(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(StackRelativeIndirectIndexedSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        EOR(val);
    }
}
void SCPUState::Op1AM1(void)
{
    {
        Cycles += (6);
        while (Cycles >= NextEvent)
            S9xDoHEventProcessing();
    };
    Registers.A.B.l++;
    SetZN(Registers.A.B.l);
}
void SCPUState::Op1AM0(void)
{
    AddCycles(ONE_CYCLE);
    Registers.A.W++;
    SetZN(Registers.A.W);
}
void SCPUState::Op1ASlow(void)
{
    AddCycles(ONE_CYCLE);
    if (CheckMemory()) {
        Registers.AL++;
        SetZN(Registers.AL);
    } else {
        Registers.A.W++;
        SetZN(Registers.A.W);
    }
}
void SCPUState::OpE6M1(void)
{
    INC8(Direct(MODIFY));
}
void SCPUState::OpE6M0(void)
{
    INC16(Direct(MODIFY), WRAP_BANK);
}
void SCPUState::OpE6Slow(void)
{
    if ((Registers.P.B.l & 32))
        INC8(DirectSlow(MODIFY));
    else
        INC16(DirectSlow(MODIFY), WRAP_BANK);
}
void SCPUState::OpF6E1(void)
{
    INC8(DirectIndexedXE1(MODIFY));
}
void SCPUState::OpF6E0M1(void)
{
    INC8(DirectIndexedXE0(MODIFY));
}
void SCPUState::OpF6E0M0(void)
{
    INC16(DirectIndexedXE0(MODIFY), WRAP_BANK);
}
void SCPUState::OpF6Slow(void)
{
    if ((Registers.P.B.l & 32))
        INC8(DirectIndexedXSlow(MODIFY));
    else
        INC16(DirectIndexedXSlow(MODIFY), WRAP_BANK);
}
void SCPUState::OpEEM1(void)
{
    INC8(Absolute(MODIFY));
}
void SCPUState::OpEEM0(void)
{
    INC16(Absolute(MODIFY), WRAP_NONE);
}
void SCPUState::OpEESlow(void)
{
    if ((Registers.P.B.l & 32))
        INC8(AbsoluteSlow(MODIFY));
    else
        INC16(AbsoluteSlow(MODIFY), WRAP_NONE);
}
void SCPUState::OpFEM1X1(void)
{
    INC8(AbsoluteIndexedXX1(MODIFY));
}
void SCPUState::OpFEM0X1(void)
{
    INC16(AbsoluteIndexedXX1(MODIFY), WRAP_NONE);
}
void SCPUState::OpFEM1X0(void)
{
    INC8(AbsoluteIndexedXX0(MODIFY));
}
void SCPUState::OpFEM0X0(void)
{
    INC16(AbsoluteIndexedXX0(MODIFY), WRAP_NONE);
}
void SCPUState::OpFESlow(void)
{
    if ((Registers.P.B.l & 32))
        INC8(AbsoluteIndexedXSlow(MODIFY));
    else
        INC16(AbsoluteIndexedXSlow(MODIFY), WRAP_NONE);
}
void SCPUState::OpA9M1(void)
{
    Registers.A.B.l = Immediate8(READ);
    SetZN(Registers.A.B.l);
}
void SCPUState::OpA9M0(void)
{
    Registers.A.W = Immediate16(READ);
    SetZN(Registers.A.W);
}
void SCPUState::OpA9Slow(void)
{
    if (CheckMemory()) {
        Registers.AL = Immediate8Slow(READ);
        SetZN(Registers.AL);
    } else {
        Registers.A.W = Immediate16Slow(READ);
        SetZN(Registers.A.W);
    }
}
void SCPUState::OpA5M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(Direct(READ));
    LDA(val);
}
void SCPUState::OpA5M0(void)
{
    uint16 val = snes->mem->S9xGetWord(Direct(READ), WRAP_BANK);
    snes->OpenBus    = (uint8)(val >> 8);
    LDA(val);
}
void SCPUState::OpA5Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectSlow(READ));
        LDA(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectSlow(READ), WRAP_BANK);
        snes->OpenBus    = (uint8)(val >> 8);
        LDA(val);
    }
}
void SCPUState::OpB5E1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedXE1(READ));
    LDA(val);
}
void SCPUState::OpB5E0M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedXE0(READ));
    LDA(val);
}
void SCPUState::OpB5E0M0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndexedXE0(READ), WRAP_BANK);
    snes->OpenBus    = (uint8)(val >> 8);
    LDA(val);
}
void SCPUState::OpB5Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedXSlow(READ));
        LDA(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndexedXSlow(READ), WRAP_BANK);
        snes->OpenBus    = (uint8)(val >> 8);
        LDA(val);
    }
}
void SCPUState::OpB2E1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectE1(READ));
    LDA(val);
}
void SCPUState::OpB2E0M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectE0(READ));
    LDA(val);
}
void SCPUState::OpB2E0M0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndirectE0(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    LDA(val);
}
void SCPUState::OpB2Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectSlow(READ));
        LDA(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndirectSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        LDA(val);
    }
}
void SCPUState::OpA1E1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedIndirectE1(READ));
    LDA(val);
}
void SCPUState::OpA1E0M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedIndirectE0(READ));
    LDA(val);
}
void SCPUState::OpA1E0M0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndexedIndirectE0(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    LDA(val);
}
void SCPUState::OpA1Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedIndirectSlow(READ));
        LDA(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndexedIndirectSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        LDA(val);
    }
}
void SCPUState::OpB1E1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedE1(READ));
    LDA(val);
}
void SCPUState::OpB1E0M1X1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedE0X1(READ));
    LDA(val);
}
void SCPUState::OpB1E0M0X1(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndirectIndexedE0X1(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    LDA(val);
}
void SCPUState::OpB1E0M1X0(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedE0X0(READ));
    LDA(val);
}
void SCPUState::OpB1E0M0X0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndirectIndexedE0X0(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    LDA(val);
}
void SCPUState::OpB1Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedSlow(READ));
        LDA(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndirectIndexedSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        LDA(val);
    }
}
void SCPUState::OpA7M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectLong(READ));
    LDA(val);
}
void SCPUState::OpA7M0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndirectLong(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    LDA(val);
}
void SCPUState::OpA7Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectLongSlow(READ));
        LDA(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndirectLongSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        LDA(val);
    }
}
void SCPUState::OpB7M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedLong(READ));
    LDA(val);
}
void SCPUState::OpB7M0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndirectIndexedLong(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    LDA(val);
}
void SCPUState::OpB7Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedLongSlow(READ));
        LDA(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndirectIndexedLongSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        LDA(val);
    }
}
void SCPUState::OpADM1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(Absolute(READ));
    LDA(val);
}
void SCPUState::OpADM0(void)
{
    uint16 val = snes->mem->S9xGetWord(Absolute(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    LDA(val);
}
void SCPUState::OpADSlow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteSlow(READ));
        LDA(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        LDA(val);
    }
}
void SCPUState::OpBDM1X1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedXX1(READ));
    LDA(val);
}
void SCPUState::OpBDM0X1(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedXX1(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    LDA(val);
}
void SCPUState::OpBDM1X0(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedXX0(READ));
    LDA(val);
}
void SCPUState::OpBDM0X0(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedXX0(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    LDA(val);
}
void SCPUState::OpBDSlow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedXSlow(READ));
        LDA(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedXSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        LDA(val);
    }
}
void SCPUState::OpB9M1X1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedYX1(READ));
    LDA(val);
}
void SCPUState::OpB9M0X1(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedYX1(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    LDA(val);
}
void SCPUState::OpB9M1X0(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedYX0(READ));
    LDA(val);
}
void SCPUState::OpB9M0X0(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedYX0(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    LDA(val);
}
void SCPUState::OpB9Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedYSlow(READ));
        LDA(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedYSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        LDA(val);
    }
}
void SCPUState::OpAFM1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteLong(READ));
    LDA(val);
}
void SCPUState::OpAFM0(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteLong(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    LDA(val);
}
void SCPUState::OpAFSlow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteLongSlow(READ));
        LDA(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteLongSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        LDA(val);
    }
}
void SCPUState::OpBFM1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteLongIndexedX(READ));
    LDA(val);
}
void SCPUState::OpBFM0(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteLongIndexedX(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    LDA(val);
}
void SCPUState::OpBFSlow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteLongIndexedXSlow(READ));
        LDA(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteLongIndexedXSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        LDA(val);
    }
}
void SCPUState::OpA3M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(StackRelative(READ));
    LDA(val);
}
void SCPUState::OpA3M0(void)
{
    uint16 val = snes->mem->S9xGetWord(StackRelative(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    LDA(val);
}
void SCPUState::OpA3Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(StackRelativeSlow(READ));
        LDA(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(StackRelativeSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        LDA(val);
    }
}
void SCPUState::OpB3M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(StackRelativeIndirectIndexed(READ));
    LDA(val);
}
void SCPUState::OpB3M0(void)
{
    uint16 val = snes->mem->S9xGetWord(StackRelativeIndirectIndexed(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    LDA(val);
}
void SCPUState::OpB3Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(StackRelativeIndirectIndexedSlow(READ));
        LDA(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(StackRelativeIndirectIndexedSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        LDA(val);
    }
}
void SCPUState::OpA2X1(void)
{
    Registers.X.B.l = Immediate8(READ);
    SetZN(Registers.X.B.l);
}
void SCPUState::OpA2X0(void)
{
    Registers.X.W = Immediate16(READ);
    SetZN(Registers.X.W);
}
void SCPUState::OpA2Slow(void)
{
    if (CheckIndex()) {
        Registers.XL = Immediate8Slow(READ);
        SetZN(Registers.XL);
    } else {
        Registers.X.W = Immediate16Slow(READ);
        SetZN(Registers.X.W);
    }
}
void SCPUState::OpA6X1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(Direct(READ));
    LDX(val);
}
void SCPUState::OpA6X0(void)
{
    uint16 val = snes->mem->S9xGetWord(Direct(READ), WRAP_BANK);
    snes->OpenBus    = (uint8)(val >> 8);
    LDX(val);
}
void SCPUState::OpA6Slow(void)
{
    if ((Registers.P.B.l & 16)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectSlow(READ));
        LDX(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectSlow(READ), WRAP_BANK);
        snes->OpenBus    = (uint8)(val >> 8);
        LDX(val);
    }
}
void SCPUState::OpB6E1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedYE1(READ));
    LDX(val);
}
void SCPUState::OpB6E0X1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedYE0(READ));
    LDX(val);
}
void SCPUState::OpB6E0X0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndexedYE0(READ), WRAP_BANK);
    snes->OpenBus    = (uint8)(val >> 8);
    LDX(val);
}
void SCPUState::OpB6Slow(void)
{
    if ((Registers.P.B.l & 16)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedYSlow(READ));
        LDX(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndexedYSlow(READ), WRAP_BANK);
        snes->OpenBus    = (uint8)(val >> 8);
        LDX(val);
    }
}
void SCPUState::OpAEX1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(Absolute(READ));
    LDX(val);
}
void SCPUState::OpAEX0(void)
{
    uint16 val = snes->mem->S9xGetWord(Absolute(READ), WRAP_BANK);
    snes->OpenBus    = (uint8)(val >> 8);
    LDX(val);
}
void SCPUState::OpAESlow(void)
{
    if ((Registers.P.B.l & 16)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteSlow(READ));
        LDX(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteSlow(READ), WRAP_BANK);
        snes->OpenBus    = (uint8)(val >> 8);
        LDX(val);
    }
}
void SCPUState::OpBEX1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedYX1(READ));
    LDX(val);
}
void SCPUState::OpBEX0(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedYX0(READ), WRAP_BANK);
    snes->OpenBus    = (uint8)(val >> 8);
    LDX(val);
}
void SCPUState::OpBESlow(void)
{
    if ((Registers.P.B.l & 16)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedYSlow(READ));
        LDX(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedYSlow(READ), WRAP_BANK);
        snes->OpenBus    = (uint8)(val >> 8);
        LDX(val);
    }
}
void SCPUState::OpA0X1(void)
{
    Registers.Y.B.l = Immediate8(READ);
    SetZN(Registers.Y.B.l);
}
void SCPUState::OpA0X0(void)
{
    Registers.Y.W = Immediate16(READ);
    SetZN(Registers.Y.W);
}
void SCPUState::OpA0Slow(void)
{
    if (CheckIndex()) {
        Registers.YL = Immediate8Slow(READ);
        SetZN(Registers.YL);
    } else {
        Registers.Y.W = Immediate16Slow(READ);
        SetZN(Registers.Y.W);
    }
}
void SCPUState::OpA4X1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(Direct(READ));
    LDY(val);
}
void SCPUState::OpA4X0(void)
{
    uint16 val = snes->mem->S9xGetWord(Direct(READ), WRAP_BANK);
    snes->OpenBus    = (uint8)(val >> 8);
    LDY(val);
}
void SCPUState::OpA4Slow(void)
{
    if ((Registers.P.B.l & 16)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectSlow(READ));
        LDY(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectSlow(READ), WRAP_BANK);
        snes->OpenBus    = (uint8)(val >> 8);
        LDY(val);
    }
}
void SCPUState::OpB4E1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedXE1(READ));
    LDY(val);
}
void SCPUState::OpB4E0X1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedXE0(READ));
    LDY(val);
}
void SCPUState::OpB4E0X0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndexedXE0(READ), WRAP_BANK);
    snes->OpenBus    = (uint8)(val >> 8);
    LDY(val);
}
void SCPUState::OpB4Slow(void)
{
    if ((Registers.P.B.l & 16)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedXSlow(READ));
        LDY(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndexedXSlow(READ), WRAP_BANK);
        snes->OpenBus    = (uint8)(val >> 8);
        LDY(val);
    }
}
void SCPUState::OpACX1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(Absolute(READ));
    LDY(val);
}
void SCPUState::OpACX0(void)
{
    uint16 val = snes->mem->S9xGetWord(Absolute(READ), WRAP_BANK);
    snes->OpenBus    = (uint8)(val >> 8);
    LDY(val);
}
void SCPUState::OpACSlow(void)
{
    if ((Registers.P.B.l & 16)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteSlow(READ));
        LDY(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteSlow(READ), WRAP_BANK);
        snes->OpenBus    = (uint8)(val >> 8);
        LDY(val);
    }
}
void SCPUState::OpBCX1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedXX1(READ));
    LDY(val);
}
void SCPUState::OpBCX0(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedXX0(READ), WRAP_BANK);
    snes->OpenBus    = (uint8)(val >> 8);
    LDY(val);
}
void SCPUState::OpBCSlow(void)
{
    if ((Registers.P.B.l & 16)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedXSlow(READ));
        LDY(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedXSlow(READ), WRAP_BANK);
        snes->OpenBus    = (uint8)(val >> 8);
        LDY(val);
    }
}
void SCPUState::Op4AM1(void)
{
    {
        Cycles += (6);
        while (Cycles >= NextEvent)
            S9xDoHEventProcessing();
    };
    ICPU._Carry = Registers.A.B.l & 1;
    Registers.A.B.l >>= 1;
    SetZN(Registers.A.B.l);
}
void SCPUState::Op4AM0(void)
{
    AddCycles(ONE_CYCLE);
    ICPU._Carry = Registers.A.W & 1;
    Registers.A.W >>= 1;
    SetZN(Registers.A.W);
}
void SCPUState::Op4ASlow(void)
{
    AddCycles(ONE_CYCLE);
    if (CheckMemory()) {
        ICPU._Carry = Registers.AL & 1;
        Registers.AL >>= 1;
        SetZN(Registers.AL);
    } else {
        ICPU._Carry = Registers.A.W & 1;
        Registers.A.W >>= 1;
        SetZN(Registers.A.W);
    }
}
void SCPUState::Op46M1(void)
{
    LSR8(Direct(MODIFY));
}
void SCPUState::Op46M0(void)
{
    LSR16(Direct(MODIFY), WRAP_BANK);
}
void SCPUState::Op46Slow(void)
{
    if ((Registers.P.B.l & 32))
        LSR8(DirectSlow(MODIFY));
    else
        LSR16(DirectSlow(MODIFY), WRAP_BANK);
}
void SCPUState::Op56E1(void)
{
    LSR8(DirectIndexedXE1(MODIFY));
}
void SCPUState::Op56E0M1(void)
{
    LSR8(DirectIndexedXE0(MODIFY));
}
void SCPUState::Op56E0M0(void)
{
    LSR16(DirectIndexedXE0(MODIFY), WRAP_BANK);
}
void SCPUState::Op56Slow(void)
{
    if ((Registers.P.B.l & 32))
        LSR8(DirectIndexedXSlow(MODIFY));
    else
        LSR16(DirectIndexedXSlow(MODIFY), WRAP_BANK);
}
void SCPUState::Op4EM1(void)
{
    LSR8(Absolute(MODIFY));
}
void SCPUState::Op4EM0(void)
{
    LSR16(Absolute(MODIFY), WRAP_NONE);
}
void SCPUState::Op4ESlow(void)
{
    if ((Registers.P.B.l & 32))
        LSR8(AbsoluteSlow(MODIFY));
    else
        LSR16(AbsoluteSlow(MODIFY), WRAP_NONE);
}
void SCPUState::Op5EM1X1(void)
{
    LSR8(AbsoluteIndexedXX1(MODIFY));
}
void SCPUState::Op5EM0X1(void)
{
    LSR16(AbsoluteIndexedXX1(MODIFY), WRAP_NONE);
}
void SCPUState::Op5EM1X0(void)
{
    LSR8(AbsoluteIndexedXX0(MODIFY));
}
void SCPUState::Op5EM0X0(void)
{
    LSR16(AbsoluteIndexedXX0(MODIFY), WRAP_NONE);
}
void SCPUState::Op5ESlow(void)
{
    if ((Registers.P.B.l & 32))
        LSR8(AbsoluteIndexedXSlow(MODIFY));
    else
        LSR16(AbsoluteIndexedXSlow(MODIFY), WRAP_NONE);
}
void SCPUState::Op09M1(void)
{
    Registers.A.B.l |= Immediate8(READ);
    SetZN(Registers.A.B.l);
}
void SCPUState::Op09M0(void)
{
    Registers.A.W |= Immediate16(READ);
    SetZN(Registers.A.W);
}
void SCPUState::Op09Slow(void)
{
    if (CheckMemory()) {
        Registers.AL |= Immediate8Slow(READ);
        SetZN(Registers.AL);
    } else {
        Registers.A.W |= Immediate16Slow(READ);
        SetZN(Registers.A.W);
    }
}
void SCPUState::Op05M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(Direct(READ));
    ORA(val);
}
void SCPUState::Op05M0(void)
{
    uint16 val = snes->mem->S9xGetWord(Direct(READ), WRAP_BANK);
    snes->OpenBus    = (uint8)(val >> 8);
    ORA(val);
}
void SCPUState::Op05Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectSlow(READ));
        ORA(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectSlow(READ), WRAP_BANK);
        snes->OpenBus    = (uint8)(val >> 8);
        ORA(val);
    }
}
void SCPUState::Op15E1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedXE1(READ));
    ORA(val);
}
void SCPUState::Op15E0M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedXE0(READ));
    ORA(val);
}
void SCPUState::Op15E0M0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndexedXE0(READ), WRAP_BANK);
    snes->OpenBus    = (uint8)(val >> 8);
    ORA(val);
}
void SCPUState::Op15Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedXSlow(READ));
        ORA(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndexedXSlow(READ), WRAP_BANK);
        snes->OpenBus    = (uint8)(val >> 8);
        ORA(val);
    }
}
void SCPUState::Op12E1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectE1(READ));
    ORA(val);
}
void SCPUState::Op12E0M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectE0(READ));
    ORA(val);
}
void SCPUState::Op12E0M0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndirectE0(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    ORA(val);
}
void SCPUState::Op12Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectSlow(READ));
        ORA(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndirectSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        ORA(val);
    }
}
void SCPUState::Op01E1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedIndirectE1(READ));
    ORA(val);
}
void SCPUState::Op01E0M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedIndirectE0(READ));
    ORA(val);
}
void SCPUState::Op01E0M0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndexedIndirectE0(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    ORA(val);
}
void SCPUState::Op01Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedIndirectSlow(READ));
        ORA(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndexedIndirectSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        ORA(val);
    }
}
void SCPUState::Op11E1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedE1(READ));
    ORA(val);
}
void SCPUState::Op11E0M1X1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedE0X1(READ));
    ORA(val);
}
void SCPUState::Op11E0M0X1(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndirectIndexedE0X1(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    ORA(val);
}
void SCPUState::Op11E0M1X0(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedE0X0(READ));
    ORA(val);
}
void SCPUState::Op11E0M0X0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndirectIndexedE0X0(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    ORA(val);
}
void SCPUState::Op11Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedSlow(READ));
        ORA(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndirectIndexedSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        ORA(val);
    }
}
void SCPUState::Op07M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectLong(READ));
    ORA(val);
}
void SCPUState::Op07M0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndirectLong(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    ORA(val);
}
void SCPUState::Op07Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectLongSlow(READ));
        ORA(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndirectLongSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        ORA(val);
    }
}
void SCPUState::Op17M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedLong(READ));
    ORA(val);
}
void SCPUState::Op17M0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndirectIndexedLong(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    ORA(val);
}
void SCPUState::Op17Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedLongSlow(READ));
        ORA(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndirectIndexedLongSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        ORA(val);
    }
}
void SCPUState::Op0DM1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(Absolute(READ));
    ORA(val);
}
void SCPUState::Op0DM0(void)
{
    uint16 val = snes->mem->S9xGetWord(Absolute(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    ORA(val);
}
void SCPUState::Op0DSlow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteSlow(READ));
        ORA(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        ORA(val);
    }
}
void SCPUState::Op1DM1X1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedXX1(READ));
    ORA(val);
}
void SCPUState::Op1DM0X1(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedXX1(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    ORA(val);
}
void SCPUState::Op1DM1X0(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedXX0(READ));
    ORA(val);
}
void SCPUState::Op1DM0X0(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedXX0(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    ORA(val);
}
void SCPUState::Op1DSlow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedXSlow(READ));
        ORA(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedXSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        ORA(val);
    }
}
void SCPUState::Op19M1X1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedYX1(READ));
    ORA(val);
}
void SCPUState::Op19M0X1(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedYX1(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    ORA(val);
}
void SCPUState::Op19M1X0(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedYX0(READ));
    ORA(val);
}
void SCPUState::Op19M0X0(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedYX0(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    ORA(val);
}
void SCPUState::Op19Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedYSlow(READ));
        ORA(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedYSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        ORA(val);
    }
}
void SCPUState::Op0FM1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteLong(READ));
    ORA(val);
}
void SCPUState::Op0FM0(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteLong(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    ORA(val);
}
void SCPUState::Op0FSlow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteLongSlow(READ));
        ORA(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteLongSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        ORA(val);
    }
}
void SCPUState::Op1FM1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteLongIndexedX(READ));
    ORA(val);
}
void SCPUState::Op1FM0(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteLongIndexedX(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    ORA(val);
}
void SCPUState::Op1FSlow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteLongIndexedXSlow(READ));
        ORA(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteLongIndexedXSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        ORA(val);
    }
}
void SCPUState::Op03M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(StackRelative(READ));
    ORA(val);
}
void SCPUState::Op03M0(void)
{
    uint16 val = snes->mem->S9xGetWord(StackRelative(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    ORA(val);
}
void SCPUState::Op03Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(StackRelativeSlow(READ));
        ORA(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(StackRelativeSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        ORA(val);
    }
}
void SCPUState::Op13M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(StackRelativeIndirectIndexed(READ));
    ORA(val);
}
void SCPUState::Op13M0(void)
{
    uint16 val = snes->mem->S9xGetWord(StackRelativeIndirectIndexed(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    ORA(val);
}
void SCPUState::Op13Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(StackRelativeIndirectIndexedSlow(READ));
        ORA(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(StackRelativeIndirectIndexedSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        ORA(val);
    }
}
void SCPUState::Op2AM1(void)
{
    {
        Cycles += (6);
        while (Cycles >= NextEvent)
            S9xDoHEventProcessing();
    };
    uint16 w        = (((uint16)Registers.A.B.l) << 1) | (ICPU._Carry);
    ICPU._Carry     = w >= 0x100;
    Registers.A.B.l = (uint8)w;
    SetZN(Registers.A.B.l);
}
void SCPUState::Op2AM0(void)
{
    AddCycles(ONE_CYCLE);
    uint32 w      = (((uint32)Registers.A.W) << 1) | CheckCarry();
    ICPU._Carry   = w >= 0x10000;
    Registers.A.W = (uint16)w;
    SetZN(Registers.A.W);
}
void SCPUState::Op2ASlow(void)
{
    AddCycles(ONE_CYCLE);
    if (CheckMemory()) {
        uint16 w     = (((uint16)Registers.AL) << 1) | CheckCarry();
        ICPU._Carry  = w >= 0x100;
        Registers.AL = (uint8)w;
        SetZN(Registers.AL);
    } else {
        uint32 w      = (((uint32)Registers.A.W) << 1) | CheckCarry();
        ICPU._Carry   = w >= 0x10000;
        Registers.A.W = (uint16)w;
        SetZN(Registers.A.W);
    }
}
void SCPUState::Op26M1(void)
{
    ROL8(Direct(MODIFY));
}
void SCPUState::Op26M0(void)
{
    ROL16(Direct(MODIFY), WRAP_BANK);
}
void SCPUState::Op26Slow(void)
{
    if ((Registers.P.B.l & 32))
        ROL8(DirectSlow(MODIFY));
    else
        ROL16(DirectSlow(MODIFY), WRAP_BANK);
}
void SCPUState::Op36E1(void)
{
    ROL8(DirectIndexedXE1(MODIFY));
}
void SCPUState::Op36E0M1(void)
{
    ROL8(DirectIndexedXE0(MODIFY));
}
void SCPUState::Op36E0M0(void)
{
    ROL16(DirectIndexedXE0(MODIFY), WRAP_BANK);
}
void SCPUState::Op36Slow(void)
{
    if ((Registers.P.B.l & 32))
        ROL8(DirectIndexedXSlow(MODIFY));
    else
        ROL16(DirectIndexedXSlow(MODIFY), WRAP_BANK);
}
void SCPUState::Op2EM1(void)
{
    ROL8(Absolute(MODIFY));
}
void SCPUState::Op2EM0(void)
{
    ROL16(Absolute(MODIFY), WRAP_NONE);
}
void SCPUState::Op2ESlow(void)
{
    if ((Registers.P.B.l & 32))
        ROL8(AbsoluteSlow(MODIFY));
    else
        ROL16(AbsoluteSlow(MODIFY), WRAP_NONE);
}
void SCPUState::Op3EM1X1(void)
{
    ROL8(AbsoluteIndexedXX1(MODIFY));
}
void SCPUState::Op3EM0X1(void)
{
    ROL16(AbsoluteIndexedXX1(MODIFY), WRAP_NONE);
}
void SCPUState::Op3EM1X0(void)
{
    ROL8(AbsoluteIndexedXX0(MODIFY));
}
void SCPUState::Op3EM0X0(void)
{
    ROL16(AbsoluteIndexedXX0(MODIFY), WRAP_NONE);
}
void SCPUState::Op3ESlow(void)
{
    if ((Registers.P.B.l & 32))
        ROL8(AbsoluteIndexedXSlow(MODIFY));
    else
        ROL16(AbsoluteIndexedXSlow(MODIFY), WRAP_NONE);
}
void SCPUState::Op6AM1(void)
{
    {
        Cycles += (6);
        while (Cycles >= NextEvent)
            S9xDoHEventProcessing();
    };
    uint16 w    = ((uint16)Registers.A.B.l) | (((uint16)(ICPU._Carry)) << 8);
    ICPU._Carry = w & 1;
    w >>= 1;
    Registers.A.B.l = (uint8)w;
    SetZN(Registers.A.B.l);
}
void SCPUState::Op6AM0(void)
{
    AddCycles(ONE_CYCLE);
    uint32 w    = ((uint32)Registers.A.W) | (((uint32)CheckCarry()) << 16);
    ICPU._Carry = w & 1;
    w >>= 1;
    Registers.A.W = (uint16)w;
    SetZN(Registers.A.W);
}
void SCPUState::Op6ASlow(void)
{
    AddCycles(ONE_CYCLE);
    if (CheckMemory()) {
        uint16 w    = ((uint16)Registers.AL) | (((uint16)CheckCarry()) << 8);
        ICPU._Carry = w & 1;
        w >>= 1;
        Registers.AL = (uint8)w;
        SetZN(Registers.AL);
    } else {
        uint32 w    = ((uint32)Registers.A.W) | (((uint32)CheckCarry()) << 16);
        ICPU._Carry = w & 1;
        w >>= 1;
        Registers.A.W = (uint16)w;
        SetZN(Registers.A.W);
    }
}
void SCPUState::Op66M1(void)
{
    ROR8(Direct(MODIFY));
}
void SCPUState::Op66M0(void)
{
    ROR16(Direct(MODIFY), WRAP_BANK);
}
void SCPUState::Op66Slow(void)
{
    if ((Registers.P.B.l & 32))
        ROR8(DirectSlow(MODIFY));
    else
        ROR16(DirectSlow(MODIFY), WRAP_BANK);
}
void SCPUState::Op76E1(void)
{
    ROR8(DirectIndexedXE1(MODIFY));
}
void SCPUState::Op76E0M1(void)
{
    ROR8(DirectIndexedXE0(MODIFY));
}
void SCPUState::Op76E0M0(void)
{
    ROR16(DirectIndexedXE0(MODIFY), WRAP_BANK);
}
void SCPUState::Op76Slow(void)
{
    if ((Registers.P.B.l & 32))
        ROR8(DirectIndexedXSlow(MODIFY));
    else
        ROR16(DirectIndexedXSlow(MODIFY), WRAP_BANK);
}
void SCPUState::Op6EM1(void)
{
    ROR8(Absolute(MODIFY));
}
void SCPUState::Op6EM0(void)
{
    ROR16(Absolute(MODIFY), WRAP_NONE);
}
void SCPUState::Op6ESlow(void)
{
    if ((Registers.P.B.l & 32))
        ROR8(AbsoluteSlow(MODIFY));
    else
        ROR16(AbsoluteSlow(MODIFY), WRAP_NONE);
}
void SCPUState::Op7EM1X1(void)
{
    ROR8(AbsoluteIndexedXX1(MODIFY));
}
void SCPUState::Op7EM0X1(void)
{
    ROR16(AbsoluteIndexedXX1(MODIFY), WRAP_NONE);
}
void SCPUState::Op7EM1X0(void)
{
    ROR8(AbsoluteIndexedXX0(MODIFY));
}
void SCPUState::Op7EM0X0(void)
{
    ROR16(AbsoluteIndexedXX0(MODIFY), WRAP_NONE);
}
void SCPUState::Op7ESlow(void)
{
    if ((Registers.P.B.l & 32))
        ROR8(AbsoluteIndexedXSlow(MODIFY));
    else
        ROR16(AbsoluteIndexedXSlow(MODIFY), WRAP_NONE);
}
void SCPUState::OpE9M1(void)
{
    SBC(Immediate8(READ));
}
void SCPUState::OpE9M0(void)
{
    SBC(Immediate16(READ));
}
void SCPUState::OpE9Slow(void)
{
    if (CheckMemory())
        SBC(Immediate8Slow(READ));
    else
        SBC(Immediate16Slow(READ));
}
void SCPUState::OpE5M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(Direct(READ));
    SBC(val);
}
void SCPUState::OpE5M0(void)
{
    uint16 val = snes->mem->S9xGetWord(Direct(READ), WRAP_BANK);
    snes->OpenBus    = (uint8)(val >> 8);
    SBC(val);
}
void SCPUState::OpE5Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectSlow(READ));
        SBC(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectSlow(READ), WRAP_BANK);
        snes->OpenBus    = (uint8)(val >> 8);
        SBC(val);
    }
}
void SCPUState::OpF5E1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedXE1(READ));
    SBC(val);
}
void SCPUState::OpF5E0M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedXE0(READ));
    SBC(val);
}
void SCPUState::OpF5E0M0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndexedXE0(READ), WRAP_BANK);
    snes->OpenBus    = (uint8)(val >> 8);
    SBC(val);
}
void SCPUState::OpF5Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedXSlow(READ));
        SBC(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndexedXSlow(READ), WRAP_BANK);
        snes->OpenBus    = (uint8)(val >> 8);
        SBC(val);
    }
}
void SCPUState::OpF2E1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectE1(READ));
    SBC(val);
}
void SCPUState::OpF2E0M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectE0(READ));
    SBC(val);
}
void SCPUState::OpF2E0M0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndirectE0(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    SBC(val);
}
void SCPUState::OpF2Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectSlow(READ));
        SBC(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndirectSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        SBC(val);
    }
}
void SCPUState::OpE1E1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedIndirectE1(READ));
    SBC(val);
}
void SCPUState::OpE1E0M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedIndirectE0(READ));
    SBC(val);
}
void SCPUState::OpE1E0M0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndexedIndirectE0(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    SBC(val);
}
void SCPUState::OpE1Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndexedIndirectSlow(READ));
        SBC(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndexedIndirectSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        SBC(val);
    }
}
void SCPUState::OpF1E1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedE1(READ));
    SBC(val);
}
void SCPUState::OpF1E0M1X1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedE0X1(READ));
    SBC(val);
}
void SCPUState::OpF1E0M0X1(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndirectIndexedE0X1(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    SBC(val);
}
void SCPUState::OpF1E0M1X0(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedE0X0(READ));
    SBC(val);
}
void SCPUState::OpF1E0M0X0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndirectIndexedE0X0(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    SBC(val);
}
void SCPUState::OpF1Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedSlow(READ));
        SBC(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndirectIndexedSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        SBC(val);
    }
}
void SCPUState::OpE7M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectLong(READ));
    SBC(val);
}
void SCPUState::OpE7M0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndirectLong(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    SBC(val);
}
void SCPUState::OpE7Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectLongSlow(READ));
        SBC(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndirectLongSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        SBC(val);
    }
}
void SCPUState::OpF7M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedLong(READ));
    SBC(val);
}
void SCPUState::OpF7M0(void)
{
    uint16 val = snes->mem->S9xGetWord(DirectIndirectIndexedLong(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    SBC(val);
}
void SCPUState::OpF7Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(DirectIndirectIndexedLongSlow(READ));
        SBC(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(DirectIndirectIndexedLongSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        SBC(val);
    }
}
void SCPUState::OpEDM1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(Absolute(READ));
    SBC(val);
}
void SCPUState::OpEDM0(void)
{
    uint16 val = snes->mem->S9xGetWord(Absolute(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    SBC(val);
}
void SCPUState::OpEDSlow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteSlow(READ));
        SBC(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        SBC(val);
    }
}
void SCPUState::OpFDM1X1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedXX1(READ));
    SBC(val);
}
void SCPUState::OpFDM0X1(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedXX1(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    SBC(val);
}
void SCPUState::OpFDM1X0(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedXX0(READ));
    SBC(val);
}
void SCPUState::OpFDM0X0(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedXX0(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    SBC(val);
}
void SCPUState::OpFDSlow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedXSlow(READ));
        SBC(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedXSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        SBC(val);
    }
}
void SCPUState::OpF9M1X1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedYX1(READ));
    SBC(val);
}
void SCPUState::OpF9M0X1(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedYX1(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    SBC(val);
}
void SCPUState::OpF9M1X0(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedYX0(READ));
    SBC(val);
}
void SCPUState::OpF9M0X0(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedYX0(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    SBC(val);
}
void SCPUState::OpF9Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteIndexedYSlow(READ));
        SBC(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteIndexedYSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        SBC(val);
    }
}
void SCPUState::OpEFM1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteLong(READ));
    SBC(val);
}
void SCPUState::OpEFM0(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteLong(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    SBC(val);
}
void SCPUState::OpEFSlow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteLongSlow(READ));
        SBC(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteLongSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        SBC(val);
    }
}
void SCPUState::OpFFM1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteLongIndexedX(READ));
    SBC(val);
}
void SCPUState::OpFFM0(void)
{
    uint16 val = snes->mem->S9xGetWord(AbsoluteLongIndexedX(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    SBC(val);
}
void SCPUState::OpFFSlow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(AbsoluteLongIndexedXSlow(READ));
        SBC(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(AbsoluteLongIndexedXSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        SBC(val);
    }
}
void SCPUState::OpE3M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(StackRelative(READ));
    SBC(val);
}
void SCPUState::OpE3M0(void)
{
    uint16 val = snes->mem->S9xGetWord(StackRelative(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    SBC(val);
}
void SCPUState::OpE3Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(StackRelativeSlow(READ));
        SBC(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(StackRelativeSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        SBC(val);
    }
}
void SCPUState::OpF3M1(void)
{
    uint8 val = snes->OpenBus = snes->mem->S9xGetByte(StackRelativeIndirectIndexed(READ));
    SBC(val);
}
void SCPUState::OpF3M0(void)
{
    uint16 val = snes->mem->S9xGetWord(StackRelativeIndirectIndexed(READ), WRAP_NONE);
    snes->OpenBus    = (uint8)(val >> 8);
    SBC(val);
}
void SCPUState::OpF3Slow(void)
{
    if ((Registers.P.B.l & 32)) {
        uint8 val = snes->OpenBus = snes->mem->S9xGetByte(StackRelativeIndirectIndexedSlow(READ));
        SBC(val);
    } else {
        uint16 val = snes->mem->S9xGetWord(StackRelativeIndirectIndexedSlow(READ), WRAP_NONE);
        snes->OpenBus    = (uint8)(val >> 8);
        SBC(val);
    }
}
void SCPUState::Op85M1(void)
{
    STA8(Direct(WRITE));
}
void SCPUState::Op85M0(void)
{
    STA16(Direct(WRITE), WRAP_BANK);
}
void SCPUState::Op85Slow(void)
{
    if ((Registers.P.B.l & 32))
        STA8(DirectSlow(WRITE));
    else
        STA16(DirectSlow(WRITE), WRAP_BANK);
}
void SCPUState::Op95E1(void)
{
    STA8(DirectIndexedXE1(WRITE));
}
void SCPUState::Op95E0M1(void)
{
    STA8(DirectIndexedXE0(WRITE));
}
void SCPUState::Op95E0M0(void)
{
    STA16(DirectIndexedXE0(WRITE), WRAP_BANK);
}
void SCPUState::Op95Slow(void)
{
    if ((Registers.P.B.l & 32))
        STA8(DirectIndexedXSlow(WRITE));
    else
        STA16(DirectIndexedXSlow(WRITE), WRAP_BANK);
}
void SCPUState::Op92E1(void)
{
    STA8(DirectIndirectE1(WRITE));
}
void SCPUState::Op92E0M1(void)
{
    STA8(DirectIndirectE0(WRITE));
}
void SCPUState::Op92E0M0(void)
{
    STA16(DirectIndirectE0(WRITE), WRAP_NONE);
}
void SCPUState::Op92Slow(void)
{
    if ((Registers.P.B.l & 32))
        STA8(DirectIndirectSlow(WRITE));
    else
        STA16(DirectIndirectSlow(WRITE), WRAP_NONE);
}
void SCPUState::Op81E1(void)
{
    STA8(DirectIndexedIndirectE1(WRITE));
}
void SCPUState::Op81E0M1(void)
{
    STA8(DirectIndexedIndirectE0(WRITE));
}
void SCPUState::Op81E0M0(void)
{
    STA16(DirectIndexedIndirectE0(WRITE), WRAP_NONE);
}
void SCPUState::Op81Slow(void)
{
    if ((Registers.P.B.l & 32))
        STA8(DirectIndexedIndirectSlow(WRITE));
    else
        STA16(DirectIndexedIndirectSlow(WRITE), WRAP_NONE);
}
void SCPUState::Op91E1(void)
{
    STA8(DirectIndirectIndexedE1(WRITE));
}
void SCPUState::Op91E0M1X1(void)
{
    STA8(DirectIndirectIndexedE0X1(WRITE));
}
void SCPUState::Op91E0M0X1(void)
{
    STA16(DirectIndirectIndexedE0X1(WRITE), WRAP_NONE);
}
void SCPUState::Op91E0M1X0(void)
{
    STA8(DirectIndirectIndexedE0X0(WRITE));
}
void SCPUState::Op91E0M0X0(void)
{
    STA16(DirectIndirectIndexedE0X0(WRITE), WRAP_NONE);
}
void SCPUState::Op91Slow(void)
{
    if ((Registers.P.B.l & 32))
        STA8(DirectIndirectIndexedSlow(WRITE));
    else
        STA16(DirectIndirectIndexedSlow(WRITE), WRAP_NONE);
}
void SCPUState::Op87M1(void)
{
    STA8(DirectIndirectLong(WRITE));
}
void SCPUState::Op87M0(void)
{
    STA16(DirectIndirectLong(WRITE), WRAP_NONE);
}
void SCPUState::Op87Slow(void)
{
    if ((Registers.P.B.l & 32))
        STA8(DirectIndirectLongSlow(WRITE));
    else
        STA16(DirectIndirectLongSlow(WRITE), WRAP_NONE);
}
void SCPUState::Op97M1(void)
{
    STA8(DirectIndirectIndexedLong(WRITE));
}
void SCPUState::Op97M0(void)
{
    STA16(DirectIndirectIndexedLong(WRITE), WRAP_NONE);
}
void SCPUState::Op97Slow(void)
{
    if ((Registers.P.B.l & 32))
        STA8(DirectIndirectIndexedLongSlow(WRITE));
    else
        STA16(DirectIndirectIndexedLongSlow(WRITE), WRAP_NONE);
}
void SCPUState::Op8DM1(void)
{
    STA8(Absolute(WRITE));
}
void SCPUState::Op8DM0(void)
{
    STA16(Absolute(WRITE), WRAP_NONE);
}
void SCPUState::Op8DSlow(void)
{
    if ((Registers.P.B.l & 32))
        STA8(AbsoluteSlow(WRITE));
    else
        STA16(AbsoluteSlow(WRITE), WRAP_NONE);
}
void SCPUState::Op9DM1X1(void)
{
    STA8(AbsoluteIndexedXX1(WRITE));
}
void SCPUState::Op9DM0X1(void)
{
    STA16(AbsoluteIndexedXX1(WRITE), WRAP_NONE);
}
void SCPUState::Op9DM1X0(void)
{
    STA8(AbsoluteIndexedXX0(WRITE));
}
void SCPUState::Op9DM0X0(void)
{
    STA16(AbsoluteIndexedXX0(WRITE), WRAP_NONE);
}
void SCPUState::Op9DSlow(void)
{
    if ((Registers.P.B.l & 32))
        STA8(AbsoluteIndexedXSlow(WRITE));
    else
        STA16(AbsoluteIndexedXSlow(WRITE), WRAP_NONE);
}
void SCPUState::Op99M1X1(void)
{
    STA8(AbsoluteIndexedYX1(WRITE));
}
void SCPUState::Op99M0X1(void)
{
    STA16(AbsoluteIndexedYX1(WRITE), WRAP_NONE);
}
void SCPUState::Op99M1X0(void)
{
    STA8(AbsoluteIndexedYX0(WRITE));
}
void SCPUState::Op99M0X0(void)
{
    STA16(AbsoluteIndexedYX0(WRITE), WRAP_NONE);
}
void SCPUState::Op99Slow(void)
{
    if ((Registers.P.B.l & 32))
        STA8(AbsoluteIndexedYSlow(WRITE));
    else
        STA16(AbsoluteIndexedYSlow(WRITE), WRAP_NONE);
}
void SCPUState::Op8FM1(void)
{
    STA8(AbsoluteLong(WRITE));
}
void SCPUState::Op8FM0(void)
{
    STA16(AbsoluteLong(WRITE), WRAP_NONE);
}
void SCPUState::Op8FSlow(void)
{
    if ((Registers.P.B.l & 32))
        STA8(AbsoluteLongSlow(WRITE));
    else
        STA16(AbsoluteLongSlow(WRITE), WRAP_NONE);
}
void SCPUState::Op9FM1(void)
{
    STA8(AbsoluteLongIndexedX(WRITE));
}
void SCPUState::Op9FM0(void)
{
    STA16(AbsoluteLongIndexedX(WRITE), WRAP_NONE);
}
void SCPUState::Op9FSlow(void)
{
    if ((Registers.P.B.l & 32))
        STA8(AbsoluteLongIndexedXSlow(WRITE));
    else
        STA16(AbsoluteLongIndexedXSlow(WRITE), WRAP_NONE);
}
void SCPUState::Op83M1(void)
{
    STA8(StackRelative(WRITE));
}
void SCPUState::Op83M0(void)
{
    STA16(StackRelative(WRITE), WRAP_NONE);
}
void SCPUState::Op83Slow(void)
{
    if ((Registers.P.B.l & 32))
        STA8(StackRelativeSlow(WRITE));
    else
        STA16(StackRelativeSlow(WRITE), WRAP_NONE);
}
void SCPUState::Op93M1(void)
{
    STA8(StackRelativeIndirectIndexed(WRITE));
}
void SCPUState::Op93M0(void)
{
    STA16(StackRelativeIndirectIndexed(WRITE), WRAP_NONE);
}
void SCPUState::Op93Slow(void)
{
    if ((Registers.P.B.l & 32))
        STA8(StackRelativeIndirectIndexedSlow(WRITE));
    else
        STA16(StackRelativeIndirectIndexedSlow(WRITE), WRAP_NONE);
}
void SCPUState::Op86X1(void)
{
    STX8(Direct(WRITE));
}
void SCPUState::Op86X0(void)
{
    STX16(Direct(WRITE), WRAP_BANK);
}
void SCPUState::Op86Slow(void)
{
    if ((Registers.P.B.l & 16))
        STX8(DirectSlow(WRITE));
    else
        STX16(DirectSlow(WRITE), WRAP_BANK);
}
void SCPUState::Op96E1(void)
{
    STX8(DirectIndexedYE1(WRITE));
}
void SCPUState::Op96E0X1(void)
{
    STX8(DirectIndexedYE0(WRITE));
}
void SCPUState::Op96E0X0(void)
{
    STX16(DirectIndexedYE0(WRITE), WRAP_BANK);
}
void SCPUState::Op96Slow(void)
{
    if ((Registers.P.B.l & 16))
        STX8(DirectIndexedYSlow(WRITE));
    else
        STX16(DirectIndexedYSlow(WRITE), WRAP_BANK);
}
void SCPUState::Op8EX1(void)
{
    STX8(Absolute(WRITE));
}
void SCPUState::Op8EX0(void)
{
    STX16(Absolute(WRITE), WRAP_BANK);
}
void SCPUState::Op8ESlow(void)
{
    if ((Registers.P.B.l & 16))
        STX8(AbsoluteSlow(WRITE));
    else
        STX16(AbsoluteSlow(WRITE), WRAP_BANK);
}
void SCPUState::Op84X1(void)
{
    STY8(Direct(WRITE));
}
void SCPUState::Op84X0(void)
{
    STY16(Direct(WRITE), WRAP_BANK);
}
void SCPUState::Op84Slow(void)
{
    if ((Registers.P.B.l & 16))
        STY8(DirectSlow(WRITE));
    else
        STY16(DirectSlow(WRITE), WRAP_BANK);
}
void SCPUState::Op94E1(void)
{
    STY8(DirectIndexedXE1(WRITE));
}
void SCPUState::Op94E0X1(void)
{
    STY8(DirectIndexedXE0(WRITE));
}
void SCPUState::Op94E0X0(void)
{
    STY16(DirectIndexedXE0(WRITE), WRAP_BANK);
}
void SCPUState::Op94Slow(void)
{
    if ((Registers.P.B.l & 16))
        STY8(DirectIndexedXSlow(WRITE));
    else
        STY16(DirectIndexedXSlow(WRITE), WRAP_BANK);
}
void SCPUState::Op8CX1(void)
{
    STY8(Absolute(WRITE));
}
void SCPUState::Op8CX0(void)
{
    STY16(Absolute(WRITE), WRAP_BANK);
}
void SCPUState::Op8CSlow(void)
{
    if ((Registers.P.B.l & 16))
        STY8(AbsoluteSlow(WRITE));
    else
        STY16(AbsoluteSlow(WRITE), WRAP_BANK);
}
void SCPUState::Op64M1(void)
{
    STZ8(Direct(WRITE));
}
void SCPUState::Op64M0(void)
{
    STZ16(Direct(WRITE), WRAP_BANK);
}
void SCPUState::Op64Slow(void)
{
    if ((Registers.P.B.l & 32))
        STZ8(DirectSlow(WRITE));
    else
        STZ16(DirectSlow(WRITE), WRAP_BANK);
}
void SCPUState::Op74E1(void)
{
    STZ8(DirectIndexedXE1(WRITE));
}
void SCPUState::Op74E0M1(void)
{
    STZ8(DirectIndexedXE0(WRITE));
}
void SCPUState::Op74E0M0(void)
{
    STZ16(DirectIndexedXE0(WRITE), WRAP_BANK);
}
void SCPUState::Op74Slow(void)
{
    if ((Registers.P.B.l & 32))
        STZ8(DirectIndexedXSlow(WRITE));
    else
        STZ16(DirectIndexedXSlow(WRITE), WRAP_BANK);
}
void SCPUState::Op9CM1(void)
{
    STZ8(Absolute(WRITE));
}
void SCPUState::Op9CM0(void)
{
    STZ16(Absolute(WRITE), WRAP_NONE);
}
void SCPUState::Op9CSlow(void)
{
    if ((Registers.P.B.l & 32))
        STZ8(AbsoluteSlow(WRITE));
    else
        STZ16(AbsoluteSlow(WRITE), WRAP_NONE);
}
void SCPUState::Op9EM1X1(void)
{
    STZ8(AbsoluteIndexedXX1(WRITE));
}
void SCPUState::Op9EM0X1(void)
{
    STZ16(AbsoluteIndexedXX1(WRITE), WRAP_NONE);
}
void SCPUState::Op9EM1X0(void)
{
    STZ8(AbsoluteIndexedXX0(WRITE));
}
void SCPUState::Op9EM0X0(void)
{
    STZ16(AbsoluteIndexedXX0(WRITE), WRAP_NONE);
}
void SCPUState::Op9ESlow(void)
{
    if ((Registers.P.B.l & 32))
        STZ8(AbsoluteIndexedXSlow(WRITE));
    else
        STZ16(AbsoluteIndexedXSlow(WRITE), WRAP_NONE);
}
void SCPUState::Op14M1(void)
{
    TRB8(Direct(MODIFY));
}
void SCPUState::Op14M0(void)
{
    TRB16(Direct(MODIFY), WRAP_BANK);
}
void SCPUState::Op14Slow(void)
{
    if ((Registers.P.B.l & 32))
        TRB8(DirectSlow(MODIFY));
    else
        TRB16(DirectSlow(MODIFY), WRAP_BANK);
}
void SCPUState::Op1CM1(void)
{
    TRB8(Absolute(MODIFY));
}
void SCPUState::Op1CM0(void)
{
    TRB16(Absolute(MODIFY), WRAP_BANK);
}
void SCPUState::Op1CSlow(void)
{
    if ((Registers.P.B.l & 32))
        TRB8(AbsoluteSlow(MODIFY));
    else
        TRB16(AbsoluteSlow(MODIFY), WRAP_BANK);
}
void SCPUState::Op04M1(void)
{
    TSB8(Direct(MODIFY));
}
void SCPUState::Op04M0(void)
{
    TSB16(Direct(MODIFY), WRAP_BANK);
}
void SCPUState::Op04Slow(void)
{
    if ((Registers.P.B.l & 32))
        TSB8(DirectSlow(MODIFY));
    else
        TSB16(DirectSlow(MODIFY), WRAP_BANK);
}
void SCPUState::Op0CM1(void)
{
    TSB8(Absolute(MODIFY));
}
void SCPUState::Op0CM0(void)
{
    TSB16(Absolute(MODIFY), WRAP_BANK);
}
void SCPUState::Op0CSlow(void)
{
    if ((Registers.P.B.l & 32))
        TSB8(AbsoluteSlow(MODIFY));
    else
        TSB16(AbsoluteSlow(MODIFY), WRAP_BANK);
}
void SCPUState::Op90E0(void)
{
    pair newPC;
    newPC.W = Relative(JUMP);
    if (!(ICPU._Carry)) {
        {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if (0 && Registers.PC.B.xPCh != newPC.B.h) {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if ((Registers.PC.W.xPC & ~((0x1000) - 1)) != (newPC.W & ~((0x1000) - 1)))
            snes->mem->S9xSetPCBase(ICPU.ShiftedPB + newPC.W);
        else
            Registers.PC.W.xPC = newPC.W;
    }
}
void SCPUState::Op90E1(void)
{
    pair newPC;
    newPC.W = Relative(JUMP);
    if (!(ICPU._Carry)) {
        {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if (1 && Registers.PC.B.xPCh != newPC.B.h) {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if ((Registers.PC.W.xPC & ~((0x1000) - 1)) != (newPC.W & ~((0x1000) - 1)))
            snes->mem->S9xSetPCBase(ICPU.ShiftedPB + newPC.W);
        else
            Registers.PC.W.xPC = newPC.W;
    }
}
void SCPUState::Op90Slow(void)
{
    pair newPC;
    newPC.W = RelativeSlow(JUMP);
    if (!(ICPU._Carry)) {
        {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if ((Registers.P.W & 256) && Registers.PC.B.xPCh != newPC.B.h) {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if ((Registers.PC.W.xPC & ~((0x1000) - 1)) != (newPC.W & ~((0x1000) - 1)))
            snes->mem->S9xSetPCBase(ICPU.ShiftedPB + newPC.W);
        else
            Registers.PC.W.xPC = newPC.W;
    }
}
void SCPUState::OpB0E0(void)
{
    pair newPC;
    newPC.W = Relative(JUMP);
    if ((ICPU._Carry)) {
        {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if (0 && Registers.PC.B.xPCh != newPC.B.h) {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if ((Registers.PC.W.xPC & ~((0x1000) - 1)) != (newPC.W & ~((0x1000) - 1)))
            snes->mem->S9xSetPCBase(ICPU.ShiftedPB + newPC.W);
        else
            Registers.PC.W.xPC = newPC.W;
    }
}
void SCPUState::OpB0E1(void)
{
    pair newPC;
    newPC.W = Relative(JUMP);
    if ((ICPU._Carry)) {
        {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if (1 && Registers.PC.B.xPCh != newPC.B.h) {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if ((Registers.PC.W.xPC & ~((0x1000) - 1)) != (newPC.W & ~((0x1000) - 1)))
            snes->mem->S9xSetPCBase(ICPU.ShiftedPB + newPC.W);
        else
            Registers.PC.W.xPC = newPC.W;
    }
}
void SCPUState::OpB0Slow(void)
{
    pair newPC;
    newPC.W = RelativeSlow(JUMP);
    if ((ICPU._Carry)) {
        {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if ((Registers.P.W & 256) && Registers.PC.B.xPCh != newPC.B.h) {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if ((Registers.PC.W.xPC & ~((0x1000) - 1)) != (newPC.W & ~((0x1000) - 1)))
            snes->mem->S9xSetPCBase(ICPU.ShiftedPB + newPC.W);
        else
            Registers.PC.W.xPC = newPC.W;
    }
}
void SCPUState::OpF0E0(void)
{
    pair newPC;
    newPC.W = Relative(JUMP);
    if (ICPU._Zero == 0) {
        {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if (0 && Registers.PC.B.xPCh != newPC.B.h) {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if ((Registers.PC.W.xPC & ~((0x1000) - 1)) != (newPC.W & ~((0x1000) - 1)))
            snes->mem->S9xSetPCBase(ICPU.ShiftedPB + newPC.W);
        else
            Registers.PC.W.xPC = newPC.W;
    }
}
void SCPUState::OpF0E1(void)
{
    pair newPC;
    newPC.W = Relative(JUMP);
    if (ICPU._Zero == 0) {
        {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if (1 && Registers.PC.B.xPCh != newPC.B.h) {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if ((Registers.PC.W.xPC & ~((0x1000) - 1)) != (newPC.W & ~((0x1000) - 1)))
            snes->mem->S9xSetPCBase(ICPU.ShiftedPB + newPC.W);
        else
            Registers.PC.W.xPC = newPC.W;
    }
}
void SCPUState::OpF0Slow(void)
{
    pair newPC;
    newPC.W = RelativeSlow(JUMP);
    if (ICPU._Zero == 0) {
        {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if ((Registers.P.W & 256) && Registers.PC.B.xPCh != newPC.B.h) {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if ((Registers.PC.W.xPC & ~((0x1000) - 1)) != (newPC.W & ~((0x1000) - 1)))
            snes->mem->S9xSetPCBase(ICPU.ShiftedPB + newPC.W);
        else
            Registers.PC.W.xPC = newPC.W;
    }
}
void SCPUState::Op30E0(void)
{
    pair newPC;
    newPC.W = Relative(JUMP);
    if ((ICPU._Negative & 0x80)) {
        {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if (0 && Registers.PC.B.xPCh != newPC.B.h) {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if ((Registers.PC.W.xPC & ~((0x1000) - 1)) != (newPC.W & ~((0x1000) - 1)))
            snes->mem->S9xSetPCBase(ICPU.ShiftedPB + newPC.W);
        else
            Registers.PC.W.xPC = newPC.W;
    }
}
void SCPUState::Op30E1(void)
{
    pair newPC;
    newPC.W = Relative(JUMP);
    if ((ICPU._Negative & 0x80)) {
        {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if (1 && Registers.PC.B.xPCh != newPC.B.h) {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if ((Registers.PC.W.xPC & ~((0x1000) - 1)) != (newPC.W & ~((0x1000) - 1)))
            snes->mem->S9xSetPCBase(ICPU.ShiftedPB + newPC.W);
        else
            Registers.PC.W.xPC = newPC.W;
    }
}
void SCPUState::Op30Slow(void)
{
    pair newPC;
    newPC.W = RelativeSlow(JUMP);
    if ((ICPU._Negative & 0x80)) {
        {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if ((Registers.P.W & 256) && Registers.PC.B.xPCh != newPC.B.h) {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if ((Registers.PC.W.xPC & ~((0x1000) - 1)) != (newPC.W & ~((0x1000) - 1)))
            snes->mem->S9xSetPCBase(ICPU.ShiftedPB + newPC.W);
        else
            Registers.PC.W.xPC = newPC.W;
    }
}
void SCPUState::OpD0E0(void)
{
    pair newPC;
    newPC.W = Relative(JUMP);
    if (!(ICPU._Zero == 0)) {
        {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if (0 && Registers.PC.B.xPCh != newPC.B.h) {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if ((Registers.PC.W.xPC & ~((0x1000) - 1)) != (newPC.W & ~((0x1000) - 1)))
            snes->mem->S9xSetPCBase(ICPU.ShiftedPB + newPC.W);
        else
            Registers.PC.W.xPC = newPC.W;
    }
}
void SCPUState::OpD0E1(void)
{
    pair newPC;
    newPC.W = Relative(JUMP);
    if (!(ICPU._Zero == 0)) {
        {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if (1 && Registers.PC.B.xPCh != newPC.B.h) {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if ((Registers.PC.W.xPC & ~((0x1000) - 1)) != (newPC.W & ~((0x1000) - 1)))
            snes->mem->S9xSetPCBase(ICPU.ShiftedPB + newPC.W);
        else
            Registers.PC.W.xPC = newPC.W;
    }
}
void SCPUState::OpD0Slow(void)
{
    pair newPC;
    newPC.W = RelativeSlow(JUMP);
    if (!(ICPU._Zero == 0)) {
        {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if ((Registers.P.W & 256) && Registers.PC.B.xPCh != newPC.B.h) {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if ((Registers.PC.W.xPC & ~((0x1000) - 1)) != (newPC.W & ~((0x1000) - 1)))
            snes->mem->S9xSetPCBase(ICPU.ShiftedPB + newPC.W);
        else
            Registers.PC.W.xPC = newPC.W;
    }
}
void SCPUState::Op10E0(void)
{
    pair newPC;
    newPC.W = Relative(JUMP);
    if (!(ICPU._Negative & 0x80)) {
        {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if (0 && Registers.PC.B.xPCh != newPC.B.h) {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if ((Registers.PC.W.xPC & ~((0x1000) - 1)) != (newPC.W & ~((0x1000) - 1)))
            snes->mem->S9xSetPCBase(ICPU.ShiftedPB + newPC.W);
        else
            Registers.PC.W.xPC = newPC.W;
    }
}
void SCPUState::Op10E1(void)
{
    pair newPC;
    newPC.W = Relative(JUMP);
    if (!(ICPU._Negative & 0x80)) {
        {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if (1 && Registers.PC.B.xPCh != newPC.B.h) {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if ((Registers.PC.W.xPC & ~((0x1000) - 1)) != (newPC.W & ~((0x1000) - 1)))
            snes->mem->S9xSetPCBase(ICPU.ShiftedPB + newPC.W);
        else
            Registers.PC.W.xPC = newPC.W;
    }
}
void SCPUState::Op10Slow(void)
{
    pair newPC;
    newPC.W = RelativeSlow(JUMP);
    if (!(ICPU._Negative & 0x80)) {
        {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if ((Registers.P.W & 256) && Registers.PC.B.xPCh != newPC.B.h) {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if ((Registers.PC.W.xPC & ~((0x1000) - 1)) != (newPC.W & ~((0x1000) - 1)))
            snes->mem->S9xSetPCBase(ICPU.ShiftedPB + newPC.W);
        else
            Registers.PC.W.xPC = newPC.W;
    }
}
void SCPUState::Op80E0(void)
{
    pair newPC;
    newPC.W = Relative(JUMP);
    if (1) {
        {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if (0 && Registers.PC.B.xPCh != newPC.B.h) {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if ((Registers.PC.W.xPC & ~((0x1000) - 1)) != (newPC.W & ~((0x1000) - 1)))
            snes->mem->S9xSetPCBase(ICPU.ShiftedPB + newPC.W);
        else
            Registers.PC.W.xPC = newPC.W;
    }
}
void SCPUState::Op80E1(void)
{
    pair newPC;
    newPC.W = Relative(JUMP);
    if (1) {
        {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if (1 && Registers.PC.B.xPCh != newPC.B.h) {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if ((Registers.PC.W.xPC & ~((0x1000) - 1)) != (newPC.W & ~((0x1000) - 1)))
            snes->mem->S9xSetPCBase(ICPU.ShiftedPB + newPC.W);
        else
            Registers.PC.W.xPC = newPC.W;
    }
}
void SCPUState::Op80Slow(void)
{
    pair newPC;
    newPC.W = RelativeSlow(JUMP);
    if (1) {
        {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if ((Registers.P.W & 256) && Registers.PC.B.xPCh != newPC.B.h) {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if ((Registers.PC.W.xPC & ~((0x1000) - 1)) != (newPC.W & ~((0x1000) - 1)))
            snes->mem->S9xSetPCBase(ICPU.ShiftedPB + newPC.W);
        else
            Registers.PC.W.xPC = newPC.W;
    }
}
void SCPUState::Op50E0(void)
{
    pair newPC;
    newPC.W = Relative(JUMP);
    if (!(ICPU._Overflow)) {
        {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if (0 && Registers.PC.B.xPCh != newPC.B.h) {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if ((Registers.PC.W.xPC & ~((0x1000) - 1)) != (newPC.W & ~((0x1000) - 1)))
            snes->mem->S9xSetPCBase(ICPU.ShiftedPB + newPC.W);
        else
            Registers.PC.W.xPC = newPC.W;
    }
}
void SCPUState::Op50E1(void)
{
    pair newPC;
    newPC.W = Relative(JUMP);
    if (!(ICPU._Overflow)) {
        {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if (1 && Registers.PC.B.xPCh != newPC.B.h) {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if ((Registers.PC.W.xPC & ~((0x1000) - 1)) != (newPC.W & ~((0x1000) - 1)))
            snes->mem->S9xSetPCBase(ICPU.ShiftedPB + newPC.W);
        else
            Registers.PC.W.xPC = newPC.W;
    }
}
void SCPUState::Op50Slow(void)
{
    pair newPC;
    newPC.W = RelativeSlow(JUMP);
    if (!(ICPU._Overflow)) {
        {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if ((Registers.P.W & 256) && Registers.PC.B.xPCh != newPC.B.h) {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if ((Registers.PC.W.xPC & ~((0x1000) - 1)) != (newPC.W & ~((0x1000) - 1)))
            snes->mem->S9xSetPCBase(ICPU.ShiftedPB + newPC.W);
        else
            Registers.PC.W.xPC = newPC.W;
    }
}
void SCPUState::Op70E0(void)
{
    pair newPC;
    newPC.W = Relative(JUMP);
    if ((ICPU._Overflow)) {
        {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if (0 && Registers.PC.B.xPCh != newPC.B.h) {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if ((Registers.PC.W.xPC & ~((0x1000) - 1)) != (newPC.W & ~((0x1000) - 1)))
            snes->mem->S9xSetPCBase(ICPU.ShiftedPB + newPC.W);
        else
            Registers.PC.W.xPC = newPC.W;
    }
}
void SCPUState::Op70E1(void)
{
    pair newPC;
    newPC.W = Relative(JUMP);
    if ((ICPU._Overflow)) {
        {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if (1 && Registers.PC.B.xPCh != newPC.B.h) {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if ((Registers.PC.W.xPC & ~((0x1000) - 1)) != (newPC.W & ~((0x1000) - 1)))
            snes->mem->S9xSetPCBase(ICPU.ShiftedPB + newPC.W);
        else
            Registers.PC.W.xPC = newPC.W;
    }
}
void SCPUState::Op70Slow(void)
{
    pair newPC;
    newPC.W = RelativeSlow(JUMP);
    if ((ICPU._Overflow)) {
        {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if ((Registers.P.W & 256) && Registers.PC.B.xPCh != newPC.B.h) {
            Cycles += (6);
            while (Cycles >= NextEvent)
                S9xDoHEventProcessing();
        };
        if ((Registers.PC.W.xPC & ~((0x1000) - 1)) != (newPC.W & ~((0x1000) - 1)))
            snes->mem->S9xSetPCBase(ICPU.ShiftedPB + newPC.W);
        else
            Registers.PC.W.xPC = newPC.W;
    }
}
void SCPUState::Op82(void)
{
    snes->mem->S9xSetPCBase(ICPU.ShiftedPB + RelativeLong(JUMP));
}
void SCPUState::Op82Slow(void)
{
    snes->mem->S9xSetPCBase(ICPU.ShiftedPB + RelativeLongSlow(JUMP));
}
void SCPUState::Op18(void)
{
    ClearCarry();
    AddCycles(ONE_CYCLE);
}
void SCPUState::Op38(void)
{
    SetCarry();
    AddCycles(ONE_CYCLE);
}
void SCPUState::OpD8(void)
{
    ClearDecimal();
    AddCycles(ONE_CYCLE);
}
void SCPUState::OpF8(void)
{
    SetDecimal();
    AddCycles(ONE_CYCLE);
}
void SCPUState::Op58(void)
{
    AddCycles(ONE_CYCLE);
    Timings.IRQFlagChanging |= IRQ_CLEAR_FLAG;
}
void SCPUState::Op78(void)
{
    AddCycles(ONE_CYCLE);
    Timings.IRQFlagChanging |= IRQ_SET_FLAG;
}
void SCPUState::OpB8(void)
{
    ClearOverflow();
    AddCycles(ONE_CYCLE);
}
void SCPUState::OpCAX1(void)
{
    AddCycles(ONE_CYCLE);
    Registers.XL--;
    SetZN(Registers.XL);
}
void SCPUState::OpCAX0(void)
{
    AddCycles(ONE_CYCLE);
    Registers.X.W--;
    SetZN(Registers.X.W);
}
void SCPUState::OpCASlow(void)
{
    AddCycles(ONE_CYCLE);
    if (CheckIndex()) {
        Registers.XL--;
        SetZN(Registers.XL);
    } else {
        Registers.X.W--;
        SetZN(Registers.X.W);
    }
}
void SCPUState::Op88X1(void)
{
    AddCycles(ONE_CYCLE);
    Registers.YL--;
    SetZN(Registers.YL);
}
void SCPUState::Op88X0(void)
{
    AddCycles(ONE_CYCLE);
    Registers.Y.W--;
    SetZN(Registers.Y.W);
}
void SCPUState::Op88Slow(void)
{
    AddCycles(ONE_CYCLE);
    if (CheckIndex()) {
        Registers.YL--;
        SetZN(Registers.YL);
    } else {
        Registers.Y.W--;
        SetZN(Registers.Y.W);
    }
}
void SCPUState::OpE8X1(void)
{
    AddCycles(ONE_CYCLE);
    Registers.XL++;
    SetZN(Registers.XL);
}
void SCPUState::OpE8X0(void)
{
    AddCycles(ONE_CYCLE);
    Registers.X.W++;
    SetZN(Registers.X.W);
}
void SCPUState::OpE8Slow(void)
{
    AddCycles(ONE_CYCLE);
    if (CheckIndex()) {
        Registers.XL++;
        SetZN(Registers.XL);
    } else {
        Registers.X.W++;
        SetZN(Registers.X.W);
    }
}
void SCPUState::OpC8X1(void)
{
    AddCycles(ONE_CYCLE);
    Registers.YL++;
    SetZN(Registers.YL);
}
void SCPUState::OpC8X0(void)
{
    AddCycles(ONE_CYCLE);
    Registers.Y.W++;
    SetZN(Registers.Y.W);
}
void SCPUState::OpC8Slow(void)
{
    AddCycles(ONE_CYCLE);
    if (CheckIndex()) {
        Registers.YL++;
        SetZN(Registers.YL);
    } else {
        Registers.Y.W++;
        SetZN(Registers.Y.W);
    }
}
void SCPUState::OpEA(void)
{
    AddCycles(ONE_CYCLE);
}
void SCPUState::OpF4E0(void)
{
    uint16 val = (uint16)Absolute(NONE);
    PushW(val);
    snes->OpenBus = val & 0xff;
}
void SCPUState::OpF4E1(void)
{
    uint16 val = (uint16)Absolute(NONE);
    PushW(val);
    snes->OpenBus      = val & 0xff;
    Registers.SH = 1;
}
void SCPUState::OpF4Slow(void)
{
    uint16 val = (uint16)AbsoluteSlow(NONE);
    PushW(val);
    snes->OpenBus = val & 0xff;
    if (CheckEmulation())
        Registers.SH = 1;
}
void SCPUState::OpD4E0(void)
{
    uint16 val = (uint16)DirectIndirectE0(NONE);
    PushW(val);
    snes->OpenBus = val & 0xff;
}
void SCPUState::OpD4E1(void)
{
    uint16 val = (uint16)DirectIndirectE1(NONE);
    PushW(val);
    snes->OpenBus      = val & 0xff;
    Registers.SH = 1;
}
void SCPUState::OpD4Slow(void)
{
    uint16 val = (uint16)DirectIndirectSlow(NONE);
    PushW(val);
    snes->OpenBus = val & 0xff;
    if (CheckEmulation())
        Registers.SH = 1;
}
void SCPUState::Op62E0(void)
{
    uint16 val = (uint16)RelativeLong(NONE);
    PushW(val);
    snes->OpenBus = val & 0xff;
}
void SCPUState::Op62E1(void)
{
    uint16 val = (uint16)RelativeLong(NONE);
    PushW(val);
    snes->OpenBus      = val & 0xff;
    Registers.SH = 1;
}
void SCPUState::Op62Slow(void)
{
    uint16 val = (uint16)RelativeLongSlow(NONE);
    PushW(val);
    snes->OpenBus = val & 0xff;
    if (CheckEmulation())
        Registers.SH = 1;
}
void SCPUState::Op48E1(void)
{
    AddCycles(ONE_CYCLE);
    PushBE(Registers.AL);
    snes->OpenBus = Registers.AL;
}
void SCPUState::Op48E0M1(void)
{
    AddCycles(ONE_CYCLE);
    PushB(Registers.AL);
    snes->OpenBus = Registers.AL;
}
void SCPUState::Op48E0M0(void)
{
    AddCycles(ONE_CYCLE);
    PushW(Registers.A.W);
    snes->OpenBus = Registers.AL;
}
void SCPUState::Op48Slow(void)
{
    AddCycles(ONE_CYCLE);
    if (CheckEmulation()) {
        PushBE(Registers.AL);
    } else if (CheckMemory()) {
        PushB(Registers.AL);
    } else {
        PushW(Registers.A.W);
    }
    snes->OpenBus = Registers.AL;
}
void SCPUState::Op8BE1(void)
{
    AddCycles(ONE_CYCLE);
    PushBE(Registers.DB);
    snes->OpenBus = Registers.DB;
}
void SCPUState::Op8BE0(void)
{
    AddCycles(ONE_CYCLE);
    PushB(Registers.DB);
    snes->OpenBus = Registers.DB;
}
void SCPUState::Op8BSlow(void)
{
    AddCycles(ONE_CYCLE);
    if (CheckEmulation()) {
        PushBE(Registers.DB);
    } else {
        PushB(Registers.DB);
    }
    snes->OpenBus = Registers.DB;
}
void SCPUState::Op0BE0(void)
{
    AddCycles(ONE_CYCLE);
    PushW(Registers.D.W);
    snes->OpenBus = Registers.DL;
}
void SCPUState::Op0BE1(void)
{
    AddCycles(ONE_CYCLE);
    PushW(Registers.D.W);
    snes->OpenBus      = Registers.DL;
    Registers.SH = 1;
}
void SCPUState::Op0BSlow(void)
{
    AddCycles(ONE_CYCLE);
    PushW(Registers.D.W);
    snes->OpenBus = Registers.DL;
    if (CheckEmulation())
        Registers.SH = 1;
}
void SCPUState::Op4BE1(void)
{
    AddCycles(ONE_CYCLE);
    PushBE(Registers.PB);
    snes->OpenBus = Registers.PB;
}
void SCPUState::Op4BE0(void)
{
    AddCycles(ONE_CYCLE);
    PushB(Registers.PB);
    snes->OpenBus = Registers.PB;
}
void SCPUState::Op4BSlow(void)
{
    AddCycles(ONE_CYCLE);
    if (CheckEmulation()) {
        PushBE(Registers.PB);
    } else {
        PushB(Registers.PB);
    }
    snes->OpenBus = Registers.PB;
}
void SCPUState::Op08E0(void)
{
    snes->mem->S9xPackStatus();
    AddCycles(ONE_CYCLE);
    PushB(Registers.PL);
    snes->OpenBus = Registers.PL;
}
void SCPUState::Op08E1(void)
{
    snes->mem->S9xPackStatus();
    AddCycles(ONE_CYCLE);
    PushBE(Registers.PL);
    snes->OpenBus = Registers.PL;
}
void SCPUState::Op08Slow(void)
{
    snes->mem->S9xPackStatus();
    AddCycles(ONE_CYCLE);
    if (CheckEmulation()) {
        PushBE(Registers.PL);
    } else {
        PushB(Registers.PL);
    }
    snes->OpenBus = Registers.PL;
}
void SCPUState::OpDAE1(void)
{
    AddCycles(ONE_CYCLE);
    PushBE(Registers.XL);
    snes->OpenBus = Registers.XL;
}
void SCPUState::OpDAE0X1(void)
{
    AddCycles(ONE_CYCLE);
    PushB(Registers.XL);
    snes->OpenBus = Registers.XL;
}
void SCPUState::OpDAE0X0(void)
{
    AddCycles(ONE_CYCLE);
    PushW(Registers.X.W);
    snes->OpenBus = Registers.XL;
}
void SCPUState::OpDASlow(void)
{
    AddCycles(ONE_CYCLE);
    if (CheckEmulation()) {
        PushBE(Registers.XL);
    } else if (CheckIndex()) {
        PushB(Registers.XL);
    } else {
        PushW(Registers.X.W);
    }
    snes->OpenBus = Registers.XL;
}
void SCPUState::Op5AE1(void)
{
    AddCycles(ONE_CYCLE);
    PushBE(Registers.YL);
    snes->OpenBus = Registers.YL;
}
void SCPUState::Op5AE0X1(void)
{
    AddCycles(ONE_CYCLE);
    PushB(Registers.YL);
    snes->OpenBus = Registers.YL;
}
void SCPUState::Op5AE0X0(void)
{
    AddCycles(ONE_CYCLE);
    PushW(Registers.Y.W);
    snes->OpenBus = Registers.YL;
}
void SCPUState::Op5ASlow(void)
{
    AddCycles(ONE_CYCLE);
    if (CheckEmulation()) {
        PushBE(Registers.YL);
    } else if (CheckIndex()) {
        PushB(Registers.YL);
    } else {
        PushW(Registers.Y.W);
    }
    snes->OpenBus = Registers.YL;
}
void SCPUState::Op68E1(void)
{
    AddCycles(TWO_CYCLES);
    PullBE(Registers.AL);
    SetZN(Registers.AL);
    snes->OpenBus = Registers.AL;
}
void SCPUState::Op68E0M1(void)
{
    AddCycles(TWO_CYCLES);
    PullB(Registers.AL);
    SetZN(Registers.AL);
    snes->OpenBus = Registers.AL;
}
void SCPUState::Op68E0M0(void)
{
    AddCycles(TWO_CYCLES);
    PullW(Registers.A.W);
    SetZN(Registers.A.W);
    snes->OpenBus = Registers.AH;
}
void SCPUState::Op68Slow(void)
{
    AddCycles(TWO_CYCLES);
    if (CheckEmulation()) {
        PullBE(Registers.AL);
        SetZN(Registers.AL);
        snes->OpenBus = Registers.AL;
    } else if (CheckMemory()) {
        PullB(Registers.AL);
        SetZN(Registers.AL);
        snes->OpenBus = Registers.AL;
    } else {
        PullW(Registers.A.W);
        SetZN(Registers.A.W);
        snes->OpenBus = Registers.AH;
    }
}
void SCPUState::OpABE1(void)
{
    AddCycles(TWO_CYCLES);
    PullBE(Registers.DB);
    SetZN(Registers.DB);
    ICPU.ShiftedDB = Registers.DB << 16;
    snes->OpenBus        = Registers.DB;
}
void SCPUState::OpABE0(void)
{
    AddCycles(TWO_CYCLES);
    PullB(Registers.DB);
    SetZN(Registers.DB);
    ICPU.ShiftedDB = Registers.DB << 16;
    snes->OpenBus        = Registers.DB;
}
void SCPUState::OpABSlow(void)
{
    AddCycles(TWO_CYCLES);
    if (CheckEmulation()) {
        PullBE(Registers.DB);
    } else {
        PullB(Registers.DB);
    }
    SetZN(Registers.DB);
    ICPU.ShiftedDB = Registers.DB << 16;
    snes->OpenBus        = Registers.DB;
}
void SCPUState::Op2BE0(void)
{
    AddCycles(TWO_CYCLES);
    PullW(Registers.D.W);
    SetZN(Registers.D.W);
    snes->OpenBus = Registers.DH;
}
void SCPUState::Op2BE1(void)
{
    AddCycles(TWO_CYCLES);
    PullW(Registers.D.W);
    SetZN(Registers.D.W);
    snes->OpenBus      = Registers.DH;
    Registers.SH = 1;
}
void SCPUState::Op2BSlow(void)
{
    AddCycles(TWO_CYCLES);
    PullW(Registers.D.W);
    SetZN(Registers.D.W);
    snes->OpenBus = Registers.DH;
    if (CheckEmulation())
        Registers.SH = 1;
}
void SCPUState::Op28E1(void)
{
    AddCycles(TWO_CYCLES);
    PullBE(Registers.PL);
    snes->OpenBus = Registers.PL;
    SetFlags(MemoryFlag | IndexFlag);
    snes->mem->S9xUnpackStatus();
    S9xFixCycles();
    CHECK_FOR_IRQ();
}
void SCPUState::Op28E0(void)
{
    AddCycles(TWO_CYCLES);
    PullB(Registers.PL);
    snes->OpenBus = Registers.PL;
    snes->mem->S9xUnpackStatus();
    if (CheckIndex()) {
        Registers.XH = 0;
        Registers.YH = 0;
    }
    S9xFixCycles();
    CHECK_FOR_IRQ();
}
void SCPUState::Op28Slow(void)
{
    AddCycles(TWO_CYCLES);
    if (CheckEmulation()) {
        PullBE(Registers.PL);
        snes->OpenBus = Registers.PL;
        SetFlags(MemoryFlag | IndexFlag);
    } else {
        PullB(Registers.PL);
        snes->OpenBus = Registers.PL;
    }
    snes->mem->S9xUnpackStatus();
    if (CheckIndex()) {
        Registers.XH = 0;
        Registers.YH = 0;
    }
    S9xFixCycles();
    CHECK_FOR_IRQ();
}
void SCPUState::OpFAE1(void)
{
    AddCycles(TWO_CYCLES);
    PullBE(Registers.XL);
    SetZN(Registers.XL);
    snes->OpenBus = Registers.XL;
}
void SCPUState::OpFAE0X1(void)
{
    AddCycles(TWO_CYCLES);
    PullB(Registers.XL);
    SetZN(Registers.XL);
    snes->OpenBus = Registers.XL;
}
void SCPUState::OpFAE0X0(void)
{
    AddCycles(TWO_CYCLES);
    PullW(Registers.X.W);
    SetZN(Registers.X.W);
    snes->OpenBus = Registers.XH;
}
void SCPUState::OpFASlow(void)
{
    AddCycles(TWO_CYCLES);
    if (CheckEmulation()) {
        PullBE(Registers.XL);
        SetZN(Registers.XL);
        snes->OpenBus = Registers.XL;
    } else if (CheckIndex()) {
        PullB(Registers.XL);
        SetZN(Registers.XL);
        snes->OpenBus = Registers.XL;
    } else {
        PullW(Registers.X.W);
        SetZN(Registers.X.W);
        snes->OpenBus = Registers.XH;
    }
}
void SCPUState::Op7AE1(void)
{
    AddCycles(TWO_CYCLES);
    PullBE(Registers.YL);
    SetZN(Registers.YL);
    snes->OpenBus = Registers.YL;
}
void SCPUState::Op7AE0X1(void)
{
    AddCycles(TWO_CYCLES);
    PullB(Registers.YL);
    SetZN(Registers.YL);
    snes->OpenBus = Registers.YL;
}
void SCPUState::Op7AE0X0(void)
{
    AddCycles(TWO_CYCLES);
    PullW(Registers.Y.W);
    SetZN(Registers.Y.W);
    snes->OpenBus = Registers.YH;
}
void SCPUState::Op7ASlow(void)
{
    AddCycles(TWO_CYCLES);
    if (CheckEmulation()) {
        PullBE(Registers.YL);
        SetZN(Registers.YL);
        snes->OpenBus = Registers.YL;
    } else if (CheckIndex()) {
        PullB(Registers.YL);
        SetZN(Registers.YL);
        snes->OpenBus = Registers.YL;
    } else {
        PullW(Registers.Y.W);
        SetZN(Registers.Y.W);
        snes->OpenBus = Registers.YH;
    }
}
void SCPUState::OpAAX1(void)
{
    AddCycles(ONE_CYCLE);
    Registers.XL = Registers.AL;
    SetZN(Registers.XL);
}
void SCPUState::OpAAX0(void)
{
    AddCycles(ONE_CYCLE);
    Registers.X.W = Registers.A.W;
    SetZN(Registers.X.W);
}
void SCPUState::OpAASlow(void)
{
    AddCycles(ONE_CYCLE);
    if (CheckIndex()) {
        Registers.XL = Registers.AL;
        SetZN(Registers.XL);
    } else {
        Registers.X.W = Registers.A.W;
        SetZN(Registers.X.W);
    }
}
void SCPUState::OpA8X1(void)
{
    AddCycles(ONE_CYCLE);
    Registers.YL = Registers.AL;
    SetZN(Registers.YL);
}
void SCPUState::OpA8X0(void)
{
    AddCycles(ONE_CYCLE);
    Registers.Y.W = Registers.A.W;
    SetZN(Registers.Y.W);
}
void SCPUState::OpA8Slow(void)
{
    AddCycles(ONE_CYCLE);
    if (CheckIndex()) {
        Registers.YL = Registers.AL;
        SetZN(Registers.YL);
    } else {
        Registers.Y.W = Registers.A.W;
        SetZN(Registers.Y.W);
    }
}
void SCPUState::Op5B(void)
{
    AddCycles(ONE_CYCLE);
    Registers.D.W = Registers.A.W;
    SetZN(Registers.D.W);
}
void SCPUState::Op1B(void)
{
    AddCycles(ONE_CYCLE);
    Registers.S.W = Registers.A.W;
    if (CheckEmulation())
        Registers.SH = 1;
}
void SCPUState::Op7B(void)
{
    AddCycles(ONE_CYCLE);
    Registers.A.W = Registers.D.W;
    SetZN(Registers.A.W);
}
void SCPUState::Op3B(void)
{
    AddCycles(ONE_CYCLE);
    Registers.A.W = Registers.S.W;
    SetZN(Registers.A.W);
}
void SCPUState::OpBAX1(void)
{
    AddCycles(ONE_CYCLE);
    Registers.XL = Registers.SL;
    SetZN(Registers.XL);
}
void SCPUState::OpBAX0(void)
{
    AddCycles(ONE_CYCLE);
    Registers.X.W = Registers.S.W;
    SetZN(Registers.X.W);
}
void SCPUState::OpBASlow(void)
{
    AddCycles(ONE_CYCLE);
    if (CheckIndex()) {
        Registers.XL = Registers.SL;
        SetZN(Registers.XL);
    } else {
        Registers.X.W = Registers.S.W;
        SetZN(Registers.X.W);
    }
}
void SCPUState::Op8AM1(void)
{
    AddCycles(ONE_CYCLE);
    Registers.AL = Registers.XL;
    SetZN(Registers.AL);
}
void SCPUState::Op8AM0(void)
{
    AddCycles(ONE_CYCLE);
    Registers.A.W = Registers.X.W;
    SetZN(Registers.A.W);
}
void SCPUState::Op8ASlow(void)
{
    AddCycles(ONE_CYCLE);
    if (CheckMemory()) {
        Registers.AL = Registers.XL;
        SetZN(Registers.AL);
    } else {
        Registers.A.W = Registers.X.W;
        SetZN(Registers.A.W);
    }
}
void SCPUState::Op9A(void)
{
    AddCycles(ONE_CYCLE);
    Registers.S.W = Registers.X.W;
    if (CheckEmulation())
        Registers.SH = 1;
}
void SCPUState::Op9BX1(void)
{
    AddCycles(ONE_CYCLE);
    Registers.YL = Registers.XL;
    SetZN(Registers.YL);
}
void SCPUState::Op9BX0(void)
{
    AddCycles(ONE_CYCLE);
    Registers.Y.W = Registers.X.W;
    SetZN(Registers.Y.W);
}
void SCPUState::Op9BSlow(void)
{
    AddCycles(ONE_CYCLE);
    if (CheckIndex()) {
        Registers.YL = Registers.XL;
        SetZN(Registers.YL);
    } else {
        Registers.Y.W = Registers.X.W;
        SetZN(Registers.Y.W);
    }
}
void SCPUState::Op98M1(void)
{
    AddCycles(ONE_CYCLE);
    Registers.AL = Registers.YL;
    SetZN(Registers.AL);
}
void SCPUState::Op98M0(void)
{
    AddCycles(ONE_CYCLE);
    Registers.A.W = Registers.Y.W;
    SetZN(Registers.A.W);
}
void SCPUState::Op98Slow(void)
{
    AddCycles(ONE_CYCLE);
    if (CheckMemory()) {
        Registers.AL = Registers.YL;
        SetZN(Registers.AL);
    } else {
        Registers.A.W = Registers.Y.W;
        SetZN(Registers.A.W);
    }
}
void SCPUState::OpBBX1(void)
{
    AddCycles(ONE_CYCLE);
    Registers.XL = Registers.YL;
    SetZN(Registers.XL);
}
void SCPUState::OpBBX0(void)
{
    AddCycles(ONE_CYCLE);
    Registers.X.W = Registers.Y.W;
    SetZN(Registers.X.W);
}
void SCPUState::OpBBSlow(void)
{
    AddCycles(ONE_CYCLE);
    if (CheckIndex()) {
        Registers.XL = Registers.YL;
        SetZN(Registers.XL);
    } else {
        Registers.X.W = Registers.Y.W;
        SetZN(Registers.X.W);
    }
}
void SCPUState::OpFB(void)
{
    AddCycles(ONE_CYCLE);
    uint8 A1     = ICPU._Carry;
    uint8 A2     = Registers.PH;
    ICPU._Carry  = A2 & 1;
    Registers.PH = A1;
    if (CheckEmulation()) {
        SetFlags(MemoryFlag | IndexFlag);
        Registers.SH = 1;
    }
    if (CheckIndex()) {
        Registers.XH = 0;
        Registers.YH = 0;
    }
    S9xFixCycles();
}
void SCPUState::Op00(void)
{
    AddCycles(MemSpeed);
    uint16 addr;
    if (!CheckEmulation()) {
        PushB(Registers.PB);
        PushW(Registers.PCw + 1);
        snes->mem->S9xPackStatus();
        PushB(Registers.PL);
        snes->OpenBus = Registers.PL;
        ClearDecimal();
        SetIRQ();
        addr = snes->mem->S9xGetWord(0xFFE6);
    } else {
        PushWE(Registers.PCw + 1);
        snes->mem->S9xPackStatus();
        PushBE(Registers.PL);
        snes->OpenBus = Registers.PL;
        ClearDecimal();
        SetIRQ();
        addr = snes->mem->S9xGetWord(0xFFFE);
    }
    snes->mem->S9xSetPCBase(addr);
    snes->OpenBus = addr >> 8;
}
void SCPUState::Op02(void)
{
    AddCycles(MemSpeed);
    uint16 addr;
    if (!CheckEmulation()) {
        PushB(Registers.PB);
        PushW(Registers.PCw + 1);
        snes->mem->S9xPackStatus();
        PushB(Registers.PL);
        snes->OpenBus = Registers.PL;
        ClearDecimal();
        SetIRQ();
        addr = snes->mem->S9xGetWord(0xFFE4);
    } else {
        PushWE(Registers.PCw + 1);
        snes->mem->S9xPackStatus();
        PushBE(Registers.PL);
        snes->OpenBus = Registers.PL;
        ClearDecimal();
        SetIRQ();
        addr = snes->mem->S9xGetWord(0xFFF4);
    }
    snes->mem->S9xSetPCBase(addr);
    snes->OpenBus = addr >> 8;
}
void SCPUState::OpDC(void)
{
    snes->mem->S9xSetPCBase(AbsoluteIndirectLong(JUMP));
}
void SCPUState::OpDCSlow(void)
{
    snes->mem->S9xSetPCBase(AbsoluteIndirectLongSlow(JUMP));
}
void SCPUState::Op5C(void)
{
    snes->mem->S9xSetPCBase(AbsoluteLong(JUMP));
}
void SCPUState::Op5CSlow(void)
{
    snes->mem->S9xSetPCBase(AbsoluteLongSlow(JUMP));
}
void SCPUState::Op4C(void)
{
    snes->mem->S9xSetPCBase(ICPU.ShiftedPB + ((uint16)Absolute(JUMP)));
}
void SCPUState::Op4CSlow(void)
{
    snes->mem->S9xSetPCBase(ICPU.ShiftedPB + ((uint16)AbsoluteSlow(JUMP)));
}
void SCPUState::Op6C(void)
{
    snes->mem->S9xSetPCBase(ICPU.ShiftedPB + ((uint16)AbsoluteIndirect(JUMP)));
}
void SCPUState::Op6CSlow(void)
{
    snes->mem->S9xSetPCBase(ICPU.ShiftedPB + ((uint16)AbsoluteIndirectSlow(JUMP)));
}
void SCPUState::Op7C(void)
{
    snes->mem->S9xSetPCBase(ICPU.ShiftedPB + ((uint16)AbsoluteIndexedIndirect(JUMP)));
}
void SCPUState::Op7CSlow(void)
{
    snes->mem->S9xSetPCBase(ICPU.ShiftedPB + ((uint16)AbsoluteIndexedIndirectSlow(JUMP)));
}
void SCPUState::Op22E1(void)
{
    uint32 addr = AbsoluteLong(JSR);
    PushB(Registers.PB);
    PushW(Registers.PCw - 1);
    Registers.SH = 1;
    snes->mem->S9xSetPCBase(addr);
}
void SCPUState::Op22E0(void)
{
    uint32 addr = AbsoluteLong(JSR);
    PushB(Registers.PB);
    PushW(Registers.PCw - 1);
    snes->mem->S9xSetPCBase(addr);
}
void SCPUState::Op22Slow(void)
{
    uint32 addr = AbsoluteLongSlow(JSR);
    PushB(Registers.PB);
    PushW(Registers.PCw - 1);
    if (CheckEmulation())
        Registers.SH = 1;
    snes->mem->S9xSetPCBase(addr);
}
void SCPUState::Op6BE1(void)
{
    AddCycles(TWO_CYCLES);
    PullW(Registers.PCw);
    PullB(Registers.PB);
    Registers.SH = 1;
    Registers.PCw++;
    snes->mem->S9xSetPCBase(Registers.PBPC);
}
void SCPUState::Op6BE0(void)
{
    AddCycles(TWO_CYCLES);
    PullW(Registers.PCw);
    PullB(Registers.PB);
    Registers.PCw++;
    snes->mem->S9xSetPCBase(Registers.PBPC);
}
void SCPUState::Op6BSlow(void)
{
    AddCycles(TWO_CYCLES);
    PullW(Registers.PCw);
    PullB(Registers.PB);
    if (CheckEmulation())
        Registers.SH = 1;
    Registers.PCw++;
    snes->mem->S9xSetPCBase(Registers.PBPC);
}
void SCPUState::Op20E1(void)
{
    uint16 addr = Absolute(JSR);
    AddCycles(ONE_CYCLE);
    PushWE(Registers.PCw - 1);
    snes->mem->S9xSetPCBase(ICPU.ShiftedPB + addr);
}
void SCPUState::Op20E0(void)
{
    uint16 addr = Absolute(JSR);
    AddCycles(ONE_CYCLE);
    PushW(Registers.PCw - 1);
    snes->mem->S9xSetPCBase(ICPU.ShiftedPB + addr);
}
void SCPUState::Op20Slow(void)
{
    uint16 addr = AbsoluteSlow(JSR);
    AddCycles(ONE_CYCLE);
    if (CheckEmulation()) {
        PushWE(Registers.PCw - 1);
    } else {
        PushW(Registers.PCw - 1);
    }
    snes->mem->S9xSetPCBase(ICPU.ShiftedPB + addr);
}
void SCPUState::OpFCE1(void)
{
    uint16 addr = AbsoluteIndexedIndirect(JSR);
    PushW(Registers.PCw - 1);
    Registers.SH = 1;
    snes->mem->S9xSetPCBase(ICPU.ShiftedPB + addr);
}
void SCPUState::OpFCE0(void)
{
    uint16 addr = AbsoluteIndexedIndirect(JSR);
    PushW(Registers.PCw - 1);
    snes->mem->S9xSetPCBase(ICPU.ShiftedPB + addr);
}
void SCPUState::OpFCSlow(void)
{
    uint16 addr = AbsoluteIndexedIndirectSlow(JSR);
    PushW(Registers.PCw - 1);
    if (CheckEmulation())
        Registers.SH = 1;
    snes->mem->S9xSetPCBase(ICPU.ShiftedPB + addr);
}
void SCPUState::Op60E1(void)
{
    AddCycles(TWO_CYCLES);
    PullWE(Registers.PCw);
    AddCycles(ONE_CYCLE);
    Registers.PCw++;
    snes->mem->S9xSetPCBase(Registers.PBPC);
}
void SCPUState::Op60E0(void)
{
    AddCycles(TWO_CYCLES);
    PullW(Registers.PCw);
    AddCycles(ONE_CYCLE);
    Registers.PCw++;
    snes->mem->S9xSetPCBase(Registers.PBPC);
}
void SCPUState::Op60Slow(void)
{
    AddCycles(TWO_CYCLES);
    if (CheckEmulation()) {
        PullWE(Registers.PCw);
    } else {
        PullW(Registers.PCw);
    }
    AddCycles(ONE_CYCLE);
    Registers.PCw++;
    snes->mem->S9xSetPCBase(Registers.PBPC);
}
void SCPUState::Op54X1(void)
{
    uint32 SrcBank;
    Registers.DB   = Immediate8(NONE);
    ICPU.ShiftedDB = Registers.DB << 16;
    snes->OpenBus = SrcBank = Immediate8(NONE);
    snes->mem->S9xSetByte(snes->OpenBus = snes->mem->S9xGetByte((SrcBank << 16) + Registers.X.W),
                          ICPU.ShiftedDB + Registers.Y.W);
    Registers.XL++;
    Registers.YL++;
    Registers.A.W--;
    if (Registers.A.W != 0xffff)
        Registers.PCw -= 3;
    AddCycles(TWO_CYCLES);
}
void SCPUState::Op54X0(void)
{
    uint32 SrcBank;
    Registers.DB   = Immediate8(NONE);
    ICPU.ShiftedDB = Registers.DB << 16;
    snes->OpenBus = SrcBank = Immediate8(NONE);
    snes->mem->S9xSetByte(snes->OpenBus = snes->mem->S9xGetByte((SrcBank << 16) + Registers.X.W),
                          ICPU.ShiftedDB + Registers.Y.W);
    Registers.X.W++;
    Registers.Y.W++;
    Registers.A.W--;
    if (Registers.A.W != 0xffff)
        Registers.PCw -= 3;
    AddCycles(TWO_CYCLES);
}
void SCPUState::Op54Slow(void)
{
    uint32 SrcBank;
    snes->OpenBus = Registers.DB = Immediate8Slow(NONE);
    ICPU.ShiftedDB         = Registers.DB << 16;
    snes->OpenBus = SrcBank = Immediate8Slow(NONE);
    snes->mem->S9xSetByte(snes->OpenBus = snes->mem->S9xGetByte((SrcBank << 16) + Registers.X.W),
                          ICPU.ShiftedDB + Registers.Y.W);
    if (CheckIndex()) {
        Registers.XL++;
        Registers.YL++;
    } else {
        Registers.X.W++;
        Registers.Y.W++;
    }
    Registers.A.W--;
    if (Registers.A.W != 0xffff)
        Registers.PCw -= 3;
    AddCycles(TWO_CYCLES);
}
void SCPUState::Op44X1(void)
{
    uint32 SrcBank;
    Registers.DB   = Immediate8(NONE);
    ICPU.ShiftedDB = Registers.DB << 16;
    snes->OpenBus = SrcBank = Immediate8(NONE);
    snes->mem->S9xSetByte(snes->OpenBus = snes->mem->S9xGetByte((SrcBank << 16) + Registers.X.W),
                          ICPU.ShiftedDB + Registers.Y.W);
    Registers.XL--;
    Registers.YL--;
    Registers.A.W--;
    if (Registers.A.W != 0xffff)
        Registers.PCw -= 3;
    AddCycles(TWO_CYCLES);
}
void SCPUState::Op44X0(void)
{
    uint32 SrcBank;
    Registers.DB   = Immediate8(NONE);
    ICPU.ShiftedDB = Registers.DB << 16;
    snes->OpenBus = SrcBank = Immediate8(NONE);
    snes->mem->S9xSetByte(snes->OpenBus = snes->mem->S9xGetByte((SrcBank << 16) + Registers.X.W),
                          ICPU.ShiftedDB + Registers.Y.W);
    Registers.X.W--;
    Registers.Y.W--;
    Registers.A.W--;
    if (Registers.A.W != 0xffff)
        Registers.PCw -= 3;
    AddCycles(TWO_CYCLES);
}
void SCPUState::Op44Slow(void)
{
    uint32 SrcBank;
    snes->OpenBus = Registers.DB = Immediate8Slow(NONE);
    ICPU.ShiftedDB         = Registers.DB << 16;
    snes->OpenBus = SrcBank = Immediate8Slow(NONE);
    snes->mem->S9xSetByte(snes->OpenBus = snes->mem->S9xGetByte((SrcBank << 16) + Registers.X.W),
                          ICPU.ShiftedDB + Registers.Y.W);
    if (CheckIndex()) {
        Registers.XL--;
        Registers.YL--;
    } else {
        Registers.X.W--;
        Registers.Y.W--;
    }
    Registers.A.W--;
    if (Registers.A.W != 0xffff)
        Registers.PCw -= 3;
    AddCycles(TWO_CYCLES);
}
void SCPUState::OpC2(void)
{
    uint8 Work8 = ~Immediate8(READ);
    Registers.PL &= Work8;
    ICPU._Carry &= Work8;
    ICPU._Overflow &= (Work8 >> 6);
    ICPU._Negative &= Work8;
    ICPU._Zero |= ~Work8 & Zero;
    AddCycles(ONE_CYCLE);
    if (CheckEmulation()) {
        SetFlags(MemoryFlag | IndexFlag);
    }
    if (CheckIndex()) {
        Registers.XH = 0;
        Registers.YH = 0;
    }
    S9xFixCycles();
    CHECK_FOR_IRQ();
}
void SCPUState::OpC2Slow(void)
{
    uint8 Work8 = ~Immediate8Slow(READ);
    Registers.PL &= Work8;
    ICPU._Carry &= Work8;
    ICPU._Overflow &= (Work8 >> 6);
    ICPU._Negative &= Work8;
    ICPU._Zero |= ~Work8 & Zero;
    AddCycles(ONE_CYCLE);
    if (CheckEmulation()) {
        SetFlags(MemoryFlag | IndexFlag);
    }
    if (CheckIndex()) {
        Registers.XH = 0;
        Registers.YH = 0;
    }
    S9xFixCycles();
    CHECK_FOR_IRQ();
}
void SCPUState::OpE2(void)
{
    uint8 Work8 = Immediate8(READ);
    Registers.PL |= Work8;
    ICPU._Carry |= Work8 & 1;
    ICPU._Overflow |= (Work8 >> 6) & 1;
    ICPU._Negative |= Work8;
    if (Work8 & Zero)
        ICPU._Zero = 0;
    AddCycles(ONE_CYCLE);
    if (CheckEmulation()) {
        SetFlags(MemoryFlag | IndexFlag);
    }
    if (CheckIndex()) {
        Registers.XH = 0;
        Registers.YH = 0;
    }
    S9xFixCycles();
}
void SCPUState::OpE2Slow(void)
{
    uint8 Work8 = Immediate8Slow(READ);
    Registers.PL |= Work8;
    ICPU._Carry |= Work8 & 1;
    ICPU._Overflow |= (Work8 >> 6) & 1;
    ICPU._Negative |= Work8;
    if (Work8 & Zero)
        ICPU._Zero = 0;
    AddCycles(ONE_CYCLE);
    if (CheckEmulation()) {
        SetFlags(MemoryFlag | IndexFlag);
    }
    if (CheckIndex()) {
        Registers.XH = 0;
        Registers.YH = 0;
    }
    S9xFixCycles();
}
void SCPUState::OpEB(void)
{
    uint8 Work8  = Registers.AL;
    Registers.AL = Registers.AH;
    Registers.AH = Work8;
    SetZN(Registers.AL);
    AddCycles(TWO_CYCLES);
}
void SCPUState::Op40Slow(void)
{
    AddCycles(TWO_CYCLES);
    if (!CheckEmulation()) {
        PullB(Registers.PL);
        snes->mem->S9xUnpackStatus();
        PullW(Registers.PCw);
        PullB(Registers.PB);
        snes->OpenBus        = Registers.PB;
        ICPU.ShiftedPB = Registers.PB << 16;
    } else {
        PullBE(Registers.PL);
        snes->mem->S9xUnpackStatus();
        PullWE(Registers.PCw);
        snes->OpenBus = Registers.PCh;
        SetFlags(MemoryFlag | IndexFlag);
    }
    snes->mem->S9xSetPCBase(Registers.PBPC);
    if (CheckIndex()) {
        Registers.XH = 0;
        Registers.YH = 0;
    }
    S9xFixCycles();
    CHECK_FOR_IRQ();
}
void SCPUState::OpCB(void)
{
    WaitingForInterrupt = TRUE;
    Registers.PCw--;
    AddCycles(ONE_CYCLE);
}
void SCPUState::OpDB(void)
{
    Registers.PCw--;
    Flags |= DEBUG_MODE_FLAG | HALTED_FLAG;
}
void SCPUState::Op42(void)
{
    snes->mem->S9xGetWord(Registers.PBPC);
    Registers.PCw++;
}
void SCPUState::create_op_table()
{
    S9xOpcodesM1X1 = {{&SCPUState::Op00},     {&SCPUState::Op01E0M1},   {&SCPUState::Op02},     {&SCPUState::Op03M1},
                      {&SCPUState::Op04M1},   {&SCPUState::Op05M1},     {&SCPUState::Op06M1},   {&SCPUState::Op07M1},
                      {&SCPUState::Op08E0},   {&SCPUState::Op09M1},     {&SCPUState::Op0AM1},   {&SCPUState::Op0BE0},
                      {&SCPUState::Op0CM1},   {&SCPUState::Op0DM1},     {&SCPUState::Op0EM1},   {&SCPUState::Op0FM1},
                      {&SCPUState::Op10E0},   {&SCPUState::Op11E0M1X1}, {&SCPUState::Op12E0M1}, {&SCPUState::Op13M1},
                      {&SCPUState::Op14M1},   {&SCPUState::Op15E0M1},   {&SCPUState::Op16E0M1}, {&SCPUState::Op17M1},
                      {&SCPUState::Op18},     {&SCPUState::Op19M1X1},   {&SCPUState::Op1AM1},   {&SCPUState::Op1B},
                      {&SCPUState::Op1CM1},   {&SCPUState::Op1DM1X1},   {&SCPUState::Op1EM1X1}, {&SCPUState::Op1FM1},
                      {&SCPUState::Op20E0},   {&SCPUState::Op21E0M1},   {&SCPUState::Op22E0},   {&SCPUState::Op23M1},
                      {&SCPUState::Op24M1},   {&SCPUState::Op25M1},     {&SCPUState::Op26M1},   {&SCPUState::Op27M1},
                      {&SCPUState::Op28E0},   {&SCPUState::Op29M1},     {&SCPUState::Op2AM1},   {&SCPUState::Op2BE0},
                      {&SCPUState::Op2CM1},   {&SCPUState::Op2DM1},     {&SCPUState::Op2EM1},   {&SCPUState::Op2FM1},
                      {&SCPUState::Op30E0},   {&SCPUState::Op31E0M1X1}, {&SCPUState::Op32E0M1}, {&SCPUState::Op33M1},
                      {&SCPUState::Op34E0M1}, {&SCPUState::Op35E0M1},   {&SCPUState::Op36E0M1}, {&SCPUState::Op37M1},
                      {&SCPUState::Op38},     {&SCPUState::Op39M1X1},   {&SCPUState::Op3AM1},   {&SCPUState::Op3B},
                      {&SCPUState::Op3CM1X1}, {&SCPUState::Op3DM1X1},   {&SCPUState::Op3EM1X1}, {&SCPUState::Op3FM1},
                      {&SCPUState::Op40Slow}, {&SCPUState::Op41E0M1},   {&SCPUState::Op42},     {&SCPUState::Op43M1},
                      {&SCPUState::Op44X1},   {&SCPUState::Op45M1},     {&SCPUState::Op46M1},   {&SCPUState::Op47M1},
                      {&SCPUState::Op48E0M1}, {&SCPUState::Op49M1},     {&SCPUState::Op4AM1},   {&SCPUState::Op4BE0},
                      {&SCPUState::Op4C},     {&SCPUState::Op4DM1},     {&SCPUState::Op4EM1},   {&SCPUState::Op4FM1},
                      {&SCPUState::Op50E0},   {&SCPUState::Op51E0M1X1}, {&SCPUState::Op52E0M1}, {&SCPUState::Op53M1},
                      {&SCPUState::Op54X1},   {&SCPUState::Op55E0M1},   {&SCPUState::Op56E0M1}, {&SCPUState::Op57M1},
                      {&SCPUState::Op58},     {&SCPUState::Op59M1X1},   {&SCPUState::Op5AE0X1}, {&SCPUState::Op5B},
                      {&SCPUState::Op5C},     {&SCPUState::Op5DM1X1},   {&SCPUState::Op5EM1X1}, {&SCPUState::Op5FM1},
                      {&SCPUState::Op60E0},   {&SCPUState::Op61E0M1},   {&SCPUState::Op62E0},   {&SCPUState::Op63M1},
                      {&SCPUState::Op64M1},   {&SCPUState::Op65M1},     {&SCPUState::Op66M1},   {&SCPUState::Op67M1},
                      {&SCPUState::Op68E0M1}, {&SCPUState::Op69M1},     {&SCPUState::Op6AM1},   {&SCPUState::Op6BE0},
                      {&SCPUState::Op6C},     {&SCPUState::Op6DM1},     {&SCPUState::Op6EM1},   {&SCPUState::Op6FM1},
                      {&SCPUState::Op70E0},   {&SCPUState::Op71E0M1X1}, {&SCPUState::Op72E0M1}, {&SCPUState::Op73M1},
                      {&SCPUState::Op74E0M1}, {&SCPUState::Op75E0M1},   {&SCPUState::Op76E0M1}, {&SCPUState::Op77M1},
                      {&SCPUState::Op78},     {&SCPUState::Op79M1X1},   {&SCPUState::Op7AE0X1}, {&SCPUState::Op7B},
                      {&SCPUState::Op7C},     {&SCPUState::Op7DM1X1},   {&SCPUState::Op7EM1X1}, {&SCPUState::Op7FM1},
                      {&SCPUState::Op80E0},   {&SCPUState::Op81E0M1},   {&SCPUState::Op82},     {&SCPUState::Op83M1},
                      {&SCPUState::Op84X1},   {&SCPUState::Op85M1},     {&SCPUState::Op86X1},   {&SCPUState::Op87M1},
                      {&SCPUState::Op88X1},   {&SCPUState::Op89M1},     {&SCPUState::Op8AM1},   {&SCPUState::Op8BE0},
                      {&SCPUState::Op8CX1},   {&SCPUState::Op8DM1},     {&SCPUState::Op8EX1},   {&SCPUState::Op8FM1},
                      {&SCPUState::Op90E0},   {&SCPUState::Op91E0M1X1}, {&SCPUState::Op92E0M1}, {&SCPUState::Op93M1},
                      {&SCPUState::Op94E0X1}, {&SCPUState::Op95E0M1},   {&SCPUState::Op96E0X1}, {&SCPUState::Op97M1},
                      {&SCPUState::Op98M1},   {&SCPUState::Op99M1X1},   {&SCPUState::Op9A},     {&SCPUState::Op9BX1},
                      {&SCPUState::Op9CM1},   {&SCPUState::Op9DM1X1},   {&SCPUState::Op9EM1X1}, {&SCPUState::Op9FM1},
                      {&SCPUState::OpA0X1},   {&SCPUState::OpA1E0M1},   {&SCPUState::OpA2X1},   {&SCPUState::OpA3M1},
                      {&SCPUState::OpA4X1},   {&SCPUState::OpA5M1},     {&SCPUState::OpA6X1},   {&SCPUState::OpA7M1},
                      {&SCPUState::OpA8X1},   {&SCPUState::OpA9M1},     {&SCPUState::OpAAX1},   {&SCPUState::OpABE0},
                      {&SCPUState::OpACX1},   {&SCPUState::OpADM1},     {&SCPUState::OpAEX1},   {&SCPUState::OpAFM1},
                      {&SCPUState::OpB0E0},   {&SCPUState::OpB1E0M1X1}, {&SCPUState::OpB2E0M1}, {&SCPUState::OpB3M1},
                      {&SCPUState::OpB4E0X1}, {&SCPUState::OpB5E0M1},   {&SCPUState::OpB6E0X1}, {&SCPUState::OpB7M1},
                      {&SCPUState::OpB8},     {&SCPUState::OpB9M1X1},   {&SCPUState::OpBAX1},   {&SCPUState::OpBBX1},
                      {&SCPUState::OpBCX1},   {&SCPUState::OpBDM1X1},   {&SCPUState::OpBEX1},   {&SCPUState::OpBFM1},
                      {&SCPUState::OpC0X1},   {&SCPUState::OpC1E0M1},   {&SCPUState::OpC2},     {&SCPUState::OpC3M1},
                      {&SCPUState::OpC4X1},   {&SCPUState::OpC5M1},     {&SCPUState::OpC6M1},   {&SCPUState::OpC7M1},
                      {&SCPUState::OpC8X1},   {&SCPUState::OpC9M1},     {&SCPUState::OpCAX1},   {&SCPUState::OpCB},
                      {&SCPUState::OpCCX1},   {&SCPUState::OpCDM1},     {&SCPUState::OpCEM1},   {&SCPUState::OpCFM1},
                      {&SCPUState::OpD0E0},   {&SCPUState::OpD1E0M1X1}, {&SCPUState::OpD2E0M1}, {&SCPUState::OpD3M1},
                      {&SCPUState::OpD4E0},   {&SCPUState::OpD5E0M1},   {&SCPUState::OpD6E0M1}, {&SCPUState::OpD7M1},
                      {&SCPUState::OpD8},     {&SCPUState::OpD9M1X1},   {&SCPUState::OpDAE0X1}, {&SCPUState::OpDB},
                      {&SCPUState::OpDC},     {&SCPUState::OpDDM1X1},   {&SCPUState::OpDEM1X1}, {&SCPUState::OpDFM1},
                      {&SCPUState::OpE0X1},   {&SCPUState::OpE1E0M1},   {&SCPUState::OpE2},     {&SCPUState::OpE3M1},
                      {&SCPUState::OpE4X1},   {&SCPUState::OpE5M1},     {&SCPUState::OpE6M1},   {&SCPUState::OpE7M1},
                      {&SCPUState::OpE8X1},   {&SCPUState::OpE9M1},     {&SCPUState::OpEA},     {&SCPUState::OpEB},
                      {&SCPUState::OpECX1},   {&SCPUState::OpEDM1},     {&SCPUState::OpEEM1},   {&SCPUState::OpEFM1},
                      {&SCPUState::OpF0E0},   {&SCPUState::OpF1E0M1X1}, {&SCPUState::OpF2E0M1}, {&SCPUState::OpF3M1},
                      {&SCPUState::OpF4E0},   {&SCPUState::OpF5E0M1},   {&SCPUState::OpF6E0M1}, {&SCPUState::OpF7M1},
                      {&SCPUState::OpF8},     {&SCPUState::OpF9M1X1},   {&SCPUState::OpFAE0X1}, {&SCPUState::OpFB},
                      {&SCPUState::OpFCE0},   {&SCPUState::OpFDM1X1},   {&SCPUState::OpFEM1X1}, {&SCPUState::OpFFM1}};


    S9xOpcodesE1   = {{&SCPUState::Op00},     {&SCPUState::Op01E1},   {&SCPUState::Op02},     {&SCPUState::Op03M1},
                      {&SCPUState::Op04M1},   {&SCPUState::Op05M1},   {&SCPUState::Op06M1},   {&SCPUState::Op07M1},
                      {&SCPUState::Op08E1},   {&SCPUState::Op09M1},   {&SCPUState::Op0AM1},   {&SCPUState::Op0BE1},
                      {&SCPUState::Op0CM1},   {&SCPUState::Op0DM1},   {&SCPUState::Op0EM1},   {&SCPUState::Op0FM1},
                      {&SCPUState::Op10E1},   {&SCPUState::Op11E1},   {&SCPUState::Op12E1},   {&SCPUState::Op13M1},
                      {&SCPUState::Op14M1},   {&SCPUState::Op15E1},   {&SCPUState::Op16E1},   {&SCPUState::Op17M1},
                      {&SCPUState::Op18},     {&SCPUState::Op19M1X1}, {&SCPUState::Op1AM1},   {&SCPUState::Op1B},
                      {&SCPUState::Op1CM1},   {&SCPUState::Op1DM1X1}, {&SCPUState::Op1EM1X1}, {&SCPUState::Op1FM1},
                      {&SCPUState::Op20E1},   {&SCPUState::Op21E1},   {&SCPUState::Op22E1},   {&SCPUState::Op23M1},
                      {&SCPUState::Op24M1},   {&SCPUState::Op25M1},   {&SCPUState::Op26M1},   {&SCPUState::Op27M1},
                      {&SCPUState::Op28E1},   {&SCPUState::Op29M1},   {&SCPUState::Op2AM1},   {&SCPUState::Op2BE1},
                      {&SCPUState::Op2CM1},   {&SCPUState::Op2DM1},   {&SCPUState::Op2EM1},   {&SCPUState::Op2FM1},
                      {&SCPUState::Op30E1},   {&SCPUState::Op31E1},   {&SCPUState::Op32E1},   {&SCPUState::Op33M1},
                      {&SCPUState::Op34E1},   {&SCPUState::Op35E1},   {&SCPUState::Op36E1},   {&SCPUState::Op37M1},
                      {&SCPUState::Op38},     {&SCPUState::Op39M1X1}, {&SCPUState::Op3AM1},   {&SCPUState::Op3B},
                      {&SCPUState::Op3CM1X1}, {&SCPUState::Op3DM1X1}, {&SCPUState::Op3EM1X1}, {&SCPUState::Op3FM1},
                      {&SCPUState::Op40Slow}, {&SCPUState::Op41E1},   {&SCPUState::Op42},     {&SCPUState::Op43M1},
                      {&SCPUState::Op44X1},   {&SCPUState::Op45M1},   {&SCPUState::Op46M1},   {&SCPUState::Op47M1},
                      {&SCPUState::Op48E1},   {&SCPUState::Op49M1},   {&SCPUState::Op4AM1},   {&SCPUState::Op4BE1},
                      {&SCPUState::Op4C},     {&SCPUState::Op4DM1},   {&SCPUState::Op4EM1},   {&SCPUState::Op4FM1},
                      {&SCPUState::Op50E1},   {&SCPUState::Op51E1},   {&SCPUState::Op52E1},   {&SCPUState::Op53M1},
                      {&SCPUState::Op54X1},   {&SCPUState::Op55E1},   {&SCPUState::Op56E1},   {&SCPUState::Op57M1},
                      {&SCPUState::Op58},     {&SCPUState::Op59M1X1}, {&SCPUState::Op5AE1},   {&SCPUState::Op5B},
                      {&SCPUState::Op5C},     {&SCPUState::Op5DM1X1}, {&SCPUState::Op5EM1X1}, {&SCPUState::Op5FM1},
                      {&SCPUState::Op60E1},   {&SCPUState::Op61E1},   {&SCPUState::Op62E1},   {&SCPUState::Op63M1},
                      {&SCPUState::Op64M1},   {&SCPUState::Op65M1},   {&SCPUState::Op66M1},   {&SCPUState::Op67M1},
                      {&SCPUState::Op68E1},   {&SCPUState::Op69M1},   {&SCPUState::Op6AM1},   {&SCPUState::Op6BE1},
                      {&SCPUState::Op6C},     {&SCPUState::Op6DM1},   {&SCPUState::Op6EM1},   {&SCPUState::Op6FM1},
                      {&SCPUState::Op70E1},   {&SCPUState::Op71E1},   {&SCPUState::Op72E1},   {&SCPUState::Op73M1},
                      {&SCPUState::Op74E1},   {&SCPUState::Op75E1},   {&SCPUState::Op76E1},   {&SCPUState::Op77M1},
                      {&SCPUState::Op78},     {&SCPUState::Op79M1X1}, {&SCPUState::Op7AE1},   {&SCPUState::Op7B},
                      {&SCPUState::Op7C},     {&SCPUState::Op7DM1X1}, {&SCPUState::Op7EM1X1}, {&SCPUState::Op7FM1},
                      {&SCPUState::Op80E1},   {&SCPUState::Op81E1},   {&SCPUState::Op82},     {&SCPUState::Op83M1},
                      {&SCPUState::Op84X1},   {&SCPUState::Op85M1},   {&SCPUState::Op86X1},   {&SCPUState::Op87M1},
                      {&SCPUState::Op88X1},   {&SCPUState::Op89M1},   {&SCPUState::Op8AM1},   {&SCPUState::Op8BE1},
                      {&SCPUState::Op8CX1},   {&SCPUState::Op8DM1},   {&SCPUState::Op8EX1},   {&SCPUState::Op8FM1},
                      {&SCPUState::Op90E1},   {&SCPUState::Op91E1},   {&SCPUState::Op92E1},   {&SCPUState::Op93M1},
                      {&SCPUState::Op94E1},   {&SCPUState::Op95E1},   {&SCPUState::Op96E1},   {&SCPUState::Op97M1},
                      {&SCPUState::Op98M1},   {&SCPUState::Op99M1X1}, {&SCPUState::Op9A},     {&SCPUState::Op9BX1},
                      {&SCPUState::Op9CM1},   {&SCPUState::Op9DM1X1}, {&SCPUState::Op9EM1X1}, {&SCPUState::Op9FM1},
                      {&SCPUState::OpA0X1},   {&SCPUState::OpA1E1},   {&SCPUState::OpA2X1},   {&SCPUState::OpA3M1},
                      {&SCPUState::OpA4X1},   {&SCPUState::OpA5M1},   {&SCPUState::OpA6X1},   {&SCPUState::OpA7M1},
                      {&SCPUState::OpA8X1},   {&SCPUState::OpA9M1},   {&SCPUState::OpAAX1},   {&SCPUState::OpABE1},
                      {&SCPUState::OpACX1},   {&SCPUState::OpADM1},   {&SCPUState::OpAEX1},   {&SCPUState::OpAFM1},
                      {&SCPUState::OpB0E1},   {&SCPUState::OpB1E1},   {&SCPUState::OpB2E1},   {&SCPUState::OpB3M1},
                      {&SCPUState::OpB4E1},   {&SCPUState::OpB5E1},   {&SCPUState::OpB6E1},   {&SCPUState::OpB7M1},
                      {&SCPUState::OpB8},     {&SCPUState::OpB9M1X1}, {&SCPUState::OpBAX1},   {&SCPUState::OpBBX1},
                      {&SCPUState::OpBCX1},   {&SCPUState::OpBDM1X1}, {&SCPUState::OpBEX1},   {&SCPUState::OpBFM1},
                      {&SCPUState::OpC0X1},   {&SCPUState::OpC1E1},   {&SCPUState::OpC2},     {&SCPUState::OpC3M1},
                      {&SCPUState::OpC4X1},   {&SCPUState::OpC5M1},   {&SCPUState::OpC6M1},   {&SCPUState::OpC7M1},
                      {&SCPUState::OpC8X1},   {&SCPUState::OpC9M1},   {&SCPUState::OpCAX1},   {&SCPUState::OpCB},
                      {&SCPUState::OpCCX1},   {&SCPUState::OpCDM1},   {&SCPUState::OpCEM1},   {&SCPUState::OpCFM1},
                      {&SCPUState::OpD0E1},   {&SCPUState::OpD1E1},   {&SCPUState::OpD2E1},   {&SCPUState::OpD3M1},
                      {&SCPUState::OpD4E1},   {&SCPUState::OpD5E1},   {&SCPUState::OpD6E1},   {&SCPUState::OpD7M1},
                      {&SCPUState::OpD8},     {&SCPUState::OpD9M1X1}, {&SCPUState::OpDAE1},   {&SCPUState::OpDB},
                      {&SCPUState::OpDC},     {&SCPUState::OpDDM1X1}, {&SCPUState::OpDEM1X1}, {&SCPUState::OpDFM1},
                      {&SCPUState::OpE0X1},   {&SCPUState::OpE1E1},   {&SCPUState::OpE2},     {&SCPUState::OpE3M1},
                      {&SCPUState::OpE4X1},   {&SCPUState::OpE5M1},   {&SCPUState::OpE6M1},   {&SCPUState::OpE7M1},
                      {&SCPUState::OpE8X1},   {&SCPUState::OpE9M1},   {&SCPUState::OpEA},     {&SCPUState::OpEB},
                      {&SCPUState::OpECX1},   {&SCPUState::OpEDM1},   {&SCPUState::OpEEM1},   {&SCPUState::OpEFM1},
                      {&SCPUState::OpF0E1},   {&SCPUState::OpF1E1},   {&SCPUState::OpF2E1},   {&SCPUState::OpF3M1},
                      {&SCPUState::OpF4E1},   {&SCPUState::OpF5E1},   {&SCPUState::OpF6E1},   {&SCPUState::OpF7M1},
                      {&SCPUState::OpF8},     {&SCPUState::OpF9M1X1}, {&SCPUState::OpFAE1},   {&SCPUState::OpFB},
                      {&SCPUState::OpFCE1},   {&SCPUState::OpFDM1X1}, {&SCPUState::OpFEM1X1}, {&SCPUState::OpFFM1}};
    S9xOpcodesM1X0 = {{&SCPUState::Op00},     {&SCPUState::Op01E0M1},   {&SCPUState::Op02},     {&SCPUState::Op03M1},
                      {&SCPUState::Op04M1},   {&SCPUState::Op05M1},     {&SCPUState::Op06M1},   {&SCPUState::Op07M1},
                      {&SCPUState::Op08E0},   {&SCPUState::Op09M1},     {&SCPUState::Op0AM1},   {&SCPUState::Op0BE0},
                      {&SCPUState::Op0CM1},   {&SCPUState::Op0DM1},     {&SCPUState::Op0EM1},   {&SCPUState::Op0FM1},
                      {&SCPUState::Op10E0},   {&SCPUState::Op11E0M1X0}, {&SCPUState::Op12E0M1}, {&SCPUState::Op13M1},
                      {&SCPUState::Op14M1},   {&SCPUState::Op15E0M1},   {&SCPUState::Op16E0M1}, {&SCPUState::Op17M1},
                      {&SCPUState::Op18},     {&SCPUState::Op19M1X0},   {&SCPUState::Op1AM1},   {&SCPUState::Op1B},
                      {&SCPUState::Op1CM1},   {&SCPUState::Op1DM1X0},   {&SCPUState::Op1EM1X0}, {&SCPUState::Op1FM1},
                      {&SCPUState::Op20E0},   {&SCPUState::Op21E0M1},   {&SCPUState::Op22E0},   {&SCPUState::Op23M1},
                      {&SCPUState::Op24M1},   {&SCPUState::Op25M1},     {&SCPUState::Op26M1},   {&SCPUState::Op27M1},
                      {&SCPUState::Op28E0},   {&SCPUState::Op29M1},     {&SCPUState::Op2AM1},   {&SCPUState::Op2BE0},
                      {&SCPUState::Op2CM1},   {&SCPUState::Op2DM1},     {&SCPUState::Op2EM1},   {&SCPUState::Op2FM1},
                      {&SCPUState::Op30E0},   {&SCPUState::Op31E0M1X0}, {&SCPUState::Op32E0M1}, {&SCPUState::Op33M1},
                      {&SCPUState::Op34E0M1}, {&SCPUState::Op35E0M1},   {&SCPUState::Op36E0M1}, {&SCPUState::Op37M1},
                      {&SCPUState::Op38},     {&SCPUState::Op39M1X0},   {&SCPUState::Op3AM1},   {&SCPUState::Op3B},
                      {&SCPUState::Op3CM1X0}, {&SCPUState::Op3DM1X0},   {&SCPUState::Op3EM1X0}, {&SCPUState::Op3FM1},
                      {&SCPUState::Op40Slow}, {&SCPUState::Op41E0M1},   {&SCPUState::Op42},     {&SCPUState::Op43M1},
                      {&SCPUState::Op44X0},   {&SCPUState::Op45M1},     {&SCPUState::Op46M1},   {&SCPUState::Op47M1},
                      {&SCPUState::Op48E0M1}, {&SCPUState::Op49M1},     {&SCPUState::Op4AM1},   {&SCPUState::Op4BE0},
                      {&SCPUState::Op4C},     {&SCPUState::Op4DM1},     {&SCPUState::Op4EM1},   {&SCPUState::Op4FM1},
                      {&SCPUState::Op50E0},   {&SCPUState::Op51E0M1X0}, {&SCPUState::Op52E0M1}, {&SCPUState::Op53M1},
                      {&SCPUState::Op54X0},   {&SCPUState::Op55E0M1},   {&SCPUState::Op56E0M1}, {&SCPUState::Op57M1},
                      {&SCPUState::Op58},     {&SCPUState::Op59M1X0},   {&SCPUState::Op5AE0X0}, {&SCPUState::Op5B},
                      {&SCPUState::Op5C},     {&SCPUState::Op5DM1X0},   {&SCPUState::Op5EM1X0}, {&SCPUState::Op5FM1},
                      {&SCPUState::Op60E0},   {&SCPUState::Op61E0M1},   {&SCPUState::Op62E0},   {&SCPUState::Op63M1},
                      {&SCPUState::Op64M1},   {&SCPUState::Op65M1},     {&SCPUState::Op66M1},   {&SCPUState::Op67M1},
                      {&SCPUState::Op68E0M1}, {&SCPUState::Op69M1},     {&SCPUState::Op6AM1},   {&SCPUState::Op6BE0},
                      {&SCPUState::Op6C},     {&SCPUState::Op6DM1},     {&SCPUState::Op6EM1},   {&SCPUState::Op6FM1},
                      {&SCPUState::Op70E0},   {&SCPUState::Op71E0M1X0}, {&SCPUState::Op72E0M1}, {&SCPUState::Op73M1},
                      {&SCPUState::Op74E0M1}, {&SCPUState::Op75E0M1},   {&SCPUState::Op76E0M1}, {&SCPUState::Op77M1},
                      {&SCPUState::Op78},     {&SCPUState::Op79M1X0},   {&SCPUState::Op7AE0X0}, {&SCPUState::Op7B},
                      {&SCPUState::Op7C},     {&SCPUState::Op7DM1X0},   {&SCPUState::Op7EM1X0}, {&SCPUState::Op7FM1},
                      {&SCPUState::Op80E0},   {&SCPUState::Op81E0M1},   {&SCPUState::Op82},     {&SCPUState::Op83M1},
                      {&SCPUState::Op84X0},   {&SCPUState::Op85M1},     {&SCPUState::Op86X0},   {&SCPUState::Op87M1},
                      {&SCPUState::Op88X0},   {&SCPUState::Op89M1},     {&SCPUState::Op8AM1},   {&SCPUState::Op8BE0},
                      {&SCPUState::Op8CX0},   {&SCPUState::Op8DM1},     {&SCPUState::Op8EX0},   {&SCPUState::Op8FM1},
                      {&SCPUState::Op90E0},   {&SCPUState::Op91E0M1X0}, {&SCPUState::Op92E0M1}, {&SCPUState::Op93M1},
                      {&SCPUState::Op94E0X0}, {&SCPUState::Op95E0M1},   {&SCPUState::Op96E0X0}, {&SCPUState::Op97M1},
                      {&SCPUState::Op98M1},   {&SCPUState::Op99M1X0},   {&SCPUState::Op9A},     {&SCPUState::Op9BX0},
                      {&SCPUState::Op9CM1},   {&SCPUState::Op9DM1X0},   {&SCPUState::Op9EM1X0}, {&SCPUState::Op9FM1},
                      {&SCPUState::OpA0X0},   {&SCPUState::OpA1E0M1},   {&SCPUState::OpA2X0},   {&SCPUState::OpA3M1},
                      {&SCPUState::OpA4X0},   {&SCPUState::OpA5M1},     {&SCPUState::OpA6X0},   {&SCPUState::OpA7M1},
                      {&SCPUState::OpA8X0},   {&SCPUState::OpA9M1},     {&SCPUState::OpAAX0},   {&SCPUState::OpABE0},
                      {&SCPUState::OpACX0},   {&SCPUState::OpADM1},     {&SCPUState::OpAEX0},   {&SCPUState::OpAFM1},
                      {&SCPUState::OpB0E0},   {&SCPUState::OpB1E0M1X0}, {&SCPUState::OpB2E0M1}, {&SCPUState::OpB3M1},
                      {&SCPUState::OpB4E0X0}, {&SCPUState::OpB5E0M1},   {&SCPUState::OpB6E0X0}, {&SCPUState::OpB7M1},
                      {&SCPUState::OpB8},     {&SCPUState::OpB9M1X0},   {&SCPUState::OpBAX0},   {&SCPUState::OpBBX0},
                      {&SCPUState::OpBCX0},   {&SCPUState::OpBDM1X0},   {&SCPUState::OpBEX0},   {&SCPUState::OpBFM1},
                      {&SCPUState::OpC0X0},   {&SCPUState::OpC1E0M1},   {&SCPUState::OpC2},     {&SCPUState::OpC3M1},
                      {&SCPUState::OpC4X0},   {&SCPUState::OpC5M1},     {&SCPUState::OpC6M1},   {&SCPUState::OpC7M1},
                      {&SCPUState::OpC8X0},   {&SCPUState::OpC9M1},     {&SCPUState::OpCAX0},   {&SCPUState::OpCB},
                      {&SCPUState::OpCCX0},   {&SCPUState::OpCDM1},     {&SCPUState::OpCEM1},   {&SCPUState::OpCFM1},
                      {&SCPUState::OpD0E0},   {&SCPUState::OpD1E0M1X0}, {&SCPUState::OpD2E0M1}, {&SCPUState::OpD3M1},
                      {&SCPUState::OpD4E0},   {&SCPUState::OpD5E0M1},   {&SCPUState::OpD6E0M1}, {&SCPUState::OpD7M1},
                      {&SCPUState::OpD8},     {&SCPUState::OpD9M1X0},   {&SCPUState::OpDAE0X0}, {&SCPUState::OpDB},
                      {&SCPUState::OpDC},     {&SCPUState::OpDDM1X0},   {&SCPUState::OpDEM1X0}, {&SCPUState::OpDFM1},
                      {&SCPUState::OpE0X0},   {&SCPUState::OpE1E0M1},   {&SCPUState::OpE2},     {&SCPUState::OpE3M1},
                      {&SCPUState::OpE4X0},   {&SCPUState::OpE5M1},     {&SCPUState::OpE6M1},   {&SCPUState::OpE7M1},
                      {&SCPUState::OpE8X0},   {&SCPUState::OpE9M1},     {&SCPUState::OpEA},     {&SCPUState::OpEB},
                      {&SCPUState::OpECX0},   {&SCPUState::OpEDM1},     {&SCPUState::OpEEM1},   {&SCPUState::OpEFM1},
                      {&SCPUState::OpF0E0},   {&SCPUState::OpF1E0M1X0}, {&SCPUState::OpF2E0M1}, {&SCPUState::OpF3M1},
                      {&SCPUState::OpF4E0},   {&SCPUState::OpF5E0M1},   {&SCPUState::OpF6E0M1}, {&SCPUState::OpF7M1},
                      {&SCPUState::OpF8},     {&SCPUState::OpF9M1X0},   {&SCPUState::OpFAE0X0}, {&SCPUState::OpFB},
                      {&SCPUState::OpFCE0},   {&SCPUState::OpFDM1X0},   {&SCPUState::OpFEM1X0}, {&SCPUState::OpFFM1}};
    S9xOpcodesM0X0 = {{&SCPUState::Op00},     {&SCPUState::Op01E0M0},   {&SCPUState::Op02},     {&SCPUState::Op03M0},
                      {&SCPUState::Op04M0},   {&SCPUState::Op05M0},     {&SCPUState::Op06M0},   {&SCPUState::Op07M0},
                      {&SCPUState::Op08E0},   {&SCPUState::Op09M0},     {&SCPUState::Op0AM0},   {&SCPUState::Op0BE0},
                      {&SCPUState::Op0CM0},   {&SCPUState::Op0DM0},     {&SCPUState::Op0EM0},   {&SCPUState::Op0FM0},
                      {&SCPUState::Op10E0},   {&SCPUState::Op11E0M0X0}, {&SCPUState::Op12E0M0}, {&SCPUState::Op13M0},
                      {&SCPUState::Op14M0},   {&SCPUState::Op15E0M0},   {&SCPUState::Op16E0M0}, {&SCPUState::Op17M0},
                      {&SCPUState::Op18},     {&SCPUState::Op19M0X0},   {&SCPUState::Op1AM0},   {&SCPUState::Op1B},
                      {&SCPUState::Op1CM0},   {&SCPUState::Op1DM0X0},   {&SCPUState::Op1EM0X0}, {&SCPUState::Op1FM0},
                      {&SCPUState::Op20E0},   {&SCPUState::Op21E0M0},   {&SCPUState::Op22E0},   {&SCPUState::Op23M0},
                      {&SCPUState::Op24M0},   {&SCPUState::Op25M0},     {&SCPUState::Op26M0},   {&SCPUState::Op27M0},
                      {&SCPUState::Op28E0},   {&SCPUState::Op29M0},     {&SCPUState::Op2AM0},   {&SCPUState::Op2BE0},
                      {&SCPUState::Op2CM0},   {&SCPUState::Op2DM0},     {&SCPUState::Op2EM0},   {&SCPUState::Op2FM0},
                      {&SCPUState::Op30E0},   {&SCPUState::Op31E0M0X0}, {&SCPUState::Op32E0M0}, {&SCPUState::Op33M0},
                      {&SCPUState::Op34E0M0}, {&SCPUState::Op35E0M0},   {&SCPUState::Op36E0M0}, {&SCPUState::Op37M0},
                      {&SCPUState::Op38},     {&SCPUState::Op39M0X0},   {&SCPUState::Op3AM0},   {&SCPUState::Op3B},
                      {&SCPUState::Op3CM0X0}, {&SCPUState::Op3DM0X0},   {&SCPUState::Op3EM0X0}, {&SCPUState::Op3FM0},
                      {&SCPUState::Op40Slow}, {&SCPUState::Op41E0M0},   {&SCPUState::Op42},     {&SCPUState::Op43M0},
                      {&SCPUState::Op44X0},   {&SCPUState::Op45M0},     {&SCPUState::Op46M0},   {&SCPUState::Op47M0},
                      {&SCPUState::Op48E0M0}, {&SCPUState::Op49M0},     {&SCPUState::Op4AM0},   {&SCPUState::Op4BE0},
                      {&SCPUState::Op4C},     {&SCPUState::Op4DM0},     {&SCPUState::Op4EM0},   {&SCPUState::Op4FM0},
                      {&SCPUState::Op50E0},   {&SCPUState::Op51E0M0X0}, {&SCPUState::Op52E0M0}, {&SCPUState::Op53M0},
                      {&SCPUState::Op54X0},   {&SCPUState::Op55E0M0},   {&SCPUState::Op56E0M0}, {&SCPUState::Op57M0},
                      {&SCPUState::Op58},     {&SCPUState::Op59M0X0},   {&SCPUState::Op5AE0X0}, {&SCPUState::Op5B},
                      {&SCPUState::Op5C},     {&SCPUState::Op5DM0X0},   {&SCPUState::Op5EM0X0}, {&SCPUState::Op5FM0},
                      {&SCPUState::Op60E0},   {&SCPUState::Op61E0M0},   {&SCPUState::Op62E0},   {&SCPUState::Op63M0},
                      {&SCPUState::Op64M0},   {&SCPUState::Op65M0},     {&SCPUState::Op66M0},   {&SCPUState::Op67M0},
                      {&SCPUState::Op68E0M0}, {&SCPUState::Op69M0},     {&SCPUState::Op6AM0},   {&SCPUState::Op6BE0},
                      {&SCPUState::Op6C},     {&SCPUState::Op6DM0},     {&SCPUState::Op6EM0},   {&SCPUState::Op6FM0},
                      {&SCPUState::Op70E0},   {&SCPUState::Op71E0M0X0}, {&SCPUState::Op72E0M0}, {&SCPUState::Op73M0},
                      {&SCPUState::Op74E0M0}, {&SCPUState::Op75E0M0},   {&SCPUState::Op76E0M0}, {&SCPUState::Op77M0},
                      {&SCPUState::Op78},     {&SCPUState::Op79M0X0},   {&SCPUState::Op7AE0X0}, {&SCPUState::Op7B},
                      {&SCPUState::Op7C},     {&SCPUState::Op7DM0X0},   {&SCPUState::Op7EM0X0}, {&SCPUState::Op7FM0},
                      {&SCPUState::Op80E0},   {&SCPUState::Op81E0M0},   {&SCPUState::Op82},     {&SCPUState::Op83M0},
                      {&SCPUState::Op84X0},   {&SCPUState::Op85M0},     {&SCPUState::Op86X0},   {&SCPUState::Op87M0},
                      {&SCPUState::Op88X0},   {&SCPUState::Op89M0},     {&SCPUState::Op8AM0},   {&SCPUState::Op8BE0},
                      {&SCPUState::Op8CX0},   {&SCPUState::Op8DM0},     {&SCPUState::Op8EX0},   {&SCPUState::Op8FM0},
                      {&SCPUState::Op90E0},   {&SCPUState::Op91E0M0X0}, {&SCPUState::Op92E0M0}, {&SCPUState::Op93M0},
                      {&SCPUState::Op94E0X0}, {&SCPUState::Op95E0M0},   {&SCPUState::Op96E0X0}, {&SCPUState::Op97M0},
                      {&SCPUState::Op98M0},   {&SCPUState::Op99M0X0},   {&SCPUState::Op9A},     {&SCPUState::Op9BX0},
                      {&SCPUState::Op9CM0},   {&SCPUState::Op9DM0X0},   {&SCPUState::Op9EM0X0}, {&SCPUState::Op9FM0},
                      {&SCPUState::OpA0X0},   {&SCPUState::OpA1E0M0},   {&SCPUState::OpA2X0},   {&SCPUState::OpA3M0},
                      {&SCPUState::OpA4X0},   {&SCPUState::OpA5M0},     {&SCPUState::OpA6X0},   {&SCPUState::OpA7M0},
                      {&SCPUState::OpA8X0},   {&SCPUState::OpA9M0},     {&SCPUState::OpAAX0},   {&SCPUState::OpABE0},
                      {&SCPUState::OpACX0},   {&SCPUState::OpADM0},     {&SCPUState::OpAEX0},   {&SCPUState::OpAFM0},
                      {&SCPUState::OpB0E0},   {&SCPUState::OpB1E0M0X0}, {&SCPUState::OpB2E0M0}, {&SCPUState::OpB3M0},
                      {&SCPUState::OpB4E0X0}, {&SCPUState::OpB5E0M0},   {&SCPUState::OpB6E0X0}, {&SCPUState::OpB7M0},
                      {&SCPUState::OpB8},     {&SCPUState::OpB9M0X0},   {&SCPUState::OpBAX0},   {&SCPUState::OpBBX0},
                      {&SCPUState::OpBCX0},   {&SCPUState::OpBDM0X0},   {&SCPUState::OpBEX0},   {&SCPUState::OpBFM0},
                      {&SCPUState::OpC0X0},   {&SCPUState::OpC1E0M0},   {&SCPUState::OpC2},     {&SCPUState::OpC3M0},
                      {&SCPUState::OpC4X0},   {&SCPUState::OpC5M0},     {&SCPUState::OpC6M0},   {&SCPUState::OpC7M0},
                      {&SCPUState::OpC8X0},   {&SCPUState::OpC9M0},     {&SCPUState::OpCAX0},   {&SCPUState::OpCB},
                      {&SCPUState::OpCCX0},   {&SCPUState::OpCDM0},     {&SCPUState::OpCEM0},   {&SCPUState::OpCFM0},
                      {&SCPUState::OpD0E0},   {&SCPUState::OpD1E0M0X0}, {&SCPUState::OpD2E0M0}, {&SCPUState::OpD3M0},
                      {&SCPUState::OpD4E0},   {&SCPUState::OpD5E0M0},   {&SCPUState::OpD6E0M0}, {&SCPUState::OpD7M0},
                      {&SCPUState::OpD8},     {&SCPUState::OpD9M0X0},   {&SCPUState::OpDAE0X0}, {&SCPUState::OpDB},
                      {&SCPUState::OpDC},     {&SCPUState::OpDDM0X0},   {&SCPUState::OpDEM0X0}, {&SCPUState::OpDFM0},
                      {&SCPUState::OpE0X0},   {&SCPUState::OpE1E0M0},   {&SCPUState::OpE2},     {&SCPUState::OpE3M0},
                      {&SCPUState::OpE4X0},   {&SCPUState::OpE5M0},     {&SCPUState::OpE6M0},   {&SCPUState::OpE7M0},
                      {&SCPUState::OpE8X0},   {&SCPUState::OpE9M0},     {&SCPUState::OpEA},     {&SCPUState::OpEB},
                      {&SCPUState::OpECX0},   {&SCPUState::OpEDM0},     {&SCPUState::OpEEM0},   {&SCPUState::OpEFM0},
                      {&SCPUState::OpF0E0},   {&SCPUState::OpF1E0M0X0}, {&SCPUState::OpF2E0M0}, {&SCPUState::OpF3M0},
                      {&SCPUState::OpF4E0},   {&SCPUState::OpF5E0M0},   {&SCPUState::OpF6E0M0}, {&SCPUState::OpF7M0},
                      {&SCPUState::OpF8},     {&SCPUState::OpF9M0X0},   {&SCPUState::OpFAE0X0}, {&SCPUState::OpFB},
                      {&SCPUState::OpFCE0},   {&SCPUState::OpFDM0X0},   {&SCPUState::OpFEM0X0}, {&SCPUState::OpFFM0}};
    S9xOpcodesM0X1 = {{&SCPUState::Op00},     {&SCPUState::Op01E0M0},   {&SCPUState::Op02},     {&SCPUState::Op03M0},
                      {&SCPUState::Op04M0},   {&SCPUState::Op05M0},     {&SCPUState::Op06M0},   {&SCPUState::Op07M0},
                      {&SCPUState::Op08E0},   {&SCPUState::Op09M0},     {&SCPUState::Op0AM0},   {&SCPUState::Op0BE0},
                      {&SCPUState::Op0CM0},   {&SCPUState::Op0DM0},     {&SCPUState::Op0EM0},   {&SCPUState::Op0FM0},
                      {&SCPUState::Op10E0},   {&SCPUState::Op11E0M0X1}, {&SCPUState::Op12E0M0}, {&SCPUState::Op13M0},
                      {&SCPUState::Op14M0},   {&SCPUState::Op15E0M0},   {&SCPUState::Op16E0M0}, {&SCPUState::Op17M0},
                      {&SCPUState::Op18},     {&SCPUState::Op19M0X1},   {&SCPUState::Op1AM0},   {&SCPUState::Op1B},
                      {&SCPUState::Op1CM0},   {&SCPUState::Op1DM0X1},   {&SCPUState::Op1EM0X1}, {&SCPUState::Op1FM0},
                      {&SCPUState::Op20E0},   {&SCPUState::Op21E0M0},   {&SCPUState::Op22E0},   {&SCPUState::Op23M0},
                      {&SCPUState::Op24M0},   {&SCPUState::Op25M0},     {&SCPUState::Op26M0},   {&SCPUState::Op27M0},
                      {&SCPUState::Op28E0},   {&SCPUState::Op29M0},     {&SCPUState::Op2AM0},   {&SCPUState::Op2BE0},
                      {&SCPUState::Op2CM0},   {&SCPUState::Op2DM0},     {&SCPUState::Op2EM0},   {&SCPUState::Op2FM0},
                      {&SCPUState::Op30E0},   {&SCPUState::Op31E0M0X1}, {&SCPUState::Op32E0M0}, {&SCPUState::Op33M0},
                      {&SCPUState::Op34E0M0}, {&SCPUState::Op35E0M0},   {&SCPUState::Op36E0M0}, {&SCPUState::Op37M0},
                      {&SCPUState::Op38},     {&SCPUState::Op39M0X1},   {&SCPUState::Op3AM0},   {&SCPUState::Op3B},
                      {&SCPUState::Op3CM0X1}, {&SCPUState::Op3DM0X1},   {&SCPUState::Op3EM0X1}, {&SCPUState::Op3FM0},
                      {&SCPUState::Op40Slow}, {&SCPUState::Op41E0M0},   {&SCPUState::Op42},     {&SCPUState::Op43M0},
                      {&SCPUState::Op44X1},   {&SCPUState::Op45M0},     {&SCPUState::Op46M0},   {&SCPUState::Op47M0},
                      {&SCPUState::Op48E0M0}, {&SCPUState::Op49M0},     {&SCPUState::Op4AM0},   {&SCPUState::Op4BE0},
                      {&SCPUState::Op4C},     {&SCPUState::Op4DM0},     {&SCPUState::Op4EM0},   {&SCPUState::Op4FM0},
                      {&SCPUState::Op50E0},   {&SCPUState::Op51E0M0X1}, {&SCPUState::Op52E0M0}, {&SCPUState::Op53M0},
                      {&SCPUState::Op54X1},   {&SCPUState::Op55E0M0},   {&SCPUState::Op56E0M0}, {&SCPUState::Op57M0},
                      {&SCPUState::Op58},     {&SCPUState::Op59M0X1},   {&SCPUState::Op5AE0X1}, {&SCPUState::Op5B},
                      {&SCPUState::Op5C},     {&SCPUState::Op5DM0X1},   {&SCPUState::Op5EM0X1}, {&SCPUState::Op5FM0},
                      {&SCPUState::Op60E0},   {&SCPUState::Op61E0M0},   {&SCPUState::Op62E0},   {&SCPUState::Op63M0},
                      {&SCPUState::Op64M0},   {&SCPUState::Op65M0},     {&SCPUState::Op66M0},   {&SCPUState::Op67M0},
                      {&SCPUState::Op68E0M0}, {&SCPUState::Op69M0},     {&SCPUState::Op6AM0},   {&SCPUState::Op6BE0},
                      {&SCPUState::Op6C},     {&SCPUState::Op6DM0},     {&SCPUState::Op6EM0},   {&SCPUState::Op6FM0},
                      {&SCPUState::Op70E0},   {&SCPUState::Op71E0M0X1}, {&SCPUState::Op72E0M0}, {&SCPUState::Op73M0},
                      {&SCPUState::Op74E0M0}, {&SCPUState::Op75E0M0},   {&SCPUState::Op76E0M0}, {&SCPUState::Op77M0},
                      {&SCPUState::Op78},     {&SCPUState::Op79M0X1},   {&SCPUState::Op7AE0X1}, {&SCPUState::Op7B},
                      {&SCPUState::Op7C},     {&SCPUState::Op7DM0X1},   {&SCPUState::Op7EM0X1}, {&SCPUState::Op7FM0},
                      {&SCPUState::Op80E0},   {&SCPUState::Op81E0M0},   {&SCPUState::Op82},     {&SCPUState::Op83M0},
                      {&SCPUState::Op84X1},   {&SCPUState::Op85M0},     {&SCPUState::Op86X1},   {&SCPUState::Op87M0},
                      {&SCPUState::Op88X1},   {&SCPUState::Op89M0},     {&SCPUState::Op8AM0},   {&SCPUState::Op8BE0},
                      {&SCPUState::Op8CX1},   {&SCPUState::Op8DM0},     {&SCPUState::Op8EX1},   {&SCPUState::Op8FM0},
                      {&SCPUState::Op90E0},   {&SCPUState::Op91E0M0X1}, {&SCPUState::Op92E0M0}, {&SCPUState::Op93M0},
                      {&SCPUState::Op94E0X1}, {&SCPUState::Op95E0M0},   {&SCPUState::Op96E0X1}, {&SCPUState::Op97M0},
                      {&SCPUState::Op98M0},   {&SCPUState::Op99M0X1},   {&SCPUState::Op9A},     {&SCPUState::Op9BX1},
                      {&SCPUState::Op9CM0},   {&SCPUState::Op9DM0X1},   {&SCPUState::Op9EM0X1}, {&SCPUState::Op9FM0},
                      {&SCPUState::OpA0X1},   {&SCPUState::OpA1E0M0},   {&SCPUState::OpA2X1},   {&SCPUState::OpA3M0},
                      {&SCPUState::OpA4X1},   {&SCPUState::OpA5M0},     {&SCPUState::OpA6X1},   {&SCPUState::OpA7M0},
                      {&SCPUState::OpA8X1},   {&SCPUState::OpA9M0},     {&SCPUState::OpAAX1},   {&SCPUState::OpABE0},
                      {&SCPUState::OpACX1},   {&SCPUState::OpADM0},     {&SCPUState::OpAEX1},   {&SCPUState::OpAFM0},
                      {&SCPUState::OpB0E0},   {&SCPUState::OpB1E0M0X1}, {&SCPUState::OpB2E0M0}, {&SCPUState::OpB3M0},
                      {&SCPUState::OpB4E0X1}, {&SCPUState::OpB5E0M0},   {&SCPUState::OpB6E0X1}, {&SCPUState::OpB7M0},
                      {&SCPUState::OpB8},     {&SCPUState::OpB9M0X1},   {&SCPUState::OpBAX1},   {&SCPUState::OpBBX1},
                      {&SCPUState::OpBCX1},   {&SCPUState::OpBDM0X1},   {&SCPUState::OpBEX1},   {&SCPUState::OpBFM0},
                      {&SCPUState::OpC0X1},   {&SCPUState::OpC1E0M0},   {&SCPUState::OpC2},     {&SCPUState::OpC3M0},
                      {&SCPUState::OpC4X1},   {&SCPUState::OpC5M0},     {&SCPUState::OpC6M0},   {&SCPUState::OpC7M0},
                      {&SCPUState::OpC8X1},   {&SCPUState::OpC9M0},     {&SCPUState::OpCAX1},   {&SCPUState::OpCB},
                      {&SCPUState::OpCCX1},   {&SCPUState::OpCDM0},     {&SCPUState::OpCEM0},   {&SCPUState::OpCFM0},
                      {&SCPUState::OpD0E0},   {&SCPUState::OpD1E0M0X1}, {&SCPUState::OpD2E0M0}, {&SCPUState::OpD3M0},
                      {&SCPUState::OpD4E0},   {&SCPUState::OpD5E0M0},   {&SCPUState::OpD6E0M0}, {&SCPUState::OpD7M0},
                      {&SCPUState::OpD8},     {&SCPUState::OpD9M0X1},   {&SCPUState::OpDAE0X1}, {&SCPUState::OpDB},
                      {&SCPUState::OpDC},     {&SCPUState::OpDDM0X1},   {&SCPUState::OpDEM0X1}, {&SCPUState::OpDFM0},
                      {&SCPUState::OpE0X1},   {&SCPUState::OpE1E0M0},   {&SCPUState::OpE2},     {&SCPUState::OpE3M0},
                      {&SCPUState::OpE4X1},   {&SCPUState::OpE5M0},     {&SCPUState::OpE6M0},   {&SCPUState::OpE7M0},
                      {&SCPUState::OpE8X1},   {&SCPUState::OpE9M0},     {&SCPUState::OpEA},     {&SCPUState::OpEB},
                      {&SCPUState::OpECX1},   {&SCPUState::OpEDM0},     {&SCPUState::OpEEM0},   {&SCPUState::OpEFM0},
                      {&SCPUState::OpF0E0},   {&SCPUState::OpF1E0M0X1}, {&SCPUState::OpF2E0M0}, {&SCPUState::OpF3M0},
                      {&SCPUState::OpF4E0},   {&SCPUState::OpF5E0M0},   {&SCPUState::OpF6E0M0}, {&SCPUState::OpF7M0},
                      {&SCPUState::OpF8},     {&SCPUState::OpF9M0X1},   {&SCPUState::OpFAE0X1}, {&SCPUState::OpFB},
                      {&SCPUState::OpFCE0},   {&SCPUState::OpFDM0X1},   {&SCPUState::OpFEM0X1}, {&SCPUState::OpFFM0}};
    S9xOpcodesSlow = {{&SCPUState::Op00},     {&SCPUState::Op01Slow}, {&SCPUState::Op02},     {&SCPUState::Op03Slow},
                      {&SCPUState::Op04Slow}, {&SCPUState::Op05Slow}, {&SCPUState::Op06Slow}, {&SCPUState::Op07Slow},
                      {&SCPUState::Op08Slow}, {&SCPUState::Op09Slow}, {&SCPUState::Op0ASlow}, {&SCPUState::Op0BSlow},
                      {&SCPUState::Op0CSlow}, {&SCPUState::Op0DSlow}, {&SCPUState::Op0ESlow}, {&SCPUState::Op0FSlow},
                      {&SCPUState::Op10Slow}, {&SCPUState::Op11Slow}, {&SCPUState::Op12Slow}, {&SCPUState::Op13Slow},
                      {&SCPUState::Op14Slow}, {&SCPUState::Op15Slow}, {&SCPUState::Op16Slow}, {&SCPUState::Op17Slow},
                      {&SCPUState::Op18},     {&SCPUState::Op19Slow}, {&SCPUState::Op1ASlow}, {&SCPUState::Op1B},
                      {&SCPUState::Op1CSlow}, {&SCPUState::Op1DSlow}, {&SCPUState::Op1ESlow}, {&SCPUState::Op1FSlow},
                      {&SCPUState::Op20Slow}, {&SCPUState::Op21Slow}, {&SCPUState::Op22Slow}, {&SCPUState::Op23Slow},
                      {&SCPUState::Op24Slow}, {&SCPUState::Op25Slow}, {&SCPUState::Op26Slow}, {&SCPUState::Op27Slow},
                      {&SCPUState::Op28Slow}, {&SCPUState::Op29Slow}, {&SCPUState::Op2ASlow}, {&SCPUState::Op2BSlow},
                      {&SCPUState::Op2CSlow}, {&SCPUState::Op2DSlow}, {&SCPUState::Op2ESlow}, {&SCPUState::Op2FSlow},
                      {&SCPUState::Op30Slow}, {&SCPUState::Op31Slow}, {&SCPUState::Op32Slow}, {&SCPUState::Op33Slow},
                      {&SCPUState::Op34Slow}, {&SCPUState::Op35Slow}, {&SCPUState::Op36Slow}, {&SCPUState::Op37Slow},
                      {&SCPUState::Op38},     {&SCPUState::Op39Slow}, {&SCPUState::Op3ASlow}, {&SCPUState::Op3B},
                      {&SCPUState::Op3CSlow}, {&SCPUState::Op3DSlow}, {&SCPUState::Op3ESlow}, {&SCPUState::Op3FSlow},
                      {&SCPUState::Op40Slow}, {&SCPUState::Op41Slow}, {&SCPUState::Op42},     {&SCPUState::Op43Slow},
                      {&SCPUState::Op44Slow}, {&SCPUState::Op45Slow}, {&SCPUState::Op46Slow}, {&SCPUState::Op47Slow},
                      {&SCPUState::Op48Slow}, {&SCPUState::Op49Slow}, {&SCPUState::Op4ASlow}, {&SCPUState::Op4BSlow},
                      {&SCPUState::Op4CSlow}, {&SCPUState::Op4DSlow}, {&SCPUState::Op4ESlow}, {&SCPUState::Op4FSlow},
                      {&SCPUState::Op50Slow}, {&SCPUState::Op51Slow}, {&SCPUState::Op52Slow}, {&SCPUState::Op53Slow},
                      {&SCPUState::Op54Slow}, {&SCPUState::Op55Slow}, {&SCPUState::Op56Slow}, {&SCPUState::Op57Slow},
                      {&SCPUState::Op58},     {&SCPUState::Op59Slow}, {&SCPUState::Op5ASlow}, {&SCPUState::Op5B},
                      {&SCPUState::Op5CSlow}, {&SCPUState::Op5DSlow}, {&SCPUState::Op5ESlow}, {&SCPUState::Op5FSlow},
                      {&SCPUState::Op60Slow}, {&SCPUState::Op61Slow}, {&SCPUState::Op62Slow}, {&SCPUState::Op63Slow},
                      {&SCPUState::Op64Slow}, {&SCPUState::Op65Slow}, {&SCPUState::Op66Slow}, {&SCPUState::Op67Slow},
                      {&SCPUState::Op68Slow}, {&SCPUState::Op69Slow}, {&SCPUState::Op6ASlow}, {&SCPUState::Op6BSlow},
                      {&SCPUState::Op6CSlow}, {&SCPUState::Op6DSlow}, {&SCPUState::Op6ESlow}, {&SCPUState::Op6FSlow},
                      {&SCPUState::Op70Slow}, {&SCPUState::Op71Slow}, {&SCPUState::Op72Slow}, {&SCPUState::Op73Slow},
                      {&SCPUState::Op74Slow}, {&SCPUState::Op75Slow}, {&SCPUState::Op76Slow}, {&SCPUState::Op77Slow},
                      {&SCPUState::Op78},     {&SCPUState::Op79Slow}, {&SCPUState::Op7ASlow}, {&SCPUState::Op7B},
                      {&SCPUState::Op7CSlow}, {&SCPUState::Op7DSlow}, {&SCPUState::Op7ESlow}, {&SCPUState::Op7FSlow},
                      {&SCPUState::Op80Slow}, {&SCPUState::Op81Slow}, {&SCPUState::Op82Slow}, {&SCPUState::Op83Slow},
                      {&SCPUState::Op84Slow}, {&SCPUState::Op85Slow}, {&SCPUState::Op86Slow}, {&SCPUState::Op87Slow},
                      {&SCPUState::Op88Slow}, {&SCPUState::Op89Slow}, {&SCPUState::Op8ASlow}, {&SCPUState::Op8BSlow},
                      {&SCPUState::Op8CSlow}, {&SCPUState::Op8DSlow}, {&SCPUState::Op8ESlow}, {&SCPUState::Op8FSlow},
                      {&SCPUState::Op90Slow}, {&SCPUState::Op91Slow}, {&SCPUState::Op92Slow}, {&SCPUState::Op93Slow},
                      {&SCPUState::Op94Slow}, {&SCPUState::Op95Slow}, {&SCPUState::Op96Slow}, {&SCPUState::Op97Slow},
                      {&SCPUState::Op98Slow}, {&SCPUState::Op99Slow}, {&SCPUState::Op9A},     {&SCPUState::Op9BSlow},
                      {&SCPUState::Op9CSlow}, {&SCPUState::Op9DSlow}, {&SCPUState::Op9ESlow}, {&SCPUState::Op9FSlow},
                      {&SCPUState::OpA0Slow}, {&SCPUState::OpA1Slow}, {&SCPUState::OpA2Slow}, {&SCPUState::OpA3Slow},
                      {&SCPUState::OpA4Slow}, {&SCPUState::OpA5Slow}, {&SCPUState::OpA6Slow}, {&SCPUState::OpA7Slow},
                      {&SCPUState::OpA8Slow}, {&SCPUState::OpA9Slow}, {&SCPUState::OpAASlow}, {&SCPUState::OpABSlow},
                      {&SCPUState::OpACSlow}, {&SCPUState::OpADSlow}, {&SCPUState::OpAESlow}, {&SCPUState::OpAFSlow},
                      {&SCPUState::OpB0Slow}, {&SCPUState::OpB1Slow}, {&SCPUState::OpB2Slow}, {&SCPUState::OpB3Slow},
                      {&SCPUState::OpB4Slow}, {&SCPUState::OpB5Slow}, {&SCPUState::OpB6Slow}, {&SCPUState::OpB7Slow},
                      {&SCPUState::OpB8},     {&SCPUState::OpB9Slow}, {&SCPUState::OpBASlow}, {&SCPUState::OpBBSlow},
                      {&SCPUState::OpBCSlow}, {&SCPUState::OpBDSlow}, {&SCPUState::OpBESlow}, {&SCPUState::OpBFSlow},
                      {&SCPUState::OpC0Slow}, {&SCPUState::OpC1Slow}, {&SCPUState::OpC2Slow}, {&SCPUState::OpC3Slow},
                      {&SCPUState::OpC4Slow}, {&SCPUState::OpC5Slow}, {&SCPUState::OpC6Slow}, {&SCPUState::OpC7Slow},
                      {&SCPUState::OpC8Slow}, {&SCPUState::OpC9Slow}, {&SCPUState::OpCASlow}, {&SCPUState::OpCB},
                      {&SCPUState::OpCCSlow}, {&SCPUState::OpCDSlow}, {&SCPUState::OpCESlow}, {&SCPUState::OpCFSlow},
                      {&SCPUState::OpD0Slow}, {&SCPUState::OpD1Slow}, {&SCPUState::OpD2Slow}, {&SCPUState::OpD3Slow},
                      {&SCPUState::OpD4Slow}, {&SCPUState::OpD5Slow}, {&SCPUState::OpD6Slow}, {&SCPUState::OpD7Slow},
                      {&SCPUState::OpD8},     {&SCPUState::OpD9Slow}, {&SCPUState::OpDASlow}, {&SCPUState::OpDB},
                      {&SCPUState::OpDCSlow}, {&SCPUState::OpDDSlow}, {&SCPUState::OpDESlow}, {&SCPUState::OpDFSlow},
                      {&SCPUState::OpE0Slow}, {&SCPUState::OpE1Slow}, {&SCPUState::OpE2Slow}, {&SCPUState::OpE3Slow},
                      {&SCPUState::OpE4Slow}, {&SCPUState::OpE5Slow}, {&SCPUState::OpE6Slow}, {&SCPUState::OpE7Slow},
                      {&SCPUState::OpE8Slow}, {&SCPUState::OpE9Slow}, {&SCPUState::OpEA},     {&SCPUState::OpEB},
                      {&SCPUState::OpECSlow}, {&SCPUState::OpEDSlow}, {&SCPUState::OpEESlow}, {&SCPUState::OpEFSlow},
                      {&SCPUState::OpF0Slow}, {&SCPUState::OpF1Slow}, {&SCPUState::OpF2Slow}, {&SCPUState::OpF3Slow},
                      {&SCPUState::OpF4Slow}, {&SCPUState::OpF5Slow}, {&SCPUState::OpF6Slow}, {&SCPUState::OpF7Slow},
                      {&SCPUState::OpF8},     {&SCPUState::OpF9Slow}, {&SCPUState::OpFASlow}, {&SCPUState::OpFB},
                      {&SCPUState::OpFCSlow}, {&SCPUState::OpFDSlow}, {&SCPUState::OpFESlow}, {&SCPUState::OpFFSlow}};
}
