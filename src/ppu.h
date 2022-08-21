#ifndef PPU_H
#define PPU_H
#include <stdint.h>


typedef struct BgLayer
{
    uint16_t hScroll       = 0;
    uint16_t vScroll       = 0;
    bool     tilemapWider  = false;
    bool     tilemapHigher = false;
    uint16_t tilemapAdr    = 0;
    uint16_t tileAdr       = 0;
    bool     bigTiles      = false;
    bool     mosaicEnabled = false;
} BgLayer;

typedef struct Layer
{
    bool mainScreenEnabled  = false;
    bool subScreenEnabled   = false;
    bool mainScreenWindowed = false;
    bool subScreenWindowed  = false;
} Layer;

typedef struct WindowLayer
{
    bool    window1enabled  = false;
    bool    window2enabled  = false;
    bool    window1inversed = false;
    bool    window2inversed = false;
    uint8_t maskLogic       = 0;
} WindowLayer;


struct Snes;
class Ppu {
  public:
    Snes *snes = nullptr;

    uint16_t vram[0x8000]                   = {};
    uint16_t cgram[0x100]                   = {};
    uint16_t oam[0x100]                     = {};
    uint8_t  highOam[0x20]                  = {};
    uint8_t  objPixelBuffer[256]            = {};
    uint8_t  objPriorityBuffer[256]         = {};
    int16_t  m7matrix[8]                    = {};    // a, b, c, d, x, y, h, v
    bool     mathEnabled[6]                 = {};
    uint8_t  pixelBuffer[512 * 4 * 239 * 2] = {};

    Layer       layer[5];
    BgLayer     bgLayer[4];
    WindowLayer windowLayer[6];


    uint16_t vramPointer         = 0;
    bool     vramIncrementOnHigh = false;
    uint16_t vramIncrement       = 0;
    uint8_t  vramRemapMode       = 0;
    uint16_t vramReadBuffer      = 0;

    // cgram access
    uint8_t cgramPointer     = 0;
    bool    cgramSecondWrite = false;
    uint8_t cgramBuffer      = 0;

    // oam access

    uint8_t oamAdr           = 0;
    uint8_t oamAdrWritten    = 0;
    bool    oamInHigh        = false;
    bool    oamInHighWritten = false;
    bool    oamSecondWrite   = false;
    uint8_t oamBuffer        = 0;

    // object/sprites
    bool     objPriority = false;
    uint16_t objTileAdr1 = 0;
    uint16_t objTileAdr2 = 0;
    uint8_t  objSize     = 0;

    bool timeOver     = false;
    bool rangeOver    = false;
    bool objInterlace = false;

    // background layers
    uint8_t scrollPrev      = 0;
    uint8_t scrollPrev2     = 0;
    uint8_t mosaicSize      = 0;
    uint8_t mosaicStartLine = 0;

    // mode 7
    uint8_t m7prev       = 0;
    bool    m7largeField = false;
    bool    m7charFill   = false;
    bool    m7xFlip      = false;
    bool    m7yFlip      = false;
    bool    m7extBg      = false;

    // mode 7 internal
    int32_t m7startX = 0;
    int32_t m7startY = 0;

    // windows
    uint8_t window1left  = 0;
    uint8_t window1right = 0;
    uint8_t window2left  = 0;
    uint8_t window2right = 0;

    // color math
    uint8_t clipMode        = 0;
    uint8_t preventMathMode = 0;
    bool    addSubscreen    = false;
    bool    subtractColor   = false;
    bool    halfColor       = false;
    uint8_t fixedColorR     = 0;
    uint8_t fixedColorG     = 0;
    uint8_t fixedColorB     = 0;

    // settings
    bool    forcedBlank    = false;
    uint8_t brightness     = 0;
    uint8_t mode           = 0;
    bool    bg3priority    = false;
    bool    evenFrame      = false;
    bool    pseudoHires    = false;
    bool    overscan       = false;
    bool    frameOverscan  = false;
    bool    interlace      = false;
    bool    frameInterlace = false;
    bool    directColor    = false;

    // latching
    uint16_t hCount          = 0;
    uint16_t vCount          = 0;
    bool     hCountSecond    = false;
    bool     vCountSecond    = false;
    bool     countersLatched = false;
    uint8_t  ppu1openBus     = 0;
    uint8_t  ppu2openBus     = 0;


  public:
    Ppu(Snes *_snes);

    void     ppu_reset();
    bool     ppu_checkOverscan();
    void     ppu_handleVblank();
    void     ppu_runLine(int line);
    void     ppu_handlePixel(int x, int y);
    int      ppu_getPixel(int x, int y, bool sub, int *r, int *g, int *b);
    void     ppu_handleOPT(int layer, int *lx, int *ly);
    uint16_t ppu_getOffsetValue(int col, int row);
    int      ppu_getPixelForBgLayer(int x, int y, int layer, bool priority);
    void     ppu_calculateMode7Starts(int y);
    int      ppu_getPixelForMode7(int x, int layer, bool priority);
    bool     ppu_getWindowState(int layer, int x);
    void     ppu_evaluateSprites(int line);
    uint16_t ppu_getVramRemap();
    uint8_t  ppu_read(uint8_t adr);
    void     ppu_write(uint8_t adr, uint8_t val);
    void     ppu_putPixels(uint8_t *pixels);

  public:
    const int layersPerMode[10][12]    = {{4, 0, 1, 4, 0, 1, 4, 2, 3, 4, 2, 3}, {4, 0, 1, 4, 0, 1, 4, 2, 4, 2, 5, 5},
                                          {4, 0, 4, 1, 4, 0, 4, 1, 5, 5, 5, 5}, {4, 0, 4, 1, 4, 0, 4, 1, 5, 5, 5, 5},
                                          {4, 0, 4, 1, 4, 0, 4, 1, 5, 5, 5, 5}, {4, 0, 4, 1, 4, 0, 4, 1, 5, 5, 5, 5},
                                          {4, 0, 4, 4, 0, 4, 5, 5, 5, 5, 5, 5}, {4, 4, 4, 0, 4, 5, 5, 5, 5, 5, 5, 5},
                                          {2, 4, 0, 1, 4, 0, 1, 4, 4, 2, 5, 5}, {4, 4, 1, 4, 0, 4, 1, 5, 5, 5, 5, 5}};
    const int prioritysPerMode[10][12] = {{3, 1, 1, 2, 0, 0, 1, 1, 1, 0, 0, 0}, {3, 1, 1, 2, 0, 0, 1, 1, 0, 0, 5, 5},
                                          {3, 1, 2, 1, 1, 0, 0, 0, 5, 5, 5, 5}, {3, 1, 2, 1, 1, 0, 0, 0, 5, 5, 5, 5},
                                          {3, 1, 2, 1, 1, 0, 0, 0, 5, 5, 5, 5}, {3, 1, 2, 1, 1, 0, 0, 0, 5, 5, 5, 5},
                                          {3, 1, 2, 1, 0, 0, 5, 5, 5, 5, 5, 5}, {3, 2, 1, 0, 0, 5, 5, 5, 5, 5, 5, 5},
                                          {1, 3, 1, 1, 2, 0, 0, 1, 0, 0, 5, 5}, {3, 2, 1, 1, 0, 0, 0, 5, 5, 5, 5, 5}};
    const int layerCountPerMode[10]    = {12, 10, 8, 8, 8, 8, 6, 5, 10, 7};
    const int bitDepthsPerMode[10][4]  = {{2, 2, 2, 2}, {4, 4, 2, 5}, {4, 4, 5, 5}, {8, 4, 5, 5}, {8, 2, 5, 5},
                                          {4, 2, 5, 5}, {4, 5, 5, 5}, {8, 5, 5, 5}, {4, 4, 2, 5}, {8, 7, 5, 5}};
    const int spriteSizes[8][2]        = {{8, 16}, {8, 32}, {8, 64}, {16, 32}, {16, 64}, {32, 64}, {16, 32}, {16, 32}};
};


#endif
