@echo off
echo Compiling Bootloader...
nasm -f bin boot.asm -o boot.bin

echo Compiling Kernel...
nasm -f bin kernel.asm -o kernel.bin

echo Linking OS Image...
copy /b boot.bin + kernel.bin os.img > nul

echo Build Complete: os.img
