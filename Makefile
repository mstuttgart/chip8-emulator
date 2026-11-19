CC      ?= gcc
CFLAGS  += -std=c99 -Wall -Wextra -O2
LDFLAGS +=

SDL_CFLAGS := $(shell pkg-config --cflags sdl2)
SDL_LIBS   := $(shell pkg-config --libs sdl2)

GTK_CFLAGS := $(shell pkg-config --cflags gtk4 2>/dev/null)
GTK_LIBS   := $(shell pkg-config --libs gtk4 2>/dev/null)
HAVE_GTK4  := $(shell pkg-config --exists gtk4 2>/dev/null && echo 1)

BUILD_DIR := build

EMU_TARGET    := chip8-emulator
GTK_TARGET    := chip8-emulator-gtk
DISASM_TARGET := chip8-disassembler
TEST_TARGET   := $(BUILD_DIR)/tests/test_chip8

EMU_SRC    := src/chip8_cpu.c src/chip8_system.c src/phosphor.c src/main.c
GTK_SRC    := src/chip8_cpu.c src/chip8_system.c src/phosphor.c src/gtk/main.c
DISASM_SRC := disassembler/chip8_disassembler.c disassembler/main.c
TEST_SRC   := src/chip8_cpu.c src/chip8_system.c src/phosphor.c tests/test_chip8.c

EMU_OBJ    := $(EMU_SRC:%.c=$(BUILD_DIR)/%.o)
GTK_OBJ    := $(GTK_SRC:%.c=$(BUILD_DIR)/%.o)
DISASM_OBJ := $(DISASM_SRC:%.c=$(BUILD_DIR)/%.o)
TEST_OBJ   := $(TEST_SRC:%.c=$(BUILD_DIR)/%.o)

.PHONY: all test clean

all: $(EMU_TARGET) $(DISASM_TARGET)

# The GTK4 frontend is only built when the GTK4 development files are present
ifeq ($(HAVE_GTK4),1)
all: $(GTK_TARGET)
endif

$(EMU_TARGET): $(EMU_OBJ)
	$(CC) $(EMU_OBJ) -o $@ $(LDFLAGS) $(SDL_LIBS)

$(GTK_TARGET): $(GTK_OBJ)
	$(CC) $(GTK_OBJ) -o $@ $(LDFLAGS) $(GTK_LIBS)

$(DISASM_TARGET): $(DISASM_OBJ)
	$(CC) $(DISASM_OBJ) -o $@ $(LDFLAGS)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): $(TEST_OBJ)
	$(CC) $(TEST_OBJ) -o $@ $(LDFLAGS)

# The emulator core (chip8_cpu.c / chip8_system.c) does not depend on any
# graphics library; only the frontends use the SDL2/GTK4 cflags.
$(BUILD_DIR)/src/main.o: CFLAGS += $(SDL_CFLAGS)
$(BUILD_DIR)/src/gtk/main.o: CFLAGS += $(GTK_CFLAGS) -Isrc

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/src/chip8_cpu.o: src/chip8_cpu.h
$(BUILD_DIR)/src/chip8_system.o: src/chip8_system.h src/chip8_cpu.h
$(BUILD_DIR)/src/phosphor.o: src/phosphor.h src/chip8_cpu.h
$(BUILD_DIR)/src/main.o: src/chip8_system.h src/chip8_cpu.h src/phosphor.h
$(BUILD_DIR)/src/gtk/main.o: src/chip8_system.h src/chip8_cpu.h src/phosphor.h
$(BUILD_DIR)/disassembler/chip8_disassembler.o: disassembler/chip8_disassembler.h
$(BUILD_DIR)/disassembler/main.o: disassembler/chip8_disassembler.h
$(BUILD_DIR)/tests/test_chip8.o: CFLAGS += -Isrc
$(BUILD_DIR)/tests/test_chip8.o: src/chip8_system.h src/chip8_cpu.h src/phosphor.h

clean:
	rm -rf $(BUILD_DIR) $(EMU_TARGET) $(GTK_TARGET) $(DISASM_TARGET)
