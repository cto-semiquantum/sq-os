@echo off
setlocal

set GCC_PATH=C:\Users\Harsh\i686-elf-tools\bin
set PATH=%GCC_PATH%;%PATH%

if "%~1"=="" (
    echo Usage: sqbuild.bat ^<source_c_file^> ^<output_elf_file^>
    exit /b 1
)

if "%~2"=="" (
    echo Usage: sqbuild.bat ^<source_c_file^> ^<output_elf_file^>
    exit /b 1
)

echo [SQ SDK] Building %~1 to %~2...

:: Compile startup stub
nasm -f elf32 sdk\lib\user_entry.asm -o sdk\lib\user_entry.o
if %ERRORLEVEL% neq 0 (
    echo [SQ SDK] NASM Compile Error.
    exit /b %ERRORLEVEL%
)

:: Compile user-mode system library
i686-elf-gcc -c -m32 -ffreestanding -O2 -Isdk/include sdk\lib\libsq.c -o sdk\lib\libsq.o
if %ERRORLEVEL% neq 0 (
    echo [SQ SDK] GCC Library Compile Error.
    exit /b %ERRORLEVEL%
)

:: Compile target C file
i686-elf-gcc -c -m32 -ffreestanding -O2 -Isdk/include "%~1" -o sdk\lib\user_app.o
if %ERRORLEVEL% neq 0 (
    echo [SQ SDK] GCC Target Compile Error.
    exit /b %ERRORLEVEL%
)

:: Link user application
i686-elf-ld -m elf_i386 -T sdk\lib\user.ld sdk\lib\user_entry.o sdk\lib\libsq.o sdk\lib\user_app.o -o "%~2"
if %ERRORLEVEL% neq 0 (
    echo [SQ SDK] Linker Error.
    exit /b %ERRORLEVEL%
)

echo [SQ SDK] Successfully built %~2!
