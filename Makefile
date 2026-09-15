# ============================================================================
#  Frisca OS — Makefile
#  Build a 32-bit x86 freestanding kernel + bootloader
#
#  Usage:
#    make              Build the OS image
#    make run          Run in QEMU
#    make debug        Run in QEMU with GDB stub
#    make clean        Remove build artifacts
#    make help         Show this help
# ============================================================================

# ── Toolchain ────────────────────────────────────────────────────────────────
# Prefer a cross-compiler if available (i686-elf-gcc), fall back to native gcc.
CROSS   ?= i686-elf-
CC      := $(shell command -v $(CROSS)gcc 2>/dev/null || echo gcc)
LD      := $(shell command -v $(CROSS)ld  2>/dev/null || echo ld)
NASM    := nasm

# ── Compiler Flags ───────────────────────────────────────────────────────────
# Freestanding, no stdlib, no PIE/PIC, no stack protector, no builtins.
CFLAGS  := -m32 -std=gnu99 -ffreestanding -O2 -Wall -Wextra \
           -fno-pie -fno-pic -fno-stack-protector -fno-builtin \
           -nostdlib -nostartfiles -I.

NASMFLAGS := -f elf32

# Linker flags — use linker.ld for layout, output raw binary.
# IMPORTANT: `-T linker.ld` must NOT be combined with `-Ttext`.
LDFLAGS := -m elf_i386 -no-pie -T linker.ld --oformat binary

# ── Directories ──────────────────────────────────────────────────────────────
BUILD_DIR := build
SRC_DIR   := kernel

# ── Output Files ─────────────────────────────────────────────────────────────
BOOTLOADER_BIN := $(BUILD_DIR)/bootloader.bin
KERNEL_BIN     := $(BUILD_DIR)/kernel.bin
OS_IMAGE       := $(BUILD_DIR)/frisca-os.bin

# ── Sources ──────────────────────────────────────────────────────────────────
# Assembly sources.
#
# IMPORTANT: Avoid naming an .asm file the same as a .c file
# (e.g., isr.asm and isr.c) — both would produce isr.o and the second
# would overwrite the first. Use distinct names like isr_stub.asm + isr.c.
ASM_SOURCES := \
    $(SRC_DIR)/arch/i386/kernel_entry.asm \
    $(SRC_DIR)/arch/i386/interrupt.asm \
    $(SRC_DIR)/arch/i386/isr_stub.asm \
    $(SRC_DIR)/memory/paging_asm.asm \
    $(SRC_DIR)/task/task_asm.asm

# C sources.
C_SOURCES := \
    $(SRC_DIR)/kernel.c \
    $(SRC_DIR)/shell.c \
    $(SRC_DIR)/arch/i386/idt.c \
    $(SRC_DIR)/arch/i386/isr.c \
    $(SRC_DIR)/drivers/vga/vga.c \
    $(SRC_DIR)/drivers/keyboard/keyboard.c \
    $(SRC_DIR)/drivers/timer/timer.c \
    $(SRC_DIR)/memory/pmm.c \
    $(SRC_DIR)/memory/heap.c \
    $(SRC_DIR)/memory/paging.c \
    $(SRC_DIR)/fs/vfs.c \
    $(SRC_DIR)/task/task.c \
    $(SRC_DIR)/lib/panic.c \
    $(SRC_DIR)/lib/string.c

# ── Objects ──────────────────────────────────────────────────────────────────
# Convert source paths to object paths.
ASM_OBJS := $(patsubst $(SRC_DIR)/%.asm,$(BUILD_DIR)/%.o,$(ASM_SOURCES))
C_OBJS   := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(C_SOURCES))

# Combine all objects. kernel_entry.o goes first so `_start` is the first
# symbol in the output binary.
OBJS := \
    $(BUILD_DIR)/arch/i386/kernel_entry.o \
    $(filter-out $(BUILD_DIR)/arch/i386/kernel_entry.o, $(ASM_OBJS) $(C_OBJS))

# ── Header Dependency Tracking ───────────────────────────────────────────────
# -MMD -MP generates .d files; we include them at the end so Make knows
# to rebuild when headers change.
DEPS := $(OBJS:.o=.d)

# ── Default Target ───────────────────────────────────────────────────────────
.PHONY: all
all: $(OS_IMAGE)

# ── Bootloader ───────────────────────────────────────────────────────────────
# The bootloader is a raw 16-bit binary, not linked with the kernel.
# We create the output directory inside the rule — no order-only
# prerequisite needed.
$(BOOTLOADER_BIN): boot/bootloader.asm
	@mkdir -p $(dir $@)
	$(NASM) -f bin $< -o $@

# ── Assembly → Object ────────────────────────────────────────────────────────
# Pattern rule: any .asm under $(SRC_DIR) becomes .o under $(BUILD_DIR).
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.asm
	@mkdir -p $(dir $@)
	$(NASM) $(NASMFLAGS) $< -o $@

# ── C → Object ───────────────────────────────────────────────────────────────
# -MMD -MP generates .d files for header dependency tracking.
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# ── Link Kernel ──────────────────────────────────────────────────────────────
# Uses linker.ld for layout. Output is a raw binary.
$(KERNEL_BIN): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) $(OBJS) -o $@
	@echo "  [OK] Linked $@ ($$(stat -c%s $@ 2>/dev/null || stat -f%z $@) bytes)"

# ── Combine Bootloader + Kernel into OS Image ────────────────────────────────
# The bootloader loads 50 sectors starting at sector 2; the kernel must fit.
$(OS_IMAGE): $(BOOTLOADER_BIN) $(KERNEL_BIN)
	cat $(BOOTLOADER_BIN) $(KERNEL_BIN) > $@
	@echo "  [OK] Built $@ ($$(stat -c%s $@ 2>/dev/null || stat -f%z $@) bytes)"

# ── Run in QEMU ──────────────────────────────────────────────────────────────
.PHONY: run
run: $(OS_IMAGE)
	qemu-system-i386 -drive format=raw,file=$(OS_IMAGE),index=0,if=floppy

# ── Run with GDB Stub ────────────────────────────────────────────────────────
# QEMU waits for a GDB connection on localhost:1234.
# Connect with:  gdb -ex "target remote :1234"
.PHONY: debug
debug: $(OS_IMAGE)
	qemu-system-i386 -drive format=raw,file=$(OS_IMAGE),index=0,if=floppy -s -S

# ── Run with QEMU Monitor on stdio ───────────────────────────────────────────
# Press Ctrl+A C to switch to the QEMU monitor.
.PHONY: run-monitor
run-monitor: $(OS_IMAGE)
	qemu-system-i386 -drive format=raw,file=$(OS_IMAGE),index=0,if=floppy -monitor stdio

# ── Clean ────────────────────────────────────────────────────────────────────
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

# ── Rebuild from Scratch ─────────────────────────────────────────────────────
.PHONY: rebuild
rebuild: clean all

# ── Include Header Dependencies ──────────────────────────────────────────────
# The `-` prefix means "don't error if the .d files don't exist yet".
-include $(DEPS)

# ── Help ─────────────────────────────────────────────────────────────────────
.PHONY: help
help:
	@echo "Frisca OS — Build targets:"
	@echo "  make             Build the OS image ($(OS_IMAGE))"
	@echo "  make run         Run in QEMU"
	@echo "  make debug       Run in QEMU with GDB stub (-s -S)"
	@echo "  make run-monitor Run in QEMU with monitor on stdio"
	@echo "  make clean       Remove build artifacts"
	@echo "  make rebuild     Clean + build"
	@echo "  make help        Show this help"