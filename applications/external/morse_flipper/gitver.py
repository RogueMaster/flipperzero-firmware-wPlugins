import os
import subprocess
import time

git_commit = "fail"
git_host = "fail"
git_built = "fail"


def read_text(path, default=""):
    try:
        with open(path, "rt") as handle:
            return handle.read().strip()
    except Exception:
        return default


def join_path(left, right):
    try:
        if not left:
            return right
        if left.endswith("/"):
            return left + right
        return left + "/" + right
    except Exception:
        return right


def parent_dir(path):
    try:
        while path and path[-1] != "/":
            path = path[:-1]
        return path
    except Exception:
        return ""


def resolve_ref(git_dir, ref_name):
    try:
        ref_value = read_text(join_path(git_dir, ref_name))
        if ref_value:
            return ref_value

        packed_refs = read_text(join_path(git_dir, "packed-refs"))
        if not packed_refs:
            return ""

        for line in packed_refs.splitlines():
            if not line or line[0] == "#" or line[0] == "^":
                continue

            parts = line.split()
            if len(parts) >= 2 and parts[1] == ref_name:
                return parts[0]
    except Exception:
        return ""

    return ""


def format_commit_groups(commit_text):
    try:
        if not commit_text:
            return "fail"

        groups = []
        while commit_text:
            groups.insert(0, commit_text[-4:])
            commit_text = commit_text[:-4]

        return " ".join(groups)
    except Exception:
        return "fail"


def get_short_commit_from_git(repo_root):
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "--short", "HEAD"],
            cwd=repo_root,
            text=True,
        ).strip()
    except Exception:
        return ""


def get_short_commit_from_git_files(repo_root):
    try:
        git_dir = join_path(repo_root, ".git")
        git_head = read_text(git_dir)

        if git_head.startswith("gitdir:"):
            git_dir = git_head[7:].strip()
            if git_dir and not git_dir.startswith("/"):
                git_dir = join_path(repo_root, git_dir)

        git_head = read_text(join_path(git_dir, "HEAD"))
        if git_head.startswith("ref: "):
            git_head = resolve_ref(git_dir, git_head[5:].strip())

        if not git_head:
            return ""

        return git_head[:7]
    except Exception:
        return ""


def get_build_host():
    try:
        return os.uname()[1][:16]
    except Exception:
        pass

    host_name = read_text("/etc/hostname", "fail")
    if host_name:
        return host_name[:16]

    return "fail"


def get_build_time_utc():
    try:
        return time.strftime("%Y%m%d %H%M%SZ", time.gmtime())
    except Exception:
        return "fail"


try:
    repo_root = parent_dir(__file__)
    short_commit = get_short_commit_from_git(repo_root)
    if not short_commit:
        short_commit = get_short_commit_from_git_files(repo_root)

    git_commit = format_commit_groups(short_commit)
    git_host = get_build_host()
    git_built = get_build_time_utc()
except Exception:
    pass
