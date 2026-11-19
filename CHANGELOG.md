# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [0.2.0] - 2026-11-19

### Added

- C implementation of the emulator core, independent of any graphics toolkit.
- SDL2 command-line frontend as `chip8-emulator`.
- GTK4 graphical frontend as `chip8-emulator-gtk`, with:
  - File, Emulation, View, and Help menus.
  - Native ROM file chooser and command-line ROM argument.
  - Run/pause, soft reset, and hard reset actions.
  - Window scaling options.
  - Five color themes.
  - Keyboard accelerators and the CHIP-8 keypad mapping.
  - About dialog with the `space.png` logo and `icon.png` window icon.
  - Flat, square menubar styling.
- C implementation of the CHIP-8 disassembler as `chip8-disassembler`.
- Root Makefile with emulator, GTK4 frontend, disassembler, test, and clean
  targets.
- Unit test suite for the CPU, opcodes, timers, ROM loading, system lifecycle,
  keypad forwarding, and phosphor persistence.
- GitHub Actions workflow for building and testing on Ubuntu.
- CRT-style phosphor persistence to prevent XOR sprite flicker without
  excessive trails.
- CHIP-8 instruction-set references in the README and Doxygen documentation.
- Separate compilation instructions for the SDL2 and GTK4 frontends.

### Changed

- Converted all C++ sources to C99.
- Renamed the legacy emulator executable to `chip8-emulator`.
- Replaced the Qt4 dependency and GUI with independent SDL2 and GTK4
  frontends.
- Decoupled emulation state and lifecycle control into `chip8_system`.
- Runs the CPU at 500 instructions per second.
- Runs delay and sound timers at approximately 60 Hz, independently of CPU
  speed.
- Moved phosphor persistence into a shared module used by both frontends.
- Formatted C sources according to consistent C conventions.
- Converted code documentation to Doxygen comments and translated comments
  to English.
- Renamed the `image` directory to `assets` and updated code and README
  references.
- Increased the About-dialog logo by approximately 20%.
- Updated the README with GTK4/SDL2 build, run, keyboard, test, and
  documentation instructions.

### Removed

- Qt4 project files, widgets, and C++ implementation files.
- Dependency on Qt for emulation, display, keyboard handling, menus, and
  file selection.

