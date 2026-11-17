# batrider360

**This project was AI-assisted by Claude Anthropic (Opus 5.5)**

**Every feature and test was done on a real Xbox 360 (RGH / JTAG).**

**IF YOU'RE UNSURE HOW TO NAVIGATE THE MENUS:**

**Press START on the opening screens to reach the title menu. Choose PLAY and press A to start a run.
Press BACK during a run to open the pause menu. If the CONTINUE? screen appears, press START before the count
reaches zero to carry on.**

**Version 0.29** (November 2026)

Armed Police Batrider (Raizing / 8ing, 1998) running natively on the Xbox 360: the arcade board emulated in an xsdk
`.xex`, with **CHASE!** (the scoring mode from Coin-Op Game Studio's N64 version and its front end), the arcade's own
ORIGINAL and ADVANCE SPECIAL modes, a TRAINING mode, save states, replays, GADGETS info panels beside the game, and an
instruction manual in English and Japanese.

**No ROM data is included.** You need MAME's `batrider.zip` (merged set) from your own board. See [Setup](#setup).

## The game

**2019 AD**

Zenovia, a man-made island located 2 kilometers south of Manhattan Island.

The island was built 15 years ago as a desperate measure by the government who were concerned about the increasing crime rate due to the high population count and concentration of man-made structures on the island. Gigantec Cybertron (GC), a fast-growing company at the time, built a large facility on the island. People who had given up on Manhattan Island moved to the island one after another. This led to the "New New York" boom. However, as time went by, problems over the location and ownership of the island surfaced. Contrary to initial expectations, the crime rate began to increase.

In addition, the expansion of the main body of the island was carried out in parallel with these problems. The island's main body was being enlarged one after another. It was only a matter of time before the island became a slum. A few years later, the island continued to grow in a distorted manner, with the GC building towering in the center of the island. The structures that have continued to proliferate and change into a bizarre appearance. The island has taken on a nightmarish aspect, with the towering GC building at its center, and a structure that has continued to grow and change into an odd shape.

The island is infested with all kinds of crimes. It had become a lawless zone where thugs who have undergone cyborg surgery roam about. And the changes were not limited to the island. The "old" New York City was also experiencing a greater number of violent crimes than ever before.

The government, fed up with the situation, began to take over the police force in exchange for the freedom of criminals captured in secret. The government decided to treat them as an organization outside the police force. Named "Zero Cops," they were given the name of "cops who are not supposed to exist. They are deployed to crack down on violent crimes and illegal activities.

The "Zero Cops" are completely controlled by a tiny "chip" implanted in their temples and when they reach a certain level of contribution, they are free to go. However, if they fail to comply with the orders, they will be subjected to pain or a swift death.

Their mission this time is to invade the island of Zenovia in teams of three. The GC Corporation will take full control of the island and will be held responsible for cybernetic surgery experimentation and testing newly developed weapons on criminals. 

Included in this release is the **CHASE!** romhack, made by Coin-Op Game Studio. It adds a score system based on chaining, medal magnetism, a
Chase Break button that trades your multiplier for a screen full of medals, and some modifications to rank/extends for a fast paced game.

## Features

### Modes
- **ORIGINAL**: the original Version B arcade game.
- The aforementioned **CHASE!**
- **ADVANCE SPECIAL**: the arcade's ADVANCE course straight away, with the nine secret bosses unlocked from the start, independently of which ship was selected.
- **TRAINING**: practise any stage or boss with your own settings (see below).

### Picture and GADGETS
- **M2 style GADGETS** as side panels (16:9 only), layout based on Baffo's Batrider MAME artwork.
- **TATE SUPPORT, FILTERS and SCREEN COLOR PRESETS**
- **LOW LATENCY** (OPTIONS): up to two frames less input lag, at the cost of some screen tearing.

### TRAINING MODE
- Team, player and course choices, including custom riders and every ship and type.
- Lives, bombs, shot and option power, a HITBOX viewer (OFF, PLAYER or ALL), and the arcade's secret command rows.

### Save states and replays
- **Save states**: 10 slots per game, with a thumbnail preview. Written beside the `.xex`.
- **Replays**: a finished run can be saved after its name entry, then played from REPLAY DATA. LB / RB change the speed
  and START pauses. Up to 30 replays can be saved.

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



## Setup

What you need:
- An Xbox 360 that runs unsigned `.xex` files (RGH / JTAG, started from a dashboard such as Aurora).
- MAME's `batrider.zip` (the merged set, which includes `batriderj`) **not included in this page**

Use your **unpatched** ROM set. CHASE! is built into `batrider360.xex` and is applied when the game loads. Don't use the
CHASE! xdelta patches for the 360: they are only for MAME on a PC (see [MAME (PC)](#mame-pc)), and a patched ROM fails
the CRC check (dark red screen).

Install:
1. Copy `batrider360.xex`, `batrider360_manual.bin` and `batrider.zip` into the same folder, on the console's HDD or a
   FAT32 USB drive, e.g. `Hdd1:\Games\batrider360\`.
2. Start `batrider360.xex` from your dashboard.
3. The first start shows NOW LOADING while the ROM set is read, and creates the menu sounds file beside the `.xex`.
   If the screen stays dark red, the ROM set was not found or did not pass the CRC check.

Settings, rankings, save states, replays and the log are written beside the `.xex`.

## MAME (PC)

The `mame` folder in the release plays both games in MAME with the same GADGETS.
- `artwork/batriderj/`: the GADGETS for MAME, one layout for both games.
- `batrider_chase.bat` and `batrider_original.bat`: the launchers.
- `chase_xdelta/` (PC only; the 360 doesn't need this): CHASE! as four xdelta patches. No ROM data is included. Copy your `batrider.zip` next to
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
