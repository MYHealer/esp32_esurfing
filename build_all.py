#!/usr/bin/env python
"""Multi-target builder. Usage: python build_all.py [c3|c3mini|s3|esp32|all]"""
import os, sys, subprocess, shutil, zipfile

BASE = r"E:\Downloads\Compressed\esp32_esurfing"
IDF_PATH = r"E:\ESP\.espressif\v5.5.4\esp-idf"
PYTHON = r"C:\Users\MR\.espressif\python_env\idf5.5_py3.14_env\Scripts\python.exe"
IDF_PY = os.path.join(IDF_PATH, "tools", "idf.py")
CMAKE_BIN = r"E:\ESP\.espressif\tools\cmake\4.0.3\bin"
NINJA_BIN = r"E:\ESP\.espressif\tools\ninja\1.12.1"
RV_TC = r"C:\Users\MR\AppData\Local\Arduino15\packages\esp32\tools\esp-rv32\2601-cn\bin"
XT_TC = r"E:\ESP\.espressif\tools\xtensa-esp-elf\esp-15.2.0_20251204\xtensa-esp-elf\bin"

SKD_DEFAULTS = """CONFIG_ESP_WIFI_SOFTAP_SUPPORT=y
# CONFIG_ESP_WIFI_ENABLE_WPA3_SAE is not set
# CONFIG_ESP_WIFI_AMPDU_TX_ENABLED is not set
# CONFIG_ESP_WIFI_AMPDU_RX_ENABLED is not set
# CONFIG_ESP_WIFI_STA_DISCONNECTED_PM_ENABLE is not set
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
CONFIG_FREERTOS_HZ=100
CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192
CONFIG_SPIFFS_SUPPORT=y
CONFIG_SPIFFS_CACHE=y
CONFIG_SPIFFS_MAX_PARTITIONS=1
CONFIG_ESURF_WIFI_SSID="your_wifi_ssid"
CONFIG_ESURF_WIFI_PASSWORD="your_wifi_password"
"""

VARIANTS = {
    "esp32c3_standard": {
        "target": "esp32c3", "name": "ESP32-C3",
        "desc": "ESP32-C3 (RISC-V, 标准功率)",
        "tc": RV_TC, "supermini": False,
    },
    "esp32s3": {
        "target": "esp32s3", "name": "ESP32-S3",
        "desc": "ESP32-S3 (Xtensa双核)",
        "tc": XT_TC, "supermini": False,
    },
    "esp32": {
        "target": "esp32", "name": "ESP32",
        "desc": "ESP32 (Xtensa双核, 经典款)",
        "tc": XT_TC, "supermini": False,
    },
}

OUT = os.path.join(BASE, "build_dist")

def build_one(variant_key, v):
    print(f"\n{'='*60}")
    print(f"  {v['name']} — {v['desc']}")
    print(f"{'='*60}")

    # Clean
    for p in ["build", "sdkconfig", "sdkconfig.old"]:
        fp = os.path.join(BASE, p)
        if os.path.isfile(fp): os.remove(fp)
        elif os.path.isdir(fp): shutil.rmtree(fp, ignore_errors=True)

    # Write sdkconfig.defaults
    with open(os.path.join(BASE, "sdkconfig.defaults"), "w", encoding="utf-8") as f:
        f.write(SKD_DEFAULTS)

    # Fixe WiFi manager (remove power reduction for non-SuperMini)
    wm_path = os.path.join(BASE, "main", "wifi_manager.c")
    with open(wm_path, "r", encoding="utf-8") as f:
        wm = f.read()
    wm_modified = "esp_wifi_set_max_tx_power(34);\n    " in wm
    wm = wm.replace("esp_wifi_set_max_tx_power(34);\n    ", "")
    with open(wm_path, "w", encoding="utf-8") as f:
        f.write(wm)

    # Build env
    env = os.environ.copy()
    env["IDF_PATH"] = IDF_PATH
    for k in list(env.keys()):
        if any(kw in k.upper() for kw in ['MSYS', 'MINGW', 'MSYSTEM']):
            del env[k]
    env["PATH"] = f"{os.path.dirname(PYTHON)};{v['tc']};{CMAKE_BIN};{NINJA_BIN};{env.get('PATH', '')}"

    # set-target
    print(f"  Target: {v['target']}")
    r = subprocess.run([PYTHON, IDF_PY, "-C", BASE, "set-target", v["target"]],
                       env=env, capture_output=True, text=True, timeout=120)
    if r.returncode != 0:
        err = r.stderr[-300:]
        print(f"  set-target 失败: {err}")
        return False

    # build
    print(f"  编译中...")
    r = subprocess.run([PYTHON, IDF_PY, "-C", BASE, "build"],
                       env=env, capture_output=True, text=True, timeout=600)
    if r.returncode != 0:
        for l in r.stderr.split('\n'):
            if 'error:' in l.lower() or 'fatal' in l.lower():
                print(f"  {l.strip()}")
        return False

    # Collect
    build_dir = os.path.join(BASE, "build")
    os.makedirs(OUT, exist_ok=True)
    vdir = os.path.join(OUT, v["name"])
    os.makedirs(vdir, exist_ok=True)

    bins = {
        "bootloader.bin": os.path.join(build_dir, "bootloader", "bootloader.bin"),
        "partition-table.bin": os.path.join(build_dir, "partition_table", "partition-table.bin"),
        "spiffs.bin": os.path.join(build_dir, "spiffs.bin"),
    }
    for f in os.listdir(build_dir):
        if f.endswith(".bin") and f not in ("bootloader.bin", "partition-table.bin", "spiffs.bin"):
            bins["esp32_esurfing.bin"] = os.path.join(build_dir, f)

    for bn, bp in bins.items():
        if os.path.exists(bp):
            shutil.copy2(bp, os.path.join(vdir, bn))

    zpath = os.path.join(OUT, f"{v['name']}_v1.0.0.zip")
    with zipfile.ZipFile(zpath, 'w', zipfile.ZIP_DEFLATED) as z:
        for bn, bp in bins.items():
            if os.path.exists(bp):
                z.write(bp, bn)
        if os.path.exists(os.path.join(BASE, "README.md")):
            z.write(os.path.join(BASE, "README.md"), "README.md")
        guide = f"Flash {v['name']}:\nChip: {v['target'].upper()}\n4MB, DIO, 80MHz, 460800 baud\n"
        z.writestr("flash_guide.txt", guide)

    print(f"  ✓ {zpath} ({os.path.getsize(zpath)//1024} KB)")
    return True


if __name__ == "__main__":
    targets = sys.argv[1:] if len(sys.argv) > 1 else ["all"]
    if "all" in targets:
        targets = list(VARIANTS.keys())

    results = {}
    for t in targets:
        if t in VARIANTS:
            results[t] = build_one(t, VARIANTS[t])
        else:
            print(f"? 未知: {t}")
            print(f"  可选: all {' '.join(VARIANTS.keys())}")

    # Restore SuperMini wifi_manager.c
    wm_path = os.path.join(BASE, "main", "wifi_manager.c")
    with open(wm_path, "r", encoding="utf-8") as f:
        wm = f.read()
    if "esp_wifi_set_max_tx_power(34)" not in wm:
        wm = wm.replace(
            '    ESP_LOGI(TAG, "AP 已开启: ESurfing-Config (192.168.4.1)");',
            '    ESP_LOGI(TAG, "AP 已开启: ESurfing-Config (192.168.4.1)");\n    esp_wifi_set_max_tx_power(34);')
        with open(wm_path, "w", encoding="utf-8") as f:
            f.write(wm)

    # Restore sdkconfig.defaults
    if os.path.exists(os.path.join(BASE, "sdkconfig.defaults.supermini")):
        shutil.copy2(os.path.join(BASE, "sdkconfig.defaults.supermini"),
                     os.path.join(BASE, "sdkconfig.defaults"))

    print(f"\n{'='*60}")
    print(f"  完成: {sum(1 for v in results.values() if v)}/{len(results)}")
    for t, ok in results.items():
        print(f"  {'✓' if ok else '✗'} {VARIANTS[t]['name']}")
