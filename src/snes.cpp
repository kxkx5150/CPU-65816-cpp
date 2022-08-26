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

#include "snes.h"
#include "cpu.h"
#include "apu/apu.h"
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
    int               samples_to_write;
    static uint8     *sound_buffer      = NULL;
    static int        sound_buffer_size = 0;
    snd_pcm_sframes_t frames_written, frames;
    frames = snd_pcm_avail(so.pcm_handle);
    if (frames < 0) {
        frames = snd_pcm_recover(so.pcm_handle, frames, 1);
        return;
    }
    if (Settings.DynamicRateControl) {
        S9xUpdateDynamicRate(snd_pcm_frames_to_bytes(so.pcm_handle, frames), so.output_buffer_size);
    }
    samples_to_write = S9xGetSampleCount();
    if (samples_to_write < 0)
        return;
    if (Settings.DynamicRateControl && !Settings.SoundSync) {
        if (frames < samples_to_write / 2) {
            S9xClearSamples();
            return;
        }
    }
    if (Settings.SoundSync && !Settings.TurboMode && !Settings.Mute) {
        snd_pcm_nonblock(so.pcm_handle, 0);
        frames = samples_to_write / 2;
    } else {
        snd_pcm_nonblock(so.pcm_handle, 1);
        frames = MIN(frames, samples_to_write / 2);
    }
    int bytes = snd_pcm_frames_to_bytes(so.pcm_handle, frames);
    if (bytes <= 0)
        return;
    if (sound_buffer_size < bytes || sound_buffer == NULL) {
        sound_buffer      = (uint8 *)realloc(sound_buffer, bytes);
        sound_buffer_size = bytes;
    }
    S9xMixSamples(sound_buffer, frames * 2);
    frames_written = 0;
    while (frames_written < frames) {
        int result;
        result = snd_pcm_writei(so.pcm_handle, sound_buffer + snd_pcm_frames_to_bytes(so.pcm_handle, frames_written),
                                frames - frames_written);
        if (result < 0) {
            result = snd_pcm_recover(so.pcm_handle, result, 1);
            if (result < 0) {
                break;
            }
        } else {
            frames_written += result;
        }
    }
}
bool8 SNESX::S9xOpenSoundDevice(void)
{
    int                  err;
    unsigned int         periods     = 8;
    unsigned int         buffer_size = unixSettings.SoundBufferSize * 1000;
    snd_pcm_sw_params_t *sw_params;
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_uframes_t    alsa_buffer_size, alsa_period_size;
    unsigned int         min  = 0;
    unsigned int         max  = 0;
    unsigned int         rate = Settings.SoundPlaybackRate;
    err                       = snd_pcm_open(&so.pcm_handle, sound_device, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
    if (err < 0) {
        printf("Failed: %s\n", snd_strerror(err));
        return (FALSE);
    }
    snd_pcm_hw_params_alloca(&hw_params);
    snd_pcm_hw_params_any(so.pcm_handle, hw_params);
    snd_pcm_hw_params_set_format(so.pcm_handle, hw_params, SND_PCM_FORMAT_S16);
    snd_pcm_hw_params_set_access(so.pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_rate_resample(so.pcm_handle, hw_params, 0);
    snd_pcm_hw_params_set_channels(so.pcm_handle, hw_params, 2);
    snd_pcm_hw_params_get_rate_min(hw_params, &min, NULL);
    snd_pcm_hw_params_get_rate_max(hw_params, &max, NULL);
    fprintf(stderr, "Alsa available rates: %d to %d\n", min, max);
    if (rate > max && rate < min) {
        fprintf(stderr, "Rate %d not available. Using %d instead.\n", rate, max);
        rate = max;
    }
    snd_pcm_hw_params_set_rate_near(so.pcm_handle, hw_params, &rate, NULL);
    snd_pcm_hw_params_get_buffer_time_min(hw_params, &min, NULL);
    snd_pcm_hw_params_get_buffer_time_max(hw_params, &max, NULL);
    fprintf(stderr, "Alsa available buffer sizes: %dms to %dms\n", min / 1000, max / 1000);
    if (buffer_size < min && buffer_size > max) {
        fprintf(stderr, "Buffer size %dms not available. Using %d instead.\n", buffer_size / 1000, (min + max) / 2000);
        buffer_size = (min + max) / 2;
    }
    snd_pcm_hw_params_set_buffer_time_near(so.pcm_handle, hw_params, &buffer_size, NULL);
    snd_pcm_hw_params_get_periods_min(hw_params, &min, NULL);
    snd_pcm_hw_params_get_periods_max(hw_params, &max, NULL);
    fprintf(stderr, "Alsa period ranges: %d to %d blocks\n", min, max);
    if (periods > max)
        periods = max;
    snd_pcm_hw_params_set_periods_near(so.pcm_handle, hw_params, &periods, NULL);
    err = snd_pcm_hw_params(so.pcm_handle, hw_params);
    if (err < 0) {
        fprintf(stderr, "Alsa error: unable to install hw params\n");
        snd_pcm_drain(so.pcm_handle);
        snd_pcm_close(so.pcm_handle);
        so.pcm_handle = NULL;
        return (FALSE);
    }
    snd_pcm_sw_params_alloca(&sw_params);
    snd_pcm_sw_params_current(so.pcm_handle, sw_params);
    snd_pcm_get_params(so.pcm_handle, &alsa_buffer_size, &alsa_period_size);
    snd_pcm_sw_params_set_start_threshold(so.pcm_handle, sw_params, (alsa_buffer_size / 2));
    err = snd_pcm_sw_params(so.pcm_handle, sw_params);
    if (err < 0) {
        fprintf(stderr, "Alsa error: unable to install sw params\n");
        snd_pcm_drain(so.pcm_handle);
        snd_pcm_close(so.pcm_handle);
        so.pcm_handle = NULL;
        return (FALSE);
    }
    err = snd_pcm_prepare(so.pcm_handle);
    if (err < 0) {
        fprintf(stderr, "Alsa error: unable to prepare audio: %s\n", snd_strerror(err));
        snd_pcm_drain(so.pcm_handle);
        snd_pcm_close(so.pcm_handle);
        so.pcm_handle = NULL;
        return (FALSE);
    }
    so.output_buffer_size = snd_pcm_frames_to_bytes(so.pcm_handle, alsa_buffer_size);
    S9xSetSamplesAvailableCallback(S9xCB, NULL);
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
    S9xInitSound(0);
    S9xSetSoundMute(TRUE);
    bool8 loaded = mem->LoadROM(rom_filename);

    mem->LoadSRAM(S9xGetFilename(".srm", SRAM_DIR));
    Settings.StopEmulation = FALSE;
    S9xInitDisplay(argc, argv);
    S9xSetSoundMute(FALSE);
    while (1) {
        scpu->S9xMainLoop();
    }
    S9xProcessEvents(FALSE);
}
int main(int argc, char **argv)
{
    if (argc < 2)
        exit(1);

    g_snes = new SNESX();
    g_snes->S9xinit(argc, argv);
}
