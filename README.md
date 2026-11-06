# batrider360

**This project was AI-assisted by Claude Anthropic (Opus 5.5)**

**Every feature and test was done on a real Xbox 360 (RGH / JTAG).**

**IF YOU'RE UNSURE HOW TO NAVIGATE THE MENUS:**

**Press START on the opening screens to reach the title menu. Choose PLAY and press A to start a run.
Press BACK during a run to open the pause menu. If the CONTINUE? screen appears, press START before the count
reaches zero to carry on.**

**Version 0.22** (November 2026)

Armed Police Batrider (Raizing / 8ing, 1998) running natively on the Xbox 360: the arcade board emulated in an xsdk
`.xex`, with **CHASE!** (the scoring mode from Coin-Op Game Studio's N64 version and its front end), the arcade's own
ORIGINAL and ADVANCE SPECIAL modes, a TRAINING mode, save states, replays, GADGETS info panels beside the game, and an
instruction manual in English and Japanese.

**No ROM data is included.** You need MAME's `batrider.zip` (merged set) from your own board. See [Setup](#setup).

## The game

**Armed Police Batrider** (1998) is a vertical shooter by RAIZING / 8ing. Your team of riders flies through the
stages, shooting enemies and collecting medals, power-ups and options. Each stage ends with a boss.

**CHASE!** is a new mode for the same game, made by Coin-Op Game Studio. It adds a chain to build, a medal magnet, a
Chase Break that trades your multiplier for a screen full of medals, and a difficulty (rank) that never goes above 75%.

## Features

### Modes
- **CHASE!**: the chain and multiplier (x1 to x8), the medal magnet, the Chase Break, and unlimited continues. Always on
  HARD with three lives. This is the default MODE.
- **ORIGINAL**: the arcade game as it plays in the arcade, with its DIP switches, courses and selection screens.
- **ADVANCE SPECIAL**: the arcade's ADVANCE course straight away, with the nine secret bosses unlocked from the start.
- **TRAINING**: practise any stage or boss with your own settings (see below).

### Picture and GADGETS
- **GADGETS** side panels: status, weapon, rank bar, medals, stage progress, controller and CHASE! panels, in a 16:9
  layout based on Baffo's Batrider MAME artwork.
- **ROTATION**: OFF (the game in the middle, gadgets on both sides), or LEFT / RIGHT for a display turned on its side.
- Picture size, aspect ratio and scanline strength are set in **OPTIONS > GENERAL SCREEN**.

### Menus
- **Title menu**: PLAY, TRAINING, LOAD STATE, REPLAY DATA, OPTIONS, EXIT.
- **Pause menu** (BACK during a run): save and load states, the controls help page, the manual, and restarting the stage.
- **OPTIONS**: GENERAL SCREEN, GADGETS, CONTROLS, SOUND, DEBUG MENU and MANUAL.
- **CONTROLS**: every button can be remapped, and the AUTO SHOT / AUTOFIRE rate is set here.
- **Menu sounds** are made from the game's own sound effects the first time you start the game.

### TRAINING
- Team, player and course choices, including custom riders and every ship and type.
- Lives, bombs, shot and option power, a HITBOX viewer (OFF, PLAYER or ALL), and the arcade's secret command rows.
- The training pause has no save or load state.

### Save states and replays
- **Save states**: 10 slots per game, each with a picture. Written beside the `.xex`.
- **Replays**: a finished run can be saved after its name entry, then played from REPLAY DATA. LB / RB change the speed
  and START pauses. Up to 30 replays can be kept.
- Loading a save state, continuing, restarting a stage, or using TRAINING makes the score invalid (shown in red with
  a `*`). The replay is still offered, marked the same way.

### Manual
- The full instruction booklet is in the release as PDFs (English and Japanese), and on screen from
  OPTIONS > MANUAL, the title menu or the pause menu.

## Controls

| Button | What it does |
| --- | --- |
| Left stick / D-pad | Move your ship. In menus: move the cursor. |
| **A** | SHOT (hold to fire). In menus: select. |
| **B** | BOMB. In menus: go back. |
| **X** | SPECIAL: changes the option formation of the Battle Garegga guest riders. In CHASE!: the Chase Break. |
| **RT** | AUTO SHOT (fires while held). |
| **BACK** | Pause menu during a run. During a replay: pause. |
| **START** | On the opening screens: go to the title. During play: the arcade's own START button (its secret commands). |
| **LB / RB** | During a replay: slower / faster. In the manual: previous / next page. |

Any of these can be changed in **OPTIONS > CONTROLS**. The menus always use A, B, X, START and BACK.

## Playing CHASE!

- **Chain**: every kill adds to the chain. Every 16 kills raise the multiplier by one step, up to x8.
- **Chain timer**: each kill refills it to 1.5 seconds in the upper half of the screen, or 1 second in the lower half.
  When it runs out you lose two multiplier steps above x1, or the chain breaks at x1.
- **Losing a life** halves the multiplier and the medal value, and pauses the timer for three seconds.
- **Medals** near your ship are pulled in. Hold SHOT to pull them all. Losing a medal worth 1,000 or more breaks the chain.
- **Chase Break**: at x4 or higher, press SPECIAL. Bullets and small enemies turn into medals, and the multiplier is
  locked at x1 for five seconds.
- **Extends**: every 1,000,000 points brings back a lost rider, up to five lives.
- **Rank** starts at 50% and stays between 50% and 75%.
- **Continues** are unlimited (free play). Continuing resets the chain and the rank, and makes the score invalid.

## Scores

- CHASE! has its own high score tables. ORIGINAL and ADVANCE SPECIAL share another, so the two never mix.
- A score is valid only when the run had no continue, no save or load state, no TRAINING, no INVULNERABLE, no RESTART
  STAGE past the first, and no QUIT. Invalid scores are shown in red with a `*` and do not enter the ranking.

## Setup

What you need:
- An Xbox 360 that runs unsigned `.xex` files (RGH / JTAG, started from a dashboard such as Aurora).
- MAME's `batrider.zip` (the merged set, which includes `batriderj`), from your own board. It is **not** included.

Install:
1. Copy `batrider360.xex`, `batrider360_manual.bin` and `batrider.zip` into the same folder, on the console's HDD or a
   FAT32 USB drive, e.g. `Hdd1:\Games\batrider360\`.
2. Start `batrider360.xex` from your dashboard.
3. The first start shows NOW LOADING while the ROM set is read, and creates the menu sounds file beside the `.xex`.
   If the screen stays dark red, the ROM set was not found.

Settings, rankings, save states, replays and the log are written beside the `.xex`.

## MAME (PC)

The `mame` folder in the release plays both games in MAME with the same GADGETS.
- `artwork/batriderj/`: the GADGETS for MAME, one layout for both games.
- `batrider_chase.bat` and `batrider_original.bat`: the launchers.
- `chase_xdelta/`: CHASE! as four xdelta patches. No ROM data is included. Copy your `batrider.zip` next to
  `make_batriderj.bat` and run it (it needs `xdelta3.exe`), then move the resulting `out\batriderj.zip` into `mame\roms\`.

See `mame\README_MAME.txt` in the release for the details.

## Building

- The `.xex`: `xsdk/build.sh` (xsdk toolchain image `xsdk-base:1` with the `xsdk-gcc` volume; the command is at the top
  of the script).
- The PC harness (headless board, scripted inputs, frame and RAM dumps): `pc/build.sh` in the `br-tools:1` image
  (`docker/Dockerfile`).
- Generated headers: `build/gen/chase_patch.h` (CHASE!'s 68000 patch), `front_assets.h` (`tools/mkfront.py`),
  `gadgets_art.h` (`tools/mkgadgets.py`); Musashi's opcode tables are made by `build.sh`.
- The manual: `tools/manual/build.py` (PDFs), then `tools/mkmanual.py` (`batrider360_manual.bin`).
- The release folder and zip: `sh tools/mkrelease.sh <version>`.

## Third-party code

Musashi (68000), superzazu's z80, ymfm (YM2151) and zlib, in `third_party/` with their licences.

## Credits

Armed Police Batrider (c) 1998 RAIZING / 8ing. CHASE! and the N64 front end: Coin-Op Game Studio's Batrider CHASE! kit.
GADGETS: Baffo's Batrider MAME layout v0.4 (design baf, Lua Olifante & DUBS, rank start bar Miquel).

This is a fan project, non-commercial. No ROM data is included.
