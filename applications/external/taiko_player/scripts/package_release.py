#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 ZeroNinx

"""Package official and both Momentum builds without local songs or settings."""

import argparse
import ast
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import zipfile

from prepare_sfx import prepare


ROOT = Path(__file__).resolve().parent.parent


def fap_metadata(path):
    data = path.read_bytes()
    if data[:7] != b"\x7fELF\x01\x01\x01":
        raise ValueError(f"Not a little-endian ELF32 FAP: {path}")
    offset = struct.unpack_from("<I", data, 32)[0]
    size, count, names_index = struct.unpack_from("<HHH", data, 46)
    sections = [
        struct.unpack_from("<10I", data, offset + i * size) for i in range(count)
    ]
    names_section = sections[names_index]
    names = data[names_section[4] : names_section[4] + names_section[5]]
    for section in sections:
        if names[section[0] :].split(b"\0", 1)[0] == b".fapmeta":
            magic, schema, api, hardware = struct.unpack_from("<IIIh", data, section[4])
            if (magic, schema, hardware) != (0x52474448, 1, 7):
                raise ValueError(f"Unsupported FAP metadata: {path}")
            version = struct.unpack_from("<I", data, section[4] + 16)[0]
            return {
                "version": f"{version >> 16}.{version & 0xffff}",
                "api": f"{api >> 16}.{api & 0xffff}",
                "hardware_target": "f7",
            }
    raise ValueError(f"Missing FAP metadata: {path}")


def installation(version, firmware, sdk_version, api):
    return f"""# Taiko Player {version} — {firmware}

构建所用固件 SDK：{sdk_version}，API {api}，硬件目标 Flipper Zero F7。
请选择与设备固件对应的安装包；遇到 API 不兼容提示时，需要使用对应 SDK 重新构建。

## 安装

1. 将本包 `sdcard/` 内的内容复制到 SD 卡根目录（也可以用 qFlipper 上传）。
2. 应用位于 `apps/Media/taiko_player.fap`，鼓声音效位于
   `apps_data/taiko_player/sfx/`。包内不含个人歌曲或 settings.ff。
3. 在 Flipper 的 Media 菜单中打开 Taiko Player，选择 TJA 谱面。

## 准备歌曲

电脑需要 Python 3 和 FFmpeg。将自己的谱面及对应音频放入本包的 `songs/` 目录，
例如 `songs/Example/chart.tja`、`songs/Example/song.ogg`，然后在解压目录运行：

```sh
python3 scripts/prepare_songs.py songs --output sdcard/apps_data/taiko_player/songs
```

将生成的 `sdcard/apps_data/taiko_player/songs/` 上传到 SD 卡的同名目录。
脚本会生成 UTF-8 TJA 副本及 22050 Hz、8 位、单声道 PCM WAV，并更新 WAVE 字段。
设备不直接解码 OGG。详细功能及操作见 docs/usage.md；英文入门见 README.en.md。

## 操作

- OK：播放 / 暂停；左 / 右：后退 / 前进。
- 上 / 下：调节应用音量；短按返回：菜单；播放时长按返回：退出。
- GPIO 输出默认关闭，可在菜单内开启并设置引脚。
- 连打和气球最多每秒自动打咚 32 下；暂不提供打击判定。

源码：https://github.com/ZeroNinx/flipper-zero-taiko-player/tree/v{version}
"""


def package(version, output):
    manifest = ast.parse((ROOT / "application.fam").read_text())
    source_version = next(
        ast.literal_eval(keyword.value)
        for keyword in manifest.body[0].value.keywords
        if keyword.arg == "fap_version"
    )
    if source_version != version:
        raise ValueError(
            f"application.fam version {source_version} does not match release {version}"
        )
    # The SDK accepts a patch component, but the FAP header stores only major/minor.
    expected_fap_version = ".".join(version.split(".")[:2])
    commit = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
    ).strip()
    dirty = bool(subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT))
    sdk_homes = {
        "official": Path(os.environ.get("UFBT_HOME", Path.home() / ".ufbt")),
        "momentum": ROOT / ".ufbt/momentum",
        "momentum-012": ROOT / ".ufbt/momentum-012",
    }
    artifacts = []
    with tempfile.TemporaryDirectory(prefix="taiko-release-") as temporary:
        effects = Path(temporary) / "sfx"
        prepare(ROOT / "assets", effects)
        effect_files = ("dong.wav", "ka.wav", "SFX-LICENSE.txt", "SFX-NOTICE.md")
        for name in effect_files:
            if not (effects / name).is_file():
                raise ValueError(f"Required release effect missing: {name}")
        for firmware, sdk_home in sdk_homes.items():
            fap = ROOT / "dist" / firmware / "taiko_player.fap"
            metadata = fap_metadata(fap)
            if metadata["version"] != expected_fap_version:
                raise ValueError(f"{fap}: FAP version does not match release {version}")
            metadata["fap_version"] = metadata["version"]
            metadata["version"] = version
            sdk = json.loads((sdk_home / "current/components.json").read_text())["meta"]
            if sdk["hw_target"] != metadata["hardware_target"]:
                raise ValueError(f"{firmware}: SDK hardware target mismatch")
            metadata.update(
                {
                    "firmware": firmware,
                    "sdk_version": sdk["version"],
                    "source_commit": commit,
                    "source_dirty": dirty,
                    "fap_sha256": hashlib.sha256(fap.read_bytes()).hexdigest(),
                }
            )
            files = {
                "sdcard/apps/Media/taiko_player.fap": fap.read_bytes(),
                "README.md": (ROOT / "README.md").read_bytes(),
                "README.en.md": (ROOT / "README.en.md").read_bytes(),
                "LICENSE": (ROOT / "LICENSE").read_bytes(),
                "screenshots/playing.png": (
                    ROOT / "screenshots/playing.png"
                ).read_bytes(),
                "assets/taiko_player.svg": (
                    ROOT / "assets/taiko_player.svg"
                ).read_bytes(),
                "INSTALL.md": installation(
                    version, firmware, sdk["version"], metadata["api"]
                ).encode(),
                "BUILD.json": (json.dumps(metadata, indent=2) + "\n").encode(),
            }
            for document in sorted((ROOT / "docs").rglob("*.md")):
                files[document.relative_to(ROOT).as_posix()] = document.read_bytes()
            for name in ("SFX-LICENSE.txt", "SFX-NOTICE.md"):
                files[f"assets/{name}"] = (ROOT / "assets" / name).read_bytes()
            for name in effect_files:
                files[f"sdcard/apps_data/taiko_player/sfx/{name}"] = (
                    effects / name
                ).read_bytes()
            for name in ("prepare_songs.py", "prepare_sfx.py"):
                files[f"scripts/{name}"] = (ROOT / "scripts" / name).read_bytes()
            files["SHA256SUMS.txt"] = "".join(
                f"{hashlib.sha256(data).hexdigest()}  {name}\n"
                for name, data in sorted(files.items())
            ).encode()
            output.mkdir(parents=True, exist_ok=True)
            archive = output / f"taiko-player-v{version}-{firmware}.zip"
            with zipfile.ZipFile(
                archive, "w", compression=zipfile.ZIP_DEFLATED
            ) as zipped:
                for directory in ("songs/", "sdcard/apps_data/taiko_player/songs/"):
                    zipped.writestr(directory, b"")
                for name, data in sorted(files.items()):
                    zipped.writestr(name, data)
            with zipfile.ZipFile(archive) as zipped:
                assert zipped.testzip() is None
            artifacts.append(archive)
            print(f"{archive}: SDK {sdk['version']}, API {metadata['api']}")
    (output / "SHA256SUMS.txt").write_text(
        "".join(
            f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.name}\n"
            for path in artifacts
        )
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", default="0.5.1")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if not re.fullmatch(r"\d+\.\d+(?:\.\d+)?", args.version):
        parser.error(
            "Version must be major.minor or major.minor.patch, matching application.fam"
        )
    package(args.version, args.output or ROOT / "dist/releases" / f"v{args.version}")


if __name__ == "__main__":
    main()
