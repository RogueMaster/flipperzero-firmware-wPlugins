#!/bin/python3

import json
import re
from dataclasses import dataclass
from enum import Enum
from pathlib import Path
from typing import IO, Any

import pypinyin
import yaml


class LineType(Enum):
    Metro = "mtr"
    BRT = "brt"
    Tram = "trm"
    Train = "trn"
    BUS = "bus"


@dataclass
class Line:
    ids: list[str]
    name: str
    short_name: str
    type: LineType
    stations: dict[str, str]
    station_id_lens: set[int]


@dataclass
class City:
    id: str
    name: str


def parse_line_type(type_name) -> LineType:
    match type_name:
        case "地铁" | "轻轨" | "磁悬浮":
            return LineType.Metro
        case "快速公交":
            return LineType.BRT
        case "火车" | "市郊铁路":
            return LineType.Train
        case "电车" | "有轨电车":
            return LineType.Tram
        case "公交":
            return LineType.BUS
        case _:
            raise NotImplementedError(f"不支持的线路类型“{type_name}”")


def line_name2short_tag(line_name: str, line_type: LineType) -> str:
    short_name = ""
    addition_prefix = ""

    # 后缀缩写规则
    if line_name.endswith(("号线", "快线")):
        line_name = line_name[:-2]
    elif line_name.endswith("线"):
        line_name = line_name[:-1]

    # 特殊符号缩写规则
    line_name = line_name.replace("/", "_")

    # 对应线路类型缩写规则
    if line_type == LineType.BRT:
        if "快速公交" in line_name:
            line_name = line_name.replace("快速公交", "")
            addition_prefix = "brt"
    elif line_type == LineType.Tram:
        if "有轨电车" in line_name:
            line_name = line_name.replace("有轨电车", "")
            addition_prefix = "trm"

    # 汉字缩写规则
    # 线路名为汉字，首字全拼，后续拼音首字母
    for idx, char in enumerate(line_name):
        if re.match(r"[一-龟]", char):
            py = pypinyin.lazy_pinyin(char)[0]
            short_name += py if idx == 0 else py[0]
        else:
            short_name += char

    short_name = addition_prefix + short_name
    short_name = short_name.lower()
    return short_name


def process_line_ids(raw_line_id: str | list[str]) -> list[str]:
    if isinstance(raw_line_id, list):
        return [to[4:] for to in raw_line_id]
    else:
        return [raw_line_id[4:]]


def process_stations_map(raw_stations_map: dict[str, str] | None) -> dict[str, str]:
    if raw_stations_map is not None:
        return {k[4:]: v for k, v in raw_stations_map.items()}
    else:
        return {}


def calibrate_station_id_lens(stations_map: dict[str, str]) -> set[int]:
    id_lens = set(len(k) for k in stations_map.keys())
    if len(id_lens) == 0:
        id_lens.add(0)
    id_lens = list(id_lens)
    id_lens.sort()
    return id_lens


def calibrate_line_prefix_lens(lines: list[Line]) -> set[int]:
    line_prefix_lens = set()
    for line in lines:
        line_prefix_lens.update(len(line_id) for line_id in line.ids)
    line_prefix_lens = list(line_prefix_lens)
    line_prefix_lens.sort()
    return line_prefix_lens


def parse_line_data(raw_data: dict) -> Line:
    line_ids = process_line_ids(raw_data["id"])
    line_name = raw_data["name"]
    line_type = parse_line_type(raw_data["type"])
    line_short_name = line_name2short_tag(line_name, line_type)
    line_stations_map = process_stations_map(raw_data["stations"])
    station_id_lens = calibrate_station_id_lens(line_stations_map)
    return Line(
        ids=line_ids,
        name=line_name,
        short_name=line_short_name,
        type=line_type,
        stations=line_stations_map,
        station_id_lens=station_id_lens,
    )


def parse_city_lines(raw_data: list):
    return [parse_line_data(to) for to in raw_data]


def load_city_code_map(f_path: Path) -> dict[str, str]:
    with f_path.open("r", encoding="utf8") as fp:
        return json.load(fp)


def query_city_by_name(code_map: dict[str, str], city_name: str) -> City:
    for k, v in code_map.items():
        if city_name == v:
            return City(id=k, name=v)
    else:
        raise ValueError(f"未知的城市“{city_name}”")


def flipper_fmt_wr_kv(fp: IO, k: str = None, v: Any = None, map: dict[str, Any] = None):
    if k is not None and v is not None and map is None:
        map = {k: v}
    if map is not None:
        for k, v in map.items():
            if isinstance(v, (set, list, tuple)):
                v = " ".join(str(to) for to in v)
            fp.write(f"{k}: {v}\n")


def flipper_fmt_wr_ln(fp: IO):
    fp.write("\n")


def flipper_fmt_wr_comm(fp: IO, text: str):
    fp.write(f"# {text}\n")


def render_line_file(f_path: Path, line: Line, city_info: City):
    with f_path.open("w", encoding="utf8") as fp:
        flipper_fmt_wr_comm(fp, f"{city_info.name}-{city_info.id}")
        flipper_fmt_wr_kv(
            fp,
            map={
                "name": line.name,
                "len": line.station_id_lens,
                "type": line.type.value,
            },
        )
        flipper_fmt_wr_ln(fp)
        flipper_fmt_wr_kv(fp, map=line.stations)


def render_prefix_map_file(f_path: Path, lines: list[Line], city_info: City):
    line_prefix_lens = calibrate_line_prefix_lens(lines)
    line_prefix_map = {}
    for line in lines:
        for line_id in line.ids:
            line_prefix_map[line_id] = line.short_name
    line_prefix_map = dict(sorted(line_prefix_map.items(), key=lambda x: x[0]))
    with f_path.open("w", encoding="utf8") as fp:
        flipper_fmt_wr_comm(fp, f"{city_info.name}-{city_info.id}")
        flipper_fmt_wr_kv(fp, "prefix-len", line_prefix_lens)
        flipper_fmt_wr_ln(fp)
        flipper_fmt_wr_kv(fp, map=line_prefix_map)


def render_city_code_file(
    f_path: Path, city_code_map: dict[str, str], has_line_cities: list[str]
):
    city_code_map = city_code_map.copy()

    has_line = {city_id: city_code_map.pop(city_id) for city_id in has_line_cities}
    not_has_line = city_code_map
    with f_path.open("w", encoding="utf8") as fp:
        flipper_fmt_wr_comm(fp, "GB/T 13497-1992")
        # 具有线路数据的城市前置 优化查表速度
        flipper_fmt_wr_kv(fp, map=has_line)
        flipper_fmt_wr_ln(fp)
        flipper_fmt_wr_kv(fp, map=not_has_line)


if __name__ == "__main__":
    STATIONS_DATA = Path("./data/line_stations.yml")
    CITY_CODE_DATA = Path("./data/GBT 13497-1992.json")
    OUTPUT_PATH = Path("./resources")

    db_data = yaml.load(STATIONS_DATA.open("r", encoding="utf8"), Loader=yaml.Loader)
    city_code_map = load_city_code_map(CITY_CODE_DATA)

    has_line_cities = []
    for city in db_data["cities"]:
        city_info = query_city_by_name(city_code_map, city["name"])
        city_lines = parse_city_lines(city["lines"])

        city_path = OUTPUT_PATH / "stations" / city_info.id
        if not city_path.is_dir():
            city_path.mkdir(parents=True)

        render_prefix_map_file(city_path / "M-prefix-map.txt", city_lines, city_info)
        for line in city_lines:
            render_line_file(city_path / f"L-{line.short_name}.txt", line, city_info)

        print(f"Render Line DB “{city_info.name}” OK!")
        has_line_cities.append(city_info.id)

    render_city_code_file(OUTPUT_PATH / "city_code.txt", city_code_map, has_line_cities)
    print(f"Render City DB OK!")
