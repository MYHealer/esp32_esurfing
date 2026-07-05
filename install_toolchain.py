#!/usr/bin/env python
"""
Install ESP-IDF v6 toolchain.
Clears MSYS env vars before calling idf_tools.py, so it works from Git Bash.
"""
import os
import sys
import subprocess

# Clear MSYS/MINGW env vars so idf_tools.py doesn't block us
for key in list(os.environ.keys()):
    if any(kw in key.upper() for kw in ['MSYS', 'MINGW', 'MSYSTEM']):
        del os.environ[key]

IDF_PATH = r"E:\ESP\.espressif\v6.0.1\esp-idf"
IDF6_PYTHON = r"C:\Users\MR\.espressif\python_env\idf6.0_py3.14_env\Scripts\python.exe"

def run_tools(args, desc):
    print(f"\n=== {desc} ===", flush=True)
    cmd = [IDF6_PYTHON, os.path.join(IDF_PATH, "tools", "idf_tools.py")] + args
    env = os.environ.copy()
    env["IDF_PATH"] = IDF_PATH
    env["IDF_PYTHON_ENV_PATH"] = os.path.dirname(os.path.dirname(IDF6_PYTHON))
    # Get ESP_IDF_VERSION from version.txt
    version_file = os.path.join(IDF_PATH, "version.txt")
    if os.path.exists(version_file):
        with open(version_file) as f:
            env["ESP_IDF_VERSION"] = f.read().strip()
    else:
        env["ESP_IDF_VERSION"] = "6.0.1"
    result = subprocess.run(cmd, env=env)
    return result.returncode

# Install RISC-V GCC toolchain
rc = run_tools(["install", "riscv32-esp-elf-gcc"], "Install RISC-V GCC")
if rc == 0:
    print("\nToolchain installed successfully!", flush=True)
    # List installed tools
    run_tools(["list"], "List installed tools")
else:
    print("\nToolchain installation FAILED", flush=True)
    sys.exit(1)
