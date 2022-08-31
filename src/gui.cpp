#define MITSHM
#define HAVE_STRINGS_H
#define FOURCC_YUY2 0x32595559
#define FOURCC_I420 0x30323449

#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <vector>
#include <SDL2/SDL.h>

#include <strings.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include "snes.h"
#include "mem.h"
#include "ppu.h"

static uint8 *XDelta = NULL;

int      sWith   = 1280;
int      sHeight = 720;
SDL_Rect sdl_sizescreen;

struct GUIData
{
    SDL_Renderer *sdl_renderer;
    SDL_Texture  *sdl_texture;
    SDL_Window   *sdl_window;
    SDL_Surface  *sdl_screen;
    uint8        *snes_buffer;
    uint8        *blit_screen;
    uint32        blit_screen_pitch;
    int           video_mode;
    bool8         fullscreen;
};

typedef std::pair<std::string, std::string> strpair_t;

static struct GUIData         GUI;
extern std::vector<strpair_t> keymaps;
typedef void (*Blitter)(uint8 *, int, uint8 *, int, int, int);

enum
{
    VIDEOMODE_BLOCKY = 1,
    VIDEOMODE_TV,
    VIDEOMODE_SMOOTH,
    VIDEOMODE_SUPEREAGLE,
    VIDEOMODE_2XSAI,
    VIDEOMODE_SUPER2XSAI,
    VIDEOMODE_EPX,
    VIDEOMODE_HQ2X
};

static void SetupImage(void);
static void TakedownImage(void);
static void Repaint(bool8);

const char *S9xParseDisplayConfig(int pass)
{
    if (pass != 1)
        return ("Unix/SDL");
    GUI.fullscreen = FALSE;
    GUI.video_mode = VIDEOMODE_BLOCKY;
    return ("Unix/SDL");
}
static void FatalError(const char *str)
{
    fprintf(stderr, "%s\n", str);
    g_snes->S9xExit();
}
static int get_inv_shift(uint32 mask, int bpp)
{
    int i;
    for (i = 0; (i < bpp) && !(mask & (1 << i)); i++) {
    };
    for (; (i < bpp) && (mask & (1 << i)); i++) {
    };
    return (bpp - i);
}
static unsigned char CLAMP(int v, int min, int max)
{
    if (v < min)
        return min;
    if (v > max)
        return max;
    return v;
}
void S9xBlitFilterDeinit(void)
{
    if (XDelta) {
        delete[] XDelta;
        XDelta = NULL;
    }
}
void S9xBlitClearDelta(void)
{
    uint32 *d = (uint32 *)XDelta;
    for (int y = 0; y < SNES_HEIGHT_EXTENDED; y++)
        for (int x = 0; x < SNES_WIDTH; x++)
            *d++ = 0x80008000;
}
bool8 S9xBlitFilterInit(void)
{
    XDelta = new uint8[SNES_WIDTH * SNES_HEIGHT_EXTENDED * 4];
    if (!XDelta)
        return (FALSE);

    S9xBlitClearDelta();

    return (TRUE);
}
void S9xBlitPixSimple1x1(uint8 *srcPtr, int srcRowBytes, uint8 *dstPtr, int dstRowBytes, int width, int height)
{
    width <<= 1;
    for (; height; height--) {
        memcpy(dstPtr, srcPtr, width);
        srcPtr += srcRowBytes;
        dstPtr += dstRowBytes;
    }
}
void S9xInitDisplay(int argc, char **argv)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("Unable to initialize SDL: %s\n", SDL_GetError());
    }

    atexit(SDL_Quit);
    S9xBlitFilterInit();

    GUI.sdl_window =
        SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SNES_WIDTH * 2, SNES_HEIGHT * 2, 0);
    GUI.sdl_renderer = SDL_CreateRenderer(GUI.sdl_window, 0, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    GUI.sdl_screen =
        SDL_CreateRGBSurfaceWithFormat(0, SNES_WIDTH * 2, SNES_HEIGHT_EXTENDED * 2, 16, SDL_PIXELFORMAT_RGB565);
    sdl_sizescreen.w = (SNES_WIDTH * 4);
    sdl_sizescreen.h = (SNES_HEIGHT_EXTENDED * 4);
    sdl_sizescreen.y = 0;
    sdl_sizescreen.x = 0;

    if (GUI.sdl_screen == NULL) {
        printf("Unable to set video mode: %s\n", SDL_GetError());
        exit(1);
    }

    SetupImage();
}
void S9xDeinitDisplay(void)
{
    TakedownImage();
    SDL_Quit();
    S9xBlitFilterDeinit();
}
static void SetupImage(void)
{
    TakedownImage();

    GFX.Pitch       = SNES_WIDTH * 2 * 2;
    GUI.snes_buffer = (uint8 *)calloc(GFX.Pitch * ((SNES_HEIGHT_EXTENDED + 4) * 2), 1);
    if (!GUI.snes_buffer)
        FatalError("Failed to allocate GUI.snes_buffer.");

    GFX.Screen            = (uint16 *)(GUI.snes_buffer + (GFX.Pitch * 2 * 2));
    GUI.blit_screen       = (uint8 *)GUI.sdl_screen->pixels;
    GUI.blit_screen_pitch = SNES_WIDTH * 2 * 2;
    g_snes->renderer->S9xGraphicsInit();
}
static void TakedownImage(void)
{
    if (GUI.snes_buffer) {
        free(GUI.snes_buffer);
        GUI.snes_buffer = NULL;
    }
    g_snes->renderer->S9xGraphicsDeinit();
}
void S9xPutImage(int width, int height)
{
    static int prevWidth = 0, prevHeight = 0;
    int        copyWidth, copyHeight;
    Blitter    blitFn = NULL;

    if (GUI.video_mode == VIDEOMODE_BLOCKY || GUI.video_mode == VIDEOMODE_TV || GUI.video_mode == VIDEOMODE_SMOOTH)
        if ((width <= SNES_WIDTH) && ((prevWidth != width) || (prevHeight != height)))
            S9xBlitClearDelta();

    copyWidth  = width;
    copyHeight = height;
    blitFn     = S9xBlitPixSimple1x1;
    blitFn((uint8 *)GFX.Screen, GFX.Pitch, GUI.blit_screen, GUI.blit_screen_pitch, width, height);

    if (height < prevHeight) {
        int p = GUI.blit_screen_pitch >> 2;
        for (int y = SNES_HEIGHT; y < SNES_HEIGHT_EXTENDED; y++) {
            uint32 *d = (uint32 *)(GUI.blit_screen + y * GUI.blit_screen_pitch);
            for (int x = 0; x < p; x++)
                *d++ = 0;
        }
    }

    Repaint(TRUE);
    prevWidth  = width;
    prevHeight = height;
}
static void Repaint(bool8 isFrameBoundry)
{
    SDL_RenderClear(GUI.sdl_renderer);
    GUI.sdl_texture = SDL_CreateTextureFromSurface(GUI.sdl_renderer, GUI.sdl_screen);
    SDL_RenderCopy(GUI.sdl_renderer, GUI.sdl_texture, NULL, &sdl_sizescreen);
    SDL_RenderPresent(GUI.sdl_renderer);
}
void S9xTextMode(void)
{
}
void S9xGraphicsMode(void)
{
}
const char *S9xSelectFilename(const char *def, const char *dir1, const char *ext1, const char *title)
{
    return (NULL);
}
void S9xMessage(int type, int number, const char *message)
{
    const int   max = 36 * 3;
    static char buffer[max + 1];
    fprintf(stdout, "%s\n", message);
    strncpy(buffer, message, max + 1);
    buffer[max] = 0;
    g_snes->renderer->S9xSetInfoString(buffer);
}
const char *S9xStringInput(const char *message)
{
    static char buffer[256];

    printf("%s: ", message);
    fflush(stdout);

    if (fgets(buffer, sizeof(buffer) - 2, stdin))
        return (buffer);

    return (NULL);
}
