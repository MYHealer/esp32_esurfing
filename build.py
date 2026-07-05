#!/usr/bin/env python
"""
Build ESurfingClient for ESP32-C3 using ESP-IDF v6 clang toolchain.
IDF v6 has built-in clang support via -DIDF_TOOLCHAIN=clang.
The clang toolchain file expects: clang, clang++, riscv32-esp-elf-clang-ld, etc.
These tools exist in esp-clang/bin. We just need PATH set correctly.
"""
import os, sys, subprocess, shutil, glob

for key in list(os.environ.keys()):
    if any(kw in key.upper() for kw in ['MSYS', 'MINGW', 'MSYSTEM']):
        del os.environ[key]

IDF_PATH = r"E:\ESP\.espressif\v6.0.1\esp-idf"
PROJECT_DIR = r"E:\Downloads\Compressed\esp32_esurfing"
IDF6_PYTHON = r"C:\Users\MR\.espressif\python_env\idf6.0_py3.14_env\Scripts\python.exe"
CLANG_BIN = r"E:\ESP\.espressif\tools\esp-clang\esp-19.1.2_20250312\esp-clang\bin"
CMAKE_BIN = r"E:\ESP\.espressif\tools\cmake\4.0.3\bin"
NINJA_BIN = r"E:\ESP\.espressif\tools\ninja\1.12.1"

os.chdir(PROJECT_DIR)

# The clang toolchain file expects these on PATH:
# - clang (exists in esp-clang/bin)
# - clang++ (exists in esp-clang/bin)
# - riscv32-esp-elf-clang-ld (exists in esp-clang/bin)
# - riscv32-esp-elf-clang-objdump (exists in esp-clang/bin)
# - llvm-ar (exists in esp-clang/bin)
# - llvm-ranlib (needs check)
# - llvm-size or similar

# Check which tools exist
print("Checking esp-clang tools:", flush=True)
for t in ["clang.exe", "clang++.exe", "riscv32-esp-elf-clang-ld.exe",
          "riscv32-esp-elf-clang-objdump.exe", "llvm-ar.exe", "llvm-ranlib.exe"]:
    path = os.path.join(CLANG_BIN, t)
    exists = "OK" if os.path.exists(path) else "MISSING"
    print(f"  {t}: {exists}", flush=True)

# Check for missing tools and create stubs
for tool in ["llvm-ranlib.exe"]:
    src = os.path.join(CLANG_BIN, tool)
    if not os.path.exists(src):
        # llvm-ranlib can be llvm-ar (they're the same on LLVM)
        llvm_ar = os.path.join(CLANG_BIN, "llvm-ar.exe")
        if os.path.exists(llvm_ar):
            shutil.copy2(llvm_ar, src)
            print(f"  Created {tool} (copy of llvm-ar)", flush=True)

# Clean
for p in ["build", "managed_components"]:
    if os.path.isdir(p): shutil.rmtree(p, ignore_errors=True)

env = os.environ.copy()
env["IDF_PATH"] = IDF_PATH
env["IDF_PYTHON_ENV_PATH"] = os.path.dirname(os.path.dirname(IDF6_PYTHON))
env["ESP_IDF_VERSION"] = "6.0.1"
env["PATH"] = CLANG_BIN + ";" + CMAKE_BIN + ";" + NINJA_BIN + ";" + env.get("PATH", "")
env["IDF_TOOLCHAIN"] = "clang"
# Also set system PATH so ninja subprocesses can find llvm-ar, etc.
os.environ["PATH"] = CLANG_BIN + ";" + CMAKE_BIN + ";" + NINJA_BIN + ";" + os.environ.get("PATH", "")

# Step 1: set-target with clang
print("\n=== set-target esp32c3 (clang) ===", flush=True)
r = subprocess.run([IDF6_PYTHON, os.path.join(IDF_PATH, "tools", "idf.py"),
                    "-C", PROJECT_DIR, "set-target", "esp32c3"], env=env)
if r.returncode != 0:
    print("\nset-target FAILED", flush=True)
    sys.exit(1)

# Step 2: build
print("\n=== build ===", flush=True)
r = subprocess.run([IDF6_PYTHON, os.path.join(IDF_PATH, "tools", "idf.py"),
                    "-C", PROJECT_DIR, "build"], env=env)

if r.returncode == 0:
    print("\n=== BUILD SUCCESS ===", flush=True)
    build_dir = os.path.join(PROJECT_DIR, "build")
    if os.path.isdir(build_dir):
        for f in os.listdir(build_dir):
            if f.endswith(".bin"):
                fp = os.path.join(build_dir, f)
                print(f"  {f} - {os.path.getsize(fp)//1024} KB")
    print("\nFlash: . E:\\ESP\\.espressif\\v6.0.1\\esp-idf\\export.ps1 ; idf.py -p COM14 flash monitor")
else:
    print("\nBUILD FAILED", flush=True)
    sys.exit(1)
