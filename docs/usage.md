# 使用指南

[项目首页](../README.md) · [使用指南](usage.md) · [谱面支持](chart-support.md) · [开发指南](development.md)

## 安装与升级

从 [GitHub Releases](https://github.com/ZeroNinx/flipper-zero-taiko-player/releases) 下载与设备固件 API 兼容的安装包。v0.5.1 提供以下三个安装包：

| 安装包后缀 | 固件 SDK | API |
| --- | --- | --- |
| official | 官方 1.5.1 | 88.2 |
| momentum | Momentum mntm-009 | 79.2 |
| momentum-012 | Momentum mntm-012，已在 Momentum CN v1.1.6 验证 | 87.1 |

解压后将 `sdcard/` 内的内容复制到 SD 卡根目录，可通过 qFlipper 上传。发布包内附带咚／咔音效、转码脚本及 `INSTALL.md`，不含歌曲或用户设置；升级时可覆盖应用和音效，保留原有歌曲及 `settings.ff`。

SD 卡上的目录结构为：

```text
apps/Media/taiko_player.fap
apps_data/taiko_player/songs/
  Example Song/
    chart.tja
    song.wav
apps_data/taiko_player/sfx/
  dong.wav
  ka.wav
```

## 仅安装应用时准备鼓声

从商店安装或只复制 FAP 时，不会自动获得 Release ZIP 中的鼓声音效。下载并解压[源码](https://github.com/ZeroNinx/flipper-zero-taiko-player/archive/refs/heads/main.zip)，在电脑安装 Python 3 和 FFmpeg，然后在源码目录运行：

```sh
python3 scripts/prepare_sfx.py
```

将生成的 sdcard/apps_data/taiko_player/sfx/ 目录连同许可、来源文件复制到 SD 卡同名位置，重新启动应用加载。转换不需要安装 uFBT；缺少鼓声时仍可播放伴奏。歌曲按下节另外准备。

## 准备曲目

将自己的 TJA 和对应音频放入本地 `songs/` 目录，音频文件名由谱面的 `WAVE:` 字段指定。例如：

```text
songs/
  Example Song/
    chart.tja
    song.ogg
```

电脑需要 Python 3 和 FFmpeg。在源码目录或解压后的发布包目录运行：

```sh
python3 scripts/prepare_songs.py songs --output sdcard/apps_data/taiko_player/songs
```

源码开发环境初始化后，也可使用 `make songs`。

默认转换为 **22050 Hz、8 位无符号、单声道 PCM WAV**。脚本支持读取 UTF-8、Shift-JIS 和 GB18030 编码的 TJA，输出 UTF-8 副本，并将副本中的 `WAVE:` 改为 WAV 文件名。原始文件保留不变；格式正确且未过期的 WAV 会跳过转换，使用 `--force` 可强制重新转换。

曲目资源不包含在仓库中，`songs/` 和生成的 `sdcard/` 均已加入 `.gitignore`。

将生成的 `sdcard/apps_data/taiko_player/songs/` 上传到 SD 卡同名目录，然后在设备上选择转换后的 TJA。发布包的转码脚本可直接使用，无需安装 uFBT。

## 按键与菜单

在 Flipper 的应用菜单中打开 **Media → Taiko Player**。选曲默认打开 `/ext/apps_data/taiko_player/songs` 目录，在其中选择谱面。播放器根据 `WAVE:` 查找对应 WAV，因此保留原始 TJA 时，也可以将同名 WAV 放到其指定位置。

| 操作 | 按键 |
| --- | --- |
| 浏览、选择曲目 | 方向键、OK |
| 文件浏览器返回上级目录 | 短按返回 |
| 播放 / 暂停 | OK |
| 后退 / 前进（默认 1 小节） | 左 / 右 |
| 增加 / 减少应用总音量 | 上 / 下（可按住连续调整） |
| 打开 / 关闭播放菜单 | 短按返回 |
| 菜单中选择项目 | 上 / 下 |
| 菜单中修改难度、速度、间隔、音量 | 左 / 右（OK 切换或增加） |
| 执行菜单项目 | OK |
| 播放时退出应用 | 长按返回 |
| 播放结束后从头重播 | OK |

暂停时，音频、谱面与鼓点指示一起停止，并清空 GPIO 输出。暂停期间也可前后跳转，按 OK 后从当前位置继续播放。跳转会限制在曲目起止范围内，不补发跳过的鼓点；左右手交替仍按谱面中的原始音符顺序计算。恢复和跳转时需要重新读取音频缓冲，可能有短暂延迟。

播放菜单从上到下为 **Return（返回）、Restart（重新播放）、Play New Song（播放新曲）、Difficulty（难度）、Playback Speed（播放速度）、Scroll Speed（谱面流速）、Frame Rate（刷新率）、Seek Mode（跳转方式）、Backward Interval（后退间隔）、Forward Interval（前进间隔）、Dong Volume（咚音量）、Ka Volume（咔音量）、GPIO Output（GPIO 输出开关）、GPIO Setting（GPIO 引脚设置）、Restore Defaults（恢复默认设置）、Exit（退出应用）**。菜单每屏显示五行，上下选择时自动滚动。默认按小节跳转，前后各 1 小节；按小节和按时间的间隔设置见下节。音量默认各为 70%。音量、跳转间隔、播放速度、流速、刷新率和 GPIO 引脚映射自动保存，下次启动恢复。打开菜单会暂停音频并清空 GPIO，选择 Return 或短按返回时恢复进入菜单前的播放或暂停状态；Restart 从头开始播放；Play New Song 停止当前曲目并回到 `taiko_player/songs/` 目录重新选曲，保留本次运行中的音量和跳转间隔设置，选定后从头播放。取消选曲会退出应用。播放结束时进入菜单再返回，仍保留结束状态。

难度选项只列出当前 TJA 中存在的 COURSE，支持名称、数字编号和 Ura（Edit）别名；优先 Oni，否则按 COURSE 顺序选最高难度。返回播放或选择 Restart 时应用难度变更，并从头开始；原本暂停时返回仍保持暂停。文件中尚未选择的难度不会同时载入音符数组。

暂停会停止音频线程和 DMA，取消 GPIO 脉冲并拉低已启用的输出。暂停时应用等待按键，不再周期计算和刷新静止谱面，谱面时间与 GOGO 动画保持不变；继续播放时从保存的位置重新启动音频。

## 跳转方式与小节线

**Seek Mode** 位于 Frame Rate 下方，左右键或 OK 在 **By Time / By Bars** 之间切换，默认 By Bars。

- **By Time**：左右键按秒后退／前进，Backward Interval 和 Forward Interval 分别支持 **1 / 3 / 5 / 10 秒**，默认 5 秒。
- **By Bars**：左右键跳到真实的小节起点，间隔选项改为 Backward Bars 和 Forward Bars，分别支持 **1 / 3 / 5 / 10 小节**，默认 1 小节。位于小节中间时，后退一次先返回当前小节开头；恰好在小节线上时，后退到上一小节，前进到下一小节。

新安装、旧配置缺少 `SeekMode` 或恢复默认设置时使用 By Bars；配置中已明确保存的 By Time / By Bars 选择继续保留。两种模式分别保留各自的前后间隔，切换不会覆盖另一种模式的设置；跳转方式与间隔跨曲目、跨重启保存。暂停时也可按小节跳转，之后继续保持暂停。超出范围时停在音频开头或结尾。

小节按 TJA 的逗号分隔统计，使用所选难度的实际时间，支持 BPM 变化、`#MEASURE`、`OFFSET`、非负 `#DELAY` 和空小节，不按固定 BPM 估算。轨道以一像素宽的白线标记小节起点，移动倍率取该起点的 BPM 和 `#SCROLL`，绘制在音符后方。`#BARLINEOFF` 只隐藏小节线，被隐藏的小节仍可跳转；`#BARLINEON` 恢复后续小节线。

## 播放速度与流速

播放速度支持 **0.5～2.0 倍，每档 0.1**；谱面流速支持 **0.5～2.5 倍（每档 0.1），以及 3.0、4.0 倍**。两者默认均为 1.0 倍，独立设置并保存到 SD 卡。播放速度同步改变伴奏、击打与 GPIO 的节奏，伴奏音高随速度变化；咚／咔音效保持原本的音高和时长，普通音符、连打和气球均适用，同音色连续击打仍会接替旧尾音。播放时间显示原曲位置，修改速度保留当前位置。菜单流速叠加在谱面自带的 HS 上，只改变音符显示距离，不改变音频或 GPIO。换曲时重新选择默认难度，速度设置保留。

HS 的计算方式见[谱面支持](chart-support.md)。

## 刷新率

菜单中的 **Frame Rate** 位于 Scroll Speed 下方，支持 **15～60 FPS，每档 1 FPS**，默认 **50 FPS**。左右键调整，OK 增加，到边界后循环选择。返回播放后按新目标刷新率绘制；设置跨曲目及应用重启保留，Restore Defaults 恢复为 50 FPS。

此选项只调整界面更新频率，音频播放、击打检查与 GPIO 调度不随之降频。实际可见帧率仍受绘制耗时、系统负载及屏幕响应影响。

## 音量与鼓声

伴奏增益为 100%，总音量、咚／咔音量默认各为 70%。咚／咔在菜单中分别调节，范围为 0–100%，每次 10%，返回播放后生效；0% 静音对应音效。普通及大音符使用对应的咚／咔音效，连打和气球使用 Dong Volume。咚、咔各自最多播放一个音效，新击打会接替同音色的旧尾音；咚和咔仍可同时发声，连打和气球与普通咚共用咚声部。设备缺少或无法加载音效时仍可播放伴奏。

总音量在混音后统一作用于伴奏和鼓声，默认 70%，范围为 0–200%，每次调整 10%，0% 静音但不停止谱面或 GPIO。播放界面按上/下键增减音量，也支持按住连续调整；暂停时同样可调，菜单中的上下键仍用于选择项目。音量在后续音频缓冲中生效，可能有短暂延迟。总音量会保存到 SD 卡，下次启动恢复。PCM 播放直接驱动扬声器，不经过系统通知音量处理，也不读取或修改系统音量设置。

100% 表示原始幅度，100% 以上是软件增益，200% 将未限幅样本相对静音中心的幅度放大两倍；超出 8 位 PCM 范围的峰值会限幅，可能增加破音。音量柱按 0–200% 显示，100% 约为半柱，200% 满柱。咚／咔各自的菜单音量仍为 0–100%。

自定义音效与试听谱面的生成方法见[开发指南](development.md#鼓声音效)。

## GPIO 输出

`GPIO Output` 默认 **Off**，关闭时不触发 GPIO，应用启动时也不会配置外部引脚。使用左右键或 OK 切换 On / Off。`GPIO Setting` 为二级菜单，依次为 Return、Left Ka、Left Dong、Right Dong、Right Ka、Pulse Width。上下选择项目，四路鼓点使用左右键切换引脚，OK 切换到下一个可用引脚；Pulse Width 使用左右键调整脉冲时长，OK 增加一档，到边界后循环。短按返回或选择 Return 回到一级菜单，再次短按返回恢复播放。

可选引脚为 **A7、A6、A4、B3、B2、C3、C1、C0**，自动跳过其他鼓点已使用的引脚，四路保持独立。关闭输出时也可修改映射；开关与映射在换曲时保留；引脚映射与脉冲时长跨次运行保存，输出开关每次启动均为 Off。首次使用及恢复默认时的映射如下：

| 鼓点 | GPIO | 排针编号 |
| --- | --- | --- |
| 左蓝（咔） | A7 / PA7 | 2 |
| 左红（咚） | A6 / PA6 | 3 |
| 右红（咚） | A4 / PA4 | 4 |
| 右蓝（咔） | B3 / PB3 | 5 |

输出为 **3.3 V 逻辑信号**，接收端应与 Flipper 的 GND 共地。

每个音符输出一个高电平脉冲，默认 **70 ms**，可在 **GPIO Setting → Pulse Width** 中调整为 **25～100 ms，每档 5 ms**；四路共用该设置，脉宽不随播放速度变化，空闲为低电平。四路独立计时，不同路的脉冲可以重叠；同一路重复触发时，先拉低再拉高，并重新计时。软件定时器的实际脉宽会有少量调度抖动。

暂停、跳转、播放结束、出错或退出时，清空已启用的输出。关闭 GPIO、更换引脚或修改脉宽时，先取消脉冲并拉低输出，再将原引脚恢复为下拉输入；退出应用时释放已使用的引脚。GPIO 开关不影响音频和屏幕鼓面指示。音频只输出到内置扬声器。

默认映射与可选引脚位于 [taiko_gpio_config.c](https://github.com/ZeroNinx/flipper-zero-taiko-player/blob/main/src/taiko_gpio_config.c)，脉宽范围和默认值位于 [taiko_gpio_config.h](https://github.com/ZeroNinx/flipper-zero-taiko-player/blob/main/src/taiko_gpio_config.h)。屏幕鼓面和命中闪动仍保持 70 ms，与 GPIO 脉宽独立。

## 设置保存

配置文件位于 `/ext/apps_data/taiko_player/settings.ff`，采用 FlipperFormat 文本格式。保存总音量、咚/咔音量、前后跳转间隔、播放速度、谱面流速、刷新率、GPIO 脉冲时长和四路 GPIO 引脚名称。GPIO 输出开关每次启动保持关闭；每首曲目仍优先选择 Oni，没有时选择最高难度，选曲默认目录仍为 `songs/`。

修改先在内存中生效，有变更时才写卡。打开菜单、从一级菜单返回播放或换曲、正常退出时保存；选择 Restore Defaults 会立即恢复并保存默认值，保持当前曲目、难度和播放位置，GPIO 输出关闭。播放界面上下调整总音量后，在下一次打开菜单或正常退出时保存，因此直接断电可能丢失最近尚未保存的音量变更。写卡集中在音频暂停或停止时。

保存时先完整写入临时文件，再替换正式文件。文件缺失或版本不支持时采用默认配置；无效或缺失字段回退到对应默认值，重复或无效的 GPIO 引脚映射整体恢复为默认四路。保存失败会显示提示，当前设置仍可继续使用。

示例配置中，`SeekMode` 的 `0` 表示 By Time、`1` 表示 By Bars；`BackwardBars` / `ForwardBars` 单位为小节，时间间隔与 GPIO 脉冲时长的单位为毫秒，速度为倍率的十倍（`10` 表示 `1.0x`），音量为百分比：

```text
Filetype: Taiko Player Settings
Version: 1
MasterVolume: 70
DongVolume: 70
KaVolume: 70
BackwardInterval: 5000
ForwardInterval: 5000
SeekMode: 1
BackwardBars: 1
ForwardBars: 1
PlaybackSpeed: 10
ScrollSpeed: 10
FrameRate: 50
GpioPulseMs: 70
LeftKaPin: A7
LeftDongPin: A6
RightDongPin: A4
RightKaPin: B3
```

旧版配置缺少 `SeekMode` 时采用 By Bars，缺少 `BackwardBars` / `ForwardBars` 时各采用 1 小节；缺少 `FrameRate` 时采用 50 FPS，缺少 `GpioPulseMs` 时采用 70 ms，其余有效设置继续保留；后续保存时补上缺少的字段，配置版本仍为 1。
