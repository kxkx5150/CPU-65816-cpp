#ifndef _MEMMAP_H_
#define _MEMMAP_H_
#include "snes.h"
#include "cpu.h"
#include "utils/port.h"
#include "rom_chip/msu1.h"

#define ROM_NAME_LEN      23
#define MEMMAP_BLOCK_SIZE (0x1000)
#define MEMMAP_NUM_BLOCKS (0x1000000 / MEMMAP_BLOCK_SIZE)
#define MEMMAP_SHIFT      (12)
#define MEMMAP_MASK       (MEMMAP_BLOCK_SIZE - 1)
#define addCyclesInMemoryAccess                                                                                        \
    if (!snes->scpu->InDMAorHDMA) {                                                                                    \
        snes->scpu->Cycles += speed;                                                                                   \
        while (snes->scpu->Cycles >= snes->scpu->NextEvent)                                                            \
            snes->scpu->S9xDoHEventProcessing();                                                                       \
    }
#define addCyclesInMemoryAccess_x2                                                                                     \
    if (!snes->scpu->InDMAorHDMA) {                                                                                    \
        snes->scpu->Cycles += speed << 1;                                                                              \
        while (snes->scpu->Cycles >= snes->scpu->NextEvent)                                                            \
            snes->scpu->S9xDoHEventProcessing();                                                                       \
    }
//
//


struct SOpcodes
{
    void (*S9xOpcode)(void);
};
enum s9xwriteorder_t
{
    WRITE_01,
    WRITE_10
};
struct SMulti
{
    int    cartType;
    int32  cartSizeA, cartSizeB;
    int32  sramSizeA, sramSizeB;
    uint32 sramMaskA, sramMaskB;
    uint32 cartOffsetA, cartOffsetB;
    uint8 *sramA, *sramB;
    char   fileNameA[PATH_MAX + 1], fileNameB[PATH_MAX + 1];
};



class CMemory {
    char LastRomFilename[PATH_MAX + 1] = "";

  public:
    enum
    {
        MAX_ROM_SIZE = 0x800000
    };
    enum file_formats
    {
        FILE_ZIP,
        FILE_JMA,
        FILE_DEFAULT
    };
    enum
    {
        NOPE,
        YEAH,
        BIGFIRST,
        SMALLFIRST
    };
    enum
    {
        MAP_TYPE_I_O,
        MAP_TYPE_ROM,
        MAP_TYPE_RAM
    };
    enum
    {
        MAP_CPU,
        MAP_PPU,
        MAP_LOROM_SRAM,
        MAP_LOROM_SRAM_B,
        MAP_HIROM_SRAM,
        MAP_DSP,
        MAP_SA1RAM,
        MAP_BWRAM,
        MAP_BWRAM_BITMAP,
        MAP_BWRAM_BITMAP2,
        MAP_SPC7110_ROM,
        MAP_SPC7110_DRAM,
        MAP_RONLY_SRAM,
        MAP_C4,
        MAP_OBC_RAM,
        MAP_SETA_DSP,
        MAP_SETA_RISC,
        MAP_BSX,
        MAP_NONE,
        MAP_LAST
    };


  public:
    uint8  NSRTHeader[32];
    uint8 *Map[MEMMAP_NUM_BLOCKS];
    uint8 *WriteMap[MEMMAP_NUM_BLOCKS];
    uint8  BlockIsRAM[MEMMAP_NUM_BLOCKS];
    uint8  BlockIsROM[MEMMAP_NUM_BLOCKS];

    char          ROMFilename[PATH_MAX + 1];
    char          ROMName[ROM_NAME_LEN];
    char          RawROMName[ROM_NAME_LEN];
    char          ROMId[5];
    unsigned char ROMSHA256[32];

  public:
    SNESX *snes = nullptr;

    int32  HeaderCount;
    uint8 *RAM;
    uint8 *ROM;
    uint8 *SRAM;
    uint8 *VRAM;
    uint8 *FillRAM;
    uint8 *BWRAM;
    uint8 *C4RAM;
    uint8 *OBC1RAM;
    uint8 *BSRAM;
    uint8 *BIOSROM;

    uint8  ExtendedFormat;
    int32  CompanyId;
    uint8  ROMRegion;
    uint8  ROMSpeed;
    uint8  ROMType;
    uint8  ROMSize;
    uint32 ROMChecksum;
    uint32 ROMComplementChecksum;
    uint32 ROMCRC32;

    int32  ROMFramesPerSecond;
    bool8  HiROM;
    bool8  LoROM;
    uint8  SRAMSize;
    uint32 SRAMMask;
    uint32 CalculatedSize;
    uint32 CalculatedChecksum;

    void (*PostRomInitFunc)(void);

  public:
    CMemory(SNESX *_snes);

    bool8 Init(void);
    void  Deinit(void);
    int   ScoreHiROM(bool8, int32 romoff = 0);
    int   ScoreLoROM(bool8, int32 romoff = 0);
    int   First512BytesCountZeroes() const;

    uint32 HeaderRemove(uint32, uint8 *);
    uint32 FileLoader(uint8 *, const char *, uint32);
    uint32 MemLoader(uint8 *, const char *, uint32);
    bool8  LoadROMMem(const uint8 *, uint32);
    bool8  LoadROM(const char *);
    bool8  LoadROMInt(int32);

    bool8 LoadSRAM(const char *);
    bool8 SaveSRAM(const char *);
    void  ClearSRAM(bool8 onlyNonSavedSRAM = 0);
    bool8 SaveSRTC(void);

    char  *Safe(const char *);
    char  *SafeANK(const char *);
    void   ParseSNESHeader(uint8 *);
    void   InitROM(void);
    uint32 map_mirror(uint32, uint32);
    void   map_lorom(uint32, uint32, uint32, uint32, uint32);
    void   map_hirom(uint32, uint32, uint32, uint32, uint32);
    void   map_lorom_offset(uint32, uint32, uint32, uint32, uint32, uint32);
    void   map_hirom_offset(uint32, uint32, uint32, uint32, uint32, uint32);
    void   map_space(uint32, uint32, uint32, uint32, uint8 *);
    void   map_index(uint32, uint32, uint32, uint32, int, int);
    void   map_System(void);
    void   map_WRAM(void);
    void   map_LoROMSRAM(void);
    void   map_HiROMSRAM(void);

    void map_SetaRISC(void);
    void map_SetaDSP(void);

    void map_WriteProtectROM(void);
    void Map_Initialize(void);
    void Map_LoROMMap(void);
    void Map_NoMAD1LoROMMap(void);
    void Map_JumboLoROMMap(void);
    void Map_ROM24MBSLoROMMap(void);
    void Map_SRAM512KLoROMMap(void);
    void Map_SufamiTurboLoROMMap(void);
    void Map_SufamiTurboPseudoLoROMMap(void);
    void Map_SuperFXLoROMMap(void);
    void Map_SetaDSPLoROMMap(void);
    void Map_SDD1LoROMMap(void);

    void   Map_HiROMMap(void);
    void   Map_ExtendedHiROMMap(void);
    void   Map_BSCartLoROMMap(uint8);
    void   Map_BSCartHiROMMap(void);
    uint16 checksum_calc_sum(uint8 *, uint32);
    uint16 checksum_mirror_sum(uint8 *, uint32 &, uint32 mask = 0x800000);
    void   Checksum_Calculate(void);
    bool8  match_na(const char *);
    bool8  match_nn(const char *);
    bool8  match_nc(const char *);
    bool8  match_id(const char *);

    void        MakeRomInfoText(char *);
    const char *MapType(void);

    void S9xFixCycles(void);
    void S9xPackStatus(void);
    void S9xUnpackStatus(void);

    uint8 *S9xGetMemPointer(uint32 Address);
    uint8 *S9xGetBasePointer(uint32 Address);

    void   S9xSetPCBase(uint32 Address);
    void   S9xSetWord(uint16 Word, uint32 Address, enum s9xwrap_t w = WRAP_NONE, enum s9xwriteorder_t o = WRITE_01);
    void   S9xSetByte(uint8 Byte, uint32 Address);
    uint16 S9xGetWord(uint32 Address, enum s9xwrap_t w = WRAP_NONE);
    uint8  S9xGetByte(uint32 Address);
    int32  memory_speed(uint32 address);
    void   sha256sum(unsigned char *data, unsigned int length, unsigned char *hash);
    bool8  ReadIPSPatch(Stream *r, long offset, int32 &rom_size);
    long   ReadInt(Stream *r, unsigned nbytes);
    bool8  ReadBPSPatch(Stream *r, long, int32 &rom_size);
    bool8  ReadUPSPatch(Stream *r, long, int32 &rom_size);
    uint32 XPSdecode(const uint8 *data, unsigned &addr, unsigned size);

  private:
    bool8  allASCII(uint8 *b, int size);
    uint32 caCRC32(uint8 *array, uint32 size, uint32 crc32 = 0xffffffff);
};

extern SMulti Multi;

#endif
