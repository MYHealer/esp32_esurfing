# ESP32 ESurfing — 天翼校园网自动认证客户端

> 基于 [ESurfingClient-CVersion](https://github.com/BadGhost520/ESurfingClient-CVersion) 移植到 ESP32-C3 的校园网自动认证工具。

## 适用人群

- **在校大学生** — 使用中国电信天翼校园网的宿舍用户
- **需要自动认证** — 路由器不方便刷机/不想折腾 OpenWrt，用 ESP32 旁路认证
- **ESP32-C3 玩家** — 手上有 ESP32-C3 SuperMini 或其他 ESP32-C3 开发板

## 功能

- ✅ **自动认证** — 检测到 captive portal 重定向后自动完成登录
- ✅ **心跳保活** — 按服务器指定间隔发送心跳，保持不掉线
- ✅ **断线重连** — WiFi 断开自动重连，认证失败自动重试（无限）
- ✅ **Web 配置后台** — AP `ESurfing-Config` 常开，浏览器访问 `http://192.168.4.1` 配置
- ✅ **WiFi + 校园网账号** — 均可通过网页修改，保存后自动重启
- ✅ **PCB 天线适配** — 已为 ESP32-C3 SuperMini 降低 TX 功率（8.5 dBm）
- ✅ **7×24 小时运行** — 插电运行，无需维护

## 工作原理

```
┌─────────────────────────────────────────────────┐
│                    ESP32-C3                      │
│  ┌────────────┐    ┌────────────────────────┐   │
│  │ WiFi STA   │───▶│  检测 captive portal    │   │
│  │ (校园网)   │    │  (GET generate_204)     │   │
│  └────────────┘    └────────┬───────────────┘   │
│                             ▼                    │
│  ┌────────────────────────────────────────┐      │
│  │  302 重定向 → 提取 portal 配置          │      │
│  └────────────────┬───────────────────────┘      │
│                   ▼                              │
│  ┌────────────────────────────────────────┐      │
│  │  init_session → 获取加密算法和会话      │      │
│  ├────────────────────────────────────────┤      │
│  │  get_ticket → 请求认证票据             │      │
│  ├────────────────────────────────────────┤      │
│  │  login → 提交用户名密码完成登录         │      │
│  ├────────────────────────────────────────┤      │
│  │  heartbeat → 保活（每 N 秒一次）        │      │
│  └────────────────────────────────────────┘      │
│                                                  │
│  ┌──────────────────────────────┐                │
│  │ SoftAP: ESurfing-Config      │◀── 手机连接    │
│  │ Web: http://192.168.4.1      │    改配置      │
│  └──────────────────────────────┘                │
└─────────────────────────────────────────────────┘
```

## 支持的开发板

本项目已编译固件适配以下芯片，覆盖市面上绝大多数 ESP32 开发板：

| 芯片 | 常见开发板 | 架构 | 特点 |
|------|-----------|------|------|
| **ESP32** | ESP32-DevKitC, NodeMCU-32S, ESP-WROOM-32 | Xtensa 双核 | 最经典，市场保有量最大 |
| **ESP32-S3** | ESP32-S3-DevKitC-1, S3-Box, T-Display-S3 | Xtensa 双核 | USB-OTG, 大 RAM, AI 加速 |
| **ESP32-C3** | ESP32-C3-DevKitC-02, C3 SuperMini | RISC-V 单核 | 低成本, RISC-V 架构 |
| **ESP32-S2** | ESP32-S2-Saola-1, S2-Mini | Xtensa 单核 | 原生 USB, 无蓝牙 |
| **ESP32-C3 SuperMini** | C3 SuperMini 专用版 | RISC-V 单核 | 已降功率适配 PCB 天线 |

> 所有版本均需要 **4MB Flash**。不支持 2MB Flash 的 ESP32-C2 芯片。
> 
> 固件可在 [Releases](https://github.com/MYHealer/esp32_esurfing/releases) 下载，选择对应芯片的 zip 包。

## 快速开始

### 1. 下载固件

从 [Releases](https://github.com/MYHealer/esp32_esurfing/releases) 下载对应芯片的 `XXX_v1.0.0.zip` 包。

每个 zip 包含：
- `bootloader.bin` — 引导程序
- `partition-table.bin` — 分区表
- `esp32_esurfing.bin` — 应用程序
- `spiffs.bin` — 配置文件分区
- `flash_guide.txt` — 该芯片的烧录参数
- `README.md` — 完整说明

## 快速开始

### 1. 下载固件

从 [Releases](https://github.com/MYHealer/esp32_esurfing/releases) 下载最新的 `esp32_esurfing.bin` 和 `spiffs.bin`。

### 2. 烧录

**Windows 一键脚本**（需安装 Python）：

```powershell
# 安装 esptool
pip install esptool

# 烧录全部组件
esptool --port COM14 --baud 460800 write_flash ^
    0x0 bootloader.bin ^
    0x8000 partition-table.bin ^
    0x10000 esp32_esurfing.bin ^
    0x2D0000 spiffs.bin
```

> `COM14` 替换为你实际的串口号。

### 3. 配置

1. 开发板上电后，手机搜索 WiFi `ESurfing-Config`（无密码）
2. 连接后浏览器打开 `http://192.168.4.1`
3. 填写校园网 WiFi 名称、密码，校园网账号密码，点击保存
4. 设备自动重启并开始认证

## 自行构建

### 前置条件

| 工具 | 版本 | 说明 |
|------|------|------|
| ESP-IDF | v5.5.4 | 构建环境 |
| Python | 3.14+ | ESP-IDF 依赖 |
| CMake | 4.0+ | 构建系统 |
| Ninja | 1.12+ | 构建工具 |
| RISC-V GCC | esp-rv32 2601-cn | 编译工具链 |

> ESP-IDF v6.x + esp-clang 也可编译，但 WiFi 兼容性不如 v5.5 + GCC。

### 构建步骤

```bash
# 1. 设置 IDF 路径
export IDF_PATH=E:/ESP/.espressif/v5.5.4/esp-idf

# 2. 安装 IDF 依赖
python -m pip install -r $IDF_PATH/tools/requirements/requirements.core.txt

# 3. 构建
python build_v55.py

# 4. 烧录
python -m esptool --port COM14 --baud 460800 write_flash ^
    0x0 build/bootloader/bootloader.bin ^
    0x8000 build/partition_table/partition-table.bin ^
    0x10000 build/esp32_esurfing.bin ^
    0x2D0000 build/spiffs.bin
```

### 一键构建脚本

项目根目录包含：
- `build_v55.py` — Windows 构建脚本（自动配置 IDF v5.5 环境）
- `build.py` — IDF v6.0 + esp-clang 构建（备选，WiFi 兼容性较差）

## 使用 Flash Download Tool 烧录

> [Flash Download Tool](https://www.espressif.com/en/support/download/other-tools) 是乐鑫官方的 Windows GUI 烧录工具，适合不熟悉命令行的用户。

### 准备工作

1. 下载并安装 [Flash Download Tool](https://www.espressif.com/en/support/download/other-tools)（**ESP32 Flash Download Tool** 版本）
2. 下载对应芯片的 `XXX_v1.0.0.zip` 并解压，固件包含：
   - `bootloader.bin` — 引导程序
   - `partition-table.bin` — 分区表
   - `esp32_esurfing.bin` — 应用程序
   - `spiffs.bin` — SPIFFS 配置分区
3. 使用 USB 数据线连接开发板到电脑
4. 打开设备管理器确认串口号（如 COM3、COM14）

### 烧录步骤

**1. 打开 Flash Download Tool**

启动 `flash_download_tool_x.x.x.exe`，根据芯片型号选择：

| 你的芯片 | Chip Type 选择 |
|----------|---------------|
| ESP32-C3 SuperMini / ESP32-C3 | **ESP32-C3** |
| ESP32-S3 | **ESP32-S3** |
| ESP32 | **ESP32** |
| ESP32-S2 | **ESP32-S2** |

Load Mode 选择 **UART**，点击 **OK**。

**2. 配置烧录参数**

在所有 4 个固件行的输入框中填入地址和文件路径：

| Address | File |
|---------|------|
| `0x0` | `bootloader.bin` |
| `0x8000` | `partition-table.bin` |
| `0x10000` | `esp32_esurfing.bin` |
| `0x2D0000` | `spiffs.bin` |

底部参数设置（所有芯片统一）：

| 参数 | 值 |
|------|-----|
| SPI SPEED | `80MHz` |
| SPI MODE | `DIO`（ESP32-C3） / `QIO`（ESP32/ESP32-S3） |
| FLASH SIZE | `4MB` |
| COM | 你的串口号 |
| BAUD | `460800` |

> SPI MODE: ESP32-C3 只能用 DIO，ESP32/ESP32-S3 可尝试 QIO 速度更快。

**3. 开始烧录**

- 勾选所有 4 个固件前面的 **√** 复选框
- 点击 **START** 按钮
- 等待进度条完成（约 10-15 秒）
- 状态显示 **FINISH** 即烧录完成

**4. 上电运行**

- 烧录完成后，按一下开发板的 **RST** 按钮（或重新上电）
- ESP32 会自动启动，手机搜索到 `ESurfing-Config` WiFi

### 烧录界面参考图

```
┌──────────────────────────────────────────────────────┐
│  Flash Download Tool  (ESP32-C3 + UART)              │
├──────────────────────────────────────────────────────┤
│  [√] bootloader.bin         ──  0x0                  │
│  [√] partition-table.bin    ──  0x8000               │
│  [√] esp32_esurfing.bin     ──  0x10000              │
│  [√] spiffs.bin             ──  0x2D0000             │
│                                                      │
│  SPI SPEED:  80MHz   ├───  SPI MODE:  DIO  ├──       │
│  FLASH SIZE: 4MB     ├───  COM:  COM14    ├──       │
│  BAUD:       460800  ├───                     │       │
│                                                      │
│  [Erase] [Com (x)]  [START]  [STOP]                  │
│                                                      │
│  Status: FINISH                                      │
└──────────────────────────────────────────────────────┘
```

### 注意事项

- **不要勾选** `DoNotChkBin` 或跳过校验，否则可能烧录后无法启动
- **先擦除再烧录**：如遇到旧数据干扰，先点 `Erase` 擦除全部 Flash，再重新烧录
- **USB 线必须是数据线**：充电线无法烧录
- **Windows 驱动**：ESP32-C3 SuperMini 使用内置 USB-Serial-JTAG，Win10/Win11 自动识别，无需额外驱动

## 配置说明

### Web 后台

| 字段 | 说明 | 示例 |
|------|------|------|
| WiFi SSID | 校园网 WiFi 名称 | `ChinaNet` |
| WiFi 密码 | 校园网 WiFi 密码 | 可留空 |
| 用户名 | 校园网账号 | 学号或手机号 |
| 密码 | 校园网密码 |  |
| 通道 | 认证类型 | `phone`（手机）或 `pc`（电脑） |

连接 `ESurfing-Config` → 浏览器访问 `http://192.168.4.1`

### SPIFFS 配置文件

`/spiffs/ESurfingClient.json` 为认证客户端读取的配置，`/spiffs/config.json` 为 Web 后台和 WiFi 配置共用。保存 Web 配置时会同步写入两个文件。

## 技术栈

| 组件 | 方案 |
|------|------|
| **主控** | ESP32-C3 (RISC-V, 160MHz) |
| **框架** | ESP-IDF v5.5 |
| **工具链** | Arduino RISC-V GCC (esp-rv32 2601-cn) |
| **HTTP** | esp_http_client |
| **Web 服务器** | esp_http_server |
| **配置存储** | SPIFFS (1.2MB 分区) |
| **加密库** | mbedTLS 3.x + 自实现 DES/3DES/SM4/ZUC |
| **JSON 解析** | cJSON |

## 认证协议

支持中国电信天翼校园网 CCTP 协议，包含以下加密算法：

AES-CBC, AES-ECB, 3DES-CBC, 3DES-ECB, DES-ECB(6轮), SM4-CBC, SM4-ECB, ZUC-128, ModXTEA 及其变体

## 项目结构

```
esp32_esurfing/
├── main/                          # 主程序
│   ├── app_main.c                 # 入口
│   ├── wifi_manager.c/h           # WiFi 管理（AP+STA 模式）
│   └── web_config.c/h             # Web 配置后台
├── components/
│   └── esurfing_core/             # 认证逻辑（移植自 CVersion）
│       ├── src/DialerClient.c     # 认证流程主控
│       ├── src/NetClient.c        # HTTP 请求
│       ├── src/States.c           # 状态管理
│       └── src/cipher/            # 17 种加密算法
├── spiffs_data/                   # SPIFFS 配置模板
│   ├── config.json                # Web 配置
│   └── ESurfingClient.json        # 认证客户端配置
├── build_v55.py                   # 构建脚本 (IDF v5.5)
└── sdkconfig.defaults             # ESP-IDF 配置
```

## 常见问题

### WiFi 连不上

- ESP32-C3 SuperMini PCB 天线信号较弱，确保靠近路由器
- 已在代码中降低 TX 功率（8.5 dBm），如仍不稳定可尝试 `esp_wifi_set_max_tx_power(28)`
- 确认 WiFi 密码正确，开放网络留空

### 认证失败

- 检查校园网账号密码是否正确
- 尝试切换通道（phone/pc）
- 查看串口日志定位原因

### 设备重启

- 保存配置后设备会自动重启以应用新配置
- 如异常重启，检查 USB 供电是否稳定

## 许可证

本项目基于 ESurfingClient-CVersion（BadGhost）的 GPL 许可进行移植分发。
