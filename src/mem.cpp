#include <string>
#include <numeric>
#include <assert.h>
#include <ctype.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdlib.h>

#include "snes.h"
#include "mem.h"
#include "apu/apu.h"
#include "ppu.h"

typedef unsigned char BYTE;
typedef unsigned int  WORD;
#ifndef SET_UI_COLOR
#define SET_UI_COLOR(r, g, b) ;
#endif
#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif
const uint32 crc32Table[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3, 0x0edb8832,
    0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7, 0x136c9856, 0x646ba8c0, 0xfd62f97a,
    0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3,
    0x45df5c75, 0xdcd60dcf, 0xabd13d59, 0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab,
    0xb6662d3d, 0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01, 0x6b6b51f4,
    0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65, 0x4db26158, 0x3ab551ce, 0xa3bc0074,
    0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525,
    0x206f85b3, 0xb966d409, 0xce61e49f, 0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615,
    0x73dc1683, 0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7, 0xfed41b76,
    0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b, 0xd80d2bda, 0xaf0a1b4c, 0x36034af6,
    0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7,
    0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d, 0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7,
    0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45, 0xa00ae278,
    0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9, 0xbdbdf21c, 0xcabac28a, 0x53b39330,
    0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d};
const WORD k[64] = {0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
                    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
                    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
                    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
                    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
                    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
                    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
                    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};


#define ROTLEFT(a, b)     (((a) << (b)) | ((a) >> (32 - (b))))
#define ROTRIGHT(a, b)    (((a) >> (b)) | ((a) << (32 - (b))))
#define CH(x, y, z)       (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z)      (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x)            (ROTRIGHT(x, 2) ^ ROTRIGHT(x, 13) ^ ROTRIGHT(x, 22))
#define EP1(x)            (ROTRIGHT(x, 6) ^ ROTRIGHT(x, 11) ^ ROTRIGHT(x, 25))
#define SIG0(x)           (ROTRIGHT(x, 7) ^ ROTRIGHT(x, 18) ^ ((x) >> 3))
#define SIG1(x)           (ROTRIGHT(x, 17) ^ ROTRIGHT(x, 19) ^ ((x) >> 10))
#define SHA256_BLOCK_SIZE 32


typedef struct
{
    BYTE     data[64];
    WORD     datalen;
    uint64_t bitlen;
    WORD     state[8];
} SHA256_CTX;
void sha256_transform(SHA256_CTX *ctx, const BYTE data[])
{
    WORD a, b, c, d, e, f, g, h, i, j, t1, t2, m[64];
    for (i = 0, j = 0; i < 16; ++i, j += 4)
        m[i] = (data[j] << 24) | (data[j + 1] << 16) | (data[j + 2] << 8) | (data[j + 3]);
    for (; i < 64; ++i)
        m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];
    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];
    for (i = 0; i < 64; ++i) {
        t1 = h + EP1(e) + CH(e, f, g) + k[i] + m[i];
        t2 = EP0(a) + MAJ(a, b, c);
        h  = g;
        g  = f;
        f  = e;
        e  = d + t1;
        d  = c;
        c  = b;
        b  = a;
        a  = t1 + t2;
    }
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}
void sha256_init(SHA256_CTX *ctx)
{
    ctx->datalen  = 0;
    ctx->bitlen   = 0;
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
}
void sha256_update(SHA256_CTX *ctx, const BYTE data[], size_t len)
{
    WORD i;
    for (i = 0; i < len; ++i) {
        ctx->data[ctx->datalen] = data[i];
        ctx->datalen++;
        if (ctx->datalen == 64) {
            sha256_transform(ctx, ctx->data);
            ctx->bitlen += 512;
            ctx->datalen = 0;
        }
    }
}
void sha256_final(SHA256_CTX *ctx, BYTE hash[])
{
    WORD i;
    i = ctx->datalen;
    if (ctx->datalen < 56) {
        ctx->data[i++] = 0x80;
        while (i < 56)
            ctx->data[i++] = 0x00;
    } else {
        ctx->data[i++] = 0x80;
        while (i < 64)
            ctx->data[i++] = 0x00;
        sha256_transform(ctx, ctx->data);
        memset(ctx->data, 0, 56);
    }
    ctx->bitlen += ctx->datalen * 8;
    ctx->data[63] = ctx->bitlen;
    ctx->data[62] = ctx->bitlen >> 8;
    ctx->data[61] = ctx->bitlen >> 16;
    ctx->data[60] = ctx->bitlen >> 24;
    ctx->data[59] = ctx->bitlen >> 32;
    ctx->data[58] = ctx->bitlen >> 40;
    ctx->data[57] = ctx->bitlen >> 48;
    ctx->data[56] = ctx->bitlen >> 56;
    sha256_transform(ctx, ctx->data);
    for (i = 0; i < 4; ++i) {
        hash[i]      = (ctx->state[0] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 4]  = (ctx->state[1] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 8]  = (ctx->state[2] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 12] = (ctx->state[3] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 16] = (ctx->state[4] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 20] = (ctx->state[5] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 24] = (ctx->state[6] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 28] = (ctx->state[7] >> (24 - i * 8)) & 0x000000ff;
    }
}

CMemory::CMemory(SNESX *_snes)
{
    snes = _snes;
}
bool8 CMemory::allASCII(uint8 *b, int size)
{
    for (int i = 0; i < size; i++) {
        if (b[i] < 32 || b[i] > 126)
            return (FALSE);
    }
    return (TRUE);
}
bool8 CMemory::Init(void)
{
    RAM  = (uint8 *)malloc(0x20000);
    SRAM = (uint8 *)malloc(0x80000);
    VRAM = (uint8 *)malloc(0x10000);
    ROM  = (uint8 *)malloc(MAX_ROM_SIZE + 0x200 + 0x8000);

    snes->ppu->IPPU.TileCache[TILE_2BIT]       = (uint8 *)malloc(MAX_2BIT_TILES * 64);
    snes->ppu->IPPU.TileCache[TILE_4BIT]       = (uint8 *)malloc(MAX_4BIT_TILES * 64);
    snes->ppu->IPPU.TileCache[TILE_8BIT]       = (uint8 *)malloc(MAX_8BIT_TILES * 64);
    snes->ppu->IPPU.TileCache[TILE_2BIT_EVEN]  = (uint8 *)malloc(MAX_2BIT_TILES * 64);
    snes->ppu->IPPU.TileCache[TILE_2BIT_ODD]   = (uint8 *)malloc(MAX_2BIT_TILES * 64);
    snes->ppu->IPPU.TileCache[TILE_4BIT_EVEN]  = (uint8 *)malloc(MAX_4BIT_TILES * 64);
    snes->ppu->IPPU.TileCache[TILE_4BIT_ODD]   = (uint8 *)malloc(MAX_4BIT_TILES * 64);
    snes->ppu->IPPU.TileCached[TILE_2BIT]      = (uint8 *)malloc(MAX_2BIT_TILES);
    snes->ppu->IPPU.TileCached[TILE_4BIT]      = (uint8 *)malloc(MAX_4BIT_TILES);
    snes->ppu->IPPU.TileCached[TILE_8BIT]      = (uint8 *)malloc(MAX_8BIT_TILES);
    snes->ppu->IPPU.TileCached[TILE_2BIT_EVEN] = (uint8 *)malloc(MAX_2BIT_TILES);
    snes->ppu->IPPU.TileCached[TILE_2BIT_ODD]  = (uint8 *)malloc(MAX_2BIT_TILES);
    snes->ppu->IPPU.TileCached[TILE_4BIT_EVEN] = (uint8 *)malloc(MAX_4BIT_TILES);
    snes->ppu->IPPU.TileCached[TILE_4BIT_ODD]  = (uint8 *)malloc(MAX_4BIT_TILES);

    if (!RAM || !SRAM || !VRAM || !ROM || !snes->ppu->IPPU.TileCache[TILE_2BIT] ||
        !snes->ppu->IPPU.TileCache[TILE_4BIT] || !snes->ppu->IPPU.TileCache[TILE_8BIT] ||
        !snes->ppu->IPPU.TileCache[TILE_2BIT_EVEN] || !snes->ppu->IPPU.TileCache[TILE_2BIT_ODD] ||
        !snes->ppu->IPPU.TileCache[TILE_4BIT_EVEN] || !snes->ppu->IPPU.TileCache[TILE_4BIT_ODD] ||
        !snes->ppu->IPPU.TileCached[TILE_2BIT] || !snes->ppu->IPPU.TileCached[TILE_4BIT] ||
        !snes->ppu->IPPU.TileCached[TILE_8BIT] || !snes->ppu->IPPU.TileCached[TILE_2BIT_EVEN] ||
        !snes->ppu->IPPU.TileCached[TILE_2BIT_ODD] || !snes->ppu->IPPU.TileCached[TILE_4BIT_EVEN] ||
        !snes->ppu->IPPU.TileCached[TILE_4BIT_ODD]) {
        Deinit();
        return (FALSE);
    }

    memset(RAM, 0, 0x20000);
    memset(SRAM, 0, 0x80000);
    memset(VRAM, 0, 0x10000);

    memset(ROM, 0, MAX_ROM_SIZE + 0x200 + 0x8000);
    memset(snes->ppu->IPPU.TileCache[TILE_2BIT], 0, MAX_2BIT_TILES * 64);
    memset(snes->ppu->IPPU.TileCache[TILE_4BIT], 0, MAX_4BIT_TILES * 64);
    memset(snes->ppu->IPPU.TileCache[TILE_8BIT], 0, MAX_8BIT_TILES * 64);
    memset(snes->ppu->IPPU.TileCache[TILE_2BIT_EVEN], 0, MAX_2BIT_TILES * 64);
    memset(snes->ppu->IPPU.TileCache[TILE_2BIT_ODD], 0, MAX_2BIT_TILES * 64);
    memset(snes->ppu->IPPU.TileCache[TILE_4BIT_EVEN], 0, MAX_4BIT_TILES * 64);
    memset(snes->ppu->IPPU.TileCache[TILE_4BIT_ODD], 0, MAX_4BIT_TILES * 64);
    memset(snes->ppu->IPPU.TileCached[TILE_2BIT], 0, MAX_2BIT_TILES);
    memset(snes->ppu->IPPU.TileCached[TILE_4BIT], 0, MAX_4BIT_TILES);
    memset(snes->ppu->IPPU.TileCached[TILE_8BIT], 0, MAX_8BIT_TILES);
    memset(snes->ppu->IPPU.TileCached[TILE_2BIT_EVEN], 0, MAX_2BIT_TILES);
    memset(snes->ppu->IPPU.TileCached[TILE_2BIT_ODD], 0, MAX_2BIT_TILES);
    memset(snes->ppu->IPPU.TileCached[TILE_4BIT_EVEN], 0, MAX_4BIT_TILES);
    memset(snes->ppu->IPPU.TileCached[TILE_4BIT_ODD], 0, MAX_4BIT_TILES);

    FillRAM = ROM;
    ROM += 0x8000;
    C4RAM   = ROM + 0x400000 + 8192 * 8;
    OBC1RAM = ROM + 0x400000;
    BIOSROM = ROM + 0x300000;
    BSRAM   = ROM + 0x400000;

    PostRomInitFunc = NULL;
    return (TRUE);
}
void CMemory::Deinit(void)
{
    if (RAM) {
        free(RAM);
        RAM = NULL;
    }
    if (SRAM) {
        free(SRAM);
        SRAM = NULL;
    }
    if (VRAM) {
        free(VRAM);
        VRAM = NULL;
    }
    if (ROM) {
        ROM -= 0x8000;
        free(ROM);
        ROM = NULL;
    }
    for (int t = 0; t < 7; t++) {
        if (snes->ppu->IPPU.TileCache[t]) {
            free(snes->ppu->IPPU.TileCache[t]);
            snes->ppu->IPPU.TileCache[t] = NULL;
        }
        if (snes->ppu->IPPU.TileCached[t]) {
            free(snes->ppu->IPPU.TileCached[t]);
            snes->ppu->IPPU.TileCached[t] = NULL;
        }
    }
    Safe(NULL);
    SafeANK(NULL);
}
int CMemory::ScoreHiROM(bool8 skip_header, int32 romoff)
{
    uint8 *buf   = ROM + 0xff00 + romoff + (skip_header ? 0x200 : 0);
    int    score = 0;
    if (buf[0xd7] == 13 && CalculatedSize > 1024 * 1024 * 4)
        score += 5;
    if (buf[0xd5] & 0x1)
        score += 2;
    if (buf[0xd5] == 0x23)
        score -= 2;
    if (buf[0xd4] == 0x20)
        score += 2;
    if ((buf[0xdc] + (buf[0xdd] << 8)) + (buf[0xde] + (buf[0xdf] << 8)) == 0xffff) {
        score += 2;
        if (0 != (buf[0xde] + (buf[0xdf] << 8)))
            score++;
    }
    if (buf[0xda] == 0x33)
        score += 2;
    if ((buf[0xd5] & 0xf) < 4)
        score += 2;
    if (!(buf[0xfd] & 0x80))
        score -= 6;
    if ((buf[0xfc] + (buf[0xfd] << 8)) > 0xffb0)
        score -= 2;
    if (CalculatedSize > 1024 * 1024 * 3)
        score += 4;
    if ((1 << (buf[0xd7] - 7)) > 48)
        score -= 1;
    if (!allASCII(&buf[0xb0], 6))
        score -= 1;
    if (!allASCII(&buf[0xc0], ROM_NAME_LEN - 1))
        score -= 1;
    return (score);
}
int CMemory::ScoreLoROM(bool8 skip_header, int32 romoff)
{
    uint8 *buf   = ROM + 0x7f00 + romoff + (skip_header ? 0x200 : 0);
    int    score = 0;
    if (!(buf[0xd5] & 0x1))
        score += 3;
    if (buf[0xd5] == 0x23)
        score += 2;
    if ((buf[0xdc] + (buf[0xdd] << 8)) + (buf[0xde] + (buf[0xdf] << 8)) == 0xffff) {
        score += 2;
        if (0 != (buf[0xde] + (buf[0xdf] << 8)))
            score++;
    }
    if (buf[0xda] == 0x33)
        score += 2;
    if ((buf[0xd5] & 0xf) < 4)
        score += 2;
    if (!(buf[0xfd] & 0x80))
        score -= 6;
    if ((buf[0xfc] + (buf[0xfd] << 8)) > 0xffb0)
        score -= 2;
    if (CalculatedSize <= 1024 * 1024 * 16)
        score += 2;
    if ((1 << (buf[0xd7] - 7)) > 48)
        score -= 1;
    if (!allASCII(&buf[0xb0], 6))
        score -= 1;
    if (!allASCII(&buf[0xc0], ROM_NAME_LEN - 1))
        score -= 1;
    return (score);
}
int CMemory::First512BytesCountZeroes() const
{
    const uint8 *buf       = ROM;
    int          zeroCount = 0;
    for (int i = 0; i < 512; i++) {
        if (buf[i] == 0) {
            zeroCount++;
        }
    }
    return zeroCount;
}
uint32 CMemory::HeaderRemove(uint32 size, uint8 *buf)
{
    uint32 calc_size = (size / 0x2000) * 0x2000;
    if ((size - calc_size == 512 && !Settings.ForceNoHeader) || Settings.ForceHeader) {
        uint8 *NSRTHead = buf + 0x1D0;
        if (!strncmp("NSRT", (char *)&NSRTHead[24], 4)) {
            if (NSRTHead[28] == 22) {
                if (((std::accumulate(NSRTHead, NSRTHead + sizeof(NSRTHeader), 0) & 0xFF) == NSRTHead[30]) &&
                    (NSRTHead[30] + NSRTHead[31] == 255) && ((NSRTHead[0] & 0x0F) <= 13) &&
                    (((NSRTHead[0] & 0xF0) >> 4) <= 3) && ((NSRTHead[0] & 0xF0) >> 4))
                    memcpy(NSRTHeader, NSRTHead, sizeof(NSRTHeader));
            }
        }
        memmove(buf, buf + 512, calc_size);
        HeaderCount++;
        size -= 512;
    }
    return (size);
}
uint32 CMemory::FileLoader(uint8 *buffer, const char *filename, uint32 maxsize)
{
    uint32 totalSize = 0;
    char   fname[PATH_MAX + 1];
    char   drive[_MAX_DRIVE + 1], dir[_MAX_DIR + 1], name[_MAX_FNAME + 1], exts[_MAX_EXT + 1];
    char  *ext;
    ext = &exts[0];

    memset(NSRTHeader, 0, sizeof(NSRTHeader));
    HeaderCount = 0;
    _splitpath(filename, drive, dir, name, exts);
    _makepath(fname, drive, dir, name, exts);
    int nFormat = FILE_DEFAULT;
    if (strcasecmp(ext, "zip") == 0 || strcasecmp(ext, "msu1") == 0)
        nFormat = FILE_ZIP;
    else if (strcasecmp(ext, "jma") == 0)
        nFormat = FILE_JMA;
    switch (nFormat) {
        case FILE_DEFAULT:
        default: {
            STREAM fp = OPEN_STREAM(fname, "rb");
            if (!fp)
                return (0);
            strcpy(ROMFilename, fname);
            int    len  = 0;
            uint32 size = 0;
            bool8  more = FALSE;
            uint8 *ptr  = buffer;
            do {
                size = READ_STREAM(ptr, maxsize + 0x200 - (ptr - buffer), fp);
                CLOSE_STREAM(fp);
                size = HeaderRemove(size, ptr);
                totalSize += size;
                ptr += size;
                if (ptr - buffer < maxsize + 0x200 && (isdigit(ext[0]) && ext[1] == 0 && ext[0] < '9')) {
                    more = TRUE;
                    ext[0]++;
                    _makepath(fname, drive, dir, name, exts);
                } else if (ptr - buffer < maxsize + 0x200 &&
                           (((len = strlen(name)) == 7 || len == 8) && strncasecmp(name, "sf", 2) == 0 &&
                            isdigit(name[2]) && isdigit(name[3]) && isdigit(name[4]) && isdigit(name[5]) &&
                            isalpha(name[len - 1]))) {
                    more = TRUE;
                    name[len - 1]++;
                    _makepath(fname, drive, dir, name, exts);
                } else
                    more = FALSE;
            } while (more && (fp = OPEN_STREAM(fname, "rb")) != NULL);
            break;
        }
    }

    return ((uint32)totalSize);
}
bool8 CMemory::LoadROMMem(const uint8 *source, uint32 sourceSize)
{
    if (!source || sourceSize > MAX_ROM_SIZE)
        return FALSE;
    strcpy(ROMFilename, "MemoryROM");
    do {
        memset(ROM, 0, MAX_ROM_SIZE);
        memset(&Multi, 0, sizeof(Multi));
        memcpy(ROM, source, sourceSize);
    } while (!LoadROMInt(sourceSize));
    return TRUE;
}
bool8 CMemory::LoadROM(const char *filename)
{
    if (!filename || !*filename)
        return FALSE;
    int32 totalFileSize;
    do {
        memset(ROM, 0, MAX_ROM_SIZE);
        memset(&Multi, 0, sizeof(Multi));
        totalFileSize = FileLoader(ROM, filename, MAX_ROM_SIZE);
        if (!totalFileSize)
            return (FALSE);
    } while (!LoadROMInt(totalFileSize));
    return TRUE;
}
bool8 CMemory::LoadROMInt(int32 ROMfillSize)
{
    Settings.DisplayColor = BUILD_PIXEL(31, 31, 31);
    SET_UI_COLOR(255, 255, 255);
    CalculatedSize = 0;
    ExtendedFormat = NOPE;
    int hi_score, lo_score;
    int score_headered;
    int score_nonheadered;
    hi_score                     = ScoreHiROM(FALSE);
    lo_score                     = ScoreLoROM(FALSE);
    score_nonheadered            = max(hi_score, lo_score);
    score_headered               = max(ScoreHiROM(TRUE), ScoreLoROM(TRUE));
    bool size_is_likely_headered = ((ROMfillSize - 512) & 0xFFFF) == 0;
    if (size_is_likely_headered) {
        score_headered += 2;
    } else {
        score_headered -= 2;
    }
    if (First512BytesCountZeroes() >= 0x1E0) {
        score_headered += 2;
    } else {
        score_headered -= 2;
    }
    bool headered_score_highest = score_headered > score_nonheadered;
    if (HeaderCount == 0 && !Settings.ForceNoHeader && headered_score_highest) {
        memmove(ROM, ROM + 512, ROMfillSize - 512);
        ROMfillSize -= 512;
        hi_score = ScoreHiROM(FALSE);
        lo_score = ScoreLoROM(FALSE);
    }
    CalculatedSize = ((ROMfillSize + 0x1fff) / 0x2000) * 0x2000;
    if (CalculatedSize > 0x400000 && (ROM[0x7fd5] + (ROM[0x7fd6] << 8)) != 0x3423 &&
        (ROM[0x7fd5] + (ROM[0x7fd6] << 8)) != 0x3523 && (ROM[0x7fd5] + (ROM[0x7fd6] << 8)) != 0x4332 &&
        (ROM[0x7fd5] + (ROM[0x7fd6] << 8)) != 0x4532 && (ROM[0xffd5] + (ROM[0xffd6] << 8)) != 0xF93a &&
        (ROM[0xffd5] + (ROM[0xffd6] << 8)) != 0xF53a)
        ExtendedFormat = YEAH;
    if (ExtendedFormat == NOPE && ((ROM[0x7ffc] + (ROM[0x7ffd] << 8)) < 0x8000) &&
        ((ROM[0xfffc] + (ROM[0xfffd] << 8)) < 0x8000)) {
    }
    hi_score         = ScoreHiROM(FALSE);
    lo_score         = ScoreLoROM(FALSE);
    uint8 *RomHeader = ROM;
    if (ExtendedFormat != NOPE) {
        int swappedhirom, swappedlorom;
        swappedhirom = ScoreHiROM(FALSE, 0x400000);
        swappedlorom = ScoreLoROM(FALSE, 0x400000);
        if (max(swappedlorom, swappedhirom) >= max(lo_score, hi_score)) {
            ExtendedFormat = BIGFIRST;
            hi_score       = swappedhirom;
            lo_score       = swappedlorom;
            RomHeader += 0x400000;
        } else
            ExtendedFormat = SMALLFIRST;
    }
    bool8 interleaved, tales = FALSE;
    interleaved = Settings.ForceInterleaved || Settings.ForceInterleaved2 || Settings.ForceInterleaveGD24;
    if (Settings.ForceLoROM || (!Settings.ForceHiROM && lo_score >= hi_score)) {
        LoROM = TRUE;
        HiROM = FALSE;
        if ((RomHeader[0x7fd5] & 0xf0) == 0x20 || (RomHeader[0x7fd5] & 0xf0) == 0x30) {
            switch (RomHeader[0x7fd5] & 0xf) {
                case 1:
                    interleaved = TRUE;
                    break;
                case 5:
                    interleaved = TRUE;
                    tales       = TRUE;
                    break;
            }
        }
    } else {
        LoROM = FALSE;
        HiROM = TRUE;
        if ((RomHeader[0xffd5] & 0xf0) == 0x20 || (RomHeader[0xffd5] & 0xf0) == 0x30) {
            switch (RomHeader[0xffd5] & 0xf) {
                case 0:
                case 3:
                    interleaved = TRUE;
                    break;
            }
        }
    }
    if (!Settings.ForceHiROM && !Settings.ForceLoROM) {
        if (strncmp((char *)&ROM[0x7fc0], "YUYU NO QUIZ DE GO!GO!", 22) == 0 ||
            (strncmp((char *)&ROM[0xffc0], "BATMAN--REVENGE JOKER", 21) == 0)) {
            LoROM       = TRUE;
            HiROM       = FALSE;
            interleaved = FALSE;
            tales       = FALSE;
        }
    }
    if (!Settings.ForceNotInterleaved && interleaved) {
        if (tales) {
            LoROM = FALSE;
            HiROM = TRUE;
        } else if (Settings.ForceInterleaveGD24 && CalculatedSize == 0x300000) {
            bool8 t = LoROM;
            LoROM   = HiROM;
            HiROM   = t;

        } else {
            bool8 t = LoROM;
            LoROM   = HiROM;
            HiROM   = t;
        }
        hi_score = ScoreHiROM(FALSE);
        lo_score = ScoreLoROM(FALSE);
        if ((HiROM && (lo_score >= hi_score || hi_score < 0)) || (LoROM && (hi_score > lo_score || lo_score < 0))) {
            Settings.ForceNotInterleaved = TRUE;
            Settings.ForceInterleaved    = FALSE;
            return (FALSE);
        }
    }
    if (ExtendedFormat == SMALLFIRST)
        tales = TRUE;
    if (tales) {
        uint8 *tmp = (uint8 *)malloc(CalculatedSize - 0x400000);
        if (tmp) {
            memmove(tmp, ROM, CalculatedSize - 0x400000);
            memmove(ROM, ROM + CalculatedSize - 0x400000, 0x400000);
            memmove(ROM + 0x400000, tmp, CalculatedSize - 0x400000);
            free(tmp);
        }
    }
    if (strncmp(LastRomFilename, ROMFilename, PATH_MAX + 1)) {
        strncpy(LastRomFilename, ROMFilename, PATH_MAX + 1);
        LastRomFilename[PATH_MAX] = 0;
    }
    memset(&SNESGameFixes, 0, sizeof(SNESGameFixes));
    SNESGameFixes.SRAMInitialValue = 0x60;
    InitROM();
    snes->scpu->S9xReset();
    return (TRUE);
}
bool8 CMemory::SaveSRTC(void)
{
    FILE *fp;
    fp = fopen(snes->S9xGetFilename(".rtc", SRAM_DIR), "wb");
    if (!fp)
        return (FALSE);


    fclose(fp);
    return (TRUE);
}
void CMemory::ClearSRAM(bool8 onlyNonSavedSRAM)
{
    if (onlyNonSavedSRAM)
        if (!(Settings.SuperFX && ROMType < 0x15) && !(Settings.SA1 && ROMType == 0x34))
            return;
    memset(SRAM, SNESGameFixes.SRAMInitialValue, 0x80000);
}
bool8 CMemory::LoadSRAM(const char *filename)
{
    FILE *file;
    int   size, len;
    char  sramName[PATH_MAX + 1];
    strcpy(sramName, filename);
    ClearSRAM();
    if (Multi.cartType && Multi.sramSizeB) {
        char temp[PATH_MAX + 1];
        strcpy(temp, ROMFilename);
        strcpy(ROMFilename, Multi.fileNameB);
        size = (1 << (Multi.sramSizeB + 3)) * 128;
        file = fopen(snes->S9xGetFilename(".srm", SRAM_DIR), "rb");
        if (file) {
            len = fread((char *)Multi.sramB, 1, 0x10000, file);
            fclose(file);
            if (len - size == 512)
                memmove(Multi.sramB, Multi.sramB + 512, size);
        }
        strcpy(ROMFilename, temp);
    }
    size = SRAMSize ? (1 << (SRAMSize + 3)) * 128 : 0;
    if (LoROM)
        size = size < 0x70000 ? size : 0x70000;
    else if (HiROM)
        size = size < 0x40000 ? size : 0x40000;
    if (size) {
        file = fopen(sramName, "rb");
        if (file) {
            len = fread((char *)SRAM, 1, size, file);
            fclose(file);
            if (len - size == 512)
                memmove(SRAM, SRAM + 512, size);

            return (TRUE);
        } else if (Settings.BS && !Settings.BSXItself) {
            char path[PATH_MAX + 1];
            strcpy(path, snes->S9xGetDirectory(SRAM_DIR));
            strcat(path, SLASH_STR);
            strcat(path, "BS-X.srm");
            file = fopen(path, "rb");
            if (file) {
                len = fread((char *)SRAM, 1, size, file);
                fclose(file);
                if (len - size == 512)
                    memmove(SRAM, SRAM + 512, size);
                return (TRUE);
            } else {
                return (FALSE);
            }
        }
        return (FALSE);
    }
    return (TRUE);
}
bool8 CMemory::SaveSRAM(const char *filename)
{
    if (Settings.SuperFX && ROMType < 0x15)
        return (TRUE);
    if (Settings.SA1 && ROMType == 0x34)
        return (TRUE);
    FILE *file;
    int   size;
    char  sramName[PATH_MAX + 1];
    strcpy(sramName, filename);
    if (Multi.cartType && Multi.sramSizeB) {
        char name[PATH_MAX + 1], temp[PATH_MAX + 1];
        strcpy(temp, ROMFilename);
        strcpy(ROMFilename, Multi.fileNameB);
        strcpy(name, snes->S9xGetFilename(".srm", SRAM_DIR));
        size = (1 << (Multi.sramSizeB + 3)) * 128;
        file = fopen(name, "wb");
        if (file) {
            if (!fwrite((char *)Multi.sramB, size, 1, file))
                printf("Couldn't write to subcart SRAM file.\n");
            fclose(file);
        }
        strcpy(ROMFilename, temp);
    }
    size = SRAMSize ? (1 << (SRAMSize + 3)) * 128 : 0;
    if (LoROM)
        size = size < 0x70000 ? size : 0x70000;
    else if (HiROM)
        size = size < 0x40000 ? size : 0x40000;
    if (size) {
        file = fopen(sramName, "wb");
        if (file) {
            if (!fwrite((char *)SRAM, size, 1, file))
                printf("Couldn't write to SRAM file.\n");
            fclose(file);
            if (Settings.SRTC || Settings.SPC7110RTC)
                SaveSRTC();
            return (TRUE);
        }
    }
    return (FALSE);
}
uint32 CMemory::caCRC32(uint8 *array, uint32 size, uint32 crc32)
{
    for (uint32 i = 0; i < size; i++)
        crc32 = ((crc32 >> 8) & 0x00FFFFFF) ^ crc32Table[(crc32 ^ array[i]) & 0xFF];
    return (~crc32);
}
char *CMemory::Safe(const char *s)
{
    char *safe     = NULL;
    int   safe_len = 0;
    if (s == NULL) {
        if (safe) {
            free(safe);
            safe = NULL;
        }
        return (NULL);
    }
    int len = strlen(s);
    if (!safe || len + 1 > safe_len) {
        if (safe)
            free(safe);
        safe_len = len + 1;
        safe     = (char *)malloc(safe_len);
    }
    for (int i = 0; i < len; i++) {
        if (s[i] >= 32 && s[i] < 127)
            safe[i] = s[i];
        else
            safe[i] = '_';
    }
    safe[len] = 0;
    return (safe);
}
char *CMemory::SafeANK(const char *s)
{
    char *safe     = NULL;
    int   safe_len = 0;
    if (s == NULL) {
        if (safe) {
            free(safe);
            safe = NULL;
        }
        return (NULL);
    }
    int len = strlen(s);
    if (!safe || len + 1 > safe_len) {
        if (safe)
            free(safe);
        safe_len = len + 1;
        safe     = (char *)malloc(safe_len);
    }
    for (int i = 0; i < len; i++) {
        if (s[i] >= 32 && s[i] < 127)
            safe[i] = s[i];
        else if (ROMRegion == 0 && ((uint8)s[i] >= 0xa0 && (uint8)s[i] < 0xe0))
            safe[i] = s[i];
        else
            safe[i] = '_';
    }
    safe[len] = 0;
    return (safe);
}
void CMemory::ParseSNESHeader(uint8 *RomHeader)
{
    bool8 bs = Settings.BS & !Settings.BSXItself;
    strncpy(ROMName, (char *)&RomHeader[0x10], ROM_NAME_LEN - 1);
    if (bs)
        memset(ROMName + 16, 0x20, ROM_NAME_LEN - 17);
    if (bs) {
        if (!(((RomHeader[0x29] & 0x20) && CalculatedSize < 0x100000) ||
              (!(RomHeader[0x29] & 0x20) && CalculatedSize == 0x100000)))
            printf("BS: Size mismatch\n");
        int p = 0;
        while ((1 << p) < (int)CalculatedSize)
            p++;
        ROMSize = p - 10;
    } else
        ROMSize = RomHeader[0x27];
    SRAMSize              = bs ? 5 : RomHeader[0x28];
    ROMSpeed              = bs ? RomHeader[0x28] : RomHeader[0x25];
    ROMType               = bs ? 0xE5 : RomHeader[0x26];
    ROMRegion             = bs ? 0 : RomHeader[0x29];
    ROMChecksum           = RomHeader[0x2E] + (RomHeader[0x2F] << 8);
    ROMComplementChecksum = RomHeader[0x2C] + (RomHeader[0x2D] << 8);
    memmove(ROMId, &RomHeader[0x02], 4);
    if (RomHeader[0x2A] != 0x33)
        CompanyId = ((RomHeader[0x2A] >> 4) & 0x0F) * 36 + (RomHeader[0x2A] & 0x0F);
    else if (isalnum(RomHeader[0x00]) && isalnum(RomHeader[0x01])) {
        int l, r, l2, r2;
        l         = toupper(RomHeader[0x00]);
        r         = toupper(RomHeader[0x01]);
        l2        = (l > '9') ? l - '7' : l - '0';
        r2        = (r > '9') ? r - '7' : r - '0';
        CompanyId = l2 * 36 + r2;
    }
}
void CMemory::InitROM(void)
{
    Settings.SuperFX    = FALSE;
    Settings.DSP        = 0;
    Settings.SA1        = FALSE;
    Settings.C4         = FALSE;
    Settings.SDD1       = FALSE;
    Settings.SPC7110    = FALSE;
    Settings.SPC7110RTC = FALSE;
    Settings.OBC1       = FALSE;
    Settings.SETA       = 0;
    Settings.SRTC       = FALSE;
    Settings.BS         = FALSE;
    Settings.MSU1       = FALSE;

    CompanyId = -1;
    memset(ROMId, 0, 5);
    uint8 *RomHeader = ROM + 0x7FB0;
    if (ExtendedFormat == BIGFIRST)
        RomHeader += 0x400000;
    if (HiROM)
        RomHeader += 0x8000;

    ParseSNESHeader(RomHeader);
    if (ROMType == 0x03) {
        if (ROMSpeed == 0x30)
            Settings.DSP = 4;
        else
            Settings.DSP = 1;
    } else if (ROMType == 0x05) {
        if (ROMSpeed == 0x20)
            Settings.DSP = 2;
        else if (ROMSpeed == 0x30 && RomHeader[0x2a] == 0xb2)
            Settings.DSP = 3;
        else
            Settings.DSP = 1;
    }

    uint32 identifier = ((ROMType & 0xff) << 8) + (ROMSpeed & 0xff);
    switch (identifier) {
        case 0x5535:
            break;
        case 0xF93A:
            Settings.SPC7110RTC = TRUE;
        case 0xF53A:
            break;
        case 0x2530:
            Settings.OBC1 = TRUE;
            break;
        case 0x3423:
        case 0x3523:
            Settings.SA1 = TRUE;
            break;
        case 0x1320:
        case 0x1420:
        case 0x1520:
        case 0x1A20:
        case 0x1330:
        case 0x1430:
        case 0x1530:
        case 0x1A30:
            if (ROM[0x7FDA] == 0x33)
                SRAMSize = ROM[0x7FBD];
            else
                SRAMSize = 5;
            break;
        case 0x4332:
        case 0x4532:
            Settings.SDD1 = TRUE;
            break;
        case 0xF530:
            SRAMSize                       = 2;
            SNESGameFixes.SRAMInitialValue = 0x00;
            break;
        case 0xF630:
            SRAMSize                       = 2;
            SNESGameFixes.SRAMInitialValue = 0x00;
            break;
        case 0xF320:
            Settings.C4 = TRUE;
            break;
    }
    Settings.MSU1 = S9xMSU1ROMExists();
    Map_Initialize();
    CalculatedChecksum = 0;
    if (HiROM) {
        if (Settings.BS)
            ;
        else if (ExtendedFormat != NOPE)
            Map_ExtendedHiROMMap();
        else if (Multi.cartType == 3)
            Map_BSCartHiROMMap();
        else
            Map_HiROMMap();
    } else {
        if (Settings.BS)
            ;
        else if (Settings.SDD1)
            Map_SDD1LoROMMap();
        else if (ExtendedFormat != NOPE)
            Map_JumboLoROMMap();
        else if (strncmp(ROMName, "WANDERERS FROM YS", 17) == 0)
            Map_NoMAD1LoROMMap();
        else if (Multi.cartType == 3)
            if (strncmp(ROMName, "SOUND NOVEL-TCOOL", 17) == 0 || strncmp(ROMName, "DERBY STALLION 96", 17) == 0)
                Map_BSCartLoROMMap(1);
            else
                Map_BSCartLoROMMap(0);
        else if (strncmp(ROMName, "SOUND NOVEL-TCOOL", 17) == 0 || strncmp(ROMName, "DERBY STALLION 96", 17) == 0)
            Map_ROM24MBSLoROMMap();
        else if (strncmp(ROMName, "THOROUGHBRED BREEDER3", 21) == 0 || strncmp(ROMName, "RPG-TCOOL 2", 11) == 0)
            Map_SRAM512KLoROMMap();
        else if (strncmp(ROMName, "ADD-ON BASE CASSETE", 19) == 0) {
            if (Multi.cartType == 4) {
                SRAMSize = Multi.sramSizeA;
                Map_SufamiTurboLoROMMap();
            } else {
                SRAMSize = 5;
                Map_SufamiTurboPseudoLoROMMap();
            }
        } else
            Map_LoROMMap();
    }
    Checksum_Calculate();
    bool8 isChecksumOK = (ROMChecksum + ROMComplementChecksum == 0xffff) & (ROMChecksum == CalculatedChecksum);
    if (!Settings.BS || Settings.BSXItself) {
        ROMCRC32 = caCRC32(ROM, CalculatedSize);
        sha256sum(ROM, CalculatedSize, ROMSHA256);
    } else {
        int   offset   = HiROM ? 0xffc0 : 0x7fc0;
        uint8 BSMagic0 = ROM[offset + 22], BSMagic1 = ROM[offset + 23];
        ROM[offset + 22] = 0x42;
        ROM[offset + 23] = 0x00;
        ROMCRC32         = caCRC32(ROM, CalculatedSize);
        sha256sum(ROM, CalculatedSize, ROMSHA256);
        ROM[offset + 22] = BSMagic0;
        ROM[offset + 23] = BSMagic1;
    }
    if (Settings.ForceNTSC)
        Settings.PAL = FALSE;
    else if (Settings.ForcePAL)
        Settings.PAL = TRUE;
    else if (!Settings.BS && (((ROMRegion >= 2) && (ROMRegion <= 12)) || ROMRegion == 18))
        Settings.PAL = TRUE;
    else
        Settings.PAL = FALSE;
    if (Settings.PAL) {
        Settings.FrameTime = Settings.FrameTimePAL;
        ROMFramesPerSecond = 50;
    } else {
        Settings.FrameTime = Settings.FrameTimeNTSC;
        ROMFramesPerSecond = 60;
    }
    ROMName[ROM_NAME_LEN - 1] = 0;
    if (strlen(ROMName)) {
        char *p = ROMName + strlen(ROMName);
        if (p > ROMName + 21 && ROMName[20] == ' ')
            p = ROMName + 21;
        while (p > ROMName && *(p - 1) == ' ')
            p--;
        *p = 0;
    }
    SRAMMask = SRAMSize ? ((1 << (SRAMSize + 3)) * 128) - 1 : 0;
    if (!isChecksumOK || ((uint32)CalculatedSize > (uint32)(((1 << (ROMSize - 7)) * 128) * 1024))) {
        Settings.DisplayColor = BUILD_PIXEL(31, 31, 0);
        SET_UI_COLOR(255, 255, 0);
    }
    if (Settings.IsPatched) {
        Settings.DisplayColor = BUILD_PIXEL(26, 26, 31);
        SET_UI_COLOR(216, 216, 255);
    }
    if (Multi.cartType == 4) {
        Settings.DisplayColor = BUILD_PIXEL(0, 16, 31);
        SET_UI_COLOR(0, 128, 255);
    }
    Timings.H_Max_Master     = SNES_CYCLES_PER_SCANLINE;
    Timings.H_Max            = Timings.H_Max_Master;
    Timings.HBlankStart      = SNES_HBLANK_START_HC;
    Timings.HBlankEnd        = SNES_HBLANK_END_HC;
    Timings.HDMAInit         = SNES_HDMA_INIT_HC;
    Timings.HDMAStart        = SNES_HDMA_START_HC;
    Timings.RenderPos        = SNES_RENDER_START_HC;
    Timings.V_Max_Master     = Settings.PAL ? SNES_MAX_PAL_VCOUNTER : SNES_MAX_NTSC_VCOUNTER;
    Timings.V_Max            = Timings.V_Max_Master;
    Timings.DMACPUSync       = 18;
    Timings.NMIDMADelay      = 24;
    Timings.IRQTriggerCycles = 14;
    Timings.APUSpeedup       = 0;
    S9xAPUTimingSetSpeedup(Timings.APUSpeedup);
    snes->ppu->IPPU.TotalEmulatedFrames = 0;

    char displayName[ROM_NAME_LEN];
    strcpy(RawROMName, ROMName);
    sprintf(displayName, "%s", SafeANK(ROMName));
    sprintf(ROMName, "%s", Safe(ROMName));
    sprintf(ROMId, "%s", Safe(ROMId));

    Settings.ForceLoROM          = FALSE;
    Settings.ForceHiROM          = FALSE;
    Settings.ForceHeader         = FALSE;
    Settings.ForceNoHeader       = FALSE;
    Settings.ForceInterleaved    = FALSE;
    Settings.ForceInterleaved2   = FALSE;
    Settings.ForceInterleaveGD24 = FALSE;
    Settings.ForceNotInterleaved = FALSE;
    Settings.ForcePAL            = FALSE;
    Settings.ForceNTSC           = FALSE;
    Settings.TakeScreenshot      = FALSE;
    if (PostRomInitFunc)
        PostRomInitFunc();
}
uint32 CMemory::map_mirror(uint32 size, uint32 pos)
{
    if (size == 0)
        return (0);
    if (pos < size)
        return (pos);
    uint32 mask = 1 << 31;
    while (!(pos & mask))
        mask >>= 1;
    if (size <= (pos & mask))
        return (map_mirror(size, pos - mask));
    else
        return (mask + map_mirror(size - mask, pos - mask));
}
void CMemory::map_lorom(uint32 bank_s, uint32 bank_e, uint32 addr_s, uint32 addr_e, uint32 size)
{
    uint32 c, i, p, addr;
    for (c = bank_s; c <= bank_e; c++) {
        for (i = addr_s; i <= addr_e; i += 0x1000) {
            p             = (c << 4) | (i >> 12);
            addr          = (c & 0x7f) * 0x8000;
            Map[p]        = ROM + map_mirror(size, addr) - (i & 0x8000);
            BlockIsROM[p] = TRUE;
            BlockIsRAM[p] = FALSE;
        }
    }
}
void CMemory::map_hirom(uint32 bank_s, uint32 bank_e, uint32 addr_s, uint32 addr_e, uint32 size)
{
    uint32 c, i, p, addr;
    for (c = bank_s; c <= bank_e; c++) {
        for (i = addr_s; i <= addr_e; i += 0x1000) {
            p             = (c << 4) | (i >> 12);
            addr          = c << 16;
            Map[p]        = ROM + map_mirror(size, addr);
            BlockIsROM[p] = TRUE;
            BlockIsRAM[p] = FALSE;
        }
    }
}
void CMemory::map_lorom_offset(uint32 bank_s, uint32 bank_e, uint32 addr_s, uint32 addr_e, uint32 size, uint32 offset)
{
    uint32 c, i, p, addr;
    for (c = bank_s; c <= bank_e; c++) {
        for (i = addr_s; i <= addr_e; i += 0x1000) {
            p             = (c << 4) | (i >> 12);
            addr          = ((c - bank_s) & 0x7f) * 0x8000;
            Map[p]        = ROM + offset + map_mirror(size, addr) - (i & 0x8000);
            BlockIsROM[p] = TRUE;
            BlockIsRAM[p] = FALSE;
        }
    }
}
void CMemory::map_hirom_offset(uint32 bank_s, uint32 bank_e, uint32 addr_s, uint32 addr_e, uint32 size, uint32 offset)
{
    uint32 c, i, p, addr;
    for (c = bank_s; c <= bank_e; c++) {
        for (i = addr_s; i <= addr_e; i += 0x1000) {
            p             = (c << 4) | (i >> 12);
            addr          = (c - bank_s) << 16;
            Map[p]        = ROM + offset + map_mirror(size, addr);
            BlockIsROM[p] = TRUE;
            BlockIsRAM[p] = FALSE;
        }
    }
}
void CMemory::map_space(uint32 bank_s, uint32 bank_e, uint32 addr_s, uint32 addr_e, uint8 *data)
{
    uint32 c, i, p;
    for (c = bank_s; c <= bank_e; c++) {
        for (i = addr_s; i <= addr_e; i += 0x1000) {
            p             = (c << 4) | (i >> 12);
            Map[p]        = data;
            BlockIsROM[p] = FALSE;
            BlockIsRAM[p] = TRUE;
        }
    }
}
void CMemory::map_index(uint32 bank_s, uint32 bank_e, uint32 addr_s, uint32 addr_e, int index, int type)
{
    uint32 c, i, p;
    bool8  isROM, isRAM;
    isROM = ((type == MAP_TYPE_I_O) || (type == MAP_TYPE_RAM)) ? FALSE : TRUE;
    isRAM = ((type == MAP_TYPE_I_O) || (type == MAP_TYPE_ROM)) ? FALSE : TRUE;
    for (c = bank_s; c <= bank_e; c++) {
        for (i = addr_s; i <= addr_e; i += 0x1000) {
            p             = (c << 4) | (i >> 12);
            Map[p]        = (uint8 *)(pint)index;
            BlockIsROM[p] = isROM;
            BlockIsRAM[p] = isRAM;
        }
    }
}
void CMemory::map_System(void)
{
    map_space(0x00, 0x3f, 0x0000, 0x1fff, RAM);
    map_index(0x00, 0x3f, 0x2000, 0x3fff, MAP_PPU, MAP_TYPE_I_O);
    map_index(0x00, 0x3f, 0x4000, 0x5fff, MAP_CPU, MAP_TYPE_I_O);
    map_space(0x80, 0xbf, 0x0000, 0x1fff, RAM);
    map_index(0x80, 0xbf, 0x2000, 0x3fff, MAP_PPU, MAP_TYPE_I_O);
    map_index(0x80, 0xbf, 0x4000, 0x5fff, MAP_CPU, MAP_TYPE_I_O);
}
void CMemory::map_WRAM(void)
{
    map_space(0x7e, 0x7e, 0x0000, 0xffff, RAM);
    map_space(0x7f, 0x7f, 0x0000, 0xffff, RAM + 0x10000);
}
void CMemory::map_LoROMSRAM(void)
{
    uint32 hi;
    if (ROMSize > 11 || SRAMSize > 5)
        hi = 0x7fff;
    else
        hi = 0xffff;
    map_index(0x70, 0x7d, 0x0000, hi, MAP_LOROM_SRAM, MAP_TYPE_RAM);
    map_index(0xf0, 0xff, 0x0000, hi, MAP_LOROM_SRAM, MAP_TYPE_RAM);
}
void CMemory::map_HiROMSRAM(void)
{
    map_index(0x20, 0x3f, 0x6000, 0x7fff, MAP_HIROM_SRAM, MAP_TYPE_RAM);
    map_index(0xa0, 0xbf, 0x6000, 0x7fff, MAP_HIROM_SRAM, MAP_TYPE_RAM);
}
void CMemory::map_SetaRISC(void)
{
    map_index(0x00, 0x3f, 0x3000, 0x3fff, MAP_SETA_RISC, MAP_TYPE_I_O);
    map_index(0x80, 0xbf, 0x3000, 0x3fff, MAP_SETA_RISC, MAP_TYPE_I_O);
}
void CMemory::map_SetaDSP(void)
{
    map_index(0x68, 0x6f, 0x0000, 0x7fff, MAP_SETA_DSP, MAP_TYPE_RAM);
    map_index(0x60, 0x67, 0x0000, 0x3fff, MAP_SETA_DSP, MAP_TYPE_I_O);
}
void CMemory::map_WriteProtectROM(void)
{
    memmove((void *)WriteMap, (void *)Map, sizeof(Map));
    for (int c = 0; c < 0x1000; c++) {
        if (BlockIsROM[c])
            WriteMap[c] = (uint8 *)MAP_NONE;
    }
}
void CMemory::Map_Initialize(void)
{
    for (int c = 0; c < 0x1000; c++) {
        Map[c]        = (uint8 *)MAP_NONE;
        WriteMap[c]   = (uint8 *)MAP_NONE;
        BlockIsROM[c] = FALSE;
        BlockIsRAM[c] = FALSE;
    }
}
void CMemory::Map_LoROMMap(void)
{
    printf("Map_LoROMMap\n");
    map_System();
    map_lorom(0x00, 0x3f, 0x8000, 0xffff, CalculatedSize);
    map_lorom(0x40, 0x7f, 0x0000, 0xffff, CalculatedSize);
    map_lorom(0x80, 0xbf, 0x8000, 0xffff, CalculatedSize);
    map_lorom(0xc0, 0xff, 0x0000, 0xffff, CalculatedSize);
    map_LoROMSRAM();
    map_WRAM();
    map_WriteProtectROM();
}
void CMemory::Map_NoMAD1LoROMMap(void)
{
    printf("Map_NoMAD1LoROMMap\n");
    map_System();
    map_lorom(0x00, 0x3f, 0x8000, 0xffff, CalculatedSize);
    map_lorom(0x40, 0x7f, 0x0000, 0xffff, CalculatedSize);
    map_lorom(0x80, 0xbf, 0x8000, 0xffff, CalculatedSize);
    map_lorom(0xc0, 0xff, 0x0000, 0xffff, CalculatedSize);
    map_index(0x70, 0x7f, 0x0000, 0xffff, MAP_LOROM_SRAM, MAP_TYPE_RAM);
    map_index(0xf0, 0xff, 0x0000, 0xffff, MAP_LOROM_SRAM, MAP_TYPE_RAM);
    map_WRAM();
    map_WriteProtectROM();
}
void CMemory::Map_JumboLoROMMap(void)
{
    printf("Map_JumboLoROMMap\n");
    map_System();
    map_lorom_offset(0x00, 0x3f, 0x8000, 0xffff, CalculatedSize - 0x400000, 0x400000);
    map_lorom_offset(0x40, 0x7f, 0x0000, 0xffff, CalculatedSize - 0x600000, 0x600000);
    map_lorom_offset(0x80, 0xbf, 0x8000, 0xffff, 0x400000, 0);
    map_lorom_offset(0xc0, 0xff, 0x0000, 0xffff, 0x400000, 0x200000);
    map_LoROMSRAM();
    map_WRAM();
    map_WriteProtectROM();
}
void CMemory::Map_ROM24MBSLoROMMap(void)
{
    printf("Map_ROM24MBSLoROMMap\n");
    map_System();
    map_lorom_offset(0x00, 0x1f, 0x8000, 0xffff, 0x100000, 0);
    map_lorom_offset(0x20, 0x3f, 0x8000, 0xffff, 0x100000, 0x100000);
    map_lorom_offset(0x80, 0x9f, 0x8000, 0xffff, 0x100000, 0x200000);
    map_lorom_offset(0xa0, 0xbf, 0x8000, 0xffff, 0x100000, 0x100000);
    map_LoROMSRAM();
    map_WRAM();
    map_WriteProtectROM();
}
void CMemory::Map_SRAM512KLoROMMap(void)
{
    printf("Map_SRAM512KLoROMMap\n");
    map_System();
    map_lorom(0x00, 0x3f, 0x8000, 0xffff, CalculatedSize);
    map_lorom(0x40, 0x7f, 0x0000, 0xffff, CalculatedSize);
    map_lorom(0x80, 0xbf, 0x8000, 0xffff, CalculatedSize);
    map_lorom(0xc0, 0xff, 0x0000, 0xffff, CalculatedSize);
    map_space(0x70, 0x70, 0x0000, 0xffff, SRAM);
    map_space(0x71, 0x71, 0x0000, 0xffff, SRAM + 0x8000);
    map_space(0x72, 0x72, 0x0000, 0xffff, SRAM + 0x10000);
    map_space(0x73, 0x73, 0x0000, 0xffff, SRAM + 0x18000);
    map_WRAM();
    map_WriteProtectROM();
}
void CMemory::Map_SufamiTurboLoROMMap(void)
{
    printf("Map_SufamiTurboLoROMMap\n");
    map_System();
    map_lorom_offset(0x00, 0x1f, 0x8000, 0xffff, 0x40000, 0);
    map_lorom_offset(0x20, 0x3f, 0x8000, 0xffff, Multi.cartSizeA, Multi.cartOffsetA);
    map_lorom_offset(0x40, 0x5f, 0x8000, 0xffff, Multi.cartSizeB, Multi.cartOffsetB);
    map_lorom_offset(0x80, 0x9f, 0x8000, 0xffff, 0x40000, 0);
    map_lorom_offset(0xa0, 0xbf, 0x8000, 0xffff, Multi.cartSizeA, Multi.cartOffsetA);
    map_lorom_offset(0xc0, 0xdf, 0x8000, 0xffff, Multi.cartSizeB, Multi.cartOffsetB);
    if (Multi.sramSizeA) {
        map_index(0x60, 0x63, 0x8000, 0xffff, MAP_LOROM_SRAM, MAP_TYPE_RAM);
        map_index(0xe0, 0xe3, 0x8000, 0xffff, MAP_LOROM_SRAM, MAP_TYPE_RAM);
    }
    if (Multi.sramSizeB) {
        map_index(0x70, 0x73, 0x8000, 0xffff, MAP_LOROM_SRAM_B, MAP_TYPE_RAM);
        map_index(0xf0, 0xf3, 0x8000, 0xffff, MAP_LOROM_SRAM_B, MAP_TYPE_RAM);
    }
    map_WRAM();
    map_WriteProtectROM();
}
void CMemory::Map_SufamiTurboPseudoLoROMMap(void)
{
    printf("Map_SufamiTurboPseudoLoROMMap\n");
    map_System();
    map_lorom_offset(0x00, 0x1f, 0x8000, 0xffff, 0x40000, 0);
    map_lorom_offset(0x20, 0x3f, 0x8000, 0xffff, 0x100000, 0x100000);
    map_lorom_offset(0x40, 0x5f, 0x8000, 0xffff, 0x100000, 0x200000);
    map_lorom_offset(0x80, 0x9f, 0x8000, 0xffff, 0x40000, 0);
    map_lorom_offset(0xa0, 0xbf, 0x8000, 0xffff, 0x100000, 0x100000);
    map_lorom_offset(0xc0, 0xdf, 0x8000, 0xffff, 0x100000, 0x200000);
    map_space(0x60, 0x63, 0x8000, 0xffff, SRAM - 0x8000);
    map_space(0xe0, 0xe3, 0x8000, 0xffff, SRAM - 0x8000);
    map_space(0x70, 0x73, 0x8000, 0xffff, SRAM + 0x4000 - 0x8000);
    map_space(0xf0, 0xf3, 0x8000, 0xffff, SRAM + 0x4000 - 0x8000);
    map_WRAM();
    map_WriteProtectROM();
}
void CMemory::Map_SuperFXLoROMMap(void)
{
    printf("Map_SuperFXLoROMMap\n");
    map_System();
    for (int c = 0; c < 64; c++) {
        memmove(&ROM[0x200000 + c * 0x10000], &ROM[c * 0x8000], 0x8000);
        memmove(&ROM[0x208000 + c * 0x10000], &ROM[c * 0x8000], 0x8000);
    }
    map_lorom(0x00, 0x3f, 0x8000, 0xffff, CalculatedSize);
    map_lorom(0x80, 0xbf, 0x8000, 0xffff, CalculatedSize);
    map_hirom_offset(0x40, 0x7f, 0x0000, 0xffff, CalculatedSize, 0);
    map_hirom_offset(0xc0, 0xff, 0x0000, 0xffff, CalculatedSize, 0);
    map_space(0x00, 0x3f, 0x6000, 0x7fff, SRAM - 0x6000);
    map_space(0x80, 0xbf, 0x6000, 0x7fff, SRAM - 0x6000);
    map_space(0x70, 0x70, 0x0000, 0xffff, SRAM);
    map_space(0x71, 0x71, 0x0000, 0xffff, SRAM + 0x10000);
    map_WRAM();
    map_WriteProtectROM();
}
void CMemory::Map_SetaDSPLoROMMap(void)
{
    printf("Map_SetaDSPLoROMMap\n");
    map_System();
    map_lorom(0x00, 0x3f, 0x8000, 0xffff, CalculatedSize);
    map_lorom(0x40, 0x7f, 0x8000, 0xffff, CalculatedSize);
    map_lorom(0x80, 0xbf, 0x8000, 0xffff, CalculatedSize);
    map_lorom(0xc0, 0xff, 0x8000, 0xffff, CalculatedSize);
    map_SetaDSP();
    map_LoROMSRAM();
    map_WRAM();
    map_WriteProtectROM();
}
void CMemory::Map_SDD1LoROMMap(void)
{
    printf("Map_SDD1LoROMMap\n");
    map_System();
    map_lorom(0x00, 0x3f, 0x8000, 0xffff, CalculatedSize);
    map_lorom(0x80, 0xbf, 0x8000, 0xffff, CalculatedSize);
    map_hirom_offset(0x60, 0x7f, 0x0000, 0xffff, CalculatedSize, 0);
    map_hirom_offset(0xc0, 0xff, 0x0000, 0xffff, CalculatedSize, 0);
    map_index(0x70, 0x7f, 0x0000, 0x7fff, MAP_LOROM_SRAM, MAP_TYPE_RAM);
    map_index(0xa0, 0xbf, 0x6000, 0x7fff, MAP_LOROM_SRAM, MAP_TYPE_RAM);
    map_WRAM();
    map_WriteProtectROM();
}
void CMemory::Map_HiROMMap(void)
{
    printf("Map_HiROMMap\n");
    map_System();
    map_hirom(0x00, 0x3f, 0x8000, 0xffff, CalculatedSize);
    map_hirom(0x40, 0x7f, 0x0000, 0xffff, CalculatedSize);
    map_hirom(0x80, 0xbf, 0x8000, 0xffff, CalculatedSize);
    map_hirom(0xc0, 0xff, 0x0000, 0xffff, CalculatedSize);
    map_HiROMSRAM();
    map_WRAM();
    map_WriteProtectROM();
}
void CMemory::Map_ExtendedHiROMMap(void)
{
    printf("Map_ExtendedHiROMMap\n");
    map_System();
    map_hirom_offset(0x00, 0x3f, 0x8000, 0xffff, CalculatedSize - 0x400000, 0x400000);
    map_hirom_offset(0x40, 0x7f, 0x0000, 0xffff, CalculatedSize - 0x400000, 0x400000);
    map_hirom_offset(0x80, 0xbf, 0x8000, 0xffff, 0x400000, 0);
    map_hirom_offset(0xc0, 0xff, 0x0000, 0xffff, 0x400000, 0);
    map_HiROMSRAM();
    map_WRAM();
    map_WriteProtectROM();
}
void CMemory::Map_BSCartLoROMMap(uint8 mapping)
{
    printf("Map_BSCartLoROMMap\n");
    map_System();
    if (mapping) {
        map_lorom_offset(0x00, 0x1f, 0x8000, 0xffff, 0x100000, 0);
        map_lorom_offset(0x20, 0x3f, 0x8000, 0xffff, 0x100000, 0x100000);
        map_lorom_offset(0x80, 0x9f, 0x8000, 0xffff, 0x100000, 0x200000);
        map_lorom_offset(0xa0, 0xbf, 0x8000, 0xffff, 0x100000, 0x100000);
    } else {
        map_lorom(0x00, 0x3f, 0x8000, 0xffff, CalculatedSize);
        map_lorom(0x40, 0x7f, 0x0000, 0x7fff, CalculatedSize);
        map_lorom(0x80, 0xbf, 0x8000, 0xffff, CalculatedSize);
        map_lorom(0xc0, 0xff, 0x0000, 0x7fff, CalculatedSize);
    }
    map_LoROMSRAM();
    map_index(0xc0, 0xef, 0x0000, 0xffff, MAP_BSX, MAP_TYPE_RAM);
    map_WRAM();
    map_WriteProtectROM();
}
void CMemory::Map_BSCartHiROMMap(void)
{
    printf("Map_BSCartHiROMMap\n");

    map_System();
    map_hirom_offset(0x00, 0x1f, 0x8000, 0xffff, Multi.cartSizeA, Multi.cartOffsetA);
    map_hirom_offset(0x20, 0x3f, 0x8000, 0xffff, Multi.cartSizeB, Multi.cartOffsetB);
    map_hirom_offset(0x40, 0x5f, 0x0000, 0xffff, Multi.cartSizeA, Multi.cartOffsetA);
    map_hirom_offset(0x60, 0x7f, 0x0000, 0xffff, Multi.cartSizeB, Multi.cartOffsetB);
    map_hirom_offset(0x80, 0x9f, 0x8000, 0xffff, Multi.cartSizeA, Multi.cartOffsetA);
    map_hirom_offset(0xa0, 0xbf, 0x8000, 0xffff, Multi.cartSizeB, Multi.cartOffsetB);
    map_hirom_offset(0xc0, 0xdf, 0x0000, 0xffff, Multi.cartSizeA, Multi.cartOffsetA);
    if ((ROM[Multi.cartOffsetB + 0xFF00] == 0x4D) && (ROM[Multi.cartOffsetB + 0xFF02] == 0x50) &&
        ((ROM[Multi.cartOffsetB + 0xFF06] & 0xF0) == 0x70)) {
        map_hirom_offset(0xe0, 0xff, 0x0000, 0xffff, Multi.cartSizeB, Multi.cartOffsetB);
    } else {
        map_index(0xe0, 0xff, 0x0000, 0xffff, MAP_BSX, MAP_TYPE_RAM);
    }
    map_HiROMSRAM();
    map_WRAM();
    map_WriteProtectROM();
}
uint16 CMemory::checksum_calc_sum(uint8 *data, uint32 length)
{
    uint16 sum = 0;
    for (uint32 i = 0; i < length; i++)
        sum += data[i];
    return (sum);
}
uint16 CMemory::checksum_mirror_sum(uint8 *start, uint32 &length, uint32 mask)
{
    while (!(length & mask) && mask)
        mask >>= 1;
    uint16 part1       = checksum_calc_sum(start, mask);
    uint16 part2       = 0;
    uint32 next_length = length - mask;
    if (next_length) {
        part2 = checksum_mirror_sum(start + mask, next_length, mask >> 1);
        while (next_length < mask) {
            next_length += next_length;
            part2 += part2;
        }
        length = mask + mask;
    }
    return (part1 + part2);
}
void CMemory::Checksum_Calculate(void)
{
    uint16 sum = 0;
    if (Settings.BS && !Settings.BSXItself)
        sum = checksum_calc_sum(ROM, CalculatedSize) - checksum_calc_sum(ROM + (HiROM ? 0xffb0 : 0x7fb0), 48);
    else if (Settings.SPC7110) {
        sum = checksum_calc_sum(ROM, CalculatedSize);
        if (CalculatedSize == 0x300000)
            sum += sum;
    } else {
        if (CalculatedSize & 0x7fff)
            sum = checksum_calc_sum(ROM, CalculatedSize);
        else {
            uint32 length = CalculatedSize;
            sum           = checksum_mirror_sum(ROM, length);
        }
    }
    CalculatedChecksum = sum;
}
const char *CMemory::MapType(void)
{
    return (HiROM ? ((ExtendedFormat != NOPE) ? "ExHiROM" : "HiROM") : "LoROM");
}
void CMemory::MakeRomInfoText(char *romtext)
{
}
bool8 CMemory::match_na(const char *str)
{
    return (strcmp(ROMName, str) == 0);
}
bool8 CMemory::match_nn(const char *str)
{
    return (strncmp(ROMName, str, strlen(str)) == 0);
}
bool8 CMemory::match_nc(const char *str)
{
    return (strncasecmp(ROMName, str, strlen(str)) == 0);
}
bool8 CMemory::match_id(const char *str)
{
    return (strncmp(ROMId, str, strlen(str)) == 0);
}
uint32 CMemory::XPSdecode(const uint8 *data, unsigned &addr, unsigned size)
{
    uint32 offset = 0, shift = 1;
    while (addr < size) {
        uint8 x = data[addr++];
        offset += (x & 0x7f) * shift;
        if (x & 0x80)
            break;
        shift <<= 7;
        offset += shift;
    }
    return offset;
}
bool8 CMemory::ReadUPSPatch(Stream *r, long, int32 &rom_size)
{
    uint8 *data = new uint8[8 * 1024 * 1024];
    uint32 size = 0;
    while (true) {
        int value = r->get_char();
        if (value == EOF)
            break;
        data[size++] = value;
        if (size >= 8 * 1024 * 1024) {
            delete[] data;
            return false;
        }
    }
    if (size < 18) {
        delete[] data;
        return false;
    }
    uint32 addr = 0;
    if (data[addr++] != 'U') {
        delete[] data;
        return false;
    }
    if (data[addr++] != 'P') {
        delete[] data;
        return false;
    }
    if (data[addr++] != 'S') {
        delete[] data;
        return false;
    }
    if (data[addr++] != '1') {
        delete[] data;
        return false;
    }
    uint32 patch_crc32 = caCRC32(data, size - 4);
    uint32 rom_crc32   = caCRC32(snes->mem->ROM, rom_size);
    uint32 px_crc32 =
        (data[size - 12] << 0) + (data[size - 11] << 8) + (data[size - 10] << 16) + (data[size - 9] << 24);
    uint32 py_crc32 = (data[size - 8] << 0) + (data[size - 7] << 8) + (data[size - 6] << 16) + (data[size - 5] << 24);
    uint32 pp_crc32 = (data[size - 4] << 0) + (data[size - 3] << 8) + (data[size - 2] << 16) + (data[size - 1] << 24);
    if (patch_crc32 != pp_crc32) {
        delete[] data;
        return false;
    }
    if (!Settings.IgnorePatchChecksum && (rom_crc32 != px_crc32) && (rom_crc32 != py_crc32)) {
        delete[] data;
        return false;
    }
    uint32 px_size  = XPSdecode(data, addr, size);
    uint32 py_size  = XPSdecode(data, addr, size);
    uint32 out_size = ((uint32)rom_size == px_size) ? py_size : px_size;
    if (out_size > CMemory::MAX_ROM_SIZE) {
        delete[] data;
        return false;
    }
    for (unsigned i = min((uint32)rom_size, out_size); i < max((uint32)rom_size, out_size); i++) {
        snes->mem->ROM[i] = 0x00;
    }
    uint32 relative = 0;
    while (addr < size - 12) {
        relative += XPSdecode(data, addr, size);
        while (addr < size - 12) {
            uint8 x = data[addr++];
            snes->mem->ROM[relative++] ^= x;
            if (!x)
                break;
        }
    }
    rom_size = out_size;
    delete[] data;
    uint32 out_crc32 = caCRC32(snes->mem->ROM, rom_size);
    if (Settings.IgnorePatchChecksum || ((rom_crc32 == px_crc32) && (out_crc32 == py_crc32)) ||
        ((rom_crc32 == py_crc32) && (out_crc32 == px_crc32))) {
        Settings.IsPatched = 3;
        return true;
    } else {
        fprintf(stderr, "WARNING: UPS patching appears to have failed.\nGame may not be playable.\n");
        return true;
    }
}
bool8 CMemory::ReadBPSPatch(Stream *r, long, int32 &rom_size)
{
    uint8 *data = new uint8[8 * 1024 * 1024];
    uint32 size = 0;
    while (true) {
        int value = r->get_char();
        if (value == EOF)
            break;
        data[size++] = value;
        if (size >= 8 * 1024 * 1024) {
            delete[] data;
            return false;
        }
    }
    if (size < 19) {
        delete[] data;
        return false;
    }
    uint32 addr = 0;
    if (data[addr++] != 'B') {
        delete[] data;
        return false;
    }
    if (data[addr++] != 'P') {
        delete[] data;
        return false;
    }
    if (data[addr++] != 'S') {
        delete[] data;
        return false;
    }
    if (data[addr++] != '1') {
        delete[] data;
        return false;
    }
    uint32 patch_crc32 = caCRC32(data, size - 4);
    uint32 rom_crc32   = caCRC32(snes->mem->ROM, rom_size);
    uint32 source_crc32 =
        (data[size - 12] << 0) + (data[size - 11] << 8) + (data[size - 10] << 16) + (data[size - 9] << 24);
    uint32 target_crc32 =
        (data[size - 8] << 0) + (data[size - 7] << 8) + (data[size - 6] << 16) + (data[size - 5] << 24);
    uint32 pp_crc32 = (data[size - 4] << 0) + (data[size - 3] << 8) + (data[size - 2] << 16) + (data[size - 1] << 24);
    if (patch_crc32 != pp_crc32) {
        delete[] data;
        return false;
    }
    if (!Settings.IgnorePatchChecksum && rom_crc32 != source_crc32) {
        delete[] data;
        return false;
    }
    XPSdecode(data, addr, size);
    uint32 target_size   = XPSdecode(data, addr, size);
    uint32 metadata_size = XPSdecode(data, addr, size);
    addr += metadata_size;
    if (target_size > CMemory::MAX_ROM_SIZE) {
        delete[] data;
        return false;
    }
    enum
    {
        SourceRead,
        TargetRead,
        SourceCopy,
        TargetCopy
    };
    uint32 outputOffset = 0, sourceRelativeOffset = 0, targetRelativeOffset = 0;
    uint8 *patched_rom = new uint8[target_size];
    memset(patched_rom, 0, target_size);
    while (addr < size - 12) {
        uint32 length = XPSdecode(data, addr, size);
        uint32 mode   = length & 3;
        length        = (length >> 2) + 1;
        switch ((int)mode) {
            case SourceRead:
                while (length--) {
                    patched_rom[outputOffset] = snes->mem->ROM[outputOffset];
                    outputOffset++;
                }
                break;
            case TargetRead:
                while (length--)
                    patched_rom[outputOffset++] = data[addr++];
                break;
            case SourceCopy:
            case TargetCopy:
                int32 offset   = XPSdecode(data, addr, size);
                bool  negative = offset & 1;
                offset >>= 1;
                if (negative)
                    offset = -offset;
                if (mode == SourceCopy) {
                    sourceRelativeOffset += offset;
                    while (length--)
                        patched_rom[outputOffset++] = snes->mem->ROM[sourceRelativeOffset++];
                } else {
                    targetRelativeOffset += offset;
                    while (length--)
                        patched_rom[outputOffset++] = patched_rom[targetRelativeOffset++];
                }
                break;
        }
    }
    delete[] data;
    uint32 out_crc32 = caCRC32(patched_rom, target_size);
    if (Settings.IgnorePatchChecksum || out_crc32 == target_crc32) {
        memcpy(snes->mem->ROM, patched_rom, target_size);
        rom_size = target_size;
        delete[] patched_rom;
        Settings.IsPatched = 2;
        return true;
    } else {
        delete[] patched_rom;
        fprintf(stderr, "WARNING: BPS patching failed.\nROM has not been altered.\n");
        return false;
    }
}
long CMemory::ReadInt(Stream *r, unsigned nbytes)
{
    long v = 0;
    while (nbytes--) {
        int c = r->get_char();
        if (c == EOF)
            return (-1);
        v = (v << 8) | (c & 0xFF);
    }
    return (v);
}
bool8 CMemory::ReadIPSPatch(Stream *r, long offset, int32 &rom_size)
{
    const int32 IPS_EOF = 0x00454F46l;
    int32       ofs;
    char        fname[6];
    fname[5] = 0;
    for (int i = 0; i < 5; i++) {
        int c = r->get_char();
        if (c == EOF)
            return (0);
        fname[i] = (char)c;
    }
    if (strncmp(fname, "PATCH", 5))
        return (0);
    for (;;) {
        long len, rlen;
        int  rchar;
        ofs = ReadInt(r, 3);
        if (ofs == -1)
            return (0);
        if (ofs == IPS_EOF)
            break;
        ofs -= offset;
        len = ReadInt(r, 2);
        if (len == -1)
            return (0);
        if (len) {
            if (ofs + len > CMemory::MAX_ROM_SIZE)
                return (0);
            while (len--) {
                rchar = r->get_char();
                if (rchar == EOF)
                    return (0);
                snes->mem->ROM[ofs++] = (uint8)rchar;
            }
            if (ofs > rom_size)
                rom_size = ofs;
        } else {
            rlen = ReadInt(r, 2);
            if (rlen == -1)
                return (0);
            rchar = r->get_char();
            if (rchar == EOF)
                return (0);
            if (ofs + rlen > CMemory::MAX_ROM_SIZE)
                return (0);
            while (rlen--)
                snes->mem->ROM[ofs++] = (uint8)rchar;
            if (ofs > rom_size)
                rom_size = ofs;
        }
    }
    ofs = ReadInt(r, 3);
    if (ofs != -1 && ofs - offset < rom_size)
        rom_size = ofs - offset;
    Settings.IsPatched = 1;
    return (1);
}
void CMemory::sha256sum(unsigned char *data, unsigned int length, unsigned char *hash)
{
    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, length);
    sha256_final(&ctx, hash);
}
int32 CMemory::memory_speed(uint32 address)
{
    if (address & 0x408000) {
        if (address & 0x800000)
            return (snes->scpu->FastROMSpeed);
        return (SLOW_ONE_CYCLE);
    }
    if ((address + 0x6000) & 0x4000)
        return (SLOW_ONE_CYCLE);
    if ((address - 0x4000) & 0x7e00)
        return (ONE_CYCLE);
    return (TWO_CYCLES);
}
uint8 CMemory::S9xGetByte(uint32 Address)
{
    int    block      = (Address & 0xffffff) >> MEMMAP_SHIFT;
    uint8 *GetAddress = snes->mem->Map[block];
    int32  speed      = memory_speed(Address);
    uint8  byte;
    if (GetAddress >= (uint8 *)CMemory::MAP_LAST) {
        byte = *(GetAddress + (Address & 0xffff));
        addCyclesInMemoryAccess;
        return (byte);
    }
    switch ((pint)GetAddress) {
        case CMemory::MAP_CPU:
            byte = snes->ppu->S9xGetCPU(Address & 0xffff);
            addCyclesInMemoryAccess;
            return (byte);
        case CMemory::MAP_PPU:
            if (snes->scpu->InDMAorHDMA && (Address & 0xff00) == 0x2100)
                return (snes->OpenBus);
            byte = snes->ppu->S9xGetPPU(Address & 0xffff);
            addCyclesInMemoryAccess;
            return (byte);
        case CMemory::MAP_LOROM_SRAM:
        case CMemory::MAP_SA1RAM:
            byte = *(snes->mem->SRAM + ((((Address & 0xff0000) >> 1) | (Address & 0x7fff)) & snes->mem->SRAMMask));
            addCyclesInMemoryAccess;
            return (byte);
        case CMemory::MAP_LOROM_SRAM_B:
            byte = *(Multi.sramB + ((((Address & 0xff0000) >> 1) | (Address & 0x7fff)) & Multi.sramMaskB));
            addCyclesInMemoryAccess;
            return (byte);
        case CMemory::MAP_HIROM_SRAM:
        case CMemory::MAP_RONLY_SRAM:
            byte = *(snes->mem->SRAM +
                     (((Address & 0x7fff) - 0x6000 + ((Address & 0x1f0000) >> 3)) & snes->mem->SRAMMask));
            addCyclesInMemoryAccess;
            return (byte);
        case CMemory::MAP_BWRAM:
            byte = *(snes->mem->BWRAM + ((Address & 0x7fff) - 0x6000));
            addCyclesInMemoryAccess;
            return (byte);
        case CMemory::MAP_NONE:
        default:
            byte = snes->OpenBus;
            addCyclesInMemoryAccess;
            return (byte);
    }
}
uint16 CMemory::S9xGetWord(uint32 Address, enum s9xwrap_t w)
{
    uint16 word;
    uint32 mask = MEMMAP_MASK & (w == WRAP_PAGE ? 0xff : (w == WRAP_BANK ? 0xffff : 0xffffff));
    if ((Address & mask) == mask) {
        PC_t a;
        word = snes->OpenBus = S9xGetByte(Address);
        switch (w) {
            case WRAP_PAGE:
                a.xPBPC = Address;
                a.B.xPCl++;
                return (word | (S9xGetByte(a.xPBPC) << 8));
            case WRAP_BANK:
                a.xPBPC = Address;
                a.W.xPC++;
                return (word | (S9xGetByte(a.xPBPC) << 8));
            case WRAP_NONE:
            default:
                return (word | (S9xGetByte(Address + 1) << 8));
        }
    }
    int    block      = (Address & 0xffffff) >> MEMMAP_SHIFT;
    uint8 *GetAddress = snes->mem->Map[block];
    int32  speed      = memory_speed(Address);
    if (GetAddress >= (uint8 *)CMemory::MAP_LAST) {
        word = READ_WORD(GetAddress + (Address & 0xffff));
        addCyclesInMemoryAccess_x2;
        return (word);
    }
    switch ((pint)GetAddress) {
        case CMemory::MAP_CPU:
            word = snes->ppu->S9xGetCPU(Address & 0xffff);
            addCyclesInMemoryAccess;
            word |= snes->ppu->S9xGetCPU((Address + 1) & 0xffff) << 8;
            addCyclesInMemoryAccess;
            return (word);
        case CMemory::MAP_PPU:
            if (snes->scpu->InDMAorHDMA) {
                word = snes->OpenBus = S9xGetByte(Address);
                return (word | (S9xGetByte(Address + 1) << 8));
            }
            word = snes->ppu->S9xGetPPU(Address & 0xffff);
            addCyclesInMemoryAccess;
            word |= snes->ppu->S9xGetPPU((Address + 1) & 0xffff) << 8;
            addCyclesInMemoryAccess;
            return (word);
        case CMemory::MAP_LOROM_SRAM:
        case CMemory::MAP_SA1RAM:
            if (snes->mem->SRAMMask >= MEMMAP_MASK)
                word = READ_WORD(snes->mem->SRAM +
                                 ((((Address & 0xff0000) >> 1) | (Address & 0x7fff)) & snes->mem->SRAMMask));
            else
                word =
                    (*(snes->mem->SRAM + ((((Address & 0xff0000) >> 1) | (Address & 0x7fff)) & snes->mem->SRAMMask))) |
                    ((*(snes->mem->SRAM +
                        (((((Address + 1) & 0xff0000) >> 1) | ((Address + 1) & 0x7fff)) & snes->mem->SRAMMask)))
                     << 8);
            addCyclesInMemoryAccess_x2;
            return (word);
        case CMemory::MAP_LOROM_SRAM_B:
            if (Multi.sramMaskB >= MEMMAP_MASK)
                word = READ_WORD(Multi.sramB + ((((Address & 0xff0000) >> 1) | (Address & 0x7fff)) & Multi.sramMaskB));
            else
                word = (*(Multi.sramB + ((((Address & 0xff0000) >> 1) | (Address & 0x7fff)) & Multi.sramMaskB))) |
                       ((*(Multi.sramB +
                           (((((Address + 1) & 0xff0000) >> 1) | ((Address + 1) & 0x7fff)) & Multi.sramMaskB)))
                        << 8);
            addCyclesInMemoryAccess_x2;
            return (word);
        case CMemory::MAP_HIROM_SRAM:
        case CMemory::MAP_RONLY_SRAM:
            if (snes->mem->SRAMMask >= MEMMAP_MASK)
                word = READ_WORD(snes->mem->SRAM +
                                 (((Address & 0x7fff) - 0x6000 + ((Address & 0x1f0000) >> 3)) & snes->mem->SRAMMask));
            else
                word = (*(snes->mem->SRAM +
                          (((Address & 0x7fff) - 0x6000 + ((Address & 0x1f0000) >> 3)) & snes->mem->SRAMMask)) |
                        (*(snes->mem->SRAM + ((((Address + 1) & 0x7fff) - 0x6000 + (((Address + 1) & 0x1f0000) >> 3)) &
                                              snes->mem->SRAMMask))
                         << 8));
            addCyclesInMemoryAccess_x2;
            return (word);
        case CMemory::MAP_BWRAM:
            word = READ_WORD(snes->mem->BWRAM + ((Address & 0x7fff) - 0x6000));
            addCyclesInMemoryAccess_x2;
            return (word);
        case CMemory::MAP_NONE:
        default:
            word = snes->OpenBus | (snes->OpenBus << 8);
            addCyclesInMemoryAccess_x2;
            return (word);
    }
}
void CMemory::S9xSetByte(uint8 Byte, uint32 Address)
{
    int    block      = (Address & 0xffffff) >> MEMMAP_SHIFT;
    uint8 *SetAddress = snes->mem->WriteMap[block];
    int32  speed      = memory_speed(Address);
    if (SetAddress >= (uint8 *)CMemory::MAP_LAST) {
        *(SetAddress + (Address & 0xffff)) = Byte;
        addCyclesInMemoryAccess;
        return;
    }
    switch ((pint)SetAddress) {
        case CMemory::MAP_CPU:
            snes->ppu->S9xSetCPU(Byte, Address & 0xffff);
            addCyclesInMemoryAccess;
            return;
        case CMemory::MAP_PPU:
            if (snes->scpu->InDMAorHDMA && (Address & 0xff00) == 0x2100)
                return;
            snes->ppu->S9xSetPPU(Byte, Address & 0xffff);
            addCyclesInMemoryAccess;
            return;
        case CMemory::MAP_LOROM_SRAM:
            if (snes->mem->SRAMMask) {
                *(snes->mem->SRAM + ((((Address & 0xff0000) >> 1) | (Address & 0x7fff)) & snes->mem->SRAMMask)) = Byte;
                snes->scpu->SRAMModified                                                                        = TRUE;
            }
            addCyclesInMemoryAccess;
            return;
        case CMemory::MAP_LOROM_SRAM_B:
            if (Multi.sramMaskB) {
                *(Multi.sramB + ((((Address & 0xff0000) >> 1) | (Address & 0x7fff)) & Multi.sramMaskB)) = Byte;
                snes->scpu->SRAMModified                                                                = TRUE;
            }
            addCyclesInMemoryAccess;
            return;
        case CMemory::MAP_HIROM_SRAM:
            if (snes->mem->SRAMMask) {
                *(snes->mem->SRAM +
                  (((Address & 0x7fff) - 0x6000 + ((Address & 0x1f0000) >> 3)) & snes->mem->SRAMMask)) = Byte;
                snes->scpu->SRAMModified                                                               = TRUE;
            }
            addCyclesInMemoryAccess;
            return;
        case CMemory::MAP_BWRAM:
            *(snes->mem->BWRAM + ((Address & 0x7fff) - 0x6000)) = Byte;
            snes->scpu->SRAMModified                            = TRUE;
            addCyclesInMemoryAccess;
            return;
        case CMemory::MAP_NONE:
        default:
            addCyclesInMemoryAccess;
            return;
    }
}
void CMemory::S9xSetWord(uint16 Word, uint32 Address, enum s9xwrap_t w, enum s9xwriteorder_t o)
{
    uint32 mask = MEMMAP_MASK & (w == WRAP_PAGE ? 0xff : (w == WRAP_BANK ? 0xffff : 0xffffff));
    if ((Address & mask) == mask) {
        PC_t a;
        if (!o)
            S9xSetByte((uint8)Word, Address);
        switch (w) {
            case WRAP_PAGE:
                a.xPBPC = Address;
                a.B.xPCl++;
                S9xSetByte(Word >> 8, a.xPBPC);
                break;
            case WRAP_BANK:
                a.xPBPC = Address;
                a.W.xPC++;
                S9xSetByte(Word >> 8, a.xPBPC);
                break;
            case WRAP_NONE:
            default:
                S9xSetByte(Word >> 8, Address + 1);
                break;
        }
        if (o)
            S9xSetByte((uint8)Word, Address);
        return;
    }
    int    block      = (Address & 0xffffff) >> MEMMAP_SHIFT;
    uint8 *SetAddress = snes->mem->WriteMap[block];
    int32  speed      = memory_speed(Address);
    if (SetAddress >= (uint8 *)CMemory::MAP_LAST) {
        WRITE_WORD(SetAddress + (Address & 0xffff), Word);
        addCyclesInMemoryAccess_x2;
        return;
    }
    switch ((pint)SetAddress) {
        case CMemory::MAP_CPU:
            if (o) {
                snes->ppu->S9xSetCPU(Word >> 8, (Address + 1) & 0xffff);
                addCyclesInMemoryAccess;
                snes->ppu->S9xSetCPU((uint8)Word, Address & 0xffff);
                addCyclesInMemoryAccess;
                return;
            } else {
                snes->ppu->S9xSetCPU((uint8)Word, Address & 0xffff);
                addCyclesInMemoryAccess;
                snes->ppu->S9xSetCPU(Word >> 8, (Address + 1) & 0xffff);
                addCyclesInMemoryAccess;
                return;
            }
        case CMemory::MAP_PPU:
            if (snes->scpu->InDMAorHDMA) {
                if ((Address & 0xff00) != 0x2100)
                    snes->ppu->S9xSetPPU((uint8)Word, Address & 0xffff);
                if (((Address + 1) & 0xff00) != 0x2100)
                    snes->ppu->S9xSetPPU(Word >> 8, (Address + 1) & 0xffff);
                return;
            }
            if (o) {
                snes->ppu->S9xSetPPU(Word >> 8, (Address + 1) & 0xffff);
                addCyclesInMemoryAccess;
                snes->ppu->S9xSetPPU((uint8)Word, Address & 0xffff);
                addCyclesInMemoryAccess;
                return;
            } else {
                snes->ppu->S9xSetPPU((uint8)Word, Address & 0xffff);
                addCyclesInMemoryAccess;
                snes->ppu->S9xSetPPU(Word >> 8, (Address + 1) & 0xffff);
                addCyclesInMemoryAccess;
                return;
            }
        case CMemory::MAP_LOROM_SRAM:
            if (snes->mem->SRAMMask) {
                if (snes->mem->SRAMMask >= MEMMAP_MASK)
                    WRITE_WORD(snes->mem->SRAM +
                                   ((((Address & 0xff0000) >> 1) | (Address & 0x7fff)) & snes->mem->SRAMMask),
                               Word);
                else {
                    *(snes->mem->SRAM + ((((Address & 0xff0000) >> 1) | (Address & 0x7fff)) & snes->mem->SRAMMask)) =
                        (uint8)Word;
                    *(snes->mem->SRAM + (((((Address + 1) & 0xff0000) >> 1) | ((Address + 1) & 0x7fff)) &
                                         snes->mem->SRAMMask)) = Word >> 8;
                }
                snes->scpu->SRAMModified = TRUE;
            }
            addCyclesInMemoryAccess_x2;
            return;
        case CMemory::MAP_LOROM_SRAM_B:
            if (Multi.sramMaskB) {
                if (Multi.sramMaskB >= MEMMAP_MASK)
                    WRITE_WORD(Multi.sramB + ((((Address & 0xff0000) >> 1) | (Address & 0x7fff)) & Multi.sramMaskB),
                               Word);
                else {
                    *(Multi.sramB + ((((Address & 0xff0000) >> 1) | (Address & 0x7fff)) & Multi.sramMaskB)) =
                        (uint8)Word;
                    *(Multi.sramB +
                      (((((Address + 1) & 0xff0000) >> 1) | ((Address + 1) & 0x7fff)) & Multi.sramMaskB)) = Word >> 8;
                }
                snes->scpu->SRAMModified = TRUE;
            }
            addCyclesInMemoryAccess_x2;
            return;
        case CMemory::MAP_HIROM_SRAM:
            if (snes->mem->SRAMMask) {
                if (snes->mem->SRAMMask >= MEMMAP_MASK)
                    WRITE_WORD(snes->mem->SRAM +
                                   (((Address & 0x7fff) - 0x6000 + ((Address & 0x1f0000) >> 3)) & snes->mem->SRAMMask),
                               Word);
                else {
                    *(snes->mem->SRAM + (((Address & 0x7fff) - 0x6000 + ((Address & 0x1f0000) >> 3)) &
                                         snes->mem->SRAMMask)) = (uint8)Word;
                    *(snes->mem->SRAM + ((((Address + 1) & 0x7fff) - 0x6000 + (((Address + 1) & 0x1f0000) >> 3)) &
                                         snes->mem->SRAMMask)) = Word >> 8;
                }
                snes->scpu->SRAMModified = TRUE;
            }
            addCyclesInMemoryAccess_x2;
            return;
        case CMemory::MAP_BWRAM:
            WRITE_WORD(snes->mem->BWRAM + ((Address & 0x7fff) - 0x6000), Word);
            snes->scpu->SRAMModified = TRUE;
            addCyclesInMemoryAccess_x2;
            return;
        case CMemory::MAP_NONE:
        default:
            addCyclesInMemoryAccess_x2;
            return;
    }
}
void CMemory::S9xSetPCBase(uint32 Address)
{
    snes->scpu->Registers.PBPC = Address & 0xffffff;
    snes->scpu->ICPU.ShiftedPB = Address & 0xff0000;

    uint8 *GetAddress      = snes->mem->Map[(int)((Address & 0xffffff) >> MEMMAP_SHIFT)];
    snes->scpu->MemSpeed   = memory_speed(Address);
    snes->scpu->MemSpeedx2 = snes->scpu->MemSpeed << 1;
    if (GetAddress >= (uint8 *)CMemory::MAP_LAST) {
        snes->scpu->PCBase = GetAddress;
        return;
    }
    switch ((pint)GetAddress) {
        case CMemory::MAP_LOROM_SRAM:
            if ((snes->mem->SRAMMask & MEMMAP_MASK) != MEMMAP_MASK)
                snes->scpu->PCBase = NULL;
            else
                snes->scpu->PCBase = snes->mem->SRAM +
                                     ((((Address & 0xff0000) >> 1) | (Address & 0x7fff)) & snes->mem->SRAMMask) -
                                     (Address & 0xffff);
            return;
        case CMemory::MAP_LOROM_SRAM_B:
            if ((Multi.sramMaskB & MEMMAP_MASK) != MEMMAP_MASK)
                snes->scpu->PCBase = NULL;
            else
                snes->scpu->PCBase = Multi.sramB +
                                     ((((Address & 0xff0000) >> 1) | (Address & 0x7fff)) & Multi.sramMaskB) -
                                     (Address & 0xffff);
            return;
        case CMemory::MAP_HIROM_SRAM:
            if ((snes->mem->SRAMMask & MEMMAP_MASK) != MEMMAP_MASK)
                snes->scpu->PCBase = NULL;
            else
                snes->scpu->PCBase =
                    snes->mem->SRAM +
                    (((Address & 0x7fff) - 0x6000 + ((Address & 0x1f0000) >> 3)) & snes->mem->SRAMMask) -
                    (Address & 0xffff);
            return;
        case CMemory::MAP_BWRAM:
            snes->scpu->PCBase = snes->mem->BWRAM - 0x6000 - (Address & 0x8000);
            return;
        case CMemory::MAP_SA1RAM:
            snes->scpu->PCBase = snes->mem->SRAM;
            return;
        case CMemory::MAP_NONE:
        default:
            snes->scpu->PCBase = NULL;
            return;
    }
}
uint8 *CMemory::S9xGetBasePointer(uint32 Address)
{
    uint8 *GetAddress = snes->mem->Map[(Address & 0xffffff) >> MEMMAP_SHIFT];
    if (GetAddress >= (uint8 *)CMemory::MAP_LAST)
        return (GetAddress);
    switch ((pint)GetAddress) {
        case CMemory::MAP_LOROM_SRAM:
            if ((snes->mem->SRAMMask & MEMMAP_MASK) != MEMMAP_MASK)
                return (NULL);
            return (snes->mem->SRAM + ((((Address & 0xff0000) >> 1) | (Address & 0x7fff)) & snes->mem->SRAMMask) -
                    (Address & 0xffff));
        case CMemory::MAP_LOROM_SRAM_B:
            if ((Multi.sramMaskB & MEMMAP_MASK) != MEMMAP_MASK)
                return (NULL);
            return (Multi.sramB + ((((Address & 0xff0000) >> 1) | (Address & 0x7fff)) & Multi.sramMaskB) -
                    (Address & 0xffff));
        case CMemory::MAP_HIROM_SRAM:
            if ((snes->mem->SRAMMask & MEMMAP_MASK) != MEMMAP_MASK)
                return (NULL);
            return (snes->mem->SRAM +
                    (((Address & 0x7fff) - 0x6000 + ((Address & 0x1f0000) >> 3)) & snes->mem->SRAMMask) -
                    (Address & 0xffff));
        case CMemory::MAP_BWRAM:
            return (snes->mem->BWRAM - 0x6000 - (Address & 0x8000));
        case CMemory::MAP_NONE:
        default:
            return (NULL);
    }
}
uint8 *CMemory::S9xGetMemPointer(uint32 Address)
{
    uint8 *GetAddress = snes->mem->Map[(Address & 0xffffff) >> MEMMAP_SHIFT];
    if (GetAddress >= (uint8 *)CMemory::MAP_LAST)
        return (GetAddress + (Address & 0xffff));
    switch ((pint)GetAddress) {
        case CMemory::MAP_LOROM_SRAM:
            if ((snes->mem->SRAMMask & MEMMAP_MASK) != MEMMAP_MASK)
                return (NULL);
            return (snes->mem->SRAM + ((((Address & 0xff0000) >> 1) | (Address & 0x7fff)) & snes->mem->SRAMMask));
        case CMemory::MAP_LOROM_SRAM_B:
            if ((Multi.sramMaskB & MEMMAP_MASK) != MEMMAP_MASK)
                return (NULL);
            return (Multi.sramB + ((((Address & 0xff0000) >> 1) | (Address & 0x7fff)) & Multi.sramMaskB));
        case CMemory::MAP_HIROM_SRAM:
            if ((snes->mem->SRAMMask & MEMMAP_MASK) != MEMMAP_MASK)
                return (NULL);
            return (snes->mem->SRAM +
                    (((Address & 0x7fff) - 0x6000 + ((Address & 0x1f0000) >> 3)) & snes->mem->SRAMMask));
        case CMemory::MAP_BWRAM:
            return (snes->mem->BWRAM - 0x6000 + (Address & 0x7fff));
        case CMemory::MAP_NONE:
        default:
            return (NULL);
    }
}
void CMemory::S9xUnpackStatus(void)
{
    snes->scpu->ICPU._Zero     = (snes->scpu->Registers.PL & Zero) == 0;
    snes->scpu->ICPU._Negative = (snes->scpu->Registers.PL & Negative);
    snes->scpu->ICPU._Carry    = (snes->scpu->Registers.PL & Carry);
    snes->scpu->ICPU._Overflow = (snes->scpu->Registers.PL & Overflow) >> 6;
}
void CMemory::S9xPackStatus(void)
{
    snes->scpu->Registers.PL &= ~(Zero | Negative | Carry | Overflow);
    snes->scpu->Registers.PL |= snes->scpu->ICPU._Carry | ((snes->scpu->ICPU._Zero == 0) << 1) |
                                (snes->scpu->ICPU._Negative & 0x80) | (snes->scpu->ICPU._Overflow << 6);
}
