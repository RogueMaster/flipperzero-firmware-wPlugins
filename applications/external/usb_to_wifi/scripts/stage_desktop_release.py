"""Move proven CI assets to an existing draft without local bandwidth or clobber.

This is an explicitly dispatched publication step, not an application runtime.
It never publishes the draft or replaces an existing asset with different bytes.
"""

import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tarfile
import tempfile
import zipfile


def gh_json(*args):
    return json.loads(subprocess.check_output(["gh", *args], text=True))


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def validate_inventory(inventory, version):
    project = [
        item
        for item in inventory
        if item["name"].replace("_", "-").lower() == "flipper-usb-internet-bridge-host"
    ]
    # Editable installs may expose both source .egg-info and installed
    # .dist-info. Identical versions are fine; conflicting/stale ones are not.
    versions = [item.get("version") for item in project]
    print("Application metadata versions:", versions, flush=True)
    if not versions or any(value != version for value in versions):
        raise ValueError(
            f"Archive versions {versions!r} do not match release {version}"
        )


def main():
    repo = os.environ["GITHUB_REPOSITORY"]
    tag = os.environ["RELEASE_TAG"]
    run_id = os.environ["BUILD_RUN_ID"]
    sha = os.environ["BUILD_SOURCE_SHA"]
    if (
        not re.fullmatch(r"v\d+\.\d+\.\d+", tag)
        or not run_id.isdigit()
        or not re.fullmatch(r"[0-9a-f]{40}", sha)
    ):
        raise ValueError("Invalid release inputs")
    run = gh_json(
        "run",
        "view",
        run_id,
        "--repo",
        repo,
        "--json",
        "headSha,status,conclusion,jobs",
    )
    if (
        run["headSha"] != sha
        or run["status"] != "completed"
        or run["conclusion"] != "success"
        or len(run["jobs"]) != 2
        or any(job["conclusion"] != "success" for job in run["jobs"])
    ):
        raise ValueError("Both target-platform jobs must pass at the exact source SHA")
    release = gh_json(
        "release",
        "view",
        tag,
        "--repo",
        repo,
        "--json",
        "isDraft,targetCommitish,assets",
    )
    if not release["isDraft"] or release["targetCommitish"] != sha:
        raise ValueError("Only the matching existing draft can be staged")
    existing = {item["name"]: item for item in release["assets"]}
    with tempfile.TemporaryDirectory(prefix="fib-release-stage-") as folder:
        root = Path(folder)
        subprocess.run(
            ["gh", "run", "download", run_id, "--repo", repo, "--dir", str(root)],
            check=True,
        )
        windows = list(root.rglob("fib-bridge-desktop-windows-x86_64.zip"))
        linux = list(root.rglob("fib-bridge-linux-x86_64.tar.gz"))
        if len(windows) != 1 or len(linux) != 1:
            raise ValueError("Exactly one GUI package per platform is required")
        sources = []
        with zipfile.ZipFile(windows[0]) as archive:
            if archive.testzip() is not None:
                raise ValueError("Windows ZIP integrity failed")
            prefix = "fib-bridge-desktop/"
            for required in (
                "fib-bridge-desktop.exe",
                "fib-bridge-cli.exe",
                "SOURCES.md",
            ):
                archive.getinfo(prefix + required)
            validate_inventory(
                json.loads(archive.read(prefix + "dependency-versions.json")), tag[1:]
            )
            sources.extend(json.loads(archive.read(prefix + "native-sources.json")))
        with tarfile.open(linux[0], "r:gz") as archive:
            prefix = "fib-bridge-linux-x86_64-package/"
            for required in (
                "install.sh",
                "fib-bridge",
                "fib-bridge-desktop/fib-bridge-desktop",
            ):
                if not archive.getmember(prefix + required).mode & 0o111:
                    raise ValueError("Linux executable bit missing")
            if any("libreadline" in name for name in archive.getnames()):
                raise ValueError("Unexpected unused GPL library")
            validate_inventory(
                json.load(
                    archive.extractfile(
                        prefix + "fib-bridge-desktop/dependency-versions.json"
                    )
                ),
                tag[1:],
            )
            sources.extend(
                json.load(
                    archive.extractfile(
                        prefix + "fib-bridge-desktop/native-sources.json"
                    )
                )
            )
        files = {windows[0].name: windows[0], linux[0].name: linux[0]}
        for record in sources:
            name = record.get("release_source_asset")
            if not name:
                continue
            if Path(name).name != name:
                raise ValueError("Invalid source archive name")
            matches = list(root.rglob(name))
            if not matches or any(digest(path) != record["sha256"] for path in matches):
                raise ValueError("Corresponding source SHA-256 mismatch: " + name)
            files[name] = matches[0]
        for name, path in files.items():
            checksum = "sha256:" + digest(path)
            if name in existing:
                if (
                    existing[name]["state"] != "uploaded"
                    or existing[name].get("digest") != checksum
                ):
                    raise ValueError(
                        "Existing asset differs; refusing to overwrite: " + name
                    )
                continue
            print("Uploading verified asset:", name, checksum, flush=True)
            subprocess.run(
                ["gh", "release", "upload", tag, str(path), "--repo", repo], check=True
            )
        result = gh_json("release", "view", tag, "--repo", repo, "--json", "assets")
        uploaded = {item["name"]: item for item in result["assets"]}
        for name, path in files.items():
            if uploaded[name]["state"] != "uploaded" or uploaded[name].get(
                "digest"
            ) != "sha256:" + digest(path):
                raise ValueError("Published draft asset verification failed: " + name)
        print("All desktop/source assets verified; release remains a draft.")


if __name__ == "__main__":
    main()
