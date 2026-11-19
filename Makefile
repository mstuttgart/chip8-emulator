CC      ?= gcc
CFLAGS  += -std=c99 -Wall -Wextra -O2
LDFLAGS +=

SDL_CFLAGS := $(shell pkg-config --cflags sdl2)
SDL_LIBS   := $(shell pkg-config --libs sdl2)

BUILD_DIR := build

EMU_TARGET    := chip8-emulator
DISASM_TARGET := chip8-disassembler
TEST_TARGET   := $(BUILD_DIR)/tests/test_chip8

EMU_SRC    := src/chip8_cpu.c src/chip8_system.c src/main.c
DISASM_SRC := disassembler/chip8_disassembler.c disassembler/main.c
TEST_SRC   := src/chip8_cpu.c src/chip8_system.c tests/test_chip8.c

EMU_OBJ    := $(EMU_SRC:%.c=$(BUILD_DIR)/%.o)
DISASM_OBJ := $(DISASM_SRC:%.c=$(BUILD_DIR)/%.o)
TEST_OBJ   := $(TEST_SRC:%.c=$(BUILD_DIR)/%.o)

.PHONY: all test clean

all: $(EMU_TARGET) $(DISASM_TARGET)

$(EMU_TARGET): $(EMU_OBJ)
	$(CC) $(EMU_OBJ) -o $@ $(LDFLAGS) $(SDL_LIBS)

$(DISASM_TARGET): $(DISASM_OBJ)
	$(CC) $(DISASM_OBJ) -o $@ $(LDFLAGS)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): $(TEST_OBJ)
	$(CC) $(TEST_OBJ) -o $@ $(LDFLAGS)

# The emulator core (chip8_cpu.c / chip8_system.c) does not depend on SDL2;
# only the frontend (src/main.c) uses the SDL2 cflags.
$(BUILD_DIR)/src/main.o: CFLAGS += $(SDL_CFLAGS)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/src/chip8_cpu.o: src/chip8_cpu.h
$(BUILD_DIR)/src/chip8_system.o: src/chip8_system.h src/chip8_cpu.h
$(BUILD_DIR)/src/main.o: src/chip8_system.h src/chip8_cpu.h
$(BUILD_DIR)/disassembler/chip8_disassembler.o: disassembler/chip8_disassembler.h
$(BUILD_DIR)/disassembler/main.o: disassembler/chip8_disassembler.h
$(BUILD_DIR)/tests/test_chip8.o: CFLAGS += -Isrc
$(BUILD_DIR)/tests/test_chip8.o: src/chip8_system.h src/chip8_cpu.h

clean:
	rm -rf $(BUILD_DIR) $(EMU_TARGET) $(DISASM_TARGET)
