# ESP-IDF Clang toolchain wrapper for ESP32-C3
# Includes the IDF toolchain, then overrides tool paths to use full paths
# so ninja can find them on Windows (where bare names fail without PATH)

set(CLANG_BIN "$ENV{IDF_PATH}/../../tools/esp-clang/esp-19.1.2_20250312/esp-clang/bin")

# Include the original IDF clang toolchain
include($ENV{IDF_PATH}/tools/cmake/toolchain-clang-esp32c3.cmake)

# Override with full paths
set(CMAKE_C_COMPILER "${CLANG_BIN}/clang.exe")
set(CMAKE_CXX_COMPILER "${CLANG_BIN}/clang++.exe")
set(CMAKE_ASM_COMPILER "${CLANG_BIN}/clang.exe")
set(CMAKE_AR "${CLANG_BIN}/llvm-ar.exe")
set(CMAKE_RANLIB "${CLANG_BIN}/llvm-ranlib.exe")
set(CMAKE_LINKER "${CLANG_BIN}/riscv32-esp-elf-clang-ld.exe")
set(CMAKE_OBJDUMP "${CLANG_BIN}/riscv32-esp-elf-clang-objdump.exe")
