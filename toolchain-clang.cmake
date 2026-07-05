# ESP-IDF Clang toolchain for ESP32-C3
# Used when riscv32-esp-elf-gcc is not installed

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR riscv)

set(CLANG_BIN "E:/ESP/.espressif/tools/esp-clang/esp-19.1.2_20250312/esp-clang/bin")
set(CLANG_RT "E:/ESP/.espressif/tools/esp-clang/esp-19.1.2_20250312/esp-clang/lib/clang-runtimes/riscv32-esp-unknown-elf")

set(CMAKE_C_COMPILER "${CLANG_BIN}/clang.exe")
set(CMAKE_CXX_COMPILER "${CLANG_BIN}/clang++.exe")
set(CMAKE_ASM_COMPILER "${CLANG_BIN}/clang.exe")
set(CMAKE_AR "${CLANG_BIN}/llvm-ar.exe")
set(CMAKE_LINKER "${CLANG_BIN}/clang.exe")
set(CMAKE_OBJCOPY "${CLANG_BIN}/llvm-objcopy.exe")
set(CMAKE_OBJDUMP "${CLANG_BIN}/llvm-objdump.exe")
set(CMAKE_SIZE "${CLANG_BIN}/llvm-size.exe")
set(CMAKE_STRIP "${CLANG_BIN}/llvm-strip.exe")

set(CMAKE_C_COMPILER_TARGET riscv32-esp-unknown-elf)
set(CMAKE_CXX_COMPILER_TARGET riscv32-esp-unknown-elf)
set(CMAKE_ASM_COMPILER_TARGET riscv32-esp-unknown-elf)

set(CMAKE_C_FLAGS_INIT "-target riscv32-esp-unknown-elf -march=rv32imc_zicsr_zifencei -mabi=ilp32")
set(CMAKE_CXX_FLAGS_INIT "-target riscv32-esp-unknown-elf -march=rv32imc_zicsr_zifencei -mabi=ilp32")
set(CMAKE_ASM_FLAGS_INIT "-target riscv32-esp-unknown-elf -march=rv32imc_zicsr_zifencei -mabi=ilp32")

set(CMAKE_FIND_ROOT_PATH "${CLANG_RT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
