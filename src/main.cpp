#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include "snes.h"

static void playAudio(Snes *snes, SDL_AudioDeviceID device, int16_t *audioBuffer);
static void renderScreen(Snes *snes, SDL_Renderer *renderer, SDL_Texture *texture);
static void handleInput(Snes *snes, int keyCode, bool pressed);


int main(int argc, char **argv)
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    SDL_Window   *window   = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 512, 480, 0);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBX8888, SDL_TEXTUREACCESS_STREAMING, 512, 480);

    SDL_AudioSpec     want, have;
    SDL_AudioDeviceID device;
    SDL_memset(&want, 0, sizeof(want));

    want.freq     = 44100;
    want.format   = AUDIO_S16;
    want.channels = 2;
    want.samples  = 2048;
    want.callback = nullptr;
    device        = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);

    int16_t *audioBuffer = (int16_t *)malloc(735 * 4);
    SDL_PauseAudioDevice(device, 0);

    Snes *snes = new Snes();
    snes->cart->loadRom(argv[1], snes);

    // display stuff
    SDL_DisplayMode mode;
    SDL_GetDisplayMode(0, 0, &mode);

    int   refreshRate = mode.refresh_rate;
    float adder       = 60 / (float)refreshRate;
    float addCol      = 0.0;
    int   expectedMs  = 1000 / refreshRate;

    // sdl loop
    bool      running = true;
    bool      paused  = false;
    bool      runOne  = false;
    bool      turbo   = false;
    SDL_Event event;
    uint32_t  lastTick = SDL_GetTicks();
    uint32_t  curTick  = 0;
    uint32_t  delta    = 0;

    while (running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_KEYDOWN: {
                    switch (event.key.keysym.sym) {
                        case SDLK_r:
                            snes->snes_reset(false);
                            break;
                        case SDLK_e:
                            snes->snes_reset(true);
                            break;
                        case SDLK_o:
                            runOne = true;
                            break;
                        case SDLK_p:
                            paused = !paused;
                            break;
                        case SDLK_t:
                            turbo = true;
                            break;
                    }
                    handleInput(snes, event.key.keysym.sym, true);
                    break;
                }
                case SDL_KEYUP: {
                    switch (event.key.keysym.sym) {
                        case SDLK_t:
                            turbo = false;
                            break;
                    }
                    handleInput(snes, event.key.keysym.sym, false);
                    break;
                }
                case SDL_QUIT: {
                    running = false;
                    break;
                }
                case SDL_DROPFILE: {
                    char *droppedFile = event.drop.file;
                    snes->cart->loadRom(droppedFile, snes);
                    SDL_free(droppedFile);
                    break;
                }
            }
        }
        addCol += adder;
        if (addCol >= 1.0) {
            addCol -= 1.0;
            if (!paused || runOne) {
                runOne = false;
                if (turbo) {
                    snes->snes_runFrame();
                }

                snes->snes_runFrame();
                playAudio(snes, device, audioBuffer);
                renderScreen(snes, renderer, texture);
            }
        }
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);

        curTick = SDL_GetTicks();
        delta   = curTick - lastTick;
        if (delta < expectedMs) {
            SDL_Delay(expectedMs - delta);
        }
        lastTick = curTick;
    }

    delete snes;
    SDL_PauseAudioDevice(device, 1);
    SDL_CloseAudioDevice(device);
    free(audioBuffer);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
static void playAudio(Snes *snes, SDL_AudioDeviceID device, int16_t *audioBuffer)
{
    snes->apu->dsp->dsp_getSamples(audioBuffer, 735);
    if (SDL_GetQueuedAudioSize(device) <= 735 * 4 * 6) {
        SDL_QueueAudio(device, audioBuffer, 735 * 4);
    }
}
static void renderScreen(Snes *snes, SDL_Renderer *renderer, SDL_Texture *texture)
{
    void *pixels = nullptr;
    int   pitch  = 0;
    SDL_LockTexture(texture, nullptr, &pixels, &pitch);
    snes->ppu->ppu_putPixels((uint8_t *)pixels);
    SDL_UnlockTexture(texture);
}
void snes_setButtonState(Snes *snes, int player, int button, bool pressed)
{
    if (player == 1) {
        if (pressed) {
            snes->input1->currentState |= 1 << button;
        } else {
            snes->input1->currentState &= ~(1 << button);
        }
    } else {
        if (pressed) {
            snes->input2->currentState |= 1 << button;
        } else {
            snes->input2->currentState &= ~(1 << button);
        }
    }
}
static void handleInput(Snes *snes, int keyCode, bool pressed)
{
    switch (keyCode) {
        case SDLK_z:
            snes_setButtonState(snes, 1, 0, pressed);
            break;
        case SDLK_a:
            snes_setButtonState(snes, 1, 1, pressed);
            break;
        case SDLK_RSHIFT:
            snes_setButtonState(snes, 1, 2, pressed);
            break;
        case SDLK_RETURN:
            snes_setButtonState(snes, 1, 3, pressed);
            break;
        case SDLK_UP:
            snes_setButtonState(snes, 1, 4, pressed);
            break;
        case SDLK_DOWN:
            snes_setButtonState(snes, 1, 5, pressed);
            break;
        case SDLK_LEFT:
            snes_setButtonState(snes, 1, 6, pressed);
            break;
        case SDLK_RIGHT:
            snes_setButtonState(snes, 1, 7, pressed);
            break;
        case SDLK_x:
            snes_setButtonState(snes, 1, 8, pressed);
            break;
        case SDLK_s:
            snes_setButtonState(snes, 1, 9, pressed);
            break;
        case SDLK_d:
            snes_setButtonState(snes, 1, 10, pressed);
            break;
        case SDLK_c:
            snes_setButtonState(snes, 1, 11, pressed);
            break;
    }
}
