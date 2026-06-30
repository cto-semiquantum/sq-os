@echo off
setlocal

set GCC_PATH=C:\Users\Harsh\i686-elf-tools\bin
set PATH=%GCC_PATH%;%PATH%

set CFLAGS=-c -m32 -ffreestanding -fno-pie -fno-stack-protector -O2 -Iinclude -Idrivers -Ifs -Ikernel

echo =============================================
echo  Building SQ-OS Hybrid Kernel  v3.0
echo =============================================

:: --------------------------------------------------
:: 0. Regenerate wallpaper pixel data
:: --------------------------------------------------
echo 0. Generating wallpaper data...
python assets\gen_wallpaper.py
if %ERRORLEVEL% neq 0 (
    echo Warning: wallpaper generation failed, using cached data.
)

:: --------------------------------------------------
:: 1. Bootloader
:: --------------------------------------------------
echo 1. Compiling Bootloader (boot/boot.asm)
nasm -f bin boot\boot.asm -o boot.bin
if %ERRORLEVEL% neq 0 (
    echo Error compiling bootloader.
    exit /b %ERRORLEVEL%
)


echo 1c. Assembling apps/crash.asm (Ring 3 crash test)
nasm -f bin apps\crash.asm -o crash.bin
if %ERRORLEVEL% neq 0 (
    echo Error assembling crash.asm.
    exit /b %ERRORLEVEL%
)

:: --------------------------------------------------
:: 2. Kernel entry point (ASM → ELF32 .o)
:: --------------------------------------------------
echo 2. Compiling Entry Point (kernel/entry.asm)
nasm -f elf32 kernel\entry.asm -o entry.o
if %ERRORLEVEL% neq 0 (
    echo Error compiling entry.asm.
    exit /b %ERRORLEVEL%
)

:: --------------------------------------------------
:: 3. C Subsystems
:: --------------------------------------------------
echo 3. Compiling C Subsystems...

echo    - kernel/kernel.c
i686-elf-gcc %CFLAGS% kernel\kernel.c -o kernel.o
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo    - kernel/graphics.c
i686-elf-gcc %CFLAGS% kernel\graphics.c -o graphics.o
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo    - kernel/memory.c
i686-elf-gcc %CFLAGS% kernel\memory.c -o memory.o
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo    - kernel/wallpaper.c  (procedural renderer)
i686-elf-gcc %CFLAGS% kernel\wallpaper.c -o wallpaper.o
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo    - kernel/window_manager.c
i686-elf-gcc %CFLAGS% kernel\window_manager.c -o window_manager.o
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo    - drivers/mouse.c
i686-elf-gcc %CFLAGS% drivers\mouse.c -o mouse.o
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo    - kernel/terminal_app.c
i686-elf-gcc %CFLAGS% kernel\terminal_app.c -o terminal_app.o
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo    - kernel/desktop.c
i686-elf-gcc %CFLAGS% kernel\desktop.c -o desktop.o
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo    - kernel/browser_app.c
i686-elf-gcc %CFLAGS% kernel\browser_app.c -o browser_app.o
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo    - apps/notes.c
i686-elf-gcc %CFLAGS% apps\notes.c -o notes.o
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo    - apps/snake.c
i686-elf-gcc %CFLAGS% apps\snake.c -o snake.o
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo    - kernel/rtc.c
i686-elf-gcc %CFLAGS% kernel\rtc.c -o rtc.o
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo    - fs/fat12.c
i686-elf-gcc %CFLAGS% fs\fat12.c -o fat12.o
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo    - kernel/loader.c
i686-elf-gcc %CFLAGS% kernel\loader.c -o loader.o
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo    - kernel/paging.c
i686-elf-gcc %CFLAGS% kernel\paging.c -o paging.o
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo    - kernel/gdt.c
i686-elf-gcc %CFLAGS% kernel\gdt.c -o gdt.o
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo    - kernel/tss.c
i686-elf-gcc %CFLAGS% kernel\tss.c -o tss.o
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo    - kernel/process.c
i686-elf-gcc %CFLAGS% kernel\process.c -o process.o
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo    - kernel/syscall.c
i686-elf-gcc %CFLAGS% kernel\syscall.c -o syscall.o
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo    - kernel/elf.c
i686-elf-gcc %CFLAGS% kernel\elf.c -o elf.o
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo    - kernel/sqpkg.c
i686-elf-gcc %CFLAGS% kernel\sqpkg.c -o sqpkg.o
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo    - drivers/pci.c
i686-elf-gcc %CFLAGS% drivers\pci.c -o pci.o
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo    - drivers/ne2000.c
i686-elf-gcc %CFLAGS% drivers\ne2000.c -o ne2000.o
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo    - net/net.c
i686-elf-gcc %CFLAGS% net\net.c -o net.o
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo    - net/ethernet.c
i686-elf-gcc %CFLAGS% net\ethernet.c -o ethernet.o
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo    - net/arp.c
i686-elf-gcc %CFLAGS% net\arp.c -o arp.o
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo    - net/ip.c
i686-elf-gcc %CFLAGS% net\ip.c -o ip.o
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo    - net/icmp.c
i686-elf-gcc %CFLAGS% net\icmp.c -o icmp.o
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo    - net/tcp.c
i686-elf-gcc %CFLAGS% net\tcp.c -o tcp.o
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

:: --------------------------------------------------
:: 4. Link
:: --------------------------------------------------
echo 4. Linking Kernel (linker.ld)
i686-elf-ld -m elf_i386 -T linker.ld -o kernel.bin ^
    entry.o kernel.o graphics.o memory.o wallpaper.o ^
    window_manager.o mouse.o terminal_app.o desktop.o ^
    notes.o snake.o browser_app.o ^
    rtc.o fat12.o loader.o paging.o gdt.o tss.o process.o syscall.o elf.o sqpkg.o ^
    pci.o ne2000.o net.o ethernet.o arp.o ip.o icmp.o tcp.o
if %ERRORLEVEL% neq 0 (
    echo Linker Error.
    exit /b %ERRORLEVEL%
)

echo 4b. Compiling hello.c to hello.elf using SQ SDK
call sdk\sqbuild.bat apps\hello.c hello.elf
if %ERRORLEVEL% neq 0 (
    echo Error compiling hello.elf.
    exit /b %ERRORLEVEL%
)

:: --------------------------------------------------
:: 5. Assemble disk image
:: --------------------------------------------------
echo 5. Assembling os.img
copy /b boot.bin + kernel.bin os.img > nul

:: --------------------------------------------------
:: 6. Pad to 101 sectors (51712 bytes)
:: --------------------------------------------------
echo 6. Padding os.img to 2880 sectors (1.44MB)...
powershell -Command ^
  "$bytes = [System.IO.File]::ReadAllBytes('os.img'); $target = 512 * 2880; if ($bytes.Length -lt $target) { $padded = New-Object byte[] $target; [System.Array]::Copy($bytes, $padded, $bytes.Length); [System.IO.File]::WriteAllBytes('os.img', $padded); echo 'Padded successfully.' } else { echo 'No padding needed.' }"

echo 7. Embedding app store into os.img (sector 120+)
python assets\embed_apps.py
if %ERRORLEVEL% neq 0 (
    echo Warning: embed_apps failed — run commands will show disk error.
)

echo =============================================
echo  Build Successful: os.img
echo  Milestone features:
echo    - Graphics corruption fix (SAFE_WRITE)
echo    - Network: ping 2s timeout + ARP retry
echo    - System Monitor Pro (HH:MM:SS uptime)
echo    - Taskbar: SQ button + active highlight
echo    - Terminal: screenshot + http commands
echo    - TCP stack (single connection)
echo =============================================
endlocal
