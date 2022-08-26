#include "ppu.h"
#include "tile.h"
#include "renderer.h"

#define TILE_PLUS(t, x) (((t)&0xfc00) | ((t + x) & 0x3ff))
#define DO_BG(n, pal, depth, hires, offset, Zh, Zl, voffoff)                                                           \
    if (BGActive & (1 << n)) {                                                                                         \
        BG.StartPalette = pal;                                                                                         \
        BG.EnableMath   = !sub && (snes->mem->FillRAM[0x2131] & (1 << n));                                             \
        BG.TileSizeH    = (!hires && snes->ppu->BG[n].BGSize) ? 16 : 8;                                                \
        BG.TileSizeV    = (snes->ppu->BG[n].BGSize) ? 16 : 8;                                                          \
        S9xSelectTileConverter(depth, hires, sub, snes->ppu->BGMosaic[n]);                                             \
        if (offset) {                                                                                                  \
            BG.OffsetSizeH = (!hires && snes->ppu->BG[2].BGSize) ? 16 : 8;                                             \
            BG.OffsetSizeV = (snes->ppu->BG[2].BGSize) ? 16 : 8;                                                       \
            if (snes->ppu->BGMosaic[n] && (hires || snes->ppu->Mosaic > 1))                                            \
                DrawBackgroundOffsetMosaic(n, D + Zh, D + Zl, voffoff);                                                \
            else                                                                                                       \
                DrawBackgroundOffset(n, D + Zh, D + Zl, voffoff);                                                      \
        } else {                                                                                                       \
            if (snes->ppu->BGMosaic[n] && (hires || snes->ppu->Mosaic > 1))                                            \
                DrawBackgroundMosaic(n, D + Zh, D + Zl);                                                               \
            else                                                                                                       \
                DrawBackground(n, D + Zh, D + Zl);                                                                     \
        }                                                                                                              \
    }
//
//


void S9xInitTileRenderer(void);
void S9xSelectTileRenderers(int, bool8, bool8);
void S9xSelectTileConverter(int, bool8, bool8, bool8);

struct SGFX GFX;
struct SBG  BG;

Renderer::Renderer(SNESX *_snes)
{
    snes = _snes;
}
bool8 Renderer::S9xGraphicsInit(void)
{
    S9xInitTileRenderer();
    memset(BlackColourMap, 0, 256 * sizeof(uint16));
    snes->ppu->IPPU.OBJChanged = TRUE;
    Settings.BG_Forced         = 0;
    snes->ppu->S9xFixColourBrightness();
    S9xBuildDirectColourMaps();
    GFX.Screen     = &GFX.ScreenBuffer[GFX.RealPPL * 32];
    GFX.ZERO       = (uint16 *)malloc(sizeof(uint16) * 0x10000);
    GFX.SubScreen  = (uint16 *)malloc(GFX.ScreenSize * sizeof(uint16));
    GFX.ZBuffer    = (uint8 *)malloc(GFX.ScreenSize);
    GFX.SubZBuffer = (uint8 *)malloc(GFX.ScreenSize);
    if (!GFX.ZERO || !GFX.SubScreen || !GFX.ZBuffer || !GFX.SubZBuffer) {
        S9xGraphicsDeinit();
        return (FALSE);
    }
    memset(GFX.ZERO, 0, 0x10000 * sizeof(uint16));
    for (uint32 r = 0; r <= MAX_RED; r++) {
        uint32 r2 = r;
        if (r2 & 0x10)
            r2 &= ~0x10;
        else
            r2 = 0;
        for (uint32 g = 0; g <= MAX_GREEN; g++) {
            uint32 g2 = g;
            if (g2 & GREEN_HI_BIT)
                g2 &= ~GREEN_HI_BIT;
            else
                g2 = 0;
            for (uint32 b = 0; b <= MAX_BLUE; b++) {
                uint32 b2 = b;
                if (b2 & 0x10)
                    b2 &= ~0x10;
                else
                    b2 = 0;
                GFX.ZERO[BUILD_PIXEL2(r, g, b)]                    = BUILD_PIXEL2(r2, g2, b2);
                GFX.ZERO[BUILD_PIXEL2(r, g, b) & ~ALPHA_BITS_MASK] = BUILD_PIXEL2(r2, g2, b2);
            }
        }
    }
    GFX.EndScreenRefreshCallback     = NULL;
    GFX.EndScreenRefreshCallbackData = NULL;
    return (TRUE);
}
void Renderer::S9xGraphicsDeinit(void)
{
    if (GFX.ZERO) {
        free(GFX.ZERO);
        GFX.ZERO = NULL;
    }
    if (GFX.SubScreen) {
        free(GFX.SubScreen);
        GFX.SubScreen = NULL;
    }
    if (GFX.ZBuffer) {
        free(GFX.ZBuffer);
        GFX.ZBuffer = NULL;
    }
    if (GFX.SubZBuffer) {
        free(GFX.SubZBuffer);
        GFX.SubZBuffer = NULL;
    }
    GFX.EndScreenRefreshCallback     = NULL;
    GFX.EndScreenRefreshCallbackData = NULL;
}
void Renderer::S9xGraphicsScreenResize(void)
{
    snes->ppu->IPPU.MaxBrightness = snes->ppu->Brightness;
    snes->ppu->IPPU.Interlace     = snes->mem->FillRAM[0x2133] & 1;
    snes->ppu->IPPU.InterlaceOBJ  = snes->mem->FillRAM[0x2133] & 2;
    snes->ppu->IPPU.PseudoHires   = snes->mem->FillRAM[0x2133] & 8;
    if (snes->ppu->BGMode == 5 || snes->ppu->BGMode == 6 || snes->ppu->IPPU.PseudoHires) {
        snes->ppu->IPPU.DoubleWidthPixels   = TRUE;
        snes->ppu->IPPU.RenderedScreenWidth = SNES_WIDTH << 1;
    } else {
        snes->ppu->IPPU.DoubleWidthPixels   = FALSE;
        snes->ppu->IPPU.RenderedScreenWidth = SNES_WIDTH;
    }
    if (snes->ppu->IPPU.Interlace) {
        GFX.PPL                              = GFX.RealPPL << 1;
        snes->ppu->IPPU.DoubleHeightPixels   = TRUE;
        snes->ppu->IPPU.RenderedScreenHeight = snes->ppu->ScreenHeight << 1;
        GFX.DoInterlace++;
    } else {
        GFX.PPL                              = GFX.RealPPL;
        snes->ppu->IPPU.DoubleHeightPixels   = FALSE;
        snes->ppu->IPPU.RenderedScreenHeight = snes->ppu->ScreenHeight;
    }
}
void Renderer::S9xBuildDirectColourMaps(void)
{
    snes->ppu->IPPU.XB = snes->ppu->mul_brightness[snes->ppu->Brightness];
    for (uint32 p = 0; p < 8; p++)
        for (uint32 c = 0; c < 256; c++)
            DirectColourMaps[p][c] = BUILD_PIXEL(snes->ppu->IPPU.XB[((c & 7) << 2) | ((p & 1) << 1)],
                                                 snes->ppu->IPPU.XB[((c & 0x38) >> 1) | (p & 2)],
                                                 snes->ppu->IPPU.XB[((c & 0xc0) >> 3) | (p & 4)]);
}
void Renderer::S9xStartScreenRefresh(void)
{
    GFX.InterlaceFrame = !GFX.InterlaceFrame;
    if (GFX.DoInterlace)
        GFX.DoInterlace--;
    if (snes->ppu->IPPU.RenderThisFrame) {
        if (!GFX.DoInterlace || !GFX.InterlaceFrame) {
            if (!snes->S9xInitUpdate()) {
                snes->ppu->IPPU.RenderThisFrame = FALSE;
                return;
            }
            S9xGraphicsScreenResize();
            snes->ppu->IPPU.RenderedFramesCount++;
        }
        snes->ppu->MosaicStart          = 0;
        snes->ppu->RecomputeClipWindows = TRUE;
        snes->ppu->IPPU.PreviousLine = snes->ppu->IPPU.CurrentLine = 0;
        memset(GFX.ZBuffer, 0, GFX.ScreenSize);
        memset(GFX.SubZBuffer, 0, GFX.ScreenSize);
    }
    if (++snes->ppu->IPPU.FrameCount == (uint32)snes->mem->ROMFramesPerSecond) {
        snes->ppu->IPPU.DisplayedRenderedFrameCount = snes->ppu->IPPU.RenderedFramesCount;
        snes->ppu->IPPU.RenderedFramesCount         = 0;
        snes->ppu->IPPU.FrameCount                  = 0;
    }
    if (GFX.InfoStringTimeout > 0 && --GFX.InfoStringTimeout == 0)
        GFX.InfoString = NULL;
    snes->ppu->IPPU.TotalEmulatedFrames++;
}
void Renderer::S9xEndScreenRefresh(void)
{
    if (snes->ppu->IPPU.RenderThisFrame) {
        snes->ppu->FLUSH_REDRAW();
        if (GFX.DoInterlace && GFX.InterlaceFrame == 0) {
        } else {
            if (snes->ppu->IPPU.ColorsChanged) {
                uint32 saved                  = snes->ppu->CGDATA[0];
                snes->ppu->IPPU.ColorsChanged = FALSE;
                snes->ppu->CGDATA[0]          = saved;
            }
            if (Settings.AutoDisplayMessages)
                S9xDisplayMessages(GFX.Screen, GFX.RealPPL, snes->ppu->IPPU.RenderedScreenWidth,
                                   snes->ppu->IPPU.RenderedScreenHeight, 1);
            snes->S9xDeinitUpdate(snes->ppu->IPPU.RenderedScreenWidth, snes->ppu->IPPU.RenderedScreenHeight);
        }
    }

    if (snes->scpu->SRAMModified) {
        if (!snes->scpu->AutoSaveTimer) {
            if (!(snes->scpu->AutoSaveTimer = Settings.AutoSaveDelay * snes->mem->ROMFramesPerSecond))
                snes->scpu->SRAMModified = FALSE;
        } else {
            if (!--snes->scpu->AutoSaveTimer) {
                snes->S9xAutoSaveSRAM();
                snes->scpu->SRAMModified = FALSE;
            }
        }
    }
    if (GFX.EndScreenRefreshCallback)
        GFX.EndScreenRefreshCallback(GFX.EndScreenRefreshCallbackData);
}
void Renderer::S9xSetEndScreenRefreshCallback(const SGFX::Callback cb, void *const data)
{
    GFX.EndScreenRefreshCallback     = cb;
    GFX.EndScreenRefreshCallbackData = data;
}
void Renderer::RenderLine(uint8 C)
{
    if (snes->ppu->IPPU.RenderThisFrame) {
        LineData[C].BG[0].VOffset = snes->ppu->BG[0].VOffset + 1;
        LineData[C].BG[0].HOffset = snes->ppu->BG[0].HOffset;
        LineData[C].BG[1].VOffset = snes->ppu->BG[1].VOffset + 1;
        LineData[C].BG[1].HOffset = snes->ppu->BG[1].HOffset;
        if (snes->ppu->BGMode == 7) {
            struct SLineMatrixData *p = &LineMatrixData[C];
            p->MatrixA                = snes->ppu->MatrixA;
            p->MatrixB                = snes->ppu->MatrixB;
            p->MatrixC                = snes->ppu->MatrixC;
            p->MatrixD                = snes->ppu->MatrixD;
            p->CentreX                = snes->ppu->CentreX;
            p->CentreY                = snes->ppu->CentreY;
            p->M7HOFS                 = snes->ppu->M7HOFS;
            p->M7VOFS                 = snes->ppu->M7VOFS;
        } else {
            LineData[C].BG[2].VOffset = snes->ppu->BG[2].VOffset + 1;
            LineData[C].BG[2].HOffset = snes->ppu->BG[2].HOffset;
            LineData[C].BG[3].VOffset = snes->ppu->BG[3].VOffset + 1;
            LineData[C].BG[3].HOffset = snes->ppu->BG[3].HOffset;
        }
        snes->ppu->IPPU.CurrentLine = C + 1;
    } else {
        if (snes->ppu->IPPU.OBJChanged)
            SetupOBJ();
        snes->ppu->RangeTimeOver |= GFX.OBJLines[C].RTOFlags;
    }
}
void Renderer::RenderScreen(bool8 sub)
{
    uint8 BGActive;
    int   D;
    if (!sub) {
        GFX.S = GFX.Screen;
        if (GFX.DoInterlace && GFX.InterlaceFrame)
            GFX.S += GFX.RealPPL;
        GFX.DB   = GFX.ZBuffer;
        GFX.Clip = snes->ppu->IPPU.Clip[0];
        BGActive = snes->mem->FillRAM[0x212c] & ~Settings.BG_Forced;
        D        = 32;
    } else {
        GFX.S    = GFX.SubScreen;
        GFX.DB   = GFX.SubZBuffer;
        GFX.Clip = snes->ppu->IPPU.Clip[1];
        BGActive = snes->mem->FillRAM[0x212d] & ~Settings.BG_Forced;
        D        = (snes->mem->FillRAM[0x2130] & 2) << 4;
    }
    if (BGActive & 0x10) {
        BG.TileAddress  = snes->ppu->OBJNameBase;
        BG.NameSelect   = snes->ppu->OBJNameSelect;
        BG.EnableMath   = !sub && (snes->mem->FillRAM[0x2131] & 0x10);
        BG.StartPalette = 128;
        S9xSelectTileConverter(4, FALSE, sub, FALSE);
        S9xSelectTileRenderers(snes->ppu->BGMode, sub, TRUE);
        DrawOBJS(D + 4);
    }
    BG.NameSelect = 0;
    S9xSelectTileRenderers(snes->ppu->BGMode, sub, FALSE);

    switch (snes->ppu->BGMode) {
        case 0:
            DO_BG(0, 0, 2, FALSE, FALSE, 15, 11, 0);
            DO_BG(1, 32, 2, FALSE, FALSE, 14, 10, 0);
            DO_BG(2, 64, 2, FALSE, FALSE, 7, 3, 0);
            DO_BG(3, 96, 2, FALSE, FALSE, 6, 2, 0);
            break;
        case 1:
            DO_BG(0, 0, 4, FALSE, FALSE, 15, 11, 0);
            DO_BG(1, 0, 4, FALSE, FALSE, 14, 10, 0);
            DO_BG(2, 0, 2, FALSE, FALSE, (snes->ppu->BG3Priority ? 17 : 7), 3, 0);
            break;
        case 2:
            DO_BG(0, 0, 4, FALSE, TRUE, 15, 7, 8);
            DO_BG(1, 0, 4, FALSE, TRUE, 11, 3, 8);
            break;
        case 3:
            DO_BG(0, 0, 8, FALSE, FALSE, 15, 7, 0);
            DO_BG(1, 0, 4, FALSE, FALSE, 11, 3, 0);
            break;
        case 4:
            DO_BG(0, 0, 8, FALSE, TRUE, 15, 7, 0);
            DO_BG(1, 0, 2, FALSE, TRUE, 11, 3, 0);
            break;
        case 5:
            DO_BG(0, 0, 4, TRUE, FALSE, 15, 7, 0);
            DO_BG(1, 0, 2, TRUE, FALSE, 11, 3, 0);
            break;
        case 6:
            DO_BG(0, 0, 4, TRUE, TRUE, 15, 7, 8);
            break;
        case 7:
            if (BGActive & 0x01) {
                BG.EnableMath = !sub && (snes->mem->FillRAM[0x2131] & 1);
                DrawBackgroundMode7(0, GFX.DrawMode7BG1Math, GFX.DrawMode7BG1Nomath, D);
            }
            if ((snes->mem->FillRAM[0x2133] & 0x40) && (BGActive & 0x02)) {
                BG.EnableMath = !sub && (snes->mem->FillRAM[0x2131] & 2);
                DrawBackgroundMode7(1, GFX.DrawMode7BG2Math, GFX.DrawMode7BG2Nomath, D);
            }
            break;
    }
    BG.EnableMath = !sub && (snes->mem->FillRAM[0x2131] & 0x20);
    DrawBackdrop();
}
void Renderer::S9xUpdateScreen(void)
{
    if (snes->ppu->IPPU.OBJChanged || snes->ppu->IPPU.InterlaceOBJ)
        SetupOBJ();
    snes->ppu->RangeTimeOver |= GFX.OBJLines[GFX.EndY].RTOFlags;
    GFX.StartY = snes->ppu->IPPU.PreviousLine;
    if ((GFX.EndY = snes->ppu->IPPU.CurrentLine - 1) >= snes->ppu->ScreenHeight)
        GFX.EndY = snes->ppu->ScreenHeight - 1;
    if (!snes->ppu->ForcedBlanking) {
        if (snes->ppu->RecomputeClipWindows) {
            S9xComputeClipWindows();
            snes->ppu->RecomputeClipWindows = FALSE;
        }
        if (!snes->ppu->IPPU.DoubleWidthPixels &&
            (snes->ppu->BGMode == 5 || snes->ppu->BGMode == 6 || snes->ppu->IPPU.PseudoHires)) {
            for (uint32 y = 0; y < GFX.StartY; y++) {
                uint16 *p = GFX.Screen + y * GFX.PPL + 255;
                uint16 *q = GFX.Screen + y * GFX.PPL + 510;
                for (int x = 255; x >= 0; x--, p--, q -= 2)
                    *q = *(q + 1) = *p;
            }
            snes->ppu->IPPU.DoubleWidthPixels   = TRUE;
            snes->ppu->IPPU.RenderedScreenWidth = 512;
        }
        if (!snes->ppu->IPPU.DoubleHeightPixels && snes->ppu->IPPU.Interlace &&
            (snes->ppu->BGMode == 5 || snes->ppu->BGMode == 6)) {
            snes->ppu->IPPU.DoubleHeightPixels   = TRUE;
            snes->ppu->IPPU.RenderedScreenHeight = snes->ppu->ScreenHeight << 1;
            GFX.PPL                              = GFX.RealPPL << 1;
            GFX.DoInterlace                      = 2;
            for (int32 y = (int32)GFX.StartY - 2; y >= 0; y--)
                memmove(GFX.Screen + (y + 1) * GFX.PPL, GFX.Screen + y * GFX.RealPPL, GFX.PPL * sizeof(uint16));
        }
        if ((snes->mem->FillRAM[0x2130] & 0x30) != 0x30 && (snes->mem->FillRAM[0x2131] & 0x3f))
            GFX.FixedColour = BUILD_PIXEL(snes->ppu->IPPU.XB[snes->ppu->FixedColourRed],
                                          snes->ppu->IPPU.XB[snes->ppu->FixedColourGreen],
                                          snes->ppu->IPPU.XB[snes->ppu->FixedColourBlue]);
        if (snes->ppu->BGMode == 5 || snes->ppu->BGMode == 6 || snes->ppu->IPPU.PseudoHires ||
            ((snes->mem->FillRAM[0x2130] & 0x30) != 0x30 && (snes->mem->FillRAM[0x2130] & 2) &&
             (snes->mem->FillRAM[0x2131] & 0x3f) && (snes->mem->FillRAM[0x212d] & 0x1f)))
            RenderScreen(TRUE);
        RenderScreen(FALSE);
    } else {
        const uint16 black = BUILD_PIXEL(0, 0, 0);
        GFX.S              = GFX.Screen + GFX.StartY * GFX.PPL;
        if (GFX.DoInterlace && GFX.InterlaceFrame)
            GFX.S += GFX.RealPPL;
        for (uint32 l = GFX.StartY; l <= GFX.EndY; l++, GFX.S += GFX.PPL)
            for (int x = 0; x < snes->ppu->IPPU.RenderedScreenWidth; x++)
                GFX.S[x] = black;
    }
    snes->ppu->IPPU.PreviousLine = snes->ppu->IPPU.CurrentLine;
}
void Renderer::SetupOBJ(void)
{
    int SmallWidth, SmallHeight, LargeWidth, LargeHeight;
    switch (snes->ppu->OBJSizeSelect) {
        case 0:
            SmallWidth = SmallHeight = 8;
            LargeWidth = LargeHeight = 16;
            break;
        case 1:
            SmallWidth = SmallHeight = 8;
            LargeWidth = LargeHeight = 32;
            break;
        case 2:
            SmallWidth = SmallHeight = 8;
            LargeWidth = LargeHeight = 64;
            break;
        case 3:
            SmallWidth = SmallHeight = 16;
            LargeWidth = LargeHeight = 32;
            break;
        case 4:
            SmallWidth = SmallHeight = 16;
            LargeWidth = LargeHeight = 64;
            break;
        case 5:
        default:
            SmallWidth = SmallHeight = 32;
            LargeWidth = LargeHeight = 64;
            break;
        case 6:
            SmallWidth  = 16;
            SmallHeight = 32;
            LargeWidth  = 32;
            LargeHeight = 64;
            break;
        case 7:
            SmallWidth  = 16;
            SmallHeight = 32;
            LargeWidth = LargeHeight = 32;
            break;
    }
    int   inc       = snes->ppu->IPPU.InterlaceOBJ ? 2 : 1;
    int   startline = (snes->ppu->IPPU.InterlaceOBJ && GFX.InterlaceFrame) ? 1 : 0;
    int   Height;
    uint8 S;
    int   sprite_limit = (Settings.MaxSpriteTilesPerLine == 128) ? 128 : 32;
    if (!snes->ppu->OAMPriorityRotation || !(snes->ppu->OAMFlip & snes->ppu->OAMAddr & 1)) {
        uint8 LineOBJ[SNES_HEIGHT_EXTENDED];
        memset(LineOBJ, 0, sizeof(LineOBJ));
        for (int i = 0; i < SNES_HEIGHT_EXTENDED; i++) {
            GFX.OBJLines[i].RTOFlags = 0;
            GFX.OBJLines[i].Tiles    = Settings.MaxSpriteTilesPerLine;
            for (int j = 0; j < sprite_limit; j++)
                GFX.OBJLines[i].OBJ[j].Sprite = -1;
        }
        uint8 FirstSprite = snes->ppu->FirstSprite;
        S                 = FirstSprite;
        do {
            if (snes->ppu->OBJ[S].Size) {
                GFX.OBJWidths[S] = LargeWidth;
                Height           = LargeHeight;
            } else {
                GFX.OBJWidths[S] = SmallWidth;
                Height           = SmallHeight;
            }
            int HPos = snes->ppu->OBJ[S].HPos;
            if (HPos == -256)
                HPos = 0;
            if (HPos > -GFX.OBJWidths[S] && HPos <= 256) {
                if (HPos < 0)
                    GFX.OBJVisibleTiles[S] = (GFX.OBJWidths[S] + HPos + 7) >> 3;
                else if (HPos + GFX.OBJWidths[S] > 255)
                    GFX.OBJVisibleTiles[S] = (256 - HPos + 7) >> 3;
                else
                    GFX.OBJVisibleTiles[S] = GFX.OBJWidths[S] >> 3;
                for (uint8 line = startline, Y = (uint8)(snes->ppu->OBJ[S].VPos & 0xff); line < Height;
                     Y++, line += inc) {
                    if (Y >= SNES_HEIGHT_EXTENDED)
                        continue;
                    if (LineOBJ[Y] >= sprite_limit) {
                        GFX.OBJLines[Y].RTOFlags |= 0x40;
                        continue;
                    }
                    GFX.OBJLines[Y].Tiles -= GFX.OBJVisibleTiles[S];
                    if (GFX.OBJLines[Y].Tiles < 0)
                        GFX.OBJLines[Y].RTOFlags |= 0x80;
                    GFX.OBJLines[Y].OBJ[LineOBJ[Y]].Sprite = S;
                    if (snes->ppu->OBJ[S].VFlip)
                        GFX.OBJLines[Y].OBJ[LineOBJ[Y]].Line = line ^ (GFX.OBJWidths[S] - 1);
                    else
                        GFX.OBJLines[Y].OBJ[LineOBJ[Y]].Line = line;
                    LineOBJ[Y]++;
                }
            }
            S = (S + 1) & 0x7f;
        } while (S != FirstSprite);
        for (int Y = 1; Y < SNES_HEIGHT_EXTENDED; Y++)
            GFX.OBJLines[Y].RTOFlags |= GFX.OBJLines[Y - 1].RTOFlags;
    } else {
        uint8 OBJOnLine[SNES_HEIGHT_EXTENDED][128];
        bool8 AnyOBJOnLine[SNES_HEIGHT_EXTENDED];
        memset(AnyOBJOnLine, FALSE, sizeof(AnyOBJOnLine));
        for (S = 0; S < 128; S++) {
            if (snes->ppu->OBJ[S].Size) {
                GFX.OBJWidths[S] = LargeWidth;
                Height           = LargeHeight;
            } else {
                GFX.OBJWidths[S] = SmallWidth;
                Height           = SmallHeight;
            }
            int HPos = snes->ppu->OBJ[S].HPos;
            if (HPos == -256)
                HPos = 256;
            if (HPos > -GFX.OBJWidths[S] && HPos <= 256) {
                if (HPos < 0)
                    GFX.OBJVisibleTiles[S] = (GFX.OBJWidths[S] + HPos + 7) >> 3;
                else if (HPos + GFX.OBJWidths[S] >= 257)
                    GFX.OBJVisibleTiles[S] = (257 - HPos + 7) >> 3;
                else
                    GFX.OBJVisibleTiles[S] = GFX.OBJWidths[S] >> 3;
                for (uint8 line = startline, Y = (uint8)(snes->ppu->OBJ[S].VPos & 0xff); line < Height;
                     Y++, line += inc) {
                    if (Y >= SNES_HEIGHT_EXTENDED)
                        continue;
                    if (!AnyOBJOnLine[Y]) {
                        memset(OBJOnLine[Y], 0, sizeof(OBJOnLine[Y]));
                        AnyOBJOnLine[Y] = TRUE;
                    }
                    if (snes->ppu->OBJ[S].VFlip)
                        OBJOnLine[Y][S] = (line ^ (GFX.OBJWidths[S] - 1)) | 0x80;
                    else
                        OBJOnLine[Y][S] = line | 0x80;
                }
            }
        }
        int j;
        for (int Y = 0; Y < SNES_HEIGHT_EXTENDED; Y++) {
            GFX.OBJLines[Y].RTOFlags = Y ? GFX.OBJLines[Y - 1].RTOFlags : 0;
            GFX.OBJLines[Y].Tiles    = Settings.MaxSpriteTilesPerLine;
            uint8 FirstSprite        = (snes->ppu->FirstSprite + Y) & 0x7f;
            S                        = FirstSprite;
            j                        = 0;
            if (AnyOBJOnLine[Y]) {
                do {
                    if (OBJOnLine[Y][S]) {
                        if (j >= sprite_limit) {
                            GFX.OBJLines[Y].RTOFlags |= 0x40;
                            break;
                        }
                        GFX.OBJLines[Y].Tiles -= GFX.OBJVisibleTiles[S];
                        if (GFX.OBJLines[Y].Tiles < 0)
                            GFX.OBJLines[Y].RTOFlags |= 0x80;
                        GFX.OBJLines[Y].OBJ[j].Sprite = S;
                        GFX.OBJLines[Y].OBJ[j++].Line = OBJOnLine[Y][S] & ~0x80;
                    }
                    S = (S + 1) & 0x7f;
                } while (S != FirstSprite);
            }
            if (j < sprite_limit)
                GFX.OBJLines[Y].OBJ[j].Sprite = -1;
        }
    }
    snes->ppu->IPPU.OBJChanged = FALSE;
}
void Renderer::DrawOBJS(int D)
{
    void (*DrawTile)(uint32, uint32, uint32, uint32)                        = NULL;
    void (*DrawClippedTile)(uint32, uint32, uint32, uint32, uint32, uint32) = NULL;
    int PixWidth                                                            = snes->ppu->IPPU.DoubleWidthPixels ? 2 : 1;
    BG.InterlaceLine                                                        = GFX.InterlaceFrame ? 8 : 0;
    GFX.Z1                                                                  = 2;
    int sprite_limit = (Settings.MaxSpriteTilesPerLine == 128) ? 128 : 32;
    for (uint32 Y = GFX.StartY, Offset = Y * GFX.PPL; Y <= GFX.EndY; Y++, Offset += GFX.PPL) {
        int I     = 0;
        int tiles = GFX.OBJLines[Y].Tiles;
        for (int S = GFX.OBJLines[Y].OBJ[I].Sprite; S >= 0 && I < sprite_limit; S = GFX.OBJLines[Y].OBJ[++I].Sprite) {
            tiles += GFX.OBJVisibleTiles[S];
            if (tiles <= 0)
                continue;
            int BaseTile = (((GFX.OBJLines[Y].OBJ[I].Line << 1) + (snes->ppu->OBJ[S].Name & 0xf0)) & 0xf0) |
                           (snes->ppu->OBJ[S].Name & 0x100) | (snes->ppu->OBJ[S].Palette << 10);
            int TileX    = snes->ppu->OBJ[S].Name & 0x0f;
            int TileLine = (GFX.OBJLines[Y].OBJ[I].Line & 7) * 8;
            int TileInc  = 1;
            if (snes->ppu->OBJ[S].HFlip) {
                TileX = (TileX + (GFX.OBJWidths[S] >> 3) - 1) & 0x0f;
                BaseTile |= H_FLIP;
                TileInc = -1;
            }
            GFX.Z2       = D + snes->ppu->OBJ[S].Priority * 4;
            int DrawMode = 3;
            int clip = 0, next_clip = -1000;
            int X = snes->ppu->OBJ[S].HPos;
            if (X == -256)
                X = 256;
            for (int t = tiles, O = Offset + X * PixWidth; X <= 256 && X < snes->ppu->OBJ[S].HPos + GFX.OBJWidths[S];
                 TileX = (TileX + TileInc) & 0x0f, X += 8, O += 8 * PixWidth) {
                if (X < -7 || --t < 0 || X == 256)
                    continue;
                for (int x = X; x < X + 8;) {
                    if (x >= next_clip) {
                        for (; clip < GFX.Clip[4].Count && GFX.Clip[4].Left[clip] <= x; clip++)
                            ;
                        if (clip == 0 || x >= GFX.Clip[4].Right[clip - 1]) {
                            DrawMode  = 0;
                            next_clip = ((clip < GFX.Clip[4].Count) ? GFX.Clip[4].Left[clip] : 1000);
                        } else {
                            DrawMode       = GFX.Clip[4].DrawMode[clip - 1];
                            next_clip      = GFX.Clip[4].Right[clip - 1];
                            GFX.ClipColors = !(DrawMode & 1);
                            if (BG.EnableMath && (snes->ppu->OBJ[S].Palette & 4) && (DrawMode & 2)) {
                                DrawTile        = GFX.DrawTileMath;
                                DrawClippedTile = GFX.DrawClippedTileMath;
                            } else {
                                DrawTile        = GFX.DrawTileNomath;
                                DrawClippedTile = GFX.DrawClippedTileNomath;
                            }
                        }
                    }
                    if (x == X && x + 8 < next_clip) {
                        if (DrawMode)
                            DrawTile(BaseTile | TileX, O, TileLine, 1);
                        x += 8;
                    } else {
                        int w = (next_clip <= X + 8) ? next_clip - x : X + 8 - x;
                        if (DrawMode)
                            DrawClippedTile(BaseTile | TileX, O, x - X, w, TileLine, 1);
                        x += w;
                    }
                }
            }
        }
    }
}
void Renderer::DrawBackground(int bg, uint8 Zh, uint8 Zl)
{
    BG.TileAddress = snes->ppu->BG[bg].NameBase << 1;
    uint32  Tile;
    uint16 *SC0, *SC1, *SC2, *SC3;
    SC0 = (uint16 *)&snes->mem->VRAM[snes->ppu->BG[bg].SCBase << 1];
    SC1 = (snes->ppu->BG[bg].SCSize & 1) ? SC0 + 1024 : SC0;
    if (SC1 >= (uint16 *)(snes->mem->VRAM + 0x10000))
        SC1 -= 0x8000;
    SC2 = (snes->ppu->BG[bg].SCSize & 2) ? SC1 + 1024 : SC0;
    if (SC2 >= (uint16 *)(snes->mem->VRAM + 0x10000))
        SC2 -= 0x8000;
    SC3 = (snes->ppu->BG[bg].SCSize & 1) ? SC2 + 1024 : SC2;
    if (SC3 >= (uint16 *)(snes->mem->VRAM + 0x10000))
        SC3 -= 0x8000;
    uint32 Lines;
    int    OffsetMask     = (BG.TileSizeH == 16) ? 0x3ff : 0x1ff;
    int    OffsetShift    = (BG.TileSizeV == 16) ? 4 : 3;
    int    PixWidth       = snes->ppu->IPPU.DoubleWidthPixels ? 2 : 1;
    bool8  HiresInterlace = snes->ppu->IPPU.Interlace && snes->ppu->IPPU.DoubleWidthPixels;
    void (*DrawTile)(uint32, uint32, uint32, uint32);
    void (*DrawClippedTile)(uint32, uint32, uint32, uint32, uint32, uint32);
    for (int clip = 0; clip < GFX.Clip[bg].Count; clip++) {
        GFX.ClipColors = !(GFX.Clip[bg].DrawMode[clip] & 1);
        if (BG.EnableMath && (GFX.Clip[bg].DrawMode[clip] & 2)) {
            DrawTile        = GFX.DrawTileMath;
            DrawClippedTile = GFX.DrawClippedTileMath;
        } else {
            DrawTile        = GFX.DrawTileNomath;
            DrawClippedTile = GFX.DrawClippedTileNomath;
        }
        for (uint32 Y = GFX.StartY; Y <= GFX.EndY; Y += Lines) {
            uint32 Y2        = HiresInterlace ? Y * 2 + GFX.InterlaceFrame : Y;
            uint32 VOffset   = LineData[Y].BG[bg].VOffset + (HiresInterlace ? 1 : 0);
            uint32 HOffset   = LineData[Y].BG[bg].HOffset;
            int    VirtAlign = ((Y2 + VOffset) & 7) >> (HiresInterlace ? 1 : 0);
            for (Lines = 1; Lines < GFX.LinesPerTile - VirtAlign; Lines++) {
                if ((VOffset != LineData[Y + Lines].BG[bg].VOffset) || (HOffset != LineData[Y + Lines].BG[bg].HOffset))
                    break;
            }
            if (Y + Lines > GFX.EndY)
                Lines = GFX.EndY - Y + 1;
            VirtAlign <<= 3;
            uint32 t1, t2;
            uint32 TilemapRow = (VOffset + Y2) >> OffsetShift;
            BG.InterlaceLine  = ((VOffset + Y2) & 1) << 3;
            if ((VOffset + Y2) & 8) {
                t1 = 16;
                t2 = 0;
            } else {
                t1 = 0;
                t2 = 16;
            }
            uint16 *b1, *b2;
            if (TilemapRow & 0x20) {
                b1 = SC2;
                b2 = SC3;
            } else {
                b1 = SC0;
                b2 = SC1;
            }
            b1 += (TilemapRow & 0x1f) << 5;
            b2 += (TilemapRow & 0x1f) << 5;
            uint32  Left   = GFX.Clip[bg].Left[clip];
            uint32  Right  = GFX.Clip[bg].Right[clip];
            uint32  Offset = Left * PixWidth + Y * GFX.PPL;
            uint32  HPos   = (HOffset + Left) & OffsetMask;
            uint32  HTile  = HPos >> 3;
            uint16 *t;
            if (BG.TileSizeH == 8) {
                if (HTile > 31)
                    t = b2 + (HTile & 0x1f);
                else
                    t = b1 + HTile;
            } else {
                if (HTile > 63)
                    t = b2 + ((HTile >> 1) & 0x1f);
                else
                    t = b1 + (HTile >> 1);
            }
            uint32 Width = Right - Left;
            if (HPos & 7) {
                uint32 l = HPos & 7;
                uint32 w = 8 - l;
                if (w > Width)
                    w = Width;
                Offset -= l * PixWidth;
                Tile   = READ_WORD(t);
                GFX.Z1 = GFX.Z2 = (Tile & 0x2000) ? Zh : Zl;
                if (BG.TileSizeV == 16)
                    Tile = TILE_PLUS(Tile, ((Tile & V_FLIP) ? t2 : t1));
                if (BG.TileSizeH == 8) {
                    DrawClippedTile(Tile, Offset, l, w, VirtAlign, Lines);
                    t++;
                    if (HTile == 31)
                        t = b2;
                    else if (HTile == 63)
                        t = b1;
                } else {
                    if (!(Tile & H_FLIP))
                        DrawClippedTile(TILE_PLUS(Tile, (HTile & 1)), Offset, l, w, VirtAlign, Lines);
                    else
                        DrawClippedTile(TILE_PLUS(Tile, 1 - (HTile & 1)), Offset, l, w, VirtAlign, Lines);
                    t += HTile & 1;
                    if (HTile == 63)
                        t = b2;
                    else if (HTile == 127)
                        t = b1;
                }
                HTile++;
                Offset += 8 * PixWidth;
                Width -= w;
            }
            while (Width >= 8) {
                Tile   = READ_WORD(t);
                GFX.Z1 = GFX.Z2 = (Tile & 0x2000) ? Zh : Zl;
                if (BG.TileSizeV == 16)
                    Tile = TILE_PLUS(Tile, ((Tile & V_FLIP) ? t2 : t1));
                if (BG.TileSizeH == 8) {
                    DrawTile(Tile, Offset, VirtAlign, Lines);
                    t++;
                    if (HTile == 31)
                        t = b2;
                    else if (HTile == 63)
                        t = b1;
                } else {
                    if (!(Tile & H_FLIP))
                        DrawTile(TILE_PLUS(Tile, (HTile & 1)), Offset, VirtAlign, Lines);
                    else
                        DrawTile(TILE_PLUS(Tile, 1 - (HTile & 1)), Offset, VirtAlign, Lines);
                    t += HTile & 1;
                    if (HTile == 63)
                        t = b2;
                    else if (HTile == 127)
                        t = b1;
                }
                HTile++;
                Offset += 8 * PixWidth;
                Width -= 8;
            }
            if (Width) {
                Tile   = READ_WORD(t);
                GFX.Z1 = GFX.Z2 = (Tile & 0x2000) ? Zh : Zl;
                if (BG.TileSizeV == 16)
                    Tile = TILE_PLUS(Tile, ((Tile & V_FLIP) ? t2 : t1));
                if (BG.TileSizeH == 8)
                    DrawClippedTile(Tile, Offset, 0, Width, VirtAlign, Lines);
                else {
                    if (!(Tile & H_FLIP))
                        DrawClippedTile(TILE_PLUS(Tile, (HTile & 1)), Offset, 0, Width, VirtAlign, Lines);
                    else
                        DrawClippedTile(TILE_PLUS(Tile, 1 - (HTile & 1)), Offset, 0, Width, VirtAlign, Lines);
                }
            }
        }
    }
}
void Renderer::DrawBackgroundMosaic(int bg, uint8 Zh, uint8 Zl)
{
    BG.TileAddress = snes->ppu->BG[bg].NameBase << 1;
    uint32  Tile;
    uint16 *SC0, *SC1, *SC2, *SC3;
    SC0 = (uint16 *)&snes->mem->VRAM[snes->ppu->BG[bg].SCBase << 1];
    SC1 = (snes->ppu->BG[bg].SCSize & 1) ? SC0 + 1024 : SC0;
    if (SC1 >= (uint16 *)(snes->mem->VRAM + 0x10000))
        SC1 -= 0x8000;
    SC2 = (snes->ppu->BG[bg].SCSize & 2) ? SC1 + 1024 : SC0;
    if (SC2 >= (uint16 *)(snes->mem->VRAM + 0x10000))
        SC2 -= 0x8000;
    SC3 = (snes->ppu->BG[bg].SCSize & 1) ? SC2 + 1024 : SC2;
    if (SC3 >= (uint16 *)(snes->mem->VRAM + 0x10000))
        SC3 -= 0x8000;
    int   Lines;
    int   OffsetMask     = (BG.TileSizeH == 16) ? 0x3ff : 0x1ff;
    int   OffsetShift    = (BG.TileSizeV == 16) ? 4 : 3;
    int   PixWidth       = snes->ppu->IPPU.DoubleWidthPixels ? 2 : 1;
    bool8 HiresInterlace = snes->ppu->IPPU.Interlace && snes->ppu->IPPU.DoubleWidthPixels;
    void (*DrawPix)(uint32, uint32, uint32, uint32, uint32, uint32);
    int MosaicStart = ((uint32)GFX.StartY - snes->ppu->MosaicStart) % snes->ppu->Mosaic;
    for (int clip = 0; clip < GFX.Clip[bg].Count; clip++) {
        GFX.ClipColors = !(GFX.Clip[bg].DrawMode[clip] & 1);
        if (BG.EnableMath && (GFX.Clip[bg].DrawMode[clip] & 2))
            DrawPix = GFX.DrawMosaicPixelMath;
        else
            DrawPix = GFX.DrawMosaicPixelNomath;
        for (uint32 Y = GFX.StartY - MosaicStart; Y <= GFX.EndY; Y += snes->ppu->Mosaic) {
            uint32 Y2      = HiresInterlace ? Y * 2 : Y;
            uint32 VOffset = LineData[Y + MosaicStart].BG[bg].VOffset + (HiresInterlace ? 1 : 0);
            uint32 HOffset = LineData[Y + MosaicStart].BG[bg].HOffset;
            Lines          = snes->ppu->Mosaic - MosaicStart;
            if (Y + MosaicStart + Lines > GFX.EndY)
                Lines = GFX.EndY - Y - MosaicStart + 1;
            int    VirtAlign = (((Y2 + VOffset) & 7) >> (HiresInterlace ? 1 : 0)) << 3;
            uint32 t1, t2;
            uint32 TilemapRow = (VOffset + Y2) >> OffsetShift;
            BG.InterlaceLine  = ((VOffset + Y2) & 1) << 3;
            if ((VOffset + Y2) & 8) {
                t1 = 16;
                t2 = 0;
            } else {
                t1 = 0;
                t2 = 16;
            }
            uint16 *b1, *b2;
            if (TilemapRow & 0x20) {
                b1 = SC2;
                b2 = SC3;
            } else {
                b1 = SC0;
                b2 = SC1;
            }
            b1 += (TilemapRow & 0x1f) << 5;
            b2 += (TilemapRow & 0x1f) << 5;
            uint32  Left   = GFX.Clip[bg].Left[clip];
            uint32  Right  = GFX.Clip[bg].Right[clip];
            uint32  Offset = Left * PixWidth + (Y + MosaicStart) * GFX.PPL;
            uint32  HPos   = (HOffset + Left - (Left % snes->ppu->Mosaic)) & OffsetMask;
            uint32  HTile  = HPos >> 3;
            uint16 *t;
            if (BG.TileSizeH == 8) {
                if (HTile > 31)
                    t = b2 + (HTile & 0x1f);
                else
                    t = b1 + HTile;
            } else {
                if (HTile > 63)
                    t = b2 + ((HTile >> 1) & 0x1f);
                else
                    t = b1 + (HTile >> 1);
            }
            uint32 Width = Right - Left;
            HPos &= 7;
            while (Left < Right) {
                uint32 w = snes->ppu->Mosaic - (Left % snes->ppu->Mosaic);
                if (w > Width)
                    w = Width;
                Tile   = READ_WORD(t);
                GFX.Z1 = GFX.Z2 = (Tile & 0x2000) ? Zh : Zl;
                if (BG.TileSizeV == 16)
                    Tile = TILE_PLUS(Tile, ((Tile & V_FLIP) ? t2 : t1));
                if (BG.TileSizeH == 8)
                    DrawPix(Tile, Offset, VirtAlign, HPos & 7, w, Lines);
                else {
                    if (!(Tile & H_FLIP))
                        DrawPix(TILE_PLUS(Tile, (HTile & 1)), Offset, VirtAlign, HPos & 7, w, Lines);
                    else
                        DrawPix(TILE_PLUS(Tile, 1 - (HTile & 1)), Offset, VirtAlign, HPos & 7, w, Lines);
                }
                HPos += snes->ppu->Mosaic;
                while (HPos >= 8) {
                    HPos -= 8;
                    if (BG.TileSizeH == 8) {
                        t++;
                        if (HTile == 31)
                            t = b2;
                        else if (HTile == 63)
                            t = b1;
                    } else {
                        t += HTile & 1;
                        if (HTile == 63)
                            t = b2;
                        else if (HTile == 127)
                            t = b1;
                    }
                    HTile++;
                }
                Offset += w * PixWidth;
                Width -= w;
                Left += w;
            }
            MosaicStart = 0;
        }
    }
}
void Renderer::DrawBackgroundOffset(int bg, uint8 Zh, uint8 Zl, int VOffOff)
{
    BG.TileAddress = snes->ppu->BG[bg].NameBase << 1;
    uint32  Tile;
    uint16 *SC0, *SC1, *SC2, *SC3;
    uint16 *BPS0, *BPS1, *BPS2, *BPS3;

    BPS0 = (uint16 *)&snes->mem->VRAM[snes->ppu->BG[2].SCBase << 1];
    BPS1 = (snes->ppu->BG[2].SCSize & 1) ? BPS0 + 1024 : BPS0;
    if (BPS1 >= (uint16 *)(snes->mem->VRAM + 0x10000))
        BPS1 -= 0x8000;
    BPS2 = (snes->ppu->BG[2].SCSize & 2) ? BPS1 + 1024 : BPS0;
    if (BPS2 >= (uint16 *)(snes->mem->VRAM + 0x10000))
        BPS2 -= 0x8000;
    BPS3 = (snes->ppu->BG[2].SCSize & 1) ? BPS2 + 1024 : BPS2;
    if (BPS3 >= (uint16 *)(snes->mem->VRAM + 0x10000))
        BPS3 -= 0x8000;
    SC0 = (uint16 *)&snes->mem->VRAM[snes->ppu->BG[bg].SCBase << 1];
    SC1 = (snes->ppu->BG[bg].SCSize & 1) ? SC0 + 1024 : SC0;
    if (SC1 >= (uint16 *)(snes->mem->VRAM + 0x10000))
        SC1 -= 0x8000;
    SC2 = (snes->ppu->BG[bg].SCSize & 2) ? SC1 + 1024 : SC0;
    if (SC2 >= (uint16 *)(snes->mem->VRAM + 0x10000))
        SC2 -= 0x8000;
    SC3 = (snes->ppu->BG[bg].SCSize & 1) ? SC2 + 1024 : SC2;
    if (SC3 >= (uint16 *)(snes->mem->VRAM + 0x10000))
        SC3 -= 0x8000;
    int   OffsetMask       = (BG.TileSizeH == 16) ? 0x3ff : 0x1ff;
    int   OffsetShift      = (BG.TileSizeV == 16) ? 4 : 3;
    int   Offset2Mask      = (BG.OffsetSizeH == 16) ? 0x3ff : 0x1ff;
    int   Offset2Shift     = (BG.OffsetSizeV == 16) ? 4 : 3;
    int   OffsetEnableMask = 0x2000 << bg;
    int   PixWidth         = snes->ppu->IPPU.DoubleWidthPixels ? 2 : 1;
    bool8 HiresInterlace   = snes->ppu->IPPU.Interlace && snes->ppu->IPPU.DoubleWidthPixels;
    void (*DrawClippedTile)(uint32, uint32, uint32, uint32, uint32, uint32);
    for (int clip = 0; clip < GFX.Clip[bg].Count; clip++) {
        GFX.ClipColors = !(GFX.Clip[bg].DrawMode[clip] & 1);
        if (BG.EnableMath && (GFX.Clip[bg].DrawMode[clip] & 2)) {
            DrawClippedTile = GFX.DrawClippedTileMath;
        } else {
            DrawClippedTile = GFX.DrawClippedTileNomath;
        }
        for (uint32 Y = GFX.StartY; Y <= GFX.EndY; Y++) {
            uint32  Y2         = HiresInterlace ? Y * 2 + GFX.InterlaceFrame : Y;
            uint32  VOff       = LineData[Y].BG[2].VOffset - 1;
            uint32  HOff       = LineData[Y].BG[2].HOffset;
            uint32  HOffsetRow = VOff >> Offset2Shift;
            uint32  VOffsetRow = (VOff + VOffOff) >> Offset2Shift;
            uint16 *s, *s1, *s2;
            if (HOffsetRow & 0x20) {
                s1 = BPS2;
                s2 = BPS3;
            } else {
                s1 = BPS0;
                s2 = BPS1;
            }
            s1 += (HOffsetRow & 0x1f) << 5;
            s2 += (HOffsetRow & 0x1f) << 5;
            s                    = ((VOffsetRow & 0x20) ? BPS2 : BPS0) + ((VOffsetRow & 0x1f) << 5);
            int32  VOffsetOffset = s - s1;
            uint32 Left          = GFX.Clip[bg].Left[clip];
            uint32 Right         = GFX.Clip[bg].Right[clip];
            uint32 Offset        = Left * PixWidth + Y * GFX.PPL;
            uint32 HScroll       = LineData[Y].BG[bg].HOffset;
            bool8  left_edge     = (Left < (8 - (HScroll & 7)));
            uint32 Width         = Right - Left;
            while (Left < Right) {
                uint32 VOffset, HOffset;
                if (left_edge) {
                    VOffset   = LineData[Y].BG[bg].VOffset;
                    HOffset   = HScroll;
                    left_edge = FALSE;
                } else {
                    int HOffTile = ((HOff + Left - 1) & Offset2Mask) >> 3;
                    if (BG.OffsetSizeH == 8) {
                        if (HOffTile > 31)
                            s = s2 + (HOffTile & 0x1f);
                        else
                            s = s1 + HOffTile;
                    } else {
                        if (HOffTile > 63)
                            s = s2 + ((HOffTile >> 1) & 0x1f);
                        else
                            s = s1 + (HOffTile >> 1);
                    }
                    uint16 HCellOffset = READ_WORD(s);
                    uint16 VCellOffset;
                    if (VOffOff)
                        VCellOffset = READ_WORD(s + VOffsetOffset);
                    else {
                        if (HCellOffset & 0x8000) {
                            VCellOffset = HCellOffset;
                            HCellOffset = 0;
                        } else
                            VCellOffset = 0;
                    }
                    if (VCellOffset & OffsetEnableMask)
                        VOffset = VCellOffset + 1;
                    else
                        VOffset = LineData[Y].BG[bg].VOffset;
                    if (HCellOffset & OffsetEnableMask)
                        HOffset = (HCellOffset & ~7) | (HScroll & 7);
                    else
                        HOffset = HScroll;
                }
                if (HiresInterlace)
                    VOffset++;
                uint32 t1, t2;
                int    VirtAlign  = (((Y2 + VOffset) & 7) >> (HiresInterlace ? 1 : 0)) << 3;
                int    TilemapRow = (VOffset + Y2) >> OffsetShift;
                BG.InterlaceLine  = ((VOffset + Y2) & 1) << 3;
                if ((VOffset + Y2) & 8) {
                    t1 = 16;
                    t2 = 0;
                } else {
                    t1 = 0;
                    t2 = 16;
                }
                uint16 *b1, *b2;
                if (TilemapRow & 0x20) {
                    b1 = SC2;
                    b2 = SC3;
                } else {
                    b1 = SC0;
                    b2 = SC1;
                }
                b1 += (TilemapRow & 0x1f) << 5;
                b2 += (TilemapRow & 0x1f) << 5;
                uint32  HPos  = (HOffset + Left) & OffsetMask;
                uint32  HTile = HPos >> 3;
                uint16 *t;
                if (BG.TileSizeH == 8) {
                    if (HTile > 31)
                        t = b2 + (HTile & 0x1f);
                    else
                        t = b1 + HTile;
                } else {
                    if (HTile > 63)
                        t = b2 + ((HTile >> 1) & 0x1f);
                    else
                        t = b1 + (HTile >> 1);
                }
                uint32 l = HPos & 7;
                uint32 w = 8 - l;
                if (w > Width)
                    w = Width;
                Offset -= l * PixWidth;
                Tile   = READ_WORD(t);
                GFX.Z1 = GFX.Z2 = (Tile & 0x2000) ? Zh : Zl;
                if (BG.TileSizeV == 16)
                    Tile = TILE_PLUS(Tile, ((Tile & V_FLIP) ? t2 : t1));
                if (BG.TileSizeH == 8) {
                    DrawClippedTile(Tile, Offset, l, w, VirtAlign, 1);
                } else {
                    if (!(Tile & H_FLIP))
                        DrawClippedTile(TILE_PLUS(Tile, (HTile & 1)), Offset, l, w, VirtAlign, 1);
                    else
                        DrawClippedTile(TILE_PLUS(Tile, 1 - (HTile & 1)), Offset, l, w, VirtAlign, 1);
                }
                Left += w;
                Offset += 8 * PixWidth;
                Width -= w;
            }
        }
    }
}
void Renderer::DrawBackgroundOffsetMosaic(int bg, uint8 Zh, uint8 Zl, int VOffOff)
{
    BG.TileAddress = snes->ppu->BG[bg].NameBase << 1;
    uint32  Tile;
    uint16 *SC0, *SC1, *SC2, *SC3;
    uint16 *BPS0, *BPS1, *BPS2, *BPS3;
    BPS0 = (uint16 *)&snes->mem->VRAM[snes->ppu->BG[2].SCBase << 1];
    BPS1 = (snes->ppu->BG[2].SCSize & 1) ? BPS0 + 1024 : BPS0;
    if (BPS1 >= (uint16 *)(snes->mem->VRAM + 0x10000))
        BPS1 -= 0x8000;
    BPS2 = (snes->ppu->BG[2].SCSize & 2) ? BPS1 + 1024 : BPS0;
    if (BPS2 >= (uint16 *)(snes->mem->VRAM + 0x10000))
        BPS2 -= 0x8000;
    BPS3 = (snes->ppu->BG[2].SCSize & 1) ? BPS2 + 1024 : BPS2;
    if (BPS3 >= (uint16 *)(snes->mem->VRAM + 0x10000))
        BPS3 -= 0x8000;
    SC0 = (uint16 *)&snes->mem->VRAM[snes->ppu->BG[bg].SCBase << 1];
    SC1 = (snes->ppu->BG[bg].SCSize & 1) ? SC0 + 1024 : SC0;
    if (SC1 >= (uint16 *)(snes->mem->VRAM + 0x10000))
        SC1 -= 0x8000;
    SC2 = (snes->ppu->BG[bg].SCSize & 2) ? SC1 + 1024 : SC0;
    if (SC2 >= (uint16 *)(snes->mem->VRAM + 0x10000))
        SC2 -= 0x8000;
    SC3 = (snes->ppu->BG[bg].SCSize & 1) ? SC2 + 1024 : SC2;
    if (SC3 >= (uint16 *)(snes->mem->VRAM + 0x10000))
        SC3 -= 0x8000;
    int   Lines;
    int   OffsetMask       = (BG.TileSizeH == 16) ? 0x3ff : 0x1ff;
    int   OffsetShift      = (BG.TileSizeV == 16) ? 4 : 3;
    int   Offset2Shift     = (BG.OffsetSizeV == 16) ? 4 : 3;
    int   OffsetEnableMask = 0x2000 << bg;
    int   PixWidth         = snes->ppu->IPPU.DoubleWidthPixels ? 2 : 1;
    bool8 HiresInterlace   = snes->ppu->IPPU.Interlace && snes->ppu->IPPU.DoubleWidthPixels;
    void (*DrawPix)(uint32, uint32, uint32, uint32, uint32, uint32);
    int MosaicStart = ((uint32)GFX.StartY - snes->ppu->MosaicStart) % snes->ppu->Mosaic;
    for (int clip = 0; clip < GFX.Clip[bg].Count; clip++) {
        GFX.ClipColors = !(GFX.Clip[bg].DrawMode[clip] & 1);
        if (BG.EnableMath && (GFX.Clip[bg].DrawMode[clip] & 2))
            DrawPix = GFX.DrawMosaicPixelMath;
        else
            DrawPix = GFX.DrawMosaicPixelNomath;
        for (uint32 Y = GFX.StartY - MosaicStart; Y <= GFX.EndY; Y += snes->ppu->Mosaic) {
            uint32 Y2   = HiresInterlace ? Y * 2 : Y;
            uint32 VOff = LineData[Y + MosaicStart].BG[2].VOffset - 1;
            uint32 HOff = LineData[Y + MosaicStart].BG[2].HOffset;
            Lines       = snes->ppu->Mosaic - MosaicStart;
            if (Y + MosaicStart + Lines > GFX.EndY)
                Lines = GFX.EndY - Y - MosaicStart + 1;
            uint32  HOffsetRow = VOff >> Offset2Shift;
            uint32  VOffsetRow = (VOff + VOffOff) >> Offset2Shift;
            uint16 *s, *s1, *s2;
            if (HOffsetRow & 0x20) {
                s1 = BPS2;
                s2 = BPS3;
            } else {
                s1 = BPS0;
                s2 = BPS1;
            }
            s1 += (HOffsetRow & 0x1f) << 5;
            s2 += (HOffsetRow & 0x1f) << 5;
            s                    = ((VOffsetRow & 0x20) ? BPS2 : BPS0) + ((VOffsetRow & 0x1f) << 5);
            int32  VOffsetOffset = s - s1;
            uint32 Left          = GFX.Clip[bg].Left[clip];
            uint32 Right         = GFX.Clip[bg].Right[clip];
            uint32 Offset        = Left * PixWidth + (Y + MosaicStart) * GFX.PPL;
            uint32 HScroll       = LineData[Y + MosaicStart].BG[bg].HOffset;
            uint32 Width         = Right - Left;
            while (Left < Right) {
                uint32 VOffset, HOffset;
                if (Left < (8 - (HScroll & 7))) {
                    VOffset = LineData[Y + MosaicStart].BG[bg].VOffset;
                    HOffset = HScroll;
                } else {
                    int HOffTile = (((Left + (HScroll & 7)) - 8) + (HOff & ~7)) >> 3;
                    if (BG.OffsetSizeH == 8) {
                        if (HOffTile > 31)
                            s = s2 + (HOffTile & 0x1f);
                        else
                            s = s1 + HOffTile;
                    } else {
                        if (HOffTile > 63)
                            s = s2 + ((HOffTile >> 1) & 0x1f);
                        else
                            s = s1 + (HOffTile >> 1);
                    }
                    uint16 HCellOffset = READ_WORD(s);
                    uint16 VCellOffset;
                    if (VOffOff)
                        VCellOffset = READ_WORD(s + VOffsetOffset);
                    else {
                        if (HCellOffset & 0x8000) {
                            VCellOffset = HCellOffset;
                            HCellOffset = 0;
                        } else
                            VCellOffset = 0;
                    }
                    if (VCellOffset & OffsetEnableMask)
                        VOffset = VCellOffset + 1;
                    else
                        VOffset = LineData[Y + MosaicStart].BG[bg].VOffset;
                    if (HCellOffset & OffsetEnableMask)
                        HOffset = (HCellOffset & ~7) | (HScroll & 7);
                    else
                        HOffset = HScroll;
                }
                if (HiresInterlace)
                    VOffset++;
                uint32 t1, t2;
                int    VirtAlign  = (((Y2 + VOffset) & 7) >> (HiresInterlace ? 1 : 0)) << 3;
                int    TilemapRow = (VOffset + Y2) >> OffsetShift;
                BG.InterlaceLine  = ((VOffset + Y2) & 1) << 3;
                if ((VOffset + Y2) & 8) {
                    t1 = 16;
                    t2 = 0;
                } else {
                    t1 = 0;
                    t2 = 16;
                }
                uint16 *b1, *b2;
                if (TilemapRow & 0x20) {
                    b1 = SC2;
                    b2 = SC3;
                } else {
                    b1 = SC0;
                    b2 = SC1;
                }
                b1 += (TilemapRow & 0x1f) << 5;
                b2 += (TilemapRow & 0x1f) << 5;
                uint32  HPos  = (HOffset + Left - (Left % snes->ppu->Mosaic)) & OffsetMask;
                uint32  HTile = HPos >> 3;
                uint16 *t;
                if (BG.TileSizeH == 8) {
                    if (HTile > 31)
                        t = b2 + (HTile & 0x1f);
                    else
                        t = b1 + HTile;
                } else {
                    if (HTile > 63)
                        t = b2 + ((HTile >> 1) & 0x1f);
                    else
                        t = b1 + (HTile >> 1);
                }
                uint32 w = snes->ppu->Mosaic - (Left % snes->ppu->Mosaic);
                if (w > Width)
                    w = Width;
                Tile   = READ_WORD(t);
                GFX.Z1 = GFX.Z2 = (Tile & 0x2000) ? Zh : Zl;
                if (BG.TileSizeV == 16)
                    Tile = TILE_PLUS(Tile, ((Tile & V_FLIP) ? t2 : t1));
                if (BG.TileSizeH == 8)
                    DrawPix(Tile, Offset, VirtAlign, HPos & 7, w, Lines);
                else {
                    if (!(Tile & H_FLIP))
                        DrawPix(TILE_PLUS(Tile, (HTile & 1)), Offset, VirtAlign, HPos & 7, w, Lines);
                    else if (!(Tile & V_FLIP))
                        DrawPix(TILE_PLUS(Tile, 1 - (HTile & 1)), Offset, VirtAlign, HPos & 7, w, Lines);
                }
                Left += w;
                Offset += w * PixWidth;
                Width -= w;
            }
            MosaicStart = 0;
        }
    }
}
void Renderer::DrawBackgroundMode7(int bg, void (*DrawMath)(uint32, uint32, int),
                                   void (*DrawNomath)(uint32, uint32, int), int D)
{
    for (int clip = 0; clip < GFX.Clip[bg].Count; clip++) {
        GFX.ClipColors = !(GFX.Clip[bg].DrawMode[clip] & 1);
        if (BG.EnableMath && (GFX.Clip[bg].DrawMode[clip] & 2))
            DrawMath(GFX.Clip[bg].Left[clip], GFX.Clip[bg].Right[clip], D);
        else
            DrawNomath(GFX.Clip[bg].Left[clip], GFX.Clip[bg].Right[clip], D);
    }
}
void Renderer::DrawBackdrop(void)
{
    uint32 Offset = GFX.StartY * GFX.PPL;
    for (int clip = 0; clip < GFX.Clip[5].Count; clip++) {
        GFX.ClipColors = !(GFX.Clip[5].DrawMode[clip] & 1);
        if (BG.EnableMath && (GFX.Clip[5].DrawMode[clip] & 2))
            GFX.DrawBackdropMath(Offset, GFX.Clip[5].Left[clip], GFX.Clip[5].Right[clip]);
        else
            GFX.DrawBackdropNomath(Offset, GFX.Clip[5].Left[clip], GFX.Clip[5].Right[clip]);
    }
}
void Renderer::S9xReRefresh(void)
{
    if (Settings.Paused)
        snes->S9xDeinitUpdate(snes->ppu->IPPU.RenderedScreenWidth, snes->ppu->IPPU.RenderedScreenHeight);
}
void Renderer::S9xSetInfoString(const char *string)
{
    if (Settings.InitialInfoStringTimeout > 0) {
        GFX.InfoString        = string;
        GFX.InfoStringTimeout = Settings.InitialInfoStringTimeout;
        S9xReRefresh();
    }
}
void Renderer::S9xDisplayChar(uint16 *s, uint8 c)
{
}
void Renderer::DisplayStringFromBottom(const char *string, int linesFromBottom, int pixelsFromLeft, bool allowWrap)
{
    if (linesFromBottom <= 0)
        linesFromBottom = 1;
    uint16 *dst = GFX.Screen + (snes->ppu->IPPU.RenderedScreenHeight - font_height * linesFromBottom) * GFX.RealPPL +
                  pixelsFromLeft;
    int len        = strlen(string);
    int max_chars  = snes->ppu->IPPU.RenderedScreenWidth / (font_width - 1);
    int char_count = 0;
    for (int i = 0; i < len; i++, char_count++) {
        if (char_count >= max_chars || (uint8)string[i] < 32) {
            if (!allowWrap)
                break;
            dst += font_height * GFX.RealPPL - (font_width - 1) * max_chars;
            if (dst >= GFX.Screen + snes->ppu->IPPU.RenderedScreenHeight * GFX.RealPPL)
                break;
            char_count -= max_chars;
        }
        if ((uint8)string[i] < 32)
            continue;
        S9xDisplayChar(dst, string[i]);
        dst += font_width - 1;
    }
}
void Renderer::DisplayTime(void)
{
    char       string[10];
    time_t     rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    sprintf(string, "%02u:%02u", timeinfo->tm_hour, timeinfo->tm_min);
    S9xDisplayString(string, 0, 0, false);
}
void Renderer::DisplayFrameRate(void)
{
    char   string[10];
    uint32 lastFrameCount = 0, calcFps = 0;
    time_t lastTime = time(NULL);
    time_t currTime = time(NULL);
    if (lastTime != currTime) {
        if (lastFrameCount < snes->ppu->IPPU.TotalEmulatedFrames) {
            calcFps = (snes->ppu->IPPU.TotalEmulatedFrames - lastFrameCount) / (uint32)(currTime - lastTime);
        }
        lastTime       = currTime;
        lastFrameCount = snes->ppu->IPPU.TotalEmulatedFrames;
    }
    sprintf(string, "%u fps", calcFps);
    S9xDisplayString(string, 2, snes->ppu->IPPU.RenderedScreenWidth - (font_width - 1) * strlen(string) - 1, false);
    const int len = 5;
    sprintf(string, "%02d/%02d", (int)snes->ppu->IPPU.DisplayedRenderedFrameCount, (int)snes->mem->ROMFramesPerSecond);
    S9xDisplayString(string, 1, snes->ppu->IPPU.RenderedScreenWidth - (font_width - 1) * len - 1, false);
}
void Renderer::DisplayPressedKeys(void)
{
}
void Renderer::DisplayWatchedAddresses(void)
{
}
void Renderer::S9xDisplayMessages(uint16 *screen, int ppl, int width, int height, int scale)
{
    if (Settings.DisplayTime)
        DisplayTime();
    if (Settings.DisplayFrameRate)
        DisplayFrameRate();
    if (Settings.DisplayWatchedAddresses)
        DisplayWatchedAddresses();
    if (Settings.DisplayPressedKeys)
        DisplayPressedKeys();
    if (GFX.InfoString && *GFX.InfoString)
        S9xDisplayString(GFX.InfoString, 5, 1, true);
}
uint16 Renderer::get_crosshair_color(uint8 color)
{
    switch (color & 15) {
        case 0:
            return (BUILD_PIXEL(0, 0, 0));
        case 1:
            return (BUILD_PIXEL(0, 0, 0));
        case 2:
            return (BUILD_PIXEL(8, 8, 8));
        case 3:
            return (BUILD_PIXEL(16, 16, 16));
        case 4:
            return (BUILD_PIXEL(23, 23, 23));
        case 5:
            return (BUILD_PIXEL(31, 31, 31));
        case 6:
            return (BUILD_PIXEL(31, 0, 0));
        case 7:
            return (BUILD_PIXEL(31, 16, 0));
        case 8:
            return (BUILD_PIXEL(31, 31, 0));
        case 9:
            return (BUILD_PIXEL(0, 31, 0));
        case 10:
            return (BUILD_PIXEL(0, 31, 31));
        case 11:
            return (BUILD_PIXEL(0, 23, 31));
        case 12:
            return (BUILD_PIXEL(0, 0, 31));
        case 13:
            return (BUILD_PIXEL(23, 0, 31));
        case 14:
            return (BUILD_PIXEL(31, 0, 31));
        case 15:
            return (BUILD_PIXEL(31, 0, 16));
    }
    return (0);
}
void Renderer::S9xDrawCrosshair(const char *crosshair, uint8 fgcolor, uint8 bgcolor, int16 x, int16 y)
{
    if (!crosshair)
        return;
    int16  r, rx = 1, c, cx = 1, W = SNES_WIDTH, H = snes->ppu->ScreenHeight;
    uint16 fg, bg;
    x -= 7;
    y -= 7;
    if (snes->ppu->IPPU.DoubleWidthPixels) {
        cx = 2;
        x *= 2;
        W *= 2;
    }
    if (snes->ppu->IPPU.DoubleHeightPixels) {
        rx = 2;
        y *= 2;
        H *= 2;
    }
    fg        = get_crosshair_color(fgcolor);
    bg        = get_crosshair_color(bgcolor);
    uint16 *s = GFX.Screen + y * (int32)GFX.RealPPL + x;
    for (r = 0; r < 15 * rx; r++, s += GFX.RealPPL - 15 * cx) {
        if (y + r < 0) {
            s += 15 * cx;
            continue;
        }
        if (y + r >= H)
            break;
        for (c = 0; c < 15 * cx; c++, s++) {
            if (x + c < 0 || s < GFX.Screen)
                continue;
            if (x + c >= W) {
                s += 15 * cx - c;
                break;
            }

            uint8 p = crosshair[(r / rx) * 15 + (c / cx)];
            if (p == '#' && fgcolor)
                *s = (fgcolor & 0x10) ? TileImpl::COLOR_ADD::fn1_2(fg, *s) : fg;
            else if (p == '.' && bgcolor)
                *s = (bgcolor & 0x10) ? TileImpl::COLOR_ADD::fn1_2(*s, bg) : bg;
        }
    }
}
uint8 Renderer::CalcWindowMask(int i, uint8 W1, uint8 W2)
{
    if (!snes->ppu->ClipWindow1Enable[i]) {
        if (!snes->ppu->ClipWindow2Enable[i])
            return (0);
        else {
            if (!snes->ppu->ClipWindow2Inside[i])
                return (~W2);
            return (W2);
        }
    } else {
        if (!snes->ppu->ClipWindow2Enable[i]) {
            if (!snes->ppu->ClipWindow1Inside[i])
                return (~W1);
            return (W1);
        } else {
            if (!snes->ppu->ClipWindow1Inside[i])
                W1 = ~W1;
            if (!snes->ppu->ClipWindow2Inside[i])
                W2 = ~W2;
            switch (snes->ppu->ClipWindowOverlapLogic[i]) {
                case 0:
                    return (W1 | W2);
                case 1:
                    return (W1 & W2);
                case 2:
                    return (W1 ^ W2);
                case 3:
                    return (~(W1 ^ W2));
            }
        }
    }
    return (0);
}
void Renderer::StoreWindowRegions(uint8 Mask, struct ClipData *Clip, int n_regions, int16 *windows,
                                  uint8 *drawing_modes, bool8 sub, bool8 StoreMode0)
{
    int ct = 0;
    for (int j = 0; j < n_regions; j++) {
        int DrawMode = drawing_modes[j];
        if (sub)
            DrawMode |= 1;
        if (Mask & (1 << j))
            DrawMode = 0;
        if (!StoreMode0 && !DrawMode)
            continue;
        if (ct > 0 && Clip->Right[ct - 1] == windows[j] && Clip->DrawMode[ct - 1] == DrawMode)
            Clip->Right[ct - 1] = windows[j + 1];
        else {
            Clip->Left[ct]     = windows[j];
            Clip->Right[ct]    = windows[j + 1];
            Clip->DrawMode[ct] = DrawMode;
            ct++;
        }
    }
    Clip->Count = ct;
}
void Renderer::S9xComputeClipWindows(void)
{
    int16 windows[6]       = {0, 256, 256, 256, 256, 256};
    uint8 drawing_modes[5] = {0, 0, 0, 0, 0};
    int   n_regions        = 1;
    int   i, j;
    if (snes->ppu->Window1Left <= snes->ppu->Window1Right) {
        if (snes->ppu->Window1Left > 0) {
            windows[2] = 256;
            windows[1] = snes->ppu->Window1Left;
            n_regions  = 2;
        }
        if (snes->ppu->Window1Right < 255) {
            windows[n_regions + 1] = 256;
            windows[n_regions]     = snes->ppu->Window1Right + 1;
            n_regions++;
        }
    }
    if (snes->ppu->Window2Left <= snes->ppu->Window2Right) {
        for (i = 0; i <= n_regions; i++) {
            if (snes->ppu->Window2Left == windows[i])
                break;
            if (snes->ppu->Window2Left < windows[i]) {
                for (j = n_regions; j >= i; j--)
                    windows[j + 1] = windows[j];
                windows[i] = snes->ppu->Window2Left;
                n_regions++;
                break;
            }
        }
        for (; i <= n_regions; i++) {
            if (snes->ppu->Window2Right + 1 == windows[i])
                break;
            if (snes->ppu->Window2Right + 1 < windows[i]) {
                for (j = n_regions; j >= i; j--)
                    windows[j + 1] = windows[j];
                windows[i] = snes->ppu->Window2Right + 1;
                n_regions++;
                break;
            }
        }
    }
    uint8 W1, W2;
    if (snes->ppu->Window1Left <= snes->ppu->Window1Right) {
        for (i = 0; windows[i] != snes->ppu->Window1Left; i++)
            ;
        for (j = i; windows[j] != snes->ppu->Window1Right + 1; j++)
            ;
        W1 = region_map[i][j];
    } else
        W1 = 0;
    if (snes->ppu->Window2Left <= snes->ppu->Window2Right) {
        for (i = 0; windows[i] != snes->ppu->Window2Left; i++)
            ;
        for (j = i; windows[j] != snes->ppu->Window2Right + 1; j++)
            ;
        W2 = region_map[i][j];
    } else
        W2 = 0;
    uint8 CW_color = 0, CW_math = 0;
    uint8 CW = CalcWindowMask(5, W1, W2);
    switch (snes->mem->FillRAM[0x2130] & 0xc0) {
        case 0x00:
            CW_color = 0;
            break;
        case 0x40:
            CW_color = ~CW;
            break;
        case 0x80:
            CW_color = CW;
            break;
        case 0xc0:
            CW_color = 0xff;
            break;
    }
    switch (snes->mem->FillRAM[0x2130] & 0x30) {
        case 0x00:
            CW_math = 0;
            break;
        case 0x10:
            CW_math = ~CW;
            break;
        case 0x20:
            CW_math = CW;
            break;
        case 0x30:
            CW_math = 0xff;
            break;
    }
    for (i = 0; i < n_regions; i++) {
        if (!(CW_color & (1 << i)))
            drawing_modes[i] |= 1;
        if (!(CW_math & (1 << i)))
            drawing_modes[i] |= 2;
    }
    StoreWindowRegions(0, &snes->ppu->IPPU.Clip[0][5], n_regions, windows, drawing_modes, FALSE, TRUE);
    StoreWindowRegions(0, &snes->ppu->IPPU.Clip[1][5], n_regions, windows, drawing_modes, TRUE, TRUE);
    for (j = 0; j < 5; j++) {
        uint8 W = Settings.DisableGraphicWindows ? 0 : CalcWindowMask(j, W1, W2);
        for (int sub = 0; sub < 2; sub++) {
            if (snes->mem->FillRAM[sub + 0x212e] & (1 << j))
                StoreWindowRegions(W, &snes->ppu->IPPU.Clip[sub][j], n_regions, windows, drawing_modes, sub);
            else
                StoreWindowRegions(0, &snes->ppu->IPPU.Clip[sub][j], n_regions, windows, drawing_modes, sub);
        }
    }
}
