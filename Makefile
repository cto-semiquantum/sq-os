build:
	nasm -f bin boot.asm -o boot.bin

run: build
	qemu-system-x86_64 boot.bin