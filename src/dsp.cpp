#include <cstdint>
#include <stdlib.h>
#include "snes.h"
#include "spc.h"
#include "dsp.h"

#define ATTACK  1
#define DECAY   2
#define SUSTAIN 3
#define RELEASE 4

DSP::DSP(SNES *_snes)
{
    snes = _snes;
}
void DSP::drawvol(BITMAP *b)
{
}
void DSP::writedsp(uint16_t a, uint8_t v)
{
    int      c;
    uint32_t templ;
    if (a & 1) {
        dspregs[curdspreg] = v;
        switch (curdspreg) {
            case 0x00:
            case 0x10:
            case 0x20:
            case 0x30:
            case 0x40:
            case 0x50:
            case 0x60:
            case 0x70:
                volumel[curdspreg >> 4] = v;
                break;
            case 0x01:
            case 0x11:
            case 0x21:
            case 0x31:
            case 0x41:
            case 0x51:
            case 0x61:
            case 0x71:
                volumer[curdspreg >> 4] = v;
                break;
            case 0x02:
            case 0x12:
            case 0x22:
            case 0x32:
            case 0x42:
            case 0x52:
            case 0x62:
            case 0x72:
                pitch[curdspreg >> 4] = (pitch[curdspreg >> 4] & 0x3F00) | v;
                break;
            case 0x03:
            case 0x13:
            case 0x23:
            case 0x33:
            case 0x43:
            case 0x53:
            case 0x63:
            case 0x73:
                pitch[curdspreg >> 4] = (pitch[curdspreg >> 4] & 0xFF) | ((v & 0x3F) << 8);
                break;
            case 0x04:
            case 0x14:
            case 0x24:
            case 0x34:
            case 0x44:
            case 0x54:
            case 0x64:
            case 0x74:
                if ((v << 2) != sourcenum[curdspreg >> 4]) {
                    sourcenum[curdspreg >> 4] = v << 2;
                    pos[curdspreg >> 4]       = 0;
                    templ                     = (dir << 8) + (v << 2);
                    voiceaddr[curdspreg >> 4] = snes->spc->spcram[templ] | (snes->spc->spcram[templ + 1] << 8);
                    brrstat[curdspreg >> 4]   = 0;
                }
                break;
            case 0x05:
            case 0x15:
            case 0x25:
            case 0x35:
            case 0x45:
            case 0x55:
            case 0x65:
            case 0x75:
                adsr1[curdspreg >> 4] = v;
                break;
            case 0x06:
            case 0x16:
            case 0x26:
            case 0x36:
            case 0x46:
            case 0x56:
            case 0x66:
            case 0x76:
                adsr2[curdspreg >> 4] = v;
                etype[curdspreg >> 4] = (gain[curdspreg >> 4] & 0x80) ? 1 : 0;
                etype[curdspreg >> 4] |= (adsr1[curdspreg >> 4] & 0x80) ? 2 : 0;
                break;
            case 0x07:
            case 0x17:
            case 0x27:
            case 0x37:
            case 0x47:
            case 0x57:
            case 0x67:
            case 0x77:
                gain[curdspreg >> 4]  = v;
                etype[curdspreg >> 4] = (gain[curdspreg >> 4] & 0x80) ? 1 : 0;
                etype[curdspreg >> 4] |= (adsr1[curdspreg >> 4] & 0x80) ? 2 : 0;
                break;
            case 0x4C:
                for (c = 0; c < 8; c++) {
                    if (v & (1 << c)) {
                        voiceon[c]   = 1;
                        voiceaddr[c] = snes->spc->spcram[(dir << 8) + sourcenum[c]] |
                                       (snes->spc->spcram[(dir << 8) + sourcenum[c] + 1] << 8);
                        brrstat[c]  = 0;
                        pos[c]      = 0;
                        adsrstat[c] = ATTACK;
                        evol[c]     = 0;
                        edelay[c]   = 8;
                        voiceend[c] = 0;
                        endx &= ~(1 << c);
                    }
                }
                break;
            case 0x5C:
                for (c = 0; c < 8; c++) {
                    if (v & (1 << c)) {
                        adsrstat[c] = RELEASE;
                        edelay[c]   = 1;
                    }
                }
                break;
            case 0x5D:
                dir = v;
                break;
            case 0x3D:
                non = v;
                break;
            case 0x6C:
                noiserate  = v & 0x1F;
                noisedelay = 1;
                break;
        }
    } else
        curdspreg = v & 127;
}
void DSP::resetdsp()
{
    noise = 0x4000;
}
uint8_t DSP::readdsp(uint16_t a)
{
    if (a & 1) {
        switch (curdspreg & 0x7F) {
            case 0x08:
            case 0x18:
            case 0x28:
            case 0x38:
            case 0x48:
            case 0x58:
            case 0x68:
            case 0x78:
                return envx[curdspreg >> 4];
            case 0x09:
            case 0x19:
            case 0x29:
            case 0x39:
            case 0x49:
            case 0x59:
            case 0x69:
            case 0x79:
                return outx[curdspreg >> 4];
            case 0x7C:
                return endx;
        }
        return dspregs[curdspreg & 127];
    }
    return curdspreg;
}
int16_t DSP::decodebrr(int v, int c)
{
    int16_t temp = v & 0xF;
    if (temp & 8) {
        temp |= 0xFFF0;
    }
    if (range[c] <= 12) {
        temp <<= range[c];
    } else {
        temp = (temp & 8) ? 0xF800 : 0;
    }
    switch (filter[c]) {
        case 0:
            break;
        case 1:
            temp = temp + lastsamp[c][0] + ((-lastsamp[c][0]) >> 4);
            break;
        case 2:
            temp = temp + (lastsamp[c][0] << 1) + ((-((lastsamp[c][0] << 1) + lastsamp[c][0])) >> 5) - lastsamp[c][1] +
                   (lastsamp[c][1] >> 4);
            break;
        case 3:
            temp = temp + (lastsamp[c][0] << 1) +
                   ((-(lastsamp[c][0] + (lastsamp[c][0] << 2) + (lastsamp[c][0] << 3))) >> 6) - lastsamp[c][1] +
                   (((lastsamp[c][1] << 1) + lastsamp[c][1]) >> 4);
            break;
        default:
            exit(-1);
    }
    lastsamp[c][1] = lastsamp[c][0];
    lastsamp[c][0] = temp;
    return temp;
}
int16_t DSP::getbrr(int c)
{
    int     temp;
    int16_t sample;
    if (!voiceon[c]) {
        return 0;
    }
    if (!brrstat[c]) {
        brrstat[c] = 1;
        brrctrl[c] = snes->spc->spcram[voiceaddr[c]++];
        range[c]   = brrctrl[c] >> 4;
        filter[c]  = (brrctrl[c] >> 2) & 3;
    }
    if (brrstat[c] & 1) {
        temp = snes->spc->spcram[voiceaddr[c]] >> 4;
        brrstat[c]++;
        sample = decodebrr(temp, c);
    } else {
        temp = snes->spc->spcram[voiceaddr[c]++] & 0xF;
        brrstat[c]++;
        sample = decodebrr(temp, c);
        if (brrstat[c] == 17) {
            brrstat[c] = 0;
            if (brrctrl[c] & 1) {
                if (brrctrl[c] & 2) {
                    voiceaddr[c] = snes->spc->spcram[(dir << 8) + sourcenum[c] + 2] |
                                   (snes->spc->spcram[(dir << 8) + sourcenum[c] + 3] << 8);
                } else {
                    voiceon[c] = 0;
                    endx |= (1 << c);
                }
            }
        }
    }
    return sample;
}
void DSP::initdsp()
{
    install_sound(DIGI_AUTODETECT, MIDI_NONE, 0);
    as = play_audio_stream(3200 / 5, 16, TRUE, 32000, 255, 128);
}
void DSP::refillbuffer()
{
    uint16_t *p = NULL;
    int       c;
    if (!dspqlen) {
        return;
    }
    while (!p)
        p = (uint16_t *)get_audio_stream_buffer(as);
    for (c = 0; c < (6400 / 5); c++)
        p[c] = dsprealbuffer[dsprsel][c] ^ 0x8000;
    free_audio_stream_buffer(as);
    dsprsel++;
    dsprsel &= 7;
    dspqlen--;
}
void DSP::pollsound()
{
    if (bufferready) {
        bufferready--;
        refillbuffer();
    }
}
void DSP::polldsp()
{
    int     c;
    int     sample;
    int16_t s;
    int16_t totalsamplel = 0, totalsampler = 0;
    for (c = 0; c < 8; c++) {
        pitchcounter[c] += pitch[c];
        if (pitchcounter[c] < 0) {
            sample = dspsamples[c];
        } else {
            while (pitchcounter[c] >= 0 && pitch[c]) {
                s             = (int16_t)getbrr(c);
                sample        = (int)s;
                dspsamples[c] = sample;
                pitchcounter[c] -= 0x1000;
            }
        }
        if (non & (1 << c)) {
            sample = noise & 0x3FFF;
            if (noise & 0x4000) {
                sample |= 0xFFFF8000;
            }
        }
        sample *= evol[c];
        sample >>= 11;
        outx[c] = sample >> 8;
        if (volumel[c]) {
            totalsamplel += (((sample * volumel[c]) >> 7) >> 3);
        }
        if (volumer[c]) {
            totalsampler += (((sample * volumer[c]) >> 7) >> 3);
        }
        edelay[c]--;
        if (edelay[c] <= 0) {
            if (adsrstat[c] == RELEASE) {
                edelay[c] = 1;
                evol[c] -= 8;
            } else
                switch (etype[c]) {
                    case 0:
                        evol[c] = (gain[c] & 0x7F) << 4;
                        break;
                    case 1:
                        switch ((gain[c] >> 5) & 3) {
                            case 0:
                                evol[c] -= 32;
                                break;
                            case 1:
                                evol[c] -= ((evol[c] - 1) >> 8) + 1;
                                break;
                            case 2:
                                evol[c] += 32;
                                break;
                            case 3:
                                evol[c] += (evol[c] < 0x600) ? 32 : 8;
                                break;
                        }
                        edelay[c] = ratetable[gain[c] & 0x1F];
                        break;
                    case 2:
                    case 3:
                        switch (adsrstat[c]) {
                            case ATTACK:
                                if ((adsr1[c] & 0xF) == 0xF) {
                                    evol[c] += 1024;
                                    edelay[c] = 1;
                                } else {
                                    evol[c] += 32;
                                    edelay[c] = ratetable[((adsr1[c] & 0xF) << 1) | 1];
                                }
                                if (evol[c] >= 0x7FF) {
                                    evol[c]     = 0x7FF;
                                    adsrstat[c] = DECAY;
                                }
                                break;
                            case DECAY:
                                evol[c] -= ((evol[c] - 1) >> 8) + 1;
                                if (evol[c] <= (adsr2[c] & 0xE0) << 3) {
                                    evol[c]     = (adsr2[c] & 0xE0) << 3;
                                    adsrstat[c] = SUSTAIN;
                                }
                                edelay[c] = ratetable[((adsr1[c] & 0x70) >> 2) | 1];
                                break;
                            case SUSTAIN:
                                evol[c] -= ((evol[c] - 1) >> 8) + 1;
                                edelay[c] = ratetable[adsr2[c] & 0x1F];
                                break;
                            case RELEASE:
                                edelay[c] = 1;
                                evol[c] -= 8;
                                break;
                        }
                        break;
                }
            if (evol[c] > 0x7FF) {
                evol[c] = 0x7FF;
            }
            if (evol[c] < 0) {
                evol[c] = 0;
            }
            envx[c] = (evol[c] >> 4);
        }
    }
    noisedelay--;
    if (noisedelay <= 0) {
        noisedelay += ratetable[noiserate];
        noise = (noise >> 1) | (((noise << 14) ^ (noise << 13)) & 0x4000);
    }
    dspbuffer[dsppos++] = totalsamplel;
    dspbuffer[dsppos++] = totalsampler;
    if (dsppos >= (6400 / 5)) {
        if (dspqlen == 8) {
            dspwsel--;
            dspwsel &= 7;
            dspqlen--;
        }
        memcpy(dsprealbuffer[dspwsel], dspbuffer, (6400 / 5) * 2);
        dspqlen++;
        dspwsel++;
        dspwsel &= 7;
        dsppos = 0;
        bufferready++;
    }
}
