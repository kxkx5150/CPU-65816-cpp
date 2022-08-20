#include <stdlib.h>
#include <string.h>
#include "../allegro.h"
#include "io.h"
#include "ppu.h"
#include "snem.h"
#include "spc.h"
#include "mem.h"

MEM::MEM(SNES *_snes)
{
    snes = _snes;
}
void MEM::allocmem()
{
    ram = (uint8_t *)malloc(128 * 1024);
    memset(ram, 0x55, 128 * 1024);
    sram = (uint8_t *)malloc(8192);
    memset(sram, 0, 8192);
}
void MEM::loadrom(char *fn)
{
    int      c = 0;
    char     name[22];
    char     sramname[512];
    FILE    *f = fopen(fn, "rb");
    int      len;
    uint16_t temp, temp2;

    snes->spc->spccycles = -10000;

    fseek(f, -1, SEEK_END);
    len = ftell(f) + 1;
    fseek(f, len & 512, SEEK_SET);

    rom = (uint8_t *)malloc(4096 * 1024);
    while (!feof(f) && c < 0x400000) {
        auto _ = fread(&rom[c], 65536, 1, f);
        c += 0x10000;
    }
    fclose(f);
    temp  = rom[0x7FDC] | (rom[0x7FDD] << 8);
    temp2 = rom[0x7FDE] | (rom[0x7FDF] << 8);

    if ((temp | temp2) == 0xFFFF)
        lorom = 1;
    else
        lorom = 0;

    initmem();
    if (((snes->readmem(0xFFFD) << 8) | snes->readmem(0xFFFC)) == 0xFFFF) {
        lorom ^= 1;
        initmem();
    }

    len = c;
    for (c = 0; c < 21; c++)
        name[c] = snes->readmem(0xFFC0 + c);

    name[21] = 0;
    srammask = (1 << (snes->readmem(0xFFD8) + 10)) - 1;
    if (!snes->readmem(0xFFD8))
        srammask = 0;

    if (srammask) {
        if ((srammask + 1) > 8192) {
            free(sram);
            sram = (uint8_t *)malloc(srammask + 1);
        }
        sramname[0] = 0;
        replace_extension(sramname, fn, "srm", 511);

        f = fopen(sramname, "rb");
        if (f) {
            auto _ = fread(sram, srammask + 1, 1, f);
            fclose(f);
        } else
            memset(sram, 0, srammask + 1);
    }
    memset(ram, 0x55, 128 * 1024);
}
void MEM::initmem()
{
    int c, d;
    for (c = 0; c < 256; c++) {
        for (d = 0; d < 8; d++) {
            memread[(c << 3) | d] = memwrite[(c << 3) | d] = 0;
        }
    }
    if (lorom) {
        for (c = 0; c < 96; c++) {
            for (d = 0; d < 4; d++) {
                memread[(c << 3) | (d + 4)]           = 1;
                memlookup[(c << 3) | (d + 4)]         = &rom[((d * 0x2000) + (c * 0x8000)) & 0x3FFFFF];
                memread[(c << 3) | (d + 4) | 0x400]   = 1;
                memlookup[(c << 3) | (d + 4) | 0x400] = &rom[((d * 0x2000) + (c * 0x8000)) & 0x3FFFFF];
            }
        }
        for (c = 0; c < 64; c++) {
            memread[(c << 3) | 0] = memwrite[(c << 3) | 0] = 1;
            memlookup[(c << 3) | 0]                        = ram;
        }
        for (c = 0; c < 64; c++) {
            memread[(c << 3) | 0x400] = memwrite[(c << 3) | 0x400] = 1;
            memlookup[(c << 3) | 0x400]                            = ram;
        }
        for (c = 0; c < 8; c++) {
            memread[(0x7E << 3) | c] = memwrite[(0x7E << 3) | c] = 1;
            memlookup[(0x7E << 3) | c]                           = &ram[c * 0x2000];
            memread[(0x7F << 3) | c] = memwrite[(0x7F << 3) | c] = 1;
            memlookup[(0x7F << 3) | c]                           = &ram[(c * 0x2000) + 0x10000];
        }
    } else {
        for (c = 0; c < 2048; c++) {
            memread[c]   = 1;
            memwrite[c]  = 0;
            memlookup[c] = &rom[(c * 0x2000) & 0x3FFFFF];
        }
        for (c = 0; c < 64; c++) {
            for (d = 1; d < 4; d++) {
                memread[(c << 3) + d] = memwrite[(c << 3) + d] = 0;
                memread[(c << 3) + d + 1024] = memwrite[(c << 3) + d + 1024] = 0;
            }
        }
        for (c = 0; c < 64; c++) {
            memread[(c << 3) | 0] = memwrite[(c << 3) | 0] = 1;
            memlookup[(c << 3) | 0]                        = ram;
            memread[(c << 3) | 1024] = memwrite[(c << 3) | 1024] = 1;
            memlookup[(c << 3) | 1024]                           = ram;
        }
        for (c = 0; c < 8; c++) {
            memread[(0x7E << 3) | c] = memwrite[(0x7E << 3) | c] = 1;
            memlookup[(0x7E << 3) | c]                           = &ram[c * 0x2000];
            memread[(0x7F << 3) | c] = memwrite[(0x7F << 3) | c] = 1;
            memlookup[(0x7F << 3) | c]                           = &ram[(c * 0x2000) + 0x10000];
        }
        for (c = 0; c < 16; c++) {
            memread[(0x70 << 3) + c] = memwrite[(0x70 << 3) + c] = 1;
            memlookup[(0x70 << 3) + c]                           = sram;
        }
    }
    for (c = 0; c < 64; c++) {
        accessspeed[(c << 3) | 0] = 8;
        accessspeed[(c << 3) | 1] = 6;
        accessspeed[(c << 3) | 2] = 6;
        accessspeed[(c << 3) | 3] = 6;
        accessspeed[(c << 3) | 4] = accessspeed[(c << 3) | 5] = 8;
        accessspeed[(c << 3) | 6] = accessspeed[(c << 3) | 7] = 8;
    }
    for (c = 64; c < 128; c++) {
        for (d = 0; d < 8; d++) {
            accessspeed[(c << 3) | d] = 8;
        }
    }
    for (c = 128; c < 192; c++) {
        accessspeed[(c << 3) | 0] = 8;
        accessspeed[(c << 3) | 1] = 6;
        accessspeed[(c << 3) | 2] = 6;
        accessspeed[(c << 3) | 3] = 6;
        accessspeed[(c << 3) | 4] = accessspeed[(c << 3) | 5] = 8;
        accessspeed[(c << 3) | 6] = accessspeed[(c << 3) | 7] = 8;
    }
    for (c = 192; c < 256; c++) {
        for (d = 0; d < 8; d++) {
            accessspeed[(c << 3) | d] = 8;
        }
    }
}
uint8_t MEM::readmeml(uint32_t addr)
{
    addr &= ~0xFF000000;
    if (((addr >> 16) & 0x7F) < 0x40) {
        switch (addr & 0xF000) {
            case 0x2000:
                return snes->ppu->readppu(addr);
            case 0x4000:
                if ((addr & 0xE00) == 0x200)
                    return snes->io->readio(addr);
                if ((addr & 0xFFFE) == 0x4016)
                    return snes->io->readjoyold(addr);
                return 0;
            case 0x6000:
            case 0x7000:
                if (!lorom)
                    return sram[addr & srammask];
            default:
                return 0xFF;
        }
    }
    if ((addr >> 16) >= 0xD0 && (addr >> 16) <= 0xFE)
        return 0;
    if ((addr >> 16) == 0x70) {
        if (srammask) {
            return sram[addr & srammask];
        }
        return 0;
    }
    if ((addr >> 16) == 0x60)
        return 0;
    return 0;
}
void MEM::writememl(uint32_t addr, uint8_t val)
{
    addr &= ~0xFF000000;
    if (((addr >> 16) & 0x7F) < 0x40) {
        switch (addr & 0xF000) {
            case 0x2000:
                if ((addr & 0xF00) == 0x100)
                    snes->ppu->writeppu(addr, val);
                return;
            case 0x3000:
                return;
            case 0x4000:
                if ((addr & 0xE00) == 0x200)
                    snes->io->writeio(addr, val);
                if ((addr & 0xFFFE) == 0x4016)
                    snes->io->writejoyold(addr, val);
                return;
            case 0x5000:
                return;
            case 0x6000:
            case 0x7000:
                if (!lorom)
                    sram[addr & srammask] = val;
                return;
            case 0x8000:
            case 0x9000:
            case 0xA000:
            case 0xB000:
            case 0xC000:
            case 0xD000:
            case 0xE000:
            case 0xF000:
                return;
            default:
                printf("Bad write %06X %02X\n", addr, val);
                exit(-1);
        }
    }
    if ((addr >> 16) >= 0xD0 && (addr >> 16) <= 0xFE)
        return;
    if ((addr >> 16) == 0x70) {
        sram[addr & srammask] = val;
        return;
    }
    if ((addr >> 16) == 0x60)
        return;
    if ((addr >= 0xC00000 && addr < 0xFE0000))
        return;
    if ((addr >= 0x710000 && addr < 0x7E0000))
        return;
    printf("Bad write %06X %02X\n", addr, val);
    exit(-1);
}
