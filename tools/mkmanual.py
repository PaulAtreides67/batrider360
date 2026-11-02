#!/usr/bin/env python3
"""batrider360 test 8: the manual's PDFs as pages the 360 shows in the game (host menu / pause box / OPTIONS: MANUAL).

    py -3 tools/mkmanual.py [release/manual] [out file]

Reads batrider360_manual_en.pdf and _ja.pdf (tools/manual/build.py makes them) and writes batrider360_manual.bin
(default: release/batrider360_manual.bin), which goes beside the .xex. Every page twice: 488 x 720 for YOKO (two side
by side on the 720p screen) and 720 x 1064 for TATE (one page across the turned TV), each RGB565 big-endian, upright,
zlib-compressed on its own so the 360 reads and unpacks only the pages it shows.

File (big-endian): "BRMN", version 1, then per language (EN, JA): page count; then per language, per page, per size
(YOKO, TATE): offset and length of its zlib block from the file's start; then the blocks.
Needs pymupdf (py -3 -m pip install --user pymupdf) and numpy.
"""
import struct
import sys
import zlib
from pathlib import Path

import numpy as np
import pymupdf

SIZES = ((488, 720), (720, 1064))      # must match xsdk/main.c MAN_* sizes
ZMAX = 2 * 1024 * 1024                 # xsdk/main.c MAN_ZMAX: the 360 refuses the whole file over it (test 8)
ROOT = Path(__file__).resolve().parent.parent


def page_rgb565(page, w, h):
    m = pymupdf.Matrix(w / page.rect.width, h / page.rect.height)
    pix = page.get_pixmap(matrix=m, alpha=False)
    if (pix.width, pix.height) != (w, h):
        raise SystemExit(f"page rendered {pix.width}x{pix.height}, wanted {w}x{h}")
    a = np.frombuffer(pix.samples, np.uint8).reshape(h, w, 3).astype(np.uint16)
    v = ((a[..., 0] >> 3) << 11) | ((a[..., 1] >> 2) << 5) | (a[..., 2] >> 3)
    return v.astype(">u2").tobytes()


def main():
    src = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "release" / "manual"
    out = Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / "release" / "batrider360_manual.bin"
    docs = [pymupdf.open(src / f"batrider360_manual_{lang}.pdf") for lang in ("en", "ja")]
    counts = [len(d) for d in docs]
    head = 8 + 4 * len(docs) + 8 * len(SIZES) * sum(counts)
    blocks, table, at = [], [], head
    for d in docs:
        for page in d:
            for w, h in SIZES:
                z = zlib.compress(page_rgb565(page, w, h), 9)
                if len(z) > ZMAX:
                    raise SystemExit(f"page {page.number + 1} at {w}x{h} packs to {len(z)} bytes, over {ZMAX}")
                table.append((at, len(z)))
                blocks.append(z)
                at += len(z)
    with open(out, "wb") as f:
        f.write(b"BRMN" + struct.pack(">I", 1))
        f.write(struct.pack(f">{len(counts)}I", *counts))
        for off, n in table:
            f.write(struct.pack(">II", off, n))
        for z in blocks:
            f.write(z)
    print(f"{out}: EN {counts[0]} pages, JA {counts[1]} pages, {at} bytes")


if __name__ == "__main__":
    main()
