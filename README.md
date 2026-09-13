# Frisca OS

> A minimal 32-bit x86 hobby operating system written in C and Assembly.

![Status](https://img.shields.io/badge/status-active-brightgreen)
![Version](https://img.shields.io/badge/version-v0.1.0-blue)
![Architecture](https://img.shields.io/badge/arch-i386-orange)
![Language](https://img.shields.io/badge/language-C%20%7C%20Assembly-red)
![License](https://img.shields.io/badge/license-MIT-lightgrey)

Frisca OS is a small, educational operating system built from scratch for the **i386 (32-bit x86)** architecture. It boots via a custom bootloader, runs in **protected mode**, and provides a minimal shell with memory management, a timer, keyboard input, and a RAM-based virtual filesystem.

---

## ✨ Features

| Subsystem | Description | Status |
|-----------|-------------|--------|
| **Bootloader** | Custom 16-bit → 32-bit protected mode switch | ✅ |
| **VGA Driver** | 80×25 text mode, colors, scrolling, cursor | ✅ |
| **GDT** | Global Descriptor Table (flat memory model) | ✅ |
| **IDT** | Interrupt Descriptor Table with 256 gates | ✅ |
| **PIC** | 8259 PIC remapped to IRQ vectors 0x20–0x2F | ✅ |
| **PIT Timer** | Programmable Interval Timer at 100 Hz | ✅ |
| **Keyboard** | PS/2 scancode set 1, with Shift support | ✅ |
| **PMM** | Physical Memory Manager (bitmap-based) | ✅ |
| **Heap** | `kmalloc` / `kfree` with splitting & coalescing | ✅ |
| **Paging** | Identity-mapped paging for the first 16 MB | ✅ |
| **VFS** | RAM-based virtual file system | ✅ |
| **Shell** | Interactive command shell | ✅ |

---

## 📸 Screenshot

```
=== Welcome to Frisca OS Shell ===
frisca-os> help
Available commands:
  help      - Show this help message
  version   - Display Frisca OS version
  timer     - Show system uptime in seconds and ticks
  testdelay - Test 2 seconds sleep delay
  testmem   - Test Physical Memory Manager (PMM)
  testheap  - Test Heap Allocator (kmalloc/kfree)
  ls        - List all files in VFS RAMDisk
  cat       - Display file content (Usage: cat <filename>)
  write     - Write to file (Usage: write <filename> <content>)
  clear     - Clear the screen
  reboot    - Restart the system

frisca-os> version
Frisca OS v0.1.0 (32-bit Protected Mode)

frisca-os> timer
System Uptime: 42 s (Total Ticks: 4200)

frisca-os> ls
Name                             Size (Bytes)
---------------------------------------------
readme.txt                       42
hello.c                          128

frisca-os> _
```

---

## 🏗️ Project Structure

```
frisca-os/
├── arch/
│   └── i386/
│       ├── boot.asm           # Bootloader & protected mode switch
│       ├── idt.asm            # ISR/IRQ assembly stubs
│       ├── idt.c              # IDT setup + PIC remap
│       ├── idt.h
│       ├── io.h               # inb / outb helpers
│       └── gdt.asm            # GDT setup
│
├── drivers/
│   ├── vga/
│   │   ├── vga.c              # VGA text mode driver
│   │   └── vga.h
│   ├── timer/
│   │   ├── timer.c            # PIT driver + sleep()
│   │   └── timer.h
│   └── keyboard/
│       ├── keyboard.c         # PS/2 keyboard driver
│       └── keyboard.h
│
├── memory/
│   ├── pmm.c                  # Physical Memory Manager
│   ├── pmm.h
│   ├── heap.c                 # kmalloc / kfree
│   ├── heap.h
│   ├── paging.c               # Paging setup
│   └── paging.h
│
├── fs/
│   ├── vfs.c                  # Virtual File System
│   └── vfs.h
│
├── lib/
│   ├── string.c               # strlen, strcmp, memcpy, ...
│   ├── string.h
│   ├── stdint.h               # Fixed-width integer types
│   └── stddef.h               # NULL, size_t, ptrdiff_t
│
├── shell/
│   ├── shell.c                # Interactive shell
│   └── shell.h
│
├── kernel.c                   # kernel_main() entry point
├── linker.ld                  # Linker script
├── Makefile
└── README.md
```

---

## 🚀 Getting Started

### Prerequisites

You need a cross-compiler or a native toolchain capable of producing 32-bit x86 freestanding binaries:

- **GCC** with `-m32` support (`gcc-multilib` on Debian/Ubuntu)
- **NASM** (Netwide Assembler)
- **GNU Make**
- **QEMU** (for testing)

**On Debian/Ubuntu:**
```bash
sudo apt install build-essential nasm qemu-system-x86 gcc-multilib
```

**On Arch Linux:**
```bash
sudo pacman -S base-devel nasm qemu-system-x86
```

**On macOS (using Homebrew + cross-compiler):**
```bash
brew install nasm qemu
brew install i386-elf-gcc   # or build from source
```

### Building

```bash
# Clone the repository
git clone https://github.com/Deheryanto/FriscaOS.git
cd FriscaOS

# Build the kernel
make

# Run in QEMU
make run
```

The `make run` target boots the kernel in QEMU with 16 MB of RAM:

```bash
qemu-system-i386 -m 16 -kernel frisca-os.bin
```

### Cleaning

```bash
make clean      # Remove build artifacts
```

---

## 🧠 Architecture Overview

### Boot Sequence

```
BIOS / QEMU
    │
    ▼
boot.asm (16-bit real mode)
    ├─ Set up segment registers
    ├─ Load GDT
    ├─ Enable protected mode (CR0.PE)
    ├─ Far jump to 32-bit code
    ├─ Set up stack (ESP)
    └─► call kernel_main
            │
            ▼
        kernel_main() (32-bit C)
            ├─ vga_init()       → Screen ready
            ├─ idt_init()       → Interrupts enabled
            ├─ timer_init(100)  → IRQ0 at 100 Hz
            ├─ pmm_init(16 MB)  → Physical memory bitmap
            ├─ heap_init()      → kmalloc / kfree
            ├─ paging_init()    → MMU enabled
            ├─ vfs_init()       → RAM filesystem
            └─ shell_init()     → Interactive prompt
                    │
                    ▼
              while(1) hlt   → Idle, wake on interrupt
```

### Memory Layout

```
Physical Address        Usage
──────────────────      ──────────────────────────────
0x00000000 – 0x000003FF  Interrupt Vector Table (IVT)
0x00000400 – 0x000004FF  BIOS Data Area (BDA)
0x00000500 – 0x00007BFF  Free conventional memory
0x00007C00 – 0x00007DFF  Bootloader (loaded by BIOS)
0x00007E00 – 0x0009FBFF  Free
0x000A0000 – 0x000BFFFF  VGA memory (text mode at 0xB8000)
0x000C0000 – 0x000FFFFF  BIOS ROM, option ROMs
0x00100000 – ...          Kernel + heap (identity-mapped)
0x01000000 (16 MB)        End of managed RAM
```

### Address Translation (Paging)

```
Virtual Address (32 bits)
┌───────────┬───────────┬──────────────┐
│ PDE index │ PTE index │ Page offset  │
│ 10 bits   │ 10 bits   │  12 bits     │
└───────────┴───────────┴──────────────┘
     │           │             │
     │           │             └─ 0..4095 (within 4 KB page)
     │           └─ 0..1023 (entry in page table)
     └─ 0..1023 (entry in page directory)

CR3 → Page Directory → Page Table → Physical Page
```

Currently uses **identity mapping**: virtual address == physical address for 0–16 MB. This lets the kernel keep running at the same addresses after enabling paging.

---

## 💻 Shell Commands

| Command | Description |
|---------|-------------|
| `help` | Show available commands |
| `version` | Display kernel version |
| `timer` | Show system uptime (seconds + ticks) |
| `testdelay` | Sleep for 2 seconds (tests timer) |
| `testmem` | Allocate & free a PMM block |
| `testheap` | Allocate & free a heap block |
| `ls` | List all files in the VFS |
| `cat <file>` | Display file content |
| `write <file> <content>` | Create a new file |
| `clear` | Clear the screen |
| `reboot` | Restart the system (via 8042) |

**Examples:**
```
frisca-os> write notes.txt Hello, Frisca OS!
File created successfully!

frisca-os> ls
Name                             Size (Bytes)
---------------------------------------------
notes.txt                        18

frisca-os> cat notes.txt
Hello, Frisca OS!

frisca-os> timer
System Uptime: 137 s (Total Ticks: 13700)
```

---

## 🔧 Building Details

### Compiler Flags

The kernel is compiled with:

```makefile
CFLAGS = -m32 -std=gnu99 -ffreestanding -fno-stack-protector \
         -fno-pic -fno-builtin -Wall -Wextra -O2 \
         -I. -nostdlib -nostartfiles
```

| Flag | Purpose |
|------|---------|
| `-m32` | Emit 32-bit x86 code |
| `-ffreestanding` | No assumptions about standard library |
| `-fno-stack-protector` | No stack canary (no `__stack_chk_fail`) |
| `-fno-pic` | No position-independent code (fixed addresses) |
| `-fno-builtin` | Don't replace our functions with compiler builtins |
| `-nostdlib` | Don't link against libc |
| `-O2` | Reasonable optimization without breaking `volatile` |

### Linker Script

The kernel is linked at **1 MB** with a flat layout:

```ld
ENTRY(_start)

SECTIONS {
    . = 1M;

    .text : { *(.text) }
    .rodata : { *(.rodata) }
    .data : { *(.data) }
    .bss : { *(.bss) }
}
```

---

## 🧪 Testing

### Running in QEMU

```bash
make run
```

QEMU is invoked with:
```bash
qemu-system-i386 -m 16 -kernel frisca-os.bin
```

- `-m 16`: 16 MB RAM (matches `pmm_init` argument)
- `-kernel`: QEMU loads the binary as a multiboot-compatible kernel

### Manual Testing

Try these commands after boot:

```
help
version
timer
testmem
testheap
write hello.txt Hello world
ls
cat hello.txt
testdelay
clear
reboot
```

---

## 🛠️ Development

### Adding a New Command

1. Add the handler in `shell.c`:
   ```c
   } else if (strcmp(command_buffer, "mycmd") == 0) {
       vga_write("Hello from mycmd!\n");
   }
   ```
2. Add it to the `help` output.
3. Rebuild with `make run`.

### Adding a New Driver

1. Create `drivers/mydriver/mydriver.c` and `mydriver.h`.
2. Register an IRQ handler in `idt.c` if needed.
3. Call `mydriver_init()` from `kernel_main`.
4. Add the object file to the `Makefile`.

### Adding a New Syscall/Feature

1. Design the interface in a header.
2. Implement the `.c` file.
3. Call the init from `kernel_main` in the correct order.
4. Test via the shell.

---

## 🗺️ Roadmap

### v0.1.0 (Current)
- [x] Bootloader + protected mode
- [x] VGA text mode driver
- [x] GDT + IDT + PIC
- [x] PIT timer
- [x] PS/2 keyboard
- [x] Physical Memory Manager
- [x] Heap allocator
- [x] Paging (identity map)
- [x] VFS (RAM-based)
- [x] Interactive shell

### v0.2.0 (Planned)
- [ ] Page fault handler with diagnostics
- [ ] `kernel_panic()` function
- [ ] CPU exception handlers (0–31)
- [ ] `print_hex` and `kprintf`
- [ ] `mem` command in help
- [ ] File deletion (`rm`)
- [ ] `krealloc`, `kcalloc`, `kstrdup`

### v0.3.0 (Future)
- [ ] Multiboot header (real GRUB support)
- [ ] E820 memory map parsing
- [ ] ACPI shutdown
- [ ] Higher-half kernel (0xC0000000+)
- [ ] User mode (ring 3)
- [ ] System calls (`int 0x80`)
- [ ] Task scheduler
- [ ] ATA / disk driver
- [ ] Real on-disk filesystem

### Long-term
- [ ] SMP support (APIC)
- [ ] Networking (e1000, TCP/IP stack)
- [ ] GUI / windowing
- [ ] POSIX-like API

---

## 📚 References & Learning Resources

This project was built following classic OS development resources:

- **[OSDev Wiki](https://wiki.osdev.org/)** — The canonical reference for hobby OS development
- **[Intel SDM](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)** — Intel Software Developer's Manual
- **[JamesM's Kernel Tutorial](https://web.archive.org/web/20200508042725/http://www.jamesmolloy.co.uk/tutorial_html/)** — Classic kernel dev tutorial
- **[Broken Thorn Tutorial](http://www.brokenthorn.com/Resources/OSDevIndex.html)** — Detailed x86 OS tutorial
- **[Writing an OS in Rust](https://os.phil-opp.com/)** — Excellent modern reference (Rust, but concepts transfer)

---

## 🤝 Contributing

Contributions are welcome! If you'd like to improve Frisca OS:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

Please make sure your code:
- Compiles without warnings (`-Wall -Wextra`)
- Follows the existing code style (4-space indent, snake_case)
- Includes English comments for non-obvious logic
- Is tested in QEMU before submitting

---

## 📝 License

This project is licensed under the **MIT License** — see the [LICENSE](LICENSE) file for details.

```
MIT License

Copyright (c) 2025 Frisca OS Contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## 👤 Author

**Your Name**
- GitHub: [@yourusername](https://github.com/Deheryanto)
- Email: deheryantocorp@gmail.com

---

## ⭐ Acknowledgments

- Thanks to the **[OSDev community](https://forum.osdev.org/)** for their invaluable wiki and forum
- Inspired by countless hobby OS projects: **SerenityOS**, **ToaruOS**, **osdev tutorials**
- Built with ❤️ and a lot of `hlt` instructions

---

<div align="center">

**⭐ If you find this project useful, consider giving it a star! ⭐**

Made with ❤️ for the OS development community

</div>
