ARMED POLICE BATRIDER + CHASE! for the Xbox 360 — batrider360 version @VERSION@
==========================================================================

The arcade game Armed Police Batrider (Raizing / 8ing, 1998), its board emulated on the Xbox 360, together with
CHASE! — the scoring mode and the whole front end of the Coin-Op Game Studio N64 version — and the GADGETS side
panels after Baffo's Batrider MAME layout (v0.4).

The full instructions are in the manuals beside this file:
  batrider360_manual_en.pdf   (English)
  batrider360_manual_ja.pdf   (Japanese)
and on the screen: OPTIONS > MANUAL, from the title's menu or the pause menu (it reads batrider360_manual.bin, which
must stay beside the .xex).


WHAT YOU NEED
-------------
- An Xbox 360 that runs unsigned .xex files (RGH / JTAG, started from a dashboard such as Aurora).
- MAME's batrider.zip (the merged set, MAME 0.2xx), from your own board. It is NOT included and never will be.


INSTALL
-------
1. Copy this folder to the console's HDD or a USB stick (FAT32), e.g. Hdd1:\Games\batrider360\.
2. Put batrider.zip in the same folder as batrider360.xex (batriderj.zip, or roms\batrider.zip, also work).
3. Start batrider360.xex from your dashboard.

Settings, rankings, save states, replays and the log (batrider360_*.txt / .bin, batrider360.log) are written beside
the .xex.


QUICK CONTROLS (the manual has the rest)
----------------------------------------
  A  SHOT        B  BOMB        X  SPECIAL (formation; in CHASE! also the Chase Break)
  RT             AUTO SHOT      (OPTIONS > CONTROLS changes which button does what)
  BACK           the pause menu (START stays the game's own: its START commands work in play)
  Title menu     PLAY, TRAINING, LOAD STATE, REPLAY DATA, OPTIONS, EXIT (back to the dashboard)
  Replays        after a credit: SAVE REPLAY? -> REPLAY DATA (LB / RB speed, START pause)


CREDITS
-------
Armed Police Batrider (c) Raizing / 8ing 1998. CHASE! and the N64 front end: Coin-Op Game Studio's
Batrider CHASE! kit. GADGETS: Baffo's Batrider MAME layout v0.4 (design baf, Lua Olifante & DUBS, rank start
bar Miquel), ported to the 360 with new CHASE! panels.
Built with xsdk; the board uses Musashi (68000), superzazu's z80, ymfm (YM2151) and zlib — their licences are in
the licences folder.

This is a fan project. It contains no ROM data.
