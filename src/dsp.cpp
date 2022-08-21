#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "dsp.h"
#include "apu.h"


Dsp::Dsp(Apu *_apu)
{
    apu = _apu;
}
void Dsp::dsp_reset()
{
    memset(ram, 0, sizeof(ram));
    ram[0x7c] = 0xff;

    for (int i = 0; i < 8; i++) {
        channel[i].pitch           = 0;
        channel[i].pitchCounter    = 0;
        channel[i].pitchModulation = false;

        memset(channel[i].decodeBuffer, 0, sizeof(channel[i].decodeBuffer));

        channel[i].srcn          = 0;
        channel[i].decodeOffset  = 0;
        channel[i].previousFlags = 0;
        channel[i].old           = 0;
        channel[i].older         = 0;
        channel[i].useNoise      = false;

        memset(channel[i].adsrRates, 0, sizeof(channel[i].adsrRates));

        channel[i].rateCounter  = 0;
        channel[i].adsrState    = 0;
        channel[i].sustainLevel = 0;
        channel[i].useGain      = false;
        channel[i].gainMode     = 0;
        channel[i].directGain   = false;
        channel[i].gainValue    = 0;
        channel[i].gain         = 0;
        channel[i].keyOn        = false;
        channel[i].keyOff       = false;
        channel[i].sampleOut    = 0;
        channel[i].volumeL      = 0;
        channel[i].volumeR      = 0;
        channel[i].echoEnable   = false;
    }

    dirPage         = 0;
    evenCycle       = false;
    mute            = true;
    reset           = true;
    masterVolumeL   = 0;
    masterVolumeR   = 0;
    noiseSample     = -0x4000;
    noiseRate       = 0;
    noiseCounter    = 0;
    echoWrites      = false;
    echoVolumeL     = 0;
    echoVolumeR     = 0;
    feedbackVolume  = 0;
    echoBufferAdr   = 0;
    echoDelay       = 1;
    echoRemain      = 1;
    echoBufferIndex = 0;
    firBufferIndex  = 0;
    lowerVolume     = 5;

    memset(firValues, 0, sizeof(firValues));
    memset(firBufferL, 0, sizeof(firBufferL));
    memset(firBufferR, 0, sizeof(firBufferR));
    memset(sampleBuffer, 0, sizeof(sampleBuffer));

    sampleOffset = 0;
}
void Dsp::dsp_cycle()
{
    int totalL = 0;
    int totalR = 0;

    for (int i = 0; i < 8; i++) {
        dsp_cycleChannel(i);
        totalL += (channel[i].sampleOut * channel[i].volumeL) >> 6;
        totalR += (channel[i].sampleOut * channel[i].volumeR) >> 6;
        totalL = totalL < -0x8000 ? -0x8000 : (totalL > 0x7fff ? 0x7fff : totalL);
        totalR = totalR < -0x8000 ? -0x8000 : (totalR > 0x7fff ? 0x7fff : totalR);
    }

    totalL = (totalL * masterVolumeL) >> 7;
    totalR = (totalR * masterVolumeR) >> 7;
    totalL = totalL < -0x8000 ? -0x8000 : (totalL > 0x7fff ? 0x7fff : totalL);
    totalR = totalR < -0x8000 ? -0x8000 : (totalR > 0x7fff ? 0x7fff : totalR);

    dsp_handleEcho(&totalL, &totalR);
    if (mute) {
        totalL = 0;
        totalR = 0;
    }

    dsp_handleNoise();

    sampleBuffer[sampleOffset * 2]     = totalL;
    sampleBuffer[sampleOffset * 2 + 1] = totalR;

    if (sampleOffset < 533)
        sampleOffset++;

    evenCycle = !evenCycle;
}
void Dsp::dsp_handleEcho(int *outputL, int *outputR)
{
    uint16_t adr = echoBufferAdr + echoBufferIndex * 4;

    firBufferL[firBufferIndex] = (apu->ram[adr] + (apu->ram[(adr + 1) & 0xffff] << 8));
    firBufferL[firBufferIndex] >>= 1;
    firBufferR[firBufferIndex] = (apu->ram[(adr + 2) & 0xffff] + (apu->ram[(adr + 3) & 0xffff] << 8));
    firBufferR[firBufferIndex] >>= 1;

    int sumL = 0, sumR = 0;

    for (int i = 0; i < 8; i++) {
        sumL += (firBufferL[(firBufferIndex + i + 1) & 0x7] * firValues[i]) >> 6;
        sumR += (firBufferR[(firBufferIndex + i + 1) & 0x7] * firValues[i]) >> 6;

        if (i == 6) {
            sumL = ((int16_t)(sumL & 0xffff));
            sumR = ((int16_t)(sumR & 0xffff));
        }
    }

    sumL = sumL < -0x8000 ? -0x8000 : (sumL > 0x7fff ? 0x7fff : sumL);
    sumR = sumR < -0x8000 ? -0x8000 : (sumR > 0x7fff ? 0x7fff : sumR);

    int outL = *outputL + ((sumL * echoVolumeL) >> 7);
    int outR = *outputR + ((sumR * echoVolumeR) >> 7);

    *outputL = outL < -0x8000 ? -0x8000 : (outL > 0x7fff ? 0x7fff : outL);
    *outputR = outR < -0x8000 ? -0x8000 : (outR > 0x7fff ? 0x7fff : outR);

    int inL = 0, inR = 0;
    for (int i = 0; i < 8; i++) {
        if (channel[i].echoEnable) {
            inL += (channel[i].sampleOut * channel[i].volumeL) >> 6;
            inR += (channel[i].sampleOut * channel[i].volumeR) >> 6;
            inL = inL < -0x8000 ? -0x8000 : (inL > 0x7fff ? 0x7fff : inL);
            inR = inR < -0x8000 ? -0x8000 : (inR > 0x7fff ? 0x7fff : inR);
        }
    }

    inL += (sumL * feedbackVolume) >> 7;
    inR += (sumR * feedbackVolume) >> 7;
    inL = inL < -0x8000 ? -0x8000 : (inL > 0x7fff ? 0x7fff : inL);
    inR = inR < -0x8000 ? -0x8000 : (inR > 0x7fff ? 0x7fff : inR);
    inL &= 0xfffe;
    inR &= 0xfffe;

    if (echoWrites) {
        apu->ram[adr]                = inL & 0xff;
        apu->ram[(adr + 1) & 0xffff] = inL >> 8;
        apu->ram[(adr + 2) & 0xffff] = inR & 0xff;
        apu->ram[(adr + 3) & 0xffff] = inR >> 8;
    }

    firBufferIndex++;
    firBufferIndex &= 7;
    echoBufferIndex++;
    echoRemain--;

    if (echoRemain == 0) {
        echoRemain      = echoDelay;
        echoBufferIndex = 0;
    }
}
void Dsp::dsp_cycleChannel(int ch)
{
    uint16_t pitch = channel[ch].pitch;
    if (ch > 0 && channel[ch].pitchModulation) {
        int factor = (channel[ch - 1].sampleOut >> 4) + 0x400;
        pitch      = (pitch * factor) >> 10;
        if (pitch > 0x3fff)
            pitch = 0x3fff;
    }

    int newCounter = channel[ch].pitchCounter + pitch;

    if (newCounter > 0xffff) {
        dsp_decodeBrr(ch);
    }

    channel[ch].pitchCounter = newCounter;

    int16_t sample = 0;

    if (channel[ch].useNoise) {
        sample = noiseSample;
    } else {
        sample = dsp_getSample(ch, channel[ch].pitchCounter >> 12, (channel[ch].pitchCounter >> 4) & 0xff);
    }

    if (evenCycle) {
        if (channel[ch].keyOff) {
            channel[ch].adsrState = 4;
        } else if (channel[ch].keyOn) {
            channel[ch].keyOn         = false;
            channel[ch].previousFlags = 0;
            uint16_t samplePointer    = dirPage + 4 * channel[ch].srcn;
            channel[ch].decodeOffset  = apu->ram[samplePointer];
            channel[ch].decodeOffset |= apu->ram[(samplePointer + 1) & 0xffff] << 8;
            memset(channel[ch].decodeBuffer, 0, sizeof(channel[ch].decodeBuffer));
            channel[ch].gain      = 0;
            channel[ch].adsrState = channel[ch].useGain ? 3 : 0;
        }
    }

    if (reset) {
        channel[ch].adsrState = 4;
        channel[ch].gain      = 0;
    }

    bool doingDirectGain = channel[ch].adsrState != 4 && channel[ch].useGain && channel[ch].directGain;

    uint16_t rate = channel[ch].adsrState == 4 ? 0 : channel[ch].adsrRates[channel[ch].adsrState];

    if (channel[ch].adsrState != 4 && !doingDirectGain && rate != 0) {
        channel[ch].rateCounter++;
    }

    if (channel[ch].adsrState == 4 || (!doingDirectGain && channel[ch].rateCounter >= rate && rate != 0)) {
        if (channel[ch].adsrState != 4)
            channel[ch].rateCounter = 0;
        dsp_handleGain(ch);
    }

    if (doingDirectGain)
        channel[ch].gain = channel[ch].gainValue;

    ram[(ch << 4) | 8]    = channel[ch].gain >> 4;
    sample                = (sample * channel[ch].gain) >> 11;
    ram[(ch << 4) | 9]    = sample >> 7;
    channel[ch].sampleOut = sample;
}
void Dsp::dsp_handleGain(int ch)
{
    switch (channel[ch].adsrState) {
        case 0: {
            uint16_t rate = channel[ch].adsrRates[channel[ch].adsrState];
            channel[ch].gain += rate == 1 ? 1024 : 32;

            if (channel[ch].gain >= 0x7e0)
                channel[ch].adsrState = 1;
            if (channel[ch].gain > 0x7ff)
                channel[ch].gain = 0x7ff;
            break;
        }
        case 1: {
            channel[ch].gain -= ((channel[ch].gain - 1) >> 8) + 1;

            if (channel[ch].gain < channel[ch].sustainLevel)
                channel[ch].adsrState = 2;
            break;
        }
        case 2: {
            channel[ch].gain -= ((channel[ch].gain - 1) >> 8) + 1;
            break;
        }
        case 3: {
            switch (channel[ch].gainMode) {
                case 0: {
                    channel[ch].gain -= 32;

                    if (channel[ch].gain > 0x7ff)
                        channel[ch].gain = 0;
                    break;
                }
                case 1: {
                    channel[ch].gain -= ((channel[ch].gain - 1) >> 8) + 1;
                    break;
                }
                case 2: {
                    channel[ch].gain += 32;

                    if (channel[ch].gain > 0x7ff)
                        channel[ch].gain = 0x7ff;
                    break;
                }
                case 3: {
                    channel[ch].gain += channel[ch].gain < 0x600 ? 32 : 8;

                    if (channel[ch].gain > 0x7ff)
                        channel[ch].gain = 0x7ff;
                    break;
                }
            }
            break;
        }
        case 4: {
            channel[ch].gain -= 8;

            if (channel[ch].gain > 0x7ff)
                channel[ch].gain = 0;
            break;
        }
    }
}
int16_t Dsp::dsp_getSample(int ch, int sampleNum, int offset)
{
    int16_t news    = channel[ch].decodeBuffer[sampleNum + 3];
    int16_t olds    = channel[ch].decodeBuffer[sampleNum + 2];
    int16_t olders  = channel[ch].decodeBuffer[sampleNum + 1];
    int16_t oldests = channel[ch].decodeBuffer[sampleNum];

    int out = (gaussValues[0xff - offset] * oldests) >> 10;
    out += (gaussValues[0x1ff - offset] * olders) >> 10;
    out += (gaussValues[0x100 + offset] * olds) >> 10;
    out = ((int16_t)(out & 0xffff));
    out += (gaussValues[offset] * news) >> 10;
    out = out < -0x8000 ? -0x8000 : (out > 0x7fff ? 0x7fff : out);
    return out >> 1;
}
void Dsp::dsp_decodeBrr(int ch)
{
    channel[ch].decodeBuffer[0] = channel[ch].decodeBuffer[16];
    channel[ch].decodeBuffer[1] = channel[ch].decodeBuffer[17];
    channel[ch].decodeBuffer[2] = channel[ch].decodeBuffer[18];

    if (channel[ch].previousFlags == 1 || channel[ch].previousFlags == 3) {
        uint16_t samplePointer   = dirPage + 4 * channel[ch].srcn;
        channel[ch].decodeOffset = apu->ram[(samplePointer + 2) & 0xffff];
        channel[ch].decodeOffset |= (apu->ram[(samplePointer + 3) & 0xffff]) << 8;

        if (channel[ch].previousFlags == 1) {
            channel[ch].adsrState = 4;
            channel[ch].gain      = 0;
        }
        ram[0x7c] |= 1 << ch;
    }

    uint8_t header = apu->ram[channel[ch].decodeOffset++];
    int     shift  = header >> 4;
    int     filter = (header & 0xc) >> 2;

    channel[ch].previousFlags = header & 0x3;
    uint8_t curByte           = 0;
    int     old               = channel[ch].old;
    int     older             = channel[ch].older;

    for (int i = 0; i < 16; i++) {
        int s = 0;
        if (i & 1) {
            s = curByte & 0xf;
        } else {
            curByte = apu->ram[channel[ch].decodeOffset++];
            s       = curByte >> 4;
        }

        if (s > 7)
            s -= 16;

        if (shift <= 0xc) {
            s = (s << shift) >> 1;
        } else {
            s = (s >> 3) << 12;
        }

        switch (filter) {
            case 1:
                s += old + (-old >> 4);
                break;
            case 2:
                s += 2 * old + ((3 * -old) >> 5) - older + (older >> 4);
                break;
            case 3:
                s += 2 * old + ((13 * -old) >> 6) - older + ((3 * older) >> 4);
                break;
        }

        s = s < -0x8000 ? -0x8000 : (s > 0x7fff ? 0x7fff : s);
        s = ((int16_t)((s & 0x7fff) << 1)) >> 1;

        older = old;
        old   = s;

        channel[ch].decodeBuffer[i + 3] = s;
    }
    channel[ch].older = older;
    channel[ch].old   = old;
}
void Dsp::dsp_handleNoise()
{
    if (noiseRate != 0) {
        noiseCounter++;
    }

    if (noiseCounter >= noiseRate && noiseRate != 0) {
        int bit      = (noiseSample & 1) ^ ((noiseSample >> 1) & 1);
        noiseSample  = ((noiseSample >> 1) & 0x3fff) | (bit << 14);
        noiseSample  = ((int16_t)((noiseSample & 0x7fff) << 1)) >> 1;
        noiseCounter = 0;
    }
}
uint8_t Dsp::dsp_read(uint8_t adr)
{
    return ram[adr];
}
void Dsp::dsp_write(uint8_t adr, uint8_t val)
{
    int ch = adr >> 4;
    switch (adr) {
        case 0x00:
        case 0x10:
        case 0x20:
        case 0x30:
        case 0x40:
        case 0x50:
        case 0x60:
        case 0x70: {
            channel[ch].volumeL = val / lowerVolume;
            break;
        }
        case 0x01:
        case 0x11:
        case 0x21:
        case 0x31:
        case 0x41:
        case 0x51:
        case 0x61:
        case 0x71: {
            channel[ch].volumeR = val / lowerVolume;
            break;
        }
        case 0x02:
        case 0x12:
        case 0x22:
        case 0x32:
        case 0x42:
        case 0x52:
        case 0x62:
        case 0x72: {
            channel[ch].pitch = (channel[ch].pitch & 0x3f00) | val;
            break;
        }
        case 0x03:
        case 0x13:
        case 0x23:
        case 0x33:
        case 0x43:
        case 0x53:
        case 0x63:
        case 0x73: {
            channel[ch].pitch = ((channel[ch].pitch & 0x00ff) | (val << 8)) & 0x3fff;
            break;
        }
        case 0x04:
        case 0x14:
        case 0x24:
        case 0x34:
        case 0x44:
        case 0x54:
        case 0x64:
        case 0x74: {
            channel[ch].srcn = val;
            break;
        }
        case 0x05:
        case 0x15:
        case 0x25:
        case 0x35:
        case 0x45:
        case 0x55:
        case 0x65:
        case 0x75: {
            channel[ch].adsrRates[0] = rateValues[(val & 0xf) * 2 + 1];
            channel[ch].adsrRates[1] = rateValues[((val & 0x70) >> 4) * 2 + 16];
            channel[ch].useGain      = (val & 0x80) == 0;
            break;
        }
        case 0x06:
        case 0x16:
        case 0x26:
        case 0x36:
        case 0x46:
        case 0x56:
        case 0x66:
        case 0x76: {
            channel[ch].adsrRates[2] = rateValues[val & 0x1f];
            channel[ch].sustainLevel = (((val & 0xe0) >> 5) + 1) * 0x100;
            break;
        }
        case 0x07:
        case 0x17:
        case 0x27:
        case 0x37:
        case 0x47:
        case 0x57:
        case 0x67:
        case 0x77: {
            channel[ch].directGain = (val & 0x80) == 0;
            if (val & 0x80) {
                channel[ch].gainMode     = (val & 0x60) >> 5;
                channel[ch].adsrRates[3] = rateValues[val & 0x1f];
            } else {
                channel[ch].gainValue = (val & 0x7f) * 16;
            }
            break;
        }
        case 0x0c: {
            masterVolumeL = val;
            break;
        }
        case 0x1c: {
            masterVolumeR = val;
            break;
        }
        case 0x2c: {
            echoVolumeL = val;
            break;
        }
        case 0x3c: {
            echoVolumeR = val;
            break;
        }
        case 0x4c: {
            for (int i = 0; i < 8; i++) {
                channel[i].keyOn = val & (1 << i);
            }
            break;
        }
        case 0x5c: {
            for (int i = 0; i < 8; i++) {
                channel[i].keyOff = val & (1 << i);
            }
            break;
        }
        case 0x6c: {
            reset      = val & 0x80;
            mute       = val & 0x40;
            echoWrites = (val & 0x20) == 0;
            noiseRate  = rateValues[val & 0x1f];
            break;
        }
        case 0x7c: {
            val = 0;
            break;
        }
        case 0x0d: {
            feedbackVolume = val;
            break;
        }
        case 0x2d: {
            for (int i = 0; i < 8; i++) {
                channel[i].pitchModulation = val & (1 << i);
            }
            break;
        }
        case 0x3d: {
            for (int i = 0; i < 8; i++) {
                channel[i].useNoise = val & (1 << i);
            }
            break;
        }
        case 0x4d: {
            for (int i = 0; i < 8; i++) {
                channel[i].echoEnable = val & (1 << i);
            }
            break;
        }
        case 0x5d: {
            dirPage = val << 8;
            break;
        }
        case 0x6d: {
            echoBufferAdr = val << 8;
            break;
        }
        case 0x7d: {
            echoDelay = (val & 0xf) * 512;
            if (echoDelay == 0)
                echoDelay = 1;
            break;
        }
        case 0x0f:
        case 0x1f:
        case 0x2f:
        case 0x3f:
        case 0x4f:
        case 0x5f:
        case 0x6f:
        case 0x7f: {
            firValues[ch] = val;
            break;
        }
    }
    ram[adr] = val;
}
void Dsp::dsp_getSamples(int16_t *sampleData, int samplesPerFrame)
{
    double adder    = 534.0 / samplesPerFrame;
    double location = 0.0;
    for (int i = 0; i < samplesPerFrame; i++) {
        sampleData[i * 2]     = sampleBuffer[((int)location) * 2];
        sampleData[i * 2 + 1] = sampleBuffer[((int)location) * 2 + 1];
        location += adder;
    }
    sampleOffset = 0;
}
