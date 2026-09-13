# ============================================================================
#  Frisca OS — Makefile
#  Build a 32-bit x86 freestanding kernel + bootloader
# ============================================================================

# ── Toolchain ────────────────────────────────────────────────────────────────
# Prefer a cross-compiler if available, fall back to native gcc.
# On macOS/ARM hosts, `gcc -m32` may not work — use i686-elf-gcc instead.
CROSS   ?= i686-elf-
CC      := $(shell command -v $(CROSS)gcc 2>/dev/null || echo gcc)
LD      := $(shell command -v $(CROSS)ld  2>/dev/null || echo ld)
NASM    := nasm

# ── Flags ────────────────────────────────────────────────────────────────────
# Freestanding, no stdlib, no PIE/PIC, no stack protector, no builtins.
CFLAGS  := -m32 -std=gnu99 -ffreestanding -O2 -Wall -Wextra \
           -fno-pie -fno-pic -fno-stack-protector -fno-builtin \
           -nostdlib -nostartfiles -I.

NASMFLAGS := -f elf32

LDFLAGS   := -m elf_i386 -no-pie -Ttext 0x1000 --oformat binary

# ── Directories ──────────────────────────────────────────────────────────────
BUILD_DIR   := build
SRC_DIR     := kernel
INCLUDE_DIR := $(SRC_DIR)

# ── Output ───────────────────────────────────────────────────────────────────
BOOTLOADER_BIN := $(BUILD_DIR)/bootloader.bin
KERNEL_BIN     := $(BUILD_DIR)/kernel.bin
OS_IMAGE       := $(BUILD_DIR)/frisca-os.bin

# ── Sources ──────────────────────────────────────────────────────────────────
# Assembly sources (order matters for entry point)
ASM_SOURCES := \
    $(SRC_DIR)/arch/i386/kernel_entry.asm \
    $(SRC_DIR)/arch/i386/interrupt.asm \
    $(SRC_DIR)/memory/paging_asm.asm

# C sources
C_SOURCES := \
    $(SRC_DIR)/kernel.c \
    $(SRC_DIR)/shell.c \
    $(SRC_DIR)/arch/i386/idt.c \
    $(SRC_DIR)/drivers/vga/vga.c \
    $(SRC_DIR)/drivers/keyboard/keyboard.c \
    $(SRC_DIR)/drivers/timer/timer.c \
    $(SRC_DIR)/memory/pmm.c \
    $(SRC_DIR)/memory/heap.c \
    $(SRC_DIR)/memory/paging.c \
    $(SRC_DIR)/fs/vfs.c \
    $(SRC_DIR)/lib/string.c

# ── Objects ──────────────────────────────────────────────────────────────────
# Keep kernel_entry.o and interrupt.o first (entry + ISR stubs).
ASM_OBJS := $(patsubst $(SRC_DIR)/%.asm,$(BUILD_DIR)/%.o,$(ASM_SOURCES))
C_OBJS   := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(C_SOURCES))

OBJS := \
    $(BUILD_DIR)/arch/i386/kernel_entry.o \
    $(BUILD_DIR)/arch/i386/interrupt.o \
    $(BUILD_DIR)/memory/paging_asm.o \
    $(filter-out $(BUILD_DIR)/arch/i386/kernel_entry.o \
                  $(BUILD_DIR)/arch/i386/interrupt.o \
                  $(BUILD_DIR)/memory/paging_asm.o, $(ASM_OBJS) $(C_OBJS))

# ── Header dependencies ──────────────────────────────────────────────────────
# Auto-generate .d files so header changes trigger rebuilds.
DEPS := $(OBJS:.o=.d)

# ── Default target ───────────────────────────────────────────────────────────
.PHONY: all
all: $(OS_IMAGE)

# ── Directory creation ───────────────────────────────────────────────────────
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Ensure subdirectories exist for nested object paths
$(BUILD_DIR)/arch/i386 $(BUILD_DIR)/drivers/vga \
$(BUILD_DIR)/drivers/keyboard $(BUILD_DIR)/drivers/timer \
$(BUILD_DIR)/memory $(BUILD_DIR)/fs $(BUILD_DIR)/lib:
	mkdir -p $@

# ── Bootloader (raw binary, 16-bit real mode) ────────────────────────────────
$(BOOTLOADER_BIN): boot/bootloader.asm | $(BUILD_DIR)
	$(NASM) -f bin $< -o $@

# ── Assembly → Object ────────────────────────────────────────────────────────
# Pattern rule handles all .asm files.
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.asm
	@mkdir -p $(dir $@)
	$(NASM) $(NASMFLAGS) $< -o $@

# ── C → Object ───────────────────────────────────────────────────────────────
# -MMD -MP generates .d dependency files so header changes trigger rebuilds.
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# ── Link kernel ──────────────────────────────────────────────────────────────
$(KERNEL_BIN): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $@

# ── Combine bootloader + kernel into single floppy image ─────────────────────
$(OS_IMAGE): $(BOOTLOADER_BIN) $(KERNEL_BIN)
	cat $(BOOTLOADER_BIN) $(KERNEL_BIN) > $@
	@echo "  [OK] Built $@ ($$(stat -c%s $@ 2>/dev/null || stat -f%z $@) bytes)"

# ── Run in QEMU ──────────────────────────────────────────────────────────────
.PHONY: run
run: $(OS_IMAGE)
	qemu-system-i386 -drive format=raw,file=$(OS_IMAGE),index=0,if=floppy

# ── Run with GDB debugging (connect with: gdb -ex "target remote :1234") ─────
.PHONY: debug
debug: $(OS_IMAGE)
	qemu-system-i386 -drive format=raw,file=$(OS_IMAGE),index=0,if=floppy -s -S

# ── Run with monitor on stdio (Ctrl+A C to switch to QEMU monitor) ───────────
.PHONY: run-monitor
run-monitor: $(OS_IMAGE)
	qemu-system-i386 -drive format=raw,file=$(OS_IMAGE),index=0,if=floppy -monitor stdio

# ── Clean ────────────────────────────────────────────────────────────────────
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

# ── Rebuild from scratch ─────────────────────────────────────────────────────
.PHONY: rebuild
rebuild: clean all

# ── Include auto-generated header dependencies ───────────────────────────────
-include $(DEPS)

# ── Help ─────────────────────────────────────────────────────────────────────
.PHONY: help
help:
	@echo "Frisca OS — Build targets:"
	@echo "  make            Build the OS image ($(OS_IMAGE))"
	@echo "  make run        Run in QEMU"
	@echo "  make debug      Run in QEMU with GDB stub (-s -S)"
	@echo "  make run-monitor Run in QEMU with monitor on stdio"
	@echo "  make clean      Remove build artifacts"
	@echo "  make rebuild    Clean + build"
	@echo "  make help       Show this help"