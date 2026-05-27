@echo off
echo Compiling Bootloader...
nasm -f bin boot.asm -o boot.bin

echo Compiling Kernel...
nasm -f bin kernel.asm -o kernel.bin

echo Linking OS Image...
python -c "open('os.img', 'wb').write(open('boot.bin', 'rb').read() + open('kernel.bin', 'rb').read())"

echo Build Complete: os.img
