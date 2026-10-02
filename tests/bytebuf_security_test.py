#!/usr/bin/env python3
"""Build and exercise the real Flipper byte buffer with sanitizers."""

from pathlib import Path
import os
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
HARNESS = r"""
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "dfc_bytebuf.h"

int main(void) {
    assert(dfc_bytebuf_alloc(DFC_BYTEBUF_MAX + 1u) == NULL);

    DfcByteBuf* buffer = dfc_bytebuf_alloc(DFC_BYTEBUF_MAX);
    assert(buffer != NULL);

    const uint8_t value = 0xA5;
    buffer->size_bytes = SIZE_MAX;
    dfc_bytebuf_append_bytes(buffer, &value, 1);
    assert(buffer->size_bytes == SIZE_MAX);
    dfc_bytebuf_append_byte(buffer, value);
    assert(buffer->size_bytes == SIZE_MAX);

    buffer->size_bytes = DFC_BYTEBUF_MAX;
    dfc_bytebuf_append_bytes(buffer, &value, SIZE_MAX);
    assert(buffer->size_bytes == DFC_BYTEBUF_MAX);
    assert(dfc_bytebuf_get_byte(buffer, DFC_BYTEBUF_MAX) == 0);

    dfc_bytebuf_free(buffer);
    return 0;
}
"""


def main() -> None:
    compiler = os.environ.get("CC", "cc")
    with tempfile.TemporaryDirectory() as temporary:
        temp = Path(temporary)
        (temp / "furi.h").write_text("#include <stdlib.h>\n")
        harness = temp / "bytebuf_harness.c"
        harness.write_text(HARNESS)
        executable = temp / "bytebuf_harness"
        subprocess.run(
            [
                compiler,
                "-std=c11",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-fsanitize=address,undefined",
                "-fno-sanitize-recover=all",
                f"-I{temp}",
                f"-I{ROOT / 'lib/core/port'}",
                str(ROOT / "port/dfc_bytebuf_flipper.c"),
                str(harness),
                "-o",
                str(executable),
            ],
            check=True,
        )
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
