[English](README.md) | [繁體中文](README_zh-TW.md) | [简体中文](README_zh-CN.md)

# Tesla Mod — Flipper Zero

[![GitHub stars](https://img.shields.io/github/stars/hypery11/flipper-tesla-fsd?style=flat-square&logo=github)](https://github.com/hypery11/flipper-tesla-fsd/stargazers)
[![GitHub forks](https://img.shields.io/github/forks/hypery11/flipper-tesla-fsd?style=flat-square&logo=github)](https://github.com/hypery11/flipper-tesla-fsd/network)
[![GitHub release](https://img.shields.io/github/v/release/hypery11/flipper-tesla-fsd?style=flat-square&logo=github)](https://github.com/hypery11/flipper-tesla-fsd/releases)
[![Downloads](https://img.shields.io/github/downloads/hypery11/flipper-tesla-fsd/total?style=flat-square&logo=github)](https://github.com/hypery11/flipper-tesla-fsd/releases)
[![Last commit](https://img.shields.io/github/last-commit/hypery11/flipper-tesla-fsd?style=flat-square&logo=github)](https://github.com/hypery11/flipper-tesla-fsd/commits/main)
[![Open issues](https://img.shields.io/github/issues/hypery11/flipper-tesla-fsd?style=flat-square&logo=github)](https://github.com/hypery11/flipper-tesla-fsd/issues)
[![License: GPL-3.0](https://img.shields.io/badge/license-GPL--3.0-blue?style=flat-square)](LICENSE)

> **Tesla FSD 区域锁绕过 — Flipper Zero 版。** 讓**已經有 FSD 订阅或购买**但所在地区的车机不显示「交通信号灯与停车标志控制」选項的车主，能透過 CAN bus 层面启用 FSD UI 开关。支持 HW3、HW4、Legacy HW1/HW2 Model S/X，FSD v14 可用。另含 Nag 抑制、限速提示音消除、OTA 自动暂停、电池预热触发、BMS 实时仪表板（这些功能**不需要** FSD 訂閱就能使用）。硬件成本：Flipper Zero + Electronic Cats CAN Bus Add-On + OBD-II 線；或做 [ESP32 移植版](https://github.com/hypery11/flipper-tesla-fsd/tree/main/esp32)，总成本 ~$14 / ¥100。

> [!IMPORTANT]
> **FSD 相关功能必须有有效的 FSD 套件** — 购买或订阅均可。此工具在 CAN bus 层面启用 FSD 功能，但车辆仍需要来自 Tesla 的合法 FSD 授权。**这不是免费解锁工具。**
>
> 如果你所在的地区無法訂閱 FSD，上游社群記錄了一個变通方法：在可訂閱 FSD 的地区（如加拿大）创建 Tesla 账号，將車輛轉移到該账号，然后订阅 FSD。详见[上游文档](https://gitlab.com/slxslx/tesla-open-can-mod-slx-repo)。
>
> Nag 抑制、限速提示音消除、BMS 仪表板、電池預熱等功能**无需 FSD 訂閱**，可独立使用。

<p align="center">
  <img src="assets/demo.gif" alt="Tesla FSD 解锁运行中 — 主菜单、HW 检测、BMS 实时仪表板" width="600">
</p>

<p align="center">
  <img src="screenshots/main_menu.png" alt="Flipper Zero Tesla FSD 主菜单" width="256">&nbsp;&nbsp;&nbsp;
  <img src="screenshots/fsd_running.png" alt="Tesla FSD 解锁运行中" width="256">
</p>

<p align="center">
  <a href="https://star-history.com/#hypery11/flipper-tesla-fsd&Date">
    <img src="https://api.star-history.com/svg?repos=hypery11/flipper-tesla-fsd&type=Date" alt="Star history" width="600">
  </a>
</p>

<p align="center">
  <a href="https://github.com/hypery11/flipper-tesla-fsd/graphs/contributors">
    <img src="https://contrib.rocks/image?repo=hypery11/flipper-tesla-fsd" alt="Contributors">
  </a>
</p>

---

## 功能

- 自動检测 HW3/HW4（從 `GTW_carConfig` `0x398` legacy / `0x7FF` Ethernet 讀取），也可手動強制指定 — **注意：** 2020 後 Model 3/Y HW3/HW4 的 `0x398` 在 Ethernet bus 上，CAN bus 可能看不到；遇到检测不到的情況請用 Force HW3 或 Force HW4
- 通过修改 `UI_autopilotControl`（`0x3FD`）的 bit 來启用 FSD
- Nag 抑制（消除方向盘握手提醒）
- 速度档位默认最快，自動從跟车距离拨杆同步
- Flipper 屏幕实时显示状态

### 支持硬件

| Tesla HW | 修改的 Bits | 速度档位 |
|----------|------------|----------|
| HW3 | bit46 | 3 段（0-2） |
| HW4（FSD V14+） | bit46 + bit60、bit47 | 5 段（0-4） |

HW4 車輛固件版本 **2026.2.3 以前**請使用 HW3 模式。详见[兼容性](#兼容性)。

---

## 硬件需求

| 组件 | 说明 | 价格 |
|------|------|------|
| [Flipper Zero](https://flipper.net/) | 本體 | ~$170 |
| [Electronic Cats CAN Bus Add-On](https://electroniccats.com/store/flipper-addon-canbus/) | MCP2515 CAN 收發器模組 | ~$30 |
| OBD-II 線或 T-tap | 接到 Tesla 的 Party CAN bus | ~$10 |

### 接线

<p align="center">
  <img src="images/wiring_diagram.png" alt="接线圖" width="700">
</p>

> **终端电阻：** Electronic Cats 這塊 Add-On 有兩個版本。v0.1 預設启用 120 Ω 終端，要把板子背面靠近 SN65HVD230 的 `J1 / TERM` solder jumper 切開。v0.2+ 預設已經是斷開状态，不用動。**接車前**先用三用電表量 CAN-H 跟 CAN-L 兩個 pin 之間的電阻：~120 Ω = 好（terminator 關閉），~60 Ω = 要切斷 jumper，無限大 = 也沒問題。完整说明見 [`HARDWARE.md`](HARDWARE.md#termination-resistor--important-detail)。

替代接点：后座中控台內的 **X179 诊断接头**（20-pin 版 Pin 13/14 = CAN-H/L；26-pin 版 Pin 18/19）。

### 其他支持的硬件

不想買 Flipper Zero？PR [#6](https://github.com/hypery11/flipper-tesla-fsd/pull/6) 提供完整 ESP32 移植版，整套硬件成本壓到 **~$14 / ¥100**，內建 WiFi 網頁仪表板。Aliexpress 上 ¥30 的通用 MCP2515 模組也能搭 Flipper Zero 用，自己拉幾條跳線就行。完整对照表见 [`HARDWARE.md`](HARDWARE.md)。

---

## 安装

### 方法一：下载编译好的 FAP

1. 到 [Releases](https://github.com/hypery11/flipper-tesla-fsd/releases) 页面
2. 下载 `tesla_fsd.fap`
3. 复制到 Flipper 的 SD 卡：`SD Card/apps/GPIO/tesla_fsd.fap`

### 方法二：自行编译

```bash
# Clone Flipper Zero 固件
git clone --recursive https://github.com/flipperdevices/flipperzero-firmware.git
cd flipperzero-firmware

# Clone 本 app 到 applications_user
git clone https://github.com/hypery11/flipper-tesla-fsd.git applications_user/tesla_fsd

# 编译
./fbt fap_tesla_fsd

# 烧录到 Flipper
./fbt launch app=tesla_fsd
```

---

## 使用方式

1. 把 CAN Add-On 插上 Flipper Zero
2. 用 CAN-H/CAN-L 接到車上 OBD-II 口
3. 打开 app：`Apps > GPIO > Tesla FSD`
4. 选 **「Auto Detect & Start」**（或手動选 HW3/HW4）
5. 等待检测（最多 8 秒）
6. App 自動開始修改 CAN frame

### 螢幕顯示

```
  Tesla FSD Active
  HW: HW4    Profile: 4/4
  FSD: ON    Nag: OFF
  Frames modified: 12345
       [BACK] to stop
```

### 啟動触发条件

車上 Autopilot 设置中的 **「交通信号灯与停车标志控制」** 打开時，app 才會開始修改 frame。這個标志是 CAN frame 裡的判断依据。

---

## 兼容性

| 车型 | HW | 固件 | 模式 | 状态 |
|------|----|------|------|------|
| Model 3 / Y（2019-2023） | HW3 | 任何 | Auto | 支持 |
| Model 3 / Y（2023+） | HW4 | `< 2026.2.3` | Force HW3 | 支持 |
| Model 3 / Y（2023+） | HW4 | `2026.2.3` ↔ `2026.2.8` | Auto | 支持 |
| Model 3 / Y（2023+） | HW4 | `2026.2.9.x`（FSD v14） | Auto | 支持 |
| Model 3 / Y（2023+） | HW4 | `2026.2.10` ↔ `2026.4.x` | Auto | 支持 |
| Model 3 / Y（2023+） | HW4 | `2026.8.6` | **Force HW3** | HW4 path 在這個版本壞掉，要強制 HW3 |
| Model 3 Highland（2024+） | HW4 | `2026.2.x` | Auto | 已有运行报告 — 需更多确认 |
| Model 3 / Y（中規 MIC） | HW3 / HW4 | `2026.2.11` | Auto + Force FSD | 已有运行报告 — 見 issue #1, #4, #7 |
| Model S / X（2021+） | HW4 | `>= 2026.2.3`（除 2026.8.6） | Auto | 支持 |
| Model S / X（2016-2019） | HW1 / HW2 | 任何 | Legacy | v2.0 已實作，**待上車驗證** |

### 社群测试报告

實車报告（用 [Car compatibility report](https://github.com/hypery11/flipper-tesla-fsd/issues/new?template=car_compatibility.yml) issue template 自己报告）：

| 报告者 | 車 | HW | 固件 | 地区 | 模式 | 结果 |
|--------|----|----|------|------|------|------|
| @vbarrier | Model 3 | HW4 | 2026.4.x | 欧洲 | Auto | 运行 |
| @kwangseok73-sudo | Model 3 | HW4 | 2026.2.x | 韩国 | Force FSD | 运行 |
| @andreiboestean | Model 3 | HW4 | 2026.2.9.3（FSD v14） | 欧洲 | Auto | 运行 |
| Marow | Model Y Juniper | HW4 | 2026.8.6 | 欧洲 | (Force HW3 尚未测试) | 顯示「Region not available」→ 用 Force FSD + Force HW3 |

### HW1/HW2 Legacy 支持 — 徵求志願者

舊款 Model S/X（2016-2019）使用 Mobileye 架構，CAN ID 完全不同。Autopilot 控制 frame 在 `0x3EE`（1006）而非 `0x3FD`（1021），bit 排列也不一樣。

邏輯記錄在 [Karolynaz/waymo-fsd-can-mod](https://github.com/Karolynaz/waymo-fsd-can-mod) 這個 CanFeather 鏡像（原始 `Starmixcraft/tesla-fsd-can-mod` GitLab 上游已被下架）。但我們需要有 HW1/HW2 車的人幫忙驗證才能上線。

**如果你有 2016-2019 Model S/X 且有 FSD，想幫忙的話：**

1. Flipper + CAN Add-On 接上 OBD-II
2. 打开內建的 CAN sniffer app
3. 确认 CAN ID `0x3EE`（1006）有出現在 bus 上
4. 擷取幾個 frame，貼到 [issue #1](https://github.com/hypery11/flipper-tesla-fsd/issues/1)

驗證通過後，Legacy 支持很快就能加上。

---

## 运行原理

在 Party CAN（Bus 0）上做單 bus 的讀取-修改-重發。不需要 MITM，不用接第二條 bus。

1. ECU 在 Bus 0 上發出 `UI_autopilotControl`（`0x3FD`）
2. Flipper 收到，改掉 FSD 启用 bit
3. Flipper 重發修改版 — 接收端採用最新的 frame

### 使用的 CAN ID

| CAN ID | 名稱 | 用途 |
|--------|------|------|
| `0x398` | `GTW_carConfig` | HW 检测（`GTW_dasHw` byte0 bit6-7） |
| `0x3F8` | Follow Distance | 速度档位來源（byte5 bit5-7） |
| `0x3FD` | `UI_autopilotControl` | FSD 解鎖目標（mux 0/1/2） |

---

## 常見問題

**拔掉之後 FSD 還會維持嗎？**
不會。這是即時 frame 修改，拔掉就恢復原樣。

**會不會把車搞壞？**
只動 UI 設定 frame，不碰煞車、轉向、動力系統。但風險自負。

**一定要 CAN Add-On 嗎？**
對。Flipper 沒有內建 CAN bus，你需要 Electronic Cats 的板子或任何 MCP2515 模組接在 GPIO 上。

---

## 相关项目

| 專案 | 是什麼 | 硬件 |
|------|--------|------|
| [slxslx/tesla-open-can-mod-slx-repo](https://gitlab.com/slxslx/tesla-open-can-mod-slx-repo) | 原始 Tesla-OPEN-CAN-MOD 上游 namespace 被下架後事實上的接班 fork。範圍更廣 — 「general CAN mod tool, not just FSD」 | Adafruit RP2040 CAN、Feather M4、ESP32、M5Stack ATOMIC CAN |
| ESP32 移植 — PR [#6](https://github.com/hypery11/flipper-tesla-fsd/pull/6) by @elonleo | 把本專案 CAN 邏輯完整移植到 ESP32，內建 WiFi 網頁仪表板。~$14 的 Flipper + Add-On 替代方案 | M5Stack ATOM Lite + ATOMIC CAN、Waveshare ESP32-S3-RS485-CAN |
| [tumik/S3XY-candump](https://github.com/tumik/S3XY-candump) | 用 enhauto S3XY Commander 當 Panda-protocol bridge 透過 WiFi dump 整條 Tesla CAN bus 的 Python 工具 | Commander dongle |
| [dzid26/ESP32-DualCAN](https://github.com/dzid26/ESP32-DualCAN) | 「Dorky Commander」— 開源硬件版的 enhauto S3XY Commander | ESP32 + dual CAN |
| [Karolynaz/waymo-fsd-can-mod](https://github.com/Karolynaz/waymo-fsd-can-mod) | 原始 `Starmixcraft/tesla-fsd-can-mod` CanFeather 研究的镜像 — 我們移植的源头。原始 GitLab 上游已被下架，這是目前还能看的版本。 | Adafruit Feather M4 CAN |
| [tuncasoftbildik/tesla-can-mod](https://github.com/tuncasoftbildik/tesla-can-mod) | Arduino 參考實作，含多個非 FSD 功能的 frame template | Arduino + MCP2515 |

## 致谢

- [commaai/opendbc](https://github.com/commaai/opendbc) — Tesla CAN 信号数据库
- [ElectronicCats/flipper-MCP2515-CANBUS](https://github.com/ElectronicCats/flipper-MCP2515-CANBUS) — Flipper 用 MCP2515 驱动
- `Starmixcraft/tesla-fsd-can-mod` — 原始 CanFeather FSD 研究（GitLab 上已被下架，鏡像在 [Karolynaz/waymo-fsd-can-mod](https://github.com/Karolynaz/waymo-fsd-can-mod)）
- mikegapinski/tesla-can-explorer — 從 Tesla 主機 `libQtCarVAPI.so` 萃取的 4 萬個 Tesla CAN 訊號字典
- talas9/tesla_can_signals — 各车型 wire format 對照

## 授权

GPL-3.0

## 免责声明

仅供教育与研究用途。**FSD 是 Tesla 的付费功能，必须合法购买或订阅使用。** 改装车辆系统可能导致保修失效，也可能违反当地法规。使用者需自行承担所有责任与风险。完整安全與責任使用说明見 [`SECURITY.md`](SECURITY.md)。
