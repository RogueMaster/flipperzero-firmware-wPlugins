#!/usr/bin/env python3
import pathlib
import sys

import common


if __name__ == "__main__":
    common.check_workdir_state()

    if len(sys.argv) > 1:
        subtrees = sys.argv[1:]
    else:
        subtrees = sorted(list(common.REPO_ROOT.glob("**/.gitsubtree")))

    for subtree in subtrees:
        if not isinstance(subtree, pathlib.Path):
            if not subtree.endswith("/.gitsubtree"):
                subtree += "/.gitsubtree"
            subtree = common.REPO_ROOT / subtree
        path = str(subtree.parent.relative_to(common.REPO_ROOT))
        print(f"\n\nUpdating {path}...")

        for remote in subtree.read_text().splitlines():
            if remote.startswith("#"):
                continue
            params = remote.split(" ")
            if len(params) == 4:
                repo, branch, subdir, cached = params
            else:
                repo, branch, subdir = params
            if subdir == "/":
                result, status = common.git(
                    "subtree",
                    "pull",
                    "-P",
                    path,
                    repo,
                    branch,
                    "-m",
                    f"Merge {path} from {repo}",
                    tee=True,
                )
                common.check_merge_result(path, repo, result, status)
            else:
                commit = common.subdir_split_helper(
                    path, repo, branch, subdir, "merge", cached=cached
                )
                if commit:
                    lines = subtree.read_text().splitlines()
                    for i in range(len(lines)):
                        if lines[i].startswith(repo):
                            lines[i] = f"{repo} {branch} {subdir} {commit}"
                            break
                    subtree.write_text("\n".join(lines) + "\n")
                    common.git("add", str(subtree.relative_to(common.REPO_ROOT)))
                    common.git("commit", "--amend", "--no-edit")

    common.send_alert("Subtree update finished", "Double check merge commits")
