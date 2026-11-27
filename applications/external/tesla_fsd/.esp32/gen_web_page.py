#!/usr/bin/env python3
# gen_web_page.py — PlatformIO pre-build extra_script, also runs on its own.
#
# The dashboard page (WEB_HTML in .firmware/web_dashboard.cpp) is ~150 KB of
# text, and handle_root() writes it on the loop task, so CAN is not serviced
# while it goes out. This gzips the page at build time into
# $BUILD_DIR/web_page/web_page_gz.h (WEB_HTML_GZ[], under a third of the
# size); handle_root() sends that copy to every browser that accepts gzip and
# WEB_HTML itself to the rest. Nothing is committed: every build writes it from
# the sources it compiles.
#
# WEB_HTML is read the way the compiler reads it: its string literals joined,
# web_i18n.inc pulled in where it is #included, and #if / #ifdef blocks
# resolved against this env's -D build flags. Anything else between the
# literals (a macro such as FW_VERSION, an #if on a value) stops the build
# here; values the page shows belong in the status JSON. web_dashboard.cpp
# static_asserts that WEB_HTML has the length this script read, and at boot
# checks the gzip trailer's CRC-32 against WEB_HTML; on a mismatch it serves
# WEB_HTML only.
#
# On its own (host checks, browser tests):
#   python3 gen_web_page.py [-D NAME ...] [--html OUT] [--gz OUT] [--header OUT]
import argparse
import gzip
import os
import re
import shlex
import sys

PAGE_FILE = "web_dashboard.cpp"  # in the firmware source dir, .firmware/
START = "static const char WEB_HTML[] PROGMEM ="

_ESCAPES = {"n": "\n", "t": "\t", "r": "\r", "\\": "\\", '"': '"', "'": "'", "?": "?"}
_RAW_OPEN = re.compile(r'R"([^()\\\s]{0,16})\(')
_TOKEN = re.compile(r"\w+|.")


class PageError(Exception):
    pass


def _where(path, text, pos):
    return "%s:%d" % (os.path.relpath(path), text.count("\n", 0, pos) + 1)


def _plain_literal(path, text, pos):
    """The "..." literal at pos: (value, end)."""
    out = []
    i = pos + 1
    while i < len(text) and text[i] != "\n":
        c = text[i]
        if c == '"':
            return "".join(out), i + 1
        if c != "\\":
            out.append(c)
            i += 1
            continue
        e = text[i + 1 : i + 2]
        m = re.compile(r"x([0-9A-Fa-f]{1,2})|([0-7]{1,3})").match(text, i + 1)
        if e in _ESCAPES:
            out.append(_ESCAPES[e])
            i += 2
        elif m:
            out.append(chr(int(m.group(1), 16) if m.group(1) else int(m.group(2), 8)))
            i = m.end()
        else:
            raise PageError("%s: unsupported escape \\%s" % (_where(path, text, i), e))
    raise PageError("%s: unterminated string literal" % _where(path, text, pos))


def _condition(path, text, pos, expr, defines):
    """Value of an #if / #elif made of defined(NAME), !, &&, || and parentheses."""
    toks = re.findall(
        r"defined\s*\(\s*\w+\s*\)|defined\s+\w+|&&|\|\||!|\(|\)|\S+", expr
    )
    at = [0]

    def fail():
        raise PageError(
            "%s: #if %s: only defined(NAME), !, && and || are read here"
            % (_where(path, text, pos), expr.strip())
        )

    def peek():
        return toks[at[0]] if at[0] < len(toks) else None

    def take():
        at[0] += 1
        return toks[at[0] - 1]

    def unary():
        t = take() if peek() else fail()
        if t == "!":
            return not unary()
        if t == "(":
            v = either()
            if take() != ")":
                fail()
            return v
        m = re.fullmatch(r"defined\s*\(\s*(\w+)\s*\)|defined\s+(\w+)", t)
        if not m:
            fail()
        return (m.group(1) or m.group(2)) in defines

    def both():
        v = unary()
        while peek() == "&&":
            take()
            v = unary() and v
        return v

    def either():
        v = both()
        while peek() == "||":
            take()
            v = both() or v
        return v

    v = either()
    if peek() is not None:
        fail()
    return v


def _directive_here(text, pos):
    """True when the '#' at pos starts a preprocessor line."""
    return text[text.rfind("\n", 0, pos) + 1 : pos].strip() == ""


def _literals(path, text, pos, defines, out, page_end):
    """Append the active literals from pos to out. page_end: stop at the ';'
    that ends the definition (else read to the end of the file). Returns the
    position it stopped at."""
    stack = []  # per open #if: [enclosing block active, a branch taken]
    active = True
    while pos < len(text):
        c = text[pos]
        if c in " \t\r\n":
            pos += 1
        elif text.startswith("//", pos):
            e = text.find("\n", pos)
            pos = len(text) if e < 0 else e
        elif text.startswith("/*", pos):
            e = text.find("*/", pos)
            if e < 0:
                raise PageError("%s: unterminated comment" % _where(path, text, pos))
            pos = e + 2
        elif c == "#" and _directive_here(text, pos):
            e = text.find("\n", pos)
            e = len(text) if e < 0 else e
            m = re.match(r"\s*(\w*)\s*(.*)", text[pos + 1 : e])
            word, arg = m.group(1), m.group(2).strip()
            if word in ("if", "ifdef", "ifndef"):
                if word == "if":
                    taken = active and _condition(path, text, pos, arg, defines)
                else:
                    taken = active and (
                        (arg.split()[0] in defines) == (word == "ifdef")
                    )
                stack.append([active, taken])
                active = taken
            elif word in ("elif", "else", "endif"):
                if not stack:
                    raise PageError(
                        "%s: #%s without #if" % (_where(path, text, pos), word)
                    )
                if word == "endif":
                    active = stack.pop()[0]
                elif stack[-1][1]:
                    active = False
                else:
                    active = stack[-1][0] and (
                        word == "else" or _condition(path, text, pos, arg, defines)
                    )
                    stack[-1][1] = active
            elif word == "include":
                inc = re.fullmatch(r'"([^"]+)"', arg)
                if not inc:
                    raise PageError("%s: #include %s" % (_where(path, text, pos), arg))
                if active:
                    inc_path = os.path.join(os.path.dirname(path), inc.group(1))
                    with open(inc_path, encoding="utf-8") as f:
                        _literals(inc_path, f.read(), 0, defines, out, False)
            else:
                raise PageError(
                    "%s: #%s inside the page" % (_where(path, text, pos), word)
                )
            pos = e
        elif text.startswith('R"', pos):
            m = _RAW_OPEN.match(text, pos)
            if not m:
                raise PageError("%s: bad raw string literal" % _where(path, text, pos))
            close = ")" + m.group(1) + '"'
            e = text.find(close, m.end())
            if e < 0:
                raise PageError(
                    "%s: unterminated raw string literal" % _where(path, text, pos)
                )
            if active:
                out.append(text[m.end() : e])
            pos = e + len(close)
        elif c == '"':
            value, pos = _plain_literal(path, text, pos)
            if active:
                out.append(value)
        elif c == ";" and page_end:
            break
        else:
            raise PageError(
                "%s: '%s' between the page's string literals; put the value "
                "in the status JSON instead"
                % (_where(path, text, pos), _TOKEN.match(text, pos).group(0))
            )
    else:
        if page_end:
            raise PageError("%s: WEB_HTML has no end" % _where(path, text, pos))
    if stack:
        raise PageError("%s: #if left open" % _where(path, text, pos))
    return pos


def _read_page(src_dir, defines):
    """(page bytes, the WEB_HTML definition as written, up to its ';')."""
    src = os.path.join(src_dir, PAGE_FILE)
    with open(src, encoding="utf-8") as f:
        text = f.read()
    start = text.find(START)
    if start < 0 or text.find(START, start + 1) >= 0:
        raise PageError("%s: expected one '%s'" % (os.path.relpath(src), START))
    out = []
    end = _literals(src, text, start + len(START), set(defines), out, True)
    return "".join(out).encode("utf-8"), text[start : end + 1]


def page_bytes(src_dir, defines):
    """WEB_HTML as the compiler builds it with these macros defined (UTF-8)."""
    return _read_page(src_dir, defines)[0]


def page_definition(src_dir):
    """The WEB_HTML definition from web_dashboard.cpp, for compiling on its own."""
    return _read_page(src_dir, ())[1]


def gzip_bytes(raw):
    # mtime=0: the same page gives the same bytes, build after build.
    return gzip.compress(raw, compresslevel=9, mtime=0)


def header_text(raw, gz, label):
    rows = ",\n".join(
        "    " + ",".join("0x%02x" % b for b in gz[i : i + 16])
        for i in range(0, len(gz), 16)
    )
    return (
        "// web_page_gz.h — GENERATED by esp32/gen_web_page.py (%s). DO NOT EDIT.\n"
        "// WEB_HTML (web_dashboard.cpp), gzip-compressed: %u -> %u bytes.\n"
        "#pragma once\n"
        "#include <stdint.h>\n"
        "\n"
        "// strlen(WEB_HTML) this copy was made from; web_dashboard.cpp checks it.\n"
        "#define WEB_HTML_GZ_SRC_LEN %uu\n"
        "\n"
        "static const uint8_t WEB_HTML_GZ[] PROGMEM = {\n%s\n};\n"
        % (label, len(raw), len(gz), len(raw), rows)
    )


def write_if_changed(path, data):
    """Write bytes or text; an unchanged file keeps its timestamp."""
    if isinstance(data, str):
        data = data.encode("utf-8")
    try:
        with open(path, "rb") as f:
            if f.read() == data:
                return
    except OSError:
        pass
    if os.path.dirname(path):
        os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)


def flag_defines(flags):
    """Macro names a list of compiler flags defines (-DNAME, -D NAME, -DNAME=1)."""
    names = set()
    args = shlex.split(" ".join(flags))
    for i, a in enumerate(args):
        if a == "-D" and i + 1 < len(args):
            names.add(args[i + 1].split("=", 1)[0])
        elif a.startswith("-D") and len(a) > 2:
            names.add(a[2:].split("=", 1)[0])
    return names


def _pio(env):
    flags = env.get("BUILD_FLAGS", [])
    if isinstance(flags, str):
        flags = [flags]
    try:
        raw = page_bytes(
            env.subst("$PROJECT_SRC_DIR"), flag_defines(env.subst(f) for f in flags)
        )
    except PageError as e:
        sys.stderr.write("[web_page] %s\n" % e)
        env.Exit(1)
    gz = gzip_bytes(raw)
    out_dir = os.path.join(env.subst("$BUILD_DIR"), "web_page")
    write_if_changed(
        os.path.join(out_dir, "web_page_gz.h"),
        header_text(raw, gz, env.subst("$PIOENV")),
    )
    env.Append(CPPPATH=[out_dir])
    print("[web_page] %u bytes, gzip %u bytes" % (len(raw), len(gz)))


def main(argv):
    ap = argparse.ArgumentParser(
        description="Build the dashboard page from web_dashboard.cpp."
    )
    ap.add_argument(
        "-D",
        dest="defines",
        action="append",
        default=[],
        metavar="NAME",
        help="macro the page's #if blocks see as defined (repeatable)",
    )
    ap.add_argument("--html", help="write the page here")
    ap.add_argument("--gz", help="write the gzip copy here")
    ap.add_argument("--header", help="write web_page_gz.h here")
    a = ap.parse_args(argv)
    try:
        src_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".firmware")
        raw = page_bytes(src_dir, [d.split("=", 1)[0] for d in a.defines])
    except PageError as e:
        sys.stderr.write("%s\n" % e)
        return 1
    gz = gzip_bytes(raw)
    if a.html:
        write_if_changed(a.html, raw)
    if a.gz:
        write_if_changed(a.gz, gz)
    if a.header:
        write_if_changed(a.header, header_text(raw, gz, "host"))
    print("page %u bytes, gzip %u bytes" % (len(raw), len(gz)))
    return 0


try:
    Import("env")  # noqa: F821  (only defined when PlatformIO runs this)
except NameError:
    if __name__ == "__main__":
        sys.exit(main(sys.argv[1:]))
else:
    _pio(env)  # noqa: F821
