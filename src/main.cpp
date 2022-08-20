#include "snem.h"
#include "../allegro.h"

int main(int argc, char *argv[])
{
    SNES *snes = new SNES();

    snes->initsnem();
    snes->loadrom((char *)"rom/ff6.sfc");
    while (1) {
        snes->execframe();
    }
    return 0;
}
END_OF_MAIN()