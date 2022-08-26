#define MITSHM
#define USE_XVIDEO
#define USE_XINERAMA
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

#include <strings.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <X11/extensions/Xvlib.h>

#include <X11/extensions/Xinerama.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#include "snes.h"
#include "mem.h"
#include "ppu.h"



static uint8 *XDelta = NULL;


struct Image
{
    union
    {
        XvImage *xvimage;
        XImage  *ximage;
    };
    char  *data;
    uint32 height;
    uint32 data_size;
    uint32 bits_per_pixel;
    uint32 bytes_per_line;
};
struct GUIData
{
    Display        *display;
    Screen         *screen;
    Visual         *visual;
    GC              gc;
    int             screen_num;
    int             depth;
    int             pixel_format;
    int             bytes_per_pixel;
    uint32          red_shift;
    uint32          blue_shift;
    uint32          green_shift;
    uint32          red_size;
    uint32          green_size;
    uint32          blue_size;
    Window          window;
    Image          *image;
    uint8          *filter_buffer;
    uint8          *blit_screen;
    uint32          blit_screen_pitch;
    bool8           need_convert;
    Cursor          point_cursor;
    Cursor          cross_hair_cursor;
    int             video_mode;
    int             mouse_x;
    int             mouse_y;
    bool8           mod1_pressed;
    bool8           no_repeat;
    bool8           fullscreen;
    bool8           js_event_latch;
    int             x_offset;
    int             y_offset;
    bool8           use_xvideo;
    int             xv_port;
    int             scale_w;
    int             scale_h;
    bool8           maxaspect;
    int             imageHeight;
    int             xv_format;
    int             xv_bpp;
    unsigned char   y_table[1 << 15];
    unsigned char   u_table[1 << 15];
    unsigned char   v_table[1 << 15];
    uint32          xinerama_head;
    XShmSegmentInfo sm_info;
    bool8           use_shared_memory;
};

static struct GUIData                       GUI;
typedef std::pair<std::string, std::string> strpair_t;
extern std::vector<strpair_t>               keymaps;
typedef void (*Blitter)(uint8 *, int, uint8 *, int, int, int);
#ifdef __linux
#define SELECT_BROKEN_FOR_SIGNALS
#endif

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
static int   ErrorHandler(Display *, XErrorEvent *);
static bool8 CheckForPendingXEvents(Display *);
static void  SetXRepeat(bool8);
static void  SetupImage(void);
static void  TakedownImage(void);
static void  SetupXImage(void);
static void  Repaint(bool8);
static void  Convert16To24(int, int);


const char *S9xParseDisplayConfig(int pass)
{
    if (pass != 1)
        return ("Unix/X11");
    GUI.no_repeat  = TRUE;
    GUI.fullscreen = FALSE;
    GUI.video_mode = VIDEOMODE_BLOCKY;
    return ("Unix/X11");
}
static void FatalError(const char *str)
{
    fprintf(stderr, "%s\n", str);
    g_snes->S9xExit();
}
static int ErrorHandler(Display *display, XErrorEvent *event)
{
    GUI.use_shared_memory = FALSE;
    return (0);
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
    GUI.display = XOpenDisplay(NULL);
    if (GUI.display == NULL)
        FatalError("Failed to connect to X server.");

    GUI.screen     = DefaultScreenOfDisplay(GUI.display);
    GUI.screen_num = XScreenNumberOfScreen(GUI.screen);
    GUI.visual     = DefaultVisualOfScreen(GUI.screen);
    XVisualInfo plate, *matches;
    int         count;
    plate.visualid = XVisualIDFromVisual(GUI.visual);
    matches        = XGetVisualInfo(GUI.display, VisualIDMask, &plate, &count);

    if (!count)
        FatalError("Your X Window System server is unwell!");

    GUI.depth = matches[0].depth;

    if ((GUI.depth != 15 && GUI.depth != 16 && GUI.depth != 24) || (matches[0].c_class != TrueColor))
        FatalError("Requiers 15, 16, 24 or 32-bit color depth supporting TrueColor.");

    GUI.red_shift   = ffs(matches[0].red_mask) - 1;
    GUI.green_shift = ffs(matches[0].green_mask) - 1;
    GUI.blue_shift  = ffs(matches[0].blue_mask) - 1;
    GUI.red_size    = matches[0].red_mask >> GUI.red_shift;
    GUI.green_size  = matches[0].green_mask >> GUI.green_shift;
    GUI.blue_size   = matches[0].blue_mask >> GUI.blue_shift;
    if (GUI.depth == 16 && GUI.green_size == 63)
        GUI.green_shift++;

    XFree(matches);
    S9xBlitFilterInit();
    XSetWindowAttributes attrib;
    memset(&attrib, 0, sizeof(attrib));
    attrib.background_pixel = BlackPixelOfScreen(GUI.screen);
    attrib.colormap         = XCreateColormap(GUI.display, RootWindowOfScreen(GUI.screen), GUI.visual, AllocNone);
    int                 screen_left = 0, screen_top = 0;
    int                 screen_w = WidthOfScreen(GUI.screen), screen_h = HeightOfScreen(GUI.screen);
    int                 heads = 0;
    XineramaScreenInfo *si    = 0;
    int                 useless1, useless2;
    if (!XineramaQueryExtension(GUI.display, &useless1, &useless2)) {
        puts("Xinerama is not available");
        goto xinerama_end;
    }
    if (!XineramaIsActive(GUI.display)) {
        puts("Xinerama is not active");
        goto xinerama_end;
    }
    si = XineramaQueryScreens(GUI.display, &heads);
    if (!si) {
        puts("XineramaQueryScreens failed");
        goto xinerama_end;
    }
    if (GUI.xinerama_head >= heads) {
        printf("Invalid xinerama head id (expected 0-%d, got %u)\n", heads - 1, GUI.xinerama_head);
        goto xinerama_end;
    }
    si          = &si[GUI.xinerama_head];
    screen_left = si->x_org;
    screen_top  = si->y_org;
    screen_w    = si->width;
    screen_h    = si->height;
    printf("Selected xinerama head %u (%d,%d %dx%d)\n", GUI.xinerama_head, screen_left, screen_top, screen_w, screen_h);
xinerama_end:

    XSizeHints Hints;
    memset((void *)&Hints, 0, sizeof(XSizeHints));
    if (GUI.fullscreen == TRUE) {
        Hints.flags = PPosition;
        Hints.x     = screen_left;
        Hints.y     = screen_top;
        GUI.window = XCreateWindow(GUI.display, RootWindowOfScreen(GUI.screen), Hints.x, Hints.y, screen_w, screen_h, 0,
                                   GUI.depth, InputOutput, GUI.visual, CWBackPixel | CWColormap, &attrib);
        Atom wm_state      = XInternAtom(GUI.display, "_NET_WM_STATE", true);
        Atom wm_fullscreen = XInternAtom(GUI.display, "_NET_WM_STATE_FULLSCREEN", true);
        XChangeProperty(GUI.display, GUI.window, wm_state, XA_ATOM, 32, PropModeReplace,
                        (unsigned char *)&wm_fullscreen, 1);
        if (GUI.use_xvideo) {
            GUI.scale_w     = screen_w;
            GUI.scale_h     = screen_h;
            GUI.imageHeight = SNES_HEIGHT_EXTENDED * 2;
            if (!GUI.maxaspect) {
                double screenAspect = (double)screen_w / screen_h;
                double snesAspect   = (double)SNES_WIDTH / SNES_HEIGHT_EXTENDED;
                double ratio        = screenAspect / snesAspect;
                printf("\tScreen (%dx%d) aspect %f vs SNES (%dx%d) aspect %f (ratio: %f)\n", screen_w, screen_h,
                       screenAspect, SNES_WIDTH, SNES_HEIGHT_EXTENDED, snesAspect, ratio);
                if (screenAspect > snesAspect) {
                    GUI.scale_w /= ratio;
                    GUI.x_offset = (screen_w - GUI.scale_w) / 2;
                } else {
                    GUI.scale_h *= ratio;
                    GUI.y_offset = (screen_h - GUI.scale_h) / 2;
                }
            }
            printf("\tUsing size %dx%d with offset (%d,%d)\n", GUI.scale_w, GUI.scale_h, GUI.x_offset, GUI.y_offset);
        } else {
            GUI.x_offset = (screen_w - SNES_WIDTH * 2) / 2;
            GUI.y_offset = (screen_h - SNES_HEIGHT_EXTENDED * 2) / 2;
        }
    } else {
        Hints.flags     = PSize | PMinSize | PMaxSize | PPosition;
        Hints.x         = screen_left + (screen_w - SNES_WIDTH * 2) / 1;
        Hints.y         = screen_top + (screen_h - SNES_HEIGHT_EXTENDED * 2) / 1;
        Hints.min_width = Hints.max_width = Hints.base_width = SNES_WIDTH * 1;
        Hints.min_height = Hints.max_height = Hints.base_height = SNES_HEIGHT_EXTENDED * 1;

        GUI.window   = XCreateWindow(GUI.display, RootWindowOfScreen(GUI.screen), Hints.x, Hints.y, SNES_WIDTH * 1,
                                     SNES_HEIGHT_EXTENDED * 1, 0, GUI.depth, InputOutput, GUI.visual,
                                     CWBackPixel | CWColormap, &attrib);
        GUI.x_offset = GUI.y_offset = 0;
        GUI.scale_w                 = SNES_WIDTH * 1;
        GUI.scale_h                 = SNES_HEIGHT_EXTENDED * 1;
    }
    XSetWMNormalHints(GUI.display, GUI.window, &Hints);
    static XColor bg, fg;
    static char   data[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    Pixmap        bitmap;
    bitmap           = XCreateBitmapFromData(GUI.display, GUI.window, data, 8, 8);
    GUI.point_cursor = XCreatePixmapCursor(GUI.display, bitmap, bitmap, &fg, &bg, 0, 0);
    XDefineCursor(GUI.display, GUI.window, GUI.point_cursor);
    GUI.cross_hair_cursor = XCreateFontCursor(GUI.display, XC_crosshair);
    GUI.gc                = DefaultGCOfScreen(GUI.screen);
    XWMHints WMHints;
    memset((void *)&WMHints, 0, sizeof(XWMHints));
    WMHints.input = True;
    WMHints.flags = InputHint;
    XSetWMHints(GUI.display, GUI.window, &WMHints);
    XSelectInput(GUI.display, GUI.window,
                 FocusChangeMask | ExposureMask | KeyPressMask | KeyReleaseMask | StructureNotifyMask |
                     ButtonPressMask | ButtonReleaseMask);
    XMapRaised(GUI.display, GUI.window);
    XClearWindow(GUI.display, GUI.window);
    XEvent event;
    do {
        XNextEvent(GUI.display, &event);
    } while (event.type != MapNotify || event.xmap.event != GUI.window);

    GUI.pixel_format = 565;
    SetupImage();
    switch (GUI.depth) {
        default:
        case 32:
            GUI.bytes_per_pixel = 4;
            break;
        case 24:
            if (GUI.image->bits_per_pixel == 24)
                GUI.bytes_per_pixel = 3;
            else
                GUI.bytes_per_pixel = 4;
            break;
        case 15:
        case 16:
            GUI.bytes_per_pixel = 2;
            break;
    }
    printf("Using internal pixel format %d\n", GUI.pixel_format);
}
void S9xDeinitDisplay(void)
{
    TakedownImage();
    if (GUI.display != NULL) {
        if (GUI.use_xvideo) {
            XvUngrabPort(GUI.display, GUI.xv_port, CurrentTime);
        }
        S9xTextMode();
        XSync(GUI.display, False);
        XCloseDisplay(GUI.display);
    }
    S9xBlitFilterDeinit();
}
static void SetupImage(void)
{
    TakedownImage();
    GUI.image = (Image *)calloc(sizeof(Image), 1);
    if (!GUI.use_xvideo)
        SetupXImage();

    GUI.filter_buffer = (uint8 *)calloc((SNES_WIDTH * 2) * 2 * (SNES_HEIGHT_EXTENDED * 2), 1);
    if (!GUI.filter_buffer)
        FatalError("Failed to allocate GUI.filter_buffer.");

    if ((GUI.depth == 15 || GUI.depth == 16) && GUI.xv_format != FOURCC_YUY2 && GUI.xv_format != FOURCC_I420) {
        GUI.blit_screen_pitch = GUI.image->bytes_per_line;
        GUI.blit_screen       = (uint8 *)GUI.image->data;
        GUI.need_convert      = FALSE;
    } else {
        GUI.blit_screen_pitch = (SNES_WIDTH * 2) * 2;
        GUI.blit_screen       = GUI.filter_buffer;
        GUI.need_convert      = TRUE;
    }
    if (GUI.need_convert) {
        printf("\tImage conversion needed before blit.\n");
    }
    g_snes->renderer->S9xGraphicsInit();
}
static void TakedownImage(void)
{
    if (GUI.filter_buffer) {
        free(GUI.filter_buffer);
        GUI.filter_buffer = NULL;
    }
    if (GUI.image) {
        free(GUI.image);
        GUI.image = NULL;
    }
    g_snes->renderer->S9xGraphicsDeinit();
}
static void SetupXImage(void)
{
    GUI.use_shared_memory = TRUE;
    int  major, minor;
    Bool shared;
    if (!XShmQueryVersion(GUI.display, &major, &minor, &shared) || !shared)
        GUI.image->ximage = NULL;
    else
        GUI.image->ximage = XShmCreateImage(GUI.display, GUI.visual, GUI.depth, ZPixmap, NULL, &GUI.sm_info,
                                            SNES_WIDTH * 2, SNES_HEIGHT_EXTENDED * 2);
    if (!GUI.image->ximage)
        GUI.use_shared_memory = FALSE;
    else {
        GUI.image->height         = GUI.image->ximage->height;
        GUI.image->bytes_per_line = GUI.image->ximage->bytes_per_line;
        GUI.image->data_size      = GUI.image->bytes_per_line * GUI.image->height;
        GUI.sm_info.shmid         = shmget(IPC_PRIVATE, GUI.image->data_size, IPC_CREAT | 0777);
        if (GUI.sm_info.shmid < 0) {
            XDestroyImage(GUI.image->ximage);
            GUI.use_shared_memory = FALSE;
        } else {
            GUI.image->ximage->data = GUI.sm_info.shmaddr = (char *)shmat(GUI.sm_info.shmid, 0, 0);
            if (!GUI.image->ximage->data) {
                XDestroyImage(GUI.image->ximage);
                shmctl(GUI.sm_info.shmid, IPC_RMID, 0);
                GUI.use_shared_memory = FALSE;
            } else {
                GUI.sm_info.readOnly = False;
                XSetErrorHandler(ErrorHandler);
                XShmAttach(GUI.display, &GUI.sm_info);
                XSync(GUI.display, False);
                if (!GUI.use_shared_memory) {
                    XDestroyImage(GUI.image->ximage);
                    shmdt(GUI.sm_info.shmaddr);
                    shmctl(GUI.sm_info.shmid, IPC_RMID, 0);
                } else
                    printf("Created XShmImage, size %d\n", GUI.image->data_size);
            }
        }
    }
    if (!GUI.use_shared_memory) {
        fprintf(stderr, "use_shared_memory failed, switching to XPutImage.\n");
        GUI.image->ximage         = XCreateImage(GUI.display, GUI.visual, GUI.depth, ZPixmap, 0, NULL, SNES_WIDTH * 2,
                                                 SNES_HEIGHT_EXTENDED * 2, BitmapUnit(GUI.display), 0);
        GUI.image->height         = GUI.image->ximage->height;
        GUI.image->bytes_per_line = GUI.image->ximage->bytes_per_line;
        GUI.image->data_size      = GUI.image->bytes_per_line * GUI.image->height;
        GUI.image->ximage->data   = (char *)malloc(GUI.image->data_size);
        if (!GUI.image->ximage || !GUI.image->ximage->data)
            FatalError("XCreateImage failed.");
        printf("Created XImage, size %d\n", GUI.image->data_size);
    }
    GUI.image->bits_per_pixel     = GUI.image->ximage->bits_per_pixel;
    GUI.image->data               = GUI.image->ximage->data;
    GUI.image->ximage->byte_order = LSBFirst;
}
void S9xPutImage(int width, int height)
{
    int     prevWidth = 0, prevHeight = 0;
    int     copyWidth, copyHeight;
    Blitter blitFn = NULL;
    if (GUI.video_mode == VIDEOMODE_BLOCKY || GUI.video_mode == VIDEOMODE_TV || GUI.video_mode == VIDEOMODE_SMOOTH)
        if ((width <= SNES_WIDTH) && ((prevWidth != width) || (prevHeight != height)))
            S9xBlitClearDelta();

    copyWidth  = width;
    copyHeight = height;
    blitFn     = S9xBlitPixSimple1x1;
    blitFn((uint8 *)GFX.Screen, GFX.Pitch, GUI.blit_screen, GUI.blit_screen_pitch, width, height);

    if (height < prevHeight) {
        int p = GUI.blit_screen_pitch >> 2;
        for (int y = SNES_HEIGHT * 2; y < SNES_HEIGHT_EXTENDED * 2; y++) {
            uint32 *d = (uint32 *)(GUI.blit_screen + y * GUI.blit_screen_pitch);
            for (int x = 0; x < p; x++)
                *d++ = 0;
        }
    }

    if (height <= SNES_HEIGHT_EXTENDED)
        GUI.imageHeight = height * 2;
    if (GUI.use_xvideo && (GUI.xv_format == FOURCC_YUY2)) {
        uint16 *s = (uint16 *)GUI.blit_screen;
        uint8  *d = (uint8 *)GUI.image->data;
        for (int y = 0; y < copyHeight; y++) {
            for (int x = 0; x < SNES_WIDTH * 2; x += 2) {
                unsigned short rgb1 = (*s & 0xFFC0) >> 1 | (*s & 0x001F);
                s++;
                unsigned short rgb2 = (*s & 0xFFC0) >> 1 | (*s & 0x001F);
                s++;
                *d = GUI.y_table[rgb1];
                d++;
                *d = (GUI.u_table[rgb1] + GUI.u_table[rgb2]) / 2;
                d++;
                *d = GUI.y_table[rgb2];
                d++;
                *d = (GUI.v_table[rgb1] + GUI.v_table[rgb2]) / 2;
                d++;
            }
        }
    } else if (GUI.use_xvideo && (GUI.xv_format == FOURCC_I420)) {
        uint8 *s  = (uint8 *)GUI.blit_screen;
        uint8 *d  = (uint8 *)GUI.image->data;
        int    tw = SNES_WIDTH * 2;
        int    th = SNES_HEIGHT_EXTENDED * 2;
        uint8 *ns = (uint8 *)calloc(tw * th * 3, 1);
        free(ns);
        GUI.need_convert = 0;
    } else if (GUI.need_convert) {
        Convert16To24(copyWidth, copyHeight);
    }
    Repaint(TRUE);
    prevWidth  = width;
    prevHeight = height;
}
static void Convert16To24(int width, int height)
{
    if (GUI.pixel_format == 565) {
        for (int y = 0; y < height; y++) {
            uint16 *s = (uint16 *)(GUI.blit_screen + y * GUI.blit_screen_pitch);
            uint32 *d = (uint32 *)(GUI.image->data + y * GUI.image->bytes_per_line);
            for (int x = 0; x < width; x++) {
                uint32 pixel = *s++;
                *d++         = (((pixel >> 11) & 0x1f) << (GUI.red_shift + 3)) |
                       (((pixel >> 6) & 0x1f) << (GUI.green_shift + 3)) | ((pixel & 0x1f) << (GUI.blue_shift + 3));
            }
        }
    } else {
        for (int y = 0; y < height; y++) {
            uint16 *s = (uint16 *)(GUI.blit_screen + y * GUI.blit_screen_pitch);
            uint32 *d = (uint32 *)(GUI.image->data + y * GUI.image->bytes_per_line);
            for (int x = 0; x < width; x++) {
                uint32 pixel = *s++;
                *d++         = (((pixel >> 10) & 0x1f) << (GUI.red_shift + 3)) |
                       (((pixel >> 5) & 0x1f) << (GUI.green_shift + 3)) | ((pixel & 0x1f) << (GUI.blue_shift + 3));
            }
        }
    }
}
static void Repaint(bool8 isFrameBoundry)
{
    if (GUI.use_xvideo) {
        if (GUI.use_shared_memory) {
            XvShmPutImage(GUI.display, GUI.xv_port, GUI.window, GUI.gc, GUI.image->xvimage, 0, 0, SNES_WIDTH * 2,
                          GUI.imageHeight, GUI.x_offset, GUI.y_offset, GUI.scale_w, GUI.scale_h, False);
        } else
            XvPutImage(GUI.display, GUI.xv_port, GUI.window, GUI.gc, GUI.image->xvimage, 0, 0, SNES_WIDTH * 2,
                       GUI.imageHeight, GUI.x_offset, GUI.y_offset, GUI.scale_w, GUI.scale_h);
    } else if (GUI.use_shared_memory) {
        XShmPutImage(GUI.display, GUI.window, GUI.gc, GUI.image->ximage, 0, 0, GUI.x_offset, GUI.y_offset,
                     SNES_WIDTH * 2, SNES_HEIGHT_EXTENDED * 2, False);
        XSync(GUI.display, False);
    } else
        XPutImage(GUI.display, GUI.window, GUI.gc, GUI.image->ximage, 0, 0, GUI.x_offset, GUI.y_offset, SNES_WIDTH * 2,
                  SNES_HEIGHT_EXTENDED * 2);
    Window       root, child;
    int          root_x, root_y, x, y;
    unsigned int mask;
    XQueryPointer(GUI.display, GUI.window, &root, &child, &root_x, &root_y, &x, &y, &mask);
    if (x >= 0 && y >= 0 && x < SNES_WIDTH * 2 && y < SNES_HEIGHT_EXTENDED * 2) {
        GUI.mouse_x = x >> 1;
        GUI.mouse_y = y >> 1;
        if (mask & Mod1Mask) {
            if (!GUI.mod1_pressed) {
                GUI.mod1_pressed = TRUE;
                XDefineCursor(GUI.display, GUI.window, GUI.cross_hair_cursor);
            }
        } else if (GUI.mod1_pressed) {
            GUI.mod1_pressed = FALSE;
            XDefineCursor(GUI.display, GUI.window, GUI.point_cursor);
        }
    }
}
void S9xTextMode(void)
{
    SetXRepeat(TRUE);
}
void S9xGraphicsMode(void)
{
    SetXRepeat(FALSE);
}
static bool8 CheckForPendingXEvents(Display *display)
{
    int arg = 0;
    return (XEventsQueued(display, QueuedAlready) || (ioctl(ConnectionNumber(display), FIONREAD, &arg) == 0 && arg));
}
void S9xLatchJSEvent()
{
    GUI.js_event_latch = TRUE;
}
void S9xProcessEvents(bool8 block)
{
    if (GUI.js_event_latch == TRUE) {
        XWarpPointer(GUI.display, None, None, 0, 0, 0, 0, 0, 0);
        GUI.js_event_latch = FALSE;
    }
    while (block || CheckForPendingXEvents(GUI.display)) {
        XEvent event;
        XNextEvent(GUI.display, &event);
        block = FALSE;
        switch (event.type) {
            case KeyPress:
            case KeyRelease:
                break;
            case FocusIn:
                SetXRepeat(FALSE);
                XFlush(GUI.display);
                break;
            case FocusOut:
                SetXRepeat(TRUE);
                XFlush(GUI.display);
                break;
            case ConfigureNotify:
                break;
            case ButtonPress:
            case ButtonRelease:
                break;
            case Expose:
                Repaint(FALSE);
                break;
        }
    }
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
static void SetXRepeat(bool8 state)
{
    if (state)
        XAutoRepeatOn(GUI.display);
    else
        XAutoRepeatOff(GUI.display);
}
