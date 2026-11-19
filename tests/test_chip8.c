/**
 * @file test_chip8.c
 * @brief Unit tests for the emulator backend (chip8_cpu / chip8_system).
 *
 * No external dependencies: compile and run with `make test`.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chip8_cpu.h"
#include "chip8_system.h"

static int assertions_failed = 0;
static int tests_run = 0;

#define ASSERT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            printf("    FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            assertions_failed++; \
        } \
    } while (0)

#define ASSERT_EQ(expected, actual) \
    do { \
        long _e = (long)(expected); \
        long _a = (long)(actual); \
        if (_e != _a) { \
            printf("    FAIL %s:%d: %s == %s (expected %ld, got %ld)\n", \
                   __FILE__, __LINE__, #expected, #actual, _e, _a); \
            assertions_failed++; \
        } \
    } while (0)

#define RUN_TEST(test) \
    do { \
        printf("RUN  %s\n", #test); \
        tests_run++; \
        test(); \
    } while (0)

/** @brief Executes a single opcode directly, bypassing the fetch step. */
static void exec_op(Chip8CPU *cpu, uint16_t opcode)
{
    cpu->opcode = opcode;
    chip8_cpu_execute(cpu);
}

/** @brief Initializes a CPU instance for a test. */
static void init_cpu(Chip8CPU *cpu)
{
    chip8_cpu_hard_reset(cpu);
}

static void test_hard_reset(void)
{
    Chip8CPU cpu;
    init_cpu(&cpu);

    ASSERT_EQ(0x200, cpu.PC);
    ASSERT_EQ(STACK_SIZE, cpu.SP);
    ASSERT_EQ(0, cpu.I);
    ASSERT_EQ(0, cpu.delay_timer);
    ASSERT_EQ(0, cpu.sound_timer);

    /* Fontset loaded at the start of the memory (digit 0 starts with 0xF0) */
    ASSERT_EQ(0xF0, cpu.memory[0]);
    ASSERT_EQ(0x80, cpu.memory[79]);

    /* Rest of the memory cleared */
    ASSERT_EQ(0, cpu.memory[0x200]);
    ASSERT_EQ(0, cpu.memory[MEMORY_SIZE - 1]);
}

static void test_soft_reset(void)
{
    Chip8CPU cpu;
    init_cpu(&cpu);

    cpu.V[3] = 0xAB;
    cpu.display[10] = true;
    cpu.delay_timer = 42;
    cpu.PC = 0x300;

    chip8_cpu_soft_reset(&cpu);

    ASSERT_EQ(0, cpu.V[3]);
    ASSERT_EQ(false, cpu.display[10]);
    ASSERT_EQ(0, cpu.delay_timer);
    ASSERT_EQ(0x200, cpu.PC);
}

static void test_fetch(void)
{
    Chip8CPU cpu;
    init_cpu(&cpu);

    cpu.memory[0x200] = 0xA2;
    cpu.memory[0x201] = 0xF0;

    chip8_cpu_fetch(&cpu);

    ASSERT_EQ(0xA2F0, cpu.opcode);
}

static void test_op_00e0_clear_screen(void)
{
    Chip8CPU cpu;
    init_cpu(&cpu);

    cpu.display[0] = true;
    cpu.display[DISPLAY - 1] = true;

    exec_op(&cpu, 0x00E0);

    ASSERT_EQ(false, cpu.display[0]);
    ASSERT_EQ(false, cpu.display[DISPLAY - 1]);
    ASSERT_EQ(0x202, cpu.PC);
}

static void test_op_1nnn_jump(void)
{
    Chip8CPU cpu;
    init_cpu(&cpu);

    exec_op(&cpu, 0x1ABC);

    ASSERT_EQ(0xABC, cpu.PC);
}

static void test_op_2nnn_call_and_00ee_ret(void)
{
    Chip8CPU cpu;
    init_cpu(&cpu);

    exec_op(&cpu, 0x2ABC);

    ASSERT_EQ(0xABC, cpu.PC);
    ASSERT_EQ(STACK_SIZE - 1, cpu.SP);
    ASSERT_EQ(0x202, cpu.stack[cpu.SP]);

    exec_op(&cpu, 0x00EE);

    ASSERT_EQ(0x202, cpu.PC);
    ASSERT_EQ(STACK_SIZE, cpu.SP);
}

static void test_op_3xnn_skip_if_equal(void)
{
    Chip8CPU cpu;
    init_cpu(&cpu);

    cpu.V[1] = 0x33;
    exec_op(&cpu, 0x3133); /* equal: skip */
    ASSERT_EQ(0x204, cpu.PC);

    exec_op(&cpu, 0x3144); /* not equal: no skip */
    ASSERT_EQ(0x206, cpu.PC);
}

static void test_op_4xnn_skip_if_not_equal(void)
{
    Chip8CPU cpu;
    init_cpu(&cpu);

    cpu.V[1] = 0x33;
    exec_op(&cpu, 0x4144); /* not equal: skip */
    ASSERT_EQ(0x204, cpu.PC);

    exec_op(&cpu, 0x4133); /* equal: no skip */
    ASSERT_EQ(0x206, cpu.PC);
}

static void test_op_5xy0_skip_if_vx_equals_vy(void)
{
    Chip8CPU cpu;
    init_cpu(&cpu);

    cpu.V[1] = 0x33;
    cpu.V[2] = 0x33;
    exec_op(&cpu, 0x5120); /* equal: skip */
    ASSERT_EQ(0x204, cpu.PC);

    cpu.V[2] = 0x44;
    exec_op(&cpu, 0x5120); /* not equal: no skip */
    ASSERT_EQ(0x206, cpu.PC);
}

static void test_op_6xnn_load(void)
{
    Chip8CPU cpu;
    init_cpu(&cpu);

    exec_op(&cpu, 0x6A42);

    ASSERT_EQ(0x42, cpu.V[0xA]);
    ASSERT_EQ(0x202, cpu.PC);
}

static void test_op_7xnn_add(void)
{
    Chip8CPU cpu;
    init_cpu(&cpu);

    cpu.V[2] = 0xF0;
    exec_op(&cpu, 0x7220); /* overflow without carry flag */

    ASSERT_EQ(0x10, cpu.V[2]);
    ASSERT_EQ(0x202, cpu.PC);
}

static void test_op_8xy_logic(void)
{
    Chip8CPU cpu;
    init_cpu(&cpu);

    /* 8XY0: LD */
    cpu.V[1] = 0x00;
    cpu.V[2] = 0xAB;
    exec_op(&cpu, 0x8120);
    ASSERT_EQ(0xAB, cpu.V[1]);

    /* 8XY1: OR */
    cpu.V[1] = 0xF0;
    cpu.V[2] = 0x0F;
    exec_op(&cpu, 0x8121);
    ASSERT_EQ(0xFF, cpu.V[1]);

    /* 8XY2: AND */
    cpu.V[1] = 0xF0;
    cpu.V[2] = 0x3C;
    exec_op(&cpu, 0x8122);
    ASSERT_EQ(0x30, cpu.V[1]);

    /* 8XY3: XOR */
    cpu.V[1] = 0xF0;
    cpu.V[2] = 0xFF;
    exec_op(&cpu, 0x8123);
    ASSERT_EQ(0x0F, cpu.V[1]);
}

static void test_op_8xy4_add_carry(void)
{
    Chip8CPU cpu;
    init_cpu(&cpu);

    cpu.V[1] = 0x10;
    cpu.V[2] = 0x20;
    exec_op(&cpu, 0x8124); /* no carry */
    ASSERT_EQ(0x30, cpu.V[1]);
    ASSERT_EQ(0, cpu.V[0xF]);

    cpu.V[1] = 0xFF;
    cpu.V[2] = 0x02;
    exec_op(&cpu, 0x8124); /* with carry */
    ASSERT_EQ(0x01, cpu.V[1]);
    ASSERT_EQ(1, cpu.V[0xF]);
}

static void test_op_8xy5_sub_borrow(void)
{
    Chip8CPU cpu;
    init_cpu(&cpu);

    cpu.V[1] = 0x20;
    cpu.V[2] = 0x10;
    exec_op(&cpu, 0x8125); /* no borrow */
    ASSERT_EQ(0x10, cpu.V[1]);
    ASSERT_EQ(1, cpu.V[0xF]);

    cpu.V[1] = 0x10;
    cpu.V[2] = 0x20;
    exec_op(&cpu, 0x8125); /* with borrow */
    ASSERT_EQ(0xF0, cpu.V[1]);
    ASSERT_EQ(0, cpu.V[0xF]);
}

static void test_op_8xy6_shift_right(void)
{
    Chip8CPU cpu;
    init_cpu(&cpu);

    /* Original behavior: VX = VY >> 1, VF = LSB of VY */
    cpu.V[2] = 0x05;
    exec_op(&cpu, 0x8126);
    ASSERT_EQ(0x02, cpu.V[1]);
    ASSERT_EQ(1, cpu.V[0xF]);
}

static void test_op_8xy7_subn(void)
{
    Chip8CPU cpu;
    init_cpu(&cpu);

    cpu.V[1] = 0x10;
    cpu.V[2] = 0x30;
    exec_op(&cpu, 0x8127); /* VX = VY - VX, no borrow */
    ASSERT_EQ(0x20, cpu.V[1]);
    ASSERT_EQ(1, cpu.V[0xF]);

    cpu.V[1] = 0x30;
    cpu.V[2] = 0x10;
    exec_op(&cpu, 0x8127); /* with borrow */
    ASSERT_EQ(0xE0, cpu.V[1]);
    ASSERT_EQ(0, cpu.V[0xF]);
}

static void test_op_8xye_shift_left(void)
{
    Chip8CPU cpu;
    init_cpu(&cpu);

    /* Original behavior: VX = VY << 1, VF = MSB of VY */
    cpu.V[2] = 0x81;
    exec_op(&cpu, 0x812E);
    ASSERT_EQ(0x02, cpu.V[1]);
    ASSERT_EQ(1, cpu.V[0xF]);
}

static void test_op_9xy0_skip_if_not_equal(void)
{
    Chip8CPU cpu;
    init_cpu(&cpu);

    cpu.V[1] = 0x01;
    cpu.V[2] = 0x02;
    exec_op(&cpu, 0x9120); /* not equal: skip */
    ASSERT_EQ(0x204, cpu.PC);

    cpu.V[2] = 0x01;
    exec_op(&cpu, 0x9120); /* equal: no skip */
    ASSERT_EQ(0x206, cpu.PC);
}

static void test_op_annn_load_i(void)
{
    Chip8CPU cpu;
    init_cpu(&cpu);

    exec_op(&cpu, 0xA123);

    ASSERT_EQ(0x123, cpu.I);
    ASSERT_EQ(0x202, cpu.PC);
}

static void test_op_bnnn_jump_v0(void)
{
    Chip8CPU cpu;
    init_cpu(&cpu);

    cpu.V[0] = 0x10;
    exec_op(&cpu, 0xB200);

    ASSERT_EQ(0x210, cpu.PC);
}

static void test_op_cxnn_random_masked(void)
{
    Chip8CPU cpu;
    init_cpu(&cpu);

    /* The random result must honor the NN mask */
    exec_op(&cpu, 0xC100);
    ASSERT_EQ(0, cpu.V[1]);

    exec_op(&cpu, 0xC20F);
    ASSERT_TRUE(cpu.V[2] <= 0x0F);
}

static void test_op_dxyn_draw_and_collision(void)
{
    Chip8CPU cpu;
    init_cpu(&cpu);

    cpu.memory[0x300] = 0xFF; /* sprite: single row of 8 pixels */
    cpu.I = 0x300;
    cpu.V[1] = 0; /* x */
    cpu.V[2] = 0; /* y */

    exec_op(&cpu, 0xD121);

    for (int i = 0; i < 8; i++) {
        ASSERT_EQ(true, cpu.display[i]);
    }

    ASSERT_EQ(false, cpu.display[8]);
    ASSERT_EQ(0, cpu.V[0xF]); /* no collision */

    /* Draw again at the same place: collision and pixels erased (XOR) */
    exec_op(&cpu, 0xD121);

    for (int i = 0; i < 8; i++) {
        ASSERT_EQ(false, cpu.display[i]);
    }

    ASSERT_EQ(1, cpu.V[0xF]);
}

static void test_op_ex9e_exa1_key_skip(void)
{
    Chip8CPU cpu;
    init_cpu(&cpu);

    cpu.V[1] = 0x5;

    chip8_cpu_set_key(&cpu, 0x5, true);
    exec_op(&cpu, 0xE19E); /* pressed: skip */
    ASSERT_EQ(0x204, cpu.PC);

    exec_op(&cpu, 0xE1A1); /* pressed: no skip */
    ASSERT_EQ(0x206, cpu.PC);

    chip8_cpu_set_key(&cpu, 0x5, false);
    exec_op(&cpu, 0xE19E); /* released: no skip */
    ASSERT_EQ(0x208, cpu.PC);

    exec_op(&cpu, 0xE1A1); /* released: skip */
    ASSERT_EQ(0x20C, cpu.PC);
}

static void test_op_fx0a_wait_key(void)
{
    Chip8CPU cpu;
    init_cpu(&cpu);

    /* First execution: enters wait mode, PC doesn't advance */
    exec_op(&cpu, 0xF10A);
    ASSERT_EQ(true, cpu.wait_for_key);
    ASSERT_EQ(0x200, cpu.PC);

    /* No key pressed: keeps waiting */
    exec_op(&cpu, 0xF10A);
    ASSERT_EQ(true, cpu.wait_for_key);
    ASSERT_EQ(0x200, cpu.PC);

    /* Key pressed: stored in VX and PC advances */
    chip8_cpu_set_key(&cpu, 0x7, true);
    exec_op(&cpu, 0xF10A);
    ASSERT_EQ(0x7, cpu.V[1]);
    ASSERT_EQ(0x202, cpu.PC);
    ASSERT_EQ(false, cpu.wait_for_key);
}

static void test_op_fx_timers(void)
{
    Chip8CPU cpu;
    init_cpu(&cpu);

    cpu.V[1] = 60;
    exec_op(&cpu, 0xF115); /* delay timer = VX */
    ASSERT_EQ(60, cpu.delay_timer);

    exec_op(&cpu, 0xF207); /* V2 = delay timer */
    ASSERT_EQ(60, cpu.V[2]);

    cpu.V[1] = 30;
    exec_op(&cpu, 0xF118); /* sound timer = VX */
    ASSERT_EQ(30, cpu.sound_timer);
}

static void test_op_fx1e_add_i(void)
{
    Chip8CPU cpu;
    init_cpu(&cpu);

    cpu.I = 0x100;
    cpu.V[1] = 0x10;
    exec_op(&cpu, 0xF11E);
    ASSERT_EQ(0x110, cpu.I);
    ASSERT_EQ(0, cpu.V[0xF]);

    cpu.I = 0xFFF;
    cpu.V[1] = 0x01;
    exec_op(&cpu, 0xF11E); /* I overflow */
    ASSERT_EQ(1, cpu.V[0xF]);
}

static void test_op_fx29_sprite_addr(void)
{
    Chip8CPU cpu;
    init_cpu(&cpu);

    cpu.V[1] = 0xA;
    exec_op(&cpu, 0xF129);

    /* Each fontset digit takes 5 bytes */
    ASSERT_EQ(0xA * 5, cpu.I);
}

static void test_op_fx33_bcd(void)
{
    Chip8CPU cpu;
    init_cpu(&cpu);

    cpu.I = 0x300;
    cpu.V[1] = 254;
    exec_op(&cpu, 0xF133);

    ASSERT_EQ(2, cpu.memory[0x300]);
    ASSERT_EQ(5, cpu.memory[0x301]);
    ASSERT_EQ(4, cpu.memory[0x302]);
}

static void test_op_fx55_fx65_store_fill(void)
{
    Chip8CPU cpu;
    init_cpu(&cpu);

    cpu.I = 0x300;

    for (int i = 0; i <= 3; i++) {
        cpu.V[i] = 0x10 + i;
    }

    exec_op(&cpu, 0xF355); /* store V0-V3 */

    for (int i = 0; i <= 3; i++) {
        ASSERT_EQ(0x10 + i, cpu.memory[0x300 + i]);
    }

    for (int i = 0; i <= 3; i++) {
        cpu.V[i] = 0;
    }

    exec_op(&cpu, 0xF365); /* reload V0-V3 */

    for (int i = 0; i <= 3; i++) {
        ASSERT_EQ(0x10 + i, cpu.V[i]);
    }
}

static void test_timers_update(void)
{
    Chip8CPU cpu;
    init_cpu(&cpu);

    cpu.delay_timer = 2;
    cpu.sound_timer = 2;

    /* Executing instructions must not touch the timers */
    exec_op(&cpu, 0x6000);
    ASSERT_EQ(2, cpu.delay_timer);
    ASSERT_EQ(2, cpu.sound_timer);

    chip8_cpu_update_timers(&cpu);
    ASSERT_EQ(1, cpu.delay_timer);
    ASSERT_EQ(1, cpu.sound_timer);

    chip8_cpu_update_timers(&cpu);
    ASSERT_EQ(0, cpu.delay_timer);
    ASSERT_EQ(0, cpu.sound_timer);

    chip8_cpu_update_timers(&cpu); /* must not decrement below zero */
    ASSERT_EQ(0, cpu.delay_timer);
    ASSERT_EQ(0, cpu.sound_timer);
}

#define TEST_ROM_FILE "build/tests/test_rom_tmp.ch8"

/** @brief Writes a temporary ROM file used by the load tests. */
static bool write_rom(const uint8_t *data, size_t size)
{
    FILE *f = fopen(TEST_ROM_FILE, "wb");

    if (!f) {
        return false;
    }

    fwrite(data, 1, size, f);
    fclose(f);
    return true;
}

static void test_load_rom(void)
{
    Chip8CPU cpu;

    const uint8_t rom[] = {0x60, 0x05, 0x12, 0x00};
    ASSERT_TRUE(write_rom(rom, sizeof(rom)));

    ASSERT_TRUE(chip8_cpu_load(&cpu, TEST_ROM_FILE));

    ASSERT_EQ(0x60, cpu.memory[0x200]);
    ASSERT_EQ(0x05, cpu.memory[0x201]);
    ASSERT_EQ(0x12, cpu.memory[0x202]);
    ASSERT_EQ(0x00, cpu.memory[0x203]);
    ASSERT_EQ(0x200, cpu.PC);

    remove(TEST_ROM_FILE);
}

static void test_load_rom_missing_file(void)
{
    Chip8CPU cpu;
    ASSERT_TRUE(!chip8_cpu_load(&cpu, "build/tests/does_not_exist.ch8"));
}

static void test_load_rom_too_big(void)
{
    Chip8CPU cpu;

    uint8_t rom[MEMORY_SIZE]; /* bigger than MEMORY_SIZE - 512 */
    memset(rom, 0xAA, sizeof(rom));

    ASSERT_TRUE(write_rom(rom, sizeof(rom)));
    ASSERT_TRUE(!chip8_cpu_load(&cpu, TEST_ROM_FILE));

    remove(TEST_ROM_FILE);
}

static void test_system_lifecycle(void)
{
    Chip8System sys;
    chip8_system_init(&sys);

    ASSERT_EQ(false, chip8_system_is_started(&sys));
    ASSERT_EQ(false, chip8_system_is_paused(&sys));

    chip8_system_start(&sys);
    ASSERT_EQ(true, chip8_system_is_started(&sys));

    chip8_system_pause(&sys);
    ASSERT_EQ(true, chip8_system_is_paused(&sys));

    chip8_system_resume(&sys);
    ASSERT_EQ(false, chip8_system_is_paused(&sys));

    chip8_system_stop(&sys);
    ASSERT_EQ(false, chip8_system_is_started(&sys));
}

static void test_system_step_only_when_running(void)
{
    Chip8System sys;
    chip8_system_init(&sys);
    chip8_cpu_hard_reset(&sys.cpu);

    /* Program: LD V0, 0x05 */
    sys.cpu.memory[0x200] = 0x60;
    sys.cpu.memory[0x201] = 0x05;

    /* Stopped: step must not execute */
    chip8_system_step(&sys);
    ASSERT_EQ(0x200, sys.cpu.PC);
    ASSERT_EQ(0, sys.cpu.V[0]);

    /* Running: step executes one cycle */
    chip8_system_start(&sys);
    chip8_system_step(&sys);
    ASSERT_EQ(0x202, sys.cpu.PC);
    ASSERT_EQ(0x05, sys.cpu.V[0]);

    /* Paused: step must not execute */
    chip8_system_pause(&sys);
    chip8_system_step(&sys);
    ASSERT_EQ(0x202, sys.cpu.PC);
}

static void test_system_timers_only_when_running(void)
{
    Chip8System sys;
    chip8_system_init(&sys);
    chip8_cpu_hard_reset(&sys.cpu);

    sys.cpu.delay_timer = 5;

    /* Stopped: timers must not decrement */
    chip8_system_update_timers(&sys);
    ASSERT_EQ(5, sys.cpu.delay_timer);

    /* Running: timers decrement */
    chip8_system_start(&sys);
    chip8_system_update_timers(&sys);
    ASSERT_EQ(4, sys.cpu.delay_timer);

    /* Paused: timers must not decrement */
    chip8_system_pause(&sys);
    chip8_system_update_timers(&sys);
    ASSERT_EQ(4, sys.cpu.delay_timer);
}

static void test_system_key_forwarding(void)
{
    Chip8System sys;
    chip8_system_init(&sys);
    chip8_cpu_hard_reset(&sys.cpu);

    chip8_system_set_key(&sys, 0x5, true);
    ASSERT_EQ(true, sys.cpu.current_key_state[0x5]);

    chip8_system_set_key(&sys, 0x5, false);
    ASSERT_EQ(false, sys.cpu.current_key_state[0x5]);

    ASSERT_TRUE(chip8_system_get_display(&sys) == sys.cpu.display);
}

int main(void)
{
    RUN_TEST(test_hard_reset);
    RUN_TEST(test_soft_reset);
    RUN_TEST(test_fetch);
    RUN_TEST(test_op_00e0_clear_screen);
    RUN_TEST(test_op_1nnn_jump);
    RUN_TEST(test_op_2nnn_call_and_00ee_ret);
    RUN_TEST(test_op_3xnn_skip_if_equal);
    RUN_TEST(test_op_4xnn_skip_if_not_equal);
    RUN_TEST(test_op_5xy0_skip_if_vx_equals_vy);
    RUN_TEST(test_op_6xnn_load);
    RUN_TEST(test_op_7xnn_add);
    RUN_TEST(test_op_8xy_logic);
    RUN_TEST(test_op_8xy4_add_carry);
    RUN_TEST(test_op_8xy5_sub_borrow);
    RUN_TEST(test_op_8xy6_shift_right);
    RUN_TEST(test_op_8xy7_subn);
    RUN_TEST(test_op_8xye_shift_left);
    RUN_TEST(test_op_9xy0_skip_if_not_equal);
    RUN_TEST(test_op_annn_load_i);
    RUN_TEST(test_op_bnnn_jump_v0);
    RUN_TEST(test_op_cxnn_random_masked);
    RUN_TEST(test_op_dxyn_draw_and_collision);
    RUN_TEST(test_op_ex9e_exa1_key_skip);
    RUN_TEST(test_op_fx0a_wait_key);
    RUN_TEST(test_op_fx_timers);
    RUN_TEST(test_op_fx1e_add_i);
    RUN_TEST(test_op_fx29_sprite_addr);
    RUN_TEST(test_op_fx33_bcd);
    RUN_TEST(test_op_fx55_fx65_store_fill);
    RUN_TEST(test_timers_update);
    RUN_TEST(test_load_rom);
    RUN_TEST(test_load_rom_missing_file);
    RUN_TEST(test_load_rom_too_big);
    RUN_TEST(test_system_lifecycle);
    RUN_TEST(test_system_step_only_when_running);
    RUN_TEST(test_system_timers_only_when_running);
    RUN_TEST(test_system_key_forwarding);

    printf("\n%d tests run, %d assertions failed\n", tests_run, assertions_failed);

    return assertions_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
