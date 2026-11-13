# Flipper Zero mechanical reference (not included)

`tools/gen_board.py`, `tools/place.py` and `tools/fit_check.py` read five files from Flipper
Devices' official 3D-model repository. They are not redistributed here; fetch them into
`hardware/ref/flipper-3d-models/` to rerun those tools. Nothing else needs them: the board, the
fabrication files and the other tools work without them.

- Source: <https://github.com/flipperdevices/flipperzero-3d-models>
- Commit: `dff3b17f58a3ec59b8d7104d6c37f450691758a4` (2022-02-16). Production body F7B9C6,
  external module drawing revision B9.
- License: GPL-3.0 (the repository's `LICENSE`).

| File | From | Size | SHA-256 |
|---|---|---|---|
| `external-module-pcb-outline.dxf` | `external-module/pcb-outline/` | 26,680 | `3ba0a7a5842aed14cdd1c537753f8c52c3e6d71c3258c94f4fad0360d4631a24` |
| `external-module-drawing.pdf` | `external-module/` | 732,956 | `9dde06ac39a55f21c6cad19d661b13dfcf4aa834ff0ae3a437dcd4ecdac1c00c` |
| `external-module-pcb-3d-with-headers.stp` | `external-module/pcb-3d/` | 830,034 | `508421a0e8943982aed95e88c3e051ffc756024ba1b420f9d077491981d22767` |
| `flipper-zero-body-3d.stp` | `flipper-zero/body-3d/` | 5,724,132 | `c5df10b1d443742e47e833a9c8a6e360e28a0472c70ca909c2ea47bcb68716f2` |
| `LICENSE` | repository root | 35,132 | `39c16b073710c5e55fc4d3ec9ba6ec6f1c84b49991e054e7ac3396f04b2f38e4` |

What the board takes from them:

- The bottom edge of the PCB outline: 67.34 mm wide, two tabs, a 14.86 mm notch, and the four
  45° corner chamfers.
- The header hole positions: two right-angle 2.54 mm headers, 8 holes and 10 holes (Flipper
  pins 1–8 and 9–18) with 17.78 mm between the groups, Ø0.9 mm, 3.84 mm above the tab edge.
- The recommended PCB thickness, 1.0–1.2 mm.
- The 3D fit check against the Flipper body (`tools/fit_check.py`).
