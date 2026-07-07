"""Build all chip variants for esp32_esurfing.
Usage: python build_release.py"""
import os, sys, subprocess, shutil, zipfile

BASE = r"E:\Downloads\Compressed\esp32_esurfing"
IDF_PATH = r"E:\ESP\.espressif\v5.5.4\esp-idf"
PYTHON = r"C:\Users\MR\.espressif\python_env\idf5.5_py3.14_env\Scripts\python.exe"
IDF_PY = os.path.join(IDF_PATH, "tools", "idf.py")
CMAKE_BIN = r"E:\ESP\.espressif\tools\cmake\4.0.3\bin"
NINJA_BIN = r"E:\ESP\.espressif\tools\ninja\1.12.1"

TOOLCHAINS = {
    "riscv": r"C:\Users\MR\AppData\Local\Arduino15\packages\esp32\tools\esp-rv32\2601-cn\bin",
    "xtensa": r"E:\ESP\.espressif\tools\xtensa-esp-elf\esp-14.2.0_20260121\xtensa-esp-elf\bin",
    "riscv_idf": r"E:\ESP\.espressif\tools\riscv32-esp-elf\esp-16.1.0_20260609\riscv32-esp-elf\bin",
}

VARIANTS = [
    {
        "id": "ESP32-C3",
        "target": "esp32c3",
        "tc": "riscv",
        "desc": "ESP32-C3 (RISC-V, 市面常见开发板)",
        "supermini": False,
        "extra_sdk": [],
    },
    {
        "id": "ESP32-S3",
        "target": "esp32s3",
        "tc": "xtensa",
        "desc": "ESP32-S3 (Xtensa双核, USB-OTG)",
        "supermini": False,
        "extra_sdk": [],
    },
    {
        "id": "ESP32",
        "target": "esp32",
        "tc": "xtensa",
        "desc": "ESP32 (Xtensa双核, 最经典款)",
        "supermini": False,
        "extra_sdk": [
            "CONFIG_FREERTOS_UNICORE=n",
        ],
    },
    {
        "id": "ESP32-S2",
        "target": "esp32s2",
        "tc": "xtensa",
        "desc": "ESP32-S2 (Xtensa单核, 原生USB)",
        "supermini": False,
        "extra_sdk": [],
    },
    {
        "id": "ESP32-C6",
        "target": "esp32c6",
        "tc": "riscv_idf",
        "desc": "ESP32-C6 (RISC-V, WiFi 6, 新一代)",
        "supermini": False,
        "extra_sdk": [],
        "boot_offset": "0x0",
    },
    {
        "id": "ESP32-C5",
        "target": "esp32c5",
        "tc": "riscv_idf",
        "desc": "ESP32-C5 (RISC-V, WiFi 6双频)",
        "supermini": False,
        "extra_sdk": [],
        "boot_offset": "0x2000",
    },
    {
        "id": "ESP32-C61",
        "target": "esp32c61",
        "tc": "riscv_idf",
        "desc": "ESP32-C61 (RISC-V, WiFi 6, C6升级版)",
        "supermini": False,
        "extra_sdk": [],
        "boot_offset": "0x0",
    },
]

SDK_BASE = """CONFIG_ESP_WIFI_SOFTAP_SUPPORT=y
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

WIFI_MGR_WITH_POWER = """    ESP_LOGI(TAG, "AP 已开启: ESurfing-Config (192.168.4.1)");
    esp_wifi_set_max_tx_power(34);"""

WIFI_MGR_NO_POWER = """    ESP_LOGI(TAG, "AP 已开启: ESurfing-Config (192.168.4.1)");"""

OUT = os.path.join(BASE, "release")
FLASH_GUIDE = """Flash Download Tool 烧录参数

芯片: {chip}
Flash Size: 4MB
SPI Mode: DIO
SPI Speed: 80MHz
Baud: 460800

地址     | 文件
---------|------------------
{boot_offset} | bootloader.bin
0x8000   | partition-table.bin
0x10000  | esp32_esurfing.bin
0x2D0000 | spiffs.bin

首次使用:
1. 打开 Flash Download Tool → 选 {chip} → UART
2. 按上表填入地址和文件
3. 点 START 烧录
4. 上电后手机搜到 WiFi "ESurfing-Config"
5. 浏览器打开 http://192.168.4.1
6. 配置WiFi和校园网账号 → 保存 → 自动重启认证
"""

# Backup original
shutil.copy2(os.path.join(BASE, "main", "wifi_manager.c"),
             os.path.join(BASE, "main", "wifi_manager.c.bak"))

results = {}
targets = sys.argv[1:] if len(sys.argv) > 1 else ["all"]
variants = [v for v in VARIANTS if "all" in targets or v["id"] in targets]

for v in variants:
    print(f"\n{'='*60}")
    print(f"  {v['id']} — {v['desc']}")
    print(f"{'='*60}")

    # Clean
    for p in ["build", "sdkconfig", "sdkconfig.old"]:
        fp = os.path.join(BASE, p)
        if os.path.exists(fp):
            if os.path.isfile(fp): os.remove(fp)
            elif os.path.isdir(fp): shutil.rmtree(fp, ignore_errors=True)

    # Write sdkconfig.defaults
    sdk = SDK_BASE
    for extra in v["extra_sdk"]:
        sdk += extra + "\n"
    with open(os.path.join(BASE, "sdkconfig.defaults"), "w", encoding="utf-8") as f:
        f.write(sdk)

    # Patch wifi_manager: remove power reduction for non-SuperMini
    with open(os.path.join(BASE, "main", "wifi_manager.c"), "r", encoding="utf-8") as f:
        wm = f.read()
    if v["supermini"]:
        if "esp_wifi_set_max_tx_power" not in wm:
            wm = wm.replace("    ESP_LOGI(TAG, \"AP 已开启: ESurfing-Config (192.168.4.1)\");",
                             WIFI_MGR_WITH_POWER)
    else:
        wm = wm.replace("esp_wifi_set_max_tx_power(34);\n    ", "")
    with open(os.path.join(BASE, "main", "wifi_manager.c"), "w", encoding="utf-8") as f:
        f.write(wm)

    # Build env
    env = os.environ.copy()
    for k in list(env.keys()):
        if any(kw in k.upper() for kw in ['MSYS', 'MINGW', 'MSYSTEM']):
            del env[k]
    env["IDF_PATH"] = IDF_PATH
    env["IDF_MAINTAINER"] = "1"
    env["PATH"] = f"{os.path.dirname(PYTHON)};{TOOLCHAINS[v['tc']]};{CMAKE_BIN};{NINJA_BIN};{env.get('PATH', '')}"

    # set-target
    print(f"  设置目标: {v['target']}")
    r = subprocess.run([PYTHON, IDF_PY, "-C", BASE, "set-target", v["target"]],
                       env=env, capture_output=True, text=True, timeout=120)
    if r.returncode != 0:
        print(f"  ✗ set-target 失败")
        for l in r.stderr.split('\n')[-5:]:
            if l.strip(): print(f"    {l.strip()}")
        results[v["id"]] = False
        continue

    # build
    print(f"  编译中...")
    r = subprocess.run([PYTHON, IDF_PY, "-C", BASE, "build"],
                       env=env, capture_output=True, text=True, timeout=600)
    if r.returncode != 0:
        print(f"  ✗ 编译失败")
        for l in r.stderr.split('\n'):
            if 'error:' in l.lower() and 'warning' not in l.lower():
                print(f"    {l.strip()}")
        results[v["id"]] = False
        continue

    # Collect binaries
    build_dir = os.path.join(BASE, "build")
    vdir = os.path.join(OUT, v["id"])
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

    # Create zip
    zpath = os.path.join(OUT, f"{v['id']}_v1.0.0.zip")
    with zipfile.ZipFile(zpath, 'w', zipfile.ZIP_DEFLATED) as z:
        for bn, bp in bins.items():
            if os.path.exists(bp):
                z.write(bp, bn)
        z.writestr("flash_guide.txt", FLASH_GUIDE.format(chip=v["target"].upper(), boot_offset=v.get("boot_offset", "0x0")))

    print(f"  ✓ {zpath} ({os.path.getsize(zpath)//1024} KB)")
    results[v["id"]] = True

# Restore backup
shutil.copy2(os.path.join(BASE, "main", "wifi_manager.c.bak"),
             os.path.join(BASE, "main", "wifi_manager.c"))
os.remove(os.path.join(BASE, "main", "wifi_manager.c.bak"))

# Summary
print(f"\n{'='*60}")
print(f"  构建结果")
print(f"{'='*60}")
for vid, ok in results.items():
    print(f"  {'✓' if ok else '✗'} {vid}")
