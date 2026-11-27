#!/usr/bin/env python3
"""test_web_page.py — host test for esp32/gen_web_page.py: for the -D flags of
every env in esp32/platformio.ini, the page it reads out of web_dashboard.cpp is
byte for byte the WEB_HTML the C++ compiler builds, and its gzip copy inflates
back to that page with the CRC-32 the firmware checks at boot.

Build + run:  make -C test check   (uses $CXX, default c++)
"""
import configparser
import gzip
import os
import subprocess
import sys
import tempfile
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ESP32 = os.path.join(ROOT, "esp32")
FIRMWARE = os.path.join(ESP32, ".firmware")
sys.path.insert(0, ESP32)
sys.dont_write_bytecode = True   # no __pycache__ in esp32/
import gen_web_page  # noqa: E402

g_pass = 0
g_fail = 0


def check(cond, msg):
    global g_pass, g_fail
    if cond:
        g_pass += 1
    else:
        g_fail += 1
        print("  FAIL %s" % msg)


def env_defines():
    """{env name: macro names its build_flags define}."""
    ini = configparser.ConfigParser(inline_comment_prefixes=(";",), interpolation=None)
    ini.read(os.path.join(ESP32, "platformio.ini"))
    out = {}
    for sec in ini.sections():
        if sec.startswith("env:"):
            flags = ini[sec].get("build_flags", "").split("\n")
            out[sec[4:]] = frozenset(gen_web_page.flag_defines(flags))
    return out


def compiled_page(work, defines):
    """WEB_HTML as the host compiler builds it."""
    src = os.path.join(work, "page.cpp")
    exe = os.path.join(work, "page")
    with open(src, "w", encoding="utf-8") as f:
        f.write("#include <cstdio>\n#define PROGMEM\n")
        f.write(gen_web_page.page_definition(FIRMWARE))
        f.write("\nint main() { fwrite(WEB_HTML, 1, sizeof(WEB_HTML) - 1, stdout); }\n")
    cxx = os.environ.get("CXX", "c++")
    cmd = [cxx, "-std=c++17", "-I", FIRMWARE, src, "-o", exe]
    cmd += ["-D" + d for d in sorted(defines)]
    subprocess.run(cmd, check=True)
    return subprocess.run([exe], check=True, stdout=subprocess.PIPE).stdout


def main():
    print("test_web_page: gzip page vs the compiled WEB_HTML")
    envs = env_defines()
    check(len(envs) > 0, "no envs in platformio.ini")
    with tempfile.TemporaryDirectory() as work:
        for defines in sorted(set(envs.values()), key=sorted):
            names = ", ".join(e for e, d in sorted(envs.items()) if d == defines)
            raw = gen_web_page.page_bytes(FIRMWARE, defines)
            check(raw == compiled_page(work, defines),
                  "%s: page differs from the compiled WEB_HTML" % names)
            gz = gen_web_page.gzip_bytes(raw)
            check(gzip.decompress(gz) == raw, "%s: gzip does not inflate to the page" % names)
            check(int.from_bytes(gz[-8:-4], "little") == zlib.crc32(raw),
                  "%s: gzip trailer CRC-32" % names)
            check(gen_web_page.gzip_bytes(raw) == gz, "%s: gzip not reproducible" % names)

    # What the build refuses: a macro between the literals, an #if on a value.
    with tempfile.TemporaryDirectory() as work:
        bad = {
            "macro": 'R"x(a)x" FW_VERSION R"x(b)x"',
            "#if value": '#if LED_COUNT > 1\nR"x(a)x"\n#endif\n',
            "#if open": '#if defined(A)\nR"x(a)x"\n',
        }
        for what, body in bad.items():
            with open(os.path.join(work, gen_web_page.PAGE_FILE), "w") as f:
                f.write("%s\n%s;\n" % (gen_web_page.START, body))
            try:
                gen_web_page.page_bytes(work, ())
                check(False, "%s: accepted" % what)
            except gen_web_page.PageError:
                check(True, what)
        with open(os.path.join(work, gen_web_page.PAGE_FILE), "w") as f:
            f.write('%s\n"a\\x41\\n"\n#if defined(A) && !defined(B)\nR"x(1)x"\n'
                    '#elif defined(B)\nR"x(2)x"\n#else\nR"x(3)x"\n#endif\n;\n'
                    % gen_web_page.START)
        check(gen_web_page.page_bytes(work, {"A"}) == b"aA\n1", "#if branch")
        check(gen_web_page.page_bytes(work, {"A", "B"}) == b"aA\n2", "#elif branch")
        check(gen_web_page.page_bytes(work, ()) == b"aA\n3", "#else branch")

    print("\n%d passed, %d failed" % (g_pass, g_fail))
    return 0 if g_fail == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
