
#ifndef _GFX_H_
#define _GFX_H_
#include "utils/port.h"
#include "snes.h"

#define H_FLIP     0x4000
#define V_FLIP     0x8000
#define BLANK_TILE 2


struct SGFX
{
    typedef void (*Callback)(void *);
    const uint32 Pitch      = sizeof(uint16) * MAX_SNES_WIDTH;
    const uint32 RealPPL    = MAX_SNES_WIDTH;
    const uint32 ScreenSize = MAX_SNES_WIDTH * MAX_SNES_HEIGHT;
    uint16       ScreenBuffer[MAX_SNES_WIDTH * (MAX_SNES_HEIGHT + 64)];

    uint16 *Screen;
    uint16 *SubScreen;
    uint8  *ZBuffer;
    uint8  *SubZBuffer;
    uint16 *S;
    uint8  *DB;
    uint16 *ZERO;
    uint32  PPL;
    uint32  LinesPerTile;
    uint16 *ScreenColors;
    uint16 *RealScreenColors;
    uint8   Z1;
    uint8   Z2;
    uint32  FixedColour;
    uint8   DoInterlace;
    uint8   InterlaceFrame;
    uint32  StartY;
    uint32  EndY;
    bool8   ClipColors;

    uint8 OBJWidths[128];
    uint8 OBJVisibleTiles[128];

    struct ClipData *Clip;
    struct
    {
        uint8 RTOFlags;
        int16 Tiles;
        struct
        {
            int8  Sprite;
            uint8 Line;
        } OBJ[128];
    } OBJLines[SNES_HEIGHT_EXTENDED];


    void (*DrawBackdropMath)(uint32, uint32, uint32);
    void (*DrawBackdropNomath)(uint32, uint32, uint32);
    void (*DrawTileMath)(uint32, uint32, uint32, uint32);
    void (*DrawTileNomath)(uint32, uint32, uint32, uint32);
    void (*DrawClippedTileMath)(uint32, uint32, uint32, uint32, uint32, uint32);
    void (*DrawClippedTileNomath)(uint32, uint32, uint32, uint32, uint32, uint32);
    void (*DrawMosaicPixelMath)(uint32, uint32, uint32, uint32, uint32, uint32);
    void (*DrawMosaicPixelNomath)(uint32, uint32, uint32, uint32, uint32, uint32);
    void (*DrawMode7BG1Math)(uint32, uint32, int);
    void (*DrawMode7BG1Nomath)(uint32, uint32, int);
    void (*DrawMode7BG2Math)(uint32, uint32, int);
    void (*DrawMode7BG2Nomath)(uint32, uint32, int);

    const char *InfoString;
    uint32      InfoStringTimeout;
    char        FrameDisplayString[256];
    Callback    EndScreenRefreshCallback;
    void       *EndScreenRefreshCallbackData;
};
struct SBG
{
    uint8 (*ConvertTile)(uint8 *, uint32, uint32);
    uint8 (*ConvertTileFlip)(uint8 *, uint32, uint32);
    uint32 TileSizeH;
    uint32 TileSizeV;
    uint32 OffsetSizeH;
    uint32 OffsetSizeV;
    uint32 TileShift;
    uint32 TileAddress;
    uint32 NameSelect;
    uint32 SCBase;
    uint32 StartPalette;
    uint32 PaletteShift;
    uint32 PaletteMask;
    uint8  EnableMath;
    uint8  InterlaceLine;
    uint8 *Buffer;
    uint8 *BufferFlip;
    uint8 *Buffered;
    uint8 *BufferedFlip;
    bool8  DirectColourMode;
};
struct SLineData
{
    struct
    {
        uint16 VOffset;
        uint16 HOffset;
    } BG[4];
};
struct SLineMatrixData
{
    short MatrixA;
    short MatrixB;
    short MatrixC;
    short MatrixD;
    short CentreX;
    short CentreY;
    short M7HOFS;
    short M7VOFS;
};

extern struct SBG  BG;
extern struct SGFX GFX;


class Renderer {
  public:
    SNESX *snes = nullptr;

    int font_width  = 8;
    int font_height = 9;

  public:
    uint8                  brightness_cap[64];
    uint16                 BlackColourMap[256];
    uint16                 DirectColourMaps[8][256];
    struct SLineData       LineData[240];
    struct SLineMatrixData LineMatrixData[240];
    uint8                  region_map[6][6] = {{0, 0x01, 0x03, 0x07, 0x0f, 0x1f},
                                               {0, 0, 0x02, 0x06, 0x0e, 0x1e},
                                               {0, 0, 0, 0x04, 0x0c, 0x1c},
                                               {0, 0, 0, 0, 0x08, 0x18},
                                               {0, 0, 0, 0, 0, 0x10}};

  public:
    Renderer(SNESX *_snes);

    bool8  S9xGraphicsInit(void);
    void   S9xGraphicsDeinit(void);
    void   S9xGraphicsScreenResize(void);
    void   S9xBuildDirectColourMaps(void);
    void   S9xStartScreenRefresh(void);
    void   S9xEndScreenRefresh(void);
    void   S9xSetEndScreenRefreshCallback(const SGFX::Callback cb, void *const data);
    void   RenderLine(uint8 C);
    void   RenderScreen(bool8 sub);
    void   S9xUpdateScreen(void);
    void   SetupOBJ(void);
    void   DrawOBJS(int D);
    void   DrawBackground(int bg, uint8 Zh, uint8 Zl);
    void   DrawBackgroundMosaic(int bg, uint8 Zh, uint8 Zl);
    void   DrawBackgroundOffset(int bg, uint8 Zh, uint8 Zl, int VOffOff);
    void   DrawBackgroundOffsetMosaic(int bg, uint8 Zh, uint8 Zl, int VOffOff);
    void   DrawBackgroundMode7(int bg, void (*DrawMath)(uint32, uint32, int), void (*DrawNomath)(uint32, uint32, int),
                               int D);
    void   DrawBackdrop(void);
    void   S9xReRefresh(void);
    void   S9xSetInfoString(const char *string);
    void   S9xDisplayChar(uint16 *s, uint8 c);
    void   DisplayStringFromBottom(const char *string, int linesFromBottom, int pixelsFromLeft, bool allowWrap);
    void   DisplayTime(void);
    void   DisplayFrameRate(void);
    void   DisplayPressedKeys(void);
    void   DisplayWatchedAddresses(void);
    void   S9xDisplayMessages(uint16 *screen, int ppl, int width, int height, int scale);
    uint16 get_crosshair_color(uint8 color);
    void   S9xDrawCrosshair(const char *crosshair, uint8 fgcolor, uint8 bgcolor, int16 x, int16 y);
    uint8  CalcWindowMask(int i, uint8 W1, uint8 W2);
    void   StoreWindowRegions(uint8 Mask, struct ClipData *Clip, int n_regions, int16 *windows, uint8 *drawing_modes,
                              bool8 sub, bool8 StoreMode0 = FALSE);
    void   S9xComputeClipWindows(void);
};
#endif
