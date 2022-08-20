#include <cstdint>
#include <stdint.h>
#include <stdlib.h>
#include "io.h"
#include "mem.h"
#include "snem.h"
#include "ppu.h"
#include "spc.h"
#include "cpu.h"

#define sgetr(c) (((c >> _rgb_r_shift_16) & 0x1F) << 3)
#define sgetg(c) (((c >> _rgb_g_shift_16) & 0x3F) << 2)
#define sgetb(c) (((c >> _rgb_b_shift_16) & 0x1F) << 3)

PPU::PPU(SNES *_snes)
{
    snes = _snes;
}
void PPU::window_logic(int windena, int windlogic, uint16_t *w, uint16_t *w2, uint16_t *w3)
{
    for (int x = 0; x < 256; x++) {
        if (windena & 2) {
            if (x < w1left || x > w1right)
                w2[x] = 0xFFFF;
            else
                w2[x] = 0;
        } else
            w2[x] = 0;
        if (windena & 1)
            w2[x] ^= 0xFFFF;
        if (windena & 8) {
            if (x < w2left || x > w2right)
                w3[x] = 0xFFFF;
            else
                w3[x] = 0;
        } else
            w3[x] = 0;
        if (windena & 4)
            w3[x] ^= 0xFFFF;
        switch (windlogic & 3) {
            case 0:
                w[x] = w2[x] | w3[x];
                break;
            case 1:
                w[x] = w2[x] | w3[x];
                break;
            case 2:
                w[x] = w2[x] ^ w3[x];
                break;
            case 3:
                w[x] = ~(w2[x] ^ w3[x]);
                break;
        }
    }
}
uint16_t PPU::cgadd(uint32_t x, uint32_t y)
{
    uint32_t sum     = x + y;
    uint32_t carries = (sum - ((x ^ y) & 0x0821)) & 0x10820;
    return (sum - carries) | (carries - (carries >> 5));
}
uint16_t PPU::cgaddh(uint32_t x, uint32_t y)
{
    return (x + y - ((x ^ y) & 0x0821)) >> 1;
}
uint32_t PPU::cgsub(uint32_t x, uint32_t y)
{
    uint32_t sub     = x - y;
    uint32_t borrows = (~(sub + ((~(x ^ y)) & 0x10820))) & 0x10820;
    sub += borrows;
    return sub & ~(borrows - (borrows >> 5));
}
uint16_t PPU::cgsubh(uint32_t x, uint32_t y)
{
    uint32_t sub     = x - y;
    uint32_t borrows = (~(sub + ((~(x ^ y)) & 0x10820))) & 0x10820;
    sub += borrows;
    return ((sub & ~(borrows - (borrows >> 5))) & 0xf79e) >> 1;
}
void PPU::initppu()
{
    int c, d;
    allegro_init();
    set_color_depth(16);
    set_gfx_mode(GFX_AUTODETECT_WINDOWED, 512, 448, 0, 0);
    otherscr = create_bitmap(640, 225);
    mainscr  = create_bitmap(640, 225);
    subscr   = create_bitmap(640, 225);
    sysb     = create_system_bitmap(256, 224);
    vramb    = (uint8_t *)malloc(0x10000);
    memset(vramb, 0, 0x10000);
    vram = (uint16_t *)vramb;
    for (c = 0; c < 4; c++) {
        for (d = 0; d < 4; d++) {
            bitlookup[0][c][d] = 0;
            if (c & 1)
                bitlookup[0][c][d] |= 0x10000;
            if (d & 1)
                bitlookup[0][c][d] |= 0x20000;
            if (c & 2)
                bitlookup[0][c][d] |= 1;
            if (d & 2)
                bitlookup[0][c][d] |= 2;
            bitlookup[1][c][d] = 0;
            if (c & 2)
                bitlookup[1][c][d] |= 0x10000;
            if (d & 2)
                bitlookup[1][c][d] |= 0x20000;
            if (c & 1)
                bitlookup[1][c][d] |= 1;
            if (d & 1)
                bitlookup[1][c][d] |= 2;
            masklookup[0][c] = 0;
            if (c & 1)
                masklookup[0][c] |= 0xFFFF0000;
            if (c & 2)
                masklookup[0][c] |= 0xFFFF;
            masklookup[1][c] = 0;
            if (c & 2)
                masklookup[1][c] |= 0xFFFF0000;
            if (c & 1)
                masklookup[1][c] |= 0xFFFF;
        }
        for (d = 0; d < 4; d++) {
            bitlookuph[0][c][d] = 0;
            if (c & 2)
                bitlookuph[0][c][d] |= 1;
            if (d & 2)
                bitlookuph[0][c][d] |= 2;
            bitlookuph[1][c][d] = 0;
            if (c & 1)
                bitlookuph[1][c][d] |= 1;
            if (d & 1)
                bitlookuph[1][c][d] |= 2;
            masklookuph[0][c] = 0;
            if (c & 2)
                masklookuph[0][c] |= 0xFFFF;
            masklookuph[1][c] = 0;
            if (c & 1)
                masklookuph[1][c] |= 0xFFFF;
        }
    }
    for (c = 0; c < 16; c++) {
        collookup[c] = (c << 4) | (c << 20);
    }
}
void PPU::resetppu()
{
    int c, d;
    portctrl = vramaddr = 0;
    palbuffer           = 0;
    memset(vram, 0, 0x10000);
    ppumask = 0;
    for (c = 0; c < 10; c++) {
        for (d = 0; d < 164; d++) {
            window[c][d] = 0xFFFFFFFF;
        }
    }
    for (d = 0; d < 164; d++) {
        window[8][d] = 0;
    }
}
void PPU::recalcwindows()
{
    int       x;
    uint16_t *w;
    uint16_t  w2[256], w3[256];
    w = (uint16_t *)&window[0][32];
    if (windena1 & 0xA && !windowdisable) {
        window_logic((windena1 & 0xF), (windlogic & 3), w, w2, w3);
    } else {
        for (x = 0; x < 256; x++) {
            w[x] = 0xFFFF;
        }
    }
    w = (uint16_t *)&window[1][32];
    if (windena1 & 0xA0 && !windowdisable) {
        window_logic((windena1 >> 4), ((windlogic >> 2) & 3), w, w2, w3);

    } else {
        for (x = 0; x < 256; x++) {
            w[x] = 0xFFFF;
        }
    }
    w = (uint16_t *)&window[2][32];
    if (windena2 & 0xA && !windowdisable) {
        window_logic((windena2 & 0xF), ((windlogic >> 4) & 3), w, w2, w3);

    } else {
        for (x = 0; x < 256; x++) {
            w[x] = 0xFFFF;
        }
    }
    w = (uint16_t *)&window[3][32];
    if (windena2 & 0xA0 && !windowdisable) {
        window_logic((windena2 >> 4), ((windlogic >> 6) & 3), w, w2, w3);

    } else {
        for (x = 0; x < 256; x++) {
            w[x] = 0xFFFF;
        }
    }
    w = (uint16_t *)&window[9][32];
    if (windena3 & 0xA && !windowdisable) {
        window_logic((windena3 & 0xF), (windlogic2 & 3), w, w2, w3);

    } else {
        for (x = 0; x < 256; x++) {
            w[x] = 0xFFFF;
        }
    }
    w = (uint16_t *)&window[5][32];
    if (windena3 & 0xA0 && !windowdisable) {
        window_logic((windena3 >> 4), ((windlogic2 >> 2) & 3), w, w2, w3);

    } else {
        for (x = 0; x < 256; x++) {
            w[x] = 0xFFFF;
        }
    }
    for (x = 0; x < 128; x++) {
        window[6][x + 32] = window[5][x + 32] ^ 0xFFFFFFFF;
    }
}

void PPU::doblit()
{
    blit(otherscr, sysb, 64, 1, 0, 0, 256, 224);
    stretch_blit(sysb, screen, 0, 0, 256, 224, 0, 0, 512, 448);
}
void PPU::docolour(uint16_t *pw, uint16_t *pw2, uint16_t *pw3, uint16_t *pw4)
{
    int       x;
    uint16_t  dat;
    uint16_t *pal = pallookup[screna & 15];
    switch ((cgadsub >> 6) | ((cgwsel & 2) << 1)) {
        case 0:
            if (!fixedcol) {
                for (x = 64; x < 320; x++)
                    pw[x] = pallookup[screna & 15][pw3[x] & 255];
            } else {
                for (x = 64; x < 320; x++) {
                    if (pw3[x] & 0x4000 && pw4[x]) {
                        pw[x] = pallookup[screna & 15][pw3[x] & 255];
                        dat   = fixedcol;
                        pw[x] = cgadd(pw[x], dat);
                    } else
                        pw[x] = pallookup[screna & 15][pw3[x] & 255];
                }
            }
            break;
        case 4:
            for (x = 64; x < 320; x++) {
                if (pw3[x] & 0x4000 && pw4[x]) {
                    pw[x] = pallookup[screna & 15][pw3[x] & 255];
                    if (!(pw2[x] & 0x2000))
                        dat = pallookup[screna & 15][pw2[x] & 255];
                    else
                        dat = fixedcol;
                    pw[x] = cgadd(pw[x], dat);
                } else
                    pw[x] = pallookup[screna & 15][pw3[x] & 255];
            }
            break;
        case 1:
            for (x = 64; x < 320; x++) {
                if (pw3[x] & 0x4000 && pw4[x]) {
                    pw[x] = pallookup[screna & 15][pw3[x] & 255];
                    dat   = fixedcol;
                    pw[x] = ((pw[x] & 0xF7DE) + (dat & 0xF7DE)) >> 1;
                } else
                    pw[x] = pallookup[screna & 15][pw3[x] & 255];
            }
            break;
        case 5:
            for (x = 64; x < 320; x++) {
                if (pw3[x] & 0x4000 && pw4[x]) {
                    pw[x] = pal[pw3[x] & 255];
                    if (!(pw2[x] & 0x2000)) {
                        dat   = pal[pw2[x] & 255];
                        pw[x] = ((pw[x] & 0xF7DE) + (dat & 0xF7DE)) >> 1;
                    } else
                        pw[x] = cgadd(pw[x], fixedcol);
                } else
                    pw[x] = pal[pw3[x] & 255];
            }
            break;
        case 2:
        case 6:
            for (x = 64; x < 320; x++) {
                if (pw3[x] & 0x4000 && pw4[x]) {
                    pw[x] = pallookup[screna & 15][pw3[x] & 255];
                    if (cgwsel & 2 && !(pw2[x] & 0x2000))
                        dat = pallookup[screna & 15][pw2[x] & 255];
                    else
                        dat = fixedcol;
                    pw[x] = cgsub(pw[x], dat);
                } else
                    pw[x] = pallookup[screna & 15][pw3[x] & 255];
            }
            break;
        case 3:
        case 7:
            for (x = 64; x < 320; x++) {
                if (pw3[x] & 0x4000 && pw4[x]) {
                    pw[x] = pallookup[screna & 15][pw3[x] & 255];
                    if (cgwsel & 2 && !(pw2[x] & 0x2000))
                        dat = pallookup[screna & 15][pw2[x] & 255];
                    else
                        dat = fixedcol;
                    pw[x] = cgsubh(pw[x], dat);
                } else
                    pw[x] = pallookup[screna & 15][pw3[x] & 255];
            }
            break;
    }
}
void PPU::drawline(int line)
{
    uint32_t  aa, bb, cc, dd, arith;
    int       ma, mb, mc, md, hoff, voff, cx, cy;
    int       c, d, x, xx, xxx, pri, y, yy, ss, sprc;
    uint16_t  addr, tile, dat, baseaddr;
    reg       b1, b2, b3, b4, b5;
    uint32_t *p;
    uint16_t *pw, *pw2, *pw3, *pw4;
    uint32_t *wp;
    int       col;
    int       l;
    uint8_t   layers = (main | sub);
    int       xsize, ysize;
    uint16_t *xlk;
    uint8_t   temp;
    uint8_t   wmask;
    lastline = line;
    if (windowschanged) {
        recalcwindows();
        windowschanged = 0;
    }
    if (spraddr & 0x200)
        pri = (spraddr & 0x1F) << 2;
    else
        pri = spraddr >> 2;
    for (ss = 0; ss < 2; ss++) {
        if (ss) {
            b      = mainscr;
            layers = main & ~ppumask;
            wmask  = wmaskmain;
        } else {
            b      = subscr;
            layers = sub & ~ppumask;
            wmask  = wmasksub;
        }
        if (!line)
            spraddr = spraddrs;
        if (screna & 0x80) {
            hline(b, 64, line, 320, 0);
        } else {
            if (ss)
                hline(b, 64, line, 320, (cgadsub & 0x20) ? 0x4000 : 0);
            else
                hline(b, 64, line, 320, 0x2000);
            for (d = 0; d < 12; d++) {
                c   = draworder[mode][d];
                pri = (c & 4) ? 0x2000 : 0;
                if (c & 8) {
                    if (layers & 16 && !(ppumask & 16)) {
                        pri  = c & 3;
                        addr = 0x1FC;
                        for (sprc = 127; sprc >= 0; sprc--) {
                            c   = sprc;
                            dat = sprram[(c >> 2) + 512];
                            dat >>= ((c & 3) << 1);
                            dat &= 3;
                            y = (sprram[addr + 1] + 1) & 255;
                            x = sprram[addr];
                            x = (x + ((dat & 1) << 8) + 64) & 511;
                            if (y > 239)
                                y |= 0xFFFFFF00;

                            if (line >= y && line < (y + (sprsize[sprsizeidx][(dat >> 1) & 1] << 3)) &&
                                pri == ((sprram[addr + 3] >> 4) & 3) && ((x < 320))) {
                                if (wmask & 0x10)
                                    wp = (uint32_t *)(((uint8_t *)window[9]) + (x << 1));
                                else
                                    wp = (uint32_t *)(((uint8_t *)window[7]) + (x << 1));
                                p    = (uint32_t *)(b->line[line] + (x << 1));
                                tile = ((sprram[addr + 2] | ((sprram[addr + 3] & 1) << 8)) << 4) + sprbase;
                                y    = line - y;
                                if (sprram[addr + 3] & 0x80) {
                                    y = y ^ ((sprsize[sprsizeidx][(dat >> 1) & 1] << 3) - 1);
                                }
                                tile += (y & 7);
                                tile += ((y & ~7) << 5);
                                dat >>= 1;
                                col = ((sprram[addr + 3] >> 1) & 7) | 8;
                                if ((col & 4) && (cgadsub & 0x10))
                                    arith = 0x40004000;
                                else
                                    arith = 0;
                                col = collookup[col] | arith;
                                if (sprram[addr + 3] & 0x40) {
                                    tile += ((sprsize[sprsizeidx][dat & 1] - 1) << 4);
                                    for (xxx = 0; xxx < sprsize[sprsizeidx][dat & 1]; xxx++) {
                                        b1.w = vram[tile & 0x7FFF];
                                        b2.w = vram[(tile + 8) & 0x7FFF];
                                        b3.w = b1.w | b2.w;
                                        b3.b.l |= b3.b.h;
                                        if (b3.b.l) {
                                            p[0] &= ~(masklookup[1][b3.b.l & 3] & wp[0]);
                                            p[0] |= (bitlookup[1][b1.b.l & 3][b1.b.h & 3] |
                                                     (bitlookup[1][b2.b.l & 3][b2.b.h & 3] << 2) |
                                                     (col & masklookup[1][b3.b.l & 3])) &
                                                    wp[0];
                                            p[1] &= ~(masklookup[1][(b3.b.l >> 2) & 3] & wp[1]);
                                            p[1] |= (bitlookup[1][(b1.b.l >> 2) & 3][(b1.b.h >> 2) & 3] |
                                                     (bitlookup[1][(b2.b.l >> 2) & 3][(b2.b.h >> 2) & 3] << 2) |
                                                     (col & masklookup[1][(b3.b.l >> 2) & 3])) &
                                                    wp[1];
                                            p[2] &= ~(masklookup[1][(b3.b.l >> 4) & 3] & wp[2]);
                                            p[2] |= (bitlookup[1][(b1.b.l >> 4) & 3][(b1.b.h >> 4) & 3] |
                                                     (bitlookup[1][(b2.b.l >> 4) & 3][(b2.b.h >> 4) & 3] << 2) |
                                                     (col & masklookup[1][(b3.b.l >> 4) & 3])) &
                                                    wp[2];
                                            p[3] &= ~(masklookup[1][(b3.b.l >> 6) & 3] & wp[3]);
                                            p[3] |= (bitlookup[1][(b1.b.l >> 6) & 3][(b1.b.h >> 6) & 3] |
                                                     (bitlookup[1][(b2.b.l >> 6) & 3][(b2.b.h >> 6) & 3] << 2) |
                                                     (col & masklookup[1][(b3.b.l >> 6) & 3])) &
                                                    wp[3];
                                        }
                                        p += 4;
                                        wp += 4;
                                        tile -= 16;
                                    }
                                } else {
                                    for (xxx = 0; xxx < sprsize[sprsizeidx][dat & 1]; xxx++) {
                                        b1.w = vram[tile & 0x7FFF];
                                        b2.w = vram[(tile + 8) & 0x7FFF];
                                        b3.w = b1.w | b2.w;
                                        b3.b.l |= b3.b.h;
                                        if (b3.b.l) {
                                            p[3] &= ~(masklookup[0][b3.b.l & 3] & wp[3]);
                                            p[3] |= (bitlookup[0][b1.b.l & 3][b1.b.h & 3] |
                                                     (bitlookup[0][b2.b.l & 3][b2.b.h & 3] << 2) |
                                                     (col & masklookup[0][b3.b.l & 3])) &
                                                    wp[3];
                                            p[2] &= ~(masklookup[0][(b3.b.l >> 2) & 3] & wp[2]);
                                            p[2] |= (bitlookup[0][(b1.b.l >> 2) & 3][(b1.b.h >> 2) & 3] |
                                                     (bitlookup[0][(b2.b.l >> 2) & 3][(b2.b.h >> 2) & 3] << 2) |
                                                     (col & masklookup[0][(b3.b.l >> 2) & 3])) &
                                                    wp[2];
                                            p[1] &= ~(masklookup[0][(b3.b.l >> 4) & 3] & wp[1]);
                                            p[1] |= (bitlookup[0][(b1.b.l >> 4) & 3][(b1.b.h >> 4) & 3] |
                                                     (bitlookup[0][(b2.b.l >> 4) & 3][(b2.b.h >> 4) & 3] << 2) |
                                                     (col & masklookup[0][(b3.b.l >> 4) & 3])) &
                                                    wp[1];
                                            p[0] &= ~(masklookup[0][(b3.b.l >> 6) & 3] & wp[0]);
                                            p[0] |= (bitlookup[0][(b1.b.l >> 6) & 3][(b1.b.h >> 6) & 3] |
                                                     (bitlookup[0][(b2.b.l >> 6) & 3][(b2.b.h >> 6) & 3] << 2) |
                                                     (col & masklookup[0][(b3.b.l >> 6) & 3])) &
                                                    wp[0];
                                        }
                                        p += 4;
                                        wp += 4;
                                        tile += 16;
                                    }
                                }
                            }
                            addr -= 4;
                        }
                    }
                } else if (layers & (1 << (c & 3))) {
                    c &= 3;
                    if (cgadsub & (1 << c))
                        arith = 0x40004000;
                    else
                        arith = 0;
                    p = (uint32_t *)(b->line[line] + ((64 - (xscroll[c] & 7)) << 1));
                    if (wmask & (1 << c))
                        wp = (uint32_t *)((uint8_t *)window[c] + ((64 - (xscroll[c] & 7)) << 1));
                    else
                        wp = (uint32_t *)((uint8_t *)window[7] + ((64 - (xscroll[c] & 7)) << 1));
                    l  = (line + yscroll[c]) & 1023;
                    xx = xscroll[c] >> 3;
                    if ((mode & 7) != 7) {
                        if (size[c] & 1)
                            xsize = 63;
                        else
                            xsize = 31;
                        if (size[c] & 2)
                            ysize = 511;
                        else
                            ysize = 255;
                        if (tilesize & (1 << c))
                            baseaddr = bg[c] + ylookup[(size[c] & 3)][l >> 4];
                        else
                            baseaddr = bg[c] + ylookup[(size[c] & 3)][(l >> 3) & 63];
                        xlk = xlookup[size[c] & 1];
                        for (x = 0; x < 33; x++) {
                            if (tilesize & (1 << c))
                                addr = baseaddr + xlk[((x + xx) >> 1) & 63];
                            else
                                addr = baseaddr + xlk[(x + xx) & 63];
                            dat = vram[addr & 0x7FFF];
                            if ((dat & 0x2000) != pri)
                                goto skiptile;
                            tile = dat & 0x3FF;
                            col  = (dat >> 10) & 7;
                            switch (bgtype[mode & 7][c]) {
                                case 2:
                                    tile <<= 3;
                                    if (tilesize & (1 << c)) {
                                        if (dat & 0x8000)
                                            tile += (((l ^ 8) & 8) << 4);
                                        else
                                            tile += ((l & 8) << 4);
                                        if (dat & 0x4000)
                                            tile += (((x + xx + 1) & 1) << 3);
                                        else
                                            tile += (((x + xx) & 1) << 3);
                                    }
                                    if (dat & 0x8000)
                                        tile += ((l & 7) ^ 7) + chr[c];
                                    else
                                        tile += (l & 7) + chr[c];
                                    tile &= 0x7FFF;
                                    b1.w   = vram[tile];
                                    b3.b.l = (b1.b.l | b1.b.h);
                                    if (b3.b.l) {
                                        col = (collookup[col] >> 2) | arith;
                                        if (dat & 0x4000) {
                                            p[0] &= ~(masklookup[1][b3.b.l & 3] & wp[0]);
                                            p[0] |= (bitlookup[1][b1.b.l & 3][b1.b.h & 3] |
                                                     (col & masklookup[1][b3.b.l & 3])) &
                                                    wp[0];
                                            p[1] &= ~(masklookup[1][(b3.b.l >> 2) & 3] & wp[1]);
                                            p[1] |= (bitlookup[1][(b1.b.l >> 2) & 3][(b1.b.h >> 2) & 3] |
                                                     (col & masklookup[1][(b3.b.l >> 2) & 3])) &
                                                    wp[1];
                                            p[2] &= ~(masklookup[1][(b3.b.l >> 4) & 3] & wp[2]);
                                            p[2] |= (bitlookup[1][(b1.b.l >> 4) & 3][(b1.b.h >> 4) & 3] |
                                                     (col & masklookup[1][(b3.b.l >> 4) & 3])) &
                                                    wp[2];
                                            p[3] &= ~(masklookup[1][(b3.b.l >> 6) & 3] & wp[3]);
                                            p[3] |= (bitlookup[1][(b1.b.l >> 6) & 3][(b1.b.h >> 6) & 3] |
                                                     (col & masklookup[1][(b3.b.l >> 6) & 3])) &
                                                    wp[3];
                                        } else {
                                            p[3] &= ~(masklookup[0][b3.b.l & 3] & wp[3]);
                                            p[3] |= (bitlookup[0][b1.b.l & 3][b1.b.h & 3] |
                                                     (col & masklookup[0][b3.b.l & 3])) &
                                                    wp[3];
                                            p[2] &= ~(masklookup[0][(b3.b.l >> 2) & 3] & wp[2]);
                                            p[2] |= (bitlookup[0][(b1.b.l >> 2) & 3][(b1.b.h >> 2) & 3] |
                                                     (col & masklookup[0][(b3.b.l >> 2) & 3])) &
                                                    wp[2];
                                            p[1] &= ~(masklookup[0][(b3.b.l >> 4) & 3] & wp[1]);
                                            p[1] |= (bitlookup[0][(b1.b.l >> 4) & 3][(b1.b.h >> 4) & 3] |
                                                     (col & masklookup[0][(b3.b.l >> 4) & 3])) &
                                                    wp[1];
                                            p[0] &= ~(masklookup[0][(b3.b.l >> 6) & 3] & wp[0]);
                                            p[0] |= (bitlookup[0][(b1.b.l >> 6) & 3][(b1.b.h >> 6) & 3] |
                                                     (col & masklookup[0][(b3.b.l >> 6) & 3])) &
                                                    wp[0];
                                        }
                                    }
                                    break;
                                case 3:
                                    tile <<= 3;
                                    if (dat & 0x8000)
                                        tile += ((l & 7) ^ 7) + chr[c];
                                    else
                                        tile += (l & 7) + chr[c];
                                    b1.w   = vram[tile & 0x7FFF];
                                    b3.b.l = (b1.b.l | b1.b.h);
                                    if (b3.b.l) {
                                        col = (collookup[col] >> 2) | arith;
                                        col |= (c << 5) | (c << 21);
                                        if (dat & 0x4000) {
                                            p[0] &= ~(masklookup[1][b3.b.l & 3] & wp[0]);
                                            p[0] |= (bitlookup[1][b1.b.l & 3][b1.b.h & 3] |
                                                     (col & masklookup[1][b3.b.l & 3])) &
                                                    wp[0];
                                            p[1] &= ~(masklookup[1][(b3.b.l >> 2) & 3] & wp[1]);
                                            p[1] |= (bitlookup[1][(b1.b.l >> 2) & 3][(b1.b.h >> 2) & 3] |
                                                     (col & masklookup[1][(b3.b.l >> 2) & 3])) &
                                                    wp[1];
                                            p[2] &= ~(masklookup[1][(b3.b.l >> 4) & 3] & wp[2]);
                                            p[2] |= (bitlookup[1][(b1.b.l >> 4) & 3][(b1.b.h >> 4) & 3] |
                                                     (col & masklookup[1][(b3.b.l >> 4) & 3])) &
                                                    wp[2];
                                            p[3] &= ~(masklookup[1][(b3.b.l >> 6) & 3] & wp[3]);
                                            p[3] |= (bitlookup[1][(b1.b.l >> 6) & 3][(b1.b.h >> 6) & 3] |
                                                     (col & masklookup[1][(b3.b.l >> 6) & 3])) &
                                                    wp[3];
                                        } else {
                                            p[3] &= ~(masklookup[0][b3.b.l & 3] & wp[3]);
                                            p[3] |= (bitlookup[0][b1.b.l & 3][b1.b.h & 3] |
                                                     (col & masklookup[0][b3.b.l & 3])) &
                                                    wp[3];
                                            p[2] &= ~(masklookup[0][(b3.b.l >> 2) & 3] & wp[2]);
                                            p[2] |= (bitlookup[0][(b1.b.l >> 2) & 3][(b1.b.h >> 2) & 3] |
                                                     (col & masklookup[0][(b3.b.l >> 2) & 3])) &
                                                    wp[2];
                                            p[1] &= ~(masklookup[0][(b3.b.l >> 4) & 3] & wp[1]);
                                            p[1] |= (bitlookup[0][(b1.b.l >> 4) & 3][(b1.b.h >> 4) & 3] |
                                                     (col & masklookup[0][(b3.b.l >> 4) & 3])) &
                                                    wp[1];
                                            p[0] &= ~(masklookup[0][(b3.b.l >> 6) & 3] & wp[0]);
                                            p[0] |= (bitlookup[0][(b1.b.l >> 6) & 3][(b1.b.h >> 6) & 3] |
                                                     (col & masklookup[0][(b3.b.l >> 6) & 3])) &
                                                    wp[0];
                                        }
                                    }
                                    break;
                                case 4:
                                    tile <<= 4;
                                    if (tilesize & (1 << c)) {
                                        if (dat & 0x8000)
                                            tile += (((l ^ 8) & 8) << 5);
                                        else
                                            tile += ((l & 8) << 5);
                                        if (dat & 0x4000)
                                            tile += (((x + xx + 1) & 1) << 4);
                                        else
                                            tile += (((x + xx) & 1) << 4);
                                    }
                                    if (dat & 0x8000)
                                        tile += ((l & 7) ^ 7) + chr[c];
                                    else
                                        tile += (l & 7) + chr[c];
                                    b1.w = vram[tile & 0x7FFF];
                                    b2.w = vram[(tile + 8) & 0x7FFF];
                                    b3.w = b1.w | b2.w;
                                    b3.b.l |= b3.b.h;
                                    if (b3.b.l) {
                                        col = collookup[col] | arith;
                                        if (dat & 0x4000) {
                                            p[0] &= ~(masklookup[1][b3.b.l & 3] & wp[0]);
                                            p[0] |= (bitlookup[1][b1.b.l & 3][b1.b.h & 3] |
                                                     (bitlookup[1][b2.b.l & 3][b2.b.h & 3] << 2) |
                                                     (col & masklookup[1][b3.b.l & 3])) &
                                                    wp[0];
                                            p[1] &= ~(masklookup[1][(b3.b.l >> 2) & 3] & wp[1]);
                                            p[1] |= (bitlookup[1][(b1.b.l >> 2) & 3][(b1.b.h >> 2) & 3] |
                                                     (bitlookup[1][(b2.b.l >> 2) & 3][(b2.b.h >> 2) & 3] << 2) |
                                                     (col & masklookup[1][(b3.b.l >> 2) & 3])) &
                                                    wp[1];
                                            p[2] &= ~(masklookup[1][(b3.b.l >> 4) & 3] & wp[2]);
                                            p[2] |= (bitlookup[1][(b1.b.l >> 4) & 3][(b1.b.h >> 4) & 3] |
                                                     (bitlookup[1][(b2.b.l >> 4) & 3][(b2.b.h >> 4) & 3] << 2) |
                                                     (col & masklookup[1][(b3.b.l >> 4) & 3])) &
                                                    wp[2];
                                            p[3] &= ~(masklookup[1][(b3.b.l >> 6) & 3] & wp[3]);
                                            p[3] |= (bitlookup[1][(b1.b.l >> 6) & 3][(b1.b.h >> 6) & 3] |
                                                     (bitlookup[1][(b2.b.l >> 6) & 3][(b2.b.h >> 6) & 3] << 2) |
                                                     (col & masklookup[1][(b3.b.l >> 6) & 3])) &
                                                    wp[3];
                                        } else {
                                            p[3] &= ~(masklookup[0][b3.b.l & 3] & wp[3]);
                                            p[3] |= (bitlookup[0][b1.b.l & 3][b1.b.h & 3] |
                                                     (bitlookup[0][b2.b.l & 3][b2.b.h & 3] << 2) |
                                                     (col & masklookup[0][b3.b.l & 3])) &
                                                    wp[3];
                                            p[2] &= ~(masklookup[0][(b3.b.l >> 2) & 3] & wp[2]);
                                            p[2] |= (bitlookup[0][(b1.b.l >> 2) & 3][(b1.b.h >> 2) & 3] |
                                                     (bitlookup[0][(b2.b.l >> 2) & 3][(b2.b.h >> 2) & 3] << 2) |
                                                     (col & masklookup[0][(b3.b.l >> 2) & 3])) &
                                                    wp[2];
                                            p[1] &= ~(masklookup[0][(b3.b.l >> 4) & 3] & wp[1]);
                                            p[1] |= (bitlookup[0][(b1.b.l >> 4) & 3][(b1.b.h >> 4) & 3] |
                                                     (bitlookup[0][(b2.b.l >> 4) & 3][(b2.b.h >> 4) & 3] << 2) |
                                                     (col & masklookup[0][(b3.b.l >> 4) & 3])) &
                                                    wp[1];
                                            p[0] &= ~(masklookup[0][(b3.b.l >> 6) & 3] & wp[0]);
                                            p[0] |= (bitlookup[0][(b1.b.l >> 6) & 3][(b1.b.h >> 6) & 3] |
                                                     (bitlookup[0][(b2.b.l >> 6) & 3][(b2.b.h >> 6) & 3] << 2) |
                                                     (col & masklookup[0][(b3.b.l >> 6) & 3])) &
                                                    wp[0];
                                        }
                                    }
                                    break;
                                case 5:
                                    tile <<= 4;
                                    if (tilesize & (1 << c)) {
                                        if (dat & 0x8000)
                                            tile += (((l ^ 8) & 8) << 5);
                                        else
                                            tile += ((l & 8) << 5);
                                        if (dat & 0x4000)
                                            tile += (((x + xx + 1) & 1) << 4);
                                        else
                                            tile += (((x + xx) & 1) << 4);
                                    }
                                    if (dat & 0x8000)
                                        tile += ((l & 7) ^ 7) + chr[c];
                                    else
                                        tile += (l & 7) + chr[c];
                                    b1.w   = vram[tile & 0x7FFF];
                                    b2.w   = vram[(tile + 8) & 0x7FFF];
                                    b3.w   = vram[(tile + 16) & 0x7FFF];
                                    b4.w   = vram[(tile + 24) & 0x7FFF];
                                    b5.b.l = b1.b.l | b1.b.h | b2.b.l | b2.b.h;
                                    b5.b.h = b3.b.l | b3.b.h | b4.b.l | b4.b.h;
                                    if (b5.b.l | b5.b.h) {
                                        pw  = (uint16_t *)p;
                                        col = collookup[col] | arith;
                                        if (dat & 0x4000) {
                                            pw[0] &= ~(masklookuph[1][b5.b.h & 3] & wp[0]);
                                            pw[0] |= (bitlookuph[1][b3.b.l & 3][b3.b.h & 3] |
                                                      (bitlookuph[1][b4.b.l & 3][b4.b.h & 3] << 2) |
                                                      (col & masklookuph[1][b5.b.h & 3])) &
                                                     wp[0];
                                            pw[1] &= ~(masklookuph[1][(b5.b.h >> 2) & 3] & (wp[0] >> 16));
                                            pw[1] |= (bitlookuph[1][(b3.b.l >> 2) & 3][(b3.b.h >> 2) & 3] |
                                                      (bitlookuph[1][(b4.b.l >> 2) & 3][(b4.b.h >> 2) & 3] << 2) |
                                                      (col & masklookuph[1][(b5.b.h >> 2) & 3])) &
                                                     (wp[0] >> 16);
                                            pw[2] &= ~(masklookuph[1][(b5.b.h >> 4) & 3] & wp[1]);
                                            pw[2] |= (bitlookuph[1][(b3.b.l >> 4) & 3][(b3.b.h >> 4) & 3] |
                                                      (bitlookuph[1][(b4.b.l >> 4) & 3][(b4.b.h >> 4) & 3] << 2) |
                                                      (col & masklookuph[1][(b5.b.h >> 4) & 3])) &
                                                     wp[1];
                                            pw[3] &= ~(masklookuph[1][(b5.b.h >> 6) & 3] & (wp[1] >> 16));
                                            pw[3] |= (bitlookuph[1][(b3.b.l >> 6) & 3][(b3.b.h >> 6) & 3] |
                                                      (bitlookuph[1][(b4.b.l >> 6) & 3][(b4.b.h >> 6) & 3] << 2) |
                                                      (col & masklookuph[1][(b5.b.h >> 6) & 3])) &
                                                     (wp[1] >> 16);
                                            pw[4] &= ~(masklookuph[1][b5.b.l & 3] & wp[2]);
                                            pw[4] |= (bitlookuph[1][b1.b.l & 3][b1.b.h & 3] |
                                                      (bitlookuph[1][b2.b.l & 3][b2.b.h & 3] << 2) |
                                                      (col & masklookuph[1][b5.b.l & 3])) &
                                                     wp[2];
                                            pw[5] &= ~(masklookuph[1][(b5.b.l >> 2) & 3] & (wp[2] >> 16));
                                            pw[5] |= (bitlookuph[1][(b1.b.l >> 2) & 3][(b1.b.h >> 2) & 3] |
                                                      (bitlookuph[1][(b2.b.l >> 2) & 3][(b2.b.h >> 2) & 3] << 2) |
                                                      (col & masklookuph[1][(b5.b.l >> 2) & 3])) &
                                                     (wp[2] >> 16);
                                            pw[6] &= ~(masklookuph[1][(b5.b.l >> 4) & 3] & wp[3]);
                                            pw[6] |= (bitlookuph[1][(b1.b.l >> 4) & 3][(b1.b.h >> 4) & 3] |
                                                      (bitlookuph[1][(b2.b.l >> 4) & 3][(b2.b.h >> 4) & 3] << 2) |
                                                      (col & masklookuph[1][(b5.b.l >> 4) & 3])) &
                                                     wp[3];
                                            pw[7] &= ~(masklookuph[1][(b5.b.l >> 6) & 3] & (wp[3] >> 16));
                                            pw[7] |= (bitlookuph[1][(b1.b.l >> 6) & 3][(b1.b.h >> 6) & 3] |
                                                      (bitlookuph[1][(b2.b.l >> 6) & 3][(b2.b.h >> 6) & 3] << 2) |
                                                      (col & masklookuph[1][(b5.b.l >> 6) & 3])) &
                                                     (wp[3] >> 16);
                                        } else {
                                            pw[7] &= ~(masklookuph[0][b5.b.h & 3] & (wp[3] >> 16));
                                            pw[7] |= (bitlookuph[0][b3.b.l & 3][b3.b.h & 3] |
                                                      (bitlookuph[0][b4.b.l & 3][b4.b.h & 3] << 2) |
                                                      (col & masklookuph[0][b5.b.h & 3])) &
                                                     (wp[3] >> 16);
                                            pw[6] &= ~(masklookuph[0][(b5.b.h >> 2) & 3] & wp[3]);
                                            pw[6] |= (bitlookuph[0][(b3.b.l >> 2) & 3][(b3.b.h >> 2) & 3] |
                                                      (bitlookuph[0][(b4.b.l >> 2) & 3][(b4.b.h >> 2) & 3] << 2) |
                                                      (col & masklookuph[0][(b5.b.h >> 2) & 3])) &
                                                     wp[3];
                                            pw[5] &= ~(masklookuph[0][(b5.b.h >> 4) & 3] & (wp[2] >> 16));
                                            pw[5] |= (bitlookuph[0][(b3.b.l >> 4) & 3][(b3.b.h >> 4) & 3] |
                                                      (bitlookuph[0][(b4.b.l >> 4) & 3][(b4.b.h >> 4) & 3] << 2) |
                                                      (col & masklookuph[0][(b5.b.h >> 4) & 3])) &
                                                     (wp[2] >> 16);
                                            pw[4] &= ~(masklookuph[0][(b5.b.h >> 6) & 3] & wp[2]);
                                            pw[4] |= (bitlookuph[0][(b3.b.l >> 6) & 3][(b3.b.h >> 6) & 3] |
                                                      (bitlookuph[0][(b4.b.l >> 6) & 3][(b4.b.h >> 6) & 3] << 2) |
                                                      (col & masklookuph[0][(b5.b.h >> 6) & 3])) &
                                                     wp[2];
                                            pw[3] &= ~(masklookuph[0][b5.b.l & 3] & (wp[1] >> 16));
                                            pw[3] |= (bitlookuph[0][b1.b.l & 3][b1.b.h & 3] |
                                                      (bitlookuph[0][b2.b.l & 3][b2.b.h & 3] << 2) |
                                                      (col & masklookuph[0][b5.b.l & 3])) &
                                                     (wp[1] >> 16);
                                            pw[2] &= ~(masklookuph[0][(b5.b.l >> 2) & 3] & wp[1]);
                                            pw[2] |= (bitlookuph[0][(b1.b.l >> 2) & 3][(b1.b.h >> 2) & 3] |
                                                      (bitlookuph[0][(b2.b.l >> 2) & 3][(b2.b.h >> 2) & 3] << 2) |
                                                      (col & masklookuph[0][(b5.b.l >> 2) & 3])) &
                                                     wp[1];
                                            pw[1] &= ~(masklookuph[0][(b5.b.l >> 4) & 3] & (wp[0] >> 16));
                                            pw[1] |= (bitlookuph[0][(b1.b.l >> 4) & 3][(b1.b.h >> 4) & 3] |
                                                      (bitlookuph[0][(b2.b.l >> 4) & 3][(b2.b.h >> 4) & 3] << 2) |
                                                      (col & masklookuph[0][(b5.b.l >> 4) & 3])) &
                                                     (wp[0] >> 16);
                                            pw[0] &= ~(masklookuph[0][(b5.b.l >> 6) & 3] & wp[0]);
                                            pw[0] |= (bitlookuph[0][(b1.b.l >> 6) & 3][(b1.b.h >> 6) & 3] |
                                                      (bitlookuph[0][(b2.b.l >> 6) & 3][(b2.b.h >> 6) & 3] << 2) |
                                                      (col & masklookuph[0][(b5.b.l >> 6) & 3])) &
                                                     wp[0];
                                        }
                                    }
                                    break;
                                case 6:
                                    tile <<= 3;
                                    if (tilesize & (1 << c)) {
                                        if (dat & 0x8000)
                                            tile += (((l ^ 8) & 8) << 5);
                                        else
                                            tile += ((l & 8) << 5);
                                        if (dat & 0x4000)
                                            tile += (((x + xx + 1) & 1) << 4);
                                        else
                                            tile += (((x + xx) & 1) << 4);
                                    }
                                    if (dat & 0x8000)
                                        tile += ((l & 7) ^ 7) + chr[c];
                                    else
                                        tile += (l & 7) + chr[c];
                                    b1.w   = vram[tile & 0x7FFF];
                                    b3.w   = vram[(tile + 8) & 0x7FFF];
                                    b5.b.l = b1.b.l | b1.b.h;
                                    b5.b.h = b3.b.l | b3.b.h;
                                    if (b5.b.l | b5.b.h) {
                                        pw  = (uint16_t *)p;
                                        col = (collookup[col] >> 2) | arith;
                                        if (dat & 0x4000) {
                                            pw[0] &= ~(masklookuph[1][b5.b.h & 3] & wp[0]);
                                            pw[0] |= (bitlookuph[1][b3.b.l & 3][b3.b.h & 3] |
                                                      (col & masklookuph[1][b5.b.h & 3])) &
                                                     wp[0];
                                            pw[1] &= ~(masklookuph[1][(b5.b.h >> 2) & 3] & (wp[0] >> 16));
                                            pw[1] |= (bitlookuph[1][(b3.b.l >> 2) & 3][(b3.b.h >> 2) & 3] |
                                                      (col & masklookuph[1][(b5.b.h >> 2) & 3])) &
                                                     (wp[0] >> 16);
                                            pw[2] &= ~(masklookuph[1][(b5.b.h >> 4) & 3] & wp[1]);
                                            pw[2] |= (bitlookuph[1][(b3.b.l >> 4) & 3][(b3.b.h >> 4) & 3] |
                                                      (col & masklookuph[1][(b5.b.h >> 4) & 3])) &
                                                     wp[1];
                                            pw[3] &= ~(masklookuph[1][(b5.b.h >> 6) & 3] & (wp[1] >> 16));
                                            pw[3] |= (bitlookuph[1][(b3.b.l >> 6) & 3][(b3.b.h >> 6) & 3] |
                                                      (col & masklookuph[1][(b5.b.h >> 6) & 3])) &
                                                     (wp[1] >> 16);
                                            pw[4] &= ~(masklookuph[1][b5.b.l & 3] & wp[2]);
                                            pw[4] |= (bitlookuph[1][b1.b.l & 3][b1.b.h & 3] |
                                                      (col & masklookuph[1][b5.b.l & 3])) &
                                                     wp[2];
                                            pw[5] &= ~(masklookuph[1][(b5.b.l >> 2) & 3] & (wp[2] >> 16));
                                            pw[5] |= (bitlookuph[1][(b1.b.l >> 2) & 3][(b1.b.h >> 2) & 3] |
                                                      (col & masklookuph[1][(b5.b.l >> 2) & 3])) &
                                                     (wp[2] >> 16);
                                            pw[6] &= ~(masklookuph[1][(b5.b.l >> 4) & 3] & wp[3]);
                                            pw[6] |= (bitlookuph[1][(b1.b.l >> 4) & 3][(b1.b.h >> 4) & 3] |
                                                      (col & masklookuph[1][(b5.b.l >> 4) & 3])) &
                                                     wp[3];
                                            pw[7] &= ~(masklookuph[1][(b5.b.l >> 6) & 3] & (wp[3] >> 16));
                                            pw[7] |= (bitlookuph[1][(b1.b.l >> 6) & 3][(b1.b.h >> 6) & 3] |
                                                      (col & masklookuph[1][(b5.b.l >> 6) & 3])) &
                                                     (wp[3] >> 16);
                                        } else {
                                            pw[7] &= ~(masklookuph[0][b5.b.h & 3] & (wp[3] >> 16));
                                            pw[7] |= (bitlookuph[0][b3.b.l & 3][b3.b.h & 3] |
                                                      (col & masklookuph[0][b5.b.h & 3])) &
                                                     (wp[3] >> 16);
                                            pw[6] &= ~(masklookuph[0][(b5.b.h >> 2) & 3] & wp[3]);
                                            pw[6] |= (bitlookuph[0][(b3.b.l >> 2) & 3][(b3.b.h >> 2) & 3] |
                                                      (col & masklookuph[0][(b5.b.h >> 2) & 3])) &
                                                     wp[3];
                                            pw[5] &= ~(masklookuph[0][(b5.b.h >> 4) & 3] & (wp[2] >> 16));
                                            pw[5] |= (bitlookuph[0][(b3.b.l >> 4) & 3][(b3.b.h >> 4) & 3] |
                                                      (col & masklookuph[0][(b5.b.h >> 4) & 3])) &
                                                     (wp[2] >> 16);
                                            pw[4] &= ~(masklookuph[0][(b5.b.h >> 6) & 3] & wp[2]);
                                            pw[4] |= (bitlookuph[0][(b3.b.l >> 6) & 3][(b3.b.h >> 6) & 3] |
                                                      (col & masklookuph[0][(b5.b.h >> 6) & 3])) &
                                                     wp[2];
                                            pw[3] &= ~(masklookuph[0][b5.b.l & 3] & (wp[1] >> 16));
                                            pw[3] |= (bitlookuph[0][b1.b.l & 3][b1.b.h & 3] |
                                                      (col & masklookuph[0][b5.b.l & 3])) &
                                                     (wp[1] >> 16);
                                            pw[2] &= ~(masklookuph[0][(b5.b.l >> 2) & 3] & wp[1]);
                                            pw[2] |= (bitlookuph[0][(b1.b.l >> 2) & 3][(b1.b.h >> 2) & 3] |
                                                      (col & masklookuph[0][(b5.b.l >> 2) & 3])) &
                                                     wp[1];
                                            pw[1] &= ~(masklookuph[0][(b5.b.l >> 4) & 3] & (wp[0] >> 16));
                                            pw[1] |= (bitlookuph[0][(b1.b.l >> 4) & 3][(b1.b.h >> 4) & 3] |
                                                      (col & masklookuph[0][(b5.b.l >> 4) & 3])) &
                                                     (wp[0] >> 16);
                                            pw[0] &= ~(masklookuph[0][(b5.b.l >> 6) & 3] & wp[0]);
                                            pw[0] |= (bitlookuph[0][(b1.b.l >> 6) & 3][(b1.b.h >> 6) & 3] |
                                                      (col & masklookuph[0][(b5.b.l >> 6) & 3])) &
                                                     wp[0];
                                        }
                                    }
                                    break;
                                case 8:
                                    tile <<= 5;
                                    if (dat & 0x8000)
                                        tile += ((l & 7) ^ 7) + chr[c];
                                    else
                                        tile += (l & 7) + chr[c];
                                    b1.w = vram[tile];
                                    b2.w = vram[tile + 8];
                                    b3.w = vram[tile + 16];
                                    b4.w = vram[tile + 24];
                                    b5.w = b1.w | b2.w | b3.w | b4.w;
                                    b5.b.l |= b5.b.h;
                                    if (b5.w) {
                                        col = arith;
                                        if (dat & 0x4000) {
                                            p[0] &= ~(masklookup[1][b5.b.l & 3] & wp[0]);
                                            p[0] |= (bitlookup[1][b1.b.l & 3][b1.b.h & 3] |
                                                     (bitlookup[1][b2.b.l & 3][b2.b.h & 3] << 2) |
                                                     (bitlookup[1][b3.b.l & 3][b3.b.h & 3] << 4) |
                                                     (bitlookup[1][b4.b.l & 3][b4.b.h & 3] << 6) |
                                                     (col & masklookup[0][b3.b.l & 3])) &
                                                    wp[0];
                                            p[1] &= ~(masklookup[1][(b5.b.l >> 2) & 3] & wp[1]);
                                            p[1] |= (bitlookup[1][(b1.b.l >> 2) & 3][(b1.b.h >> 2) & 3] |
                                                     (bitlookup[1][(b2.b.l >> 2) & 3][(b2.b.h >> 2) & 3] << 2) |
                                                     (bitlookup[1][(b3.b.l >> 2) & 3][(b3.b.h >> 2) & 3] << 4) |
                                                     (bitlookup[1][(b4.b.l >> 2) & 3][(b4.b.h >> 2) & 3] << 6) |
                                                     (col & masklookup[0][(b3.b.l >> 2) & 3])) &
                                                    wp[1];
                                            p[2] &= ~(masklookup[1][(b5.b.l >> 4) & 3] & wp[2]);
                                            p[2] |= (bitlookup[1][(b1.b.l >> 4) & 3][(b1.b.h >> 4) & 3] |
                                                     (bitlookup[1][(b2.b.l >> 4) & 3][(b2.b.h >> 4) & 3] << 2) |
                                                     (bitlookup[1][(b3.b.l >> 4) & 3][(b3.b.h >> 4) & 3] << 4) |
                                                     (bitlookup[1][(b4.b.l >> 4) & 3][(b4.b.h >> 4) & 3] << 6) |
                                                     (col & masklookup[0][(b3.b.l >> 4) & 3])) &
                                                    wp[2];
                                            p[3] &= ~(masklookup[1][(b5.b.l >> 6) & 3] & wp[3]);
                                            p[3] |= (bitlookup[1][(b1.b.l >> 6) & 3][(b1.b.h >> 6) & 3] |
                                                     (bitlookup[1][(b2.b.l >> 6) & 3][(b2.b.h >> 6) & 3] << 2) |
                                                     (bitlookup[1][(b3.b.l >> 6) & 3][(b3.b.h >> 6) & 3] << 4) |
                                                     (bitlookup[1][(b4.b.l >> 6) & 3][(b4.b.h >> 6) & 3] << 6) |
                                                     (col & masklookup[0][(b3.b.l >> 6) & 3])) &
                                                    wp[3];
                                        } else {
                                            p[3] &= ~(masklookup[0][b5.b.l & 3] & wp[3]);
                                            p[3] |= (bitlookup[0][b1.b.l & 3][b1.b.h & 3] |
                                                     (bitlookup[0][b2.b.l & 3][b2.b.h & 3] << 2) |
                                                     (bitlookup[0][b3.b.l & 3][b3.b.h & 3] << 4) |
                                                     (bitlookup[0][b4.b.l & 3][b4.b.h & 3] << 6) |
                                                     (col & masklookup[0][b3.b.l & 3])) &
                                                    wp[3];
                                            p[2] &= ~(masklookup[0][(b5.b.l >> 2) & 3] & wp[2]);
                                            p[2] |= (bitlookup[0][(b1.b.l >> 2) & 3][(b1.b.h >> 2) & 3] |
                                                     (bitlookup[0][(b2.b.l >> 2) & 3][(b2.b.h >> 2) & 3] << 2) |
                                                     (bitlookup[0][(b3.b.l >> 2) & 3][(b3.b.h >> 2) & 3] << 4) |
                                                     (bitlookup[0][(b4.b.l >> 2) & 3][(b4.b.h >> 2) & 3] << 6) |
                                                     (col & masklookup[0][(b3.b.l >> 2) & 3])) &
                                                    wp[2];
                                            p[1] &= ~(masklookup[0][(b5.b.l >> 4) & 3] & wp[1]);
                                            p[1] |= (bitlookup[0][(b1.b.l >> 4) & 3][(b1.b.h >> 4) & 3] |
                                                     (bitlookup[0][(b2.b.l >> 4) & 3][(b2.b.h >> 4) & 3] << 2) |
                                                     (bitlookup[0][(b3.b.l >> 4) & 3][(b3.b.h >> 4) & 3] << 4) |
                                                     (bitlookup[0][(b4.b.l >> 4) & 3][(b4.b.h >> 4) & 3] << 6) |
                                                     (col & masklookup[0][(b3.b.l >> 4) & 3])) &
                                                    wp[1];
                                            p[0] &= ~(masklookup[0][(b5.b.l >> 6) & 3] & wp[0]);
                                            p[0] |= (bitlookup[0][(b1.b.l >> 6) & 3][(b1.b.h >> 6) & 3] |
                                                     (bitlookup[0][(b2.b.l >> 6) & 3][(b2.b.h >> 6) & 3] << 2) |
                                                     (bitlookup[0][(b3.b.l >> 6) & 3][(b3.b.h >> 6) & 3] << 4) |
                                                     (bitlookup[0][(b4.b.l >> 6) & 3][(b4.b.h >> 6) & 3] << 6) |
                                                     (col & masklookup[0][(b3.b.l >> 6) & 3])) &
                                                    wp[0];
                                        }
                                    }
                                    break;
                            }
                        skiptile:
                            p += 4;
                            wp += 4;
                        }
                    } else if (!pri) {
                        pw   = (uint16_t *)(((b->line[line])) + 128);
                        pw2  = (uint16_t *)((window[c]) + 32);
                        cx   = (((int)m7x << 19) >> 19);
                        cy   = (((int)m7y << 19) >> 19);
                        hoff = ((int)xscroll[0] << 19) >> 19;
                        voff = ((int)yscroll[0] << 19) >> 19;
                        ma   = ((int)m7a << 16) >> 16;
                        mb   = ((int)m7b << 16) >> 16;
                        mc   = ((int)m7c << 16) >> 16;
                        md   = ((int)m7d << 16) >> 16;
                        y    = line + (voff - cy);
                        bb   = (mb * y) + (cx << 8);
                        dd   = (md * y) + (cy << 8);
                        x    = hoff - cx;
                        aa   = (ma * x);
                        cc   = (mc * x);
                        for (x = 0; x < 256; x++) {
                            xx = ((aa + bb) >> 8);
                            yy = ((cc + dd) >> 8);
                            if (!(m7sel & 0x80)) {
                                xx &= 0x3ff;
                                yy &= 0x3ff;
                                temp = vramb[(((yy & ~7) << 5) | ((xx & ~7) >> 2))];
                                col  = vramb[((temp << 7) + ((yy & 7) << 4) + ((xx & 7) << 1) + 1)];
                            } else {
                                if ((xx | yy) & 0xFFFFFC00) {
                                    switch (m7sel >> 6) {
                                        case 2:
                                            col = 0;
                                            break;
                                        case 3:
                                            col = vramb[(((yy & 7) << 4) + ((xx & 7) << 1) + 1) & 0x7FFF];
                                            break;
                                    }
                                } else {
                                    temp = vramb[(((yy & ~7) << 5) | ((xx & ~7) >> 2))];
                                    col  = vramb[((temp << 7) + ((yy & 7) << 4) + ((xx & 7) << 1) + 1)];
                                }
                            }
                            if (col && *pw2)
                                *pw = (col | (uint16_t)arith);
                            aa += ma;
                            cc += mc;
                            pw++;
                            pw2++;
                        }
                    }
                }
            }
        }
        b = otherscr;
    }
    pw  = (uint16_t *)otherscr->line[line];
    pw2 = (uint16_t *)subscr->line[line];
    pw3 = (uint16_t *)mainscr->line[line];
    switch (cgwsel & 0x30) {
        case 0x00:
            pw4 = (uint16_t *)window[7];
            break;
        case 0x10:
            pw4 = (uint16_t *)window[6];
            break;
        case 0x20:
            pw4 = (uint16_t *)window[5];
            break;
        case 0x30:
            pw4 = (uint16_t *)window[8];
            break;
    }
    docolour(pw, pw2, pw3, pw4);
    if (line == 224)
        doblit();
    if (line < 225)
        snes->io->dohdma(line);
}
void PPU::writeppu(uint16_t addr, uint8_t val)
{
    int      r, g, b, c;
    uint16_t tempaddr;
    switch (addr & 0xFF) {
        case 0x00:
            screna = val;
            break;
        case 0x01:
            sprsizeidx = val >> 5;
            sprbase    = (val & 7) << 13;
            break;
        case 0x02:
            spraddr  = (spraddr & 0x200) | (val << 1);
            spraddrs = spraddr;
            break;
        case 0x03:
            if (val & 1)
                spraddr |= 0x200;
            else
                spraddr &= ~0x200;
            spraddrs    = spraddr;
            prirotation = val & 0x80;
            break;
        case 0x04:
            sprram[spraddr++] = val;
            if (spraddr >= 544)
                spraddr = 0;
            break;
        case 0x05:
            mode     = val & 15;
            tilesize = val >> 4;
            break;
        case 0x06:
            mosaic = val >> 4;
            break;
        case 0x07:
            bg[0]   = (val & 0xFC) << 8;
            size[0] = val & 3;
            break;
        case 0x08:
            bg[1]   = (val & 0xFC) << 8;
            size[1] = val & 3;
            break;
        case 0x09:
            bg[2]   = (val & 0xFC) << 8;
            size[2] = val & 3;
            break;
        case 0x0A:
            bg[3]   = (val & 0xFC) << 8;
            size[3] = val & 3;
            break;
        case 0x0B:
            chr[0] = (val & 0xF) << 12;
            chr[1] = (val & 0xF0) << 8;
            break;
        case 0x0C:
            chr[2] = (val & 0xF) << 12;
            chr[3] = (val & 0xF0) << 8;
            break;
        case 0x0D:
            xscroll[0] = (xscroll[0] >> 8) | (val << 8);
            break;
        case 0x0E:
            yscroll[0] = (yscroll[0] >> 8) | (val << 8);
            break;
        case 0x0F:
            xscroll[1] = (xscroll[1] >> 8) | (val << 8);
            break;
        case 0x10:
            yscroll[1] = (yscroll[1] >> 8) | (val << 8);
            break;
        case 0x11:
            xscroll[2] = (xscroll[2] >> 8) | (val << 8);
            break;
        case 0x12:
            yscroll[2] = (yscroll[2] >> 8) | (val << 8);
            break;
        case 0x13:
            xscroll[3] = (xscroll[3] >> 8) | (val << 8);
            break;
        case 0x14:
            yscroll[3] = (yscroll[3] >> 8) | (val << 8);
            break;
        case 0x15:
            portctrl = val;
            switch (val & 3) {
                case 0:
                    vinc = 1;
                    break;
                case 1:
                    vinc = 32;
                    break;
                case 2:
                case 3:
                    vinc = 128;
            }
            break;
        case 0x16:
            vramaddr  = (vramaddr & 0xFF00) | val;
            firstread = 1;
            wroteaddr = snes->cpu->pbr | snes->cpu->pc;
            break;
        case 0x17:
            vramaddr  = (vramaddr & 0xFF) | (val << 8);
            firstread = 1;
            wroteaddr = snes->cpu->pbr | snes->cpu->pc;
            break;
        case 0x18:
            firstread = 1;
            tempaddr  = vramaddr;
            switch (portctrl & 0xC) {
                case 0x4:
                    tempaddr = (tempaddr & 0xff00) | ((tempaddr & 0x001f) << 3) | ((tempaddr >> 5) & 7);
                    break;
                case 0x8:
                    tempaddr = (tempaddr & 0xfe00) | ((tempaddr & 0x003f) << 3) | ((tempaddr >> 6) & 7);
                    break;
                case 0xC:
                    tempaddr = (tempaddr & 0xfc00) | ((tempaddr & 0x007f) << 3) | ((tempaddr >> 7) & 7);
                    break;
            }
            vramb[(tempaddr << 1) & 0xFFFF] = val;
            if (!(portctrl & 0x80))
                vramaddr += vinc;
            break;
        case 0x19:
            firstread = 1;
            tempaddr  = vramaddr;
            switch (portctrl & 0xC) {
                case 0x4:
                    tempaddr = (tempaddr & 0xff00) | ((tempaddr & 0x001f) << 3) | ((tempaddr >> 5) & 7);
                    break;
                case 0x8:
                    tempaddr = (tempaddr & 0xfe00) | ((tempaddr & 0x003f) << 3) | ((tempaddr >> 6) & 7);
                    break;
                case 0xC:
                    tempaddr = (tempaddr & 0xfc00) | ((tempaddr & 0x007f) << 3) | ((tempaddr >> 7) & 7);
                    break;
            }
            vramb[((tempaddr << 1) & 0xFFFF) | 1] = val;
            if (portctrl & 0x80)
                vramaddr += vinc;
            break;
        case 0x1A:
            m7sel = val;
            return;
        case 0x1B:
            m7a     = (m7a >> 8) | (val << 8);
            matrixr = (int16_t)m7a * ((int16_t)m7b >> 8);
            return;
        case 0x1C:
            m7b     = (m7b >> 8) | (val << 8);
            matrixr = (int16_t)m7a * ((int16_t)m7b >> 8);
            return;
        case 0x1D:
            m7c = (m7c >> 8) | (val << 8);
            return;
        case 0x1E:
            m7d = (m7d >> 8) | (val << 8);
            return;
        case 0x1F:
            m7x = (m7x >> 8) | (val << 8);
            return;
        case 0x20:
            m7y = (m7y >> 8) | (val << 8);
            return;
        case 0x21:
            palindex  = val;
            palbuffer = 0;
            break;
        case 0x22:
            if (!palbuffer)
                palbuffer = val | 0x100;
            else {
                pal[palindex] = (val << 8) | (palbuffer & 0xFF);
                for (c = 0; c < 16; c++) {
                    r                      = (int)((float)(pal[palindex] & 31) * ((float)c / (float)15));
                    g                      = (int)((float)((pal[palindex] >> 5) & 31) * ((float)c / (float)15));
                    b                      = (int)((float)((pal[palindex] >> 10) & 31) * ((float)c / (float)15));
                    pallookup[c][palindex] = makecol(r << 3, g << 3, b << 3);
                }
                palindex++;
                palindex &= 255;
                palbuffer = 0;
            }
            break;
        case 0x23:
            if (val != windena1)
                windowschanged = 1;
            windena1 = val;
            break;
        case 0x24:
            if (val != windena2)
                windowschanged = 1;
            windena2 = val;
            break;
        case 0x25:
            if (val != windena3)
                windowschanged = 1;
            windena3 = val;
            break;
        case 0x26:
            if (val != w1left)
                windowschanged = 1;
            w1left = val;
            break;
        case 0x27:
            if (val != w1right)
                windowschanged = 1;
            w1right = val;
            break;
        case 0x28:
            if (val != w2left)
                windowschanged = 1;
            w2left = val;
            break;
        case 0x29:
            if (val != w2right)
                windowschanged = 1;
            w2right = val;
            break;
        case 0x2A:
            if (val != windlogic)
                windowschanged = 1;
            windlogic = val;
            break;
        case 0x2B:
            if (val != windlogic2)
                windowschanged = 1;
            windlogic2 = val;
            break;
        case 0x2C:
            main = val;
            break;
        case 0x2D:
            sub = val;
            break;
        case 0x2E:
            wmaskmain = val;
            break;
        case 0x2F:
            wmasksub = val;
            break;
        case 0x30:
            cgwsel = val;
            break;
        case 0x31:
            cgadsub = val;
            break;
        case 0x32:
            if (val & 0x20)
                fixedc.r = (val & 0x1F) << 3;
            if (val & 0x40)
                fixedc.g = (val & 0x1F) << 3;
            if (val & 0x80)
                fixedc.b = (val & 0x1F) << 3;
            fixedcol = (((fixedc.r >> 3) << _rgb_r_shift_16) | ((fixedc.g >> 2) << _rgb_g_shift_16) |
                        ((fixedc.b >> 3) << _rgb_b_shift_16));
            break;
        case 0x40:
        case 0x41:
        case 0x42:
        case 0x43:
            snes->spc->writetospc(addr, val);
            break;
            snes->cpu->setzf = 0;
            break;
        case 0x80:
            snes->mem->ram[wramaddr & 0x1FFFF] = val;
            wramaddr++;
            break;
        case 0x81:
            wramaddr = (wramaddr & 0xFFFF00) | val;
            break;
        case 0x82:
            wramaddr = (wramaddr & 0xFF00FF) | (val << 8);
            break;
        case 0x83:
            wramaddr = (wramaddr & 0x00FFFF) | (val << 16);
            break;
    }
}
uint8_t PPU::doskipper()
{
    int temp = spcskip;
    spcskip++;
    if (spcskip == 19)
        spcskip = 0;
    switch (temp >> 1) {
        case 0:
        case 1:
            snes->cpu->setzf = 2;
            return 0;
        case 2:
            if (temp & 1)
                return snes->cpu->a.b.h;
            else
                return snes->cpu->a.b.l;
            break;
        case 3:
            if (temp & 1)
                return snes->cpu->x.b.h;
            else
                return snes->cpu->x.b.l;
            break;
        case 4:
            if (temp & 1)
                return snes->cpu->y.b.h;
            else
                return snes->cpu->y.b.l;
            break;
        case 5:
            if (temp & 1)
                return 0xBB;
            else
                return 0xAA;
            break;
        case 6:
            snes->cpu->setzf = 2;
            return 0;
        case 7:
            if (temp & 1)
                return 0xBB;
            else
                return 0xAA;
            break;
        case 8:
            if (temp & 1)
                return 0x33;
            else
                return 0x33;
            break;
        case 9:
            return 0;
    }
    printf("Shouldn't have got here %i %i\n", temp, spcskip);
    exit(-1);
}
uint8_t PPU::readppu(uint16_t addr)
{
    uint8_t temp;
    switch (addr & 0xFF) {
        case 0x34:
            return matrixr;
        case 0x35:
            return matrixr >> 8;
        case 0x36:
            return matrixr >> 16;
        case 0x37:
            vcount = snes->lines;
            hcount = (1364 - snes->cpu->cycles) >> 2;
            break;
        case 0x38:
            return sprram[spraddr++];
        case 0x39:
            if (firstread)
                temp = vramb[(vramaddr << 1) & 0xFFFF];
            else
                temp = vramb[((vramaddr << 1) - 2) & 0xFFFF];
            if (!(portctrl & 0x80)) {
                vramaddr += vinc;
                firstread = 0;
            }
            return temp;
        case 0x3A:
            if (firstread)
                temp = vramb[((vramaddr << 1) & 0xFFFF) | 1];
            else
                temp = vramb[(((vramaddr << 1) - 2) & 0xFFFF) | 1];
            if ((portctrl & 0x80)) {
                vramaddr += vinc;
                firstread = 0;
            }
            return temp;
        case 0x3D:
            temp = vcount & 0xFF;
            vcount >>= 8;
            return temp;
        case 0x3E:
            return 1;
        case 0x3F:
            return 0x00;
        case 0x40:
        case 0x42:
            return snes->spc->readfromspc(addr);
            return doskipper();
            spcskip++;
            if (spcskip == 41)
                spcskip = 0;
            switch (spcskip >> 1) {
                case 0:
                    return snes->cpu->a.b.l;
                case 1:
                    return snes->cpu->x.b.l;
                case 2:
                    return snes->cpu->y.b.l;
                case 3:
                    return 0xFF;
                case 4:
                    return 0x00;
                case 5:
                    return 0x55;
                case 6:
                    return 0xAA;
                case 7:
                    return 1;
                case 8:
                    return 0xAA;
                case 9:
                    return 0xCD;
                case 10:
                    return 0xBB;
                case 11:
                    return 0xAA;
                case 12:
                    return 7;
                case 13:
                    return snes->cpu->a.b.l;
                case 14:
                    return 0xCC;
                case 15:
                    return 0;
                case 16:
                    return 0;
                case 17:
                    return 3;
                case 18:
                    return snes->cpu->a.b.l;
                case 19:
                    return snes->cpu->a.b.l;
                case 20:
                    return 2;
            }
            break;
        case 0x41:
        case 0x43:
            return snes->spc->readfromspc(addr);
            return doskipper();
            spcskip++;
            if (spcskip == 41)
                spcskip = 0;
            switch (spcskip >> 1) {
                case 0:
                    return snes->cpu->a.b.h;
                case 1:
                    return snes->cpu->x.b.h;
                case 2:
                    return snes->cpu->y.b.h;
                case 3:
                    return 0xFF;
                case 4:
                    return 2;
                case 5:
                    return 0x55;
                case 6:
                    return 0xAA;
                case 7:
                    return 1;
                case 8:
                    return 0xBB;
                case 9:
                    return 0xCD;
                case 10:
                    return 0xAA;
                case 11:
                    return 0xBB;
                case 12:
                    return 7;
                case 13:
                    return snes->cpu->a.b.l;
                case 14:
                    return 0xCC;
                case 15:
                    return 0;
                case 16:
                    return 0;
                case 17:
                    return 3;
                case 18:
                    return snes->cpu->a.b.h;
                case 19:
                    return snes->cpu->a.b.h;
                case 20:
                    return 0;
            }
            break;
        case 0x80:
            temp = snes->mem->ram[wramaddr & 0x1FFFF];
            wramaddr++;
            return temp;
        default:
            return 0;
            exit(-1);
    }
    return 0;
}
uint16_t PPU::getvramaddr()
{
    return vramaddr << 1;
}
void PPU::drawchar(int tile, int x, int y, int col)
{
    uint8_t  dat, dat1, dat2, dat3, dat4;
    uint16_t addr = tile << 5;
    int      yy, xx;
    for (yy = 0; yy < 8; yy++) {
        dat1 = vramb[addr];
        dat2 = vramb[addr + 1];
        dat3 = vramb[addr + 16];
        dat4 = vramb[addr + 17];
        addr += 2;
        for (xx = 7; xx > -1; xx--) {
            dat = (dat1 & 1);
            dat |= ((dat2 & 1) << 1);
            dat |= ((dat3 & 1) << 2);
            dat |= ((dat4 & 1) << 3);
            dat |= (col << 4);
            dasbuffer->line[y + yy][x + xx] = dat;
            dat1 >>= 1;
            dat2 >>= 1;
            dat3 >>= 1;
            dat4 >>= 1;
        }
    }
}
