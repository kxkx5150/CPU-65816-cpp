#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "input.h"
#include "snes.h"


Input::Input(Snes *_snes)
{
    snes = _snes;
}
void Input::input_reset()
{
    latchLine    = false;
    latchedState = 0;
}
void Input::input_cycle()
{
    if (latchLine) {
        latchedState = currentState;
    }
}
uint8_t Input::input_read()
{
    uint8_t ret = latchedState & 1;
    latchedState >>= 1;
    latchedState |= 0x8000;
    return ret;
}
