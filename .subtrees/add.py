#!/usr/bin/env python3
import tempfile
import pathlib
import shutil
import sys
import re

import common


def print_usage_and_exit():
    print("Usage 1: <path> <repo url> <branch> [subdir]")
    print("Usage 2: <path> <repo url>/tree/<branch>[/subdir]")
    sys.exit(1)


if __name__ == "__main__":
    common.check_workdir_state()

    if len(sys.argv) < 3 or not sys.argv[1] or not sys.argv[2]:
        print_usage_and_exit()

    if len(sys.argv) == 3 and "/tree/" not in sys.argv[2]:
        print_usage_and_exit()

    path = sys.argv[1]
    repo = sys.argv[2].strip("/")
    if len(sys.argv) == 3:
        res = re.match(r"(https?://[^/]+/[^/]+/[^/]+)/tree/([^/]+)/?(.*)", repo)
        if not res:
            print(f"Could not parse repo URL: {repo}")
            sys.exit(1)
        repo = res.group(1)
        branch = res.group(2)
        subdir = res.group(3).strip("/")
    else:
        branch = sys.argv[3]
        if len(sys.argv) > 4:
            subdir = sys.argv[4].strip("/")
        else:
            subdir = ""
    gitsubtree = common.REPO_ROOT / path / ".gitsubtree"

    prevremotedir = None
    if gitsubtree.is_file():
        print("Subtree already exists, adding new remote to it.")
        prevremotedir = pathlib.Path(tempfile.mkdtemp(prefix="gitsubtree"))
        # To use 2 remotes for subtree we need to remove current one, add new one, then merge
        shutil.move(path, prevremotedir / path)
        common.git("add", path)
        common.git("commit", "-m", f"Add new remote for {path}")

    if subdir == "":
        subdir = "/"
        common.git(
            "subtree",
            "add",
            "-P",
            path,
            repo,
            branch,
            "-m",
            f"Add {path} from {repo}",
        )
        commit = None
    else:
        commit = common.subdir_split_helper(path, repo, branch, subdir, "add")

    if prevremotedir:
        shutil.rmtree(path)
        shutil.move(prevremotedir / path, path)
        prevremotedir.rmdir()

    line = f"{repo} {branch} {subdir}" + (f" {commit}" if commit else "")
    if gitsubtree.is_file():
        # Add new remote at the top
        lines = gitsubtree.read_text().splitlines()
        lines.insert(0, line)
    else:
        lines = [line]
    gitsubtree.write_text("\n".join(lines) + "\n")
    common.git("add", str(gitsubtree.relative_to(common.REPO_ROOT)))
    common.git("commit", "--amend", "--no-edit")

    if prevremotedir:
        prevremotedir = None
        print(
            "Added new remote for existing subtree, you must resolve conflicts manually..."
        )
