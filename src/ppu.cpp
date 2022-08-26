#include "snes.h"
#include "mem.h"
#include "dma.h"
#include "apu/apu.h"
#include "ppu.h"


SPPU::SPPU(SNESX *_snes)
{
    snes = _snes;
}
void SPPU::S9xLatchCounters(bool force)
{
    if (force || (snes->mem->FillRAM[0x4213] & 0x80)) {
        HVBeamCounterLatched = 1;
        VBeamPosLatched      = (uint16)snes->scpu->V_Counter;
        int32 hc             = snes->scpu->Cycles;
        if (Timings.H_Max == Timings.H_Max_Master) {
            if (hc >= 1292)
                hc -= (ONE_DOT_CYCLE / 2);
            if (hc >= 1308)
                hc -= (ONE_DOT_CYCLE / 2);
        }
        HBeamPosLatched = (uint16)(hc / ONE_DOT_CYCLE);
        snes->mem->FillRAM[0x213f] |= 0x40;
    }
    if (snes->scpu->V_Counter > GunVLatch ||
        (snes->scpu->V_Counter == GunVLatch && snes->scpu->Cycles >= GunHLatch * ONE_DOT_CYCLE))
        GunVLatch = 1000;
}
void SPPU::S9xTryGunLatch(bool force)
{
    if (snes->scpu->V_Counter > GunVLatch ||
        (snes->scpu->V_Counter == GunVLatch && snes->scpu->Cycles >= GunHLatch * ONE_DOT_CYCLE)) {
        if (force || (snes->mem->FillRAM[0x4213] & 0x80)) {
            HVBeamCounterLatched = 1;
            VBeamPosLatched      = (uint16)GunVLatch;
            HBeamPosLatched      = (uint16)GunHLatch;
            snes->mem->FillRAM[0x213f] |= 0x40;
        }
        GunVLatch = 1000;
    }
}
int SPPU::CyclesUntilNext(int hc, int vc)
{
    int32 total = 0;
    int   vpos  = snes->scpu->V_Counter;
    if (vc - vpos > 0) {
        total += (vc - vpos) * Timings.H_Max_Master;
        if (vpos <= 240 && vc > 240 && Timings.InterlaceField & !IPPU.Interlace)
            total -= ONE_DOT_CYCLE;
    } else {
        if (vc == vpos && (hc > snes->scpu->Cycles)) {
            return hc;
        }
        total += (Timings.V_Max - vpos) * Timings.H_Max_Master;
        if (vpos <= 240 && Timings.InterlaceField && !IPPU.Interlace)
            total -= ONE_DOT_CYCLE;
        total += (vc)*Timings.H_Max_Master;
        if (vc > 240 && !Timings.InterlaceField && !IPPU.Interlace)
            total -= ONE_DOT_CYCLE;
    }
    total += hc;
    return total;
}
void SPPU::S9xUpdateIRQPositions(bool initial)
{
    HTimerPosition = IRQHBeamPos * ONE_DOT_CYCLE + Timings.IRQTriggerCycles;
    HTimerPosition -= IRQHBeamPos ? 0 : ONE_DOT_CYCLE;
    HTimerPosition += IRQHBeamPos > 322 ? (ONE_DOT_CYCLE / 2) : 0;
    HTimerPosition += IRQHBeamPos > 326 ? (ONE_DOT_CYCLE / 2) : 0;
    VTimerPosition = IRQVBeamPos;
    if (VTimerEnabled && (VTimerPosition >= (Timings.V_Max + (IPPU.Interlace ? 1 : 0)))) {
        Timings.NextIRQTimer = 0x0fffffff;
    } else if (!HTimerEnabled && !VTimerEnabled) {
        Timings.NextIRQTimer = 0x0fffffff;
    } else if (HTimerEnabled && !VTimerEnabled) {
        int v_pos            = snes->scpu->V_Counter;
        Timings.NextIRQTimer = HTimerPosition;
        if (snes->scpu->Cycles > Timings.NextIRQTimer - Timings.IRQTriggerCycles) {
            Timings.NextIRQTimer += Timings.H_Max;
            v_pos++;
        }
        if (v_pos == 240 && Timings.InterlaceField && !IPPU.Interlace) {
            Timings.NextIRQTimer -= IRQHBeamPos <= 322 ? ONE_DOT_CYCLE / 2 : 0;
            Timings.NextIRQTimer -= IRQHBeamPos <= 326 ? ONE_DOT_CYCLE / 2 : 0;
        }
    } else if (!HTimerEnabled && VTimerEnabled) {
        if (snes->scpu->V_Counter == VTimerPosition && initial)
            Timings.NextIRQTimer = snes->scpu->Cycles + Timings.IRQTriggerCycles - ONE_DOT_CYCLE;
        else
            Timings.NextIRQTimer = CyclesUntilNext(Timings.IRQTriggerCycles - ONE_DOT_CYCLE, VTimerPosition);
    } else {
        Timings.NextIRQTimer = CyclesUntilNext(HTimerPosition, VTimerPosition);
        int field            = Timings.InterlaceField;
        if (VTimerPosition < snes->scpu->V_Counter ||
            (VTimerPosition == snes->scpu->V_Counter && Timings.NextIRQTimer > Timings.H_Max)) {
            field = !field;
        }
        if (VTimerPosition == 240 && field && !IPPU.Interlace) {
            Timings.NextIRQTimer -= IRQHBeamPos <= 322 ? ONE_DOT_CYCLE / 2 : 0;
            Timings.NextIRQTimer -= IRQHBeamPos <= 326 ? ONE_DOT_CYCLE / 2 : 0;
        }
    }
}
void SPPU::S9xFixColourBrightness(void)
{
    IPPU.XB = mul_brightness[Brightness];
    for (int i = 0; i < 64; i++) {
        if (i > IPPU.XB[0x1f])
            snes->renderer->brightness_cap[i] = IPPU.XB[0x1f];
        else
            snes->renderer->brightness_cap[i] = i;
    }
    for (int i = 0; i < 256; i++) {
        IPPU.Red[i]          = IPPU.XB[(CGDATA[i]) & 0x1f];
        IPPU.Green[i]        = IPPU.XB[(CGDATA[i] >> 5) & 0x1f];
        IPPU.Blue[i]         = IPPU.XB[(CGDATA[i] >> 10) & 0x1f];
        IPPU.ScreenColors[i] = BUILD_PIXEL(IPPU.Red[i], IPPU.Green[i], IPPU.Blue[i]);
    }
}
void SPPU::S9xSetPPU(uint8 Byte, uint16 Address)
{
    if (snes->scpu->InDMAorHDMA) {
        if (snes->scpu->CurrentDMAorHDMAChannel >= 0 &&
            snes->dma->DMA[snes->scpu->CurrentDMAorHDMAChannel].ReverseTransfer) {
            if ((Address & 0xff00) == 0x2100) {
                return;
            } else {
                return;
            }
        } else {
            if (Address > 0x21ff)
                Address = 0x2100 + (Address & 0xff);
        }
    }

    if (Settings.MSU1 && (Address & 0xfff8) == 0x2000)
        S9xMSU1WritePort(Address & 7, Byte);
    else if ((Address & 0xffc0) == 0x2140)
        S9xAPUWritePort(Address & 3, Byte);
    else if (Address <= 0x2183) {
        switch (Address) {
            case 0x2100:
                if (Byte != snes->mem->FillRAM[0x2100]) {
                    FLUSH_REDRAW();
                    if (Brightness != (Byte & 0xf)) {
                        IPPU.ColorsChanged = TRUE;
                        Brightness         = Byte & 0xf;
                        S9xFixColourBrightness();
                        snes->renderer->S9xBuildDirectColourMaps();
                        if (Brightness > IPPU.MaxBrightness)
                            IPPU.MaxBrightness = Brightness;
                    }
                    if ((snes->mem->FillRAM[0x2100] & 0x80) != (Byte & 0x80)) {
                        IPPU.ColorsChanged = TRUE;
                        ForcedBlanking     = (Byte >> 7) & 1;
                    }
                }
                if ((snes->mem->FillRAM[0x2100] & 0x80) && snes->scpu->V_Counter == ScreenHeight + FIRST_VISIBLE_LINE) {
                    OAMAddr   = SavedOAMAddr;
                    uint8 tmp = 0;
                    if (OAMPriorityRotation)
                        tmp = (OAMAddr & 0xfe) >> 1;
                    if ((OAMFlip & 1) || FirstSprite != tmp) {
                        FirstSprite     = tmp;
                        IPPU.OBJChanged = TRUE;
                    }
                    OAMFlip = 0;
                }
                break;
            case 0x2101:
                if (Byte != snes->mem->FillRAM[0x2101]) {
                    FLUSH_REDRAW();
                    OBJNameBase     = (Byte & 3) << 14;
                    OBJNameSelect   = ((Byte >> 3) & 3) << 13;
                    OBJSizeSelect   = (Byte >> 5) & 7;
                    IPPU.OBJChanged = TRUE;
                }
                break;
            case 0x2102:
                OAMAddr      = ((snes->mem->FillRAM[0x2103] & 1) << 8) | Byte;
                OAMFlip      = 0;
                OAMReadFlip  = 0;
                SavedOAMAddr = OAMAddr;
                if (OAMPriorityRotation && FirstSprite != (OAMAddr >> 1)) {
                    FirstSprite     = (OAMAddr & 0xfe) >> 1;
                    IPPU.OBJChanged = TRUE;
                }
                break;
            case 0x2103:
                OAMAddr             = ((Byte & 1) << 8) | snes->mem->FillRAM[0x2102];
                OAMPriorityRotation = (Byte & 0x80) ? 1 : 0;
                if (OAMPriorityRotation) {
                    if (FirstSprite != (OAMAddr >> 1)) {
                        FirstSprite     = (OAMAddr & 0xfe) >> 1;
                        IPPU.OBJChanged = TRUE;
                    }
                } else {
                    if (FirstSprite != 0) {
                        FirstSprite     = 0;
                        IPPU.OBJChanged = TRUE;
                    }
                }
                OAMFlip      = 0;
                OAMReadFlip  = 0;
                SavedOAMAddr = OAMAddr;
                break;
            case 0x2104:
                REGISTER_2104(Byte);
                break;
            case 0x2105:
                if (Byte != snes->mem->FillRAM[0x2105]) {
                    FLUSH_REDRAW();
                    BG[0].BGSize = (Byte >> 4) & 1;
                    BG[1].BGSize = (Byte >> 5) & 1;
                    BG[2].BGSize = (Byte >> 6) & 1;
                    BG[3].BGSize = (Byte >> 7) & 1;
                    BGMode       = Byte & 7;
                    BG3Priority  = ((Byte & 0x0f) == 0x09);
                    if (BGMode == 6 || BGMode == 5 || BGMode == 7)
                        IPPU.Interlace = snes->mem->FillRAM[0x2133] & 1;
                    else
                        IPPU.Interlace = 0;
                }
                break;
            case 0x2106:
                if (Byte != snes->mem->FillRAM[0x2106]) {
                    FLUSH_REDRAW();
                    MosaicStart = snes->scpu->V_Counter;
                    if (MosaicStart > ScreenHeight)
                        MosaicStart = 0;
                    Mosaic      = (Byte >> 4) + 1;
                    BGMosaic[0] = (Byte & 1);
                    BGMosaic[1] = (Byte & 2);
                    BGMosaic[2] = (Byte & 4);
                    BGMosaic[3] = (Byte & 8);
                }
                break;
            case 0x2107:
                if (Byte != snes->mem->FillRAM[0x2107]) {
                    FLUSH_REDRAW();
                    BG[0].SCSize = Byte & 3;
                    BG[0].SCBase = (Byte & 0x7c) << 8;
                }
                break;
            case 0x2108:
                if (Byte != snes->mem->FillRAM[0x2108]) {
                    FLUSH_REDRAW();
                    BG[1].SCSize = Byte & 3;
                    BG[1].SCBase = (Byte & 0x7c) << 8;
                }
                break;
            case 0x2109:
                if (Byte != snes->mem->FillRAM[0x2109]) {
                    FLUSH_REDRAW();
                    BG[2].SCSize = Byte & 3;
                    BG[2].SCBase = (Byte & 0x7c) << 8;
                }
                break;
            case 0x210a:
                if (Byte != snes->mem->FillRAM[0x210a]) {
                    FLUSH_REDRAW();
                    BG[3].SCSize = Byte & 3;
                    BG[3].SCBase = (Byte & 0x7c) << 8;
                }
                break;
            case 0x210b:
                if (Byte != snes->mem->FillRAM[0x210b]) {
                    FLUSH_REDRAW();
                    BG[0].NameBase = (Byte & 7) << 12;
                    BG[1].NameBase = ((Byte >> 4) & 7) << 12;
                }
                break;
            case 0x210c:
                if (Byte != snes->mem->FillRAM[0x210c]) {
                    FLUSH_REDRAW();
                    BG[2].NameBase = (Byte & 7) << 12;
                    BG[3].NameBase = ((Byte >> 4) & 7) << 12;
                }
                break;
            case 0x210d:
                BG[0].HOffset = (Byte << 8) | (BGnxOFSbyte & ~7) | ((BG[0].HOffset >> 8) & 7);
                M7HOFS        = (Byte << 8) | M7byte;
                BGnxOFSbyte   = Byte;
                M7byte        = Byte;
                break;
            case 0x210e:
                BG[0].VOffset = (Byte << 8) | BGnxOFSbyte;
                M7VOFS        = (Byte << 8) | M7byte;
                BGnxOFSbyte   = Byte;
                M7byte        = Byte;
                break;
            case 0x210f:
                BG[1].HOffset = (Byte << 8) | (BGnxOFSbyte & ~7) | ((BG[1].HOffset >> 8) & 7);
                BGnxOFSbyte   = Byte;
                break;
            case 0x2110:
                BG[1].VOffset = (Byte << 8) | BGnxOFSbyte;
                BGnxOFSbyte   = Byte;
                break;
            case 0x2111:
                BG[2].HOffset = (Byte << 8) | (BGnxOFSbyte & ~7) | ((BG[2].HOffset >> 8) & 7);
                BGnxOFSbyte   = Byte;
                break;
            case 0x2112:
                BG[2].VOffset = (Byte << 8) | BGnxOFSbyte;
                BGnxOFSbyte   = Byte;
                break;
            case 0x2113:
                BG[3].HOffset = (Byte << 8) | (BGnxOFSbyte & ~7) | ((BG[3].HOffset >> 8) & 7);
                BGnxOFSbyte   = Byte;
                break;
            case 0x2114:
                BG[3].VOffset = (Byte << 8) | BGnxOFSbyte;
                BGnxOFSbyte   = Byte;
                break;
            case 0x2115:
                VMA.High = (Byte & 0x80) == 0 ? FALSE : TRUE;
                switch (Byte & 3) {
                    case 0:
                        VMA.Increment = 1;
                        break;
                    case 1:
                        VMA.Increment = 32;
                        break;
                    case 2:
                        VMA.Increment = 128;
                        break;
                    case 3:
                        VMA.Increment = 128;
                        break;
                }
                if (Byte & 0x0c) {
                    uint16 Shift[4]      = {0, 5, 6, 7};
                    uint16 IncCount[4]   = {0, 32, 64, 128};
                    uint8  i             = (Byte & 0x0c) >> 2;
                    VMA.FullGraphicCount = IncCount[i];
                    VMA.Mask1            = IncCount[i] * 8 - 1;
                    VMA.Shift            = Shift[i];
                } else
                    VMA.FullGraphicCount = 0;
                break;
            case 0x2116:
                VMA.Address &= 0xff00;
                VMA.Address |= Byte;
                S9xUpdateVRAMReadBuffer();
                break;
            case 0x2117:
                VMA.Address &= 0x00ff;
                VMA.Address |= Byte << 8;
                S9xUpdateVRAMReadBuffer();
                break;
            case 0x2118:
                REGISTER_2118(Byte);
                break;
            case 0x2119:
                REGISTER_2119(Byte);
                break;
            case 0x211a:
                if (Byte != snes->mem->FillRAM[0x211a]) {
                    FLUSH_REDRAW();
                    Mode7Repeat = Byte >> 6;
                    if (Mode7Repeat == 1)
                        Mode7Repeat = 0;
                    Mode7VFlip = (Byte & 2) >> 1;
                    Mode7HFlip = Byte & 1;
                }
                break;
            case 0x211b:
                MatrixA          = M7byte | (Byte << 8);
                Need16x8Mulitply = TRUE;
                M7byte           = Byte;
                break;
            case 0x211c:
                MatrixB          = M7byte | (Byte << 8);
                Need16x8Mulitply = TRUE;
                M7byte           = Byte;
                break;
            case 0x211d:
                MatrixC = M7byte | (Byte << 8);
                M7byte  = Byte;
                break;
            case 0x211e:
                MatrixD = M7byte | (Byte << 8);
                M7byte  = Byte;
                break;
            case 0x211f:
                CentreX = M7byte | (Byte << 8);
                M7byte  = Byte;
                break;
            case 0x2120:
                CentreY = M7byte | (Byte << 8);
                M7byte  = Byte;
                break;
            case 0x2121:
                CGFLIP     = 0;
                CGFLIPRead = 0;
                CGADD      = Byte;
                break;
            case 0x2122:
                REGISTER_2122(Byte);
                break;
            case 0x2123:
                if (Byte != snes->mem->FillRAM[0x2123]) {
                    FLUSH_REDRAW();
                    ClipWindow1Enable[0] = !!(Byte & 0x02);
                    ClipWindow1Enable[1] = !!(Byte & 0x20);
                    ClipWindow2Enable[0] = !!(Byte & 0x08);
                    ClipWindow2Enable[1] = !!(Byte & 0x80);
                    ClipWindow1Inside[0] = !(Byte & 0x01);
                    ClipWindow1Inside[1] = !(Byte & 0x10);
                    ClipWindow2Inside[0] = !(Byte & 0x04);
                    ClipWindow2Inside[1] = !(Byte & 0x40);
                    RecomputeClipWindows = TRUE;
                }
                break;
            case 0x2124:
                if (Byte != snes->mem->FillRAM[0x2124]) {
                    FLUSH_REDRAW();
                    ClipWindow1Enable[2] = !!(Byte & 0x02);
                    ClipWindow1Enable[3] = !!(Byte & 0x20);
                    ClipWindow2Enable[2] = !!(Byte & 0x08);
                    ClipWindow2Enable[3] = !!(Byte & 0x80);
                    ClipWindow1Inside[2] = !(Byte & 0x01);
                    ClipWindow1Inside[3] = !(Byte & 0x10);
                    ClipWindow2Inside[2] = !(Byte & 0x04);
                    ClipWindow2Inside[3] = !(Byte & 0x40);
                    RecomputeClipWindows = TRUE;
                }
                break;
            case 0x2125:
                if (Byte != snes->mem->FillRAM[0x2125]) {
                    FLUSH_REDRAW();
                    ClipWindow1Enable[4] = !!(Byte & 0x02);
                    ClipWindow1Enable[5] = !!(Byte & 0x20);
                    ClipWindow2Enable[4] = !!(Byte & 0x08);
                    ClipWindow2Enable[5] = !!(Byte & 0x80);
                    ClipWindow1Inside[4] = !(Byte & 0x01);
                    ClipWindow1Inside[5] = !(Byte & 0x10);
                    ClipWindow2Inside[4] = !(Byte & 0x04);
                    ClipWindow2Inside[5] = !(Byte & 0x40);
                    RecomputeClipWindows = TRUE;
                }
                break;
            case 0x2126:
                if (Byte != snes->mem->FillRAM[0x2126]) {
                    FLUSH_REDRAW();
                    Window1Left          = Byte;
                    RecomputeClipWindows = TRUE;
                }
                break;
            case 0x2127:
                if (Byte != snes->mem->FillRAM[0x2127]) {
                    FLUSH_REDRAW();
                    Window1Right         = Byte;
                    RecomputeClipWindows = TRUE;
                }
                break;
            case 0x2128:
                if (Byte != snes->mem->FillRAM[0x2128]) {
                    FLUSH_REDRAW();
                    Window2Left          = Byte;
                    RecomputeClipWindows = TRUE;
                }
                break;
            case 0x2129:
                if (Byte != snes->mem->FillRAM[0x2129]) {
                    FLUSH_REDRAW();
                    Window2Right         = Byte;
                    RecomputeClipWindows = TRUE;
                }
                break;
            case 0x212a:
                if (Byte != snes->mem->FillRAM[0x212a]) {
                    FLUSH_REDRAW();
                    ClipWindowOverlapLogic[0] = (Byte & 0x03);
                    ClipWindowOverlapLogic[1] = (Byte & 0x0c) >> 2;
                    ClipWindowOverlapLogic[2] = (Byte & 0x30) >> 4;
                    ClipWindowOverlapLogic[3] = (Byte & 0xc0) >> 6;
                    RecomputeClipWindows      = TRUE;
                }
                break;
            case 0x212b:
                if (Byte != snes->mem->FillRAM[0x212b]) {
                    FLUSH_REDRAW();
                    ClipWindowOverlapLogic[4] = (Byte & 0x03);
                    ClipWindowOverlapLogic[5] = (Byte & 0x0c) >> 2;
                    RecomputeClipWindows      = TRUE;
                }
                break;
            case 0x212c:
                if (Byte != snes->mem->FillRAM[0x212c]) {
                    FLUSH_REDRAW();
                    RecomputeClipWindows = TRUE;
                }
                break;
            case 0x212d:
                if (Byte != snes->mem->FillRAM[0x212d]) {
                    FLUSH_REDRAW();
                    RecomputeClipWindows = TRUE;
                }
                break;
            case 0x212e:
                if (Byte != snes->mem->FillRAM[0x212e]) {
                    FLUSH_REDRAW();
                    RecomputeClipWindows = TRUE;
                }
                break;
            case 0x212f:
                if (Byte != snes->mem->FillRAM[0x212f]) {
                    FLUSH_REDRAW();
                    RecomputeClipWindows = TRUE;
                }
                break;
            case 0x2130:
                if (Byte != snes->mem->FillRAM[0x2130]) {
                    FLUSH_REDRAW();
                    RecomputeClipWindows = TRUE;
                }
                break;
            case 0x2131:
                if (Byte != snes->mem->FillRAM[0x2131]) {
                    FLUSH_REDRAW();
                }
                break;
            case 0x2132:
                if (Byte != snes->mem->FillRAM[0x2132]) {
                    FLUSH_REDRAW();
                    if (Byte & 0x80)
                        FixedColourBlue = Byte & 0x1f;
                    if (Byte & 0x40)
                        FixedColourGreen = Byte & 0x1f;
                    if (Byte & 0x20)
                        FixedColourRed = Byte & 0x1f;
                }
                break;
            case 0x2133:
                if (Byte != snes->mem->FillRAM[0x2133]) {
                    if ((snes->mem->FillRAM[0x2133] ^ Byte) & 8) {
                        FLUSH_REDRAW();
                        IPPU.PseudoHires = Byte & 8;
                    }
                    if (Byte & 0x04) {
                        ScreenHeight = SNES_HEIGHT_EXTENDED;
                        if (IPPU.DoubleHeightPixels)
                            IPPU.RenderedScreenHeight = ScreenHeight << 1;
                        else
                            IPPU.RenderedScreenHeight = ScreenHeight;
                    } else {
                        ScreenHeight = SNES_HEIGHT;
                        if (IPPU.DoubleHeightPixels)
                            IPPU.RenderedScreenHeight = ScreenHeight << 1;
                        else
                            IPPU.RenderedScreenHeight = ScreenHeight;
                    }
                    if ((snes->mem->FillRAM[0x2133] ^ Byte) & 3) {
                        FLUSH_REDRAW();
                        if ((snes->mem->FillRAM[0x2133] ^ Byte) & 2)
                            IPPU.OBJChanged = TRUE;
                        IPPU.Interlace    = Byte & 1;
                        IPPU.InterlaceOBJ = Byte & 2;
                    }
                }
                break;
            case 0x2134:
            case 0x2135:
            case 0x2136:
            case 0x2137:
            case 0x2138:
            case 0x2139:
            case 0x213a:
            case 0x213b:
            case 0x213c:
            case 0x213d:
            case 0x213e:
            case 0x213f:
                return;
            case 0x2180:
                if (!snes->scpu->InWRAMDMAorHDMA)
                    REGISTER_2180(Byte);
                break;
            case 0x2181:
                if (!snes->scpu->InWRAMDMAorHDMA) {
                    WRAM &= 0x1ff00;
                    WRAM |= Byte;
                }
                break;
            case 0x2182:
                if (!snes->scpu->InWRAMDMAorHDMA) {
                    WRAM &= 0x100ff;
                    WRAM |= Byte << 8;
                }
                break;
            case 0x2183:
                if (!snes->scpu->InWRAMDMAorHDMA) {
                    WRAM &= 0x0ffff;
                    WRAM |= Byte << 16;
                    WRAM &= 0x1ffff;
                }
                break;
        }
    } else {
    }
    snes->mem->FillRAM[Address] = Byte;
}
uint8 SPPU::S9xGetPPU(uint16 Address)
{
    if (Settings.MSU1 && (Address & 0xfff8) == 0x2000)
        return (S9xMSU1ReadPort(Address & 7));
    else if (Address < 0x2100)
        return (snes->OpenBus);
    if (snes->scpu->InDMAorHDMA) {
        if (snes->scpu->CurrentDMAorHDMAChannel >= 0 &&
            !snes->dma->DMA[snes->scpu->CurrentDMAorHDMAChannel].ReverseTransfer) {
            if ((Address & 0xff00) == 0x2100)
                return (snes->OpenBus);
            else
                return (snes->OpenBus);
        } else {
            if (Address > 0x21ff)
                Address = 0x2100 + (Address & 0xff);
        }
    }
    if ((Address & 0xffc0) == 0x2140)
        return (S9xAPUReadPort(Address & 3));
    else if (Address <= 0x2183) {
        uint8 byte;
        switch (Address) {
            case 0x2104:
            case 0x2105:
            case 0x2106:
            case 0x2108:
            case 0x2109:
            case 0x210a:
            case 0x2114:
            case 0x2115:
            case 0x2116:
            case 0x2118:
            case 0x2119:
            case 0x211a:
            case 0x2124:
            case 0x2125:
            case 0x2126:
            case 0x2128:
            case 0x2129:
            case 0x212a:
                return (OpenBus1);
            case 0x2134:
            case 0x2135:
            case 0x2136:
                if (Need16x8Mulitply) {
                    int32 r                    = (int32)MatrixA * (int32)(MatrixB >> 8);
                    snes->mem->FillRAM[0x2134] = (uint8)r;
                    snes->mem->FillRAM[0x2135] = (uint8)(r >> 8);
                    snes->mem->FillRAM[0x2136] = (uint8)(r >> 16);
                    Need16x8Mulitply           = FALSE;
                }
                return (OpenBus1 = snes->mem->FillRAM[Address]);
            case 0x2137:
                S9xLatchCounters(0);
                return (OpenBus1);
            case 0x2138:
                if (OAMAddr & 0x100) {
                    if (!(OAMFlip & 1))
                        byte = OAMData[(OAMAddr & 0x10f) << 1];
                    else {
                        byte    = OAMData[((OAMAddr & 0x10f) << 1) + 1];
                        OAMAddr = (OAMAddr + 1) & 0x1ff;
                        if (OAMPriorityRotation && FirstSprite != (OAMAddr >> 1)) {
                            FirstSprite     = (OAMAddr & 0xfe) >> 1;
                            IPPU.OBJChanged = TRUE;
                        }
                    }
                } else {
                    if (!(OAMFlip & 1))
                        byte = OAMData[snes->ppu->OAMAddr << 1];
                    else {
                        byte = OAMData[(OAMAddr << 1) + 1];
                        ++OAMAddr;
                        if (OAMPriorityRotation && FirstSprite != (OAMAddr >> 1)) {
                            FirstSprite     = (OAMAddr & 0xfe) >> 1;
                            IPPU.OBJChanged = TRUE;
                        }
                    }
                }
                OAMFlip ^= 1;
                return (OpenBus1 = byte);
            case 0x2139:
                byte = VRAMReadBuffer & 0xff;
                if (!VMA.High) {
                    S9xUpdateVRAMReadBuffer();
                    VMA.Address += VMA.Increment;
                }

                return (OpenBus1 = byte);
            case 0x213a:
                byte = (VRAMReadBuffer >> 8) & 0xff;
                if (VMA.High) {
                    S9xUpdateVRAMReadBuffer();
                    VMA.Address += VMA.Increment;
                }

                return (OpenBus1 = byte);
            case 0x213b:
                if (CGFLIPRead)
                    byte = (OpenBus2 & 0x80) | ((CGDATA[snes->ppu->CGADD++] >> 8) & 0x7f);
                else
                    byte = CGDATA[snes->ppu->CGADD] & 0xff;
                CGFLIPRead ^= 1;
                return (OpenBus2 = byte);
            case 0x213c:
                S9xTryGunLatch(false);
                if (HBeamFlip)
                    byte = (OpenBus2 & 0xfe) | ((HBeamPosLatched >> 8) & 0x01);
                else
                    byte = (uint8)HBeamPosLatched;
                HBeamFlip ^= 1;
                return (OpenBus2 = byte);
            case 0x213d:
                S9xTryGunLatch(false);
                if (VBeamFlip)
                    byte = (OpenBus2 & 0xfe) | ((VBeamPosLatched >> 8) & 0x01);
                else
                    byte = (uint8)VBeamPosLatched;
                VBeamFlip ^= 1;
                return (OpenBus2 = byte);
            case 0x213e:
                FLUSH_REDRAW();
                byte = (OpenBus1 & 0x10) | RangeTimeOver | snes->Model->_5C77;
                return (OpenBus1 = byte);
            case 0x213f:
                S9xTryGunLatch(false);
                VBeamFlip = HBeamFlip = 0;
                byte = (OpenBus2 & 0x20) | (snes->mem->FillRAM[0x213f] & 0xc0) | (Settings.PAL ? 0x10 : 0) |
                       snes->Model->_5C78;
                snes->mem->FillRAM[0x213f] &= ~0x40;
                return (OpenBus2 = byte);
            case 0x2180:
                if (!snes->scpu->InWRAMDMAorHDMA) {
                    byte = snes->mem->RAM[snes->ppu->WRAM++];
                    WRAM &= 0x1ffff;
                } else
                    byte = snes->OpenBus;
                return (byte);
            default:
                return (snes->OpenBus);
        }
    } else {
        switch (Address) {
            case 0x21c2:
                if (snes->Model->_5C77 == 2)
                    return (0x20);
                return (snes->OpenBus);
            case 0x21c3:
                if (snes->Model->_5C77 == 2)
                    return (0);
                return (snes->OpenBus);
            default:
                return (snes->OpenBus);
        }
    }
}
void SPPU::S9xSetCPU(uint8 Byte, uint16 Address)
{
    if (Address < 0x4200) {
        switch (Address) {
            case 0x4016:
                break;
            case 0x4017:
                return;
            default:
                break;
        }
    } else if ((Address & 0xff80) == 0x4300) {
        if (snes->scpu->InDMAorHDMA)
            return;
        int d = (Address >> 4) & 0x7;
        switch (Address & 0xf) {
            case 0x0:
                snes->dma->DMA[d].ReverseTransfer        = (Byte & 0x80) ? TRUE : FALSE;
                snes->dma->DMA[d].HDMAIndirectAddressing = (Byte & 0x40) ? TRUE : FALSE;
                snes->dma->DMA[d].UnusedBit43x0          = (Byte & 0x20) ? TRUE : FALSE;
                snes->dma->DMA[d].AAddressDecrement      = (Byte & 0x10) ? TRUE : FALSE;
                snes->dma->DMA[d].AAddressFixed          = (Byte & 0x08) ? TRUE : FALSE;
                snes->dma->DMA[d].TransferMode           = (Byte & 7);
                return;
            case 0x1:
                snes->dma->DMA[d].BAddress = Byte;
                return;
            case 0x2:
                snes->dma->DMA[d].AAddress &= 0xff00;
                snes->dma->DMA[d].AAddress |= Byte;
                return;
            case 0x3:
                snes->dma->DMA[d].AAddress &= 0xff;
                snes->dma->DMA[d].AAddress |= Byte << 8;
                return;
            case 0x4:
                snes->dma->DMA[d].ABank       = Byte;
                snes->dma->HDMAMemPointers[d] = NULL;
                return;
            case 0x5:
                snes->dma->DMA[d].DMACount_Or_HDMAIndirectAddress &= 0xff00;
                snes->dma->DMA[d].DMACount_Or_HDMAIndirectAddress |= Byte;
                snes->dma->HDMAMemPointers[d] = NULL;
                return;
            case 0x6:
                snes->dma->DMA[d].DMACount_Or_HDMAIndirectAddress &= 0xff;
                snes->dma->DMA[d].DMACount_Or_HDMAIndirectAddress |= Byte << 8;
                snes->dma->HDMAMemPointers[d] = NULL;
                return;
            case 0x7:
                snes->dma->DMA[d].IndirectBank = Byte;
                snes->dma->HDMAMemPointers[d]  = NULL;
                return;
            case 0x8:
                snes->dma->DMA[d].Address &= 0xff00;
                snes->dma->DMA[d].Address |= Byte;
                snes->dma->HDMAMemPointers[d] = NULL;
                return;
            case 0x9:
                snes->dma->DMA[d].Address &= 0xff;
                snes->dma->DMA[d].Address |= Byte << 8;
                snes->dma->HDMAMemPointers[d] = NULL;
                return;
            case 0xa:
                if (Byte & 0x7f) {
                    snes->dma->DMA[d].LineCount = Byte & 0x7f;
                    snes->dma->DMA[d].Repeat    = !(Byte & 0x80);
                } else {
                    snes->dma->DMA[d].LineCount = 128;
                    snes->dma->DMA[d].Repeat    = !!(Byte & 0x80);
                }
                return;
            case 0xb:
            case 0xf:
                snes->dma->DMA[d].UnknownByte = Byte;
                return;
            default:
                break;
        }
    } else {
        uint16 pos;
        switch (Address) {
            case 0x4200:
                if (Byte == snes->mem->FillRAM[0x4200])
                    break;
                if (Byte & 0x20) {
                    VTimerEnabled = TRUE;
                } else
                    VTimerEnabled = FALSE;
                if (Byte & 0x10) {
                    HTimerEnabled = TRUE;
                } else
                    HTimerEnabled = FALSE;
                if (!(Byte & 0x10) && !(Byte & 0x20)) {
                    snes->scpu->IRQLine       = FALSE;
                    snes->scpu->IRQTransition = FALSE;
                }
                if ((Byte & 0x30) != (snes->mem->FillRAM[0x4200] & 0x30)) {
                    if ((Byte & 0x30) == 0 || (snes->mem->FillRAM[0x4200] & 0x30) == 0)
                        S9xUpdateIRQPositions(true);
                    else
                        S9xUpdateIRQPositions(false);
                }
                if ((Byte & 0x80) && !(snes->mem->FillRAM[0x4200] & 0x80) &&
                    (snes->scpu->V_Counter >= ScreenHeight + FIRST_VISIBLE_LINE) &&
                    (snes->mem->FillRAM[0x4210] & 0x80)) {
                    Timings.IRQFlagChanging |= IRQ_TRIGGER_NMI;
                }

                break;
            case 0x4201:
                if ((Byte & 0x80) == 0 && (snes->mem->FillRAM[0x4213] & 0x80) == 0x80)
                    S9xLatchCounters(1);
                else
                    S9xTryGunLatch((Byte & 0x80) ? true : false);
                snes->mem->FillRAM[0x4201] = snes->mem->FillRAM[0x4213] = Byte;
                break;
            case 0x4202:
                break;
            case 0x4203: {
                uint32 res                 = snes->mem->FillRAM[0x4202] * Byte;
                snes->mem->FillRAM[0x4216] = (uint8)res;
                snes->mem->FillRAM[0x4217] = (uint8)(res >> 8);
                break;
            }
            case 0x4204:
            case 0x4205:
                break;
            case 0x4206: {
                uint16 a                   = snes->mem->FillRAM[0x4204] + (snes->mem->FillRAM[0x4205] << 8);
                uint16 div                 = Byte ? a / Byte : 0xffff;
                uint16 rem                 = Byte ? a % Byte : a;
                snes->mem->FillRAM[0x4214] = (uint8)div;
                snes->mem->FillRAM[0x4215] = div >> 8;
                snes->mem->FillRAM[0x4216] = (uint8)rem;
                snes->mem->FillRAM[0x4217] = rem >> 8;
                break;
            }
            case 0x4207:
                pos         = IRQHBeamPos;
                IRQHBeamPos = (IRQHBeamPos & 0xff00) | Byte;
                if (IRQHBeamPos != pos)
                    S9xUpdateIRQPositions(false);
                break;
            case 0x4208:
                pos         = IRQHBeamPos;
                IRQHBeamPos = (IRQHBeamPos & 0xff) | ((Byte & 1) << 8);
                if (IRQHBeamPos != pos)
                    S9xUpdateIRQPositions(false);
                break;
            case 0x4209:
                pos         = IRQVBeamPos;
                IRQVBeamPos = (IRQVBeamPos & 0xff00) | Byte;
                if (IRQVBeamPos != pos)
                    S9xUpdateIRQPositions(true);
                break;
            case 0x420a:
                pos         = IRQVBeamPos;
                IRQVBeamPos = (IRQVBeamPos & 0xff) | ((Byte & 1) << 8);
                if (IRQVBeamPos != pos)
                    S9xUpdateIRQPositions(true);
                break;
            case 0x420b:
                if (snes->scpu->InDMAorHDMA)
                    return;
                if (Byte) {
                    snes->scpu->Cycles += Timings.DMACPUSync;
                }
                if (Byte & 0x01)
                    snes->dma->S9xDoDMA(0);
                if (Byte & 0x02)
                    snes->dma->S9xDoDMA(1);
                if (Byte & 0x04)
                    snes->dma->S9xDoDMA(2);
                if (Byte & 0x08)
                    snes->dma->S9xDoDMA(3);
                if (Byte & 0x10)
                    snes->dma->S9xDoDMA(4);
                if (Byte & 0x20)
                    snes->dma->S9xDoDMA(5);
                if (Byte & 0x40)
                    snes->dma->S9xDoDMA(6);
                if (Byte & 0x80)
                    snes->dma->S9xDoDMA(7);
                break;
            case 0x420c:
                if (snes->scpu->InDMAorHDMA)
                    return;
                snes->mem->FillRAM[0x420c] = Byte;
                HDMA                       = Byte & ~HDMAEnded;
                break;
            case 0x420d:
                if ((Byte & 1) != (snes->mem->FillRAM[0x420d] & 1)) {
                    if (Byte & 1) {
                        snes->scpu->FastROMSpeed = ONE_CYCLE;
                    } else
                        snes->scpu->FastROMSpeed = SLOW_ONE_CYCLE;
                    snes->mem->S9xSetPCBase(snes->scpu->Registers.PBPC);
                }
                break;
            case 0x4210:
            case 0x4211:
            case 0x4212:
            case 0x4213:
            case 0x4214:
            case 0x4215:
            case 0x4216:
            case 0x4217:
            case 0x4218:
            case 0x4219:
            case 0x421a:
            case 0x421b:
            case 0x421c:
            case 0x421d:
            case 0x421e:
            case 0x421f:
                return;
            default:
                break;
        }
    }
    snes->mem->FillRAM[Address] = Byte;
}
uint8 SPPU::S9xGetCPU(uint16 Address)
{
    if (Address < 0x4200) {
        switch (Address) {
            case 0x4016:
            case 0x4017:
                return 0;
            default:
                return (snes->OpenBus);
        }
    } else if ((Address & 0xff80) == 0x4300) {
        if (snes->scpu->InDMAorHDMA)
            return (snes->OpenBus);
        int d = (Address >> 4) & 0x7;
        switch (Address & 0xf) {
            case 0x0:
                return ((snes->dma->DMA[d].ReverseTransfer ? 0x80 : 0) |
                        (snes->dma->DMA[d].HDMAIndirectAddressing ? 0x40 : 0) |
                        (snes->dma->DMA[d].UnusedBit43x0 ? 0x20 : 0) |
                        (snes->dma->DMA[d].AAddressDecrement ? 0x10 : 0) |
                        (snes->dma->DMA[d].AAddressFixed ? 0x08 : 0) | (snes->dma->DMA[d].TransferMode & 7));
            case 0x1:
                return (snes->dma->DMA[d].BAddress);
            case 0x2:
                return (snes->dma->DMA[d].AAddress & 0xff);
            case 0x3:
                return (snes->dma->DMA[d].AAddress >> 8);
            case 0x4:
                return (snes->dma->DMA[d].ABank);
            case 0x5:
                return (snes->dma->DMA[d].DMACount_Or_HDMAIndirectAddress & 0xff);
            case 0x6:
                return (snes->dma->DMA[d].DMACount_Or_HDMAIndirectAddress >> 8);
            case 0x7:
                return (snes->dma->DMA[d].IndirectBank);
            case 0x8:
                return (snes->dma->DMA[d].Address & 0xff);
            case 0x9:
                return (snes->dma->DMA[d].Address >> 8);
            case 0xa:
                return (snes->dma->DMA[d].LineCount ^ (snes->dma->DMA[d].Repeat ? 0x00 : 0x80));
            case 0xb:
            case 0xf:
                return (snes->dma->DMA[d].UnknownByte);
            default:
                return (snes->OpenBus);
        }
    } else {
        uint8 byte;
        switch (Address) {
            case 0x4210:
                byte                       = snes->mem->FillRAM[0x4210];
                snes->mem->FillRAM[0x4210] = snes->Model->_5A22;
                return ((byte & 0x80) | (snes->OpenBus & 0x70) | snes->Model->_5A22);
            case 0x4211:
                byte = 0;
                if (snes->scpu->IRQLine) {
                    byte                      = 0x80;
                    snes->scpu->IRQLine       = FALSE;
                    snes->scpu->IRQTransition = FALSE;
                }
                return (byte | (snes->OpenBus & 0x7f));
            case 0x4212:
                return (REGISTER_4212() | (snes->OpenBus & 0x3e));
            case 0x4213:
                return (snes->mem->FillRAM[0x4213]);
            case 0x4214:
            case 0x4215:
            case 0x4216:
            case 0x4217:
                return (snes->mem->FillRAM[Address]);
            case 0x4218:
            case 0x4219:
            case 0x421a:
            case 0x421b:
            case 0x421c:
            case 0x421d:
            case 0x421e:
            case 0x421f:
                return (snes->mem->FillRAM[Address]);
            default:
                return (snes->OpenBus);
        }
    }
}
void SPPU::S9xResetPPU(void)
{
    S9xSoftResetPPU();
    M7HOFS = 0;
    M7VOFS = 0;
    M7byte = 0;
}
void SPPU::S9xResetPPUFast(void)
{
    RecomputeClipWindows = TRUE;
    IPPU.ColorsChanged   = TRUE;
    IPPU.OBJChanged      = TRUE;
    memset(IPPU.TileCached[TILE_2BIT], 0, MAX_2BIT_TILES);
    memset(IPPU.TileCached[TILE_4BIT], 0, MAX_4BIT_TILES);
    memset(IPPU.TileCached[TILE_8BIT], 0, MAX_8BIT_TILES);
    memset(IPPU.TileCached[TILE_2BIT_EVEN], 0, MAX_2BIT_TILES);
    memset(IPPU.TileCached[TILE_2BIT_ODD], 0, MAX_2BIT_TILES);
    memset(IPPU.TileCached[TILE_4BIT_EVEN], 0, MAX_4BIT_TILES);
    memset(IPPU.TileCached[TILE_4BIT_ODD], 0, MAX_4BIT_TILES);
}
void SPPU::S9xSoftResetPPU(void)
{
    VMA.High             = 0;
    VMA.Increment        = 1;
    VMA.Address          = 0;
    VMA.FullGraphicCount = 0;
    VMA.Shift            = 0;
    WRAM                 = 0;
    for (int c = 0; c < 4; c++) {
        BG[c].SCBase   = 0;
        BG[c].HOffset  = 0;
        BG[c].VOffset  = 0;
        BG[c].BGSize   = 0;
        BG[c].NameBase = 0;
        BG[c].SCSize   = 0;
    }
    BGMode      = 0;
    BG3Priority = 0;
    CGFLIP      = 0;
    CGFLIPRead  = 0;
    CGADD       = 0;
    for (int c = 0; c < 256; c++) {
        IPPU.Red[c]   = (c & 7) << 2;
        IPPU.Green[c] = ((c >> 3) & 7) << 2;
        IPPU.Blue[c]  = ((c >> 6) & 2) << 3;
        CGDATA[c]     = IPPU.Red[c] | (IPPU.Green[c] << 5) | (IPPU.Blue[c] << 10);
    }
    for (int c = 0; c < 128; c++) {
        OBJ[c].HPos     = 0;
        OBJ[c].VPos     = 0;
        OBJ[c].HFlip    = 0;
        OBJ[c].VFlip    = 0;
        OBJ[c].Name     = 0;
        OBJ[c].Priority = 0;
        OBJ[c].Palette  = 0;
        OBJ[c].Size     = 0;
    }
    OBJThroughMain      = FALSE;
    OBJThroughSub       = FALSE;
    OBJAddition         = FALSE;
    OBJNameBase         = 0;
    OBJNameSelect       = 0;
    OBJSizeSelect       = 0;
    OAMAddr             = 0;
    SavedOAMAddr        = 0;
    OAMPriorityRotation = 0;
    OAMFlip             = 0;
    OAMReadFlip         = 0;
    OAMTileAddress      = 0;
    OAMWriteRegister    = 0;
    memset(OAMData, 0, 512 + 32);
    FirstSprite          = 0;
    LastSprite           = 127;
    RangeTimeOver        = 0;
    HTimerEnabled        = FALSE;
    VTimerEnabled        = FALSE;
    HTimerPosition       = Timings.H_Max + 1;
    VTimerPosition       = Timings.V_Max + 1;
    IRQHBeamPos          = 0x1ff;
    IRQVBeamPos          = 0x1ff;
    HBeamFlip            = 0;
    VBeamFlip            = 0;
    HBeamPosLatched      = 0;
    VBeamPosLatched      = 0;
    GunHLatch            = 0;
    GunVLatch            = 1000;
    HVBeamCounterLatched = 0;
    Mode7HFlip           = FALSE;
    Mode7VFlip           = FALSE;
    Mode7Repeat          = 0;
    MatrixA              = 0;
    MatrixB              = 0;
    MatrixC              = 0;
    MatrixD              = 0;
    CentreX              = 0;
    CentreY              = 0;
    Mosaic               = 0;
    BGMosaic[0]          = FALSE;
    BGMosaic[1]          = FALSE;
    BGMosaic[2]          = FALSE;
    BGMosaic[3]          = FALSE;
    Window1Left          = 1;
    Window1Right         = 0;
    Window2Left          = 1;
    Window2Right         = 0;
    RecomputeClipWindows = TRUE;
    for (int c = 0; c < 6; c++) {
        ClipCounts[c]             = 0;
        ClipWindowOverlapLogic[c] = CLIP_OR;
        ClipWindow1Enable[c]      = FALSE;
        ClipWindow2Enable[c]      = FALSE;
        ClipWindow1Inside[c]      = TRUE;
        ClipWindow2Inside[c]      = TRUE;
    }
    ForcedBlanking   = TRUE;
    FixedColourRed   = 0;
    FixedColourGreen = 0;
    FixedColourBlue  = 0;
    Brightness       = 0;
    ScreenHeight     = SNES_HEIGHT;
    Need16x8Mulitply = FALSE;
    BGnxOFSbyte      = 0;
    HDMA             = 0;
    HDMAEnded        = 0;
    OpenBus1         = 0;
    OpenBus2         = 0;
    for (int c = 0; c < 2; c++)
        memset(&IPPU.Clip[c], 0, sizeof(struct ClipData));
    IPPU.ColorsChanged = TRUE;
    IPPU.OBJChanged    = TRUE;
    memset(IPPU.TileCached[TILE_2BIT], 0, MAX_2BIT_TILES);
    memset(IPPU.TileCached[TILE_4BIT], 0, MAX_4BIT_TILES);
    memset(IPPU.TileCached[TILE_8BIT], 0, MAX_8BIT_TILES);
    memset(IPPU.TileCached[TILE_2BIT_EVEN], 0, MAX_2BIT_TILES);
    memset(IPPU.TileCached[TILE_2BIT_ODD], 0, MAX_2BIT_TILES);
    memset(IPPU.TileCached[TILE_4BIT_EVEN], 0, MAX_4BIT_TILES);
    memset(IPPU.TileCached[TILE_4BIT_ODD], 0, MAX_4BIT_TILES);
    VRAMReadBuffer          = 0;
    GFX.InterlaceFrame      = 0;
    GFX.DoInterlace         = 0;
    IPPU.Interlace          = FALSE;
    IPPU.InterlaceOBJ       = FALSE;
    IPPU.DoubleWidthPixels  = FALSE;
    IPPU.DoubleHeightPixels = FALSE;
    IPPU.CurrentLine        = 0;
    IPPU.PreviousLine       = 0;
    IPPU.XB                 = NULL;
    for (int c = 0; c < 256; c++)
        IPPU.ScreenColors[c] = c;
    IPPU.MaxBrightness               = 0;
    IPPU.RenderThisFrame             = TRUE;
    IPPU.RenderedScreenWidth         = SNES_WIDTH;
    IPPU.RenderedScreenHeight        = SNES_HEIGHT;
    IPPU.FrameCount                  = 0;
    IPPU.RenderedFramesCount         = 0;
    IPPU.DisplayedRenderedFrameCount = 0;
    IPPU.SkippedFrames               = 0;
    IPPU.FrameSkip                   = 0;
    S9xFixColourBrightness();
    snes->renderer->S9xBuildDirectColourMaps();
    for (int c = 0; c < 0x8000; c += 0x100)
        memset(&snes->mem->FillRAM[c], c >> 8, 0x100);
    memset(&snes->mem->FillRAM[0x2100], 0, 0x100);
    memset(&snes->mem->FillRAM[0x4200], 0, 0x100);
    memset(&snes->mem->FillRAM[0x4000], 0, 0x100);
    memset(&snes->mem->FillRAM[0x1000], 0, 0x1000);
    snes->mem->FillRAM[0x4201] = snes->mem->FillRAM[0x4213] = 0xff;
    snes->mem->FillRAM[0x2126] = snes->mem->FillRAM[0x2128] = 1;
}
void SPPU::FLUSH_REDRAW(void)
{
    if (IPPU.PreviousLine != IPPU.CurrentLine)
        snes->renderer->S9xUpdateScreen();
}
void SPPU::S9xUpdateVRAMReadBuffer()
{
    if (VMA.FullGraphicCount) {
        uint32 addr    = VMA.Address;
        uint32 rem     = addr & VMA.Mask1;
        uint32 address = (addr & ~VMA.Mask1) + (rem >> VMA.Shift) + ((rem & (VMA.FullGraphicCount - 1)) << 3);
        VRAMReadBuffer = READ_WORD(snes->mem->VRAM + ((address << 1) & 0xffff));
    } else
        VRAMReadBuffer = READ_WORD(snes->mem->VRAM + ((VMA.Address << 1) & 0xffff));
}
void SPPU::REGISTER_2104(uint8 Byte)
{
    if (!(OAMFlip & 1)) {
        OAMWriteRegister &= 0xff00;
        OAMWriteRegister |= Byte;
    }
    if (OAMAddr & 0x100) {
        int addr = ((OAMAddr & 0x10f) << 1) + (OAMFlip & 1);
        if (Byte != OAMData[addr]) {
            FLUSH_REDRAW();
            OAMData[addr]     = Byte;
            IPPU.OBJChanged   = TRUE;
            struct SOBJ *pObj = &OBJ[(addr & 0x1f) * 4];
            pObj->HPos        = (pObj->HPos & 0xFF) | SignExtend[(Byte >> 0) & 1];
            pObj++->Size      = Byte & 2;
            pObj->HPos        = (pObj->HPos & 0xFF) | SignExtend[(Byte >> 2) & 1];
            pObj++->Size      = Byte & 8;
            pObj->HPos        = (pObj->HPos & 0xFF) | SignExtend[(Byte >> 4) & 1];
            pObj++->Size      = Byte & 32;
            pObj->HPos        = (pObj->HPos & 0xFF) | SignExtend[(Byte >> 6) & 1];
            pObj->Size        = Byte & 128;
        }
    } else if (OAMFlip & 1) {
        OAMWriteRegister &= 0x00ff;
        uint8 lowbyte  = (uint8)(OAMWriteRegister);
        uint8 highbyte = Byte;
        OAMWriteRegister |= Byte << 8;
        int addr = (OAMAddr << 1);
        if (lowbyte != OAMData[addr] || highbyte != OAMData[addr + 1]) {
            FLUSH_REDRAW();
            OAMData[addr]     = lowbyte;
            OAMData[addr + 1] = highbyte;
            IPPU.OBJChanged   = TRUE;
            if (addr & 2) {
                OBJ[addr = OAMAddr >> 1].Name = OAMWriteRegister & 0x1ff;
                OBJ[addr].Palette             = (highbyte >> 1) & 7;
                OBJ[addr].Priority            = (highbyte >> 4) & 3;
                OBJ[addr].HFlip               = (highbyte >> 6) & 1;
                OBJ[addr].VFlip               = (highbyte >> 7) & 1;
            } else {
                OBJ[addr = OAMAddr >> 1].HPos &= 0xff00;
                OBJ[addr].HPos |= lowbyte;
                OBJ[addr].VPos = highbyte;
            }
        }
    }
    OAMFlip ^= 1;
    if (!(OAMFlip & 1)) {
        ++OAMAddr;
        OAMAddr &= 0x1ff;
        if (OAMPriorityRotation && FirstSprite != (OAMAddr >> 1)) {
            FirstSprite     = (OAMAddr & 0xfe) >> 1;
            IPPU.OBJChanged = TRUE;
        }
    } else {
        if (OAMPriorityRotation && (OAMAddr & 1))
            IPPU.OBJChanged = TRUE;
    }
}
void SPPU::REGISTER_2118(uint8 Byte)
{
    CHECK_INBLANK();
    uint32 address;
    if (VMA.FullGraphicCount) {
        uint32 rem = VMA.Address & VMA.Mask1;
        address = (((VMA.Address & ~VMA.Mask1) + (rem >> VMA.Shift) + ((rem & (VMA.FullGraphicCount - 1)) << 3)) << 1) &
                  0xffff;
        snes->mem->VRAM[address] = Byte;
    } else
        snes->mem->VRAM[address = (VMA.Address << 1) & 0xffff] = Byte;
    IPPU.TileCached[TILE_2BIT][address >> 4]                                     = FALSE;
    IPPU.TileCached[TILE_4BIT][address >> 5]                                     = FALSE;
    IPPU.TileCached[TILE_8BIT][address >> 6]                                     = FALSE;
    IPPU.TileCached[TILE_2BIT_EVEN][address >> 4]                                = FALSE;
    IPPU.TileCached[TILE_2BIT_EVEN][((address >> 4) - 1) & (MAX_2BIT_TILES - 1)] = FALSE;
    IPPU.TileCached[TILE_2BIT_ODD][address >> 4]                                 = FALSE;
    IPPU.TileCached[TILE_2BIT_ODD][((address >> 4) - 1) & (MAX_2BIT_TILES - 1)]  = FALSE;
    IPPU.TileCached[TILE_4BIT_EVEN][address >> 5]                                = FALSE;
    IPPU.TileCached[TILE_4BIT_EVEN][((address >> 5) - 1) & (MAX_4BIT_TILES - 1)] = FALSE;
    IPPU.TileCached[TILE_4BIT_ODD][address >> 5]                                 = FALSE;
    IPPU.TileCached[TILE_4BIT_ODD][((address >> 5) - 1) & (MAX_4BIT_TILES - 1)]  = FALSE;
    if (!VMA.High) {
        VMA.Address += VMA.Increment;
    }
}
void SPPU::REGISTER_2118_tile(uint8 Byte)
{
    CHECK_INBLANK();
    uint32 rem = VMA.Address & VMA.Mask1;
    uint32 address =
        (((VMA.Address & ~VMA.Mask1) + (rem >> VMA.Shift) + ((rem & (VMA.FullGraphicCount - 1)) << 3)) << 1) & 0xffff;
    snes->mem->VRAM[address]                                                     = Byte;
    IPPU.TileCached[TILE_2BIT][address >> 4]                                     = FALSE;
    IPPU.TileCached[TILE_4BIT][address >> 5]                                     = FALSE;
    IPPU.TileCached[TILE_8BIT][address >> 6]                                     = FALSE;
    IPPU.TileCached[TILE_2BIT_EVEN][address >> 4]                                = FALSE;
    IPPU.TileCached[TILE_2BIT_EVEN][((address >> 4) - 1) & (MAX_2BIT_TILES - 1)] = FALSE;
    IPPU.TileCached[TILE_2BIT_ODD][address >> 4]                                 = FALSE;
    IPPU.TileCached[TILE_2BIT_ODD][((address >> 4) - 1) & (MAX_2BIT_TILES - 1)]  = FALSE;
    IPPU.TileCached[TILE_4BIT_EVEN][address >> 5]                                = FALSE;
    IPPU.TileCached[TILE_4BIT_EVEN][((address >> 5) - 1) & (MAX_4BIT_TILES - 1)] = FALSE;
    IPPU.TileCached[TILE_4BIT_ODD][address >> 5]                                 = FALSE;
    IPPU.TileCached[TILE_4BIT_ODD][((address >> 5) - 1) & (MAX_4BIT_TILES - 1)]  = FALSE;
    if (!VMA.High)
        VMA.Address += VMA.Increment;
}
void SPPU::REGISTER_2118_linear(uint8 Byte)
{
    CHECK_INBLANK();
    uint32 address;
    snes->mem->VRAM[address = (VMA.Address << 1) & 0xffff]                       = Byte;
    IPPU.TileCached[TILE_2BIT][address >> 4]                                     = FALSE;
    IPPU.TileCached[TILE_4BIT][address >> 5]                                     = FALSE;
    IPPU.TileCached[TILE_8BIT][address >> 6]                                     = FALSE;
    IPPU.TileCached[TILE_2BIT_EVEN][address >> 4]                                = FALSE;
    IPPU.TileCached[TILE_2BIT_EVEN][((address >> 4) - 1) & (MAX_2BIT_TILES - 1)] = FALSE;
    IPPU.TileCached[TILE_2BIT_ODD][address >> 4]                                 = FALSE;
    IPPU.TileCached[TILE_2BIT_ODD][((address >> 4) - 1) & (MAX_2BIT_TILES - 1)]  = FALSE;
    IPPU.TileCached[TILE_4BIT_EVEN][address >> 5]                                = FALSE;
    IPPU.TileCached[TILE_4BIT_EVEN][((address >> 5) - 1) & (MAX_4BIT_TILES - 1)] = FALSE;
    IPPU.TileCached[TILE_4BIT_ODD][address >> 5]                                 = FALSE;
    IPPU.TileCached[TILE_4BIT_ODD][((address >> 5) - 1) & (MAX_4BIT_TILES - 1)]  = FALSE;
    if (!VMA.High)
        VMA.Address += VMA.Increment;
}
void SPPU::REGISTER_2119(uint8 Byte)
{
    CHECK_INBLANK();
    uint32 address;
    if (VMA.FullGraphicCount) {
        uint32 rem = VMA.Address & VMA.Mask1;
        address =
            ((((VMA.Address & ~VMA.Mask1) + (rem >> VMA.Shift) + ((rem & (VMA.FullGraphicCount - 1)) << 3)) << 1) + 1) &
            0xffff;
        snes->mem->VRAM[address] = Byte;
    } else
        snes->mem->VRAM[address = ((VMA.Address << 1) + 1) & 0xffff] = Byte;
    IPPU.TileCached[TILE_2BIT][address >> 4]                                     = FALSE;
    IPPU.TileCached[TILE_4BIT][address >> 5]                                     = FALSE;
    IPPU.TileCached[TILE_8BIT][address >> 6]                                     = FALSE;
    IPPU.TileCached[TILE_2BIT_EVEN][address >> 4]                                = FALSE;
    IPPU.TileCached[TILE_2BIT_EVEN][((address >> 4) - 1) & (MAX_2BIT_TILES - 1)] = FALSE;
    IPPU.TileCached[TILE_2BIT_ODD][address >> 4]                                 = FALSE;
    IPPU.TileCached[TILE_2BIT_ODD][((address >> 4) - 1) & (MAX_2BIT_TILES - 1)]  = FALSE;
    IPPU.TileCached[TILE_4BIT_EVEN][address >> 5]                                = FALSE;
    IPPU.TileCached[TILE_4BIT_EVEN][((address >> 5) - 1) & (MAX_4BIT_TILES - 1)] = FALSE;
    IPPU.TileCached[TILE_4BIT_ODD][address >> 5]                                 = FALSE;
    IPPU.TileCached[TILE_4BIT_ODD][((address >> 5) - 1) & (MAX_4BIT_TILES - 1)]  = FALSE;
    if (VMA.High) {
        VMA.Address += VMA.Increment;
    }
}
void SPPU::REGISTER_2119_tile(uint8 Byte)
{
    CHECK_INBLANK();
    uint32 rem = VMA.Address & VMA.Mask1;
    uint32 address =
        ((((VMA.Address & ~VMA.Mask1) + (rem >> VMA.Shift) + ((rem & (VMA.FullGraphicCount - 1)) << 3)) << 1) + 1) &
        0xffff;
    snes->mem->VRAM[address]                                                     = Byte;
    IPPU.TileCached[TILE_2BIT][address >> 4]                                     = FALSE;
    IPPU.TileCached[TILE_4BIT][address >> 5]                                     = FALSE;
    IPPU.TileCached[TILE_8BIT][address >> 6]                                     = FALSE;
    IPPU.TileCached[TILE_2BIT_EVEN][address >> 4]                                = FALSE;
    IPPU.TileCached[TILE_2BIT_EVEN][((address >> 4) - 1) & (MAX_2BIT_TILES - 1)] = FALSE;
    IPPU.TileCached[TILE_2BIT_ODD][address >> 4]                                 = FALSE;
    IPPU.TileCached[TILE_2BIT_ODD][((address >> 4) - 1) & (MAX_2BIT_TILES - 1)]  = FALSE;
    IPPU.TileCached[TILE_4BIT_EVEN][address >> 5]                                = FALSE;
    IPPU.TileCached[TILE_4BIT_EVEN][((address >> 5) - 1) & (MAX_4BIT_TILES - 1)] = FALSE;
    IPPU.TileCached[TILE_4BIT_ODD][address >> 5]                                 = FALSE;
    IPPU.TileCached[TILE_4BIT_ODD][((address >> 5) - 1) & (MAX_4BIT_TILES - 1)]  = FALSE;
    if (VMA.High)
        VMA.Address += VMA.Increment;
}
void SPPU::REGISTER_2119_linear(uint8 Byte)
{
    CHECK_INBLANK();
    uint32 address;
    snes->mem->VRAM[address = ((VMA.Address << 1) + 1) & 0xffff]                 = Byte;
    IPPU.TileCached[TILE_2BIT][address >> 4]                                     = FALSE;
    IPPU.TileCached[TILE_4BIT][address >> 5]                                     = FALSE;
    IPPU.TileCached[TILE_8BIT][address >> 6]                                     = FALSE;
    IPPU.TileCached[TILE_2BIT_EVEN][address >> 4]                                = FALSE;
    IPPU.TileCached[TILE_2BIT_EVEN][((address >> 4) - 1) & (MAX_2BIT_TILES - 1)] = FALSE;
    IPPU.TileCached[TILE_2BIT_ODD][address >> 4]                                 = FALSE;
    IPPU.TileCached[TILE_2BIT_ODD][((address >> 4) - 1) & (MAX_2BIT_TILES - 1)]  = FALSE;
    IPPU.TileCached[TILE_4BIT_EVEN][address >> 5]                                = FALSE;
    IPPU.TileCached[TILE_4BIT_EVEN][((address >> 5) - 1) & (MAX_4BIT_TILES - 1)] = FALSE;
    IPPU.TileCached[TILE_4BIT_ODD][address >> 5]                                 = FALSE;
    IPPU.TileCached[TILE_4BIT_ODD][((address >> 5) - 1) & (MAX_4BIT_TILES - 1)]  = FALSE;
    if (VMA.High)
        VMA.Address += VMA.Increment;
}
void SPPU::REGISTER_2122(uint8 Byte)
{
    if (CGFLIP) {
        if ((Byte & 0x7f) != (CGDATA[snes->ppu->CGADD] >> 8) ||
            CGSavedByte != (uint8)(CGDATA[snes->ppu->CGADD] & 0xff)) {
            FLUSH_REDRAW();
            CGDATA[snes->ppu->CGADD]            = (Byte & 0x7f) << 8 | CGSavedByte;
            IPPU.ColorsChanged                  = TRUE;
            IPPU.Red[snes->ppu->CGADD]          = IPPU.XB[snes->ppu->CGSavedByte & 0x1f];
            IPPU.Blue[snes->ppu->CGADD]         = IPPU.XB[(Byte >> 2) & 0x1f];
            IPPU.Green[snes->ppu->CGADD]        = IPPU.XB[(CGDATA[snes->ppu->CGADD] >> 5) & 0x1f];
            IPPU.ScreenColors[snes->ppu->CGADD] = (uint16)BUILD_PIXEL(
                IPPU.Red[snes->ppu->CGADD], IPPU.Green[snes->ppu->CGADD], IPPU.Blue[snes->ppu->CGADD]);
        }
        CGADD++;
    } else {
        CGSavedByte = Byte;
    }
    CGFLIP ^= 1;
}
void SPPU::REGISTER_2180(uint8 Byte)
{
    snes->mem->RAM[snes->ppu->WRAM++] = Byte;
    WRAM &= 0x1ffff;
}
uint8 SPPU::REGISTER_4212(void)
{
    uint8 byte = 0;
    if ((snes->scpu->V_Counter >= ScreenHeight + FIRST_VISIBLE_LINE) &&
        (snes->scpu->V_Counter < ScreenHeight + FIRST_VISIBLE_LINE + 3))
        byte = 1;
    if ((snes->scpu->Cycles < Timings.HBlankEnd) || (snes->scpu->Cycles >= Timings.HBlankStart))
        byte |= 0x40;
    if (snes->scpu->V_Counter >= ScreenHeight + FIRST_VISIBLE_LINE)
        byte |= 0x80;
    return (byte);
}
