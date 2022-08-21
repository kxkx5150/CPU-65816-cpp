
#ifndef CART_H
#define CART_H
#include <stdint.h>

typedef struct CartHeader
{
    char name[22]     = {};
    char makerCode[3] = {};
    char gameCode[5]  = {};

    uint8_t  headerVersion      = 0;
    uint8_t  speed              = 0;
    uint8_t  type               = 0;
    uint8_t  coprocessor        = 0;
    uint8_t  chips              = 0;
    uint32_t romSize            = 0;
    uint32_t ramSize            = 0;
    uint8_t  region             = 0;
    uint8_t  maker              = 0;
    uint8_t  version            = 0;
    uint16_t checksumComplement = 0;
    uint16_t checksum           = 0;

    int16_t score    = 0;
    uint8_t cartType = 0;

    uint32_t flashSize      = 0;
    uint32_t exRamSize      = 0;
    uint8_t  specialVersion = 0;
    uint8_t  exCoprocessor  = 0;

    bool pal = false;
} CartHeader;


class Snes;
class Cart {
  public:
    Snes *snes = nullptr;

    CartHeader headers[4];
    uint8_t    type = 0;

    uint8_t *rom = nullptr;
    uint8_t *ram = nullptr;

    uint32_t romSize = 0;
    uint32_t ramSize = 0;

  public:
    Cart(Snes *_snes);

    bool loadRom(const char *name, Snes *snes);
    bool snes_loadRom(Snes *snes, const uint8_t *data, int length);
    void cart_load(int _type, uint8_t *_rom, int _romSize, int _ramSize);

    void readHeader(const uint8_t *data, int length, int location, CartHeader *header);

    void cart_reset();

    uint8_t cart_read(uint8_t bank, uint16_t adr);
    void    cart_write(uint8_t bank, uint16_t adr, uint8_t val);
    uint8_t cart_readLorom(uint8_t bank, uint16_t adr);
    void    cart_writeLorom(uint8_t bank, uint16_t adr, uint8_t val);
    uint8_t cart_readHirom(uint8_t bank, uint16_t adr);
    void    cart_writeHirom(uint8_t bank, uint16_t adr, uint8_t val);
};


#endif
