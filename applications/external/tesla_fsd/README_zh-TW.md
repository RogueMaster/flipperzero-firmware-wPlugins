[English](README.md) | [繁體中文](README_zh-TW.md)

# Tesla FSD 解鎖 — Flipper Zero

[![GitHub stars](https://img.shields.io/github/stars/hypery11/flipper-tesla-fsd?style=flat-square&logo=github)](https://github.com/hypery11/flipper-tesla-fsd/stargazers)
[![GitHub forks](https://img.shields.io/github/forks/hypery11/flipper-tesla-fsd?style=flat-square&logo=github)](https://github.com/hypery11/flipper-tesla-fsd/network)
[![GitHub release](https://img.shields.io/github/v/release/hypery11/flipper-tesla-fsd?style=flat-square&logo=github)](https://github.com/hypery11/flipper-tesla-fsd/releases)
[![Downloads](https://img.shields.io/github/downloads/hypery11/flipper-tesla-fsd/total?style=flat-square&logo=github)](https://github.com/hypery11/flipper-tesla-fsd/releases)
[![Last commit](https://img.shields.io/github/last-commit/hypery11/flipper-tesla-fsd?style=flat-square&logo=github)](https://github.com/hypery11/flipper-tesla-fsd/commits/main)
[![Open issues](https://img.shields.io/github/issues/hypery11/flipper-tesla-fsd?style=flat-square&logo=github)](https://github.com/hypery11/flipper-tesla-fsd/issues)
[![License: GPL-3.0](https://img.shields.io/badge/license-GPL--3.0-blue?style=flat-square)](LICENSE)

> **Tesla FSD 區域鎖繞過 — Flipper Zero 版。** 讓**已經有 FSD 訂閱或購買**但所在地區的車機不顯示「交通號誌與停車標誌控制」選項的車主，能透過 CAN bus 層面啟用 FSD UI 開關。支援 HW3、HW4、Legacy HW1/HW2 Model S/X，FSD v14 可用。另含 Nag 抑制、限速提示音消除、OTA 自動暫停、電池預熱觸發、BMS 即時儀表板（這些功能**不需要** FSD 訂閱就能使用）。硬體成本：Flipper Zero + Electronic Cats CAN Bus Add-On + OBD-II 線；或做 [ESP32 移植版](https://github.com/hypery11/flipper-tesla-fsd/tree/main/esp32)，總成本 ~$14 / ¥100。

> [!IMPORTANT]
> **FSD 相關功能必須有有效的 FSD 套件** — 購買或訂閱均可。此工具在 CAN bus 層面啟用 FSD 功能，但車輛仍需要來自 Tesla 的合法 FSD 授權。**這不是免費解鎖工具。**
>
> 如果你所在的地區無法訂閱 FSD，上游社群記錄了一個變通方法：在可訂閱 FSD 的地區（如加拿大）建立 Tesla 帳號，將車輛轉移到該帳號，然後訂閱 FSD。詳見[上游文件](https://gitlab.com/slxslx/tesla-open-can-mod-slx-repo)。
>
> Nag 抑制、限速提示音消除、BMS 儀表板、電池預熱等功能**無需 FSD 訂閱**，可獨立使用。

<p align="center">
  <img src="assets/demo.gif" alt="Tesla FSD 解鎖運作中 — 主選單、HW 偵測、BMS 即時儀表板" width="600">
</p>

<p align="center">
  <img src="screenshots/main_menu.png" alt="Flipper Zero Tesla FSD 主選單" width="256">&nbsp;&nbsp;&nbsp;
  <img src="screenshots/fsd_running.png" alt="Tesla FSD 解鎖運作中" width="256">
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

- 自動偵測 HW3/HW4（從 `GTW_carConfig` `0x398` legacy / `0x7FF` Ethernet 讀取），也可手動強制指定 — **注意：** 2020 後 Model 3/Y HW3/HW4 的 `0x398` 在 Ethernet bus 上，CAN bus 可能看不到；遇到偵測不到的情況請用 Force HW3 或 Force HW4
- 透過修改 `UI_autopilotControl`（`0x3FD`）的 bit 來啟用 FSD
- Nag 抑制（消除方向盤握手提醒）
- 速度檔位預設最快，自動從跟車距離撥桿同步
- Flipper 螢幕即時顯示狀態

### 支援硬體

| Tesla HW | 修改的 Bits | 速度檔位 |
|----------|------------|----------|
| HW3 | bit46 | 3 段（0-2） |
| HW4（FSD V14+） | bit46 + bit60、bit47 | 5 段（0-4） |

HW4 車輛韌體版本 **2026.2.3 以前**請使用 HW3 模式。詳見[相容性](#相容性)。

---

## 硬體需求

| 元件 | 說明 | 價格 |
|------|------|------|
| [Flipper Zero](https://flipper.net/) | 本體 | ~$170 |
| [Electronic Cats CAN Bus Add-On](https://electroniccats.com/store/flipper-addon-canbus/) | MCP2515 CAN 收發器模組 | ~$30 |
| OBD-II 線或 T-tap | 接到 Tesla 的 Party CAN bus | ~$10 |

### 接線

<p align="center">
  <img src="images/wiring_diagram.png" alt="接線圖" width="700">
</p>

> **終端電阻：** Electronic Cats 這塊 Add-On 有兩個版本。v0.1 預設啟用 120 Ω 終端，要把板子背面靠近 SN65HVD230 的 `J1 / TERM` solder jumper 切開。v0.2+ 預設已經是斷開狀態，不用動。**接車前**先用三用電表量 CAN-H 跟 CAN-L 兩個 pin 之間的電阻：~120 Ω = 好（terminator 關閉），~60 Ω = 要切斷 jumper，無限大 = 也沒問題。完整說明見 [`HARDWARE.md`](HARDWARE.md#termination-resistor--important-detail)。

替代接點：後座中控台內的 **X179 診斷接頭**（20-pin 版 Pin 13/14 = CAN-H/L；26-pin 版 Pin 18/19）。

### 其他支援的硬體

不想買 Flipper Zero？PR [#6](https://github.com/hypery11/flipper-tesla-fsd/pull/6) 提供完整 ESP32 移植版，整套硬體成本壓到 **~$14 / ¥100**，內建 WiFi 網頁儀表板。Aliexpress 上 ¥30 的通用 MCP2515 模組也能搭 Flipper Zero 用，自己拉幾條跳線就行。完整對照表見 [`HARDWARE.md`](HARDWARE.md)。

---

## 安裝

### 方法一：下載編譯好的 FAP

1. 到 [Releases](https://github.com/hypery11/flipper-tesla-fsd/releases) 頁面
2. 下載 `tesla_fsd.fap`
3. 複製到 Flipper 的 SD 卡：`SD Card/apps/GPIO/tesla_fsd.fap`

### 方法二：自行編譯

```bash
# Clone Flipper Zero 韌體
git clone --recursive https://github.com/flipperdevices/flipperzero-firmware.git
cd flipperzero-firmware

# Clone 本 app 到 applications_user
git clone https://github.com/hypery11/flipper-tesla-fsd.git applications_user/tesla_fsd

# 編譯
./fbt fap_tesla_fsd

# 燒錄到 Flipper
./fbt launch app=tesla_fsd
```

---

## 使用方式

1. 把 CAN Add-On 插上 Flipper Zero
2. 用 CAN-H/CAN-L 接到車上 OBD-II 口
3. 開啟 app：`Apps > GPIO > Tesla FSD`
4. 選 **「Auto Detect & Start」**（或手動選 HW3/HW4）
5. 等待偵測（最多 8 秒）
6. App 自動開始修改 CAN frame

### 螢幕顯示

```
  Tesla FSD Active
  HW: HW4    Profile: 4/4
  FSD: ON    Nag: OFF
  Frames modified: 12345
       [BACK] to stop
```

### 啟動觸發條件

車上 Autopilot 設定中的 **「交通號誌與停車標誌控制」** 開啟時，app 才會開始修改 frame。這個旗標是 CAN frame 裡的判斷依據。

---

## 相容性

| 車型 | HW | 韌體 | 模式 | 狀態 |
|------|----|------|------|------|
| Model 3 / Y（2019-2023） | HW3 | 任何 | Auto | 支援 |
| Model 3 / Y（2023+） | HW4 | `< 2026.2.3` | Force HW3 | 支援 |
| Model 3 / Y（2023+） | HW4 | `2026.2.3` ↔ `2026.2.8` | Auto | 支援 |
| Model 3 / Y（2023+） | HW4 | `2026.2.9.x`（FSD v14） | Auto | 支援 |
| Model 3 / Y（2023+） | HW4 | `2026.2.10` ↔ `2026.4.x` | Auto | 支援 |
| Model 3 / Y（2023+） | HW4 | `2026.8.6` | **Force HW3** | HW4 path 在這個版本壞掉，要強制 HW3 |
| Model 3 Highland（2024+） | HW4 | `2026.2.x` | Auto | 已有運作回報 — 需更多確認 |
| Model 3 / Y（中規 MIC） | HW3 / HW4 | `2026.2.11` | Auto + Force FSD | 已有運作回報 — 見 issue #1, #4, #7 |
| Model S / X（2021+） | HW4 | `>= 2026.2.3`（除 2026.8.6） | Auto | 支援 |
| Model S / X（2016-2019） | HW1 / HW2 | 任何 | Legacy | v2.0 已實作，**待上車驗證** |

### 社群測試回報

實車回報（用 [Car compatibility report](https://github.com/hypery11/flipper-tesla-fsd/issues/new?template=car_compatibility.yml) issue template 自己回報）：

| 回報者 | 車 | HW | 韌體 | 地區 | 模式 | 結果 |
|--------|----|----|------|------|------|------|
| @vbarrier | Model 3 | HW4 | 2026.4.x | 歐洲 | Auto | 運作 |
| @kwangseok73-sudo | Model 3 | HW4 | 2026.2.x | 韓國 | Force FSD | 運作 |
| @andreiboestean | Model 3 | HW4 | 2026.2.9.3（FSD v14） | 歐洲 | Auto | 運作 |
| Marow | Model Y Juniper | HW4 | 2026.8.6 | 歐洲 | (Force HW3 尚未測試) | 顯示「Region not available」→ 用 Force FSD + Force HW3 |

### HW1/HW2 Legacy 支援 — 徵求志願者

舊款 Model S/X（2016-2019）使用 Mobileye 架構，CAN ID 完全不同。Autopilot 控制 frame 在 `0x3EE`（1006）而非 `0x3FD`（1021），bit 排列也不一樣。

邏輯記錄在 [Karolynaz/waymo-fsd-can-mod](https://github.com/Karolynaz/waymo-fsd-can-mod) 這個 CanFeather 鏡像（原始 `Starmixcraft/tesla-fsd-can-mod` GitLab 上游已被下架）。但我們需要有 HW1/HW2 車的人幫忙驗證才能上線。

**如果你有 2016-2019 Model S/X 且有 FSD，想幫忙的話：**

1. Flipper + CAN Add-On 接上 OBD-II
2. 開啟內建的 CAN sniffer app
3. 確認 CAN ID `0x3EE`（1006）有出現在 bus 上
4. 擷取幾個 frame，貼到 [issue #1](https://github.com/hypery11/flipper-tesla-fsd/issues/1)

驗證通過後，Legacy 支援很快就能加上。

---

## 運作原理

在 Party CAN（Bus 0）上做單 bus 的讀取-修改-重發。不需要 MITM，不用接第二條 bus。

1. ECU 在 Bus 0 上發出 `UI_autopilotControl`（`0x3FD`）
2. Flipper 收到，改掉 FSD 啟用 bit
3. Flipper 重發修改版 — 接收端採用最新的 frame

### 使用的 CAN ID

| CAN ID | 名稱 | 用途 |
|--------|------|------|
| `0x398` | `GTW_carConfig` | HW 偵測（`GTW_dasHw` byte0 bit6-7） |
| `0x3F8` | Follow Distance | 速度檔位來源（byte5 bit5-7） |
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

## 相關專案

| 專案 | 是什麼 | 硬體 |
|------|--------|------|
| [slxslx/tesla-open-can-mod-slx-repo](https://gitlab.com/slxslx/tesla-open-can-mod-slx-repo) | 原始 Tesla-OPEN-CAN-MOD 上游 namespace 被下架後事實上的接班 fork。範圍更廣 — 「general CAN mod tool, not just FSD」 | Adafruit RP2040 CAN、Feather M4、ESP32、M5Stack ATOMIC CAN |
| ESP32 移植 — PR [#6](https://github.com/hypery11/flipper-tesla-fsd/pull/6) by @elonleo | 把本專案 CAN 邏輯完整移植到 ESP32，內建 WiFi 網頁儀表板。~$14 的 Flipper + Add-On 替代方案 | M5Stack ATOM Lite + ATOMIC CAN、Waveshare ESP32-S3-RS485-CAN |
| [tumik/S3XY-candump](https://github.com/tumik/S3XY-candump) | 用 enhauto S3XY Commander 當 Panda-protocol bridge 透過 WiFi dump 整條 Tesla CAN bus 的 Python 工具 | Commander dongle |
| [dzid26/ESP32-DualCAN](https://github.com/dzid26/ESP32-DualCAN) | 「Dorky Commander」— 開源硬體版的 enhauto S3XY Commander | ESP32 + dual CAN |
| [Karolynaz/waymo-fsd-can-mod](https://github.com/Karolynaz/waymo-fsd-can-mod) | 原始 `Starmixcraft/tesla-fsd-can-mod` CanFeather 研究的鏡像 — 我們移植的源頭。原始 GitLab 上游已被下架，這是目前還能看的版本。 | Adafruit Feather M4 CAN |
| [tuncasoftbildik/tesla-can-mod](https://github.com/tuncasoftbildik/tesla-can-mod) | Arduino 參考實作，含多個非 FSD 功能的 frame template | Arduino + MCP2515 |

## 致謝

- [commaai/opendbc](https://github.com/commaai/opendbc) — Tesla CAN 訊號資料庫
- [ElectronicCats/flipper-MCP2515-CANBUS](https://github.com/ElectronicCats/flipper-MCP2515-CANBUS) — Flipper 用 MCP2515 驅動
- `Starmixcraft/tesla-fsd-can-mod` — 原始 CanFeather FSD 研究（GitLab 上已被下架，鏡像在 [Karolynaz/waymo-fsd-can-mod](https://github.com/Karolynaz/waymo-fsd-can-mod)）
- mikegapinski/tesla-can-explorer — 從 Tesla 主機 `libQtCarVAPI.so` 萃取的 4 萬個 Tesla CAN 訊號字典
- talas9/tesla_can_signals — 各車型 wire format 對照

## 授權

GPL-3.0

## 免責聲明

僅供教育與研究用途。**FSD 是 Tesla 的付費功能，必須合法購買或訂閱使用。** 改裝車輛系統可能導致保固失效，也可能違反當地法規。使用者需自行承擔所有責任與風險。完整安全與責任使用說明見 [`SECURITY.md`](SECURITY.md)。
