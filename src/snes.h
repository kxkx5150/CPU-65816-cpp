#ifndef _SNES9X_H_
#define _SNES9X_H_
#include <alsa/asoundlib.h>
#include "utils/port.h"

#define VERSION                "1.00"
#define STREAM                 Stream *
#define READ_STREAM(p, l, s)   s->read(p, l)
#define WRITE_STREAM(p, l, s)  s->write(p, l)
#define GETS_STREAM(p, l, s)   s->gets(p, l)
#define GETC_STREAM(s)         s->get_char()
#define OPEN_STREAM(f, m)      openStreamFromFSTREAM(f, m)
#define REOPEN_STREAM(f, m)    reopenStreamFromFd(f, m)
#define FIND_STREAM(s)         s->pos()
#define REVERT_STREAM(s, o, p) s->revert(p, o)
#define CLOSE_STREAM(s)        s->closeStream()

#define SNES_WIDTH           256
#define SNES_HEIGHT          224
#define SNES_HEIGHT_EXTENDED 239

#define MAX_SNES_WIDTH  (SNES_WIDTH * 2)
#define MAX_SNES_HEIGHT (SNES_HEIGHT_EXTENDED * 2)

#define NTSC_MASTER_CLOCK      21477272.727272
#define PAL_MASTER_CLOCK       21281370.0
#define SNES_MAX_NTSC_VCOUNTER 262
#define SNES_MAX_PAL_VCOUNTER  312
#define SNES_HCOUNTER_MAX      341
#define ONE_CYCLE              6
#define SLOW_ONE_CYCLE         8
#define TWO_CYCLES             12
#define ONE_DOT_CYCLE          4

#define SNES_CYCLES_PER_SCANLINE (SNES_HCOUNTER_MAX * ONE_DOT_CYCLE)
#define SNES_SCANLINE_TIME       (SNES_CYCLES_PER_SCANLINE / NTSC_MASTER_CLOCK)

#define SNES_WRAM_REFRESH_HC_v1  530
#define SNES_WRAM_REFRESH_HC_v2  538
#define SNES_WRAM_REFRESH_CYCLES 40
#define SNES_HBLANK_START_HC     1096
#define SNES_HDMA_START_HC       1106
#define SNES_HBLANK_END_HC       4
#define SNES_HDMA_INIT_HC        20

#define SNES_RENDER_START_HC (128 * ONE_DOT_CYCLE)
#define SNES_TR_MASK         (1 << 4)
#define SNES_TL_MASK         (1 << 5)
#define SNES_X_MASK          (1 << 6)
#define SNES_A_MASK          (1 << 7)
#define SNES_RIGHT_MASK      (1 << 8)
#define SNES_LEFT_MASK       (1 << 9)
#define SNES_DOWN_MASK       (1 << 10)
#define SNES_UP_MASK         (1 << 11)
#define SNES_START_MASK      (1 << 12)
#define SNES_SELECT_MASK     (1 << 13)
#define SNES_Y_MASK          (1 << 14)
#define SNES_B_MASK          (1 << 15)
#define DEBUG_MODE_FLAG      (1 << 0)
#define TRACE_FLAG           (1 << 1)
#define SINGLE_STEP_FLAG     (1 << 2)
#define BREAK_FLAG           (1 << 3)
#define SCAN_KEYS_FLAG       (1 << 4)
#define HALTED_FLAG          (1 << 12)
#define FRAME_ADVANCE_FLAG   (1 << 9)
#define AUTO_FRAMERATE       200


void        S9xPutImage(int, int);
void        S9xInitDisplay(int, char **);
void        S9xDeinitDisplay(void);
void        S9xTextMode(void);
void        S9xGraphicsMode(void);
const char *S9xStringInput(const char *);
void        S9xProcessEvents(bool8);
const char *S9xSelectFilename(const char *, const char *, const char *, const char *);
const char *S9xParseDisplayConfig(int pass);


// enum
enum
{
    S9X_TRACE,
    S9X_DEBUG,
    S9X_WARNING,
    S9X_INFO,
    S9X_ERROR,
    S9X_FATAL_ERROR
};
enum
{
    S9X_NO_INFO,
    S9X_ROM_INFO,
    S9X_HEADERS_INFO,
    S9X_CONFIG_INFO,
    S9X_ROM_CONFUSING_FORMAT_INFO,
    S9X_ROM_INTERLEAVED_INFO,
    S9X_SOUND_DEVICE_OPEN_FAILED,
    S9X_APU_STOPPED,
    S9X_USAGE,
    S9X_GAME_GENIE_CODE_ERROR,
    S9X_ACTION_REPLY_CODE_ERROR,
    S9X_GOLD_FINGER_CODE_ERROR,
    S9X_DEBUG_OUTPUT,
    S9X_DMA_TRACE,
    S9X_HDMA_TRACE,
    S9X_WRONG_FORMAT,
    S9X_WRONG_VERSION,
    S9X_ROM_NOT_FOUND,
    S9X_FREEZE_FILE_NOT_FOUND,
    S9X_PPU_TRACE,
    S9X_TRACE_DSP1,
    S9X_FREEZE_ROM_NAME,
    S9X_HEADER_WARNING,
    S9X_NETPLAY_NOT_SERVER,
    S9X_FREEZE_FILE_INFO,
    S9X_TURBO_MODE,
    S9X_SOUND_NOT_BUILT,
    S9X_MOVIE_INFO,
    S9X_WRONG_MOVIE_SNAPSHOT,
    S9X_NOT_A_MOVIE_SNAPSHOT,
    S9X_SNAPSHOT_INCONSISTENT,
    S9X_AVI_INFO,
    S9X_PRESSED_KEYS_INFO
};
enum s9x_getdirtype
{
    DEFAULT_DIR = 0,
    HOME_DIR,
    ROMFILENAME_DIR,
    ROM_DIR,
    SRAM_DIR,
    SNAPSHOT_DIR,
    SCREENSHOT_DIR,
    SPC_DIR,
    CHEAT_DIR,
    PATCH_DIR,
    BIOS_DIR,
    LOG_DIR,
    SAT_DIR,
    LAST_DIR
};
enum
{
    HC_HBLANK_START_EVENT = 1,
    HC_HDMA_START_EVENT   = 2,
    HC_HCOUNTER_MAX_EVENT = 3,
    HC_HDMA_INIT_EVENT    = 4,
    HC_RENDER_EVENT       = 5,
    HC_WRAM_REFRESH_EVENT = 6
};
enum
{
    IRQ_NONE        = 0x0,
    IRQ_SET_FLAG    = 0x1,
    IRQ_CLEAR_FLAG  = 0x2,
    IRQ_TRIGGER_NMI = 0x4
};
enum
{
    PAUSE_NETPLAY_CONNECT    = (1 << 0),
    PAUSE_TOGGLE_FULL_SCREEN = (1 << 1),
    PAUSE_EXIT               = (1 << 2),
    PAUSE_MENU               = (1 << 3),
    PAUSE_INACTIVE_WINDOW    = (1 << 4),
    PAUSE_WINDOW_ICONISED    = (1 << 5),
    PAUSE_RESTORE_GUI        = (1 << 6),
    PAUSE_FREEZE_FILE        = (1 << 7)
};

// struct
struct STimings
{
    int32 H_Max_Master;
    int32 H_Max;
    int32 V_Max_Master;
    int32 V_Max;
    int32 HBlankStart;
    int32 HBlankEnd;
    int32 HDMAInit;
    int32 HDMAStart;
    int32 NMITriggerPos;
    int32 NextIRQTimer;
    int32 IRQTriggerCycles;
    int32 WRAMRefreshPos;
    int32 RenderPos;
    bool8 InterlaceField;
    int32 DMACPUSync;
    int32 NMIDMADelay;
    int32 IRQFlagChanging;
    int32 APUSpeedup;
    bool8 APUAllowTimeOverflow;
};
struct SSettings
{
    bool8  TraceDMA;
    bool8  TraceHDMA;
    bool8  TraceVRAM;
    bool8  TraceUnknownRegisters;
    bool8  TraceDSP;
    bool8  TraceHCEvent;
    bool8  TraceSMP;
    bool8  SuperFX;
    uint8  DSP;
    bool8  SA1;
    bool8  C4;
    bool8  SDD1;
    bool8  SPC7110;
    bool8  SPC7110RTC;
    bool8  OBC1;
    uint8  SETA;
    bool8  SRTC;
    bool8  BS;
    bool8  BSXItself;
    bool8  BSXBootup;
    bool8  MSU1;
    bool8  MouseMaster;
    bool8  SuperScopeMaster;
    bool8  JustifierMaster;
    bool8  MultiPlayer5Master;
    bool8  MacsRifleMaster;
    bool8  ForceLoROM;
    bool8  ForceHiROM;
    bool8  ForceHeader;
    bool8  ForceNoHeader;
    bool8  ForceInterleaved;
    bool8  ForceInterleaved2;
    bool8  ForceInterleaveGD24;
    bool8  ForceNotInterleaved;
    bool8  ForcePAL;
    bool8  ForceNTSC;
    bool8  PAL;
    uint32 FrameTimePAL;
    uint32 FrameTimeNTSC;
    uint32 FrameTime;
    bool8  SoundSync;
    bool8  SixteenBitSound;
    uint32 SoundPlaybackRate;
    uint32 SoundInputRate;
    bool8  Stereo;
    bool8  ReverseStereo;
    bool8  Mute;
    bool8  DynamicRateControl;
    int32  DynamicRateLimit;
    int32  InterpolationMethod;
    bool8  Transparency;
    uint8  BG_Forced;
    bool8  DisableGraphicWindows;
    bool8  DisplayTime;
    bool8  DisplayFrameRate;
    bool8  DisplayWatchedAddresses;
    bool8  DisplayPressedKeys;
    bool8  DisplayMovieFrame;
    bool8  AutoDisplayMessages;
    uint32 InitialInfoStringTimeout;
    uint16 DisplayColor;
    bool8  BilinearFilter;
    bool8  Multi;
    char   CartAName[PATH_MAX + 1];
    char   CartBName[PATH_MAX + 1];
    bool8  DisableGameSpecificHacks;
    bool8  BlockInvalidVRAMAccessMaster;
    bool8  BlockInvalidVRAMAccess;
    int32  HDMATimingHack;
    bool8  ForcedPause;
    bool8  Paused;
    bool8  StopEmulation;
    uint32 SkipFrames;
    uint32 TurboSkipFrames;
    uint32 AutoMaxSkipFrames;
    bool8  TurboMode;
    uint32 HighSpeedSeek;
    bool8  FrameAdvance;
    bool8  NetPlay;
    bool8  NetPlayServer;
    char   ServerName[128];
    int    Port;
    bool8  MovieTruncate;
    bool8  MovieNotifyIgnored;
    bool8  WrongMovieStateProtection;
    bool8  DumpStreams;
    int    DumpStreamsMaxFrames;
    bool8  TakeScreenshot;
    int8   StretchScreenshots;
    bool8  SnapshotScreenshots;
    char   InitialSnapshotFilename[PATH_MAX + 1];
    bool8  FastSavestates;
    bool8  ApplyCheats;
    bool8  NoPatch;
    bool8  IgnorePatchChecksum;
    bool8  IsPatched;
    int32  AutoSaveDelay;
    bool8  DontSaveOopsSnapshot;
    bool8  UpAndDown;
    bool8  SeparateEchoBuffer;
    uint32 SuperFXClockMultiplier;
    int    OverclockMode;
    int    OneClockCycle;
    int    OneSlowClockCycle;
    int    TwoClockCycles;
    int    MaxSpriteTilesPerLine;
};
struct SSNESGameFixes
{
    uint8 SRAMInitialValue;
    uint8 Uniracers;
};
struct SUnixSettings
{
    bool8  JoystickEnabled;
    bool8  ThreadSound;
    uint32 SoundBufferSize;
    uint32 SoundFragmentSize;
    uint32 rewindBufferSize;
    uint32 rewindGranularity;
};
struct SoundStatus
{
    snd_pcm_t *pcm_handle;
    int        output_buffer_size;
};
typedef struct
{
    uint8 _5C77;
    uint8 _5C78;
    uint8 _5A22;
} SnesModel;


class SCPUState;
class CMemory;
class SDMAS;
class SPPU;
class Renderer;

class SNESX {
  public:
    SCPUState *scpu     = nullptr;
    CMemory   *mem      = nullptr;
    SDMAS     *dma      = nullptr;
    SPPU      *ppu      = nullptr;
    Renderer  *renderer = nullptr;

  public:
    uint8 OpenBus = 0;

    SnesModel  M1SNES = {1, 3, 2};
    SnesModel *Model  = &M1SNES;

    struct SMSU1          *m_MSU1;
    struct SMulti         *m_Multi;
    struct SSettings      *m_Settings;
    struct STimings       *m_Timings;
    struct SSNESGameFixes *m_SNESGameFixes;
    struct SUnixSettings  *m_unixSettings;
    struct SoundStatus    *m_so;

  public:
    SNESX();
    ~SNESX();

    void        S9xinit(int argc, char **argv);
    void        S9xExit(void);
    bool8       S9xOpenSoundDevice(void);
    void        S9xSamplesAvailable(void *data);
    void        S9xSyncSpeed(void);
    void        S9xAutoSaveSRAM(void);
    bool8       S9xDeinitUpdate(int width, int height);
    bool8       S9xInitUpdate(void);
    const char *S9xGetFilename(const char *ex, enum s9x_getdirtype dirtype);
    const char *S9xGetDirectory(enum s9x_getdirtype dirtype);

  public:
};
extern SNESX                *g_snes;
extern struct SSettings      Settings;
extern struct STimings       Timings;
extern struct SSNESGameFixes SNESGameFixes;
#endif
