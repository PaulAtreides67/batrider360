BATRIDER CHASE! AND BATRIDER ON MAME (with the batrider360 GADGETS)
===================================================================

This folder lets you play both games on a PC with MAME, with the same GADGETS side panels as the Xbox 360 version.

  roms\batriderj.zip     the CHASE! program (four patched program ROMs; everything else comes from your own
                         batrider.zip, which is not included)
  artwork\batriderj\     the GADGETS for MAME: one layout for both games. In the original game the CHASE! panels
                         stay empty, as on the 360.
  batrider_chase.bat     starts CHASE!
  batrider_original.bat  starts the original arcade game (your own batriderj, untouched)

SETUP
1. You need MAME (0.289 or newer was used for testing) and your own MAME ROM set batrider.zip (merged set).
2. Open both .bat files in a text editor and set MAME= to your MAME folder and ROMS= to the folder that holds
   batrider.zip. Keep this folder's roms\ and artwork\ next to the .bat files.
3. Double-click batrider_chase.bat or batrider_original.bat.

NOTES
- The GADGETS need MAME's layout plugin; the .bat files turn it on (-plugin layout). If you start MAME another way,
  turn it on in MAME's Plugins menu, and choose the "GADGETS" view in Video Options.
- For CHASE!, MAME warns that the program ROM checksums are wrong. That is expected: the program is patched.
- Do not put roms\batriderj.zip into your main ROM folder: it would replace the original game.
- The artwork is batrider360's 16:9 layout (based on Baffo's Batrider MAME layout v0.4: design baf, Lua Olifante &
  DUBS, rank start bar Miquel), with its STATUS, WEAPON, BGM, STAGES and CHASE! panels.
