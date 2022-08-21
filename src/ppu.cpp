
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "ppu.h"
#include "snes.h"

Ppu::Ppu(Snes *_snes)
{
    snes = _snes;
}
void Ppu::ppu_reset()
{
    memset(vram, 0, sizeof(vram));
    vramPointer         = 0;
    vramIncrementOnHigh = false;
    vramIncrement       = 1;
    vramRemapMode       = 0;
    vramReadBuffer      = 0;

    memset(cgram, 0, sizeof(cgram));
    cgramPointer     = 0;
    cgramSecondWrite = false;
    cgramBuffer      = 0;

    memset(oam, 0, sizeof(oam));
    memset(highOam, 0, sizeof(highOam));
    oamAdr           = 0;
    oamAdrWritten    = 0;
    oamInHigh        = false;
    oamInHighWritten = false;
    oamSecondWrite   = false;
    oamBuffer        = 0;

    objPriority = false;
    objTileAdr1 = 0;
    objTileAdr2 = 0;
    objSize     = 0;
    memset(objPixelBuffer, 0, sizeof(objPixelBuffer));

    memset(objPriorityBuffer, 0, sizeof(objPriorityBuffer));
    timeOver     = false;
    rangeOver    = false;
    objInterlace = false;

    for (int i = 0; i < 4; i++) {
        bgLayer[i].hScroll       = 0;
        bgLayer[i].vScroll       = 0;
        bgLayer[i].tilemapWider  = false;
        bgLayer[i].tilemapHigher = false;
        bgLayer[i].tilemapAdr    = 0;
        bgLayer[i].tileAdr       = 0;
        bgLayer[i].bigTiles      = false;
        bgLayer[i].mosaicEnabled = false;
    }

    scrollPrev      = 0;
    scrollPrev2     = 0;
    mosaicSize      = 1;
    mosaicStartLine = 1;

    for (int i = 0; i < 5; i++) {
        layer[i].mainScreenEnabled  = false;
        layer[i].subScreenEnabled   = false;
        layer[i].mainScreenWindowed = false;
        layer[i].subScreenWindowed  = false;
    }
    memset(m7matrix, 0, sizeof(m7matrix));

    m7prev       = 0;
    m7largeField = false;
    m7charFill   = false;
    m7xFlip      = false;
    m7yFlip      = false;
    m7extBg      = false;
    m7startX     = 0;
    m7startY     = 0;

    for (int i = 0; i < 6; i++) {
        windowLayer[i].window1enabled  = false;
        windowLayer[i].window2enabled  = false;
        windowLayer[i].window1inversed = false;
        windowLayer[i].window2inversed = false;
        windowLayer[i].maskLogic       = 0;
    }

    window1left     = 0;
    window1right    = 0;
    window2left     = 0;
    window2right    = 0;
    clipMode        = 0;
    preventMathMode = 0;
    addSubscreen    = false;
    subtractColor   = false;
    halfColor       = false;

    memset(mathEnabled, 0, sizeof(mathEnabled));

    fixedColorR     = 0;
    fixedColorG     = 0;
    fixedColorB     = 0;
    forcedBlank     = true;
    brightness      = 0;
    mode            = 0;
    bg3priority     = false;
    evenFrame       = false;
    pseudoHires     = false;
    overscan        = false;
    frameOverscan   = false;
    interlace       = false;
    frameInterlace  = false;
    directColor     = false;
    hCount          = 0;
    vCount          = 0;
    hCountSecond    = false;
    vCountSecond    = false;
    countersLatched = false;
    ppu1openBus     = 0;
    ppu2openBus     = 0;

    memset(pixelBuffer, 0, sizeof(pixelBuffer));
}
bool Ppu::ppu_checkOverscan()
{
    frameOverscan = overscan;
    return frameOverscan;
}
void Ppu::ppu_handleVblank()
{
    if (!forcedBlank) {
        oamAdr         = oamAdrWritten;
        oamInHigh      = oamInHighWritten;
        oamSecondWrite = false;
    }

    frameInterlace = interlace;
}
void Ppu::ppu_runLine(int line)
{
    if (line == 0) {
        mosaicStartLine = 1;
        rangeOver       = false;
        timeOver        = false;
        evenFrame       = !evenFrame;
    } else {
        memset(objPixelBuffer, 0, sizeof(objPixelBuffer));

        if (!forcedBlank)
            ppu_evaluateSprites(line - 1);
        if (mode == 7)
            ppu_calculateMode7Starts(line);
        for (int x = 0; x < 256; x++) {
            ppu_handlePixel(x, line);
        }
    }
}
void Ppu::ppu_handlePixel(int x, int y)
{
    int r = 0, r2 = 0;
    int g = 0, g2 = 0;
    int b = 0, b2 = 0;

    if (!forcedBlank) {
        int  mainLayer        = ppu_getPixel(x, y, false, &r, &g, &b);
        bool colorWindowState = ppu_getWindowState(5, x);
        if (clipMode == 3 || (clipMode == 2 && colorWindowState) || (clipMode == 1 && !colorWindowState)) {
            r = 0;
            g = 0;
            b = 0;
        }

        int  secondLayer    = 5;
        bool mathEnabledflg = mainLayer < 6 && mathEnabled[mainLayer] &&
                              !(preventMathMode == 3 || (preventMathMode == 2 && colorWindowState) ||
                                (preventMathMode == 1 && !colorWindowState));

        if ((mathEnabledflg && addSubscreen) || pseudoHires || mode == 5 || mode == 6) {
            secondLayer = ppu_getPixel(x, y, true, &r2, &g2, &b2);
        }

        if (mathEnabledflg) {
            if (subtractColor) {
                r -= (addSubscreen && secondLayer != 5) ? r2 : fixedColorR;
                g -= (addSubscreen && secondLayer != 5) ? g2 : fixedColorG;
                b -= (addSubscreen && secondLayer != 5) ? b2 : fixedColorB;
            } else {
                r += (addSubscreen && secondLayer != 5) ? r2 : fixedColorR;
                g += (addSubscreen && secondLayer != 5) ? g2 : fixedColorG;
                b += (addSubscreen && secondLayer != 5) ? b2 : fixedColorB;
            }

            if (halfColor && (secondLayer != 5 || !addSubscreen)) {
                r >>= 1;
                g >>= 1;
                b >>= 1;
            }

            if (r > 31)
                r = 31;
            if (g > 31)
                g = 31;
            if (b > 31)
                b = 31;
            if (r < 0)
                r = 0;
            if (g < 0)
                g = 0;
            if (b < 0)
                b = 0;
        }

        if (!(pseudoHires || mode == 5 || mode == 6)) {
            r2 = r;
            g2 = g;
            b2 = b;
        }
    }

    int row = (y - 1) + (evenFrame ? 0 : 239);

    pixelBuffer[row * 2048 + x * 8 + 1] = ((b2 << 3) | (b2 >> 2)) * brightness / 15;
    pixelBuffer[row * 2048 + x * 8 + 2] = ((g2 << 3) | (g2 >> 2)) * brightness / 15;
    pixelBuffer[row * 2048 + x * 8 + 3] = ((r2 << 3) | (r2 >> 2)) * brightness / 15;
    pixelBuffer[row * 2048 + x * 8 + 5] = ((b << 3) | (b >> 2)) * brightness / 15;
    pixelBuffer[row * 2048 + x * 8 + 6] = ((g << 3) | (g >> 2)) * brightness / 15;
    pixelBuffer[row * 2048 + x * 8 + 7] = ((r << 3) | (r >> 2)) * brightness / 15;
}
int Ppu::ppu_getPixel(int x, int y, bool sub, int *r, int *g, int *b)
{
    int actMode  = mode == 1 && bg3priority ? 8 : mode;
    actMode      = mode == 7 && m7extBg ? 9 : actMode;
    int layeridx = 5;
    int pixel    = 0;

    for (int i = 0; i < layerCountPerMode[actMode]; i++) {
        int  curLayer    = layersPerMode[actMode][i];
        int  curPriority = prioritysPerMode[actMode][i];
        bool layerActive = false;

        if (!sub) {
            layerActive = layer[curLayer].mainScreenEnabled &&
                          (!layer[curLayer].mainScreenWindowed || !ppu_getWindowState(curLayer, x));
        } else {
            layerActive = layer[curLayer].subScreenEnabled &&
                          (!layer[curLayer].subScreenWindowed || !ppu_getWindowState(curLayer, x));
        }

        if (layerActive) {
            if (curLayer < 4) {
                int lx = x;
                int ly = y;
                if (bgLayer[curLayer].mosaicEnabled && mosaicSize > 1) {
                    lx -= lx % mosaicSize;
                    ly -= (ly - mosaicStartLine) % mosaicSize;
                }

                if (mode == 7) {
                    pixel = ppu_getPixelForMode7(lx, curLayer, curPriority);
                } else {
                    lx += bgLayer[curLayer].hScroll;
                    if (mode == 5 || mode == 6) {
                        lx *= 2;
                        lx += (sub || bgLayer[curLayer].mosaicEnabled) ? 0 : 1;
                        if (interlace) {
                            ly *= 2;
                            ly += (evenFrame || bgLayer[curLayer].mosaicEnabled) ? 0 : 1;
                        }
                    }
                    ly += bgLayer[curLayer].vScroll;
                    if (mode == 2 || mode == 4 || mode == 6) {
                        ppu_handleOPT(curLayer, &lx, &ly);
                    }
                    pixel = ppu_getPixelForBgLayer(lx & 0x3ff, ly & 0x3ff, curLayer, curPriority);
                }
            } else {
                pixel = 0;
                if (objPriorityBuffer[x] == curPriority)
                    pixel = objPixelBuffer[x];
            }
        }
        if (pixel > 0) {
            layeridx = curLayer;
            break;
        }
    }
    if (directColor && layeridx < 4 && bitDepthsPerMode[actMode][layeridx] == 8) {
        *r = ((pixel & 0x7) << 2) | ((pixel & 0x100) >> 7);
        *g = ((pixel & 0x38) >> 1) | ((pixel & 0x200) >> 8);
        *b = ((pixel & 0xc0) >> 3) | ((pixel & 0x400) >> 8);
    } else {
        uint16_t color = cgram[pixel & 0xff];
        *r             = color & 0x1f;
        *g             = (color >> 5) & 0x1f;
        *b             = (color >> 10) & 0x1f;
    }

    if (layeridx == 4 && pixel < 0xc0)
        layeridx = 6;

    return layeridx;
}
void Ppu::ppu_handleOPT(int layer, int *lx, int *ly)
{
    int x      = *lx;
    int y      = *ly;
    int column = 0;

    if (mode == 6) {
        column = ((x - (x & 0xf)) - ((bgLayer[layer].hScroll * 2) & 0xfff0)) >> 4;
    } else {
        column = ((x - (x & 0x7)) - (bgLayer[layer].hScroll & 0xfff8)) >> 3;
    }

    if (column > 0) {
        int      valid   = layer == 0 ? 0x2000 : 0x4000;
        uint16_t hOffset = ppu_getOffsetValue(column - 1, 0);
        uint16_t vOffset = 0;
        if (mode == 4) {
            if (hOffset & 0x8000) {
                vOffset = hOffset;
                hOffset = 0;
            }
        } else {
            vOffset = ppu_getOffsetValue(column - 1, 1);
        }

        if (mode == 6) {
            if (hOffset & valid)
                *lx = (((hOffset & 0x3f8) + (column * 8)) * 2) | (x & 0xf);
        } else {
            if (hOffset & valid)
                *lx = ((hOffset & 0x3f8) + (column * 8)) | (x & 0x7);
        }

        if (vOffset & valid)
            *ly = (vOffset & 0x3ff) + (y - bgLayer[layer].vScroll);
    }
}
uint16_t Ppu::ppu_getOffsetValue(int col, int row)
{
    int      x           = col * 8 + bgLayer[2].hScroll;
    int      y           = row * 8 + bgLayer[2].vScroll;
    int      tileBits    = bgLayer[2].bigTiles ? 4 : 3;
    int      tileHighBit = bgLayer[2].bigTiles ? 0x200 : 0x100;
    uint16_t tilemapAdr  = bgLayer[2].tilemapAdr + (((y >> tileBits) & 0x1f) << 5 | ((x >> tileBits) & 0x1f));

    if ((x & tileHighBit) && bgLayer[2].tilemapWider)
        tilemapAdr += 0x400;
    if ((y & tileHighBit) && bgLayer[2].tilemapHigher)
        tilemapAdr += bgLayer[2].tilemapWider ? 0x800 : 0x400;

    return vram[tilemapAdr & 0x7fff];
}
int Ppu::ppu_getPixelForBgLayer(int x, int y, int layer, bool priority)
{
    bool     wideTiles    = bgLayer[layer].bigTiles || mode == 5 || mode == 6;
    int      tileBitsX    = wideTiles ? 4 : 3;
    int      tileHighBitX = wideTiles ? 0x200 : 0x100;
    int      tileBitsY    = bgLayer[layer].bigTiles ? 4 : 3;
    int      tileHighBitY = bgLayer[layer].bigTiles ? 0x200 : 0x100;
    uint16_t tilemapAdr   = bgLayer[layer].tilemapAdr + (((y >> tileBitsY) & 0x1f) << 5 | ((x >> tileBitsX) & 0x1f));

    if ((x & tileHighBitX) && bgLayer[layer].tilemapWider)
        tilemapAdr += 0x400;

    if ((y & tileHighBitY) && bgLayer[layer].tilemapHigher)
        tilemapAdr += bgLayer[layer].tilemapWider ? 0x800 : 0x400;

    uint16_t tile = vram[tilemapAdr & 0x7fff];

    if (((bool)(tile & 0x2000)) != priority)
        return 0;

    int paletteNum = (tile & 0x1c00) >> 10;
    int row        = (tile & 0x8000) ? 7 - (y & 0x7) : (y & 0x7);
    int col        = (tile & 0x4000) ? (x & 0x7) : 7 - (x & 0x7);
    int tileNum    = tile & 0x3ff;

    if (wideTiles) {
        if (((bool)(x & 8)) ^ ((bool)(tile & 0x4000)))
            tileNum += 1;
    }

    if (bgLayer[layer].bigTiles) {
        if (((bool)(y & 8)) ^ ((bool)(tile & 0x8000)))
            tileNum += 0x10;
    }

    int bitDepth = bitDepthsPerMode[mode][layer];
    if (mode == 0)
        paletteNum += 8 * layer;

    int      paletteSize = 4;
    uint16_t plane1      = vram[(bgLayer[layer].tileAdr + ((tileNum & 0x3ff) * 4 * bitDepth) + row) & 0x7fff];
    int      pixel       = (plane1 >> col) & 1;
    pixel |= ((plane1 >> (8 + col)) & 1) << 1;

    if (bitDepth > 2) {
        paletteSize     = 16;
        uint16_t plane2 = vram[(bgLayer[layer].tileAdr + ((tileNum & 0x3ff) * 4 * bitDepth) + 8 + row) & 0x7fff];
        pixel |= ((plane2 >> col) & 1) << 2;
        pixel |= ((plane2 >> (8 + col)) & 1) << 3;
    }

    if (bitDepth > 4) {
        paletteSize     = 256;
        uint16_t plane3 = vram[(bgLayer[layer].tileAdr + ((tileNum & 0x3ff) * 4 * bitDepth) + 16 + row) & 0x7fff];
        pixel |= ((plane3 >> col) & 1) << 4;
        pixel |= ((plane3 >> (8 + col)) & 1) << 5;
        uint16_t plane4 = vram[(bgLayer[layer].tileAdr + ((tileNum & 0x3ff) * 4 * bitDepth) + 24 + row) & 0x7fff];
        pixel |= ((plane4 >> col) & 1) << 6;
        pixel |= ((plane4 >> (8 + col)) & 1) << 7;
    }

    return pixel == 0 ? 0 : paletteSize * paletteNum + pixel;
}
void Ppu::ppu_calculateMode7Starts(int y)
{
    int hScroll = ((int16_t)(m7matrix[6] << 3)) >> 3;
    int vScroll = ((int16_t)(m7matrix[7] << 3)) >> 3;
    int xCenter = ((int16_t)(m7matrix[4] << 3)) >> 3;
    int yCenter = ((int16_t)(m7matrix[5] << 3)) >> 3;

    int clippedH = hScroll - xCenter;
    int clippedV = vScroll - yCenter;

    clippedH = (clippedH & 0x2000) ? (clippedH | ~1023) : (clippedH & 1023);
    clippedV = (clippedV & 0x2000) ? (clippedV | ~1023) : (clippedV & 1023);

    if (bgLayer[0].mosaicEnabled && mosaicSize > 1) {
        y -= (y - mosaicStartLine) % mosaicSize;
    }

    uint8_t ry = m7yFlip ? 255 - y : y;
    m7startX   = (((m7matrix[0] * clippedH) & ~63) + ((m7matrix[1] * ry) & ~63) + ((m7matrix[1] * clippedV) & ~63) +
                (xCenter << 8));
    m7startY   = (((m7matrix[2] * clippedH) & ~63) + ((m7matrix[3] * ry) & ~63) + ((m7matrix[3] * clippedV) & ~63) +
                (yCenter << 8));
}
int Ppu::ppu_getPixelForMode7(int x, int layer, bool priority)
{
    uint8_t rx         = m7xFlip ? 255 - x : x;
    int     xPos       = (m7startX + m7matrix[0] * rx) >> 8;
    int     yPos       = (m7startY + m7matrix[2] * rx) >> 8;
    bool    outsideMap = xPos < 0 || xPos >= 1024 || yPos < 0 || yPos >= 1024;

    xPos &= 0x3ff;
    yPos &= 0x3ff;

    if (!m7largeField)
        outsideMap = false;

    uint8_t tile  = outsideMap ? 0 : vram[(yPos >> 3) * 128 + (xPos >> 3)] & 0xff;
    uint8_t pixel = outsideMap && !m7charFill ? 0 : vram[tile * 64 + (yPos & 7) * 8 + (xPos & 7)] >> 8;

    if (layer == 1) {
        if (((bool)(pixel & 0x80)) != priority)
            return 0;
        return pixel & 0x7f;
    }

    return pixel;
}
bool Ppu::ppu_getWindowState(int layer, int x)
{
    if (!windowLayer[layer].window1enabled && !windowLayer[layer].window2enabled) {
        return false;
    }

    if (windowLayer[layer].window1enabled && !windowLayer[layer].window2enabled) {
        bool test = x >= window1left && x <= window1right;
        return windowLayer[layer].window1inversed ? !test : test;
    }

    if (!windowLayer[layer].window1enabled && windowLayer[layer].window2enabled) {
        bool test = x >= window2left && x <= window2right;
        return windowLayer[layer].window2inversed ? !test : test;
    }

    bool test1 = x >= window1left && x <= window1right;
    bool test2 = x >= window2left && x <= window2right;

    if (windowLayer[layer].window1inversed)
        test1 = !test1;

    if (windowLayer[layer].window2inversed)
        test2 = !test2;

    switch (windowLayer[layer].maskLogic) {
        case 0:
            return test1 || test2;
        case 1:
            return test1 && test2;
        case 2:
            return test1 != test2;
        case 3:
            return test1 == test2;
    }

    return false;
}
void Ppu::ppu_evaluateSprites(int line)
{
    uint8_t index        = objPriority ? (oamAdr & 0xfe) : 0;
    int     spritesFound = 0;
    int     tilesFound   = 0;

    for (int i = 0; i < 128; i++) {
        uint8_t y            = oam[index] >> 8;
        uint8_t row          = line - y;
        int     spriteSize   = spriteSizes[objSize][(highOam[index >> 3] >> ((index & 7) + 1)) & 1];
        int     spriteHeight = objInterlace ? spriteSize / 2 : spriteSize;

        if (row < spriteHeight) {
            int x = oam[index] & 0xff;
            x |= ((highOam[index >> 3] >> (index & 7)) & 1) << 8;

            if (x > 255)
                x -= 512;

            if (x > -spriteSize) {
                spritesFound++;

                if (spritesFound > 32) {
                    rangeOver = true;
                    break;
                }

                if (objInterlace)
                    row = row * 2 + (evenFrame ? 0 : 1);

                int  tile     = oam[index + 1] & 0xff;
                int  palette  = (oam[index + 1] & 0xe00) >> 9;
                bool hFlipped = oam[index + 1] & 0x4000;

                if (oam[index + 1] & 0x8000)
                    row = spriteSize - 1 - row;

                for (int col = 0; col < spriteSize; col += 8) {
                    if (col + x > -8 && col + x < 256) {
                        tilesFound++;
                        if (tilesFound > 34) {
                            timeOver = true;
                            break;
                        }
                        int      usedCol  = hFlipped ? spriteSize - 1 - col : col;
                        uint8_t  usedTile = (((tile >> 4) + (row / 8)) << 4) | (((tile & 0xf) + (usedCol / 8)) & 0xf);
                        uint16_t objAdr   = (oam[index + 1] & 0x100) ? objTileAdr2 : objTileAdr1;
                        uint16_t plane1   = vram[(objAdr + usedTile * 16 + (row & 0x7)) & 0x7fff];
                        uint16_t plane2   = vram[(objAdr + usedTile * 16 + 8 + (row & 0x7)) & 0x7fff];

                        for (int px = 0; px < 8; px++) {
                            int shift = hFlipped ? px : 7 - px;
                            int pixel = (plane1 >> shift) & 1;
                            pixel |= ((plane1 >> (8 + shift)) & 1) << 1;
                            pixel |= ((plane2 >> shift) & 1) << 2;
                            pixel |= ((plane2 >> (8 + shift)) & 1) << 3;
                            int screenCol = col + x + px;

                            if (pixel > 0 && screenCol >= 0 && screenCol < 256 && objPixelBuffer[screenCol] == 0) {
                                objPixelBuffer[screenCol]    = 0x80 + 16 * palette + pixel;
                                objPriorityBuffer[screenCol] = (oam[index + 1] & 0x3000) >> 12;
                            }
                        }
                    }
                }
                if (tilesFound > 34)
                    break;
            }
        }
        index += 2;
    }
}
uint16_t Ppu::ppu_getVramRemap()
{
    uint16_t adr = vramPointer;
    switch (vramRemapMode) {
        case 0:
            return adr;
        case 1:
            return (adr & 0xff00) | ((adr & 0xe0) >> 5) | ((adr & 0x1f) << 3);
        case 2:
            return (adr & 0xfe00) | ((adr & 0x1c0) >> 6) | ((adr & 0x3f) << 3);
        case 3:
            return (adr & 0xfc00) | ((adr & 0x380) >> 7) | ((adr & 0x7f) << 3);
    }
    return adr;
}
uint8_t Ppu::ppu_read(uint8_t adr)
{
    switch (adr) {
        case 0x04:
        case 0x14:
        case 0x24:
        case 0x05:
        case 0x15:
        case 0x25:
        case 0x06:
        case 0x16:
        case 0x26:
        case 0x08:
        case 0x18:
        case 0x28:
        case 0x09:
        case 0x19:
        case 0x29:
        case 0x0a:
        case 0x1a:
        case 0x2a: {
            return ppu1openBus;
        }
        case 0x34:
        case 0x35:
        case 0x36: {
            int result  = m7matrix[0] * (m7matrix[1] >> 8);
            ppu1openBus = (result >> (8 * (adr - 0x34))) & 0xff;
            return ppu1openBus;
        }
        case 0x37: {
            hCount          = snes->hPos / 4;
            vCount          = snes->vPos;
            countersLatched = true;
            return snes->openBus;
        }
        case 0x38: {
            uint8_t ret = 0;
            if (oamInHigh) {
                ret = highOam[((oamAdr & 0xf) << 1) | oamSecondWrite];
                if (oamSecondWrite) {
                    oamAdr++;
                    if (oamAdr == 0)
                        oamInHigh = false;
                }
            } else {
                if (!oamSecondWrite) {
                    ret = oam[oamAdr] & 0xff;
                } else {
                    ret = oam[oamAdr++] >> 8;
                    if (oamAdr == 0)
                        oamInHigh = true;
                }
            }

            oamSecondWrite = !oamSecondWrite;
            ppu1openBus    = ret;
            return ret;
        }
        case 0x39: {
            uint16_t val = vramReadBuffer;
            if (!vramIncrementOnHigh) {
                vramReadBuffer = vram[ppu_getVramRemap() & 0x7fff];
                vramPointer += vramIncrement;
            }

            ppu1openBus = val & 0xff;
            return val & 0xff;
        }
        case 0x3a: {
            uint16_t val = vramReadBuffer;
            if (vramIncrementOnHigh) {
                vramReadBuffer = vram[ppu_getVramRemap() & 0x7fff];
                vramPointer += vramIncrement;
            }

            ppu1openBus = val >> 8;
            return val >> 8;
        }
        case 0x3b: {
            uint8_t ret = 0;
            if (!cgramSecondWrite) {
                ret = cgram[cgramPointer] & 0xff;
            } else {
                ret = ((cgram[cgramPointer++] >> 8) & 0x7f) | (ppu2openBus & 0x80);
            }

            cgramSecondWrite = !cgramSecondWrite;
            ppu2openBus      = ret;
            return ret;
        }
        case 0x3c: {
            uint8_t val = 0;
            if (hCountSecond) {
                val = ((hCount >> 8) & 1) | (ppu2openBus & 0xfe);
            } else {
                val = hCount & 0xff;
            }

            hCountSecond = !hCountSecond;
            ppu2openBus  = val;
            return val;
        }
        case 0x3d: {
            uint8_t val = 0;
            if (vCountSecond) {
                val = ((vCount >> 8) & 1) | (ppu2openBus & 0xfe);
            } else {
                val = vCount & 0xff;
            }

            vCountSecond = !vCountSecond;
            ppu2openBus  = val;
            return val;
        }
        case 0x3e: {
            uint8_t val = 0x1;
            val |= ppu1openBus & 0x10;
            val |= rangeOver << 6;
            val |= timeOver << 7;
            ppu1openBus = val;
            return val;
        }
        case 0x3f: {
            uint8_t val = 0x3;
            val |= ppu2openBus & 0x20;
            val |= countersLatched << 6;
            val |= evenFrame << 7;
            countersLatched = false;
            hCountSecond    = false;
            vCountSecond    = false;
            ppu2openBus     = val;
            return val;
        }
        default: {
            return snes->openBus;
        }
    }
}
void Ppu::ppu_write(uint8_t adr, uint8_t val)
{
    switch (adr) {
        case 0x00: {
            brightness  = val & 0xf;
            forcedBlank = val & 0x80;
            break;
        }
        case 0x01: {
            objSize     = val >> 5;
            objTileAdr1 = (val & 7) << 13;
            objTileAdr2 = objTileAdr1 + (((val & 0x18) + 8) << 9);
            break;
        }
        case 0x02: {
            oamAdr         = val;
            oamAdrWritten  = oamAdr;
            oamInHigh      = oamInHighWritten;
            oamSecondWrite = false;
            break;
        }
        case 0x03: {
            objPriority      = val & 0x80;
            oamInHigh        = val & 1;
            oamInHighWritten = oamInHigh;
            oamAdr           = oamAdrWritten;
            oamSecondWrite   = false;
            break;
        }
        case 0x04: {
            if (oamInHigh) {
                highOam[((oamAdr & 0xf) << 1) | oamSecondWrite] = val;
                if (oamSecondWrite) {
                    oamAdr++;
                    if (oamAdr == 0)
                        oamInHigh = false;
                }
            } else {
                if (!oamSecondWrite) {
                    oamBuffer = val;
                } else {
                    oam[oamAdr++] = (val << 8) | oamBuffer;
                    if (oamAdr == 0)
                        oamInHigh = true;
                }
            }
            oamSecondWrite = !oamSecondWrite;
            break;
        }
        case 0x05: {
            mode                = val & 0x7;
            bg3priority         = val & 0x8;
            bgLayer[0].bigTiles = val & 0x10;
            bgLayer[1].bigTiles = val & 0x20;
            bgLayer[2].bigTiles = val & 0x40;
            bgLayer[3].bigTiles = val & 0x80;
            break;
        }
        case 0x06: {
            bgLayer[0].mosaicEnabled = val & 0x1;
            bgLayer[1].mosaicEnabled = val & 0x2;
            bgLayer[2].mosaicEnabled = val & 0x4;
            bgLayer[3].mosaicEnabled = val & 0x8;
            mosaicSize               = (val >> 4) + 1;
            mosaicStartLine          = snes->vPos;
            break;
        }
        case 0x07:
        case 0x08:
        case 0x09:
        case 0x0a: {
            bgLayer[adr - 7].tilemapWider  = val & 0x1;
            bgLayer[adr - 7].tilemapHigher = val & 0x2;
            bgLayer[adr - 7].tilemapAdr    = (val & 0xfc) << 8;
            break;
        }
        case 0x0b: {
            bgLayer[0].tileAdr = (val & 0xf) << 12;
            bgLayer[1].tileAdr = (val & 0xf0) << 8;
            break;
        }
        case 0x0c: {
            bgLayer[2].tileAdr = (val & 0xf) << 12;
            bgLayer[3].tileAdr = (val & 0xf0) << 8;
            break;
        }
        case 0x0d: {
            m7matrix[6] = ((val << 8) | m7prev) & 0x1fff;
            m7prev      = val;
        }
        case 0x0f:
        case 0x11:
        case 0x13: {
            bgLayer[(adr - 0xd) / 2].hScroll = ((val << 8) | (scrollPrev & 0xf8) | (scrollPrev2 & 0x7)) & 0x3ff;
            scrollPrev                       = val;
            scrollPrev2                      = val;
            break;
        }
        case 0x0e: {
            m7matrix[7] = ((val << 8) | m7prev) & 0x1fff;
            m7prev      = val;
        }
        case 0x10:
        case 0x12:
        case 0x14: {
            bgLayer[(adr - 0xe) / 2].vScroll = ((val << 8) | scrollPrev) & 0x3ff;
            scrollPrev                       = val;
            break;
        }
        case 0x15: {
            if ((val & 3) == 0) {
                vramIncrement = 1;
            } else if ((val & 3) == 1) {
                vramIncrement = 32;
            } else {
                vramIncrement = 128;
            }
            vramRemapMode       = (val & 0xc) >> 2;
            vramIncrementOnHigh = val & 0x80;
            break;
        }
        case 0x16: {
            vramPointer    = (vramPointer & 0xff00) | val;
            vramReadBuffer = vram[ppu_getVramRemap() & 0x7fff];
            break;
        }
        case 0x17: {
            vramPointer    = (vramPointer & 0x00ff) | (val << 8);
            vramReadBuffer = vram[ppu_getVramRemap() & 0x7fff];
            break;
        }
        case 0x18: {
            uint16_t vramAdr       = ppu_getVramRemap();
            vram[vramAdr & 0x7fff] = (vram[vramAdr & 0x7fff] & 0xff00) | val;
            if (!vramIncrementOnHigh)
                vramPointer += vramIncrement;
            break;
        }
        case 0x19: {
            uint16_t vramAdr       = ppu_getVramRemap();
            vram[vramAdr & 0x7fff] = (vram[vramAdr & 0x7fff] & 0x00ff) | (val << 8);
            if (vramIncrementOnHigh)
                vramPointer += vramIncrement;
            break;
        }
        case 0x1a: {
            m7largeField = val & 0x80;
            m7charFill   = val & 0x40;
            m7yFlip      = val & 0x2;
            m7xFlip      = val & 0x1;
            break;
        }
        case 0x1b:
        case 0x1c:
        case 0x1d:
        case 0x1e: {
            m7matrix[adr - 0x1b] = (val << 8) | m7prev;
            m7prev               = val;
            break;
        }
        case 0x1f:
        case 0x20: {
            m7matrix[adr - 0x1b] = ((val << 8) | m7prev) & 0x1fff;
            m7prev               = val;
            break;
        }
        case 0x21: {
            cgramPointer     = val;
            cgramSecondWrite = false;
            break;
        }
        case 0x22: {
            if (!cgramSecondWrite) {
                cgramBuffer = val;
            } else {
                cgram[cgramPointer++] = (val << 8) | cgramBuffer;
            }
            cgramSecondWrite = !cgramSecondWrite;
            break;
        }
        case 0x23:
        case 0x24:
        case 0x25: {
            windowLayer[(adr - 0x23) * 2].window1inversed     = val & 0x1;
            windowLayer[(adr - 0x23) * 2].window1enabled      = val & 0x2;
            windowLayer[(adr - 0x23) * 2].window2inversed     = val & 0x4;
            windowLayer[(adr - 0x23) * 2].window2enabled      = val & 0x8;
            windowLayer[(adr - 0x23) * 2 + 1].window1inversed = val & 0x10;
            windowLayer[(adr - 0x23) * 2 + 1].window1enabled  = val & 0x20;
            windowLayer[(adr - 0x23) * 2 + 1].window2inversed = val & 0x40;
            windowLayer[(adr - 0x23) * 2 + 1].window2enabled  = val & 0x80;
            break;
        }
        case 0x26: {
            window1left = val;
            break;
        }
        case 0x27: {
            window1right = val;
            break;
        }
        case 0x28: {
            window2left = val;
            break;
        }
        case 0x29: {
            window2right = val;
            break;
        }
        case 0x2a: {
            windowLayer[0].maskLogic = val & 0x3;
            windowLayer[1].maskLogic = (val >> 2) & 0x3;
            windowLayer[2].maskLogic = (val >> 4) & 0x3;
            windowLayer[3].maskLogic = (val >> 6) & 0x3;
            break;
        }
        case 0x2b: {
            windowLayer[4].maskLogic = val & 0x3;
            windowLayer[5].maskLogic = (val >> 2) & 0x3;
            break;
        }
        case 0x2c: {
            layer[0].mainScreenEnabled = val & 0x1;
            layer[1].mainScreenEnabled = val & 0x2;
            layer[2].mainScreenEnabled = val & 0x4;
            layer[3].mainScreenEnabled = val & 0x8;
            layer[4].mainScreenEnabled = val & 0x10;
            break;
        }
        case 0x2d: {
            layer[0].subScreenEnabled = val & 0x1;
            layer[1].subScreenEnabled = val & 0x2;
            layer[2].subScreenEnabled = val & 0x4;
            layer[3].subScreenEnabled = val & 0x8;
            layer[4].subScreenEnabled = val & 0x10;
            break;
        }
        case 0x2e: {
            layer[0].mainScreenWindowed = val & 0x1;
            layer[1].mainScreenWindowed = val & 0x2;
            layer[2].mainScreenWindowed = val & 0x4;
            layer[3].mainScreenWindowed = val & 0x8;
            layer[4].mainScreenWindowed = val & 0x10;
            break;
        }
        case 0x2f: {
            layer[0].subScreenWindowed = val & 0x1;
            layer[1].subScreenWindowed = val & 0x2;
            layer[2].subScreenWindowed = val & 0x4;
            layer[3].subScreenWindowed = val & 0x8;
            layer[4].subScreenWindowed = val & 0x10;
            break;
        }
        case 0x30: {
            directColor     = val & 0x1;
            addSubscreen    = val & 0x2;
            preventMathMode = (val & 0x30) >> 4;
            clipMode        = (val & 0xc0) >> 6;
            break;
        }
        case 0x31: {
            subtractColor = val & 0x80;
            halfColor     = val & 0x40;
            for (int i = 0; i < 6; i++) {
                mathEnabled[i] = val & (1 << i);
            }
            break;
        }
        case 0x32: {
            if (val & 0x80)
                fixedColorB = val & 0x1f;
            if (val & 0x40)
                fixedColorG = val & 0x1f;
            if (val & 0x20)
                fixedColorR = val & 0x1f;
            break;
        }
        case 0x33: {
            interlace    = val & 0x1;
            objInterlace = val & 0x2;
            overscan     = val & 0x4;
            pseudoHires  = val & 0x8;
            m7extBg      = val & 0x40;
            break;
        }
        default: {
            break;
        }
    }
}
void Ppu::ppu_putPixels(uint8_t *pixels)
{
    for (int y = 0; y < (frameOverscan ? 239 : 224); y++) {
        int dest = y * 2 + (frameOverscan ? 2 : 16);
        int y1 = y, y2 = y + 239;
        if (!frameInterlace) {
            y1 = y + (evenFrame ? 0 : 239);
            y2 = y1;
        }
        memcpy(pixels + (dest * 2048), &pixelBuffer[y1 * 2048], 2048);
        memcpy(pixels + ((dest + 1) * 2048), &pixelBuffer[y2 * 2048], 2048);
    }
    memset(pixels, 0, 2048 * 2);
    if (!frameOverscan) {
        memset(pixels + (2 * 2048), 0, 2048 * 14);
        memset(pixels + (464 * 2048), 0, 2048 * 16);
    }
}
