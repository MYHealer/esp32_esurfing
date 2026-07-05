#!/usr/bin/env python
"""
Create compiler wrapper .exe files using ctypes + Win32 API.
These will invoke the real compiler.
"""
import os
import struct
import shutil

CLANG_DIR = r"E:\ESP\.espressif\tools\esp-clang\esp-19.1.2_20250312\esp-clang\bin"
WRAPPER_DIR = r"E:\Downloads\Compressed\esp32_esurfing\clang_wrappers"
os.makedirs(WRAPPER_DIR, exist_ok=True)

# Python to the rescue: create .exe that runs python to call clang
# We'll create a batch file and use cmd.exe /c to run it
# But CMake needs .exe...

# Actually, let's use a completely different approach:
# Create a .exe using a tiny shellcode that calls CreateProcess
# with the right compiler path.

# Simplest Windows .exe: a batch file wrapped with a .exe launcher
# Use certutil to decode a base64 .exe that just does CreateProcess

# The MINIMAL Windows .exe is a PE header + some bytes
# But creating that from scratch is complex.

# Alternative: use cygwin's ln or mklink to create hard links
# Or: just use PowerShell to create .exe from .cmd

# Actually, the BEST approach on Windows:
# Use `copy /b` with a small .exe stub

# Let me try yet another approach:
# Write a small C program, compile it with clang, and use that as wrapper

# Create wrapper C source
c_source = '''
#include <windows.h>
#include <stdio.h>

int main(int argc, char* argv[]) {
    // Build command line: CLANG.exe --target=riscv32-esp-unknown-elf <args>
    char cmdline[8192] = "";
    strcat(cmdline, getenv("WRAPPER_TARGET"));

    for (int i = 1; i < argc; i++) {
        strcat(cmdline, " ");
        strcat(cmdline, argv[i]);
    }

    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);

    if (!CreateProcessA(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        fprintf(stderr, "Failed to launch compiler\\n");
        return 1;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return exit_code;
}
'''

# First, compile the wrapper launcher itself using clang
clang = os.path.join(CLANG_DIR, "clang.exe")
launcher_c = os.path.join(WRAPPER_DIR, "launcher.c")
launcher_exe = os.path.join(WRAPPER_DIR, "launcher.exe")

with open(launcher_c, "w") as f:
    f.write(c_source)

import subprocess
# Compile launcher for host (Windows x64)
result = subprocess.run([
    clang,
    "-o", launcher_exe,
    launcher_c,
    "-lkernel32"
], capture_output=True, text=True)

if result.returncode != 0:
    print(f"Failed to compile launcher: {result.stderr}")
    # Fallback: try with cl.exe or gcc
    # Let's just use a different strategy
    print("Trying fallback: creating .bat -> .exe via iexpress...")
else:
    print(f"Launcher compiled: {launcher_exe}")

# Now create individual compiler wrappers
compilers = {
    "riscv32-esp-elf-gcc": "clang.exe",
    "riscv32-esp-elf-g++": "clang++.exe",
    "riscv32-esp-elf-ar": "llvm-ar.exe",
    "riscv32-esp-elf-objcopy": "llvm-objcopy.exe",
    "riscv32-esp-elf-objdump": "llvm-objdump.exe",
    "riscv32-esp-elf-size": "llvm-size.exe",
    "riscv32-esp-elf-strip": "llvm-strip.exe",
    "riscv32-esp-elf-readelf": "llvm-readelf.exe",
}

if os.path.exists(launcher_exe):
    for wrapper_name, real_compiler in compilers.items():
        wrapper_exe = os.path.join(WRAPPER_DIR, wrapper_name + ".exe")
        shutil.copy2(launcher_exe, wrapper_exe)

        # Create env file that the launcher reads
        target = "--target=riscv32-esp-unknown-elf" if "clang" in real_compiler else ""
        compiler_path = os.path.join(CLANG_DIR, real_compiler)
        env_name = wrapper_name + ".env"
        with open(os.path.join(WRAPPER_DIR, env_name), "w") as f:
            f.write(f"WRAPPER_TARGET={compiler_path} {target}\n")

    print(f"Created {len(compilers)} compiler wrappers", flush=True)
else:
    print("Launcher compilation failed, using .cmd fallback", flush=True)
    # Create .cmd files anyway - they work with subprocess but not cmake
    for wrapper_name, real_compiler in compilers.items():
        target = "--target=riscv32-esp-unknown-elf" if "clang" in real_compiler else ""
        compiler_path = os.path.join(CLANG_DIR, real_compiler)
        cmd_path = os.path.join(WRAPPER_DIR, wrapper_name + ".cmd")
        with open(cmd_path, "w") as f:
            f.write(f'@"{compiler_path}" {target} %*\n')
