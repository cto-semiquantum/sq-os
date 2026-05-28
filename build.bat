@echo off
setlocal

set GCC_PATH=C:\Users\Harsh\i686-elf-tools\bin
set PATH=%GCC_PATH%;%PATH%

echo =============================================
echo Building SQ-OS Hybrid Kernel
echo =============================================

echo 1. Compiling Bootloader (boot/boot.asm)
nasm -f bin boot\boot.asm -o boot.bin
if %ERRORLEVEL% neq 0 (
    echo Error compiling bootloader.
    exit /b %ERRORLEVEL%
)

echo 2. Compiling Entry Point (kernel/entry.asm)
nasm -f elf32 kernel\entry.asm -o entry.o
if %ERRORLEVEL% neq 0 (
    echo Error compiling entry.asm.
    exit /b %ERRORLEVEL%
)

echo 3. Compiling C Subsystems
echo    - kernel.c
i686-elf-gcc -c -m32 -ffreestanding -fno-pie -fno-stack-protector -O2 -Iinclude kernel\kernel.c -o kernel.o
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo    - graphics.c
i686-elf-gcc -c -m32 -ffreestanding -fno-pie -fno-stack-protector -O2 -Iinclude kernel\graphics.c -o graphics.o
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo    - window_manager.c
i686-elf-gcc -c -m32 -ffreestanding -fno-pie -fno-stack-protector -O2 -Iinclude kernel\window_manager.c -o window_manager.o
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo    - mouse.c
i686-elf-gcc -c -m32 -ffreestanding -fno-pie -fno-stack-protector -O2 -Iinclude kernel\mouse.c -o mouse.o
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo    - terminal_app.c
i686-elf-gcc -c -m32 -ffreestanding -fno-pie -fno-stack-protector -O2 -Iinclude kernel\terminal_app.c -o terminal_app.o
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo    - desktop.c
i686-elf-gcc -c -m32 -ffreestanding -fno-pie -fno-stack-protector -O2 -Iinclude kernel\desktop.c -o desktop.o
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo    - rtc.c
i686-elf-gcc -c -m32 -ffreestanding -fno-pie -fno-stack-protector -O2 -Iinclude kernel\rtc.c -o rtc.o
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo 4. Linking Kernel Binaries (linker.ld)
i686-elf-ld -m elf_i386 -T linker.ld -o kernel.bin entry.o kernel.o graphics.o window_manager.o mouse.o terminal_app.o desktop.o rtc.o
if %ERRORLEVEL% neq 0 (
    echo Linker Error.
    exit /b %ERRORLEVEL%
)

echo 5. Assembling os.img
copy /b boot.bin + kernel.bin os.img > null
del null

echo 6. Padding Disk Image to Match Disk Geometry (51 sectors)
powershell -Command "$bytes = [System.IO.File]::ReadAllBytes('os.img'); $target = 512 * 51; if ($bytes.Length -lt $target) { $padded = New-Object byte[] $target; [System.Array]::Copy($bytes, $padded, $bytes.Length); [System.IO.File]::WriteAllBytes('os.img', $padded); echo 'Padded successfully.' } else { echo 'No padding needed.' }"

echo =============================================
echo Build Successful: os.img
echo =============================================
endlocal
