#!/usr/bin/env python3
"""Generate and encode the first passive-listening Polly voice trial."""

from __future__ import annotations

import argparse
import hashlib
import html
import json
import math
import shutil
import subprocess
import sys
import wave
from array import array
from pathlib import Path
from typing import Any


TOKENS = {
    "A": "Alpha", "B": "Bravo", "C": "Charlie", "D": "Delta", "E": "Echo",
    "F": "Foxtrot", "G": "Golf", "H": "Hotel", "I": "India", "J": "Juliett",
    "K": "Kilo", "L": "Lima", "M": "Mike", "N": "November", "O": "Oscar",
    "P": "Papa", "Q": "Quebec", "R": "Romeo", "S": "Sierra", "T": "Tango",
    "U": "Uniform", "V": "Victor", "W": "Whiskey", "X": "X-ray", "Y": "Yankee",
    "Z": "Zulu", "0": "Zero", "1": "One", "2": "Two", "3": "Three", "4": "Four",
    "5": "Five", "6": "Six", "7": "Seven", "8": "Eight", "9": "Nine",
    "stroke": "Stroke", "period": "Period", "comma": "Comma", "question-mark": "Question mark",
}

RATES = {
    "normal": None,
    "90pct": "90%",
}

VARIANTS = {
    "s16_16k": (16000, "pcm_s16le"),
    "s16_8k": (8000, "pcm_s16le"),
    "u8_8k": (8000, "pcm_u8"),
    "mulaw_8k": (8000, "pcm_mulaw"),
    "ima_adpcm_8k": (8000, "adpcm_ima_wav"),
}

VOICE_FILTER = (
    "acompressor=threshold=-24dB:ratio=3:attack=5:release=80:makeup=6dB,"
    "loudnorm=I=-18:LRA=7:TP=-2"
)
PASSIVE_PRESENCE_GATE_FILTER = (
    "agate=threshold=0.012:ratio=4:range=0.05:"
    "attack=2:release=60:knee=2:detection=rms,"
    "highpass=f=250:p=2,lowpass=f=6500:p=2,"
    "equalizer=f=2400:t=q:w=1.1:g=4,"
    "acompressor=threshold=0.25:ratio=2:attack=3:release=80:makeup=2.5,"
    "highpass=f=30:p=1,"
    "afade=t=in:d=0.008,"
    "areverse,afade=t=in:d=0.012,areverse,"
    "alimiter=limit=0.891:level=false:attack=3:release=50:latency=true,"
    "aresample=16000:resampler=soxr:precision=28:osf=s16:dither_method=none"
)
MASTERING_PROFILES = {
    "balanced": (VOICE_FILTER,),
    "passive-hot": (
        (
            f"{VOICE_FILTER},volume=11dB,"
            "alimiter=limit=0.944:level=false:attack=1:release=20:latency=true"
        ),
    ),
    # Preserve the tested intermediate PCM boundary after the balanced master.
    "passive-presence-gate": (VOICE_FILTER, PASSIVE_PRESENCE_GATE_FILTER),
}
SOURCE_RATE = 16000
VOICE_ID = "Amy"
LANGUAGE_CODE = "en-GB"
ENGINE = "neural"
DEFAULT_REGION = "us-east-1"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Generate six British-English Amy neural Polly tokens, encode the "
            "comparison matrix, and write an audition page."
        )
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("build/audio-assets/amy-trial"),
        help="output directory (default: build/audio-assets/amy-trial)",
    )
    parser.add_argument(
        "--region",
        default=DEFAULT_REGION,
        help=f"AWS region (default: {DEFAULT_REGION})",
    )
    parser.add_argument(
        "--encode-only",
        action="store_true",
        help="reuse existing source WAVs without contacting Polly",
    )
    parser.add_argument(
        "--mastering-profile",
        choices=tuple(MASTERING_PROFILES),
        default="balanced",
        help="voice mastering profile (default: balanced)",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="replace existing source WAVs",
    )
    parser.add_argument(
        "--runtime-pack",
        type=Path,
        help="also build the selected 90pct/s16_16k MFVA runtime pack",
    )
    return parser.parse_args()


def require_command(name: str) -> None:
    if shutil.which(name) is None:
        raise RuntimeError(f"required command is unavailable: {name}")


def run(command: list[str], *, capture: bool = False) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(
        command,
        check=True,
        stdout=subprocess.PIPE if capture else subprocess.DEVNULL,
        stderr=subprocess.PIPE,
    )


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while chunk := source.read(65536):
            digest.update(chunk)
    return digest.hexdigest()


def pcm16_samples(raw: bytes) -> array[int]:
    if len(raw) % 2:
        raise ValueError("PCM byte count is not aligned to 16-bit samples")
    samples = array("h")
    samples.frombytes(raw)
    if sys.byteorder != "little":
        samples.byteswap()
    return samples


def samples_to_pcm16(samples: array[int]) -> bytes:
    output = array("h", samples)
    if sys.byteorder != "little":
        output.byteswap()
    return output.tobytes()


def write_pcm16_wave(path: Path, samples: array[int], sample_rate: int) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(sample_rate)
        output.writeframes(samples_to_pcm16(samples))


def write_pcm_u8_wave(path: Path, samples: array[int], sample_rate: int) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    encoded = bytes(max(0, min(255, (value + 32768) >> 8)) for value in samples)
    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(1)
        output.setframerate(sample_rate)
        output.writeframes(encoded)


def clean_source_pcm(raw: bytes) -> tuple[array[int], dict[str, int | float]]:
    samples = pcm16_samples(raw)
    if not samples:
        raise ValueError("Polly returned no PCM samples")

    dc = round(sum(samples) / len(samples))
    centred = array("h", (max(-32768, min(32767, value - dc)) for value in samples))
    peak = max(abs(value) for value in centred)
    if peak == 0:
        raise ValueError("Polly returned silent PCM")

    window = SOURCE_RATE // 100  # 10 ms
    threshold = max(32, round(peak * (10 ** (-50 / 20))))
    active: list[int] = []
    for start in range(0, len(centred), window):
        block = centred[start : start + window]
        if not block:
            continue
        rms = math.isqrt(sum(value * value for value in block) // len(block))
        if rms >= threshold:
            active.append(start)

    if not active:
        raise ValueError("speech activity was not found in Polly PCM")

    margin = SOURCE_RATE * 20 // 1000
    first = max(0, active[0] - margin)
    last = min(len(centred), active[-1] + window + margin)
    trimmed = array("h", centred[first:last])

    target_peak = round(32767 * (10 ** (-2 / 20)))
    trimmed_peak = max(abs(value) for value in trimmed)
    gain = target_peak / trimmed_peak
    normalised = array(
        "h",
        (
            max(-32768, min(32767, round(value * gain)))
            for value in trimmed
        ),
    )

    fade_samples = SOURCE_RATE * 5 // 1000
    for index in range(min(fade_samples, len(normalised))):
        scale = index / fade_samples
        normalised[index] = round(normalised[index] * scale)
        reverse = len(normalised) - 1 - index
        normalised[reverse] = round(normalised[reverse] * scale)

    pad = array("h", [0]) * (SOURCE_RATE * 10 // 1000)
    cleaned = array("h", pad)
    cleaned.extend(normalised)
    cleaned.extend(pad)

    return cleaned, {
        "source_samples": len(samples),
        "trimmed_samples": len(trimmed),
        "cleaned_samples": len(cleaned),
        "removed_dc": dc,
        "gain": round(gain, 6),
        "activity_threshold": threshold,
    }


def build_polly_text(spoken_text: str, rate: str | None) -> tuple[str, str]:
    if rate is None:
        return spoken_text, "text"
    escaped = html.escape(spoken_text)
    return f'<speak><prosody rate="{rate}">{escaped}</prosody></speak>', "ssml"


def polly_client(region: str) -> Any:
    try:
        import boto3
    except Exception as error:
        raise RuntimeError(
            "boto3 is unavailable; install tools/mfva/requirements.txt "
            "in a virtual environment"
        ) from error
    return boto3.Session(region_name=region).client("polly")


def verify_voice(client: Any) -> None:
    response = client.describe_voices(Engine=ENGINE, LanguageCode=LANGUAGE_CODE)
    matches = [
        voice
        for voice in response.get("Voices", [])
        if voice.get("Id") == VOICE_ID and ENGINE in voice.get("SupportedEngines", [])
    ]
    if not matches:
        raise RuntimeError(
            f"{VOICE_ID} {ENGINE} {LANGUAGE_CODE} is unavailable in the selected region"
        )


def synthesize(client: Any, spoken_text: str, rate: str | None) -> tuple[bytes, dict[str, Any]]:
    text, text_type = build_polly_text(spoken_text, rate)
    response = client.synthesize_speech(
        Engine=ENGINE,
        LanguageCode=LANGUAGE_CODE,
        VoiceId=VOICE_ID,
        OutputFormat="pcm",
        SampleRate=str(SOURCE_RATE),
        Text=text,
        TextType=text_type,
    )
    raw = response["AudioStream"].read()
    return raw, {
        "request_text": spoken_text,
        "text_type": text_type,
        "prosody_rate": rate or "normal",
        "content_type": response.get("ContentType"),
        "request_characters": response.get("RequestCharacters"),
    }


def make_voice_reference(
    source: Path,
    destination: Path,
    voice_filters: tuple[str, ...],
) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    stage_input = source
    temporary: list[Path] = []
    try:
        for index, voice_filter in enumerate(voice_filters):
            stage_output = (
                destination
                if index == len(voice_filters) - 1
                else destination.with_name(f".{destination.stem}.stage-{index}.wav")
            )
            if stage_output != destination:
                temporary.append(stage_output)
            run(
                [
                    "ffmpeg",
                    "-nostdin",
                    "-hide_banner",
                    "-loglevel",
                    "error",
                    "-y",
                    "-i",
                    str(stage_input),
                    "-af",
                    voice_filter,
                    "-ar",
                    str(SOURCE_RATE),
                    "-ac",
                    "1",
                    "-c:a",
                    "pcm_s16le",
                    str(stage_output),
                ]
            )
            stage_input = stage_output
    finally:
        for path in temporary:
            path.unlink(missing_ok=True)


def build_runtime_pack(
    output: Path,
    destination: Path,
    decoded: dict[tuple[str, str, str], array[int]],
) -> dict[str, int | str]:
    runtime_input = output / "runtime-input" / "passive-presence-gate-s16-16k"
    runtime_input.mkdir(parents=True, exist_ok=True)
    sample_lines: list[str] = []
    for token_name in TOKENS:
        samples = decoded[("90pct", token_name, "s16_16k")]
        (runtime_input / f"{token_name}.bin").write_bytes(samples_to_pcm16(samples))
        sample_lines.append(f"{token_name} {len(samples)}")

    sample_file = runtime_input / "samples"
    sample_file.write_text("\n".join(sample_lines) + "\n", encoding="ascii")
    destination = destination.resolve()
    run(
        [
            sys.executable,
            str(Path(__file__).with_name("build_passive_voice_pack.py")),
            "--input",
            str(runtime_input),
            "--variant",
            "s16_16k",
            "--samples",
            str(sample_file),
            "--output",
            str(destination),
        ]
    )
    return {
        "bytes": destination.stat().st_size,
        "codec": "pcm_s16le",
        "sample_rate": SOURCE_RATE,
        "sha256": sha256_file(destination),
    }


def encode_variant(source: Path, destination: Path, sample_rate: int, codec: str) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    filters: list[str] = []
    if sample_rate < SOURCE_RATE:
        output_format = "u8" if codec == "pcm_u8" else "s16"
        filters.extend(
            [
                "lowpass=f=3400",
                (
                    f"aresample={sample_rate}:resampler=soxr:precision=28:"
                    f"osf={output_format}:dither_method=none"
                ),
            ]
        )

    command = [
        "ffmpeg",
        "-nostdin",
        "-hide_banner",
        "-loglevel",
        "error",
        "-y",
        "-i",
        str(source),
    ]
    if filters:
        command.extend(["-af", ",".join(filters)])
    command.extend(
        [
            "-ar",
            str(sample_rate),
            "-ac",
            "1",
            "-c:a",
            codec,
        ]
    )
    if codec == "adpcm_ima_wav":
        command.extend(["-block_size", "256"])
    command.append(str(destination))
    run(command)


def decode_pcm(path: Path, sample_rate: int) -> array[int]:
    result = run(
        [
            "ffmpeg",
            "-nostdin",
            "-hide_banner",
            "-loglevel",
            "error",
            "-i",
            str(path),
            "-ar",
            str(sample_rate),
            "-ac",
            "1",
            "-f",
            "s16le",
            "-c:a",
            "pcm_s16le",
            "pipe:1",
        ],
        capture=True,
    )
    return pcm16_samples(result.stdout)


def sample_metrics(samples: array[int], sample_rate: int) -> dict[str, int | float]:
    if not samples:
        return {"samples": 0, "duration_s": 0.0, "peak_dbfs": -math.inf, "rms_dbfs": -math.inf}
    peak = max(abs(value) for value in samples)
    rms = math.sqrt(sum(value * value for value in samples) / len(samples))
    return {
        "samples": len(samples),
        "duration_s": round(len(samples) / sample_rate, 6),
        "peak_dbfs": round(20 * math.log10(peak / 32768), 3) if peak else -math.inf,
        "rms_dbfs": round(20 * math.log10(rms / 32768), 3) if rms else -math.inf,
    }


def snr_db(reference: array[int], candidate: array[int]) -> float | None:
    count = min(len(reference), len(candidate))
    if count == 0:
        return None
    signal = sum(reference[index] ** 2 for index in range(count))
    error = sum((reference[index] - candidate[index]) ** 2 for index in range(count))
    if error == 0:
        return None
    return round(10 * math.log10(signal / error), 3)


def write_comparison_page(output: Path) -> None:
    rows: list[str] = []
    for rate_name in RATES:
        for token_name, spoken_text in TOKENS.items():
            cells = [f"<th>{html.escape(rate_name)} / {html.escape(spoken_text)}</th>"]
            source_relative = Path("source") / rate_name / f"{token_name}.wav"
            cells.append(
                "<td>"
                f"<audio controls preload=\"none\" src=\"{html.escape(source_relative.as_posix())}\"></audio>"
                "</td>"
            )
            for variant_name in VARIANTS:
                relative = Path("audition") / rate_name / variant_name / f"{token_name}.wav"
                cells.append(
                    "<td>"
                    f"<audio controls preload=\"none\" src=\"{html.escape(relative.as_posix())}\"></audio>"
                    "</td>"
                )
            rows.append("<tr>" + "".join(cells) + "</tr>")

    headings = "<th>untouched_polly_16k</th>" + "".join(
        f"<th>{html.escape(name)}</th>" for name in VARIANTS
    )
    document = f"""<!doctype html>
<html lang="en">
<meta charset="utf-8">
<title>Amy neural voice trial</title>
<style>
body {{ font: 14px sans-serif; margin: 24px; color: #202124; }}
table {{ border-collapse: collapse; }}
th, td {{ border: 1px solid #bbb; padding: 8px; text-align: left; }}
audio {{ width: 220px; }}
</style>
<h1>Amy neural voice trial</h1>
<table>
<thead><tr><th>Token</th>{headings}</tr></thead>
<tbody>
{''.join(rows)}
</tbody>
</table>
</html>
"""
    (output / "comparison.html").write_text(document, encoding="utf-8")


def main() -> int:
    args = parse_args()
    require_command("ffmpeg")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    voice_filters = MASTERING_PROFILES[args.mastering_profile]
    if args.runtime_pack is not None and args.mastering_profile != "passive-presence-gate":
        raise ValueError("--runtime-pack requires --mastering-profile passive-presence-gate")

    client = None
    if not args.encode_only:
        client = polly_client(args.region)
        verify_voice(client)

    manifest: dict[str, Any] = {
        "schema": 1,
        "voice": {
            "language": "English (British)",
            "language_code": LANGUAGE_CODE,
            "voice_id": VOICE_ID,
            "engine": ENGINE,
            "region": args.region,
            "source_format": "pcm_s16le",
            "source_sample_rate": SOURCE_RATE,
            "mastering_profile": args.mastering_profile,
            "processing_filter": ",".join(voice_filters),
            "processing_stages": list(voice_filters),
        },
        "tokens": {},
        "totals": {},
    }

    decoded: dict[tuple[str, str, str], array[int]] = {}
    for rate_name, rate in RATES.items():
        for token_name, spoken_text in TOKENS.items():
            source_path = output / "source" / rate_name / f"{token_name}.wav"
            clean_path = output / "clean" / rate_name / f"{token_name}.wav"
            reference_path = output / "reference" / rate_name / f"{token_name}.wav"
            _, text_type = build_polly_text(spoken_text, rate)
            request_metadata: dict[str, Any] = {
                "request_text": spoken_text,
                "text_type": text_type,
                "prosody_rate": rate or "normal",
                "content_type": "audio/pcm",
                "request_characters": len(spoken_text),
            }

            if not args.encode_only and (args.force or not source_path.exists()):
                assert client is not None
                raw, request_metadata = synthesize(client, spoken_text, rate)
                write_pcm16_wave(source_path, pcm16_samples(raw), SOURCE_RATE)
            elif not source_path.exists():
                raise RuntimeError(f"source WAV is missing for --encode-only: {source_path}")

            with wave.open(str(source_path), "rb") as source_wave:
                if (
                    source_wave.getnchannels() != 1
                    or source_wave.getsampwidth() != 2
                    or source_wave.getframerate() != SOURCE_RATE
                ):
                    raise RuntimeError(f"unexpected source WAV format: {source_path}")
                source_pcm = source_wave.readframes(source_wave.getnframes())

            cleaned, clean_metadata = clean_source_pcm(source_pcm)
            write_pcm16_wave(clean_path, cleaned, SOURCE_RATE)
            make_voice_reference(clean_path, reference_path, voice_filters)
            reference_samples = decode_pcm(reference_path, SOURCE_RATE)

            token_entry: dict[str, Any] = {
                "spoken_text": spoken_text,
                "rate": rate or "normal",
                "source": {
                    "path": source_path.relative_to(output).as_posix(),
                    "bytes": source_path.stat().st_size,
                    "sha256": sha256_file(source_path),
                    **request_metadata,
                },
                "cleaning": clean_metadata,
                "reference": {
                    "path": reference_path.relative_to(output).as_posix(),
                    "bytes": reference_path.stat().st_size,
                    "sha256": sha256_file(reference_path),
                    **sample_metrics(reference_samples, SOURCE_RATE),
                },
                "variants": {},
            }

            for variant_name, (sample_rate, codec) in VARIANTS.items():
                encoded_path = output / "encoded" / rate_name / variant_name / f"{token_name}.wav"
                audition_path = output / "audition" / rate_name / variant_name / f"{token_name}.wav"
                if variant_name == "u8_8k":
                    write_pcm_u8_wave(
                        encoded_path,
                        decoded[(rate_name, token_name, "s16_8k")],
                        sample_rate,
                    )
                else:
                    encode_variant(reference_path, encoded_path, sample_rate, codec)
                samples = decode_pcm(encoded_path, sample_rate)
                logical_samples = round(len(reference_samples) * sample_rate / SOURCE_RATE)
                encoded_samples = len(samples)
                samples = samples[:logical_samples]
                decoded[(rate_name, token_name, variant_name)] = samples
                write_pcm16_wave(audition_path, samples, sample_rate)
                token_entry["variants"][variant_name] = {
                    "path": encoded_path.relative_to(output).as_posix(),
                    "audition_path": audition_path.relative_to(output).as_posix(),
                    "codec": codec,
                    "sample_rate": sample_rate,
                    "bytes": encoded_path.stat().st_size,
                    "encoded_samples": encoded_samples,
                    "logical_samples": logical_samples,
                    "sha256": sha256_file(encoded_path),
                    **sample_metrics(samples, sample_rate),
                }

            reference = decoded[(rate_name, token_name, "s16_8k")]
            for variant_name in ("u8_8k", "mulaw_8k", "ima_adpcm_8k"):
                token_entry["variants"][variant_name]["snr_vs_s16_8k_db"] = snr_db(
                    reference,
                    decoded[(rate_name, token_name, variant_name)],
                )

            manifest["tokens"][f"{rate_name}/{token_name}"] = token_entry

    for rate_name in RATES:
        manifest["totals"][rate_name] = {}
        for variant_name in VARIANTS:
            entries = [
                manifest["tokens"][f"{rate_name}/{token_name}"]["variants"][variant_name]
                for token_name in TOKENS
            ]
            snr_values = [
                entry["snr_vs_s16_8k_db"]
                for entry in entries
                if entry.get("snr_vs_s16_8k_db") is not None
            ]
            manifest["totals"][rate_name][variant_name] = {
                "bytes": sum(entry["bytes"] for entry in entries),
                "duration_s": round(sum(entry["duration_s"] for entry in entries), 6),
                "mean_snr_vs_s16_8k_db": (
                    round(sum(snr_values) / len(snr_values), 3) if snr_values else None
                ),
            }

    if args.runtime_pack is not None:
        manifest["runtime_pack"] = build_runtime_pack(output, args.runtime_pack, decoded)

    (output / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    write_comparison_page(output)
    print(f"Voice trial written to {output}")
    print(f"Audition page: {output / 'comparison.html'}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (RuntimeError, ValueError, subprocess.CalledProcessError) as error:
        print(f"error: {error}", file=sys.stderr)
        raise SystemExit(1)
