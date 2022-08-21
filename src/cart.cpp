
#include <cstddef>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "cart.h"
#include "snes.h"


Cart::Cart(Snes *_snes)
{
    snes = _snes;
}
bool Cart::snes_loadRom(Snes *snes, const uint8_t *data, int length)
{
    for (int i = 0; i < 4; i++) {
        headers[i].score = -50;
    }
    if (length >= 0x8000)
        readHeader(data, length, 0x7fc0, &headers[0]);
    if (length >= 0x8200)
        readHeader(data, length, 0x81c0, &headers[1]);
    if (length >= 0x10000)
        readHeader(data, length, 0xffc0, &headers[2]);
    if (length >= 0x10200)
        readHeader(data, length, 0x101c0, &headers[3]);

    int max  = 0;
    int used = 0;
    for (int i = 0; i < 4; i++) {
        if (headers[i].score > max) {
            max  = headers[i].score;
            used = i;
        }
    }

    if (used & 1) {
        data += 0x200;
        length -= 0x200;
    }

    if (headers[used].cartType > 2) {
        return false;
    }

    int newLength = 0x8000;
    while (true) {
        if (length <= newLength) {
            break;
        }
        newLength *= 2;
    }

    uint8_t *newData = (uint8_t *)malloc(newLength);
    memcpy(newData, data, length);
    int test = 1;

    while (length != newLength) {
        if (length & test) {
            memcpy(newData + length, newData + length - test, test);
            length += test;
        }
        test *= 2;
    }

    cart_load(headers[used].cartType, newData, newLength, headers[used].chips > 0 ? headers[used].ramSize : 0);
    free(newData);
    return true;
}
void Cart::readHeader(const uint8_t *data, int length, int location, CartHeader *header)
{
    for (int i = 0; i < 21; i++) {
        uint8_t ch = data[location + i];
        if (ch >= 0x20 && ch < 0x7f) {
            header->name[i] = ch;
        } else {
            header->name[i] = '.';
        }
    }

    header->name[21]           = 0;
    header->speed              = data[location + 0x15] >> 4;
    header->type               = data[location + 0x15] & 0xf;
    header->coprocessor        = data[location + 0x16] >> 4;
    header->chips              = data[location + 0x16] & 0xf;
    header->romSize            = 0x400 << data[location + 0x17];
    header->ramSize            = 0x400 << data[location + 0x18];
    header->region             = data[location + 0x19];
    header->maker              = data[location + 0x1a];
    header->version            = data[location + 0x1b];
    header->checksumComplement = (data[location + 0x1d] << 8) + data[location + 0x1c];
    header->checksum           = (data[location + 0x1f] << 8) + data[location + 0x1e];
    header->headerVersion      = 1;

    if (header->maker == 0x33) {
        header->headerVersion = 3;

        for (int i = 0; i < 2; i++) {
            uint8_t ch = data[location - 0x10 + i];
            if (ch >= 0x20 && ch < 0x7f) {
                header->makerCode[i] = ch;
            } else {
                header->makerCode[i] = '.';
            }
        }

        header->makerCode[2] = 0;
        for (int i = 0; i < 4; i++) {
            uint8_t ch = data[location - 0xe + i];
            if (ch >= 0x20 && ch < 0x7f) {
                header->gameCode[i] = ch;
            } else {
                header->gameCode[i] = '.';
            }
        }
        header->gameCode[4]    = 0;
        header->flashSize      = 0x400 << data[location - 4];
        header->exRamSize      = 0x400 << data[location - 3];
        header->specialVersion = data[location - 2];
        header->exCoprocessor  = data[location - 1];
    } else if (data[location + 0x14] == 0) {
        header->headerVersion = 2;
        header->exCoprocessor = data[location - 1];
    }

    header->pal      = (header->region >= 0x2 && header->region <= 0xc) || header->region == 0x11;
    header->cartType = location < 0x9000 ? 1 : 2;

    int score = 0;
    score += (header->speed == 2 || header->speed == 3) ? 5 : -4;
    score += (header->type <= 3 || header->type == 5) ? 5 : -2;
    score += (header->coprocessor <= 5 || header->coprocessor >= 0xe) ? 5 : -2;
    score += (header->chips <= 6 || header->chips == 9 || header->chips == 0xa) ? 5 : -2;
    score += (header->region <= 0x14) ? 5 : -2;
    score += (header->checksum + header->checksumComplement == 0xffff) ? 8 : -6;

    uint16_t resetVector = data[location + 0x3c] | (data[location + 0x3d] << 8);
    score += (resetVector >= 0x8000) ? 8 : -20;

    int     opcodeLoc = location + 0x40 - 0x8000 + (resetVector & 0x7fff);
    uint8_t opcode    = 0xff;

    if (opcodeLoc < length) {
        opcode = data[opcodeLoc];
    } else {
        score -= 14;
    }
    if (opcode == 0x78 || opcode == 0x18) {
        score += 6;
    }
    if (opcode == 0x4c || opcode == 0x5c || opcode == 0x9c) {
        score += 3;
    }
    if (opcode == 0x00 || opcode == 0xff || opcode == 0xdb) {
        score -= 6;
    }
    header->score = score;
}
void Cart::cart_reset()
{
    if (ramSize > 0 && ram != nullptr)
        memset(ram, 0, ramSize);
}
void Cart::cart_load(int _type, uint8_t *_rom, int _romSize, int _ramSize)
{
    type    = _type;
    rom     = (uint8_t *)malloc(_romSize);
    romSize = _romSize;
    if (_ramSize > 0) {
        ram = (uint8_t *)malloc(_ramSize);
        memset(ram, 0, _ramSize);
    } else {
        ram = nullptr;
    }
    ramSize = _ramSize;
    memcpy(rom, _rom, _romSize);
}
uint8_t Cart::cart_read(uint8_t bank, uint16_t adr)
{
    switch (type) {
        case 0:
            return snes->openBus;
        case 1:
            return cart_readLorom(bank, adr);
        case 2:
            return cart_readHirom(bank, adr);
    }
    return snes->openBus;
}
void Cart::cart_write(uint8_t bank, uint16_t adr, uint8_t val)
{
    switch (type) {
        case 0:
            break;
        case 1:
            cart_writeLorom(bank, adr, val);
            break;
        case 2:
            cart_writeHirom(bank, adr, val);
            break;
    }
}
uint8_t Cart::cart_readLorom(uint8_t bank, uint16_t adr)
{
    if (((bank >= 0x70 && bank < 0x7e) || bank >= 0xf0) && adr < 0x8000 && ramSize > 0) {
        return ram[(((bank & 0xf) << 15) | adr) & (ramSize - 1)];
    }

    bank &= 0x7f;

    if (adr >= 0x8000 || bank >= 0x40) {
        return rom[((bank << 15) | (adr & 0x7fff)) & (romSize - 1)];
    }

    return snes->openBus;
}
void Cart::cart_writeLorom(uint8_t bank, uint16_t adr, uint8_t val)
{
    if (((bank >= 0x70 && bank < 0x7e) || bank > 0xf0) && adr < 0x8000 && ramSize > 0) {
        ram[(((bank & 0xf) << 15) | adr) & (ramSize - 1)] = val;
    }
}
uint8_t Cart::cart_readHirom(uint8_t bank, uint16_t adr)
{
    bank &= 0x7f;
    if (bank < 0x40 && adr >= 0x6000 && adr < 0x8000 && ramSize > 0) {
        return ram[(((bank & 0x3f) << 13) | (adr & 0x1fff)) & (ramSize - 1)];
    }

    if (adr >= 0x8000 || bank >= 0x40) {
        return rom[(((bank & 0x3f) << 16) | adr) & (romSize - 1)];
    }

    return snes->openBus;
}
void Cart::cart_writeHirom(uint8_t bank, uint16_t adr, uint8_t val)
{
    bank &= 0x7f;
    if (bank < 0x40 && adr >= 0x6000 && adr < 0x8000 && ramSize > 0) {
        ram[(((bank & 0x3f) << 13) | (adr & 0x1fff)) & (ramSize - 1)] = val;
    }
}
bool Cart::loadRom(const char *name, Snes *snes)
{
    size_t length = 0;
    FILE  *f      = fopen(name, "rb");
    fseek(f, 0, SEEK_END);
    length = ftell(f);
    rewind(f);
    uint8_t *file = (uint8_t *)malloc(length);
    auto     _    = fread(file, length, 1, f);
    fclose(f);
    bool result = snes->cart->snes_loadRom(snes, file, length);
    snes->snes_reset(true);
    free(file);
    return true;
}
