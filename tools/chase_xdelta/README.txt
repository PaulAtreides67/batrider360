ARMED POLICE BATRIDER CHASE! FOR MAME - xdelta patches
======================================================

The CHASE! hack as four small xdelta patches for MAME's Batrider program ROMs. No ROM data is included: you
need your own MAME ROM set batrider.zip.

EASY WAY (Windows)
1. Download xdelta3.exe (https://github.com/jmacd/xdelta-gpl/releases) and put it in this folder,
   renamed to xdelta3.exe.
2. Copy your batrider.zip into this folder (merged set; for a split set, copy batrider.zip AND batriderj.zip).
3. Double-click make_batriderj.bat. It writes out\batriderj.zip: the four patched program ROMs.
   (Your ROMs elsewhere: powershell -ExecutionPolicy Bypass -File make_batriderj.ps1 -Roms "D:\your\roms")
4. Start MAME's batriderj with out\ first in the ROM path, so the parent batrider.zip supplies the rest:
     mame batriderj -rompath "<this folder>\out;<your rom folder>"
   Do not put out\batriderj.zip into your main ROM folder: it would replace the original game.
   MAME warns that the checksums are wrong: that is expected for a patched program.

BY HAND (any xdelta tool, e.g. xdelta UI or Delta Patcher)
Apply each patch to the stock file and zip the four results as batriderj.zip:

  file        found in batrider.zip      stock CRC32   patch                  patched CRC32
  prg0b.u22   batriderj\prg0b.u22        4f3fc729      prg0b.u22.xdelta       961a21db
  prg1b.u23   prg1b.u23                  8e70b492      prg1b.u23.xdelta       36b28c80
  prg2.u21    prg2.u21                   bdaa5fbf      prg2.u21.xdelta        7f148ab7
  prg3.u24    prg3.u24                   7aa9f941      prg3.u24.xdelta        b7afd48e

xdelta3 checks its result, so a ROM that is not the stock MAME file stops with an error instead of
writing a broken one.

The GADGETS side panels for MAME (both games) are in the batrider360 release's mame folder.

Armed Police Batrider (c) 1998 Raizing / 8ing. CHASE! by Coin-Op Game Studio. Fan-made, non-commercial.
