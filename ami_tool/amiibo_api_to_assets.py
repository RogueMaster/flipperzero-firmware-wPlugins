"""
Read the JSON database from AmiiboAPI repository, process it, and convert it
into asset files for the Flipper Zero Amiibo Toolkit application.

The original data is re-assembled to be flat, so that Flipper Zero does not need
to look up multiple files to get all the information about a single Amiibo.
"""

import requests


AMIIBO_JSON_URL = "https://raw.githubusercontent.com/8bitDream/AmiiboAPI/refs/heads/master/database/amiibo.json"
GAMES_INFO_JSON_URL = "https://raw.githubusercontent.com/8bitDream/AmiiboAPI/refs/heads/master/database/games_info.json"


def fetch_json(url: str) -> dict:
    """
    Fetch JSON data from a given URL, and output it as a dictionary.
    :param url: The URL to fetch JSON data from.
    :type url: str
    :return: The JSON data as a dictionary.
    :rtype: dict
    """
    response = requests.get(url)
    response.raise_for_status()
    return response.json()


def process_amiibo_data(amiibo_data: dict) -> dict[str, str]:
    """
    Process the Amiibo data and convert it into asset files.

    :param amiibo_data: The Amiibo data as a dictionary.
    :type amiibo_data: dict
    :return: Mapping of amiibo_id_clean -> amiibo_name (for sorting elsewhere).
    :rtype: dict[str, str]
    """
    amiibos = amiibo_data.get("amiibos", {})
    amiibo_series = amiibo_data.get("amiibo_series", {})
    game_series = amiibo_data.get("game_series", {})
    types = amiibo_data.get("types", {})
    characters = amiibo_data.get("characters", {})

    amiibo_strs: dict[str, str] = {}
    amiibo_mapping_strs: dict[str, str] = {}
    amiibo_id_to_name: dict[str, str] = {}

    for amiibo_id, amiibo in amiibos.items():
        amiibo_id_clean = amiibo_id[2:]  # Remove "0x" prefix
        name = amiibo.get("name", "Unknown")

        # Save for later sorting (e.g. game mapping files)
        amiibo_id_to_name[amiibo_id_clean] = name

        character = characters.get(f"0x{amiibo_id_clean[0:4]}", "Unknown")
        amiibo_series_name = amiibo_series.get(f"0x{amiibo_id_clean[12:14]}", "Unknown")
        game_series_name = game_series.get(f"0x{amiibo_id_clean[0:3]}", "Unknown")
        type_name = types.get(f"0x{amiibo_id_clean[6:8]}", "Unknown")

        release_info = amiibo.get("release", {})
        release_info_strs = []
        for region, date in release_info.items():
            release_info_strs.append(f"{region}.{date}")
        release_info_line = ",".join(release_info_strs)

        amiibo_strs[amiibo_id_clean] = (
            f"{name}|{character}|{amiibo_series_name}|{game_series_name}|{type_name}|{release_info_line}"
        )
        amiibo_mapping_strs[name] = amiibo_id_clean

    # Already sorted as you had
    amiibo_strs = dict(sorted(amiibo_strs.items()))
    amiibo_mapping_strs = dict(sorted(amiibo_mapping_strs.items()))

    with open("files/amiibo.dat", "w") as amiibo_file:
        amiibo_file.write("Filetype: AmiTool Amiibo DB\n")
        amiibo_file.write("Version: 1\n")
        amiibo_file.write(f"AmiiboCount: {len(amiibos)}\n")
        amiibo_file.write("\n")

        for amiibo_id, amiibo_str in amiibo_strs.items():
            amiibo_file.write(f"{amiibo_id}: {amiibo_str}\n")

    with open("files/amiibo_name.dat", "w") as amiibo_name_file:
        amiibo_name_file.write("Filetype: AmiTool Amiibo Name DB\n")
        amiibo_name_file.write("Version: 1\n")
        amiibo_name_file.write(f"AmiiboCount: {len(amiibos)}\n")
        amiibo_name_file.write("\n")

        for amiibo_name, amiibo_id in amiibo_mapping_strs.items():
            amiibo_name_file.write(f"{amiibo_name}: {amiibo_id}\n")

    return amiibo_id_to_name


def _generate_usage_string(info: dict, platform: str) -> str:
    """
    Generate a usage string for a given platform from the provided info.
    """
    usage_str = f"{platform}^{info['gameName']}"

    if "amiiboUsage" in info:
        for usage in info["amiiboUsage"]:
            usage_str += f"^{usage['Usage']}*{usage.get('write', 'False')}"

    return usage_str


def _sort_ids_by_amiibo_name(
    amiibo_ids: list[str], amiibo_id_to_name: dict[str, str]
) -> list[str]:
    """
    Sort amiibo IDs by their amiibo name (stable fallback to ID).
    """
    return sorted(
        amiibo_ids,
        key=lambda aid: (amiibo_id_to_name.get(aid, "").casefold(), aid.casefold()),
    )


def _write_game_files(
    console: str,
    games_map: dict[str, list[str]],
    amiibo_id_to_name: dict[str, str],
):
    """
    Write both game list and mapping files for a given console, fully sorted.
    """
    # Ensure each game's amiibo ID list is sorted by amiibo name
    for game_name, ids in games_map.items():
        games_map[game_name] = _sort_ids_by_amiibo_name(ids, amiibo_id_to_name)

    # Sort games by name for deterministic UI streaming
    sorted_game_names = sorted(games_map.keys(), key=lambda n: n.casefold())

    with open(f"files/game_{console.lower()}.dat", "w") as games_file:
        games_file.write("Filetype: AmiTool Games DB\n")
        games_file.write("Version: 1\n")
        games_file.write(f"Console: {console}\n")
        games_file.write(f"GameCount: {len(games_map)}\n")
        games_file.write("\n")

        for game_name in sorted_game_names:
            games_file.write(f"{game_name}\n")

    with open(f"files/game_{console.lower()}_mapping.dat", "w") as mapping_file:
        mapping_file.write("Filetype: AmiTool Games Mapping DB\n")
        mapping_file.write("Version: 1\n")
        mapping_file.write(f"Console: {console}\n")
        mapping_file.write(f"GameCount: {len(games_map)}\n")
        mapping_file.write("\n")

        for game_name in sorted_game_names:
            mapping_file.write(f"{game_name}: {'|'.join(games_map[game_name])}\n")


def process_games_info_data(games_info_data: dict, amiibo_id_to_name: dict[str, str]):
    """
    Process the games info data and convert it into asset files, with full alphabetical sorting.

    - Games are sorted by game name.
    - Amiibo IDs in mapping files are sorted by amiibo name (from amiibo.json).
    - amiibo_usage.dat entries are sorted by amiibo name; per-amiibo usage entries by platform, then game name.
    """
    games_3ds: dict[str, list[str]] = {}
    games_wii_u: dict[str, list[str]] = {}
    games_switch: dict[str, list[str]] = {}
    games_switch2: dict[str, list[str]] = {}

    usage_lines: list[tuple[str, str, str]] = []
    # tuple: (sort_key_name_casefold, amiibo_id_clean, "amiibo_id_clean: ...")

    for amiibo_id, game_info in games_info_data.get("amiibos", {}).items():
        amiibo_id_clean = amiibo_id[2:]  # Remove "0x" prefix
        amiibo_name = amiibo_id_to_name.get(amiibo_id_clean, "Unknown")

        # Build usage entries, sorted later
        usage_entries: list[tuple[str, str, str]] = []  # (platform, gameName, usageStr)

        for info_3ds in game_info.get("games3DS", []):
            games_3ds.setdefault(info_3ds["gameName"], []).append(amiibo_id_clean)
            usage_entries.append(
                ("3DS", info_3ds["gameName"], _generate_usage_string(info_3ds, "3DS"))
            )

        for info_wii_u in game_info.get("gamesWiiU", []):
            games_wii_u.setdefault(info_wii_u["gameName"], []).append(amiibo_id_clean)
            usage_entries.append(
                (
                    "WiiU",
                    info_wii_u["gameName"],
                    _generate_usage_string(info_wii_u, "WiiU"),
                )
            )

        for info_switch in game_info.get("gamesSwitch", []):
            games_switch.setdefault(info_switch["gameName"], []).append(amiibo_id_clean)
            usage_entries.append(
                (
                    "Switch",
                    info_switch["gameName"],
                    _generate_usage_string(info_switch, "Switch"),
                )
            )

        for info_switch2 in game_info.get("gamesSwitch2", []):
            games_switch2.setdefault(info_switch2["gameName"], []).append(
                amiibo_id_clean
            )
            usage_entries.append(
                (
                    "Switch2",
                    info_switch2["gameName"],
                    _generate_usage_string(info_switch2, "Switch2"),
                )
            )

        usage_entries.sort(key=lambda t: (t[0].casefold(), t[1].casefold()))
        usage_str = "|".join(u[2] for u in usage_entries)
        usage_lines.append(
            (
                amiibo_name.casefold(),
                amiibo_id_clean,
                f"{amiibo_id_clean}: {usage_str}\n",
            )
        )

    # Sort amiibo_usage.dat by amiibo name (then ID for stability)
    usage_lines.sort(key=lambda t: (t[0], t[1].casefold()))

    with open("files/amiibo_usage.dat", "w") as usage_file:
        usage_file.write("Filetype: AmiTool Usage DB\n")
        usage_file.write("Version: 1\n")
        usage_file.write(f"AmiiboCount: {len(games_info_data.get('amiibos', {}))}\n")
        usage_file.write("\n")

        for _, __, line in usage_lines:
            usage_file.write(line)

    # Write game list + mapping files, fully sorted
    _write_game_files("3DS", games_3ds, amiibo_id_to_name)
    _write_game_files("WiiU", games_wii_u, amiibo_id_to_name)
    _write_game_files("Switch", games_switch, amiibo_id_to_name)
    _write_game_files("Switch2", games_switch2, amiibo_id_to_name)


def main():
    """
    Main function to fetch and process Amiibo data.
    """
    amiibo_data = fetch_json(AMIIBO_JSON_URL)
    games_info_data = fetch_json(GAMES_INFO_JSON_URL)

    amiibo_id_to_name = process_amiibo_data(amiibo_data)
    process_games_info_data(games_info_data, amiibo_id_to_name)


if __name__ == "__main__":
    main()
