BATRIDER CHASE! AND BATRIDER ON MAME (with the batrider360 GADGETS)
===================================================================

This folder lets you play both games on a PC with MAME, with the same GADGETS side panels as the Xbox 360 version.

  chase_xdelta\          the CHASE! program as four xdelta patches. No ROM data is included: the kit builds
                         roms\batriderj.zip from your own batrider.zip (see chase_xdelta\README.txt)
  artwork\batriderj\     the GADGETS for MAME: one layout for both games. In the original game the CHASE! panels
                         stay empty, as on the 360.
  batrider_chase.bat     starts CHASE!
  batrider_original.bat  starts the original arcade game (your own batriderj, untouched)

SETUP
1. You need MAME (0.289 or newer was used for testing) and your own MAME ROM set batrider.zip (merged set).
   Build the CHASE! ROM: copy batrider.zip into chase_xdelta\ and double-click make_batriderj.bat (it needs
   xdelta3.exe, see chase_xdelta\README.txt). Move the resulting out\batriderj.zip into this folder's roms\.
2. The .bat files find mame.exe by themselves (D:\mame, C:\mame, their own folder, or the PATH) and use the
   ROM folders from MAME's mame.ini. If yours are elsewhere, set MAME= and ROMS= at the top of the .bat files.
   Keep this folder's roms\ and artwork\ next to the .bat files.
   Note: make_batriderj.bat's "out" folder is inside chase_xdelta\; move out\batriderj.zip to roms\batriderj.zip.
3. Double-click batrider_chase.bat or batrider_original.bat.

NOTES
- The GADGETS need MAME's layout plugin; the .bat files turn it on (-plugin layout). If you start MAME another way,
  turn it on in MAME's Plugins menu, and choose the "GADGETS" view in Video Options.
- For CHASE!, MAME warns that the program ROM checksums are wrong. That is expected: the program is patched.
- Do not put roms\batriderj.zip into your main ROM folder: it would replace the original game.
- The artwork is batrider360's 16:9 layout (based on Baffo's Batrider MAME layout v0.4: design baf, Lua Olifante &
  DUBS, rank start bar Miquel), with its STATUS, WEAPON, BGM, STAGES and CHASE! panels.
