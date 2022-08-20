#ifndef _DSP_H_
#define _DSP_H_
#include <cstdint>
#include "../allegro.h"


class SNES;
class DSP {
  public:
    SNES *snes = nullptr;

    AUDIOSTREAM *as = nullptr;

    int      dspqlen = 0;
    int      dsppos  = 0;
    int      dspwsel = 0, dsprsel = 0;
    int      bufferready = 0;
    int      curdspreg   = 0;
    uint8_t  dir         = 0;
    uint16_t noise       = 0;
    int      noisedelay = 0, noiserate = 0;
    uint8_t  non  = 0;
    uint8_t  endx = 0;

  public:
    int     dspsamples[8]  = {};
    uint8_t dspregs[256]   = {};
    int16_t lastsamp[8][2] = {};
    int     range[8] = {}, filter[8] = {};
    int16_t dspbuffer[20000]        = {};
    int16_t dsprealbuffer[8][20000] = {};
    int     ratetable[32] = {1 << 30, 2048, 1536, 1280, 1024, 768, 640, 512, 384, 320, 256, 192, 160, 128, 96, 80,
                             64,      48,   40,   32,   24,   20,  16,  12,  10,  8,   6,   5,   4,   3,   2,  1};

    int      pos[8]          = {};
    int      pitchcounter[8] = {}, pitch[8] = {};
    int      volumer[8] = {}, volumel[8] = {};
    int      sourcenum[8] = {};
    uint16_t voiceaddr[8] = {};
    uint8_t  brrctrl[8]   = {};
    int      brrstat[8]   = {};
    int      voiceon[8]   = {};
    int      voiceend[8]  = {};
    int      evol[8] = {}, edelay[8] = {}, etype[8] = {};
    uint8_t  gain[8] = {}, adsr1[8] = {}, adsr2[8] = {};
    int      adsrstat[8] = {};
    uint8_t  envx[8] = {}, outx[8] = {};

  public:
    DSP(SNES *_snes);

    void    drawvol(BITMAP *b);
    void    writedsp(uint16_t a, uint8_t v);
    void    resetdsp();
    uint8_t readdsp(uint16_t a);
    int16_t decodebrr(int v, int c);
    int16_t getbrr(int c);
    void    initdsp();
    void    refillbuffer();
    void    pollsound();
    void    polldsp();

  public:
};
#endif
