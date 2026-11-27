# gen_version.py — PlatformIO pre-build extra_script.
#
# Resolves the firmware release version and hands it to the project sources as
# the FW_VERSION string macro (boot banner, dashboard, /api/status). First hit
# wins:
#   1. FW_VERSION environment variable (manual override)
#   2. GITHUB_REF_NAME when CI builds a tag — the release binaries
#   3. `git describe --tags --always --dirty`, prefixed "dev-" unless it is a
#      v* tag description (a shallow CI checkout only has the commit hash)
#   4. "dev"
#
# The value lands inside a C string, a JSON string and HTML text, so it is
# reduced to [A-Za-z0-9._+-] and capped in length.
Import("env")
import os
import re
import subprocess

_MAX_LEN = 40


def _git_describe(project_dir):
    try:
        out = subprocess.check_output(
            ["git", "-C", project_dir, "describe", "--tags", "--always", "--dirty"],
            stderr=subprocess.DEVNULL, timeout=10)
    except (OSError, subprocess.SubprocessError):
        return ""
    desc = out.decode("utf-8", "replace").strip()
    if desc and not desc.startswith("v"):
        desc = "dev-" + desc
    return desc


def resolve_version(project_dir):
    version = os.environ.get("FW_VERSION", "").strip()
    if not version and os.environ.get("GITHUB_REF_TYPE") == "tag":
        version = os.environ.get("GITHUB_REF_NAME", "").strip()
    if not version:
        version = _git_describe(project_dir)
    version = re.sub(r"[^A-Za-z0-9._+-]", "-", version)[:_MAX_LEN]
    return version or "dev"


FW_VERSION = resolve_version(env.subst("$PROJECT_DIR"))
print("[version] FW_VERSION = %s" % FW_VERSION)

_SRC_DIR = os.path.realpath(env.subst("$PROJECT_SRC_DIR")) + os.sep


def _define_version(env, node):
    # Project sources only: the version changes with every commit, and as a
    # global define it would force a rebuild of the whole Arduino core each time.
    if not os.path.realpath(node.srcnode().get_abspath()).startswith(_SRC_DIR):
        return node
    return env.Object(
        node,
        CPPDEFINES=list(env["CPPDEFINES"])
        + [("FW_VERSION", env.StringifyMacro(FW_VERSION))])


env.AddBuildMiddleware(_define_version)
