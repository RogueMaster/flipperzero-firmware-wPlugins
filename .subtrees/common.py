import subprocess
import pathlib
import time
import sys

REPO_ROOT = pathlib.Path(__file__).parent.parent


def git(*args, pipe=False, tee=False):
    if tee:
        result = b""
        line = b""
        with subprocess.Popen(
            ["git", *args],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            bufsize=0,
        ) as proc:
            cr = False
            while True:
                char = proc.stdout.read(1)
                if not char:
                    result += line
                    break
                sys.stdout.write(char.decode())
                # Simulate terminal line buffering, where \r resets to start of line overwriting what is already there
                # unless \n follows it, which is just going to next line keeping the content on previous one
                # Useful for terminal progress bars/numbers, no need to log all this data as separate lines
                if char == b"\r":
                    cr = True
                    continue
                elif cr:
                    cr = False
                    if char != b"\n":
                        line = b""
                line += char
                if char == b"\n":
                    result += line
                    line = b""
        return result.decode().removesuffix("\n"), proc.returncode

    elif pipe:
        return subprocess.check_output(["git", *args], text=True).removesuffix("\n")

    else:
        return subprocess.check_call(["git", *args])


def is_workdir_clean():
    try:
        git("diff", "--quiet")
        git("diff", "--cached", "--quiet")
        git("merge", "HEAD", pipe=True)
        return True
    except subprocess.CalledProcessError:
        return False


def send_alert(title, message):
    try:
        subprocess.run(
            ["notify-send", "-a", "Git", "-i", "git", title, message],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
    except Exception:
        pass


def check_merge_result(path, repo, result, status):
    if "Automatic merge failed; fix conflicts and then commit the result." in result:
        print(f"MERGE_MSG: Merge {path} from {repo}")
        send_alert("Subtree merge failed", "Resolve current index to continue")
        while True:
            input("Resolve current index then press Enter...")
            if is_workdir_clean():
                break
            time.sleep(1)

    elif (
        "Please commit your changes or stash them before you switch branches." in result
    ):
        sys.exit(1)

    elif "fatal: " in result:
        sys.exit(1)

    elif status != 0:
        print(f"Git returned status: {status}")
        sys.exit(1)


def check_workdir_state():
    if git("rev-parse", "--show-prefix", pipe=True) != "":
        print("Must be in root of git repo!")
        sys.exit(1)

    if git("branch", "--show-current", pipe=True) == "":
        print("Must be on a branch!")
        sys.exit(1)

    if not is_workdir_clean():
        print("Workdir must be clean!")
        sys.exit(1)


def subdir_split_helper(path, repo, branch, subdir, action, cached=None):
    check_workdir_state()

    prevbranch = git("branch", "--show-current", pipe=True)
    temp = "/".join(repo.split("/")[-2:] + [branch]).replace("/", "-")
    fetch = f"_fetch-{temp}"
    split = f"_split-{temp}-{subdir.replace('/', '-')}"
    git("fetch", "--no-tags", repo, f"{branch}:{fetch}")

    current = git("rev-parse", fetch, pipe=True)
    skip = False
    if cached:
        try:
            git("diff", "--quiet", cached, current, "--", subdir)
            skip = True
        except subprocess.CalledProcessError:
            pass
    if skip:
        print("No updates, skipping expensive subtree split.")
        return

    ok = True
    git("checkout", fetch)
    result, status = git("subtree", "split", "-P", subdir, "-b", split, tee=True)
    if "is not an ancestor of commit" in result:
        print("Resetting split branch...")
        git("branch", "-D", split)
        result, status = git("subtree", "split", "-P", subdir, "-b", split, tee=True)
    if "fatal: " in result:
        ok = False
    git("checkout", prevbranch)
    if not ok:
        print(f"Something went wrong with {path}/.gitsubtree, check above")
        send_alert("Subtree merge failed", "Something went wrong, check logs")
        sys.exit(1)
    else:
        prevhead = git("rev-parse", "HEAD", pipe=True)
        result, status = git(
            "subtree",
            action,
            "-P",
            path,
            split,
            "-m",
            f"{action.title()} {path} from {repo}",
            tee=True,
        )
        check_merge_result(path, repo, result, status)
        if git("rev-parse", "HEAD", pipe=True) != prevhead:
            # Only return new upstream hash if subtree was updated
            # Not if merge was aborted or nothing to update
            return current
