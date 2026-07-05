#!/usr/bin/env python
"""
Install all ESP-IDF v6 tools.
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
TOOLS_PY = os.path.join(IDF_PATH, "tools", "idf_tools.py")

def run_tools(args, desc):
    print(f"\n=== {desc} ===", flush=True)
    cmd = [IDF6_PYTHON, TOOLS_PY] + args
    env = os.environ.copy()
    env["IDF_PATH"] = IDF_PATH
    env["IDF_PYTHON_ENV_PATH"] = os.path.dirname(os.path.dirname(IDF6_PYTHON))
    version_file = os.path.join(IDF_PATH, "version.txt")
    if os.path.exists(version_file):
        with open(version_file) as f:
            env["ESP_IDF_VERSION"] = f.read().strip()
    else:
        env["ESP_IDF_VERSION"] = "6.0.1"
    result = subprocess.run(cmd, env=env)
    return result.returncode

# Install all required tools for ESP32-C3
print("Installing all ESP-IDF v6 tools...", flush=True)
rc = run_tools(["install"], "Install all tools")
if rc == 0:
    print("\nAll tools installed successfully!", flush=True)
    # Export paths
    run_tools(["export", "--format", "key-value"], "Export tool paths")
else:
    print("\nTool installation FAILED", flush=True)
    sys.exit(1)
