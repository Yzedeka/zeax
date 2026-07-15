# Zeax OS
Version: 1.2.3
[Zeax.org](https://zeax.org/)

> A Open Source Toy operating system built from scratch in C and x86 Assembly.

## About

Zeax OS is a Toy operating system project created to learn how computers work at the lowest level.
The long-term goal is to build a lightweight, modern, and developer friendly operating system with its own kernel, applications and package format

This project is currently in early development.

## Features

- Custom x86 kernel
- Written in C and Assembly
- Multiboot compatible (GRUB)
- Built with GCC and NASM
- Bootable ISO generation
- Runs in QEMU and VirtualBox (Tested on physical hardware, but currently does not boot successfully)

## Planned Features

- Memory management
- Process scheduler
- Virtual file system
- USB support
- Networking
- Audio
- Window manager
- Package manager
- Zeax Executables (`.zxe`)
- Git support
- Hardware Support

## Building

Requirements:

- GCC
- NASM
- GRUB
- xorriso
- make

Clone the repository:

```bash
git clone https://github.com/Yzedeka/Zeax.git
cd Zeax
```

Build:

```bash
make
```

The build will generate a bootable ISO.

## Running

Using QEMU:

```bash
qemu-system-x86_64 -cdrom zeax.iso
```

## Project Structure

```
boot/       Bootloader
kernel/     Kernel source
iso/        ISO filesystem
Makefile    Build system
linker.ld   Linker script
```

## Communication and Support
- Discord Server: [dsc.gg/uKAavXeZcy](https://discord.gg/uKAavXeZcy)
- Email: [hi@yzedeka.org](mailto:hi@yzedeka.org)

## License

This project is licensed under the GNU General Public License v2.0 (GPL-2.0). See the `LICENSE` file for details.
