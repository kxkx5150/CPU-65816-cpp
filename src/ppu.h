#ifndef _PPU_H_
#define _PPU_H_
#include "snes.h"
#include "utils/port.h"
#include "renderer.h"
#include "mem.h"

#define FIRST_VISIBLE_LINE 1
#define TILE_2BIT          0
#define TILE_4BIT          1
#define TILE_8BIT          2
#define TILE_2BIT_EVEN     3
#define TILE_2BIT_ODD      4
#define TILE_4BIT_EVEN     5
#define TILE_4BIT_ODD      6
#define MAX_2BIT_TILES     4096
#define MAX_4BIT_TILES     2048
#define MAX_8BIT_TILES     1024
#define CLIP_OR            0
#define CLIP_AND           1
#define CLIP_XOR           2
#define CLIP_XNOR          3
#define MAX_5C77_VERSION   0x01
#define MAX_5C78_VERSION   0x03
#define MAX_5A22_VERSION   0x02
#define CHECK_INBLANK()                                                                                                \
    if (Settings.BlockInvalidVRAMAccess && !ForcedBlanking &&                                                          \
        snes->cpu->V_Counter < ScreenHeight + FIRST_VISIBLE_LINE) {                                                    \
        VMA.Address += !VMA.High ? VMA.Increment : 0;                                                                  \
        return;                                                                                                        \
    }
#undef CHECK_INBLANK
#define CHECK_INBLANK()                                                                                                \
    if (Settings.BlockInvalidVRAMAccess && !ForcedBlanking &&                                                          \
        snes->scpu->V_Counter < ScreenHeight + FIRST_VISIBLE_LINE) {                                                   \
        VMA.Address += VMA.High ? VMA.Increment : 0;                                                                   \
        return;                                                                                                        \
    }

struct ClipData
{
    uint8  Count;
    uint8  DrawMode[6];
    uint16 Left[6];
    uint16 Right[6];
};
struct SOBJ
{
    int16  HPos;
    uint16 VPos;
    uint8  HFlip;
    uint8  VFlip;
    uint16 Name;
    uint8  Priority;
    uint8  Palette;
    uint8  Size;
};


//

class SPPU {
  public:
    SNESX *snes = nullptr;

    uint16 SignExtend[2] = {0x0000, 0xff00};

  public:
    struct InternalPPU
    {
        struct ClipData Clip[2][6];
        bool8           ColorsChanged;
        bool8           OBJChanged;
        uint8          *TileCache[7];
        uint8          *TileCached[7];
        bool8           Interlace;
        bool8           InterlaceOBJ;
        bool8           PseudoHires;
        bool8           DoubleWidthPixels;
        bool8           DoubleHeightPixels;
        int             CurrentLine;
        int             PreviousLine;
        uint8          *XB;
        uint32          Red[256];
        uint32          Green[256];
        uint32          Blue[256];
        uint16          ScreenColors[256];
        uint8           MaxBrightness;
        bool8           RenderThisFrame;
        int             RenderedScreenWidth;
        int             RenderedScreenHeight;
        uint32          FrameCount;
        uint32          RenderedFramesCount;
        uint32          DisplayedRenderedFrameCount;
        uint32          TotalEmulatedFrames;
        uint32          SkippedFrames;
        uint32          FrameSkip;
    } IPPU;

  public:
    struct
    {
        bool8  High;
        uint8  Increment;
        uint16 Address;
        uint16 Mask1;
        uint16 FullGraphicCount;
        uint16 Shift;
    } VMA;
    uint32 WRAM;
    struct
    {
        uint16 SCBase;
        uint16 HOffset;
        uint16 VOffset;
        uint8  BGSize;
        uint16 NameBase;
        uint16 SCSize;
    } BG[4];

  public:
    uint8       BGMode;
    uint8       BG3Priority;
    bool8       CGFLIP;
    uint8       CGFLIPRead;
    uint8       CGADD;
    uint8       CGSavedByte;
    uint16      CGDATA[256];
    struct SOBJ OBJ[128];
    bool8       OBJThroughMain;
    bool8       OBJThroughSub;
    bool8       OBJAddition;
    uint16      OBJNameBase;
    uint16      OBJNameSelect;
    uint8       OBJSizeSelect;
    uint16      OAMAddr;
    uint16      SavedOAMAddr;
    uint8       OAMPriorityRotation;
    uint8       OAMFlip;
    uint8       OAMReadFlip;
    uint16      OAMTileAddress;
    uint16      OAMWriteRegister;
    uint8       OAMData[512 + 32];
    uint8       FirstSprite;
    uint8       LastSprite;
    uint8       RangeTimeOver;
    bool8       HTimerEnabled;
    bool8       VTimerEnabled;
    short       HTimerPosition;
    short       VTimerPosition;
    uint16      IRQHBeamPos;
    uint16      IRQVBeamPos;
    uint8       HBeamFlip;
    uint8       VBeamFlip;
    uint16      HBeamPosLatched;
    uint16      VBeamPosLatched;
    uint16      GunHLatch;
    uint16      GunVLatch;
    uint8       HVBeamCounterLatched;
    bool8       Mode7HFlip;
    bool8       Mode7VFlip;
    uint8       Mode7Repeat;
    short       MatrixA;
    short       MatrixB;
    short       MatrixC;
    short       MatrixD;
    short       CentreX;
    short       CentreY;
    short       M7HOFS;
    short       M7VOFS;
    uint8       Mosaic;
    uint8       MosaicStart;
    bool8       BGMosaic[4];
    uint8       Window1Left;
    uint8       Window1Right;
    uint8       Window2Left;
    uint8       Window2Right;
    bool8       RecomputeClipWindows;
    uint8       ClipCounts[6];
    uint8       ClipWindowOverlapLogic[6];
    uint8       ClipWindow1Enable[6];
    uint8       ClipWindow2Enable[6];
    bool8       ClipWindow1Inside[6];
    bool8       ClipWindow2Inside[6];
    bool8       ForcedBlanking;
    uint8       FixedColourRed;
    uint8       FixedColourGreen;
    uint8       FixedColourBlue;
    uint8       Brightness;
    uint16      ScreenHeight;
    bool8       Need16x8Mulitply;
    uint8       BGnxOFSbyte;
    uint8       M7byte;
    uint8       HDMA;
    uint8       HDMAEnded;
    uint8       OpenBus1;
    uint8       OpenBus2;
    uint16      VRAMReadBuffer;

  public:
    SPPU(SNESX *_snes);

    void  S9xLatchCounters(bool force);
    void  S9xTryGunLatch(bool force);
    int   CyclesUntilNext(int hc, int vc);
    void  S9xUpdateIRQPositions(bool initial);
    void  S9xFixColourBrightness(void);
    void  S9xSetPPU(uint8 Byte, uint16 Address);
    uint8 S9xGetPPU(uint16 Address);
    void  S9xSetCPU(uint8 Byte, uint16 Address);
    uint8 S9xGetCPU(uint16 Address);
    void  S9xResetPPU(void);
    void  S9xResetPPUFast(void);
    void  S9xSoftResetPPU(void);
    void  FLUSH_REDRAW(void);
    void  S9xUpdateVRAMReadBuffer();
    void  REGISTER_2104(uint8 Byte);
    void  REGISTER_2118(uint8 Byte);
    void  REGISTER_2118_tile(uint8 Byte);
    void  REGISTER_2118_linear(uint8 Byte);
    void  REGISTER_2119(uint8 Byte);
    void  REGISTER_2119_tile(uint8 Byte);
    void  REGISTER_2119_linear(uint8 Byte);
    void  REGISTER_2122(uint8 Byte);
    void  REGISTER_2180(uint8 Byte);
    uint8 REGISTER_4212(void);

  public:
    uint8 mul_brightness[16][32] = {
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
         0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02},
        {0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x02, 0x02, 0x02,
         0x02, 0x02, 0x02, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x04, 0x04, 0x04, 0x04, 0x04},
        {0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x02, 0x02, 0x02, 0x02, 0x03, 0x03, 0x03,
         0x03, 0x03, 0x04, 0x04, 0x04, 0x04, 0x04, 0x05, 0x05, 0x05, 0x05, 0x05, 0x06, 0x06, 0x06, 0x06},
        {0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x02, 0x02, 0x02, 0x02, 0x03, 0x03, 0x03, 0x03, 0x04, 0x04,
         0x04, 0x05, 0x05, 0x05, 0x05, 0x06, 0x06, 0x06, 0x06, 0x07, 0x07, 0x07, 0x07, 0x08, 0x08, 0x08},
        {0x00, 0x00, 0x01, 0x01, 0x01, 0x02, 0x02, 0x02, 0x03, 0x03, 0x03, 0x04, 0x04, 0x04, 0x05, 0x05,
         0x05, 0x06, 0x06, 0x06, 0x07, 0x07, 0x07, 0x08, 0x08, 0x08, 0x09, 0x09, 0x09, 0x0a, 0x0a, 0x0a},
        {0x00, 0x00, 0x01, 0x01, 0x02, 0x02, 0x02, 0x03, 0x03, 0x04, 0x04, 0x04, 0x05, 0x05, 0x06, 0x06,
         0x06, 0x07, 0x07, 0x08, 0x08, 0x08, 0x09, 0x09, 0x0a, 0x0a, 0x0a, 0x0b, 0x0b, 0x0c, 0x0c, 0x0c},
        {0x00, 0x00, 0x01, 0x01, 0x02, 0x02, 0x03, 0x03, 0x04, 0x04, 0x05, 0x05, 0x06, 0x06, 0x07, 0x07,
         0x07, 0x08, 0x08, 0x09, 0x09, 0x0a, 0x0a, 0x0b, 0x0b, 0x0c, 0x0c, 0x0d, 0x0d, 0x0e, 0x0e, 0x0e},
        {0x00, 0x01, 0x01, 0x02, 0x02, 0x03, 0x03, 0x04, 0x04, 0x05, 0x05, 0x06, 0x06, 0x07, 0x07, 0x08,
         0x09, 0x09, 0x0a, 0x0a, 0x0b, 0x0b, 0x0c, 0x0c, 0x0d, 0x0d, 0x0e, 0x0e, 0x0f, 0x0f, 0x10, 0x11},
        {0x00, 0x01, 0x01, 0x02, 0x02, 0x03, 0x04, 0x04, 0x05, 0x05, 0x06, 0x07, 0x07, 0x08, 0x08, 0x09,
         0x0a, 0x0a, 0x0b, 0x0b, 0x0c, 0x0d, 0x0d, 0x0e, 0x0e, 0x0f, 0x10, 0x10, 0x11, 0x11, 0x12, 0x13},
        {0x00, 0x01, 0x01, 0x02, 0x03, 0x03, 0x04, 0x05, 0x05, 0x06, 0x07, 0x07, 0x08, 0x09, 0x09, 0x0a,
         0x0b, 0x0b, 0x0c, 0x0d, 0x0d, 0x0e, 0x0f, 0x0f, 0x10, 0x11, 0x11, 0x12, 0x13, 0x13, 0x14, 0x15},
        {0x00, 0x01, 0x01, 0x02, 0x03, 0x04, 0x04, 0x05, 0x06, 0x07, 0x07, 0x08, 0x09, 0x0a, 0x0a, 0x0b,
         0x0c, 0x0c, 0x0d, 0x0e, 0x0f, 0x0f, 0x10, 0x11, 0x12, 0x12, 0x13, 0x14, 0x15, 0x15, 0x16, 0x17},
        {0x00, 0x01, 0x02, 0x02, 0x03, 0x04, 0x05, 0x06, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0a, 0x0b, 0x0c,
         0x0d, 0x0e, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x12, 0x13, 0x14, 0x15, 0x16, 0x16, 0x17, 0x18, 0x19},
        {0x00, 0x01, 0x02, 0x03, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0a, 0x0b, 0x0c, 0x0d,
         0x0e, 0x0f, 0x10, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x17, 0x18, 0x19, 0x1a, 0x1b},
        {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
         0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d},
        {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
         0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f}};
};
#endif
