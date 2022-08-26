
#define _PORT_H_
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>

#include <memory.h>

#include <time.h>
#include <string.h>
#include <sys/types.h>
#define PIXEL_FORMAT RGB565

#define alwaysinline inline __attribute__((always_inline))
#define snes9x_types_defined
typedef unsigned char  bool8;
typedef signed char    int8;
typedef unsigned char  uint8;
typedef signed short   int16;
typedef unsigned short uint16;
typedef signed int     int32;
typedef unsigned int   uint32;

typedef long long          int64;
typedef unsigned long long uint64;
typedef size_t             pint;
#define TRUE  1
#define FALSE 0

#define START_EXTERN_C extern "C" {
#define END_EXTERN_C   }

#define _MAX_DRIVE 1
#define _MAX_DIR   1024
#define _MAX_FNAME 1024
#define _MAX_EXT   1024
#define _MAX_PATH  1024
void _splitpath(const char *, char *, char *, char *, char *);
void _makepath(char *, const char *, const char *, const char *, const char *);
#define S9xDisplayString DisplayStringFromBottom
#define SLASH_STR        "/"
#define SLASH_CHAR       '/'
#define SIG_PF           void (*)(int)
#define TITLE            "Snes9x: Linux"
#define SYS_CONFIG_FILE  "/etc/snes9x/snes9x.conf"

#define LSB_FIRST
#define FAST_LSB_WORD_ACCESS
#define READ_WORD(s)      (*(uint16 *)(s))
#define READ_3WORD(s)     (*(uint32 *)(s)&0x00ffffff)
#define READ_DWORD(s)     (*(uint32 *)(s))
#define WRITE_WORD(s, d)  *(uint16 *)(s) = (d)
#define WRITE_3WORD(s, d) *(uint16 *)(s) = (uint16)(d), *((uint8 *)(s) + 2) = (uint8)((d) >> 16)
#define WRITE_DWORD(s, d) *(uint32 *)(s) = (d)
#define SWAP_WORD(s)      (s) = (((s)&0xff) << 8) | (((s)&0xff00) >> 8)
#define SWAP_DWORD(s)     (s) = (((s)&0xff) << 24) | (((s)&0xff00) << 8) | (((s)&0xff0000) >> 8) | (((s)&0xff000000) >> 24)

#define _PIXFORM_H_
#define BUILD_PIXEL_RGB565(R, G, B)  (((int)(R) << 11) | ((int)(G) << 6) | (((int)(G)&0x10) << 1) | (int)(B))
#define BUILD_PIXEL2_RGB565(R, G, B) (((int)(R) << 11) | ((int)(G) << 5) | (int)(B))
#define DECOMPOSE_PIXEL_RGB565(PIX, R, G, B)                                                                           \
    {                                                                                                                  \
        (R) = (PIX) >> 11;                                                                                             \
        (G) = ((PIX) >> 6) & 0x1f;                                                                                     \
        (B) = (PIX)&0x1f;                                                                                              \
    }
#define SPARE_RGB_BIT_MASK_RGB565    (1 << 5)
#define MAX_RED_RGB565               31
#define MAX_GREEN_RGB565             63
#define MAX_BLUE_RGB565              31
#define RED_SHIFT_BITS_RGB565        11
#define GREEN_SHIFT_BITS_RGB565      6
#define RED_LOW_BIT_MASK_RGB565      0x0800
#define GREEN_LOW_BIT_MASK_RGB565    0x0020
#define BLUE_LOW_BIT_MASK_RGB565     0x0001
#define RED_HI_BIT_MASK_RGB565       0x8000
#define GREEN_HI_BIT_MASK_RGB565     0x0400
#define BLUE_HI_BIT_MASK_RGB565      0x0010
#define FIRST_COLOR_MASK_RGB565      0xF800
#define SECOND_COLOR_MASK_RGB565     0x07E0
#define THIRD_COLOR_MASK_RGB565      0x001F
#define ALPHA_BITS_MASK_RGB565       0x0000
#define BUILD_PIXEL_RGB555(R, G, B)  (((int)(R) << 10) | ((int)(G) << 5) | (int)(B))
#define BUILD_PIXEL2_RGB555(R, G, B) (((int)(R) << 10) | ((int)(G) << 5) | (int)(B))
#define DECOMPOSE_PIXEL_RGB555(PIX, R, G, B)                                                                           \
    {                                                                                                                  \
        (R) = (PIX) >> 10;                                                                                             \
        (G) = ((PIX) >> 5) & 0x1f;                                                                                     \
        (B) = (PIX)&0x1f;                                                                                              \
    }
#define SPARE_RGB_BIT_MASK_RGB555          (1 << 15)
#define MAX_RED_RGB555                     31
#define MAX_GREEN_RGB555                   31
#define MAX_BLUE_RGB555                    31
#define RED_SHIFT_BITS_RGB555              10
#define GREEN_SHIFT_BITS_RGB555            5
#define RED_LOW_BIT_MASK_RGB555            0x0400
#define GREEN_LOW_BIT_MASK_RGB555          0x0020
#define BLUE_LOW_BIT_MASK_RGB555           0x0001
#define RED_HI_BIT_MASK_RGB555             0x4000
#define GREEN_HI_BIT_MASK_RGB555           0x0200
#define BLUE_HI_BIT_MASK_RGB555            0x0010
#define FIRST_COLOR_MASK_RGB555            0x7C00
#define SECOND_COLOR_MASK_RGB555           0x03E0
#define THIRD_COLOR_MASK_RGB555            0x001F
#define ALPHA_BITS_MASK_RGB555             0x0000
#define CONCAT(X, Y)                       X##Y
#define BUILD_PIXEL_D(F, R, G, B)          CONCAT(BUILD_PIXEL_, F)(R, G, B)
#define BUILD_PIXEL2_D(F, R, G, B)         CONCAT(BUILD_PIXEL2_, F)(R, G, B)
#define DECOMPOSE_PIXEL_D(F, PIX, R, G, B) CONCAT(DECOMPOSE_PIXEL_, F)(PIX, R, G, B)
#define BUILD_PIXEL(R, G, B)               BUILD_PIXEL_D(PIXEL_FORMAT, R, G, B)
#define BUILD_PIXEL2(R, G, B)              BUILD_PIXEL2_D(PIXEL_FORMAT, R, G, B)
#define DECOMPOSE_PIXEL(PIX, R, G, B)      DECOMPOSE_PIXEL_D(PIXEL_FORMAT, PIX, R, G, B)
#define MAX_RED_D(F)                       CONCAT(MAX_RED_, F)
#define MAX_GREEN_D(F)                     CONCAT(MAX_GREEN_, F)
#define MAX_BLUE_D(F)                      CONCAT(MAX_BLUE_, F)
#define RED_SHIFT_BITS_D(F)                CONCAT(RED_SHIFT_BITS_, F)
#define GREEN_SHIFT_BITS_D(F)              CONCAT(GREEN_SHIFT_BITS_, F)
#define RED_LOW_BIT_MASK_D(F)              CONCAT(RED_LOW_BIT_MASK_, F)
#define GREEN_LOW_BIT_MASK_D(F)            CONCAT(GREEN_LOW_BIT_MASK_, F)
#define BLUE_LOW_BIT_MASK_D(F)             CONCAT(BLUE_LOW_BIT_MASK_, F)
#define RED_HI_BIT_MASK_D(F)               CONCAT(RED_HI_BIT_MASK_, F)
#define GREEN_HI_BIT_MASK_D(F)             CONCAT(GREEN_HI_BIT_MASK_, F)
#define BLUE_HI_BIT_MASK_D(F)              CONCAT(BLUE_HI_BIT_MASK_, F)
#define FIRST_COLOR_MASK_D(F)              CONCAT(FIRST_COLOR_MASK_, F)
#define SECOND_COLOR_MASK_D(F)             CONCAT(SECOND_COLOR_MASK_, F)
#define THIRD_COLOR_MASK_D(F)              CONCAT(THIRD_COLOR_MASK_, F)
#define ALPHA_BITS_MASK_D(F)               CONCAT(ALPHA_BITS_MASK_, F)
#define MAX_RED                            MAX_RED_D(PIXEL_FORMAT)
#define MAX_GREEN                          MAX_GREEN_D(PIXEL_FORMAT)
#define MAX_BLUE                           MAX_BLUE_D(PIXEL_FORMAT)
#define RED_SHIFT_BITS                     RED_SHIFT_BITS_D(PIXEL_FORMAT)
#define GREEN_SHIFT_BITS                   GREEN_SHIFT_BITS_D(PIXEL_FORMAT)
#define RED_LOW_BIT_MASK                   RED_LOW_BIT_MASK_D(PIXEL_FORMAT)
#define GREEN_LOW_BIT_MASK                 GREEN_LOW_BIT_MASK_D(PIXEL_FORMAT)
#define BLUE_LOW_BIT_MASK                  BLUE_LOW_BIT_MASK_D(PIXEL_FORMAT)
#define RED_HI_BIT_MASK                    RED_HI_BIT_MASK_D(PIXEL_FORMAT)
#define GREEN_HI_BIT_MASK                  GREEN_HI_BIT_MASK_D(PIXEL_FORMAT)
#define BLUE_HI_BIT_MASK                   BLUE_HI_BIT_MASK_D(PIXEL_FORMAT)
#define FIRST_COLOR_MASK                   FIRST_COLOR_MASK_D(PIXEL_FORMAT)
#define SECOND_COLOR_MASK                  SECOND_COLOR_MASK_D(PIXEL_FORMAT)
#define THIRD_COLOR_MASK                   THIRD_COLOR_MASK_D(PIXEL_FORMAT)
#define ALPHA_BITS_MASK                    ALPHA_BITS_MASK_D(PIXEL_FORMAT)
#define GREEN_HI_BIT                       ((MAX_GREEN + 1) >> 1)
#define RGB_LOW_BITS_MASK                  (RED_LOW_BIT_MASK | GREEN_LOW_BIT_MASK | BLUE_LOW_BIT_MASK)
#define RGB_HI_BITS_MASK                   (RED_HI_BIT_MASK | GREEN_HI_BIT_MASK | BLUE_HI_BIT_MASK)
#define RGB_HI_BITS_MASKx2                 ((RED_HI_BIT_MASK | GREEN_HI_BIT_MASK | BLUE_HI_BIT_MASK) << 1)
#define RGB_REMOVE_LOW_BITS_MASK           (~RGB_LOW_BITS_MASK)
#define FIRST_THIRD_COLOR_MASK             (FIRST_COLOR_MASK | THIRD_COLOR_MASK)
#define TWO_LOW_BITS_MASK                  (RGB_LOW_BITS_MASK | (RGB_LOW_BITS_MASK << 1))
#define HIGH_BITS_SHIFTED_TWO_MASK                                                                                     \
    (((FIRST_COLOR_MASK | SECOND_COLOR_MASK | THIRD_COLOR_MASK) & ~TWO_LOW_BITS_MASK) >> 2)
