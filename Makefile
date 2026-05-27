# Makefile for SQ-OS

.PHONY: all clean run

all: os.img

boot.bin: boot.asm
	nasm -f bin boot.asm -o boot.bin

kernel.bin: kernel.asm graphics.asm shell.asm keyboard.asm fonts.asm
	nasm -f bin kernel.asm -o kernel.bin

os.img: boot.bin kernel.bin
	# Concatenate bootloader and kernel into a single raw disk image
	python -c "open('os.img', 'wb').write(open('boot.bin', 'rb').read() + open('kernel.bin', 'rb').read())"

run: os.img
	qemu-system-x86_64 -drive format=raw,file=os.img

clean:
	-del /f /q boot.bin kernel.bin os.img 2>nul || rm -f boot.bin kernel.bin os.img