# batrider360

Armed Police Batrider (Raizing / 8ing, 1998) on the Xbox 360: the arcade board emulated in an xsdk `.xex`, with
CHASE! (the N64 version's scoring mode) and the N64 front end, Baffo's GADGETS side panels (MAME layout v0.4, with new
CHASE! panels), save states, replays, a TRAINING mode, and an instruction manual in English and Japanese.

No ROM data is included. The game needs MAME's `batrider.zip` (merged set) from your own board, beside the `.xex`.

## Building

- The `.xex`: `xsdk/build.sh` (xsdk toolchain image `xsdk-base:1` with the `xsdk-gcc` volume; the command is at the
  top of the script).
- The PC harness (headless board, scripted inputs, frame and RAM dumps): `pc/build.sh` in the `br-tools:1` image
  (`docker/Dockerfile`).
- Generated headers: `build/gen/chase_patch.h` (CHASE!'s 68000 patch), `front_assets.h` (`tools/mkfront.py`),
  `gadgets_art.h` (`tools/mkgadgets.py`); Musashi's opcode tables are made by `build.sh`.
- The manual: `tools/manual/build.py` (PDFs), then `tools/mkmanual.py` (`batrider360_manual.bin`, the in-game manual).

## Third-party code

Musashi (68000), superzazu's z80, ymfm (YM2151) and zlib, in `third_party/` with their licences.

## Credits

Armed Police Batrider (c) Raizing / 8ing 1998. CHASE! and the N64 front end: Coin-Op Game Studio's Batrider CHASE!
kit. GADGETS: Baffo's Batrider MAME layout v0.4 (design baf, Lua Olifante & DUBS).
