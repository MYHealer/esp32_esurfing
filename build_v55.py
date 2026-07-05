#!/usr/bin/env python
"""用 ESP-IDF v5.5 + Arduino RISC-V 工具链编译"""
import os, sys, subprocess, shutil

for key in list(os.environ.keys()):
    if any(kw in key.upper() for kw in ['MSYS', 'MINGW', 'MSYSTEM']):
        del os.environ[key]

IDF_PATH = r"E:\ESP\.espressif\v5.5.4\esp-idf"
PROJECT_DIR = r"E:\Downloads\Compressed\esp32_esurfing"
IDF6_PYTHON = r"C:\Users\MR\.espressif\python_env\idf5.5_py3.14_env\Scripts\python.exe"
RV_TOOLCHAIN = r"C:\Users\MR\AppData\Local\Arduino15\packages\esp32\tools\esp-rv32\2601-cn\bin"
CMAKE_PATH = r"E:\ESP\.espressif\tools\cmake\4.0.3\bin"
NINJA_PATH = r"E:\ESP\.espressif\tools\ninja\1.12.1"

os.chdir(PROJECT_DIR)
for p in ["build", "managed_components"]:
    if os.path.isdir(p): shutil.rmtree(p, ignore_errors=True)

env = os.environ.copy()
env["IDF_PATH"] = IDF_PATH
env["PATH"] = RV_TOOLCHAIN + ";" + CMAKE_PATH + ";" + NINJA_PATH + ";" + env.get("PATH", "")
env["IDF_PYTHON_ENV_PATH"] = os.path.dirname(os.path.dirname(IDF6_PYTHON))
env["ESP_IDF_VERSION"] = "5.5.4"

def run_idf(args, desc):
    print(f"\n=== {desc} ===", flush=True)
    cmd = [IDF6_PYTHON, os.path.join(IDF_PATH, "tools", "idf.py"), "-C", PROJECT_DIR] + args
    return subprocess.run(cmd, env=env).returncode

rc = run_idf(["set-target", "esp32c3"], "set-target")
if rc != 0: sys.exit(1)

rc = run_idf(["build"], "build")
if rc == 0:
    print("\n=== BUILD SUCCESS ===")
    for f in os.listdir("build"):
        if f.endswith(".bin"):
            print(f"  {f} - {os.path.getsize(os.path.join('build',f))//1024} KB")
else:
    print("\nBUILD FAILED"); sys.exit(1)
