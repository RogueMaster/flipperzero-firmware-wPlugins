#!/usr/bin/env python3
"""Generate .firmware/web_i18n.inc — Chinese (zh-Hans + zh-Hant) translation layer
for the ESP32 web dashboard.

Reads .firmware/web_dashboard.cpp, validates every English key exists in the
served page (whitespace-normalized, HTML-entity-decoded — same as DOM
textContent), then emits web_i18n.inc: a single C++ raw string literal
containing the <script> translator block.

It is deliberately NOT wrapped in a #define macro: the xtensa GCC 8.4 shipped
with espressif32 6.9.0 rejects multi-line raw strings inside macro definitions
("unterminated raw string"). web_dashboard.cpp includes the .inc between the
page's own raw string literals instead:

    </script>
    )rawliteral"
    #include "web_i18n.inc"
    R"rawliteral(
    </body>

Design: the upstream English strings are NEVER modified. The translator runs
at page load (dictionary + EN/简体/繁體 toggle button injected into the page
header).

Behavior:
 - First visit: browser language decides (zh-Hant/TW/HK/MO -> Traditional,
   other zh-* -> Simplified, else English). detectLang() below.
 - Toggle cycles en -> zh-Hans -> zh-Hant -> en, persisted in localStorage
   ("teslaFsdLang"); document.documentElement.lang is set accordingly. Where
   storage is refused (captive-portal sheets) the choice rides in the URL
   fragment (#lang=...) instead, so it still survives the reload.
 - en -> zh-* translates live; any other switch reloads the page.
 - Live status updates and the page's own dialogs / toasts (uiAlert, uiConfirm,
   uiToast) are ordinary DOM text, translated via MutationObserver. Messages
   with a dynamic part (file name, size) rely on the prefix / suffix match in
   zhText(): put the variable text after a key, or between two keys.
 - Upstream strings missing from the tables simply stay English (graceful).
 - Text under an element with translate="no" stays English (the Acceleration
   Mode names, which the zh READMEs write in English too).

To update translations, edit the T table below and re-run:
    python3 gen_i18n.py
"""
import html
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, ".firmware", "web_dashboard.cpp")
OUT = os.path.join(HERE, ".firmware", "web_i18n.inc")

T = [
    # (English source, Simplified Chinese, Traditional Chinese).
    # English must match the page's textContent after whitespace normalization.
    ("Connection lost — retrying…", "连接已断开 — 正在重试…", "連線已中斷 — 正在重試…"),
    (
        "Command failed — the device did not accept it. Check the connection and try again.",
        "命令未生效 — 设备没有接受该命令。请检查连接后重试。",
        "指令未生效 — 裝置沒有接受這個指令。請檢查連線後再試一次。",
    ),
    (
        "⚠️ 2026.14.x firmware enforcement active.",
        "⚠️ 2026.14.x 固件限制已生效。",
        "⚠️ 2026.14.x 韌體限制已生效。",
    ),
    (
        "⚠️ IN-CAR AUTOPARK — CAN TX PAUSED",
        "⚠️ 车内自动泊车 — CAN 发送已暂停",
        "⚠️ 車內自動停車 — CAN 傳送已暫停",
    ),
    (
        "⚠️ OTA UPDATE IN PROGRESS — CAN TX SUSPENDED",
        "⚠️ OTA 更新进行中 — CAN 发送已挂起",
        "⚠️ OTA 更新進行中 — CAN 傳送已暫停",
    ),
    (
        "⚠️ Signal Map DAS id not seen on this bus.",
        "⚠️ 当前总线上未发现 Signal Map 的 DAS id。",
        "⚠️ 在此匯流排上未發現 Signal Map 的 DAS id。",
    ),
    (
        "⚠️ Volatile storage — download events before power-off; they are lost on reboot.",
        "⚠️ 易失性存储 — 断电前请下载事件记录，重启后会丢失。",
        "⚠️ 揮發性儲存 — 斷電前請下載事件記錄，重新開機後將遺失。",
    ),
    (
        "⚠️ Last save failed — low memory. The recorded event was dropped.",
        "⚠️ 上次保存失败 — 内存不足。本次事件记录已丢弃。",
        "⚠️ 上次儲存失敗 — 記憶體不足。本次事件記錄已捨棄。",
    ),
    (
        "— the standard parser can't read AP-state on this bus.",
        "— 标准解析器无法在此总线上读取 AP 状态。",
        "— 標準解析器無法在此匯流排上讀取 AP 狀態。",
    ),
    (
        "The configured DAS id isn't arriving, so AP-state can't be read and the nag killer is paused. Set",
        "配置的 DAS id 一直没有到来，无法读取 AP 状态，NAG 消除已暂停。设为",
        "設定的 DAS id 一直沒有出現，無法讀取 AP 狀態，NAG 消除已暫停。設為",
    ),
    (
        "for auto, or fix the mapping / tap.",
        "以自动检测，或修正映射 / 取线。",
        "以自動偵測，或修正映射 / 接線。",
    ),
    (
        "Tesla added a preflight check in 2026.14.x that disables autosteer the moment any CAN frame touches",
        "特斯拉在 2026.14.x 加入了预检：任何 CAN 帧只要碰到以下帧即禁用自动转向：",
        "特斯拉在 2026.14.x 加入了預檢：任何 CAN 訊框只要碰到以下訊框即停用自動轉向：",
    ),
    (". Symptom on the dash:", "。仪表盘上的症状：", "。儀表板上的症狀："),
    ('"Autopilot turning off"', '"Autopilot turning off"', '"Autopilot turning off"'),
    ("Autopilot turning off", "Autopilot turning off", "Autopilot turning off"),
    (
        "appears within a second of stalk engagement, then AP immediately disengages. Listen-Only mode is safe.",
        "在拨动拨杆约一秒后出现，随后 AP 立即退出。仅监听模式是安全的。",
        "在撥動撥桿約一秒後出現，隨後 AP 立即退出。僅監聽模式是安全的。",
    ),
    (
        "(delay injection until AP is engaged) is on the Flipper build only right now — on the ESP32, engage AP from the stalk first, then turn on injection. Dismiss if you're on pre-14.x firmware.",
        "（等 AP 启用后再延迟注入）目前仅 Flipper 版本支持 — 在 ESP32 上请先用拨杆启用 AP，再开启注入。如为 14.x 之前固件可忽略本提示。",
        "（等 AP 啟用後再延遲注入）目前僅 Flipper 版本支援 — 在 ESP32 上請先用撥桿啟用 AP，再開啟注入。若為 14.x 之前韌體可忽略本提示。",
    ),
    ("FSD Status", "FSD 状态", "FSD 狀態"),
    ("AP Status", "AP 状态", "AP 狀態"),
    ("NAG Killer", "NAG 消除", "NAG 消除"),
    ("Controls", "控制", "控制"),
    ("CAN Bus", "CAN 总线", "CAN 匯流排"),
    ("Black-box", "黑匣子", "黑盒子"),
    ("CAN Dump", "CAN 转储", "CAN 側錄"),
    # MCP2515 receive task toggle and its status line.
    (
        "MCP2515 RX task (experimental)",
        "MCP2515 接收任务（实验性）",
        "MCP2515 接收任務（實驗性）",
    ),
    (
        "Reads the MCP2515 from its own task into a 256-frame queue, so a slow loop pass no longer loses frames from its two receive buffers.",
        "由独立任务读取 MCP2515，放入 256 帧的队列，主循环一时变慢也不会再丢掉它两个接收缓冲区里的帧。",
        "由獨立任務讀取 MCP2515，放入 256 幀的佇列，主迴圈一時變慢也不會再遺失它兩個接收緩衝區裡的幀。",
    ),
    ("Running, INT pin", "运行中，INT 引脚唤醒", "執行中，INT 腳位喚醒"),
    ("Running, 1 ms poll", "运行中，每 1 ms 轮询", "執行中，每 1 ms 輪詢"),
    ("queue max", "队列峰值", "佇列峰值"),
    ("lost on a full queue", "队列满时丢失", "佇列滿時遺失"),
    ("chip overflows", "芯片溢出", "晶片溢位"),
    ("Waiting for the MCP2515", "等待 MCP2515", "等待 MCP2515"),
    ("Off (read from the loop)", "关闭（由主循环读取）", "關閉（由主迴圈讀取）"),
    ("HTTP CAN Log", "HTTP CAN 日志", "HTTP CAN 日誌"),
    ("BMS Display", "BMS 显示", "BMS 顯示"),
    (
        "Prints voltage, current, power, SoC and pack temperature to the USB serial log once a second. The dashboard shows them either way.",
        "每秒把电压、电流、功率、电量和电池包温度输出到 USB 串口日志。仪表盘照常显示这些数据。",
        "每秒把電壓、電流、功率、電量和電池組溫度輸出到 USB 序列埠日誌。儀表板照常顯示這些資料。",
    ),
    ("WiFi Configuration", "WiFi 配置", "WiFi 設定"),
    ("OTA Firmware Update", "OTA 固件更新", "OTA 韌體更新"),
    ("SD Card", "SD 卡", "SD 卡"),
    ("Storage", "存储", "儲存空間"),
    ("Device", "设备", "裝置"),
    ("Hardware", "硬件", "硬體"),
    ("Firmware", "固件", "韌體"),
    ("Activate", "激活", "啟用"),
    ("Deactivate", "停用", "停用"),
    ("⚑ MARK NOW", "⚑ 立即标记", "⚑ 立即標記"),
    ("RE-CHECK", "重新检测", "重新檢測"),
    ("Tap Check", "取线检测", "接線檢查"),
    ("SAVE & RESTART WIFI", "保存并重启 WiFi", "儲存並重啟 WiFi"),
    (
        "WiFi settings will be updated and the device will restart.",
        "WiFi 设置将被更新，设备将重启。",
        "將更新 WiFi 設定，裝置會重新啟動。",
    ),
    ("RESTART DEVICE", "重启设备", "重啟裝置"),
    ("Restart device?", "确定重启设备吗？", "確定要重啟裝置嗎？"),
    (
        "The device will reboot immediately and the web connection will drop briefly.",
        "设备将立即重启，网页连接会短暂中断。",
        "裝置將立即重啟，網頁連線會短暫中斷。",
    ),
    ("Cancel", "取消", "取消"),
    ("OK", "确定", "確定"),
    ("Dismiss", "忽略", "忽略"),
    ("Got it", "知道了", "知道了"),
    ("Sign In", "登录", "登入"),
    ("Connect to WiFi", "连接 WiFi", "連接 WiFi"),
    ("SELECT FIRMWARE (.bin)", "选择固件（.bin）", "選擇韌體（.bin）"),
    (
        "Upload a .bin firmware file. Device will reboot after a successful update.",
        "上传 .bin 固件文件，更新成功后设备将重启。",
        "上傳 .bin 韌體檔案，更新成功後裝置將重啟。",
    ),
    (
        "Error: Please select a .bin firmware file",
        "错误：请选择 .bin 固件文件",
        "錯誤：請選擇 .bin 韌體檔案",
    ),
    (
        "Error: Firmware file too large (max 16MB)",
        "错误：固件文件过大（最大 16MB）",
        "錯誤：韌體檔案過大（上限 16MB）",
    ),
    # Prefix keys: the dialog appends " (<size> KB)." / " <file> (<size> KB)?".
    ("Warning: This file is very small", "警告：此文件非常小", "警告：這個檔案非常小"),
    (
        "Are you sure it is a valid ESP32 firmware?",
        "确定这是有效的 ESP32 固件吗？",
        "確定這是有效的 ESP32 韌體嗎？",
    ),
    ("Flash firmware:", "刷入固件", "燒錄韌體"),
    (
        "Device will reboot after update.",
        "更新后设备将重启。",
        "更新後裝置會重新啟動。",
    ),
    ("STREAM LOG AND SAVE", "开始串流并保存", "開始串流並儲存"),
    (
        "Ready to collect a candump file in this browser.",
        "已就绪，可在此浏览器中采集 candump 文件。",
        "已就緒，可在此瀏覽器中擷取 candump 檔案。",
    ),
    ("FORMAT SD CARD", "格式化 SD 卡", "格式化 SD 卡"),
    (
        "Some files or folders could not be deleted (nested too deep, or refused by the card)",
        "部分文件或文件夹无法删除（嵌套过深，或被卡拒绝）",
        "部分檔案或資料夾無法刪除（巢狀過深，或遭記憶卡拒絕）",
    ),
    (
        "Format SD card? All data will be lost.",
        "确定格式化 SD 卡吗？所有数据都将丢失。",
        "確定要格式化 SD 卡嗎？所有資料都會遺失。",
    ),
    ("DELETE ALL EVENTS", "删除全部事件", "刪除全部事件"),
    (
        "Delete all recorded events from the device?",
        "确定删除设备上所有已记录的事件吗？",
        "確定刪除裝置上所有已記錄的事件嗎？",
    ),
    ("No events recorded yet.", "暂无已记录的事件。", "尚無已記錄的事件。"),
    ("Save mapping", "保存映射", "儲存映射"),
    ("Apply this profile", "应用此配置", "套用此設定檔"),
    ("Expand to setup", "展开设置", "展開設定"),
    ("Live", "Live", "Live"),
    ("Idle", "空闲", "閒置"),
    ("Active", "已激活", "已啟用"),
    ("Disabled", "已禁用", "已停用"),
    ("Off", "关闭", "關閉"),
    ("No", "否", "否"),
    ("Yes", "是", "是"),
    ("NEW", "新增", "新增"),
    ("Ready", "就绪", "就緒"),
    ("Recording", "录制中", "錄製中"),
    ("Streaming", "串流中", "串流中"),
    ("Collecting", "采集中", "擷取中"),
    ("Preparing...", "准备中…", "準備中…"),
    ("Demo", "Demo", "Demo"),
    ("Dev", "Dev", "Dev"),
    ("Mode", "模式", "模式"),
    ("Listen-Only", "仅监听", "僅監聽"),
    (
        "AP Branch/Tier (experimental)",
        "AP 分支/层级（实验性）",
        "AP 分支/層級（實驗性）",
    ),
    ("Continuous AP", "持续 AP", "持續 AP"),
    ("Force FSD", "强制 FSD", "強制 FSD"),
    ("FSD activate", "FSD 激活", "FSD 啟用"),
    ("Force HW3", "强制 HW3", "強制 HW3"),
    ("Force HW4", "强制 HW4", "強制 HW4"),
    ("Force Legacy", "强制 Legacy", "強制 Legacy"),
    ("China Mode", "中国模式", "中國模式"),
    ("Right-Hand Drive (RHD)", "右舵驾驶（RHD）", "右駕（RHD）"),
    (
        "RHD markets only — do NOT enable while driving on the right.",
        "仅限右舵市场 — 在靠右行驶地区行车时请勿启用。",
        "僅限右駕市場 — 在靠右行駛地區行車時請勿啟用。",
    ),
    ("FSD Unlock", "FSD 解锁", "FSD 解鎖"),
    ("Summon EU Unlock", "欧盟版召唤解锁", "歐盟版召喚解鎖"),
    ("Track Mode (experimental)", "赛道模式（实验性）", "賽道模式（實驗性）"),
    ("Charge Port", "充电口", "充電埠"),
    (
        "0x25D CP_status — Vehicle CAN only",
        "0x25D CP_status — 仅在车辆 CAN 上",
        "0x25D CP_status — 僅在車輛 CAN 上",
    ),
    (
        "Stale: no CP_status from the car for",
        "未更新：车辆已停止发送 CP_status，已持续",
        "未更新：車輛已停止傳送 CP_status，已持續",
    ),
    (
        "no CP_status on this tap (the 8-byte 0x25D, Vehicle CAN only)",
        "此接入点没有 CP_status（8 字节的 0x25D，仅在车辆 CAN 上）",
        "此接線點沒有 CP_status（8 位元組的 0x25D，僅在車輛 CAN 上）",
    ),
    (
        "EU Charge-Port Type (experimental)",
        "欧盟充电口类型（实验性）",
        "歐盟充電埠類型（實驗性）",
    ),
    (
        "Re-sends 0x25D on the Vehicle bus with CP_type set to IEC CCS so EU Superchargers show in nav. Held while a cable is plugged in or charging. Experimental.",
        "在车辆总线上重发 0x25D，把 CP_type 设为 IEC CCS，使导航显示欧盟超充。插着充电线或充电时暂停。实验性。",
        "在車輛匯流排上重送 0x25D，把 CP_type 設為 IEC CCS，使導航顯示歐盟超充。插著充電線或充電時暫停。實驗性。",
    ),
    # Acceleration Mode status line (#211). Mode names stay English, as the
    # zh READMEs write them.
    ("Car", "车辆", "車輛"),
    ("sent", "发出", "送出"),
    ("pass-through", "原样转发", "原樣轉送"),
    ("pending, applies at standstill", "待生效，静止时切换", "待生效，靜止時切換"),
    ("TX off", "发送已关闭", "傳送已關閉"),
    ("Compressor Overclock", "压缩机超频", "壓縮機超頻"),
    ("Post-drive Cooling", "停车后散热", "停車後散熱"),
    ("Precondition", "电池预处理", "電池預處理"),
    (
        "battery preheat trigger (0x082)",
        "电池预热触发（0x082）",
        "電池預熱觸發（0x082）",
    ),
    ("max cooling", "最大制冷", "最大冷卻"),
    ("Continue on Green", "绿灯继续通行", "綠燈繼續通行"),
    ("Suppress Chime", "屏蔽提示音", "關閉提示音"),
    ("Stability Assist", "稳定辅助", "穩定輔助"),
    ("Handling Balance", "操控平衡", "操控平衡"),
    ("(stable → rotation)", "（稳定 → 旋转）", "（穩定 → 旋轉）"),
    ("Stealth Mode (Hidden)", "隐身模式（隐藏）", "隱身模式（隱藏）"),
    ("Telemetry Off (experimental)", "关闭遥测（实验性）", "關閉遙測（實驗性）"),
    ("Soft Engage", "柔和启用", "柔和啟用"),
    (
        "Soft Engage (14.x, exp.)",
        "柔和启用（14.x，实验性）",
        "柔和啟用（14.x，實驗性）",
    ),
    ("Instant Engage (exp.)", "即时启用（实验性）", "即時啟用（實驗性）"),
    ("Minimal Inject (exp.)", "最小注入（实验性）", "最小注入（實驗性）"),
    ("AP-First", "AP 优先", "AP 優先"),
    ("AP-First (14.x)", "AP 优先（14.x）", "AP 優先（14.x）"),
    ("Nag Burst (14.x, exp.)", "NAG 连发（14.x，实验性）", "NAG 連發（14.x，實驗性）"),
    (
        "Nag EPAS-faithful (14.x, exp.)",
        "NAG EPAS 高还原（14.x，实验性）",
        "NAG EPAS 高還原（14.x，實驗性）",
    ),
    (
        "Abort Guard (14.x, exp.)",
        "中断保护（14.x，实验性）",
        "中斷保護（14.x，實驗性）",
    ),
    ("TLSSC Restore", "TLSSC 恢复", "TLSSC 還原"),
    ("pairs with TLSSC", "与 TLSSC 联动", "與 TLSSC 連動"),
    ("Signal Map (advanced, 14.x)", "信号映射（高级，14.x）", "訊號映射（進階，14.x）"),
    (
        "Override where the nag killer reads AP-state / hands-on / steering. Leave DAS id",
        "覆盖 NAG 消除读取 AP 状态 / 手握检测 / 转向信号的位置。DAS id 保持为 0 则",
        "覆寫 NAG 消除讀取 AP 狀態 / 手握偵測 / 轉向訊號的位置。DAS id 保持為 0 則",
    ),
    ("DAS id (0x..)", "DAS id（0x..）", "DAS id（0x..）"),
    ("Steer id (0x..)", "转向 id（0x..）", "轉向 id（0x..）"),
    ("Steer hi/lo byte", "转向高/低字节", "轉向高/低位元組"),
    (
        "Steer id 0 or 0x129: the car's 0x129 steering angle, read with its own layout and only while valid; hi/lo are not used. Another id: a signed 16-bit angle at the hi/lo bytes, 0.1° per bit; hi = lo maps nothing.",
        "转向 id 为 0 或 0x129：使用车辆 0x129 的转向角，按其自身格式解析，且只在有效时使用；不使用 hi/lo。其他 id：hi/lo 字节处的有符号 16 位角度，每单位 0.1°；hi = lo 表示不映射。",
        "轉向 id 為 0 或 0x129：使用車輛 0x129 的轉向角，依其本身格式解析，且只在有效時使用；不使用 hi/lo。其他 id：hi/lo 位元組處的有號 16 位元角度，每單位 0.1°；hi = lo 表示不映射。",
    ),
    ("AP-state byte/sh/mask", "AP 状态 字节/位移/掩码", "AP 狀態 位元組/位移/遮罩"),
    ("Hands-on byte/sh/mask", "手握检测 字节/位移/掩码", "手握偵測 位元組/位移/遮罩"),
    ("Auto-detect", "自动检测", "自動偵測"),
    (
        "Auto-detect needs 0x398 — many Model 3/Y never send it. Pick your car if detection is wrong.",
        "自动检测需要 0x398 — 很多 Model 3/Y 从不发送该帧，若识别错误请手动选择车型。",
        "自動偵測需要 0x398 — 許多 Model 3/Y 從不傳送該訊框，若辨識錯誤請手動選擇車型。",
    ),
    ("Best guess:", "最佳推测：", "最佳推測："),
    (
        "— confirm with your car's wiring diagram (Service Mode → CAN Port where available)",
        "— 请对照车辆的接线图确认（车上有 Service Mode → CAN Port 页面的也可在那里查看）",
        "— 請對照車輛的線路圖確認（車上有 Service Mode → CAN Port 頁面的也可在那裡查看）",
    ),
    (
        "for auto-detect. byte 0-7, shift 0-7, mask hex.",
        "用于自动检测。字节 0-7，位移 0-7，掩码为十六进制。",
        "用於自動偵測。位元組 0-7，位移 0-7，遮罩為十六進位。",
    ),
    (
        "Experimental & non-persistent — injects a UI branch/tier hint only, reverts when injection stops; unverified and may be a ban signal. Off by default.",
        "实验性且不持久 — 仅注入 UI 分支/层级提示，停止注入后恢复；未经验证，可能成为封禁依据。默认关闭。",
        "實驗性且不持久 — 僅注入 UI 分支/層級提示，停止注入後還原；未經驗證，可能成為停權依據。預設關閉。",
    ),
    (
        "Experimental & unverified — clears reachable telemetry flags only (not the Vehicle-bus ECU log-upload). Does NOT guarantee reduced detection.",
        "实验性且未经验证 — 仅清除可达的遥测标记（不影响车辆总线 ECU 的日志上传），不保证降低被检测概率。",
        "實驗性且未經驗證 — 僅清除可觸及的遙測旗標（不影響車輛匯流排 ECU 的日誌上傳），不保證降低被偵測機率。",
    ),
    (
        "Experimental — Vehicle-bus; not car-validated. Defaults to rear-biased (rotation 100) + 30% stability — fun with a safety margin. Raise stability for stock feel.",
        "实验性 — 车辆总线；未经实车验证。默认为偏后驱（rotation 100）+ 30% 稳定性 — 在安全余量内体验乐趣，提高稳定性可获得接近原厂的感受。",
        "實驗性 — 車輛匯流排；未經實車驗證。預設為偏後驅（rotation 100）+ 30% 穩定性 — 在安全餘裕內體驗樂趣，提高穩定性可獲得接近原廠的感受。",
    ),
    ("CAN Errors", "CAN 错误", "CAN 錯誤"),
    ("CAN Vehicle", "车辆 CAN", "車輛 CAN"),
    ("Dump Status", "转储状态", "擷取狀態"),
    ("Filter IDs", "过滤 ID", "過濾 ID"),
    ("RX Frames", "接收帧", "接收訊框"),
    ("TX Frames", "发送帧", "傳送訊框"),
    ("Frames/s", "帧/秒", "訊框/秒"),
    ("Dropped", "丢弃", "丟棄"),
    ("Buffered", "已缓冲", "已緩衝"),
    ("Filtered", "已过滤", "已過濾"),
    ("0 frames", "0 帧", "0 訊框"),
    ("frames / rx-missed", "帧 / 接收丢失", "訊框 / 接收遺失"),
    # CAN Bus card: counter reset (#252), per-bus table (#253).
    ("Reset", "清零", "歸零"),
    (
        "Reset the CAN Bus counters?",
        "要把 CAN 总线计数清零吗？",
        "要把 CAN 匯流排計數歸零嗎？",
    ),
    (
        "RX, TX, the error counts and frames/s start again from zero on every open dashboard. Black-box, CAN captures and Tap Check keep their own counts.",
        "接收、发送、错误计数和帧/秒会从零重新开始，所有打开的仪表盘同步生效。黑匣子、CAN 采集和取线检测各自计数，不受影响。",
        "接收、傳送、錯誤計數和訊框/秒會從零重新開始，所有開啟的儀表板同步生效。黑盒子、CAN 擷取和接線檢查各自計數，不受影響。",
    ),
    ("Since boot", "开机以来", "開機以來"),
    ("Since reset", "清零以来", "歸零以來"),
    ("RX missed", "接收丢失", "接收遺失"),
    # "bus" = bus errors, in the CAN Errors tile's breakdown.
    ("bus", "总线错误", "匯流排錯誤"),
    ("Bus errors", "总线错误", "匯流排錯誤"),
    ("TX fail", "发送失败", "傳送失敗"),
    ("not detected", "未检测到", "未偵測到"),
    (
        "No CAN frames collected. Nothing saved.",
        "未采集到 CAN 帧，未保存任何内容。",
        "未擷取到 CAN 訊框，未儲存任何內容。",
    ),
    (
        "No frames seen — check wiring / that the car is awake.",
        "未收到任何帧 — 请检查接线 / 确认车辆已唤醒。",
        "未收到任何訊框 — 請檢查接線 / 確認車輛已喚醒。",
    ),
    (
        "no 0x370 on this tap — wrong bus for the nag killer",
        "此接入点没有 0x370 — NAG 消除接错了总线",
        "此接線點沒有 0x370 — NAG 消除接錯了匯流排",
    ),
    (
        "HW unconfirmed — 0x399 reading assumed; verdict may change once HW is detected.",
        "硬件未确认 — 暂按 0x399 读数判断，硬件识别后结论可能变化。",
        "硬體未確認 — 暫按 0x399 讀數判斷，硬體辨識後結論可能改變。",
    ),
    ("Looks like variant", "疑似变体", "疑似變體"),
    (
        "Connect to run a check, or press Re-check.",
        "连接后运行检测，或点击重新检测。",
        "連接後執行檢測，或按重新檢測。",
    ),
    (
        "Listens a few seconds and reports whether each feature can work on the bus this device is tapped into. Pure read-only — nothing is transmitted.",
        "监听数秒，报告各功能在当前接入总线上是否可用。纯只读 — 不发送任何数据。",
        "監聽數秒，報告各功能在目前接入匯流排上是否可用。純唯讀 — 不傳送任何資料。",
    ),
    ("No CAN Traffic", "无 CAN 流量", "無 CAN 流量"),
    ("Body/comfort bus", "车身/舒适总线", "車身/舒適匯流排"),
    ("Bus hint", "总线提示", "匯流排提示"),
    (
        "no 0x3C2 and no body frames — no hint",
        "没有 0x3C2，也没有车身帧 — 无提示",
        "沒有 0x3C2，也沒有車身訊框 — 無提示",
    ),
    (
        "0x3C2 seen — likely the Vehicle CAN pair (a hint, not a verdict)",
        "看到 0x3C2 — 可能是车辆 CAN 线对（仅供参考，不是定论）",
        "看到 0x3C2 — 可能是車輛 CAN 線對（僅供參考，不是定論）",
    ),
    (
        "body frames without 0x3C2 — likely a forwarded subset, not the Vehicle CAN pair (a hint, not a verdict)",
        "有车身帧但没有 0x3C2 — 可能是网关转发的子集，而不是车辆 CAN 线对（仅供参考，不是定论）",
        "有車身訊框但沒有 0x3C2 — 可能是閘道轉發的子集，而不是車輛 CAN 線對（僅供參考，不是定論）",
    ),
    ("DAS state readable", "DAS 状态可读", "DAS 狀態可讀"),
    # Tap Check card (#125): feature rows, verdict lines, reachable frames and
    # the firmware's bus-family hint.
    ("Nag killer", "NAG 消除", "NAG 消除"),
    ("frames", "帧", "訊框"),
    (
        "0x370 + DAS state present — gates correctly",
        "0x370 与 DAS 状态都在 — 可正常判断",
        "0x370 與 DAS 狀態都在 — 可正常判斷",
    ),
    (
        "0x370 here but no DAS state — dual-CAN recommended (read DAS on a second tap)",
        "有 0x370 但没有 DAS 状态 — 建议双路 CAN（在第二个接入点读取 DAS）",
        "有 0x370 但沒有 DAS 狀態 — 建議雙路 CAN（在第二個接線點讀取 DAS）",
    ),
    (
        "no DAS state here (need 0x399/0x39B)",
        "此处没有 DAS 状态（需要 0x399/0x39B）",
        "這裡沒有 DAS 狀態（需要 0x399/0x39B）",
    ),
    (
        "AP control frame present (0x3FD/0x3EE)",
        "有 AP 控制帧（0x3FD/0x3EE）",
        "有 AP 控制訊框（0x3FD/0x3EE）",
    ),
    (
        "no AP control frame to modify here",
        "此处没有可修改的 AP 控制帧",
        "這裡沒有可修改的 AP 控制訊框",
    ),
    ("0x129 steering angle present", "有 0x129 转向角", "有 0x129 轉向角"),
    (
        "no 0x129 — degrades to AP-First-only",
        "没有 0x129 — 降级为仅 AP 优先",
        "沒有 0x129 — 降級為僅 AP 優先",
    ),
    (
        "0x129 present, no valid angle yet",
        "有 0x129，但还没有有效的转向角",
        "有 0x129，但還沒有有效的轉向角",
    ),
    (
        "Vehicle/body bus reachable — mirror/window/lights frames present (RX only; not proof of actuation)",
        "可接入车辆/车身总线 — 有后视镜/车窗/灯光帧（仅接收，不代表能实际控制）",
        "可接到車輛/車身匯流排 — 有後照鏡/車窗/燈光訊框（僅接收，不代表能實際控制）",
    ),
    (
        "not on this tap — needs the X179 A-pillar (Vehicle-bus) tap",
        "不在此接入点 — 需要 X179 A 柱（车辆总线）接入点",
        "不在此接線點 — 需要 X179 A 柱（車輛匯流排）接線點",
    ),
    ("mirror/lock/horn", "后视镜/门锁/喇叭", "後照鏡/門鎖/喇叭"),
    ("windows", "车窗", "車窗"),
    ("lights/turn", "灯光/转向灯", "燈光/方向燈"),
    ("looks Party-like", "看起来像 Party CAN", "看起來像 Party CAN"),
    ("looks Chassis/Vehicle-like", "看起来像底盘/车辆 CAN", "看起來像底盤/車輛 CAN"),
    # List separator between the reachable frames (one text node each).
    (",", "、", "、"),
    (
        "Records the key diagnostic CAN IDs around anomalies (aborts, bus-off, manual marks) to the device only — never uploaded. Use the toggle below to enable or disable.",
        "仅在设备本地记录异常（中断、bus-off、手动标记）前后的关键诊断 CAN ID — 永不上传。用下方开关启用或禁用。",
        "僅在裝置本機記錄異常（中斷、bus-off、手動標記）前後的關鍵診斷 CAN ID — 永不上傳。用下方開關啟用或停用。",
    ),
    ("Auto-record", "自动录制", "自動錄製"),
    (
        "HTTP CAN log is available only in Listen-Only mode.",
        "HTTP CAN 日志仅在仅监听模式下可用。",
        "HTTP CAN 日誌僅在僅監聽模式下可用。",
    ),
    (
        "Switch to Listen-Only mode before starting HTTP CAN log.",
        "启动 HTTP CAN 日志前，请先切换到仅监听模式。",
        "啟動 HTTP CAN 日誌前，請先切換到僅監聽模式。",
    ),
    (
        "Log ready in phone memory:",
        "日志已就绪（手机内存）：",
        "日誌已就緒（手機記憶體）：",
    ),
    ("Save/share requested for", "已请求保存/分享：", "已要求儲存/分享："),
    ("Share cancelled or failed:", "分享已取消或失败：", "分享已取消或失敗："),
    ("Server response:", "服务器响应：", "伺服器回應："),
    (
        "This browser does not support HTTP stream collection.",
        "此浏览器不支持 HTTP 串流采集。",
        "此瀏覽器不支援 HTTP 串流擷取。",
    ),
    (". Trying download link...", "。正在尝试下载链接…", "。正在嘗試下載連結…"),
    ("Connecting to HTTP stream...", "正在连接 HTTP 串流…", "正在連接 HTTP 串流…"),
    ("connection closed", "连接已关闭", "連線已關閉"),
    ("Stream stopped:", "串流已停止：", "串流已停止："),
    (
        "The device starts its own access point by default. Optionally set a network below; when a network name is set, the device tries to connect to it on boot and starts its own access point if it cannot connect.",
        "设备默认会启动自带热点。也可在下方设置网络；设置网络名称后，设备启动时会优先连接该网络，失败则回退到自带热点。",
        "裝置預設會啟動自帶熱點。也可在下方設定網路；設定網路名稱後，裝置啟動時會優先連接該網路，失敗則退回自帶熱點。",
    ),
    ("Network Name", "网络名称", "網路名稱"),
    ("Network Password", "网络密码", "網路密碼"),
    ("Hostname", "主机名", "主機名稱"),
    ("name shown on your router", "路由器上显示的名称", "路由器上顯示的名稱"),
    ("Password", "密码", "密碼"),
    ("Access Point", "接入点", "存取點"),
    (
        "Enter the admin username and the WiFi AP password.",
        "请输入管理员用户名和 WiFi 热点密码。",
        "請輸入管理員使用者名稱和 WiFi 熱點密碼。",
    ),
    ("Authentication Required", "需要身份验证", "需要驗證"),
    ("Username", "用户名", "使用者名稱"),
    ("Authentication failed", "身份验证失败", "驗證失敗"),
    ("Authorization", "授权", "授權"),
    ("No OTA Partition", "无 OTA 分区", "無 OTA 分割區"),
    ("No OTA partition", "无 OTA 分区", "無 OTA 分割區"),
    ("OTA Partition", "OTA 分区", "OTA 分割區"),
    ("Partition Safety", "分区安全", "分割區安全"),
    ("Ignore OTA", "忽略 OTA", "忽略 OTA"),
    ("Deep Sleep (sec)", "深度睡眠（秒）", "深層睡眠（秒）"),
    ("Display Brightness (%)", "显示亮度（%）", "顯示器亮度（%）"),
    ("Display Timeout (s)", "显示超时（秒）", "顯示逾時（秒）"),
    ("Administration", "管理", "管理"),
    ("Voltage", "电压", "電壓"),
    ("Current", "电流", "電流"),
    ("Pack temp min~max", "电池包温度 最低~最高", "電池組溫度 最低~最高"),
    ("unconfirmed on some cars", "部分车型未确认", "部分車型未確認"),
    # Performance card: switch text, tiles, advisory and the notes.
    ("Performance", "性能", "性能"),
    (
        "Read-only read-out derived from frames already on the bus — it transmits nothing. Enable to show acceleration / braking timers, an estimated G (no IMU), wheel slip and pack temperature / power. Speed & est. G need any tap carrying 0x257; wheel slip needs Party CAN (0x175); temp & power need a Vehicle-CAN tap (BMS).",
        "只读读数，由总线上已有的帧推算 — 不发送任何数据。启用后显示加速 / 制动计时、估算 G 值（无 IMU）、车轮打滑以及电池包温度 / 功率。车速和估算 G 需要任一带 0x257 的接入点；车轮打滑需要 Party CAN（0x175）；温度和功率需要车辆 CAN 接入点（BMS）。",
        "唯讀讀數，由匯流排上已有的訊框推算 — 不傳送任何資料。啟用後顯示加速 / 煞車計時、估算 G 值（無 IMU）、車輪打滑以及電池組溫度 / 功率。車速和估算 G 需要任一帶 0x257 的接線點；車輪打滑需要 Party CAN（0x175）；溫度和功率需要車輛 CAN 接線點（BMS）。",
    ),
    ("Vehicle speed", "车速", "車速"),
    ("100-0 km/h brake", "100-0 km/h 制动", "100-0 km/h 煞車"),
    ("best", "最佳", "最佳"),
    ("Est. long. G", "估算纵向 G", "估算縱向 G"),
    ("peak", "峰值", "峰值"),
    ("Wheel slip", "车轮打滑", "車輪打滑"),
    ("Pack temp", "电池包温度", "電池組溫度"),
    ("Pack power", "电池包功率", "電池組功率"),
    ("Battery advisory", "电池提示", "電池提示"),
    ("braking", "制动中", "煞車中"),
    ("n/a", "无数据", "無資料"),
    (
        "Cold pack — reduced regen and power until warmed",
        "电池包偏冷 — 升温前动能回收和功率会降低",
        "電池組偏冷 — 升溫前動能回收和功率會降低",
    ),
    (
        "Hot pack — power may be limited",
        "电池包偏热 — 功率可能受限",
        "電池組偏熱 — 功率可能受限",
    ),
    ("Normal", "正常", "正常"),
    (
        "No BMS frames (needs Vehicle-CAN tap)",
        "没有 BMS 帧（需要车辆 CAN 接入点）",
        "沒有 BMS 訊框（需要車輛 CAN 接線點）",
    ),
    (
        "G is estimated from dv/dt of vehicle speed (no IMU). Timers auto-arm leaving a standstill, lock at the target, and reset at the next standstill; best time is kept per session. Temp / power need a Vehicle-CAN tap; wheel slip needs 0x175.",
        "G 值由车速的 dv/dt 估算（无 IMU）。计时在离开静止时自动开始，到达目标时锁定，下次静止时重置；最佳成绩在本次运行期间保留。温度 / 功率需要车辆 CAN 接入点；车轮打滑需要 0x175。",
        "G 值由車速的 dv/dt 估算（無 IMU）。計時在離開靜止時自動開始，到達目標時鎖定，下次靜止時重設；最佳成績在本次執行期間保留。溫度 / 功率需要車輛 CAN 接線點；車輪打滑需要 0x175。",
    ),
    ("Pack SOC", "电池包电量", "電池組電量"),
    ("Car SOC", "车辆显示电量", "車輛顯示電量"),
    (
        "the % on the car's screen (0x33A)",
        "车辆屏幕上显示的百分比（0x33A）",
        "車輛螢幕上顯示的百分比（0x33A）",
    ),
    ("Battery", "电池", "電池"),
    ("Tyre Pressure", "胎压", "胎壓"),
    ("Receiving", "接收中", "接收中"),
    ("Stale", "未更新", "未更新"),
    ("No reading", "无读数", "無讀數"),
    ("Since last reading", "距上次读数", "距上次讀數"),
    ("Car's target", "车辆目标胎压", "車輛目標胎壓"),
    ("Unit", "单位", "單位"),
    (
        "Read-only read-out of the tyre pressures the car sends on 0x219 — it transmits nothing. Turn on to show them.",
        "只读显示车辆在 0x219 上发送的胎压 — 不发送任何数据。打开开关即可显示。",
        "唯讀顯示車輛在 0x219 上傳送的胎壓 — 不傳送任何資料。打開開關即可顯示。",
    ),
    ("FL", "左前", "左前"),
    ("FR", "右前", "右前"),
    ("RL", "左后", "左後"),
    ("RR", "右后", "右後"),
    (
        "Columns follow the order the car sends them. Which wheel each one belongs to is not confirmed yet.",
        "各列按车辆发送的顺序排列，尚未确认分别对应哪个车轮。",
        "各欄依車輛傳送的順序排列，尚未確認分別對應哪個車輪。",
    ),
    (
        "0x219 is arriving on this tap, but the car is not reporting a pressure in it.",
        "此接入点收到了 0x219，但车辆没有在其中报告胎压。",
        "此接線點收到了 0x219，但車輛沒有在其中回報胎壓。",
    ),
    (
        "This 0x219 has a length this firmware does not read yet. A capture of it would help.",
        "本固件尚无法解读此长度的 0x219，欢迎采集该帧并提供。",
        "本韌體尚無法解讀此長度的 0x219，歡迎擷取該訊框並提供。",
    ),
    ("Uptime", "运行时间", "運行時間"),
    ("BMS Frames", "BMS 帧", "BMS 訊框"),
    ("BMS Status", "BMS 状态", "BMS 狀態"),
    ("Free & open source ·", "免费开源 ·", "免費開源 ·"),
    ("ESP32 CAN Controller ·", "ESP32 CAN 控制器 ·", "ESP32 CAN 控制器 ·"),
    ("support the research", "支持本研究", "支持本研究"),
    ("mirror read-back", "后视镜状态回读", "後照鏡狀態回讀"),
    ("optional", "可选", "選填"),
    ("default", "默认", "預設"),
    # Black-box card: the Storage line and each event's frame count.
    ("internal RAM", "内部 RAM", "內部 RAM"),
    ("saved", "条已保存", "筆已儲存"),
    ("single", "单路", "單路"),
    ("dual-CAN", "双路 CAN", "雙路 CAN"),
    ("Legacy", "Legacy", "Legacy"),
    ("del", "删除", "刪除"),
    ("Status", "状态", "狀態"),
    ("State", "状态", "狀態"),
    ("Stream", "串流", "串流"),
    ("Stage", "Stage", "Stage"),
    ("Stage2", "Stage2", "Stage2"),
    ("Listening on the bus…", "正在监听总线…", "正在監聽匯流排…"),
    ("Reachable frames:", "可达帧：", "可達訊框："),
    (
        "(presence only — not proof injection actuates them).",
        "（仅表示存在 — 不能证明注入能实际生效）。",
        "（僅表示存在 — 不能證明注入能實際生效）。",
    ),
    ("Waiting Frames", "等待帧", "等待訊框"),
    ("Waiting", "等待中", "等待中"),
    ("Capturing", "记录中", "記錄中"),
    ("Armed", "待命", "待命"),
    ("Detected", "已检测到", "已偵測到"),
    ("Listening…", "监听中…", "監聽中…"),
    ("Done", "完成", "完成"),
    ("ON", "开", "開"),
    ("OFF", "关", "關"),
    ("Device restart triggered", "已触发设备重启", "已觸發裝置重啟"),
    (
        "Network password must be empty or 8+ chars",
        "网络密码须为空或 8 位以上",
        "網路密碼須為空或 8 位以上",
    ),
    (
        "Password must be empty or 8+ chars",
        "密码须为空或 8 位以上",
        "密碼須為空或 8 位以上",
    ),
    ("SSID required", "需要填写 SSID", "需要填寫 SSID"),
    ("WiFi settings were not saved.", "WiFi 设置未保存。", "WiFi 設定未儲存。"),
    (
        "SSID must be 1 to 32 bytes.",
        "SSID 须为 1 到 32 字节。",
        "SSID 須為 1 到 32 位元組。",
    ),
    (
        "Password must be empty, 8 to 63 bytes, or exactly 64 hex digits (0-9, a-f), which are the key itself.",
        "密码须为空、8 到 63 字节，或正好 64 位十六进制数字（0-9、a-f，即密钥本身）。",
        "密碼須為空、8 到 63 位元組，或正好 64 位十六進位數字（0-9、a-f，即金鑰本身）。",
    ),
    (
        "Network name must be at most 32 bytes.",
        "网络名称最多 32 字节。",
        "網路名稱最多 32 位元組。",
    ),
    (
        "Network password must be empty, 8 to 63 bytes, or exactly 64 hex digits (0-9, a-f), which are the key itself.",
        "网络密码须为空、8 到 63 字节，或正好 64 位十六进制数字（0-9、a-f，即密钥本身）。",
        "網路密碼須為空、8 到 63 位元組，或正好 64 位十六進位數字（0-9、a-f，即金鑰本身）。",
    ),
    (
        "Characters outside plain ASCII take 2 to 4 bytes each; control characters are not accepted.",
        "非 ASCII 字符（如中文）每个占 2 到 4 字节；不接受控制字符。",
        "非 ASCII 字元（如中文）每個佔 2 到 4 位元組；不接受控制字元。",
    ),
    (
        "Hostname must be 1 to 31 letters, digits or hyphens, and cannot start or end with a hyphen.",
        "主机名须为 1 到 31 个字母、数字或连字符，且不能以连字符开头或结尾。",
        "主機名稱須為 1 到 31 個字母、數字或連字號，且不能以連字號開頭或結尾。",
    ),
    (
        "OTA writes to the next app partition when available. The new image is kept only after 15 s of runtime; a crash or power cut before that restores the previous firmware. Keep USB reflashing available as a recovery path.",
        "OTA 会在可用时写入下一个 app 分区；新固件需连续运行 15 秒后才会被保留，在此之前崩溃或断电会回滚到上一版固件。请保留 USB 线刷作为恢复手段。",
        "OTA 會在可用時寫入下一個 app 分割區；新韌體需連續執行 15 秒後才會被保留，在此之前當機或斷電會還原到上一版韌體。請保留 USB 線刷作為還原手段。",
    ),
    ("WiFi Clients", "WiFi 客户端", "WiFi 客戶端"),
    ("Memory", "内存", "記憶體"),
    # Loop row: longest loop() pass (stage names stay as canstat prints them).
    ("Loop", "主循环", "主迴圈"),
    ("longest", "最长", "最長"),
    ("over 25 ms", "次超过 25 ms", "次超過 25 ms"),
    ("since CAN reset", "自 CAN 计数重置起", "自 CAN 計數重設起"),
    # Power / reset log (#218). Device-card label, reset reasons, 12V verdicts.
    ("Power", "电源", "電源"),
    ("Unknown reset", "未知复位", "未知重置"),
    ("Power-on", "上电", "上電"),
    ("Brownout", "电压骤降", "電壓驟降"),
    ("Panic", "崩溃", "當機"),
    ("Watchdog", "看门狗", "看門狗"),
    ("Software restart", "软件重启", "軟體重啟"),
    ("Deep-sleep wake", "深度睡眠唤醒", "深層睡眠喚醒"),
    ("External reset", "外部复位", "外部重置"),
    ("USB reset", "USB 复位", "USB 重置"),
    ("restarts since power-on", "上电后重启次数", "上電後重啟次數"),
    ("Not known yet", "尚无结果", "尚無結果"),
    (
        "Inconclusive (car never slept)",
        "无法判定（车辆未休眠）",
        "無法判定（車輛未休眠）",
    ),
    (
        "Switched (dies with the car)",
        "随车断电（车辆休眠即断电）",
        "隨車斷電（車輛休眠即斷電）",
    ),
    (
        "Always-on (outlives the car)",
        "常供电（车辆休眠仍供电）",
        "常供電（車輛休眠仍供電）",
    ),
    ("Brownout (check wiring)", "电压骤降（请检查接线）", "電壓驟降（請檢查接線）"),
    ("Crash (supply not implicated)", "崩溃（与供电无关）", "當機（與供電無關）"),
    ("Inconclusive (USB powered)", "无法判定（USB 供电）", "無法判定（USB 供電）"),
]
JS_TEMPLATE = r"""// web_i18n.inc — GENERATED by esp32/gen_i18n.py. DO NOT EDIT.
// Chinese (Simplified + Traditional) translation layer for the ESP32 web
// dashboard. Included by web_dashboard.cpp between the page's raw string
// literals (NOT inside a macro — xtensa GCC 8.4 can't do multi-line raw
// strings in #define).
R"rawliteral(
<script>
(function(){
"use strict";
/* English UI text -> Chinese (Simplified / Traditional). Keys match DOM
   textContent after collapsing whitespace (same normalization applied). */
var TABLES = {
"zh-Hans": __HANS_JSON__,
"zh-Hant": __HANT_JSON__
};

var LS_KEY = "teslaFsdLang";
var LANGS = ["en", "zh-Hans", "zh-Hant"];
var HTML_LANG = {"en": "en", "zh-Hans": "zh-CN", "zh-Hant": "zh-Hant"};
var BTN_LABEL = {"en": "EN", "zh-Hans": "简体", "zh-Hant": "繁體"};

function detectLang(tag){
  var l = String(tag || "").toLowerCase().replace(/_/g, "-");
  if(!/^zh(-|$)/.test(l)) return "en";
  if(/^zh-(hant|tw|hk|mo)(-|$)/.test(l)) return "zh-Hant";
  return "zh-Hans";
}

var lang = "en";
var DICT = {};
var KEYS = [];

function setDict(){
  DICT = (lang === "en") ? {} : (TABLES[lang] || {});
  KEYS = Object.keys(DICT).sort(function(a,b){ return b.length - a.length; });
}

function setLang(l){
  lang = l;
  setDict();
  try{ if(document.documentElement) document.documentElement.lang = HTML_LANG[l] || "en"; }catch(e){}
}

function norm(s){ return String(s == null ? "" : s).replace(/\s+/g, " ").trim(); }

function zhText(raw){
  var n = norm(raw);
  if(!n) return null;
  if(Object.prototype.hasOwnProperty.call(DICT, n)) return DICT[n];
  var i, k, tail, out, j, s2, head;
  // Longest-prefix match for templated strings, e.g. "Best guess: <dynamic>".
  for(i = 0; i < KEYS.length; i++){
    k = KEYS[i];
    if(k.length < 6 || n.indexOf(k) !== 0) continue;
    tail = n.slice(k.length);
    if(tail !== "" && /[A-Za-z0-9]/.test(tail.charAt(0))) continue; // word boundary
    out = DICT[k] + tail;
    // Suffix match on the remainder, e.g. "... -- confirm with your car's wiring diagram".
    for(j = 0; j < KEYS.length; j++){
      s2 = KEYS[j];
      if(s2.length < 6 || out.length <= s2.length) continue;
      if(out.slice(-s2.length) !== s2) continue;
      head = out.slice(0, out.length - s2.length);
      if(/[A-Za-z0-9]/.test(head.charAt(head.length - 1)) && /[A-Za-z0-9]/.test(s2.charAt(0))) continue;
      out = head + DICT[s2];
      break;
    }
    return out;
  }
  return null;
}

var SKIP_TAGS = {SCRIPT:1, STYLE:1, TEXTAREA:1, NOSCRIPT:1, CODE:1, PRE:1};
var ATTR_NAMES = ["placeholder", "title", "aria-label", "alt"];

function keepEnglish(el){
  return !!(el && el.closest && el.closest('[translate="no"]'));
}

function translateTextNode(node){
  if(keepEnglish(node.parentElement)) return;
  var t = zhText(node.nodeValue);
  if(t !== null && t !== node.nodeValue) node.nodeValue = t;
}

function translateElement(el){
  var i, a, v, t;
  if(!el.getAttribute || keepEnglish(el)) return;
  for(i = 0; i < ATTR_NAMES.length; i++){
    a = ATTR_NAMES[i];
    v = el.getAttribute(a);
    if(v){
      t = zhText(v);
      if(t !== null && t !== v) el.setAttribute(a, t);
    }
  }
}

function walk(root){
  var els, i, n, walker;
  if(root.querySelectorAll){
    els = root.querySelectorAll("*");
    for(i = 0; i < els.length; i++) translateElement(els[i]);
  }
  if(root.nodeType === 1) translateElement(root);
  walker = document.createTreeWalker(root, NodeFilter.SHOW_TEXT, {
    acceptNode: function(node){
      var p = node.parentElement;
      if(p && SKIP_TAGS[p.tagName]) return NodeFilter.FILTER_REJECT;
      return NodeFilter.FILTER_ACCEPT;
    }
  });
  while((n = walker.nextNode())) translateTextNode(n);
}

function currentIsZh(){ return lang !== "en"; }

function applyLang(){
  if(!currentIsZh() || !document.documentElement) return;
  walk(document.documentElement);
}

// Saved choice: localStorage, else the URL fragment the toggle leaves behind
// when storage is refused (captive-portal sheets), else the browser language.
var saved = null;
try{ saved = window.localStorage.getItem(LS_KEY); }catch(e){}
if(saved === "zh") saved = "zh-Hans";  // migrate the old two-way value
if(LANGS.indexOf(saved) < 0){
  var hm = /(?:^|[#&])lang=([A-Za-z-]+)/.exec(window.location.hash || "");
  saved = hm ? hm[1] : null;
}
if(LANGS.indexOf(saved) >= 0){ setLang(saved); }
else { setLang(detectLang(navigator.language || navigator.userLanguage)); }

var btn = document.createElement("button");
btn.id = "langToggle";
btn.type = "button";
function paintBtn(){ btn.textContent = BTN_LABEL[lang] || "EN"; }
paintBtn();
btn.style.cssText = "position:absolute;left:0;top:26px;z-index:50;background:rgba(0,212,170,.12);"
  + "border:1px solid rgba(0,212,170,.45);color:#00d4aa;font-size:.72em;font-weight:700;"
  + "padding:5px 10px;border-radius:20px;cursor:pointer;letter-spacing:.05em";
btn.setAttribute("aria-label", "Switch language / 切换语言 / 切換語言");
btn.onclick = function(){
  var from = lang;
  var i = LANGS.indexOf(lang);
  setLang(LANGS[(i + 1) % LANGS.length]);
  var stored = false;
  try{ window.localStorage.setItem(LS_KEY, lang); stored = (window.localStorage.getItem(LS_KEY) === lang); }catch(e){}
  if(!stored){ try{ window.location.hash = "lang=" + lang; }catch(e){} }
  paintBtn();
  if(from === "en" && currentIsZh()){ applyLang(); }
  else { window.location.reload(); }
};

function mount(){
  var hdr = document.querySelector(".hdr");
  if(hdr){ hdr.appendChild(btn); }
  else if(document.body){ document.body.insertBefore(btn, document.body.firstChild); }
  applyLang();
}

if(document.readyState === "loading"){
  document.addEventListener("DOMContentLoaded", mount);
} else {
  mount();
}

// Translate live updates, and the page's own dialogs / toasts, as they land
// in the DOM.
if("MutationObserver" in window){
  var mo = new MutationObserver(function(muts){
    var i, j, m, added, nd;
    if(!currentIsZh()) return;
    for(i = 0; i < muts.length; i++){
      m = muts[i];
      if(m.type === "characterData"){ translateTextNode(m.target); }
      else{
        added = m.addedNodes;
        for(j = 0; j < added.length; j++){
          nd = added[j];
          if(nd.nodeType === 3){ translateTextNode(nd); }
          else if(nd.nodeType === 1){ walk(nd); }
        }
      }
    }
  });
  var startWatch = function(){
    if(document.documentElement) mo.observe(document.documentElement,
      {childList: true, subtree: true, characterData: true});
  };
  if(document.readyState === "loading"){
    document.addEventListener("DOMContentLoaded", startWatch);
  } else {
    startWatch();
  }
}
})();
</script>
)rawliteral"
"""


def main():
    src = open(SRC, encoding="utf-8", errors="replace").read()
    page = html.unescape(src)
    page_norm = re.sub(r"\s+", " ", page)

    seen = set()
    missing = []
    for en, _hs, _ht in T:
        if en in seen:
            print("dup key: %r" % en, file=sys.stderr)
        seen.add(en)
        if en not in page_norm and re.sub(r"\s+", " ", en) not in page_norm:
            missing.append(en)
    if missing:
        print("WARNING: %d keys not found in page:" % len(missing), file=sys.stderr)
        for m in missing:
            print("  - %r" % m, file=sys.stderr)

    hans = {}
    hant = {}
    for en, hs, ht in T:
        if "\u2028" in en or "\u2029" in en:
            raise SystemExit("bad char in key")
        hans[en] = hs
        hant[en] = ht
    hans_json = json.dumps(hans, ensure_ascii=False, indent=1)
    hant_json = json.dumps(hant, ensure_ascii=False, indent=1)
    # Defensive: keep the raw-string delimiter intact.
    assert ')rawliteral"' not in hans_json, "delimiter collision in zh-Hans"
    assert ')rawliteral"' not in hant_json, "delimiter collision in zh-Hant"
    assert JS_TEMPLATE.count(')rawliteral"') == 1, "delimiter collision in JS"

    out = JS_TEMPLATE.replace("__HANS_JSON__", hans_json)
    out = out.replace("__HANT_JSON__", hant_json)
    open(OUT, "w", encoding="utf-8").write(out)
    print("wrote %s (%d entries x2)" % (OUT, len(hans)))
    if missing:
        sys.exit(2)


if __name__ == "__main__":
    main()
