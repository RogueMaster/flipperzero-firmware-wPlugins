[English](README.md) | [繁體中文](README_zh-TW.md) | [简体中文](README_zh-CN.md)

> [!WARNING]
> **本翻译可能落后于英文版。** 功能描述、CAN ID 表、硬件接线指南等以 [英文 README](README.md) 为准。如果你发现翻译与英文版不一致，欢迎提交 PR 修正。

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

> **开源 Tesla CAN bus 工具集，支持 Flipper Zero 与 ESP32。** FSD 区域锁绕过、给 VIN 被封禁车辆的 TLSSC Restore、带拟真扭力变化的 nag killer、GTW Config Replay、BMS 实时仪表板，以及横跨 Model 3、Model Y、Model S、Model X 的 30+ 个 CAN handler。支持 HW3、HW4 与 Legacy HW1/HW2。$200+ 的 S3XY Commander 的免费替代方案 — 搭配 [ESP32 移植版](https://github.com/hypery11/flipper-tesla-fsd/tree/main/esp32) 总成本最低只要 **$14**。

> [!IMPORTANT]
> **FSD 相关功能需要有效的 FSD 套件** — 购买或订阅皆可。此工具在 CAN bus 层面启用 FSD 功能，但车辆仍需要来自 Tesla 的合法 FSD 授权。非 FSD 功能（nag killer、BMS 仪表板、诊断）无需任何订阅即可使用。

> [!CAUTION]
> **Tesla 已开始实施 VIN 层级封禁**（2026 年 4 月）。受影响的车辆会静默失去 TLSSC 开关 — 没有 OTA、没有警告，且在账号转移与重新订阅后依然存在。**TLSSC Restore** 功能（v2.10+）可通过 0x331 DAS 设置伪造，在被封禁的 Palladium 与 HW4 车上恢复停车标志／交通信号灯控制。完整封禁研究见 [SECURITY.md](SECURITY.md) 与 [issue #18](https://github.com/hypery11/flipper-tesla-fsd/issues/18)。

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

### 核心 FSD
- 从 `GTW_carConfig`（`0x398`）自动检测 HW3/HW4；当所接的总线上没有 `0x398` 时，改用 `0x3FD`/`0x399`/`0x3EE` 备用检测
- **Legacy→HW3 自动升级**（Palladium Model S/X）— 先检测到 `das_hw=0`，之后当 `0x3FD` 出现在总线上时升级
- 通过修改 `UI_autopilotControl`（`0x3FD` / `0x3EE`）的 bit 来解锁 FSD
- **Legacy 模式**，支持 HW1/HW2（Model S/X 2016-2019）
- 速度档位默认最快，并从跟车距离拨杆同步

### TLSSC Restore（v2.10+）
- 在 **VIN 被封禁** 的车辆上恢复交通信号灯与停车标志控制
- 对 CAN ID `0x331` 做读取-修改-重发 — 将 `DAS_autopilot` 设为 SELF_DRIVING
- 已在 Palladium（Model S Plaid 2023）、HW4 Highland（Model 3 Performance 2024）与 Intel HW3（需 AP-first 变通）上确认可用
- 不会恢复完整 FSD 可视化 — 只恢复 TLSSC（停车标志／交通信号灯）
- **建议的封禁车组合**：同时启用 **TLSSC Restore** + **TLSSC bit38**（`0x3FD` mux 0 bit 38）— @RoyRakete 在 HW3 / 2026.2.6 上确认可靠（[#18](https://github.com/hypery11/flipper-tesla-fsd/issues/18#issuecomment-4413430516)）。在某些封禁固件上，单独开任一个都不稳定；两者搭配才能重新启用 AP/TACC 接管

### GTW Config Replay（v2.9+，v2.15 从「Ban Shield」改名）
- 监看 `GTW_carConfig`（`0x7FF`），当网关发出被修改的 frame 时，实时重播先前学到的健康总线广播
- 第一次运行时学习全部 8 个 mux frame，之后自动武装
- **它实际上做什么：** 只在广播层做掩码。武装后，AP ECU 看到的是重播的健康 frame，而不是网关修改过的那个。Tesla 的封禁会写入 GTW NVRAM（重启后仍在）与服务器端标志；本功能不会还原 NVRAM 状态或后端记录，只影响其他总线上的 ECU 实时看到的内容。
- **它不做什么：** 不能预防封禁、不能解除封禁、也不能改变 Tesla 服务器端的授权记录。在 v2.9-v2.14 部署的 6 周内，没有任何实证确认可预防封禁。诚实说明见 [#60](https://github.com/hypery11/flipper-tesla-fsd/issues/60) 与 [#67](https://github.com/hypery11/flipper-tesla-fsd/issues/67)。v2.14 的名字「Ban Shield」过度承诺了 — v2.15 改名反映代码实际的行为。

### Nag Killer（v2.1+）
- DAS 感知门控 — 只在 DAS 真的要求手扶方向盘时才回应，DAS 满足时零总线流量
- 拟真扭力变化 — 在 1.00-2.40 Nm 之间用 xorshift32 PRNG 随机游走，每 5-9 秒有一次到 3.10-3.30 Nm 的握力脉冲
- **按需握力脉冲（v2.15+）** — 当 `handsOnLevel` 升到提醒需求状态（0 即将 / 3 升级）时，立即发出一次握力脉冲并重置周期排程。补上 v2.14 及更早版本在排程脉冲之间可能出现的 2 秒黄色升级空窗
- 在 `0x370` 做 EPAS counter+1 回应，并抑制 level 0（提醒即将出现）与 level 3（升级警报）
- **请接 Party CAN（X179 pin 2/3）给 nag killer。** `0x370` 在 Party CAN — 不在 Vehicle CAN（9/10），而网关转发的 Chassis 副本（13/14）会触发 2026.14.x preflight。已在 HW4 2026.20 上以接 2/3 确认可用（[#100](https://github.com/hypery11/flipper-tesla-fsd/issues/100)）。接错线对的单 CAN 板子没有东西可回应 — 这是「nag killer 在 HW4 上没反应」最常见的原因。用车上的 **Service Mode → CAN Port** 页面确认你这条线束上哪一对是 Party；见 [HARDWARE.md](HARDWARE.md)。

### AP-First 模式（v2.14+，给 2026.14.x 固件）
- Tesla 2026.14.x 新增了 preflight 检查，若 CAN 注入已在进行就挡下 AP/TACC 接管
- 启用 **AP-First** 后，app 监看 `0x39B` 的 `DAS_autopilotState`，只在 AP 接管后才开始注入 `0x3FD`。在 ESP32 上，DAS 状态来源会依检测到的 HW 版本而定。
- Nag killer、TLSSC Restore 与 GTW Config Replay 不受影响（它们针对不同的 CAN ID）

### 14.x 固件警告（v2.15+）
- **默认开启。** 只要启用警告开关，Flipper 运行画面就会把 BMS / flags 那一行换成 `!14.x: TX may stop AP`。ESP32 网页仪表板则在顶端显示可关闭的黄色横幅。
- 悲观默认：大多数 14.x 固件用户要到自动转向在行驶中脱离时才知道自己受影响。这个警告会在他们启用任何 TX 功能之前先提醒到。
- 可通过 **On 14.x?** 设置开关（Flipper）或横幅上的 **Dismiss** 按钮（ESP32，存在 NVS）退出。若你确定是 pre-14.x 固件就可关闭。
- 地区注意：执法强度因市场而异。部分地区（没有 Tesla 直营的市场）似乎执法较不积极。14.x / 2026.20 的实时追踪见 [#122](https://github.com/hypery11/flipper-tesla-fsd/issues/122)。
- **「2026.14.x / .20 / .26.x 还能解锁 FSD 吗？」** 简短答案：不行——激活 preflight 加上链外区域锁挡住了,但 nag killer / TLSSC / Summon EU / 抓包仍可用。完整说明见置顶的 [FSD-on-14.x FAQ (#168)](https://github.com/hypery11/flipper-tesla-fsd/discussions/168),frame 级证明见 [#163](https://github.com/hypery11/flipper-tesla-fsd/discussions/163)。

### 诊断（只读，不需要 FSD）
- BMS 实时仪表板：电池组电压、电流、SoC、温度范围、**能耗（Wh/km）**
- 车速、方向盘角度、电机扭力、刹车状态
- DAS 状态：autopilot 状态、手扶提醒等级、变道状态、盲点警示、FCW、视觉限速
- GTW autopilot 层级回读（NONE/HIGHWAY/ENHANCED/SELF_DRIVING/BASIC）
- OTA 检测含防抖 — 固件更新期间自动暂停 TX，除非明确启用 Ignore OTA 覆盖

### CAN Capture + 测试配置文件（v2.16+）
- **CAN Capture** — 将每个收到的 frame 以 candump 格式录到 SD 卡（`apps_data/tesla_mod/captures/`）。只读；在任何车上运行都安全。可喂给 `tools/tesla_crc_cracker.py`。
- **Send Test** — 从 SD 卡载入用户自定义的 `.cantest` 文本配置文件并重播你自己的 frame。默认为 dry-run；发送硬性限制在 **停妥、静止** 的车（fail-closed），且每个 frame 前都会重新检查。结果会记录以便回报 bug。格式与流程：[docs/cantest-format.md](docs/cantest-format.md)，示例：[examples/example.cantest](examples/example.cantest)。

### 额外解锁（v2.16+，可选，默认关闭）
- **Summon EU Unlock** — `0x3FD` mux1：清掉 bit19（EU AP 限制）并设 bit47（summon-enable），在受 EU 限制的车上开放召唤（Summon）
- **Continue on Green** — `0x3FD` mux0 bit39 `UI_fsdContinueOnGreenWithCIPV` — 在有前车的情况下，不用拨杆确认就通过绿灯；搭配 TLSSC 使用
- **右舵（RHD）覆盖** — `0x3F8` bit41 `UI_drivingSide` = RHD。仅限右舵市场
- **AP 分支／层级选择器** — `0x3FD` mux1 bits 40-42 `UI_apmv3Branch`：Live / Stage / Dev / Stage2 / EAP / Demo。实验性，非持久化的 UI 提示 — 停止注入后即还原
- **可调 Track Mode** — `0x313` `UI_trackModeSettings`：操控平衡（Handling Balance）+ 稳定辅助（Stability Assist）+ 收车后冷却，校验和会重算。走 Vehicle 总线；默认为 rotation 100 / stability 30%，非 Performance 车型也可用

### 设置（运行时开关）

**稳定（已上车测试）：**

| 设置 | 说明 |
|------|------|
| **Mode** | `Active` / `Listen-Only` / `Service`。Listen-Only 是**首次开机的默认值** — MCP2515 处于硬件 listen-only 模式，物理上无法 TX。 |
| **Nag Killer** | DAS 感知的 EPAS counter+1 回应，带拟真扭力变化。 |
| **Force FSD** | 绕过 `isFSDSelectedInUI` 检查。不会绕过 Tesla 服务器端授权 — 只影响本地 CAN frame 流。 |
| **Ignore OTA** | 即使 `0x318` 报告 Tesla OTA 更新进行中，也允许在 Active 模式下 CAN TX。默认关闭。 |
| **TLSSC Restore** | 0x331 DAS 设置伪造，在被封禁的车上恢复 TLSSC。会触发 MCU 重启。 |
| **AP-First (14.x)** | 延后 0x3FD 注入直到 AP 接管。Tesla 固件 2026.14.x 需要此项。 |
| **GTW Config Replay** | 当网关发出被修改的 frame 时，重播先前学到的健康 `GTW_carConfig`（0x7FF）广播。只在 CAN 广播层做掩码 — 不会还原 NVRAM 或后端封禁标志，也不能预防封禁。v2.15 从「Ban Shield」改名（[#60](https://github.com/hypery11/flipper-tesla-fsd/issues/60)、[#67](https://github.com/hypery11/flipper-tesla-fsd/issues/67)）。 |
| **Suppress Chime** | 消掉 ISA 限速警告提示音（仅 HW4，`0x399`）。在 ESP32 上只在检测到 HW4 后生效；Legacy/HW3 把 `0x399` 当 DAS 状态用。 |
| **Emerg. Vehicle** | 启用紧急车辆检测标志（仅 HW4，bit59）。 |
| **Precondition** | 通过 `0x082` 触发电池预热。 |

**Beta（未测试，请回报结果）：**

| 设置 | CAN ID | 说明 |
|------|--------|------|
| **ScrollPress AP** | `0x3C2` mux=1 | **仅 HW4、仅 Service 模式。** 以基于时间、拟人化的滚轮手势（press ~250ms → scroll-up ~150ms → press ~250ms → scroll-up）在 `swcRightPressed`（bits 12-13）+ `swcRightScrollTicks`（bits 24-29）上接管 AP，于 `DAS_autopilotState` 由 0→1 上升时触发 — 不动 `0x3FD`。已知第一个 2026.14.x 绕过法；由 @JakNo 在 Highland HW4 / 2026.14.2 上发现并台架验证（[#43](https://github.com/hypery11/flipper-tesla-fsd/issues/43)，计时流程 [#82](https://github.com/hypery11/flipper-tesla-fsd/pull/82)）。在 @DmitroPanteliuk 于 Intel HW3 2026.14.6 上报告紧急刹车后，HW3 已于 v2.15 停用 |
| **Nav FSD Route** | `0x3F8` bits 13/48/49 | 启用基于导航的 FSD routing（EU／受限地区） |
| **TLSSC bit38** | `0x3FD` mux0 bit38 | 明确启用 TLSSC；与 TLSSC Restore（0x331）搭配为建议的封禁车组合 |
| **Lane Graph** | `0x3FD` mux1 bit45 | UI_showLaneGraph — 在非 FSD 层级显示车道可视化 |
| **Tier Override** | `0x7FF` mux=2 | 强制 GTW_autopilot 为 SELF_DRIVING（比 GTW Config Replay 更激进 — 主动写入而非重播） |
| **Dev Mode** | `0x3F8` bit5 | UI_dasDeveloper 标志 |
| **右舵（RHD）** | `0x3F8` bit41 | `UI_drivingSide` = RHD（设 bit41、清 bit40 — 与旧的 LHD 探针互斥）。仅限右舵市场。诚实说明：先前的 Force-LHD 探针**实测无效** — 在被封禁的右舵 HW3 / 2026.2.6 上，值 0/1/2 都让 FSD 停在 LHD 侧（[#66](https://github.com/hypery11/flipper-tesla-fsd/issues/66)）；RHD 现在改以「请求的行驶方向」覆盖出货 |
| **Hands-Off** | `0x3F8` bit14 | UI 层的手扶停用（第二条 nag 向量） |
| **Telemetry Off** | `0x3F8` bits 19/42/43/44/55 + `0x3FD` mux1 bits 48/50 | 清掉可触及的遥测启用标志（0x3F8 上的 clip / trip / road-segment，0x3FD 上的座舱摄像头 / 中国）。实验性 — **只涵盖可触及的标志，不含 Vehicle 总线 ECU 日志上传，也不保证免于封禁。** 仅在拔掉 SIM 卡时使用 |

**14.x 实验性（默认关闭，请回报）：**

这些针对 Tesla 2026.14.x / 2026.20 行为，**全部默认关闭**。它们是探针，不是已确认的通用修法 — 实时状态见 [#122](https://github.com/hypery11/flipper-tesla-fsd/issues/122)。在 ESP32 网页仪表板切换（部分也在 Flipper 设置中）。

| 设置 | 说明 |
|------|------|
| **Abort Guard**（ESP32） | Steer-jerk 缓解（[#108](https://github.com/hypery11/flipper-tesla-fsd/issues/108)）。启动瞬间的方向盘抽动其实是车自己**中止**接管（`DAS_autopilotState` → `8 ABORTING` → `9 ABORTED`）。开启后一检测到 abort 状态就立刻切掉所有 activation 注入，并维持到干净脱离。**已上车验证：** 在宽／直路上消除了抽动（数百次循环 0 次，原本约 1/25–30）。局限：部分窄路会直接跳到 `FAULT (9)`、没有前导信号，挡不住。 |
| **Soft Engage** | Steer-jerk 缓解（[#108](https://github.com/hypery11/flipper-tesla-fsd/issues/108)）。把启动边缘的注入压住，直到方向盘回到中心 ±5° 内。需要总线上有 `0x129`（方向盘角度）；没有就退化成只有 AP-First。直路抽动已大致被 Abort Guard 取代。 |
| **Nag Burst** | 以爆发／暂停方式回放 `0x370`（约 1 秒开 / 1.5 秒关），而非连续（[#122](https://github.com/hypery11/flipper-tesla-fsd/issues/122)）。休息期被认为是一些在野设备能躲过更严格 14.x nag 检测的原因。搭配 ±1.8 Nm 转向扭力上限。 |
| **EPAS-faithful（Mode-C）** | 模拟真实 EPAS 的 demand-state 扭力模型，不去翻 `handsOnLevel`（[#100](https://github.com/hypery11/flipper-tesla-fsd/issues/100)）。用于标准 nag 抑制会触发 preflight 的车。**尚未上车确认。** |
| **Signal Map**（ESP32 → 高级） | 自定义 nag 抑制读取 AP-state／hands-on／方向盘的位置：`id + byte/shift/mask`（[#122](https://github.com/hypery11/flipper-tesla-fsd/issues/122)）。用于 `0x39B`/`0x399` 布局不同的车型变体。有新鲜度门控 — 设错会 fail-closed。DAS id 留 `0` 为自动检测。 |

**硬件：**

| 设置 | 说明 |
|------|------|
| **MCP Crystal** | 16 / 8 / 12 MHz — 对应你 CAN 模块的晶振频率。 |
| **Hardware**（ESP32） | Auto-detect / Force HW4 / Force HW3 / Force Legacy。Auto-detect 需要 `0x398`，但许多 Model 3/Y 从不发送它 — 检测错误时可自己指定车型。存于 NVS，开机时应用。 |

### HW 支持

| Tesla HW | 修改的 Bits | 速度档位 |
|----------|------------|----------|
| Legacy（HW1/HW2） | bit46 | 3 段（0-2） |
| HW3 | bit46 | 3 段（0-2） |
| HW4（FSD V14+） | bit46 + bit60、bit47 | 5 段（0-4） |

---

## 硬件

### Flipper Zero

| 组件 | 说明 | 价格 |
|------|------|------|
| [Flipper Zero](https://flipper.net/) | 多功能工具本体 | ~$170 |
| [Electronic Cats CAN Bus Add-On](https://electroniccats.com/store/flipper-addon-canbus/) | MCP2515 CAN 收发器（支持 v1.2） | ~$30 |
| OBD-II 线或 X179 pigtail | 接到 Tesla 的 CAN bus | ~$5-10 |

### ESP32（$14 起）

功能完整的 ESP32 移植版，内建 WiFi 网页仪表板、NVS 设置保存、深度睡眠与出厂重置。与 Flipper app 相同的 CAN 逻辑。

ESP32 固件会依检测到的硬件版本对应 AP/DAS 状态来源：

| 检测到的 HW | `0x399` | `0x39B` | ISA 限速提示音 |
|-------------|---------|---------|----------------|
| Legacy HW1/HW2 | `DAS_status` | 不使用 | 停用 |
| HW3 | `DAS_status` | 不使用 | 停用 |
| HW4 | `ISA_SPEED` | `DAS_status` | 启用 |

| 板子 | 成本 | 编译目标 |
|------|------|----------|
| M5Stack ATOM Lite + ATOMIC CAN | ~$14 | `m5stack-atom` |
| Lilygo T-CAN485 | ~$15 | `esp32-lilygo` |
| Waveshare ESP32-S3-RS485-CAN | ~$18 | `waveshare-s3-can` |
| 通用 ESP32 + MCP2515 | ~$6 | `esp32-mcp2515` |

设置见 [`esp32/README.md`](https://github.com/hypery11/flipper-tesla-fsd/tree/main/esp32)，完整对照＋接线图＋X179 引脚见 [`HARDWARE.md`](HARDWARE.md)。

### 接点

- **OBD-II**（方向盘柱下方）— Party CAN。部分 Model 3/Y 车款在 Drive 挡可能静默。
- **X179**（副驾驶座脚踢板后方）— 建议使用。Pin 13/14 = Bus 6（混合转发，在所有模式下都保持活动）。20-pin 与 26-pin 引脚见 [`HARDWARE.md`](HARDWARE.md)。

<p align="center">
  <img src="images/wiring_diagram.png" alt="接线图" width="700">
</p>

---

## 安装

### 快速开始（不需要编译工具）

第一次接触、不太懂技术？选你的硬件 — 两条路径都不用命令行：

**Flipper Zero**
1. 打开 [Releases](https://github.com/hypery11/flipper-tesla-fsd/releases)，从最新版本下载 `tesla_mod.fap`。
2. 接上 Flipper，打开 [qFlipper](https://flipperzero.one/update)（官方桌面 app）。
3. 把 `tesla_mod.fap` 复制到 SD 卡的 `apps/GPIO/`。
4. 在 Flipper 上：**Apps → GPIO → Tesla Mod**。

**ESP32** — 直接从浏览器烧录，什么都不用装：
1. 取得烧录器：打开在线的 **[Web Flasher](https://hypery11.github.io/flipper-tesla-fsd/install/)**，或从最新[版本](https://github.com/hypery11/flipper-tesla-fsd/releases)下载 `tesla-flasher.html` 打开 — 两者都能在桌面版的 **Chrome、Edge 或 Opera** 上运行。
2. 用 USB 接上板子，在你的板子旁按 **Install**，选择串口。
3. 完成后，连上板子的 Wi-Fi 网络并打开 `http://192.168.4.1` 来控制它。

板子开机时处于 **Listen-Only 模式**（无法发送），直到你在仪表板启用 Active。接到车上的接线依你的 Tesla 车型／年份而定 — 见 [HARDWARE.md](HARDWARE.md) 或开 issue 询问。

### 方法一：下载编译好的 FAP

1. 到 [Releases](https://github.com/hypery11/flipper-tesla-fsd/releases) 页面
2. 下载 `tesla_mod.fap`
3. 复制到 Flipper 的 SD 卡：`SD Card/apps/GPIO/tesla_mod.fap`

### 方法二：自行编译

```bash
git clone https://github.com/hypery11/flipper-tesla-fsd.git
cd flipper-tesla-fsd
ufbt
# 输出：dist/tesla_mod.fap
```

### ESP32

> 不想自己编译？从 **[Web Flasher](https://hypery11.github.io/flipper-tesla-fsd/install/)** 烧录预先编译好的镜像 — 一键完成，不需要工具链。

```bash
git clone https://github.com/hypery11/flipper-tesla-fsd.git
cd flipper-tesla-fsd/esp32
pio run -e m5stack-atom    # 或：esp32-lilygo、waveshare-s3-can、esp32-mcp2515
```

---

## 使用方式

1. 把 CAN Add-On 插上 Flipper Zero（或烧录 ESP32）
2. 用 CAN-H/CAN-L 通过 OBD-II 或 X179 pin 13/14 接到车辆
3. 打开 app：`Apps > GPIO > Tesla Mod`
4. 选 **「Auto Detect & Start」**（或手动 Force HW3/HW4）
5. 等待检测（最多 8 秒）— Palladium S/X 会自动从 Legacy 升级到 HW3
6. 当车上启用 TLSSC 开关时，app 就会自动开始修改 frame

---

## 兼容性

### 已确认可用（社区测试）

| 车型 | HW | 固件 | 测试者 | 功能 |
|------|----|------|--------|------|
| Model S Plaid 2023（Palladium） | HW3/MCU3 | 2026.2.9.3 | @MiniCS、@nagotti | TLSSC Restore、FSD |
| Model 3 Highland Perf 2024 | HW4 | 2026.8.6 | @kp43h8 | TLSSC Restore，断线后仍保留 |
| Model 3 2019-2023 | HW3 | 多种 | @THER4iN 等多人 | FSD、nag killer |
| Model X Raven 2017（HW3 retrofit） | HW3/MCU2 | 2026.8.3 | @dmagyar | Nag killer、EAP |
| Model Y 2023（中规 MIC） | HW3 | 2026.2.11 | 社区 | FSD（Force FSD 模式） |
| Model 3/Y 2023+ | HW4 | < 2026.2.9 | @vbarrier、@kwangseok73-sudo | FSD |

### 已知限制

| 固件 | 问题 | 变通方法 |
|------|------|----------|
| 2026.8.6+ | 区域锁 — FSD 神经网络在部分地区拒绝运行 | 拔 SIM 卡，用 Force FSD |
| 2026.8.6 HW4 | HW4 注入路径在这个特定版本坏掉 | 用 Force HW3 模式 |
| Intel HW3（被封禁） | 通过 0x331 恢复了 TLSSC 开关，但启用它会让 AP 失效 | 先接管 AP，再通过 0x3FD 注入 TLSSC |

用 [Car compatibility report](https://github.com/hypery11/flipper-tesla-fsd/issues/new?template=car_compatibility.yml) 模板回报你自己的测试结果。

---

## 运行原理

在 Party CAN 上做单总线的读取-修改-重发。不需要 MITM，不用接第二条总线。

1. 网关／ECU 在 CAN bus 上发出一个 frame
2. Flipper/ESP32 收到，修改目标 bit
3. 重发 — 接收端采用最新的 frame

### CAN ID

| CAN ID | 名称 | 方向 | 用途 |
|--------|------|------|------|
| `0x331` | `DAS_autopilotConfig` | TX | TLSSC Restore — 将层级设为 SELF_DRIVING |
| `0x370` | `EPAS3P_sysStatus` | TX | Nag killer — counter+1 回应带拟真扭力 |
| `0x399` | `ISA_speedLimit` / `DAS_status` | TX/RX | ESP32 依 HW 而定：Legacy/HW3 在此读 DAS 状态；HW4 用于 ISA 限速提示音抑制 |
| `0x3FD` | `UI_autopilotControl` | TX | FSD 解锁 — bit46/60（HW3/HW4）、TLSSC bit38、lane graph bit45 |
| `0x3F8` | `UI_driverAssistControl` | TX | Nav FSD route、hands-off、dev mode、RHD 行驶方向（bit41）、telemetry-off（beta） |
| `0x3EE` | `UI_autopilotControl` | TX | FSD 解锁 — Legacy HW1/HW2 |
| `0x3C2` | `VCLEFT_switchStatus` | TX | ScrollPress AP — 于 mux=1 注入右滚轮（HW4、Service 模式、beta） |
| `0x7FF` | `GTW_carConfig` | TX | GTW Config Replay + 主动层级覆盖 |
| `0x082` | `UI_tripPlanning` | TX | 电池预热触发 |
| `0x313` | `UI_trackModeSettings` | TX | Track Mode — 操控平衡／稳定／冷却（校验和重算；Vehicle 总线） |
| `0x398` | `GTW_carConfig` | RX | HW 版本检测 |
| `0x318` | `GTW_carState` | RX | OTA 检测（自动暂停 TX） |
| `0x399` | `DAS_status`（HW3/Legacy）/ `ISA_speedLimit`（HW4） | RX/TX | 依 HW 分派：pre-Highland HW3 读为 DAS_status（AP 状态＋手扶）；HW4 保留提示音抑制写入路径 |
| `0x39B` | `DAS_status` | RX | HW4 + Highland HW3 — AP 状态（给 AP-First）、nag 等级、变道、盲点 |
| `0x132` | `BMS_hvBusStatus` | RX | 电池组电压／电流 |
| `0x292` | `BMS_socStatus` | RX | 充电状态 |
| `0x312` | `BMS_thermalStatus` | RX | 电池温度 |
| `0x33A` | `UI_ratedConsumption` | RX | 能耗（Wh/km） |

完整 42 个 handler 清单（18 TX、24 RX）见 [`fsd_logic/fsd_handler.h`](fsd_logic/fsd_handler.h)。

---

## 常见问题

**拔掉之后 FSD 还会维持吗？**
不会。这是实时 frame 修改，拔掉就恢复原样。

**没有 FSD 订阅能用吗？**
FSD 功能（TLSSC、交通信号灯／停车标志控制）需要来自 Tesla 的 FSD 授权。没有它，AP ECU 就没有载入神经网络权重。非 FSD 功能（nag killer、BMS 仪表板、限速提示音抑制、诊断）在任何支持 AP 的车上都能用。

**VIN 层级封禁怎么办？**
Tesla 自 2026 年 4 月起在服务器端封禁 VIN。封禁会把 `GTW_autopilot` 层级从 SELF_DRIVING 降到 ENHANCED，并移除 TLSSC 开关。**TLSSC Restore** 功能（0x331）可在 Palladium 与 HW4 上恢复停车标志／交通信号灯控制。完整研究见 [issue #18](https://github.com/hypery11/flipper-tesla-fsd/issues/18)。**GTW Config Replay**（0x7FF，前称「Ban Shield」）可实时重播先前学到的健康设置，但只在 CAN 广播层 — 不会还原底层的 NVRAM 或服务器端状态。

**Flipper Zero vs ESP32 — 该买哪个？**
ESP32 更便宜（$14 vs $200+），有 WiFi 仪表板、NVS 保存与深度睡眠。Flipper 更便携，且有内建屏幕。两者跑相同的 CAN 逻辑。如果你还没有 Flipper，选 ESP32。

**支持 Model S / Model X 吗？**
支持。Palladium S/X（2021+）已确认可用 TLSSC Restore。2021 前、做了 HW3 retrofit 的 S/X 通过 Legacy→HW3 自动升级可用。HW1/HW2 Model S/X 走 Legacy 模式（`0x3EE`）。Model S/X 使用不同的 BMS CAN ID — BMS 仪表板可能显示错误数值。

**这会不会把车搞坏（brick）？**
只动 UI 设置 frame。不会写入刹车、转向或动力系统。App 默认以 Listen-Only 模式开机。完整 TX 面清单见 [SECURITY.md](SECURITY.md)。

**一定要 Flipper CAN Add-On 吗？**
给 Flipper：是的，任何 MCP2515 模块（Electronic Cats、通用板子）都行。给 ESP32：多数支持的板子有内建 CAN 收发器（M5Stack ATOMIC CAN、Lilygo T-CAN485、Waveshare S3）。

---

## 相关项目

| 项目 | 是什么 | 硬件 |
|------|--------|------|
| [ev-open-can-tools](https://github.com/ev-open-can-tools/ev-open-can-tools) | 上游社区项目。开发活动在 GitHub 上（v3.0.x，GPL-3.0）。前身是 GitLab 上的 `Tesla-OPEN-CAN-MOD`；该组已改名为 `ev-open-can-tools`，GitLab repo 现已停摆（0 个开启中的 issue/MR，最后一次 commit 2026-04-25）— 请追踪 GitHub repo。 | RP2040 CAN、Feather M4、ESP32 |
| [dzid26/ESP32-DualCAN](https://github.com/dzid26/ESP32-DualCAN) | 「Dorky Commander」— S3XY Commander 的开源硬件替代品 | ESP32 + dual CAN |
| [tuncasoftbildik/tesla-can-mod](https://github.com/tuncasoftbildik/tesla-can-mod) | Arduino 参考实现，附 frame template | Arduino + MCP2515 |
| [tumik/S3XY-candump](https://github.com/tumik/S3XY-candump) | 通过 S3XY Commander（Panda 协议）的 Python CAN dump 工具 | Commander dongle |

## 致谢

- [commaai/opendbc](https://github.com/commaai/opendbc) — Tesla CAN 信号数据库
- [ElectronicCats/flipper-MCP2515-CANBUS](https://github.com/ElectronicCats/flipper-MCP2515-CANBUS) — Flipper 用 MCP2515 驱动
- 社区贡献者 — 本项目赖以运作的实车测试、抓包与研究：
  - **协议、nag killer 与 2026.14.x：** @jewelrylin（T-2CAN 双总线抓包、frame-content preflight 测试、X179 Service Mode 针脚图）、@DrStrangeglovebox（`0x370` 参考抓包 + HW4 双 CAN 数据 + 安全发现）、@ssw0209-sys（Mode-C 转向扭力参考 + HW4 14.x 测试）、@0xAccretion（HW4 Highland 中规 MIC DAS 布局发现，#116/#117）、@dunckencn（国行 HW3 start-after-AP 验证、steer-jerk 与 bus-off 报告）、@kristopf007（HW4 14.x 实车测试）
  - **功能、抓包与 PR：** @JakNo（ScrollPress AP / `0x3C2`）、@vrs11（Continuous AP）、@sqladm1n（RTC 抓包日志 PR + 总线/接线排查）、@DmitroPanteliuk（全速率 `0x229` 抓包）、@se7en7777777（`0x485` / Highland / 校验和分析）、@RoyRakete（TLSSC 封禁车组合）、@mamixsystem（post-SOP10 连接器参考;frame 级 14.x FSD-engage 决定性调查，#163）、@p0sixturtle（Summon / tier-selector 线索，#139）、@dahua910（RHD 需求，#66）、@HamzaObaidat（剧院模式 `0x118` 研究，#149）、@fboulegue（EU / 新线束 Juniper 报告，#143/#109/#110）、@densen2014（ESP32 HW 选择器建议 #110、TLSSC bit38 开关 PR #159、Summon 行驶中安全防护建议 #160）
  - **封禁研究、平台测试、ESP32、bug 修复：** @THER4iN、@MiniCS、@kp43h8、@gauner1986、@dmagyar、@ViPiMP、@marcobellinoroci-source、@danpadure、@bruvv、@Symness、@hkloudou、@nagotti、@patatman、@JordanzhaoD
- `Starmixcraft/tesla-fsd-can-mod` — 原始 CanFeather FSD 研究（GitLab repo 已被移除；镜像在 [Karolynaz/waymo-fsd-can-mod](https://github.com/Karolynaz/waymo-fsd-can-mod)）

## 支持这个研究

如果这个项目帮你省下了改装盒子的钱、让你看懂 Tesla 的 CAN bus，或在封禁后保住了你的 TLSSC，欢迎赞助持续的研究与测试。

[![Crypto](https://img.shields.io/badge/Crypto-Donate-F7931A?style=for-the-badge&logo=bitcoin&logoColor=white)](https://fsd.fkey.id/) [![PayPal](https://img.shields.io/badge/PayPal-Donate-00457C?style=for-the-badge&logo=paypal&logoColor=white)](https://www.paypal.com/cgi-bin/webscr?cmd=_xclick&business=hypery11@gmail.com&item_name=Tesla+FSD+Open+Source+Research&currency_code=USD) [![GitHub Sponsors](https://img.shields.io/badge/Sponsor-hypery11-EA4AAA?style=for-the-badge&logo=github&logoColor=white)](https://github.com/sponsors/hypery11)

加密货币请至 **[fsd.fkey.id](https://fsd.fkey.id/)** — 同一个地址、支持多链。实际可用的网络请直接打开页面查看。

款项用于测试用的 Tesla 零件（待救援的封禁 VIN、不同 MCU/HW 组合）、各种 ESP32 硬件，以及逆向新固件版本所花的时间。

## 授权

GPL-3.0

## 免责声明

仅供教育与研究用途。**FSD 是 Tesla 的付费功能，必须合法购买或订阅使用。** 改装车辆系统可能导致保修失效，也可能违反当地法规。使用者需自行承担所有责任与风险。完整安全与责任使用说明见 [`SECURITY.md`](SECURITY.md)。
