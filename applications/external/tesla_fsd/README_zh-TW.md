[English](README.md) | [繁體中文](README_zh-TW.md) | [简体中文](README_zh-CN.md)

> [!WARNING]
> **本翻譯可能落後於英文版。** 功能描述、CAN ID 表、硬體接線指南等以 [英文 README](README.md) 為準。如果你發現翻譯與英文版不一致，歡迎提交 PR 修正。

# Tesla Mod for Flipper Zero

[![GitHub stars](https://img.shields.io/github/stars/hypery11/flipper-tesla-fsd?style=flat-square&logo=github)](https://github.com/hypery11/flipper-tesla-fsd/stargazers)
[![GitHub forks](https://img.shields.io/github/forks/hypery11/flipper-tesla-fsd?style=flat-square&logo=github)](https://github.com/hypery11/flipper-tesla-fsd/network)
[![GitHub release](https://img.shields.io/github/v/release/hypery11/flipper-tesla-fsd?style=flat-square&logo=github)](https://github.com/hypery11/flipper-tesla-fsd/releases)
[![Downloads](https://img.shields.io/github/downloads/hypery11/flipper-tesla-fsd/total?style=flat-square&logo=github)](https://github.com/hypery11/flipper-tesla-fsd/releases)
[![Last commit](https://img.shields.io/github/last-commit/hypery11/flipper-tesla-fsd?style=flat-square&logo=github)](https://github.com/hypery11/flipper-tesla-fsd/commits/main)
[![Open issues](https://img.shields.io/github/issues/hypery11/flipper-tesla-fsd?style=flat-square&logo=github)](https://github.com/hypery11/flipper-tesla-fsd/issues)
[![PRs welcome](https://img.shields.io/badge/PRs-welcome-brightgreen?style=flat-square)](CONTRIBUTING.md)
[![License: GPL-3.0](https://img.shields.io/badge/license-GPL--3.0-blue?style=flat-square)](LICENSE)
[![Build](https://img.shields.io/badge/build-ufbt-brightgreen?style=flat-square)](https://github.com/flipperdevices/flipperzero-ufbt)
[![Flipper target](https://img.shields.io/badge/Flipper%20target-7%20%2F%20API%2087.1-orange?style=flat-square)](https://github.com/flipperdevices/flipperzero-firmware)
[![Tracked on FSD CAN Mod Hub](https://img.shields.io/badge/tracked%20on-FSD%20CAN%20Mod%20Hub-orange?style=flat-square)](https://fsdcanmod.com/project/hypery11-flipper-zero)

> **開源 Tesla CAN bus 工具組，支援 Flipper Zero 與 ESP32。** FSD 區域鎖繞過、給 VIN 被封禁車輛的 TLSSC Restore、帶擬真扭力變化的 nag killer、GTW Config Replay、BMS 即時儀表板，以及橫跨 Model 3、Model Y、Model S、Model X 的 30+ 個 CAN handler。支援 HW3、HW4 與 Legacy HW1/HW2。$200+ 的 S3XY Commander 的免費替代方案 — 搭配 [ESP32 移植版](https://github.com/hypery11/flipper-tesla-fsd/tree/main/esp32) 總成本最低只要 **$14**。

> [!IMPORTANT]
> **FSD 相關功能需要有效的 FSD 套件** — 購買或訂閱皆可。此工具在 CAN bus 層面啟用 FSD 功能，但車輛仍需要來自 Tesla 的合法 FSD 授權。非 FSD 功能（nag killer、BMS 儀表板、診斷）無需任何訂閱即可使用。

> [!CAUTION]
> **Tesla 已開始實施 VIN 層級封禁**（2026 年 4 月）。受影響的車輛會靜默失去 TLSSC 開關 — 沒有 OTA、沒有警告，且在帳號轉移與重新訂閱後依然存在。**TLSSC Restore** 功能（v2.10+）可透過 0x331 DAS 設定偽造，在被封禁的 Palladium 與 HW4 車上恢復停車標誌／交通號誌控制。完整封禁研究見 [SECURITY.md](SECURITY.md) 與 [issue #18](https://github.com/hypery11/flipper-tesla-fsd/issues/18)。

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

### 核心 FSD
- 從 `GTW_carConfig`（`0x398`）自動偵測 HW3/HW4；當所接的匯流排上沒有 `0x398` 時，改用 `0x3FD`/`0x399`/`0x3EE` 備援偵測
- **Legacy→HW3 自動升級**（Palladium Model S/X）— 先偵測到 `das_hw=0`，之後當 `0x3FD` 出現在匯流排上時升級
- 透過修改 `UI_autopilotControl`（`0x3FD` / `0x3EE`）的 bit 來解鎖 FSD
- **Legacy 模式**，支援 HW1/HW2（Model S/X 2016-2019）
- 速度檔位預設最快，並從跟車距離撥桿同步

### TLSSC Restore（v2.10+）
- 在 **VIN 被封禁** 的車輛上恢復交通號誌與停車標誌控制
- 對 CAN ID `0x331` 做讀取-修改-重發 — 將 `DAS_autopilot` 設為 SELF_DRIVING
- 已在 Palladium（Model S Plaid 2023）、HW4 Highland（Model 3 Performance 2024）與 Intel HW3（需 AP-first 變通）上確認可用
- 不會恢復完整 FSD 視覺化 — 只恢復 TLSSC（停車標誌／交通號誌）
- **建議的封禁車組合**：同時啟用 **TLSSC Restore** + **TLSSC bit38**（`0x3FD` mux 0 bit 38）— @RoyRakete 在 HW3 / 2026.2.6 上確認可靠（[#18](https://github.com/hypery11/flipper-tesla-fsd/issues/18#issuecomment-4413430516)）。在某些封禁韌體上，單獨開任一個都不穩定；兩者搭配才能重新啟用 AP/TACC 接管

### GTW Config Replay（v2.9+，v2.15 從「Ban Shield」改名）
- 監看 `GTW_carConfig`（`0x7FF`），當閘道器發出被修改的 frame 時，即時重播先前學到的健康匯流排廣播
- 第一次執行時學習全部 8 個 mux frame，之後自動武裝
- **它實際上做什麼：** 只在廣播層做遮罩。武裝後，AP ECU 看到的是重播的健康 frame，而不是閘道器修改過的那個。Tesla 的封禁會寫入 GTW NVRAM（重開機後仍在）與伺服器端旗標；本功能不會還原 NVRAM 狀態或後端紀錄，只影響其他匯流排上的 ECU 即時看到的內容。
- **它不做什麼：** 不能預防封禁、不能解除封禁、也不能改變 Tesla 伺服器端的授權紀錄。在 v2.9-v2.14 部署的 6 週內，沒有任何實證確認可預防封禁。誠實說明見 [#60](https://github.com/hypery11/flipper-tesla-fsd/issues/60) 與 [#67](https://github.com/hypery11/flipper-tesla-fsd/issues/67)。v2.14 的名字「Ban Shield」過度承諾了 — v2.15 改名反映程式碼實際的行為。

### Nag Killer（v2.1+）
- DAS 感知閘門 — 只在 DAS 真的要求手扶方向盤時才回應，DAS 滿足時零匯流排流量
- 擬真扭力變化 — 在 1.00-2.40 Nm 之間用 xorshift32 PRNG 隨機漫步，每 5-9 秒有一次到 3.10-3.30 Nm 的握力脈衝
- **按需握力脈衝（v2.15+）** — 當 `handsOnLevel` 升到提醒需求狀態（0 即將 / 3 升級）時，立即發出一次握力脈衝並重置週期排程。補上 v2.14 及更早版本在排程脈衝之間可能出現的 2 秒黃色升級空窗
- 在 `0x370` 做 EPAS counter+1 回應，並抑制 level 0（提醒即將出現）與 level 3（升級警報）
- **請接 Party CAN（X179 pin 2/3）給 nag killer。** `0x370` 在 Party CAN — 不在 Vehicle CAN（9/10），而閘道器轉送的 Chassis 副本（13/14）會觸發 2026.14.x preflight。已在 HW4 2026.20 上以接 2/3 確認可用（[#100](https://github.com/hypery11/flipper-tesla-fsd/issues/100)）。接錯線對的單 CAN 板子沒有東西可回應 — 這是「nag killer 在 HW4 上沒反應」最常見的原因。用車上的 **Service Mode → CAN Port** 頁面確認你這條線束上哪一對是 Party；見 [HARDWARE.md](HARDWARE.md)。

### AP-First 模式（v2.14+，給 2026.14.x 韌體）
- Tesla 2026.14.x 新增了 preflight 檢查，若 CAN 注入已在進行就擋下 AP/TACC 接管
- 啟用 **AP-First** 後，app 監看 `0x39B` 的 `DAS_autopilotState`，只在 AP 接管後才開始注入 `0x3FD`。在 ESP32 上，DAS 狀態來源會依偵測到的 HW 版本而定。
- Nag killer、TLSSC Restore 與 GTW Config Replay 不受影響（它們針對不同的 CAN ID）

### 14.x 韌體警告（v2.15+）
- **預設開啟。** 只要啟用警告開關，Flipper 執行畫面就會把 BMS / flags 那一行換成 `!14.x: TX may stop AP`。ESP32 網頁儀表板則在頂端顯示可關閉的黃色橫幅。
- 悲觀預設：大多數 14.x 韌體使用者要到自動轉向在行駛中脫離時才知道自己受影響。這個警告會在他們啟用任何 TX 功能之前先提醒到。
- 可透過 **On 14.x?** 設定開關（Flipper）或橫幅上的 **Dismiss** 按鈕（ESP32，存在 NVS）退出。若你確定是 pre-14.x 韌體就可關閉。
- 地區注意：執法強度因市場而異。部分地區（沒有 Tesla 直營的市場）似乎執法較不積極。14.x / 2026.20 的即時追蹤見 [#122](https://github.com/hypery11/flipper-tesla-fsd/issues/122)。

### 診斷（唯讀，不需要 FSD）
- BMS 即時儀表板：電池組電壓、電流、SoC、溫度範圍、**能耗（Wh/km）**
- 車速、方向盤角度、馬達扭力、煞車狀態
- DAS 狀態：autopilot 狀態、手扶提醒等級、變換車道狀態、盲點警示、FCW、視覺限速
- GTW autopilot 層級回讀（NONE/HIGHWAY/ENHANCED/SELF_DRIVING/BASIC）
- OTA 偵測含防抖 — 韌體更新期間自動暫停 TX，除非明確啟用 Ignore OTA 覆寫

### CAN Capture + 測試設定檔（v2.16+）
- **CAN Capture** — 將每個收到的 frame 以 candump 格式錄到 SD 卡（`apps_data/tesla_mod/captures/`）。唯讀；在任何車上執行都安全。可餵給 `tools/tesla_crc_cracker.py`。
- **Send Test** — 從 SD 卡載入使用者自訂的 `.cantest` 文字設定檔並重播你自己的 frame。預設為 dry-run；傳送硬性限制在 **停妥、靜止** 的車（fail-closed），且每個 frame 前都會重新檢查。結果會記錄以利回報 bug。格式與流程：[docs/cantest-format.md](docs/cantest-format.md)，範例：[examples/example.cantest](examples/example.cantest)。

### 額外解鎖（v2.16+，選用，預設關閉）
- **Summon EU Unlock** — `0x3FD` mux1：清掉 bit19（EU AP 限制）並設 bit47（summon-enable），在受 EU 限制的車上開放召喚（Summon）
- **Continue on Green** — `0x3FD` mux0 bit39 `UI_fsdContinueOnGreenWithCIPV` — 在有前車的情況下，不用撥桿確認就通過綠燈；搭配 TLSSC 使用
- **右駕（RHD）覆寫** — `0x3F8` bit41 `UI_drivingSide` = RHD。僅限右駕市場
- **AP 分支／層級選擇器** — `0x3FD` mux1 bits 40-42 `UI_apmv3Branch`：Live / Stage / Dev / Stage2 / EAP / Demo。實驗性，非持久化的 UI 提示 — 停止注入後即還原
- **可調 Track Mode** — `0x313` `UI_trackModeSettings`：操控平衡（Handling Balance）+ 穩定輔助（Stability Assist）+ 收車後冷卻，校驗和會重算。走 Vehicle 匯流排；預設為 rotation 100 / stability 30%，非 Performance 車型也可用

### 設定（執行時開關）

**穩定（已上車測試）：**

| 設定 | 說明 |
|------|------|
| **Mode** | `Active` / `Listen-Only` / `Service`。Listen-Only 是**首次開機的預設值** — MCP2515 處於硬體 listen-only 模式，實體上無法 TX。 |
| **Nag Killer** | DAS 感知的 EPAS counter+1 回應，帶擬真扭力變化。 |
| **Force FSD** | 繞過 `isFSDSelectedInUI` 檢查。不會繞過 Tesla 伺服器端授權 — 只影響本地 CAN frame 流。 |
| **Ignore OTA** | 即使 `0x318` 回報 Tesla OTA 更新進行中，也允許在 Active 模式下 CAN TX。預設關閉。 |
| **TLSSC Restore** | 0x331 DAS 設定偽造，在被封禁的車上恢復 TLSSC。會觸發 MCU 重開機。 |
| **AP-First (14.x)** | 延後 0x3FD 注入直到 AP 接管。Tesla 韌體 2026.14.x 需要此項。 |
| **GTW Config Replay** | 當閘道器發出被修改的 frame 時，重播先前學到的健康 `GTW_carConfig`（0x7FF）廣播。只在 CAN 廣播層做遮罩 — 不會還原 NVRAM 或後端封禁旗標，也不能預防封禁。v2.15 從「Ban Shield」改名（[#60](https://github.com/hypery11/flipper-tesla-fsd/issues/60)、[#67](https://github.com/hypery11/flipper-tesla-fsd/issues/67)）。 |
| **Suppress Chime** | 消掉 ISA 限速警告提示音（僅 HW4，`0x399`）。在 ESP32 上只在偵測到 HW4 後生效；Legacy/HW3 把 `0x399` 當 DAS 狀態用。 |
| **Emerg. Vehicle** | 啟用緊急車輛偵測旗標（僅 HW4，bit59）。 |
| **Precondition** | 透過 `0x082` 觸發電池預熱。 |

**Beta（未測試，請回報結果）：**

| 設定 | CAN ID | 說明 |
|------|--------|------|
| **ScrollPress AP** | `0x3C2` mux=1 | **僅 HW4、僅 Service 模式。** 以基於時間、擬人化的滾輪手勢（press ~250ms → scroll-up ~150ms → press ~250ms → scroll-up）在 `swcRightPressed`（bits 12-13）+ `swcRightScrollTicks`（bits 24-29）上接管 AP，於 `DAS_autopilotState` 由 0→1 上升時觸發 — 不動 `0x3FD`。已知第一個 2026.14.x 繞過法；由 @JakNo 在 Highland HW4 / 2026.14.2 上發現並台架驗證（[#43](https://github.com/hypery11/flipper-tesla-fsd/issues/43)，計時流程 [#82](https://github.com/hypery11/flipper-tesla-fsd/pull/82)）。在 @DmitroPanteliuk 於 Intel HW3 2026.14.6 上回報緊急煞車後，HW3 已於 v2.15 停用 |
| **Nav FSD Route** | `0x3F8` bits 13/48/49 | 啟用基於導航的 FSD routing（EU／受限地區） |
| **TLSSC bit38** | `0x3FD` mux0 bit38 | 明確啟用 TLSSC；與 TLSSC Restore（0x331）搭配為建議的封禁車組合 |
| **Lane Graph** | `0x3FD` mux1 bit45 | UI_showLaneGraph — 在非 FSD 層級顯示車道視覺化 |
| **Tier Override** | `0x7FF` mux=2 | 強制 GTW_autopilot 為 SELF_DRIVING（比 GTW Config Replay 更激進 — 主動寫入而非重播） |
| **Dev Mode** | `0x3F8` bit5 | UI_dasDeveloper 旗標 |
| **右駕（RHD）** | `0x3F8` bit41 | `UI_drivingSide` = RHD（設 bit41、清 bit40 — 與舊的 LHD 探針互斥）。僅限右駕市場。誠實說明：先前的 Force-LHD 探針**實測無效** — 在被封禁的右駕 HW3 / 2026.2.6 上，值 0/1/2 都讓 FSD 停在 LHD 側（[#66](https://github.com/hypery11/flipper-tesla-fsd/issues/66)）；RHD 現在改以「請求的行駛方向」覆寫出貨 |
| **Hands-Off** | `0x3F8` bit14 | UI 層的手扶停用（第二條 nag 向量） |
| **Telemetry Off** | `0x3F8` bits 19/42/43/44/55 + `0x3FD` mux1 bits 48/50 | 清掉可觸及的遙測啟用旗標（0x3F8 上的 clip / trip / road-segment，0x3FD 上的座艙攝影機 / 中國）。實驗性 — **只涵蓋可觸及的旗標，不含 Vehicle 匯流排 ECU 日誌上傳，也不保證免於封禁。** 僅在拔掉 SIM 卡時使用 |

**14.x 實驗性（預設關閉，請回報）：**

這些針對 Tesla 2026.14.x / 2026.20 行為，**全部預設關閉**。它們是探針，不是已確認的通用修法 — 即時狀態見 [#122](https://github.com/hypery11/flipper-tesla-fsd/issues/122)。在 ESP32 網頁儀表板切換（部分也在 Flipper 設定中）。

| 設定 | 說明 |
|------|------|
| **Abort Guard**（ESP32） | Steer-jerk 緩解（[#108](https://github.com/hypery11/flipper-tesla-fsd/issues/108)）。啟動瞬間的方向盤抽動其實是車自己**中止**接管（`DAS_autopilotState` → `8 ABORTING` → `9 ABORTED`）。開啟後一偵測到 abort 狀態就立刻切掉所有 activation 注入，並維持到乾淨脫離。**已上車驗證：** 在寬／直路上消除了抽動（數百次循環 0 次，原本約 1/25–30）。侷限：部分窄路會直接跳到 `FAULT (9)`、沒有前導訊號，擋不住。 |
| **Soft Engage** | Steer-jerk 緩解（[#108](https://github.com/hypery11/flipper-tesla-fsd/issues/108)）。把啟動邊緣的注入壓住，直到方向盤回到中心 ±5° 內。需要匯流排上有 `0x129`（方向盤角度）；沒有就退化成只有 AP-First。直路抽動已大致被 Abort Guard 取代。 |
| **Nag Burst** | 以爆發／暫停方式回放 `0x370`（約 1 秒開 / 1.5 秒關），而非連續（[#122](https://github.com/hypery11/flipper-tesla-fsd/issues/122)）。休息期被認為是一些在野裝置能躲過更嚴格 14.x nag 偵測的原因。搭配 ±1.8 Nm 轉向扭力上限。 |
| **EPAS-faithful（Mode-C）** | 模擬真實 EPAS 的 demand-state 扭力模型，不去翻 `handsOnLevel`（[#100](https://github.com/hypery11/flipper-tesla-fsd/issues/100)）。用於標準 nag 抑制會觸發 preflight 的車。**尚未上車確認。** |
| **Signal Map**（ESP32 → 進階） | 自訂 nag 抑制讀取 AP-state／hands-on／方向盤的位置：`id + byte/shift/mask`（[#122](https://github.com/hypery11/flipper-tesla-fsd/issues/122)）。用於 `0x39B`/`0x399` 佈局不同的車型變體。有新鮮度閘門 — 設錯會 fail-closed。DAS id 留 `0` 為自動偵測。 |

**硬體：**

| 設定 | 說明 |
|------|------|
| **MCP Crystal** | 16 / 8 / 12 MHz — 對應你 CAN 模組的晶振頻率。 |
| **Hardware**（ESP32） | Auto-detect / Force HW4 / Force HW3 / Force Legacy。Auto-detect 需要 `0x398`，但許多 Model 3/Y 從不發送它 — 偵測錯誤時可自己指定車型。存於 NVS，開機時套用。 |

### HW 支援

| Tesla HW | 修改的 Bits | 速度檔位 |
|----------|------------|----------|
| Legacy（HW1/HW2） | bit46 | 3 段（0-2） |
| HW3 | bit46 | 3 段（0-2） |
| HW4（FSD V14+） | bit46 + bit60、bit47 | 5 段（0-4） |

---

## 硬體

### Flipper Zero

| 元件 | 說明 | 價格 |
|------|------|------|
| [Flipper Zero](https://flipper.net/) | 多功能工具本體 | ~$170 |
| [Electronic Cats CAN Bus Add-On](https://electroniccats.com/store/flipper-addon-canbus/) | MCP2515 CAN 收發器（支援 v1.2） | ~$30 |
| OBD-II 線或 X179 pigtail | 接到 Tesla 的 CAN bus | ~$5-10 |

### ESP32（$14 起）

功能完整的 ESP32 移植版，內建 WiFi 網頁儀表板、NVS 設定保存、深度睡眠與出廠重設。與 Flipper app 相同的 CAN 邏輯。

ESP32 韌體會依偵測到的硬體版本對應 AP/DAS 狀態來源：

| 偵測到的 HW | `0x399` | `0x39B` | ISA 限速提示音 |
|-------------|---------|---------|----------------|
| Legacy HW1/HW2 | `DAS_status` | 不使用 | 停用 |
| HW3 | `DAS_status` | 不使用 | 停用 |
| HW4 | `ISA_SPEED` | `DAS_status` | 啟用 |

| 板子 | 成本 | 編譯目標 |
|------|------|----------|
| M5Stack ATOM Lite + ATOMIC CAN | ~$14 | `m5stack-atom` |
| Lilygo T-CAN485 | ~$15 | `esp32-lilygo` |
| Waveshare ESP32-S3-RS485-CAN | ~$18 | `waveshare-s3-can` |
| 通用 ESP32 + MCP2515 | ~$6 | `esp32-mcp2515` |

設定見 [`esp32/README.md`](https://github.com/hypery11/flipper-tesla-fsd/tree/main/esp32)，完整對照＋接線圖＋X179 針腳見 [`HARDWARE.md`](HARDWARE.md)。

### 接點

- **OBD-II**（方向盤柱下方）— Party CAN。部分 Model 3/Y 車款在 Drive 檔可能靜默。
- **X179**（副駕駛座腳踢板後方）— 建議使用。Pin 13/14 = Bus 6（混合轉送，在所有模式下都保持活動）。20-pin 與 26-pin 針腳見 [`HARDWARE.md`](HARDWARE.md)。

<p align="center">
  <img src="images/wiring_diagram.png" alt="接線圖" width="700">
</p>

---

## 安裝

### 快速開始（不需要編譯工具）

第一次接觸、不太懂技術？選你的硬體 — 兩條路徑都不用命令列：

**Flipper Zero**
1. 打開 [Releases](https://github.com/hypery11/flipper-tesla-fsd/releases)，從最新版本下載 `tesla_mod.fap`。
2. 接上 Flipper，打開 [qFlipper](https://flipperzero.one/update)（官方桌面 app）。
3. 把 `tesla_mod.fap` 複製到 SD 卡的 `apps/GPIO/`。
4. 在 Flipper 上：**Apps → GPIO → Tesla Mod**。

**ESP32** — 直接從瀏覽器燒錄，什麼都不用裝：
1. 取得燒錄器：打開線上的 **[Web Flasher](https://hypery11.github.io/flipper-tesla-fsd/install/)**，或從最新[版本](https://github.com/hypery11/flipper-tesla-fsd/releases)下載 `tesla-flasher.html` 打開 — 兩者都能在桌面版的 **Chrome、Edge 或 Opera** 上運作。
2. 用 USB 接上板子，在你的板子旁按 **Install**，選擇序列埠。
3. 完成後，連上板子的 Wi-Fi 網路並打開 `http://192.168.4.1` 來控制它。

板子開機時處於 **Listen-Only 模式**（無法傳送），直到你在儀表板啟用 Active。接到車上的接線依你的 Tesla 車型／年份而定 — 見 [HARDWARE.md](HARDWARE.md) 或開 issue 詢問。

### 方法一：下載編譯好的 FAP

1. 到 [Releases](https://github.com/hypery11/flipper-tesla-fsd/releases) 頁面
2. 下載 `tesla_mod.fap`
3. 複製到 Flipper 的 SD 卡：`SD Card/apps/GPIO/tesla_mod.fap`

### 方法二：自行編譯

```bash
git clone https://github.com/hypery11/flipper-tesla-fsd.git
cd flipper-tesla-fsd
ufbt
# 輸出：dist/tesla_mod.fap
```

### ESP32

> 不想自己編譯？從 **[Web Flasher](https://hypery11.github.io/flipper-tesla-fsd/install/)** 燒錄預先編譯好的映像檔 — 一鍵完成，不需要工具鏈。

```bash
git clone https://github.com/hypery11/flipper-tesla-fsd.git
cd flipper-tesla-fsd/esp32
pio run -e m5stack-atom    # 或：esp32-lilygo、waveshare-s3-can、esp32-mcp2515
```

---

## 使用方式

1. 把 CAN Add-On 插上 Flipper Zero（或燒錄 ESP32）
2. 用 CAN-H/CAN-L 透過 OBD-II 或 X179 pin 13/14 接到車輛
3. 開啟 app：`Apps > GPIO > Tesla Mod`
4. 選 **「Auto Detect & Start」**（或手動 Force HW3/HW4）
5. 等待偵測（最多 8 秒）— Palladium S/X 會自動從 Legacy 升級到 HW3
6. 當車上啟用 TLSSC 開關時，app 就會自動開始修改 frame

---

## 相容性

### 已確認可用（社群測試）

| 車型 | HW | 韌體 | 測試者 | 功能 |
|------|----|------|--------|------|
| Model S Plaid 2023（Palladium） | HW3/MCU3 | 2026.2.9.3 | @MiniCS、@nagotti | TLSSC Restore、FSD |
| Model 3 Highland Perf 2024 | HW4 | 2026.8.6 | @kp43h8 | TLSSC Restore，斷線後仍保留 |
| Model 3 2019-2023 | HW3 | 多種 | @THER4iN 等多人 | FSD、nag killer |
| Model X Raven 2017（HW3 retrofit） | HW3/MCU2 | 2026.8.3 | @dmagyar | Nag killer、EAP |
| Model Y 2023（中規 MIC） | HW3 | 2026.2.11 | 社群 | FSD（Force FSD 模式） |
| Model 3/Y 2023+ | HW4 | < 2026.2.9 | @vbarrier、@kwangseok73-sudo | FSD |

### 已知限制

| 韌體 | 問題 | 變通方法 |
|------|------|----------|
| 2026.8.6+ | 區域鎖 — FSD 神經網路在部分地區拒絕執行 | 拔 SIM 卡，用 Force FSD |
| 2026.8.6 HW4 | HW4 注入路徑在這個特定版本壞掉 | 用 Force HW3 模式 |
| Intel HW3（被封禁） | 透過 0x331 恢復了 TLSSC 開關，但啟用它會讓 AP 失效 | 先接管 AP，再透過 0x3FD 注入 TLSSC |

用 [Car compatibility report](https://github.com/hypery11/flipper-tesla-fsd/issues/new?template=car_compatibility.yml) 範本回報你自己的測試結果。

---

## 運作原理

在 Party CAN 上做單匯流排的讀取-修改-重發。不需要 MITM，不用接第二條匯流排。

1. 閘道器／ECU 在 CAN bus 上發出一個 frame
2. Flipper/ESP32 收到，修改目標 bit
3. 重發 — 接收端採用最新的 frame

### CAN ID

| CAN ID | 名稱 | 方向 | 用途 |
|--------|------|------|------|
| `0x331` | `DAS_autopilotConfig` | TX | TLSSC Restore — 將層級設為 SELF_DRIVING |
| `0x370` | `EPAS3P_sysStatus` | TX | Nag killer — counter+1 回應帶擬真扭力 |
| `0x399` | `ISA_speedLimit` / `DAS_status` | TX/RX | ESP32 依 HW 而定：Legacy/HW3 在此讀 DAS 狀態；HW4 用於 ISA 限速提示音抑制 |
| `0x3FD` | `UI_autopilotControl` | TX | FSD 解鎖 — bit46/60（HW3/HW4）、TLSSC bit38、lane graph bit45 |
| `0x3F8` | `UI_driverAssistControl` | TX | Nav FSD route、hands-off、dev mode、RHD 行駛方向（bit41）、telemetry-off（beta） |
| `0x3EE` | `UI_autopilotControl` | TX | FSD 解鎖 — Legacy HW1/HW2 |
| `0x3C2` | `VCLEFT_switchStatus` | TX | ScrollPress AP — 於 mux=1 注入右滾輪（HW4、Service 模式、beta） |
| `0x7FF` | `GTW_carConfig` | TX | GTW Config Replay + 主動層級覆寫 |
| `0x082` | `UI_tripPlanning` | TX | 電池預熱觸發 |
| `0x313` | `UI_trackModeSettings` | TX | Track Mode — 操控平衡／穩定／冷卻（校驗和重算；Vehicle 匯流排） |
| `0x398` | `GTW_carConfig` | RX | HW 版本偵測 |
| `0x318` | `GTW_carState` | RX | OTA 偵測（自動暫停 TX） |
| `0x399` | `DAS_status`（HW3/Legacy）/ `ISA_speedLimit`（HW4） | RX/TX | 依 HW 分派：pre-Highland HW3 讀為 DAS_status（AP 狀態＋手扶）；HW4 保留提示音抑制寫入路徑 |
| `0x39B` | `DAS_status` | RX | HW4 + Highland HW3 — AP 狀態（給 AP-First）、nag 等級、變換車道、盲點 |
| `0x132` | `BMS_hvBusStatus` | RX | 電池組電壓／電流 |
| `0x292` | `BMS_socStatus` | RX | 充電狀態 |
| `0x312` | `BMS_thermalStatus` | RX | 電池溫度 |
| `0x33A` | `UI_ratedConsumption` | RX | 能耗（Wh/km） |

完整 42 個 handler 清單（18 TX、24 RX）見 [`fsd_logic/fsd_handler.h`](fsd_logic/fsd_handler.h)。

---

## 常見問題

**拔掉之後 FSD 還會維持嗎？**
不會。這是即時 frame 修改，拔掉就恢復原樣。

**沒有 FSD 訂閱能用嗎？**
FSD 功能（TLSSC、交通號誌／停車標誌控制）需要來自 Tesla 的 FSD 授權。沒有它，AP ECU 就沒有載入神經網路權重。非 FSD 功能（nag killer、BMS 儀表板、限速提示音抑制、診斷）在任何支援 AP 的車上都能用。

**VIN 層級封禁怎麼辦？**
Tesla 自 2026 年 4 月起在伺服器端封禁 VIN。封禁會把 `GTW_autopilot` 層級從 SELF_DRIVING 降到 ENHANCED，並移除 TLSSC 開關。**TLSSC Restore** 功能（0x331）可在 Palladium 與 HW4 上恢復停車標誌／交通號誌控制。完整研究見 [issue #18](https://github.com/hypery11/flipper-tesla-fsd/issues/18)。**GTW Config Replay**（0x7FF，前稱「Ban Shield」）可即時重播先前學到的健康設定，但只在 CAN 廣播層 — 不會還原底層的 NVRAM 或伺服器端狀態。

**Flipper Zero vs ESP32 — 該買哪個？**
ESP32 更便宜（$14 vs $200+），有 WiFi 儀表板、NVS 保存與深度睡眠。Flipper 更便攜，且有內建螢幕。兩者跑相同的 CAN 邏輯。如果你還沒有 Flipper，選 ESP32。

**支援 Model S / Model X 嗎？**
支援。Palladium S/X（2021+）已確認可用 TLSSC Restore。2021 前、做了 HW3 retrofit 的 S/X 透過 Legacy→HW3 自動升級可用。HW1/HW2 Model S/X 走 Legacy 模式（`0x3EE`）。Model S/X 使用不同的 BMS CAN ID — BMS 儀表板可能顯示錯誤數值。

**這會不會把車搞壞（brick）？**
只動 UI 設定 frame。不會寫入煞車、轉向或動力系統。App 預設以 Listen-Only 模式開機。完整 TX 面清單見 [SECURITY.md](SECURITY.md)。

**一定要 Flipper CAN Add-On 嗎？**
給 Flipper：是的，任何 MCP2515 模組（Electronic Cats、通用板子）都行。給 ESP32：多數支援的板子有內建 CAN 收發器（M5Stack ATOMIC CAN、Lilygo T-CAN485、Waveshare S3）。

---

## 相關專案

| 專案 | 是什麼 | 硬體 |
|------|--------|------|
| [ev-open-can-tools](https://github.com/ev-open-can-tools/ev-open-can-tools) | 上游社群專案。開發活動在 GitHub 上（v3.0.x，GPL-3.0）。前身是 GitLab 上的 `Tesla-OPEN-CAN-MOD`；該群組已改名為 `ev-open-can-tools`，GitLab repo 現已停擺（0 個開啟中的 issue/MR，最後一次 commit 2026-04-25）— 請追蹤 GitHub repo。 | RP2040 CAN、Feather M4、ESP32 |
| [dzid26/ESP32-DualCAN](https://github.com/dzid26/ESP32-DualCAN) | 「Dorky Commander」— S3XY Commander 的開源硬體替代品 | ESP32 + dual CAN |
| [tuncasoftbildik/tesla-can-mod](https://github.com/tuncasoftbildik/tesla-can-mod) | Arduino 參考實作，附 frame template | Arduino + MCP2515 |
| [tumik/S3XY-candump](https://github.com/tumik/S3XY-candump) | 透過 S3XY Commander（Panda 協議）的 Python CAN dump 工具 | Commander dongle |

## 致謝

- [commaai/opendbc](https://github.com/commaai/opendbc) — Tesla CAN 訊號資料庫
- [ElectronicCats/flipper-MCP2515-CANBUS](https://github.com/ElectronicCats/flipper-MCP2515-CANBUS) — Flipper 用 MCP2515 驅動
- 社群貢獻者 — 本專案賴以運作的實車測試、擷取與研究：
  - **協議、nag killer 與 2026.14.x：** @jewelrylin（T-2CAN 雙匯流排擷取、frame-content preflight 測試、X179 Service Mode 針腳圖）、@DrStrangeglovebox（`0x370` 參考擷取 + HW4 雙 CAN 資料 + 安全發現）、@ssw0209-sys（Mode-C 轉向扭力參考 + HW4 14.x 測試）、@0xAccretion（HW4 Highland 中規 MIC DAS 佈局發現，#116/#117）、@dunckencn（國行 HW3 start-after-AP 驗證、steer-jerk 與 bus-off 回報）、@kristopf007（HW4 14.x 實車測試）
  - **功能、擷取與 PR：** @JakNo（ScrollPress AP / `0x3C2`）、@vrs11（Continuous AP）、@sqladm1n（RTC 擷取日誌 PR + 匯流排/接線排查）、@DmitroPanteliuk（全速率 `0x229` 擷取）、@se7en7777777（`0x485` / Highland / 校驗和分析）、@RoyRakete（TLSSC 封禁車組合）、@mamixsystem（post-SOP10 連接器參考）、@p0sixturtle（Summon / tier-selector 線索，#139）、@dahua910（RHD 需求，#66）、@HamzaObaidat（劇院模式 `0x118` 研究，#149）、@fboulegue（EU / 新線束 Juniper 回報，#143/#109/#110）、@densen2014（ESP32 HW 選擇器建議，#110）
  - **封禁研究、平台測試、ESP32、bug 修復：** @THER4iN、@MiniCS、@kp43h8、@gauner1986、@dmagyar、@ViPiMP、@marcobellinoroci-source、@danpadure、@bruvv、@Symness、@hkloudou、@nagotti、@patatman、@JordanzhaoD
- `Starmixcraft/tesla-fsd-can-mod` — 原始 CanFeather FSD 研究（GitLab repo 已被移除；鏡像在 [Karolynaz/waymo-fsd-can-mod](https://github.com/Karolynaz/waymo-fsd-can-mod)）

## 支持這個研究

如果這個專案幫你省下了改裝盒子的錢、讓你看懂 Tesla 的 CAN bus，或在封禁後保住了你的 TLSSC，歡迎贊助持續的研究與測試。

[![Crypto](https://img.shields.io/badge/Crypto-Donate-F7931A?style=for-the-badge&logo=bitcoin&logoColor=white)](https://fsd.fkey.id/) [![PayPal](https://img.shields.io/badge/PayPal-Donate-00457C?style=for-the-badge&logo=paypal&logoColor=white)](https://www.paypal.com/cgi-bin/webscr?cmd=_xclick&business=hypery11@gmail.com&item_name=Tesla+FSD+Open+Source+Research&currency_code=USD) [![GitHub Sponsors](https://img.shields.io/badge/Sponsor-hypery11-EA4AAA?style=for-the-badge&logo=github&logoColor=white)](https://github.com/sponsors/hypery11)

加密貨幣請至 **[fsd.fkey.id](https://fsd.fkey.id/)** — 同一個地址、支援多鏈。實際可用的網路請直接開頁面查看。

款項用於測試用的 Tesla 零件（待救援的封禁 VIN、不同 MCU/HW 組合）、各種 ESP32 硬體，以及逆向新韌體版本所花的時間。

## 授權

GPL-3.0

## 免責聲明

僅供教育與研究用途。**FSD 是 Tesla 的付費功能，必須合法購買或訂閱使用。** 改裝車輛系統可能導致保固失效，也可能違反當地法規。使用者需自行承擔所有責任與風險。完整安全與責任使用說明見 [`SECURITY.md`](SECURITY.md)。
