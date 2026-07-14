import os
import re
from collections import defaultdict

SOURCE_EXT = (".h", ".hpp", ".c", ".cpp")

# ловим: inline / static inline + return type + name(
INLINE_FUNC_RE = re.compile(r"\b(?:static\s+)?inline\s+[\w:<>\s*&]+\s+(\w+)\s*\(")


def read_files(root="."):
    files = []
    for dirpath, _, filenames in os.walk(root):
        for f in filenames:
            if f.endswith(SOURCE_EXT):
                files.append(os.path.join(dirpath, f))
    return files


def load_text(path):
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        return f.read()


def find_inline_functions(files):
    """
    Возвращает dict: name -> (path, line, col)
    col = позиция имени функции в строке (1-based)
    """
    inline_funcs = {}
    for path in files:
        try:
            with open(path, "r", encoding="utf-8", errors="ignore") as f:
                for lineno, line in enumerate(f, 1):
                    m = INLINE_FUNC_RE.search(line)
                    if m:
                        name = m.group(1)
                        col = m.start(1) + 1  # VS Code: 1-based
                        # если одинаковые имена встречаются, оставим первое (обычно достаточно)
                        inline_funcs.setdefault(name, (path, lineno, col))
        except Exception:
            pass
    return inline_funcs


def index_line_starts(text):
    """Вернёт массив индексов начала каждой строки (0-based)"""
    starts = [0]
    for m in re.finditer(r"\n", text):
        starts.append(m.end())
    return starts


def pos_to_line_col(line_starts, pos):
    """
    pos (0-based индекс в тексте) -> (line, col) 1-based
    """
    # бинарный поиск по line_starts
    lo, hi = 0, len(line_starts) - 1
    while lo <= hi:
        mid = (lo + hi) // 2
        if (
            line_starts[mid]
            <= pos
            < (line_starts[mid + 1] if mid + 1 < len(line_starts) else 10**18)
        ):
            line = mid + 1
            col = pos - line_starts[mid] + 1
            return line, col
        if pos < line_starts[mid]:
            hi = mid - 1
        else:
            lo = mid + 1
    return 1, 1


def is_probably_declaration_line(line):
    """
    Грубая эвристика: чтобы не считать "вызовами" строки,
    где функция объявляется/определяется.
    """
    if "inline" in line:
        return True
    # типичный вид определения: name(...) {
    if re.search(r"\b\w+\s*\([^;]*\)\s*\{", line):
        return True
    return False


def find_calls(files, func_names):
    """
    Возвращает dict: func -> list of (path, line, col)
    """
    calls = defaultdict(list)

    # заранее компилируем regex для каждого имени
    call_res = {name: re.compile(rf"\b{re.escape(name)}\s*\(") for name in func_names}

    for path in files:
        try:
            text = load_text(path)
            line_starts = index_line_starts(text)

            # Чтобы не ловить в комментариях — грубо вырежем // и /* */
            # (не идеальный парсер, но снижает шум)
            scrubbed = re.sub(r"//.*", "", text)
            scrubbed = re.sub(r"/\*.*?\*/", "", scrubbed, flags=re.S)

            for name, cre in call_res.items():
                for m in cre.finditer(scrubbed):
                    pos = m.start()
                    line, col = pos_to_line_col(line_starts, pos)

                    # вытащим строку для эвристики (не считать объявление)
                    line_start = line_starts[line - 1]
                    line_end = scrubbed.find("\n", line_start)
                    if line_end == -1:
                        line_end = len(scrubbed)
                    line_text = scrubbed[line_start:line_end]

                    if is_probably_declaration_line(line_text):
                        continue

                    calls[name].append((path, line, col))
        except Exception:
            pass

    return calls


def main():
    files = read_files(".")
    inline_funcs = find_inline_functions(files)
    calls = find_calls(files, inline_funcs.keys())

    # 1) Печатаем определения inline в формате VS Code
    for name, (path, line, col) in sorted(inline_funcs.items()):
        print(f"{path}:{line}:{col}: inline definition: {name}")

    # 2) Печатаем вызовы inline-функций в формате VS Code
    for name in sorted(calls.keys()):
        decl_path, decl_line, decl_col = inline_funcs[name]
        for path, line, col in calls[name]:
            print(
                f"{path}:{line}:{col}: calls inline '{name}' (defined at {decl_path}:{decl_line}:{decl_col})"
            )


if __name__ == "__main__":
    main()
