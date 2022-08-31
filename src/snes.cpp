#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <algorithm>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <SDL2/SDL.h>

#include "snes.h"
#include "cpu.h"
#include "snes.h"
#include "mem.h"
#include "dma.h"
#include "apu/apu.h"
#include "ppu.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
static const char *sound_device = NULL;
static char        default_dir[PATH_MAX + 1];
static const char *s9x_base_dir = NULL, *rom_filename = NULL, *snapshot_filename = NULL, *play_smv_filename = NULL,
                  *record_smv_filename = NULL;
//
struct SMSU1          MSU1;
struct SMulti         Multi;
struct SSettings      Settings;
struct STimings       Timings;
struct SSNESGameFixes SNESGameFixes;
struct SUnixSettings  unixSettings;
struct SoundStatus    so;
SNESX                *g_snes = nullptr;
SDL_AudioSpec        *audiospec;
uint32                sound_buffer_size = 100;

SNESX::SNESX()
{
    m_MSU1          = &MSU1;
    m_Multi         = &Multi;
    m_Settings      = &Settings;
    m_Timings       = &Timings;
    m_SNESGameFixes = &SNESGameFixes;
    m_unixSettings  = &unixSettings;
    m_so            = &so;

    scpu     = new SCPUState(this);
    dma      = new SDMAS(this);
    mem      = new CMemory(this);
    ppu      = new SPPU(this);
    renderer = new Renderer(this);
}
SNESX::~SNESX()
{
    delete scpu;
    delete dma;
    delete mem;
    delete ppu;
    delete renderer;
}
void S9xCB(void *data)
{
    g_snes->S9xSamplesAvailable(data);
}
const char *SNESX::S9xGetDirectory(enum s9x_getdirtype dirtype)
{
    static char s[PATH_MAX + 1];
    switch (dirtype) {
        case DEFAULT_DIR:
            strncpy(s, s9x_base_dir, PATH_MAX + 1);
            s[PATH_MAX] = 0;
            break;
        case HOME_DIR:
            strncpy(s, getenv("HOME"), PATH_MAX + 1);
            s[PATH_MAX] = 0;
            break;
        case ROMFILENAME_DIR: {

            strncpy(s, mem->ROMFilename, PATH_MAX + 1);
            s[PATH_MAX] = 0;
            for (int i = strlen(s); i >= 0; i--) {
                if (s[i] == SLASH_CHAR) {
                    s[i] = 0;
                    break;
                }
            }
        } break;
        default:
            s[0] = 0;
            break;
    }
    return (s);
}
const char *SNESX::S9xGetFilename(const char *ex, enum s9x_getdirtype dirtype)
{
    static char s[PATH_MAX + 1];
    char        drive[_MAX_DRIVE + 1], dir[_MAX_DIR + 1], fname[_MAX_FNAME + 1], ext[_MAX_EXT + 1];
    _splitpath(mem->ROMFilename, drive, dir, fname, ext);
    snprintf(s, PATH_MAX + 1, "%s%s%s%s", S9xGetDirectory(dirtype), SLASH_STR, fname, ex);
    return (s);
}
bool8 SNESX::S9xInitUpdate(void)
{
    return (TRUE);
}
bool8 SNESX::S9xDeinitUpdate(int width, int height)
{
    S9xPutImage(width, height);
    return (TRUE);
}
void SNESX::S9xAutoSaveSRAM(void)
{
    mem->SaveSRAM(S9xGetFilename(".srm", SRAM_DIR));
}
void SNESX::S9xSyncSpeed(void)
{
    if (Settings.SoundSync) {
        return;
    }
    if (Settings.DumpStreams)
        return;
    if (Settings.HighSpeedSeek > 0)
        Settings.HighSpeedSeek--;
    if (Settings.TurboMode) {
        if ((++ppu->IPPU.FrameSkip >= Settings.TurboSkipFrames) && !Settings.HighSpeedSeek) {
            ppu->IPPU.FrameSkip       = 0;
            ppu->IPPU.SkippedFrames   = 0;
            ppu->IPPU.RenderThisFrame = TRUE;
        } else {
            ppu->IPPU.SkippedFrames++;
            ppu->IPPU.RenderThisFrame = FALSE;
        }
        return;
    }
    static struct timeval next1 = {0, 0};
    struct timeval        now;
    while (gettimeofday(&now, NULL) == -1)
        ;
    if (next1.tv_sec == 0) {
        next1 = now;
        next1.tv_usec++;
    }
    unsigned limit =
        (Settings.SkipFrames == AUTO_FRAMERATE) ? (timercmp(&next1, &now, <) ? 10 : 1) : Settings.SkipFrames;
    ppu->IPPU.RenderThisFrame = (++ppu->IPPU.SkippedFrames >= limit) ? TRUE : FALSE;
    if (ppu->IPPU.RenderThisFrame)
        ppu->IPPU.SkippedFrames = 0;
    else {
        if (timercmp(&next1, &now, <)) {
            unsigned lag = (now.tv_sec - next1.tv_sec) * 1000000 + now.tv_usec - next1.tv_usec;
            if (lag >= 500000) {
                next1 = now;
            }
        }
    }
    while (timercmp(&next1, &now, >)) {
        unsigned timeleft = (next1.tv_sec - now.tv_sec) * 1000000 + next1.tv_usec - now.tv_usec;
        usleep(timeleft);
        while (gettimeofday(&now, NULL) == -1)
            ;
    }
    next1.tv_usec += Settings.FrameTime;
    if (next1.tv_usec >= 1000000) {
        next1.tv_sec += next1.tv_usec / 1000000;
        next1.tv_usec %= 1000000;
    }
}
void SNESX::S9xSamplesAvailable(void *data)
{
}
static void sdl_audio_callback(void *userdata, Uint8 *stream, int len)
{
    SDL_LockAudio();
    S9xMixSamples(stream, len >> (Settings.SixteenBitSound ? 1 : 0));
    SDL_UnlockAudio();
    return;
}
static void samples_available(void *data)
{
    SDL_LockAudio();
    S9xFinalizeSamples();
    SDL_UnlockAudio();

    return;
}
bool8 SNESX::S9xOpenSoundDevice(void)
{
    SDL_InitSubSystem(SDL_INIT_AUDIO);

    audiospec = (SDL_AudioSpec *)malloc(sizeof(SDL_AudioSpec));

    audiospec->freq     = Settings.SoundPlaybackRate;
    audiospec->channels = Settings.Stereo ? 2 : 1;
    audiospec->format   = Settings.SixteenBitSound ? AUDIO_S16SYS : AUDIO_U8;
    audiospec->samples  = (sound_buffer_size * audiospec->freq / 1000) >> 1;
    audiospec->callback = sdl_audio_callback;

    printf("SDL sound driver initializing...\n");
    printf("    --> (Frequency: %dhz, Latency: %dms)...", audiospec->freq,
           (audiospec->samples * 1000 / audiospec->freq) << 1);

    if (SDL_OpenAudio(audiospec, NULL) < 0) {
        printf("Failed\n");

        free(audiospec);
        audiospec = NULL;

        return FALSE;
    }

    printf("OK\n");

    SDL_PauseAudio(0);

    S9xSetSamplesAvailableCallback(samples_available, NULL);
    return (TRUE);
}
void SNESX::S9xExit(void)
{
    S9xSetSoundMute(TRUE);
    Settings.StopEmulation = TRUE;
    mem->SaveSRAM(S9xGetFilename(".srm", SRAM_DIR));
    S9xDeinitDisplay();
    mem->Deinit();
    S9xDeinitAPU();
    exit(0);
}
void SNESX::S9xkeyDownUp(SDL_Event event)
{
    switch (event.type) {
        case SDL_KEYDOWN:
            if (SDLK_x)
                pad[0] |= 0x8000;
            if (SDLK_z)
                pad[0] |= 0x4000;
            if (SDLK_SPACE)
                pad[0] |= 0x2000;
            if (SDLK_RETURN)
                pad[0] |= 0x1000;
            if (SDLK_UP)
                pad[0] |= 0x0800;
            if (SDLK_DOWN)
                pad[0] |= 0x0400;
            if (SDLK_LEFT)
                pad[0] |= 0x0200;
            if (SDLK_RIGHT)
                pad[0] |= 0x0100;
            if (SDLK_s)
                pad[0] |= 0x0080;
            if (SDLK_a)
                pad[0] |= 0x0040;
            if (SDLK_q)
                pad[0] |= 0x0020;
            if (SDLK_w)
                pad[0] |= 0x0010;
            break;
        case SDL_KEYUP:
            if (SDLK_x)
                pad[0] &= ~0x8000;
            if (SDLK_z)
                pad[0] &= ~0x4000;
            if (SDLK_SPACE)
                pad[0] &= ~0x2000;
            if (SDLK_RETURN)
                pad[0] &= ~0x1000;
            if (SDLK_UP)
                pad[0] &= ~0x0800;
            if (SDLK_DOWN)
                pad[0] &= ~0x0400;
            if (SDLK_LEFT)
                pad[0] &= ~0x0200;
            if (SDLK_RIGHT)
                pad[0] &= ~0x0100;
            if (SDLK_s)
                pad[0] &= ~0x0080;
            if (SDLK_a)
                pad[0] &= ~0x0040;
            if (SDLK_q)
                pad[0] &= ~0x0020;
            if (SDLK_w)
                pad[0] &= ~0x0010;
            break;
    }
}
void SNESX::S9xinit(int argc, char **argv)
{
    rom_filename = argv[1];

    s9x_base_dir = default_dir;
    memset(&Settings, 0, sizeof(Settings));
    Settings.MouseMaster                  = TRUE;
    Settings.SuperScopeMaster             = TRUE;
    Settings.JustifierMaster              = TRUE;
    Settings.MultiPlayer5Master           = TRUE;
    Settings.FrameTimePAL                 = 20000;
    Settings.FrameTimeNTSC                = 16667;
    Settings.SixteenBitSound              = TRUE;
    Settings.Stereo                       = TRUE;
    Settings.SoundPlaybackRate            = 48000;
    Settings.SoundInputRate               = 31950;
    Settings.Transparency                 = TRUE;
    Settings.AutoDisplayMessages          = TRUE;
    Settings.InitialInfoStringTimeout     = 120;
    Settings.HDMATimingHack               = 100;
    Settings.BlockInvalidVRAMAccessMaster = TRUE;
    Settings.StopEmulation                = TRUE;
    Settings.WrongMovieStateProtection    = TRUE;
    Settings.DumpStreamsMaxFrames         = -1;
    Settings.StretchScreenshots           = 1;
    Settings.SnapshotScreenshots          = TRUE;
    Settings.SkipFrames                   = AUTO_FRAMERATE;
    Settings.TurboSkipFrames              = 15;
    Settings.CartAName[0]                 = 0;
    Settings.CartBName[0]                 = 0;
    Settings.ForceInterleaved2            = false;
    Settings.ForceInterleaveGD24          = false;
    Settings.ApplyCheats                  = false;
    Settings.NoPatch                      = true;
    Settings.IgnorePatchChecksum          = false;
    Settings.ForceLoROM                   = false;
    Settings.ForceHiROM                   = false;
    Settings.ForcePAL                     = false;
    Settings.ForceNTSC                    = false;
    Settings.InitialSnapshotFilename[0]   = '\0';
    Settings.SoundSync                    = false;
    Settings.SixteenBitSound              = true;
    Settings.Stereo                       = true;
    Settings.ReverseStereo                = false;
    Settings.SoundPlaybackRate            = 48000;
    Settings.SoundInputRate               = 31950;
    Settings.Mute                         = false;
    Settings.DynamicRateControl           = false;
    Settings.DynamicRateLimit             = 5;
    Settings.InterpolationMethod          = 2;
    Settings.Transparency                 = true;
    Settings.DisableGraphicWindows        = true;
    Settings.DisplayTime                  = false;
    Settings.DisplayFrameRate             = false;
    Settings.DisplayWatchedAddresses      = false;
    Settings.DisplayPressedKeys           = false;
    Settings.DisplayMovieFrame            = false;
    Settings.AutoDisplayMessages          = true;
    Settings.InitialInfoStringTimeout     = 120;
    Settings.BilinearFilter               = false;
    Settings.BSXBootup                    = false;
    Settings.TurboMode                    = false;
    Settings.TurboSkipFrames              = 15;
    Settings.MovieTruncate                = false;
    Settings.MovieNotifyIgnored           = false;
    Settings.WrongMovieStateProtection    = true;
    Settings.StretchScreenshots           = 1;
    Settings.SnapshotScreenshots          = true;
    Settings.DontSaveOopsSnapshot         = false;
    Settings.AutoSaveDelay                = 0;
    Settings.MouseMaster                  = false;
    Settings.SuperScopeMaster             = false;
    Settings.JustifierMaster              = false;
    Settings.MacsRifleMaster              = false;
    Settings.MultiPlayer5Master           = false;
    Settings.UpAndDown                    = false;
    Settings.SuperFXClockMultiplier       = 100;
    Settings.OverclockMode                = 0;
    Settings.SeparateEchoBuffer           = false;
    Settings.DisableGameSpecificHacks     = true;
    Settings.BlockInvalidVRAMAccessMaster = false;
    Settings.HDMATimingHack               = 100;
    Settings.MaxSpriteTilesPerLine        = 34;

    unixSettings.JoystickEnabled   = FALSE;
    unixSettings.ThreadSound       = TRUE;
    unixSettings.SoundBufferSize   = 100;
    unixSettings.SoundFragmentSize = 2048;
    unixSettings.rewindBufferSize  = 0;
    unixSettings.rewindGranularity = 1;
    unixSettings.SoundBufferSize   = 100;
    unixSettings.SoundFragmentSize = 2048;

    s9x_base_dir        = default_dir;
    snapshot_filename   = NULL;
    play_smv_filename   = NULL;
    record_smv_filename = NULL;
    sound_device        = "default";

    memset(&so, 0, sizeof(so));
    scpu->Flags = 0;
    mem->Init();

    S9xParseDisplayConfig(1);
    S9xInitAPU();
    S9xInitSound(sound_buffer_size, 0);
    S9xSetSoundMute(TRUE);
    bool8 loaded = mem->LoadROM(rom_filename);

    mem->LoadSRAM(S9xGetFilename(".srm", SRAM_DIR));
    Settings.StopEmulation = FALSE;
    S9xInitDisplay(argc, argv);
    S9xSetSoundMute(FALSE);

    SDL_Event event;
    bool8     quit_state = FALSE;

    while (1) {
        scpu->S9xMainLoop();

        while (SDL_PollEvent(&event) != 0) {
            switch (event.type) {
                case SDL_QUIT:
                    quit_state = TRUE;
                    break;
                case SDL_KEYDOWN:
                    S9xkeyDownUp(event);
                    break;

                case SDL_KEYUP:
                    S9xkeyDownUp(event);
                    break;

                default: {
                    break;
                }
            }
        }
        if (quit_state == TRUE) {
            g_snes->S9xExit();
        }
    }
}
int main(int argc, char **argv)
{
    if (argc < 2)
        exit(1);

    g_snes = new SNESX();
    g_snes->S9xinit(argc, argv);
}
