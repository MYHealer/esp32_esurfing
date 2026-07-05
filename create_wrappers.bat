@echo off
REM Wrapper scripts for ESP-IDF clang toolchain
REM These make clang appear as riscv32-esp-elf-gcc/g++/ar/etc

set CLANG_DIR=E:\ESP\.espressif\tools\esp-clang\esp-19.1.2_20250312\esp-clang\bin

REM gcc wrapper
echo @echo off > "%CLANG_DIR%\riscv32-esp-elf-gcc.exe.wrapper"
echo "%CLANG_DIR%\clang.exe" --target=riscv32-esp-unknown-elf %%* >> "%CLANG_DIR%\riscv32-esp-elf-gcc.exe.wrapper"

REM Create a batch file approach instead
