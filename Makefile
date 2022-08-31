#S9XNETPLAY=0
#S9XZIP=0
#SYSTEM_ZIP=0

# Fairly good and special-char-safe descriptor of the os being built on.
OS         = `uname -s -r -m|sed \"s/ /-/g\"|tr \"[A-Z]\" \"[a-z]\"|tr \"/()\" \"___\"`
BUILDDIR   = .

ROMCHIPS   = src/rom_chip/stream.o src/rom_chip/msu1.o
OBJECTS    = src/renderer.o src/cpu.o src/dma.o src/mem.o src/ppu.o src/snes.o src/tile.o src/utils/compat.o src/gui.o \
src/apu/apu.o src/apu/SNES_SPC_misc.o src/apu/SNES_SPC_state.o src/apu/SNES_SPC.o src/apu/SPC_DSP.o src/apu/SPC_Filter.o

DEFS       = -DMITSHM


CCC        = g++
CC         = gcc
GASM       = g++
INCLUDES   += -I. -I.. -Isrc/apu/ -Isrc/

OPT        = -O0
CCFLAGS    = -std=c++11 -g $(OPT) -fomit-frame-pointer -fno-exceptions -fno-rtti -pedantic -W -Wno-unused-parameter -DHAVE_STRINGS_H -DHAVE_STDINT_H $(DEFS)
CFLAGS     = $(CCFLAGS)

.SUFFIXES: .o .cpp .c .cc .h .m .i .s .obj

snes: $(OBJECTS) $(ROMCHIPS)
	$(CCC) $(LDFLAGS) $(INCLUDES) -o $@ $(OBJECTS) $(ROMCHIPS) -lm -lz -lICE -lpthread -lpng -lSDL2


.cpp.o:
	$(CCC) $(INCLUDES) -c $(CCFLAGS) $*.cpp -o $@

.c.o:
	$(CC) $(INCLUDES) -c $(CCFLAGS) $*.c -o $@

.cpp.S:
	$(GASM) $(INCLUDES) -S $(CCFLAGS) $*.cpp -o $@

.cpp.i:
	$(GASM) $(INCLUDES) -E $(CCFLAGS) $*.cpp -o $@

.S.o:
	$(GASM) $(INCLUDES) -c $(CCFLAGS) $*.S -o $@

.S.i:
	$(GASM) $(INCLUDES) -c -E $(CCFLAGS) $*.S -o $@

.s.o:
	@echo Compiling $*.s
	sh-elf-as -little $*.s -o $@

.obj.o:
	cp $*.obj $*.o

clean:
	rm -f $(OBJECTS) $(ROMCHIPS) snes
