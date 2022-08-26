#ifndef _TILEIMPL_H_
#define _TILEIMPL_H_
#include "snes.h"
#include "ppu.h"




namespace TileImpl {
struct BPProgressive
{
    enum
    {
        Pitch = 1
    };
    static alwaysinline uint32 Get(uint32 StartLine)
    {
        return StartLine;
    }
};
struct BPInterlace
{
    enum
    {
        Pitch = 2
    };
    static alwaysinline uint32 Get(uint32 StartLine)
    {
        return StartLine * 2 + BG.InterlaceLine;
    }
};
template <class MATH, class BPSTART> struct Normal1x1Base
{
    enum
    {
        Pitch = BPSTART::Pitch
    };
    typedef BPSTART bpstart_t;
    static void     Draw(int N, int M, uint32 Offset, uint32 OffsetInLine, uint8 Pix, uint8 Z1, uint8 Z2);
};
template <class MATH> struct Normal1x1 : public Normal1x1Base<MATH, BPProgressive>
{
};
template <class MATH, class BPSTART> struct Normal2x1Base
{
    enum
    {
        Pitch = BPSTART::Pitch
    };
    typedef BPSTART bpstart_t;
    static void     Draw(int N, int M, uint32 Offset, uint32 OffsetInLine, uint8 Pix, uint8 Z1, uint8 Z2);
};
template <class MATH> struct Normal2x1 : public Normal2x1Base<MATH, BPProgressive>
{
};
template <class MATH> struct Interlace : public Normal2x1Base<MATH, BPInterlace>
{
};
template <class MATH, class BPSTART> struct HiresBase
{
    enum
    {
        Pitch = BPSTART::Pitch
    };
    typedef BPSTART bpstart_t;
    static void     Draw(int N, int M, uint32 Offset, uint32 OffsetInLine, uint8 Pix, uint8 Z1, uint8 Z2);
};
template <class MATH> struct Hires : public HiresBase<MATH, BPProgressive>
{
};
template <class MATH> struct HiresInterlace : public HiresBase<MATH, BPInterlace>
{
};
class CachedTile {
  public:
    CachedTile(uint32 tile) : Tile(tile)
    {
    }
    alwaysinline void GetCachedTile()
    {
        TileAddr = BG.TileAddress + ((Tile & 0x3ff) << BG.TileShift);
        if (Tile & 0x100)
            TileAddr += BG.NameSelect;
        TileAddr &= 0xffff;
        TileNumber = TileAddr >> BG.TileShift;
        if (Tile & H_FLIP) {
            pCache = &BG.BufferFlip[TileNumber << 6];
            if (!BG.BufferedFlip[TileNumber])
                BG.BufferedFlip[TileNumber] = BG.ConvertTileFlip(pCache, TileAddr, Tile & 0x3ff);
        } else {
            pCache = &BG.Buffer[TileNumber << 6];
            if (!BG.Buffered[TileNumber])
                BG.Buffered[TileNumber] = BG.ConvertTile(pCache, TileAddr, Tile & 0x3ff);
        }
    }
    alwaysinline bool IsBlankTile() const
    {
        return ((Tile & H_FLIP) ? BG.BufferedFlip[TileNumber] : BG.Buffered[TileNumber]) == BLANK_TILE;
    }
    alwaysinline void SelectPalette() const
    {
        if (BG.DirectColourMode) {
            GFX.RealScreenColors = g_snes->renderer->DirectColourMaps[(Tile >> 10) & 7];
        } else
            GFX.RealScreenColors =
                &g_snes->ppu->IPPU.ScreenColors[((Tile >> BG.PaletteShift) & BG.PaletteMask) + BG.StartPalette];
        GFX.ScreenColors = GFX.ClipColors ? g_snes->renderer->BlackColourMap : GFX.RealScreenColors;
    }
    alwaysinline uint8 *Ptr() const
    {
        return pCache;
    }

  private:
    uint8 *pCache;
    uint32 Tile;
    uint32 TileNumber;
    uint32 TileAddr;
};
struct NOMATH
{
    static alwaysinline uint16 Calc(uint16 Main, uint16 Sub, uint8 SD)
    {
        return Main;
    }
};
struct COLOR_ADD
{
    static alwaysinline uint16 fn(uint16 C1, uint16 C2)
    {
        const int RED_MASK   = 0x1F << RED_SHIFT_BITS;
        const int GREEN_MASK = 0x1F << GREEN_SHIFT_BITS;
        const int BLUE_MASK  = 0x1F;
        int       rb         = C1 & (RED_MASK | BLUE_MASK);
        rb += C2 & (RED_MASK | BLUE_MASK);
        int    rbcarry     = rb & ((0x20 << RED_SHIFT_BITS) | (0x20 << 0));
        int    g           = (C1 & (GREEN_MASK)) + (C2 & (GREEN_MASK));
        int    rgbsaturate = (((g & (0x20 << GREEN_SHIFT_BITS)) | rbcarry) >> 5) * 0x1f;
        uint16 retval      = (rb & (RED_MASK | BLUE_MASK)) | (g & GREEN_MASK) | rgbsaturate;
        retval |= (retval & 0x0400) >> 5;
        return retval;
    }
    static alwaysinline uint16 fn1_2(uint16 C1, uint16 C2)
    {
        return ((((C1 & RGB_REMOVE_LOW_BITS_MASK) + (C2 & RGB_REMOVE_LOW_BITS_MASK)) >> 1) +
                (C1 & C2 & RGB_LOW_BITS_MASK)) |
               ALPHA_BITS_MASK;
    }
};
struct COLOR_ADD_BRIGHTNESS
{
    static alwaysinline uint16 fn(uint16 C1, uint16 C2)
    {
        return ((g_snes->renderer->brightness_cap[(C1 >> RED_SHIFT_BITS) + (C2 >> RED_SHIFT_BITS)] << RED_SHIFT_BITS) |
                (g_snes->renderer->brightness_cap[((C1 >> GREEN_SHIFT_BITS) & 0x1f) + ((C2 >> GREEN_SHIFT_BITS) & 0x1f)]
                 << GREEN_SHIFT_BITS) |
                ((g_snes->renderer->brightness_cap[((C1 >> 6) & 0x1f) + ((C2 >> 6) & 0x1f)] & 0x10) << 1) |
                (g_snes->renderer->brightness_cap[(C1 & 0x1f) + (C2 & 0x1f)]));
    }
    static alwaysinline uint16 fn1_2(uint16 C1, uint16 C2)
    {
        return COLOR_ADD::fn1_2(C1, C2);
    }
};
struct COLOR_SUB
{
    static alwaysinline uint16 fn(uint16 C1, uint16 C2)
    {
        int    rb1         = (C1 & (THIRD_COLOR_MASK | FIRST_COLOR_MASK)) | ((0x20 << 0) | (0x20 << RED_SHIFT_BITS));
        int    rb2         = C2 & (THIRD_COLOR_MASK | FIRST_COLOR_MASK);
        int    rb          = rb1 - rb2;
        int    rbcarry     = rb & ((0x20 << RED_SHIFT_BITS) | (0x20 << 0));
        int    g           = ((C1 & (SECOND_COLOR_MASK)) | (0x20 << GREEN_SHIFT_BITS)) - (C2 & (SECOND_COLOR_MASK));
        int    rgbsaturate = (((g & (0x20 << GREEN_SHIFT_BITS)) | rbcarry) >> 5) * 0x1f;
        uint16 retval      = ((rb & (THIRD_COLOR_MASK | FIRST_COLOR_MASK)) | (g & SECOND_COLOR_MASK)) & rgbsaturate;
        retval |= (retval & 0x0400) >> 5;
        return retval;
    }
    static alwaysinline uint16 fn1_2(uint16 C1, uint16 C2)
    {
        return GFX.ZERO[((C1 | RGB_HI_BITS_MASKx2) - (C2 & RGB_REMOVE_LOW_BITS_MASK)) >> 1];
    }
};

typedef NOMATH Blend_None;
template <class Op> struct REGMATH
{
    static alwaysinline uint16 Calc(uint16 Main, uint16 Sub, uint8 SD)
    {
        return Op::fn(Main, (SD & 0x20) ? Sub : GFX.FixedColour);
    }
};
typedef REGMATH<COLOR_ADD>            Blend_Add;
typedef REGMATH<COLOR_SUB>            Blend_Sub;
typedef REGMATH<COLOR_ADD_BRIGHTNESS> Blend_AddBrightness;
template <class Op> struct MATHF1_2
{
    static alwaysinline uint16 Calc(uint16 Main, uint16 Sub, uint8 SD)
    {
        return GFX.ClipColors ? Op::fn(Main, GFX.FixedColour) : Op::fn1_2(Main, GFX.FixedColour);
    }
};
typedef MATHF1_2<COLOR_ADD> Blend_AddF1_2;
typedef MATHF1_2<COLOR_SUB> Blend_SubF1_2;
template <class Op> struct MATHS1_2
{
    static alwaysinline uint16 Calc(uint16 Main, uint16 Sub, uint8 SD)
    {
        return GFX.ClipColors ? REGMATH<Op>::Calc(Main, Sub, SD)
               : (SD & 0x20)  ? Op::fn1_2(Main, Sub)
                              : Op::fn(Main, GFX.FixedColour);
    }
};
typedef MATHS1_2<COLOR_ADD>            Blend_AddS1_2;
typedef MATHS1_2<COLOR_SUB>            Blend_SubS1_2;
typedef MATHS1_2<COLOR_ADD_BRIGHTNESS> Blend_AddS1_2Brightness;
template <template <class PIXEL_> class TILE, template <class MATH> class PIXEL> struct Renderers
{
    enum
    {
        Pitch = PIXEL<Blend_None>::Pitch
    };
    typedef typename TILE<PIXEL<Blend_None>>::call_t call_t;
    static call_t                                    Functions[9];
};
template <template <class PIXEL_> class TILE, template <class MATH> class PIXEL>
typename Renderers<TILE, PIXEL>::call_t Renderers<TILE, PIXEL>::Functions[9] = {
    TILE<PIXEL<Blend_None>>::Draw,
    TILE<PIXEL<Blend_Add>>::Draw,
    TILE<PIXEL<Blend_AddF1_2>>::Draw,
    TILE<PIXEL<Blend_AddS1_2>>::Draw,
    TILE<PIXEL<Blend_Sub>>::Draw,
    TILE<PIXEL<Blend_SubF1_2>>::Draw,
    TILE<PIXEL<Blend_SubS1_2>>::Draw,
    TILE<PIXEL<Blend_AddBrightness>>::Draw,
    TILE<PIXEL<Blend_AddS1_2Brightness>>::Draw,
};
#define OFFSET_IN_LINE   uint32 OffsetInLine = Offset % GFX.RealPPL;
#define DRAW_PIXEL(N, M) PIXEL::Draw(N, M, Offset, OffsetInLine, Pix, Z1, Z2)
#define Z1               GFX.Z1
#define Z2               GFX.Z2
template <class PIXEL> struct DrawTile16
{
    typedef void (*call_t)(uint32, uint32, uint32, uint32);
    enum
    {
        Pitch = PIXEL::Pitch
    };
    typedef typename PIXEL::bpstart_t bpstart_t;
    static void                       Draw(uint32 Tile, uint32 Offset, uint32 StartLine, uint32 LineCount)
    {
        CachedTile cache(Tile);
        int32      l;
        uint8     *bp, Pix;
        cache.GetCachedTile();
        if (cache.IsBlankTile())
            return;
        cache.SelectPalette();
        if (!(Tile & (V_FLIP | H_FLIP))) {
            bp = cache.Ptr() + bpstart_t::Get(StartLine);
            OFFSET_IN_LINE;
            for (l = LineCount; l > 0; l--, bp += 8 * Pitch, Offset += GFX.PPL) {
                for (int x = 0; x < 8; x++) {
                    Pix = bp[x];
                    DRAW_PIXEL(x, Pix);
                }
            }
        } else if (!(Tile & V_FLIP)) {
            bp = cache.Ptr() + bpstart_t::Get(StartLine);
            OFFSET_IN_LINE;
            for (l = LineCount; l > 0; l--, bp += 8 * Pitch, Offset += GFX.PPL) {
                for (int x = 0; x < 8; x++) {
                    Pix = bp[7 - x];
                    DRAW_PIXEL(x, Pix);
                }
            }
        } else if (!(Tile & H_FLIP)) {
            bp = cache.Ptr() + 56 - bpstart_t::Get(StartLine);
            OFFSET_IN_LINE;
            for (l = LineCount; l > 0; l--, bp -= 8 * Pitch, Offset += GFX.PPL) {
                for (int x = 0; x < 8; x++) {
                    Pix = bp[x];
                    DRAW_PIXEL(x, Pix);
                }
            }
        } else {
            bp = cache.Ptr() + 56 - bpstart_t::Get(StartLine);
            OFFSET_IN_LINE;
            for (l = LineCount; l > 0; l--, bp -= 8 * Pitch, Offset += GFX.PPL) {
                for (int x = 0; x < 8; x++) {
                    Pix = bp[7 - x];
                    DRAW_PIXEL(x, Pix);
                }
            }
        }
    }
};
#undef Z1
#undef Z2
#define Z1 GFX.Z1
#define Z2 GFX.Z2
template <class PIXEL> struct DrawClippedTile16
{
    typedef void (*call_t)(uint32, uint32, uint32, uint32, uint32, uint32);
    enum
    {
        Pitch = PIXEL::Pitch
    };
    typedef typename PIXEL::bpstart_t bpstart_t;
    static void Draw(uint32 Tile, uint32 Offset, uint32 StartPixel, uint32 Width, uint32 StartLine, uint32 LineCount)
    {
        CachedTile cache(Tile);
        int32      l;
        uint8     *bp, Pix, w;
        cache.GetCachedTile();
        if (cache.IsBlankTile())
            return;
        cache.SelectPalette();
        if (!(Tile & (V_FLIP | H_FLIP))) {
            bp = cache.Ptr() + bpstart_t::Get(StartLine);
            OFFSET_IN_LINE;
            for (l = LineCount; l > 0; l--, bp += 8 * Pitch, Offset += GFX.PPL) {
                w = Width;
                switch (StartPixel) {
                    case 0:
                        Pix = bp[0];
                        DRAW_PIXEL(0, Pix);
                        if (!--w)
                            break;
                    case 1:
                        Pix = bp[1];
                        DRAW_PIXEL(1, Pix);
                        if (!--w)
                            break;
                    case 2:
                        Pix = bp[2];
                        DRAW_PIXEL(2, Pix);
                        if (!--w)
                            break;
                    case 3:
                        Pix = bp[3];
                        DRAW_PIXEL(3, Pix);
                        if (!--w)
                            break;
                    case 4:
                        Pix = bp[4];
                        DRAW_PIXEL(4, Pix);
                        if (!--w)
                            break;
                    case 5:
                        Pix = bp[5];
                        DRAW_PIXEL(5, Pix);
                        if (!--w)
                            break;
                    case 6:
                        Pix = bp[6];
                        DRAW_PIXEL(6, Pix);
                        if (!--w)
                            break;
                    case 7:
                        Pix = bp[7];
                        DRAW_PIXEL(7, Pix);
                        break;
                }
            }
        } else if (!(Tile & V_FLIP)) {
            bp = cache.Ptr() + bpstart_t::Get(StartLine);
            OFFSET_IN_LINE;
            for (l = LineCount; l > 0; l--, bp += 8 * Pitch, Offset += GFX.PPL) {
                w = Width;
                switch (StartPixel) {
                    case 0:
                        Pix = bp[7];
                        DRAW_PIXEL(0, Pix);
                        if (!--w)
                            break;
                    case 1:
                        Pix = bp[6];
                        DRAW_PIXEL(1, Pix);
                        if (!--w)
                            break;
                    case 2:
                        Pix = bp[5];
                        DRAW_PIXEL(2, Pix);
                        if (!--w)
                            break;
                    case 3:
                        Pix = bp[4];
                        DRAW_PIXEL(3, Pix);
                        if (!--w)
                            break;
                    case 4:
                        Pix = bp[3];
                        DRAW_PIXEL(4, Pix);
                        if (!--w)
                            break;
                    case 5:
                        Pix = bp[2];
                        DRAW_PIXEL(5, Pix);
                        if (!--w)
                            break;
                    case 6:
                        Pix = bp[1];
                        DRAW_PIXEL(6, Pix);
                        if (!--w)
                            break;
                    case 7:
                        Pix = bp[0];
                        DRAW_PIXEL(7, Pix);
                        break;
                }
            }
        } else if (!(Tile & H_FLIP)) {
            bp = cache.Ptr() + 56 - bpstart_t::Get(StartLine);
            OFFSET_IN_LINE;
            for (l = LineCount; l > 0; l--, bp -= 8 * Pitch, Offset += GFX.PPL) {
                w = Width;
                switch (StartPixel) {
                    case 0:
                        Pix = bp[0];
                        DRAW_PIXEL(0, Pix);
                        if (!--w)
                            break;
                    case 1:
                        Pix = bp[1];
                        DRAW_PIXEL(1, Pix);
                        if (!--w)
                            break;
                    case 2:
                        Pix = bp[2];
                        DRAW_PIXEL(2, Pix);
                        if (!--w)
                            break;
                    case 3:
                        Pix = bp[3];
                        DRAW_PIXEL(3, Pix);
                        if (!--w)
                            break;
                    case 4:
                        Pix = bp[4];
                        DRAW_PIXEL(4, Pix);
                        if (!--w)
                            break;
                    case 5:
                        Pix = bp[5];
                        DRAW_PIXEL(5, Pix);
                        if (!--w)
                            break;
                    case 6:
                        Pix = bp[6];
                        DRAW_PIXEL(6, Pix);
                        if (!--w)
                            break;
                    case 7:
                        Pix = bp[7];
                        DRAW_PIXEL(7, Pix);
                        break;
                }
            }
        } else {
            bp = cache.Ptr() + 56 - bpstart_t::Get(StartLine);
            OFFSET_IN_LINE;
            for (l = LineCount; l > 0; l--, bp -= 8 * Pitch, Offset += GFX.PPL) {
                w = Width;
                switch (StartPixel) {
                    case 0:
                        Pix = bp[7];
                        DRAW_PIXEL(0, Pix);
                        if (!--w)
                            break;
                    case 1:
                        Pix = bp[6];
                        DRAW_PIXEL(1, Pix);
                        if (!--w)
                            break;
                    case 2:
                        Pix = bp[5];
                        DRAW_PIXEL(2, Pix);
                        if (!--w)
                            break;
                    case 3:
                        Pix = bp[4];
                        DRAW_PIXEL(3, Pix);
                        if (!--w)
                            break;
                    case 4:
                        Pix = bp[3];
                        DRAW_PIXEL(4, Pix);
                        if (!--w)
                            break;
                    case 5:
                        Pix = bp[2];
                        DRAW_PIXEL(5, Pix);
                        if (!--w)
                            break;
                    case 6:
                        Pix = bp[1];
                        DRAW_PIXEL(6, Pix);
                        if (!--w)
                            break;
                    case 7:
                        Pix = bp[0];
                        DRAW_PIXEL(7, Pix);
                        break;
                }
            }
        }
    }
};
#undef Z1
#undef Z2
#define Z1 GFX.Z1
#define Z2 GFX.Z2
template <class PIXEL> struct DrawMosaicPixel16
{
    typedef void (*call_t)(uint32, uint32, uint32, uint32, uint32, uint32);
    typedef typename PIXEL::bpstart_t bpstart_t;
    static void Draw(uint32 Tile, uint32 Offset, uint32 StartLine, uint32 StartPixel, uint32 Width, uint32 LineCount)
    {
        CachedTile cache(Tile);
        int32      l, w;
        uint8      Pix;
        cache.GetCachedTile();
        if (cache.IsBlankTile())
            return;
        cache.SelectPalette();
        if (Tile & H_FLIP)
            StartPixel = 7 - StartPixel;
        if (Tile & V_FLIP)
            Pix = cache.Ptr()[56 - bpstart_t::Get(StartLine) + StartPixel];
        else
            Pix = cache.Ptr()[bpstart_t::Get(StartLine) + StartPixel];
        if (Pix) {
            OFFSET_IN_LINE;
            for (l = LineCount; l > 0; l--, Offset += GFX.PPL) {
                for (w = Width - 1; w >= 0; w--)
                    DRAW_PIXEL(w, 1);
            }
        }
    }
};
#undef Z1
#undef Z2
#define Z1  1
#define Z2  1
#define Pix 0
template <class PIXEL> struct DrawBackdrop16
{
    typedef void (*call_t)(uint32 Offset, uint32 Left, uint32 Right);
    static void Draw(uint32 Offset, uint32 Left, uint32 Right)
    {
        uint32 l, x;
        GFX.RealScreenColors = g_snes->ppu->IPPU.ScreenColors;
        GFX.ScreenColors     = GFX.ClipColors ? g_snes->renderer->BlackColourMap : GFX.RealScreenColors;
        OFFSET_IN_LINE;
        for (l = GFX.StartY; l <= GFX.EndY; l++, Offset += GFX.PPL) {
            for (x = Left; x < Right; x++)
                DRAW_PIXEL(x, 1);
        }
    }
};
#undef Pix
#undef Z1
#undef Z2
#undef DRAW_PIXEL
#define CLIP_10_BIT_SIGNED(a) (((a)&0x2000) ? ((a) | ~0x3ff) : ((a)&0x3ff))
#define DRAW_PIXEL(N, M)      PIXEL::Draw(N, M, Offset, OffsetInLine, Pix, OP::Z1(D, b), OP::Z2(D, b))
struct DrawMode7BG1_OP
{
    enum
    {
        MASK = 0xff,
        BG   = 0
    };
    static uint8 Z1(int D, uint8 b)
    {
        return D + 7;
    }
    static uint8 Z2(int D, uint8 b)
    {
        return D + 7;
    }
    static uint8 DCMODE()
    {
        return g_snes->mem->FillRAM[0x2130] & 1;
    }
};
struct DrawMode7BG2_OP
{
    enum
    {
        MASK = 0x7f,
        BG   = 1
    };
    static uint8 Z1(int D, uint8 b)
    {
        return D + ((b & 0x80) ? 11 : 3);
    }
    static uint8 Z2(int D, uint8 b)
    {
        return D + ((b & 0x80) ? 11 : 3);
    }
    static uint8 DCMODE()
    {
        return 0;
    }
};
template <class PIXEL, class OP> struct DrawTileNormal
{
    typedef void (*call_t)(uint32 Left, uint32 Right, int D);
    static void Draw(uint32 Left, uint32 Right, int D)
    {
        uint8 *VRAM1 = g_snes->mem->VRAM + 1;
        if (OP::DCMODE()) {
            GFX.RealScreenColors = g_snes->renderer->DirectColourMaps[0];
        } else
            GFX.RealScreenColors = g_snes->ppu->IPPU.ScreenColors;
        GFX.ScreenColors = GFX.ClipColors ? g_snes->renderer->BlackColourMap : GFX.RealScreenColors;
        int                     aa, cc;
        int                     startx;
        uint32                  Offset = GFX.StartY * GFX.PPL;
        struct SLineMatrixData *l      = &g_snes->renderer->LineMatrixData[GFX.StartY];
        OFFSET_IN_LINE;
        for (uint32 Line = GFX.StartY; Line <= GFX.EndY; Line++, Offset += GFX.PPL, l++) {
            int   yy, starty;
            int32 HOffset = ((int32)l->M7HOFS << 19) >> 19;
            int32 VOffset = ((int32)l->M7VOFS << 19) >> 19;
            int32 CentreX = ((int32)l->CentreX << 19) >> 19;
            int32 CentreY = ((int32)l->CentreY << 19) >> 19;
            if (g_snes->ppu->Mode7VFlip)
                starty = 255 - (int)(Line + 1);
            else
                starty = Line + 1;
            yy     = CLIP_10_BIT_SIGNED(VOffset - CentreY);
            int BB = ((l->MatrixB * starty) & ~63) + ((l->MatrixB * yy) & ~63) + (CentreX << 8);
            int DD = ((l->MatrixD * starty) & ~63) + ((l->MatrixD * yy) & ~63) + (CentreY << 8);
            if (g_snes->ppu->Mode7HFlip) {
                startx = Right - 1;
                aa     = -l->MatrixA;
                cc     = -l->MatrixC;
            } else {
                startx = Left;
                aa     = l->MatrixA;
                cc     = l->MatrixC;
            }
            int   xx = CLIP_10_BIT_SIGNED(HOffset - CentreX);
            int   AA = l->MatrixA * startx + ((l->MatrixA * xx) & ~63);
            int   CC = l->MatrixC * startx + ((l->MatrixC * xx) & ~63);
            uint8 Pix;
            if (!g_snes->ppu->Mode7Repeat) {
                for (uint32 x = Left; x < Right; x++, AA += aa, CC += cc) {
                    int    X        = ((AA + BB) >> 8) & 0x3ff;
                    int    Y        = ((CC + DD) >> 8) & 0x3ff;
                    uint8 *TileData = VRAM1 + (g_snes->mem->VRAM[((Y & ~7) << 5) + ((X >> 2) & ~1)] << 7);
                    uint8  b        = *(TileData + ((Y & 7) << 4) + ((X & 7) << 1));
                    Pix             = b & OP::MASK;
                    DRAW_PIXEL(x, Pix);
                }
            } else {
                for (uint32 x = Left; x < Right; x++, AA += aa, CC += cc) {
                    int   X = ((AA + BB) >> 8);
                    int   Y = ((CC + DD) >> 8);
                    uint8 b;
                    if (((X | Y) & ~0x3ff) == 0) {
                        uint8 *TileData = VRAM1 + (g_snes->mem->VRAM[((Y & ~7) << 5) + ((X >> 2) & ~1)] << 7);
                        b               = *(TileData + ((Y & 7) << 4) + ((X & 7) << 1));
                    } else if (g_snes->ppu->Mode7Repeat == 3)
                        b = *(VRAM1 + ((Y & 7) << 4) + ((X & 7) << 1));
                    else
                        continue;
                    Pix = b & OP::MASK;
                    DRAW_PIXEL(x, Pix);
                }
            }
        }
    }
};
template <class PIXEL> struct DrawMode7BG1 : public DrawTileNormal<PIXEL, DrawMode7BG1_OP>
{
};
template <class PIXEL> struct DrawMode7BG2 : public DrawTileNormal<PIXEL, DrawMode7BG2_OP>
{
};
template <class PIXEL, class OP> struct DrawTileMosaic
{
    typedef void (*call_t)(uint32 Left, uint32 Right, int D);
    static void Draw(uint32 Left, uint32 Right, int D)
    {
        uint8 *VRAM1 = g_snes->mem->VRAM + 1;
        if (OP::DCMODE()) {
            GFX.RealScreenColors = g_snes->renderer->DirectColourMaps[0];
        } else
            GFX.RealScreenColors = g_snes->ppu->IPPU.ScreenColors;
        GFX.ScreenColors = GFX.ClipColors ? g_snes->renderer->BlackColourMap : GFX.RealScreenColors;
        int   aa, cc;
        int   startx, StartY = GFX.StartY;
        int   HMosaic = 1, VMosaic = 1, MosaicStart = 0;
        int32 MLeft = Left, MRight = Right;
        if (g_snes->ppu->BGMosaic[0]) {
            VMosaic     = g_snes->ppu->Mosaic;
            MosaicStart = ((uint32)GFX.StartY - g_snes->ppu->MosaicStart) % VMosaic;
            StartY -= MosaicStart;
        }
        if (g_snes->ppu->BGMosaic[OP::BG]) {
            HMosaic = g_snes->ppu->Mosaic;
            MLeft -= MLeft % HMosaic;
            MRight += HMosaic - 1;
            MRight -= MRight % HMosaic;
        }
        uint32                  Offset = StartY * GFX.PPL;
        struct SLineMatrixData *l      = &g_snes->renderer->LineMatrixData[StartY];
        OFFSET_IN_LINE;
        for (uint32 Line = StartY; Line <= GFX.EndY; Line += VMosaic, Offset += VMosaic * GFX.PPL, l += VMosaic) {
            if (Line + VMosaic > GFX.EndY)
                VMosaic = GFX.EndY - Line + 1;
            int   yy, starty;
            int32 HOffset = ((int32)l->M7HOFS << 19) >> 19;
            int32 VOffset = ((int32)l->M7VOFS << 19) >> 19;
            int32 CentreX = ((int32)l->CentreX << 19) >> 19;
            int32 CentreY = ((int32)l->CentreY << 19) >> 19;
            if (g_snes->ppu->Mode7VFlip)
                starty = 255 - (int)(Line + 1);
            else
                starty = Line + 1;
            yy     = CLIP_10_BIT_SIGNED(VOffset - CentreY);
            int BB = ((l->MatrixB * starty) & ~63) + ((l->MatrixB * yy) & ~63) + (CentreX << 8);
            int DD = ((l->MatrixD * starty) & ~63) + ((l->MatrixD * yy) & ~63) + (CentreY << 8);
            if (g_snes->ppu->Mode7HFlip) {
                startx = MRight - 1;
                aa     = -l->MatrixA;
                cc     = -l->MatrixC;
            } else {
                startx = MLeft;
                aa     = l->MatrixA;
                cc     = l->MatrixC;
            }
            int   xx = CLIP_10_BIT_SIGNED(HOffset - CentreX);
            int   AA = l->MatrixA * startx + ((l->MatrixA * xx) & ~63);
            int   CC = l->MatrixC * startx + ((l->MatrixC * xx) & ~63);
            uint8 Pix;
            uint8 ctr = 1;
            if (!g_snes->ppu->Mode7Repeat) {
                for (int32 x = MLeft; x < MRight; x++, AA += aa, CC += cc) {
                    if (--ctr)
                        continue;
                    ctr             = HMosaic;
                    int    X        = ((AA + BB) >> 8) & 0x3ff;
                    int    Y        = ((CC + DD) >> 8) & 0x3ff;
                    uint8 *TileData = VRAM1 + (g_snes->mem->VRAM[((Y & ~7) << 5) + ((X >> 2) & ~1)] << 7);
                    uint8  b        = *(TileData + ((Y & 7) << 4) + ((X & 7) << 1));
                    if ((Pix = (b & OP::MASK))) {
                        for (int32 h = MosaicStart; h < VMosaic; h++) {
                            for (int32 w = x + HMosaic - 1; w >= x; w--)
                                DRAW_PIXEL(w + h * GFX.PPL, (w >= (int32)Left && w < (int32)Right));
                        }
                    }
                }
            } else {
                for (int32 x = MLeft; x < MRight; x++, AA += aa, CC += cc) {
                    if (--ctr)
                        continue;
                    ctr     = HMosaic;
                    int   X = ((AA + BB) >> 8);
                    int   Y = ((CC + DD) >> 8);
                    uint8 b;
                    if (((X | Y) & ~0x3ff) == 0) {
                        uint8 *TileData = VRAM1 + (g_snes->mem->VRAM[((Y & ~7) << 5) + ((X >> 2) & ~1)] << 7);
                        b               = *(TileData + ((Y & 7) << 4) + ((X & 7) << 1));
                    } else if (g_snes->ppu->Mode7Repeat == 3)
                        b = *(VRAM1 + ((Y & 7) << 4) + ((X & 7) << 1));
                    else
                        continue;
                    if ((Pix = (b & OP::MASK))) {
                        for (int32 h = MosaicStart; h < VMosaic; h++) {
                            for (int32 w = x + HMosaic - 1; w >= x; w--)
                                DRAW_PIXEL(w + h * GFX.PPL, (w >= (int32)Left && w < (int32)Right));
                        }
                    }
                }
            }
            MosaicStart = 0;
        }
    }
};
template <class PIXEL> struct DrawMode7MosaicBG1 : public DrawTileMosaic<PIXEL, DrawMode7BG1_OP>
{
};
template <class PIXEL> struct DrawMode7MosaicBG2 : public DrawTileMosaic<PIXEL, DrawMode7BG2_OP>
{
};
#undef DRAW_PIXEL
}    // namespace TileImpl
#endif
