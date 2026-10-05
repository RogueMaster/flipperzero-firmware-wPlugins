"""Build-time filtering for macOS Finder / AppleDouble metadata."""

import os


def is_macos_metadata_name(name) -> bool:
    basename = os.path.basename(os.fspath(name))
    return basename == ".DS_Store" or basename.startswith("._")


def is_macos_metadata_path(path) -> bool:
    normalized = os.fspath(path).replace("\\", "/")
    return any(
        is_macos_metadata_name(component)
        for component in normalized.split("/")
        if component
    )


def filter_macos_metadata_names(names):
    return [name for name in names if not is_macos_metadata_name(name)]
