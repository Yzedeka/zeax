CC=gcc
LD=i686-elf-ld
AS=nasm

CFLAGS=-m32 -ffreestanding -O2 -Wall -Wextra \
-fno-stack-protector \
-mno-mmx -mno-sse -mno-sse2 -mno-sse3 -mno-3dnow -mno-avx

all: zeax.iso

boot.o:
	$(AS) boot/boot.asm -f elf32 -o boot.o

kernel.o:
	$(CC) $(CFLAGS) -c kernel/kernel.c -o kernel.o

zeax.bin: boot.o kernel.o
	$(LD) -T linker.ld -m elf_i386 boot.o kernel.o -o zeax.bin

zeax.iso: zeax.bin
	mkdir -p iso/boot/grub
	cp zeax.bin iso/boot/
	cp grub.cfg iso/boot/grub/

	grub-mkrescue -o zeax.iso iso

clean:
	rm -rf *.o *.bin iso zeax.iso
