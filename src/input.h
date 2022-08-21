#ifndef INPUT_H
#define INPUT_H
#include <stdint.h>


class Snes;
class Input {
  public:
    Snes *snes = nullptr;

    uint8_t  type         = 1;
    bool     latchLine    = false;
    uint16_t currentState = 0;
    uint16_t latchedState = 0;

  public:
    Input(Snes *_snes);

    void    input_reset();
    void    input_cycle();
    uint8_t input_read();
};
#endif
