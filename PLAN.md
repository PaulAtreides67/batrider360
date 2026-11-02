# batrider360 — Armed Police Batrider + CHASE! on the Xbox 360

User request 2026-11-01: "make an xbox360 port of batrider chase next, and port over every element of the N64 front
end as well and also the original arcade game, as one package".

One .xex (xsdk, like ibara360 since test 39) that plays both games on one title screen, the way the N64 version of
Coin-Op Game Studio's CHASE! kit does: MODE = CHASE! or ORIGINAL. For the user's own use only (kit README).

## How it is built

- The board is emulated, not recompiled: the 68000 is cheap enough to interpret on Xenon (16 MHz, ~4 M
  instructions/s) and an interpreter runs the CHASE! 68k patch (batrider-chase project) unchanged.
  - 68000: Musashi (MIT, third_party/musashi @313ebf1), 68000 only, illegal-opcode callback for host hooks.
  - Z80 (sound): superzazu/z80 (MIT, third_party/z80 @d64fe10).
  - YM2151: ymfm (BSD-3, third_party/ymfm @81aec25) — the same core the N64 kit credits.
  - OKIM6295 x2 + Raizing's GAL banking, GP9001 VDP + RAM text layer: own code in core/, written from MAME's
    driver (ref/mame, read as documentation; MAME is allowed as a reference in this project, unlike ibara360).
- ROMs: the user's batrider.zip (MAME merged set) beside the .xex. ORIGINAL = batriderj (prg0b.u22 + common).
  CHASE! = the same with the batrider-chase patch applied at load (tools/patch.py's output, CRC-checked).
- PC harness (pc/): the same core headless in Docker: frames to PNG, audio to WAV, scripted pads. Checked
  against MAME 0.289 (D:\mame) frame for frame and RAM for RAM.

## Board facts used (MAME raizing_batrider.cpp)

68000 16 MHz, 1024 cycles a line; Z80 32/6 MHz; screen 6.75 MHz, 432x262 (59.637 Hz), 320x240 visible, ROT270.
IRQ2 = GP9001 at line 230, cleared by scroll register 0x0E/0x0F; IRQ4 = Z80 port 0x44, cleared at 0x500026.
Z80 NMI on the 68k's latch writes (0x500021/23), cleared by port 0x46. Text layer, palette, line select and
line scroll are DMA'd from main RAM 0x200000-0x2033FF on writes to 0x500082 (each vblank); text glyphs from
0x200000-0x207FFF on 0x500080 (once). Object banks 0x5000C1-CF: 8 x 4 bits, code = bank[code>>15]<<15 | low.

## Phases (each ends in a hardware test; the 360 skill's gates apply)

1. Test 1 — the original arcade game boots and plays on the 360: picture (YOKO, game upright and centred), sound,
   pad (coin on BACK), from batrider.zip beside the .xex. PC harness matches MAME first.
2. Test 2 — CHASE! and the title: the patched program as a second ROM image, the N64 title menu (START /
   PRACTICE / OPTIONS), MODE, free play, WARNING + logo splash, memory test skipped.
3. Test 3+ — the rest of the N64 front end, ported from the decompiled N64 code (batrider-chase/work/chase.c,
   f_hud.c, f_frame.c): host menu (RESUME, HITBOX, MUSIC, EFFECTS, LANGUAGE, SCREEN, MODE, RESET), OPTIONS
   (continues, buttons, sound, music/sound test, GAME SETTINGS), pause box (RESUME, RESTART, QUIT,
   INVULNERABLE), PRACTICE (7 stages, 18 bosses, REPLAY / SELECT STAGE / TITLE), rankings per mode kept on the
   HDD/USB, scores-not-kept rules, CHASE! HUD extras (boss timer, boss bar + name, hitbox disc), SCREEN = TATE A /
   TATE B / YOKO.

4. GADGETS (user 2026-11-01, mandatory): Baffo's Batrider MAME layout, version 0.4 (D:\mame\artwork\batriderj.zip
   -> 0.4/batriderj.zip, copy in ref/gadgets_baffo04/0.4: design baf, Lua Olifante & DUBS, rank_start bar Miquel),
   ported as the 360's side panels for YOKO (1920x1080 view: game 810x1080 at x 555) and TATE (its GadgetsTATE
   view): rank bar + rank start bar + %, next 6 drops, 7 drop counters (red at their cap), medal level, boss HP +
   timer (its BOSSES table), shot charge, current player + next two + type, P1/P2 input, 12 secret-boss
   conditions. Plus NEW gadgets for CHASE! in the same look (chain, multiplier, chain timer, Chase Break ready /
   rest, lives + banked extends, next extend), with the layout adapted to fit them in CHASE! mode.
   (The older batriderj.zip on G: is a 2021 4:3 version by trap/olifante/dubs; v0.4 is the one to port.)
   Note: default.lay names boss_time.png, which the zip lacks (unused by the views).
   AGREED YOKO LAYOUT (user 2026-11-01, work/gadgets_mock4.png from tools/gadget_mock.py = the reference):
   - right: Rank Bar + Item Drops at 82%; NEW Item Count row (F2-F7: P, big P, option, bomb, big B, 1UP, icon +
     count, red at cap) in Item Drops' frame; Medal drops (F1) + Medal Lv. side by side; NEW Status (stage, time,
     enemies, bullets — "Most" row dropped by the user) and Weapon (shot LV, options, formation, bombs,
     Sp.Power / Sp.Option checkboxes) in the Secret bosses frame widened.
   - left: Current Player 72% + input 62% beside it; Secret bosses, Boss Info, shot charge at 72%; NEW Stages
     (ibara's STAGES: score per stage + total) in the strip right of them; CHASE! at the bottom: Chain (Rank Bar
     frame: bar = chain timer, box = chain), Multiplier (Medal Lv. frame, x1-x8, amber C, forward ARROW drawn:
     amber up / blue down), Lives (1UP drop frame), Next Extend (rank % box).
   - New titles in Batrider's own text font (glyphs lie sideways in the ROM's orientation; turned upright).
   CHANGED (user 2026-11-01, after test 3): Status + Weapon move to the LEFT; Secret bosses, Boss Info (timer) and
   Stages (stage scores) move to the RIGHT; Rank Bar, Item Drops and Item Count packed tighter together to make room;
   Medal drops (F1) and Medal Lv. united into ONE medal gadget (like ibara360's), on the right.
   - Still open: TATE version; a Music panel (track + composer) for the space right of Lives if wanted;
     RAM for shot LV / options / formation / bombs / stage time to be found.

## Defaults the user asked for

- 2026-11-01: DIP switches for the secret characters and modes ON by default (both modes): batriderj DSWC
  (0x500002 high byte) Guest Players 0x20, Player Select 0x40, Special Course 0x80, Stage Edit 0x04 -> 0xE4.
  (MAME's batriderj PORT_MODIFY: these read 1 = On on the Japanese set.) Verify in the game's own DIP test.
- CHANGED 2026-11-01 (user, test 4): Stage Edit OFF, "let the N64 version take over": DSWC 0xE0 (Guest Players,
  Player Select, Special Course on) and the N64's mask 0xE2E7 on 0x500002 (Stage Edit, Continue Play,
  Invulnerability, Flip forced off; work/n64_frontend_spec.md §0.4).

## Status

- 2026-11-01: plan; cores fetched; MAME driver files in ref/mame.
- 2026-11-01: PC harness (pc/, Docker br-tools:1) runs the board at ~400 fps. Against MAME 0.289 batriderj:
  frames pixel-exact after the boot (7/7 compared, attract to frame 3000), once offset by 39 frames: the boot
  ROM/RAM test runs ~5% faster on Musashi than MAME's 68000 (WARNING at frame 814 vs 853); after that no drift
  (the demo's gameplay keeps the same offset) -> check slowdown against MAME in real play later. Sound: band
  energies within ~15% of MAME's 0-16 kHz (windowed-sinc resampler; YM cut at ~15.6 kHz like MAME's output).
  Fixed on the way: GP9001 port decode ignores the low word bit (32-bit writes), tilemap priority = attrib bits 8-11.
- Test 1 STAGED 2026-11-01 deploy/test1x: batrider360.xex (md5 1d590f97) + the user's batrider.zip. xsdk/main.c,
  CMakeLists.txt, build.sh (volume br-xsdk). Not run on a console yet.
- Test 1 FAILURE 2026-11-01 (log work/test1_hardware.log): game ran; stick drifted up/left (dead zone 12000; ibara360
  uses 16384 since its test 3 for the user's pads); board 12-16.4 ms + 2.2 ms draw on one thread -> slowdown and
  sound underruns (up to 520 frames of 600 over 16 ms in play).
- Test 1b STAGED 2026-11-01 deploy/test1bx (md5 7cdc290a): split board on three hardware threads (0: 68000/Z80/OKI +
  YM status chip; 2: GP9001 picture + texture fill, one frame behind; 4: YM2151 synthesis from the logged writes +
  resampler). PC harness: split = unsplit byte for byte (3000 frames, sound + 4 frames). PC per frame: 68000 285 us,
  Z80 111, picture 783, sound mix 272 -> the CPUs are ~30% of test 1's board time. ymfm patched: skip_clocks
  (timer B's load phase reads the sample count). Dead zone 16384; stick values logged. Per-part times in the log.
  If the main thread still runs over 16 ms: a 68000 recompiler, as ibara360's SH-3 one (user 2026-11-01).
- Test 1b FAILURE 2026-11-01 (log work/test1b_hardware.log, video D:\obs\2026-11-01 10-30-25.mp4): timing now fine
  (board 5-6.3 ms, video thread <= 11 ms, 0 frames over 16 ms, the capture shows no repeated frames in play beyond
  the 59.94/60 Hz capture beat). Drift = a bug, not the dead zone: CLEAR_ARGB = 0xFF000000 in the same anonymous
  enum made the enum unsigned (GCC), so thumb_ly > STICK_DEAD_ZONE / thumb_lx < -STICK_DEAD_ZONE compared
  unsigned and every slight tilt was a direction (log: x -7578 -> RIGHT, y -50 -> UP, y +50 -> DOWN). Sound: 800
  samples a frame at the display's 59.94 Hz left the queue short -> 140 underruns (5 ms holes).
  Video scroll steps (phase correlation, 90-120 s): steady 3 px/frame with a skip+repeat pair ~once a second;
  the PC harness's scroll registers (BR_SCROLLLOG) move evenly -> console presenter or OBS capture, unknown.
- Test 1c STAGED 2026-11-01 deploy/test1cx (md5 8dd928c8): colours out of the enum (+ _Static_assert that it stays
  signed); sound queue held at RING_TARGET by stretching each frame 800 +-4 samples (0.5% max), primed with silence;
  log line "present:" every 600 frames (vblanks, flips, repeated, late flips) to settle the stutter's source.
- Test 1c SUCCESS 2026-11-01 (log work/test1c_hardware.log, ~4 min, several runs): no drift, smooth play; user saw
  one minor stutter when starting a run. Log: frames 4200-4800 (run start ~4724) underruns 2 -> 53, repeated
  flips 1 -> 19, sound queue 2429 -> 1925, yet board max 6.3 ms, video max 10.3 ms, 0 frames over 16 ms; a smaller
  burst at the second run start (~9938: underruns 53 -> 58, repeats 29 -> 34). Otherwise ~1 repeat / 600 frames.
  So a stall outside the timed parts at run start (candidate: log file writes on the main thread, or the
  sound-queue refill vs. a large OKI/YM burst); look into it in test 2.
- Test 2 STAGED 2026-11-01 deploy/test2x (md5 7f256dea): CHASE! + the N64 title. core/progpatch.c makes ORIGINAL
  and CHASE! (build/gen/chase_patch.h from tools/mkpatch.py: 18 runs of batrider-chase's prg_chase.bin, CRC-checked)
  and skips the ROM/RAM check (0x2CA jsr 0x176CE -> NOPs; WARNING then runs board frames 14-319). Free play: DSW
  0x00DC (0x1C alone = NO MUSIC / NO SOUND). "START anywhere -> title" = SP 0x210000, PC 0x1AEA (the attract
  loop, title first; scene 0x20E6AA = 0x28, START takes from +68, background still from +126). Title = the
  arcade's BG + FG layers (its logo is the TOP layer + sprites), the N64 menu / ARRANGED BY in the board's own
  text layer (pixel-exact vs the manual's N64 shot; pink highlight = a spare text palette), the logo + RAIZING line
  from the N64 manual's art (ref/n64_manual_art, tools/mkfront.py fits them on the N64 shot, adds the N64's
  shadow). COIN-OP GAME STUDIO splash after WARNING. Board frozen on the title; 30 s idle -> demo. LB/RB = MODE
  until test 3's host menu; PRACTICE / OPTIONS say COMES IN TEST 3. Credit start = START pulses until 0x20E69D bit
  7 clears (after a credit: 0x20 + scene 0x17 = name entry / ranking, A0 + 0x28 = title again). Pad lines no
  longer logged; the log's "main loop:" line times the loop's longest frame, pad reads, stats log writes.
- Test 2 SUCCESS 2026-11-01 (logs work/test2_hardware_1.log, _2.log): CHASE!, the N64 title, MODE switching and
  both programs play. User: stutter still there, and a ~2 s one in the demo at the end. Cause = the log: every long
  main-loop frame follows a log write to the HDD (frame 16200: 1306 ms of which the stats lines 1290 ms = the demo's
  stall; frame 9000: 861 ms; 486-573 ms after the front's state lines at 2458, 640, 8436); underruns jump by ~187
  per second of stall. Also the title: video thread 18.4 ms (> 16.7) = ~1 repeated flip in 10 and the sound queue
  at 0, so every run start began with an empty queue (+24 underruns) that the +4/frame control took ~10 s to refill.
- Test 3 STAGED 2026-11-01 deploy/test3x (md5 602bd821): -Wl,--wrap=xsdk_log_write -> lines into a 256-line ring,
  written by a log thread on hardware thread 5 (sleeps 10 ms; at exit drained on the main thread); the stats line
  gives the log thread's longest file write. Title blend without divides (div255, exact for all inputs) and the
  standing title drawn once, copied while (copy, menu, alpha) stays. Sound queue under RING_TARGET/4 -> silence back
  to RING_TARGET at once ("sound queue refilled n times").

- Test 3 SUCCESS 2026-11-01 = current .xex (log pasted in the conversation; work/test3_hardware.log is a stub): no
  stutter (0 frames over 16 ms, 6-9 repeated flips in 35400, underruns 2, log thread's file writes up to 255 ms
  now harmless). User: (1) jumbled tiles in the intro's first scene; (2) the opening music cuts off on the title and
  resumes before the demo. (1) reproduced in the PC harness (work/intro3, BR_MODE=chase: a row of stray tiles over
  the A.D.2014 / N.Y. / MANHATTAN scene); the unpatched program has none (work/intro3o) -> the skipped ROM/RAM check
  is what clears that VRAM/RAM; the skip must still do its clears. (2) the board is frozen on the title (board 0 us
  at frames 3600-4200), so the Z80/YM/OKI stop too -> keep the sound side running while the 68000 is held.

- Test 4 IN PROGRESS 2026-11-01 (user: "test 4 as proposed and the other things for batrider 360 all at once"):
  (1) intro tiles FIXED at the source: batrider-chase's ROM gap R3 0x15DCB0-0x15E84C is the opening's FG map (fill
  test: only R3 changes frames 460-850); CHASE!'s R3 code moved into R1 (batrider-chase src/chase.s, chase.ld),
  chase_patch.h regenerated (17 runs), the CHASE! intro = the ORIGINAL one in the harness; the MAME zip rebuilt too.
  (2) title music: the N64 never freezes the title (work/n64_frontend_spec.md §8: 0x20342E/0x203430 refreshed); the
  arcade's own opening tune ends ~2 s into the title by itself. br_hold_main() (68000 held, Z80/YM/OKI on) for the
  host menu. (3) the whole N64 front end -> core/fe.c (PC-harness driven): PRACTICE, pause box, OPTIONS + GAME
  SETTINGS, CONTINUES, rankings per MODE, host menu (LB+RB; SCREEN YOKO/TATE A/TATE B, GADGETS ON/OFF), hitbox,
  LANGUAGE. (4) CHASE! HUD top row + boss bar/name (core/chasehud.c; N64 host code, missing from hud.s). (5) GADGETS
  data + drawing (core/gadgets.c), the user's new layout + TATE; canvas behind the game on hardware thread 3, all
  three screens' quads in main.c. (6) the manual EN + JA (release/manual, tools/manual). Saves: batrider360_<name>
  beside the .xex, written by the log thread. Spec of the N64 front end: work/n64_frontend_spec.md.
  Harness ops added: vram, poke, hold; env BR_UNPATCH, BR_FILL. Docker for helpers: tools/dk.sh (2 slots).

- Test 4 STAGED 2026-11-01 deploy/test4x (md5 abe05191; + batrider.zip + the manual PDFs): main.c drives core/fe.c
  (fe_frame / fe_after_board_frame / fe_video), the gadget thread (HW 3) never blocks the main loop (3 canvases, the
  newest finished one drawn; "gadget frames late" in the log), quads for YOKO / TATE A / TATE B, files
  batrider360_host.txt / _options.txt / _rankings_chase.bin / _rankings_original.bin beside the .xex. board.c:
  br_set_volumes (MUSIC = YM + OKI 1, EFFECTS = OKI 0). STATUS gained a Course row (user). Manual rebuilt with the
  host menu shots (work/hostmenu_360.png, hostmenu_reset_360.png): tools/manual/build.py (needs pymupdf).
  Open: hitbox also in ORIGINAL (N64 code does; manual p.21 says no); after a MUSIC TEST the title stays silent (N64
  does the same); CHASE!'s stage line on text line 36 (N64: 38) so the CHASE tag hides while a boss bar is up;
  formation names not in RAM (Weapon shows TYPE A-D / SPECIAL); stage time counted by the module.
- Test 4 SUCCESS 2026-11-01 (log work/test4_hardware.log). User: slow with GADGETS on, a bit less with them off.
  Log: video thread 16-21 ms a frame (test 2: ~9 ms) = repeated flips. The gadget thread on HW 3 shares core 1 with
  the video thread (HW 2): its idle spin halves the video thread, its 11 ms canvas a frame (memcpy + upload of
  1280x720) adds to it. The "waits for them" stat missed the main loop's first wait for each worker.
- Test 5 (speed only, user "yes"): gadget thread on HW 5 (core 2: sound HW 4 + the sleeping log thread); the sound
  and gadget workers sleep on an auto-reset event between jobs (video still spins, alone on core 1); a gadget job only
  when the canvas would change (GadgetState memcmp, screen shape, CHASE!, on/off, P1's flashing full charge); every
  wait for a worker timed; "unchanged" count beside "gadget frames late". STAGED 2026-11-01 deploy/test5x
  (md5 fedd5b24; + batrider.zip + the manual PDFs).
- Test 5 SUCCESS 2026-11-01 = current .xex (deploy/test5x): the game runs smoothly again (user).
- Test 6 (user "implement everything for test 6") STAGED 2026-11-01 deploy/test6x (md5 0cdc6aa8; + batrider.zip +
  the manual PDFs) = version 0.6, release/batrider360_v0.6(.zip) by tools/mkrelease.sh (no ROM; README from
  tools/release_readme.txt, licences):
  (1) CHASE!'s stage line = the N64's job 1 (0x8028F158) on line 38 (core/chasehud.c draw_stage): the 68000's row 1
  cols 0..7, 'x' + lives, the row's words reflowed to col 24 or EXTEND / MAX LIVES / CASH IN, CHASE at col 25;
  hud.s's line-36 cells blanked, row 1's dirty byte cleared -> the boss bar never hides the tag (matches the
  manual's N64 HUD shot). (2) WEAPON formation = player +0x22 (P1 0x204084), only the Battle Garegga guests
  (riders 9-12): 0 V (default, reset on respawn), 2 BACK, 4 FRONT, 6 CIRCLE, 8 CONTROL, 0x0A-0x12 specials ->
  SPECIAL; others "-". Command table 0x30862, matcher 0x3080C, C steps via 0x2F656 (work/form2). TYPE A-D was
  never a formation (it is the stat balance; still shown in CURRENT PLAYER). (3) the game keeps no stage clock
  (gadgets.c counts it); shot LV / options / bombs RAM were already found in test 4. (4) hitbox stays in ORIGINAL
  (N64 code); manual p.24 fixed. (5) BGM panel: board.c tracks the 68000's 0x20,nn music command (br_music; 0x21 /
  0x20,E0 stop; jingles 0x11/0x12 time out), titles read at run time from snd.u77 0x3E200 (entry nn+1; confirmed
  in play: 01 Zero Cop opening, 02 Choice Or Die, 03 Easy To Go stage 1, 0B boss), composers in gadgets.c (VGMRips
  GD3 split; credits Namiki / Koyano / Sakimoto, guests Motoyama / Iwata). YOKO: a strip under Status/Weapon;
  TATE: under Item Count. Manuals: SPECIAL = Garegga guests' formation, WEAPON + MUSIC rows, new GADGETS shots.

## Release (user 2026-11-01)

- The full manual goes in the release: the N64 booklet's whole content (getting started, controls, CHASE! rules,
  modes and menus, host menu, what is kept, troubleshooting), English and Japanese, rewritten for the 360 (pad,
  YOKO/TATE, GADGETS, saves on HDD/USB), as PDFs beside the .xex. Layout after the kit's manual.

- Test 6 SUCCESS 2026-11-01 = previous .xex (v0.6). User: ORIGINAL gives no tick points for shot hits; the title's
  white flash never comes up "or it freezes". (1) ORIGINAL's DSW 0x00DC (from the N64, spec 0.4) = free play with
  SW1:6 Hit Score OFF (in free play SW1:6-8 = Hit Score / Sound Effect / Music); harness, same inputs: 0xDC 83,770 vs
  0xFC 93,630 at the same CONTINUE. (2) since test 2 the title scene's first 60 frames were black: the arcade's logo
  out of the white, the background wipe and the white flash were hidden (looked like a freeze), only the fade showed.
- Test 7 STAGED 2026-11-01 deploy/test7x (md5 b3db6673) = version 0.7, release/batrider360_v0.7.zip: DSW_ORIGINAL_LOW
  0xFC; the title drawn from its first frame (white 5 frames, the N64 logo's alpha follows the arcade logo's: in at
  10-22, out into the flash 52-58, back 60-100; menu text from the flash's peak, 56). Harness sheets
  work/t7flash/a_sheet.png (START skip) and b_sheet.png (opening played out). Awaiting SUCCESS/FAILURE.
- Test 7 SUCCESS 2026-11-01 = current .xex (v0.7; log work/test7_hardware.log): ORIGINAL credits with DSW 00FC
  (Hit Score on), title opening plays, 0 frames over 16 ms, main loop max 18 ms (one 38 ms frame = host menu save +
  board reset). The log thread had 1.17 s / 0.45 s HDD writes without touching the main loop (the ring absorbs them).
- Test 8 (user 2026-11-01: in-game manual, "PRESS LB+RB FOR MENU", + save states, replays, display options,
  controls page) IN PROGRESS = version 0.8:
  * core/stio.h + snap.c/h: a state = board (RAM, Z80 struct, OKIs, VDP, latches) + Musashi's context up to its
    pointers (m68kstate.c) + both YM2151s (ymfm save_restore) + fe's credit state (+ OPTIONS/LANGUAGE for replays) +
    chasehud + gadgets history; ~172 KB raw, ~36 KB deflated. Load keeps a backup and restores it on failure.
  * replays = a state taken after the frame a credit starts + every pad fe_frame took (out.rec) until 60 frames
    after it ends. Pages over a credit (host menu, states, manual, controls) now stop the whole board (music too)
    and return early from fe_frame, so their frames are not in the replay (fe never touches run state there).
  * PC harness (work/t8: a/b/c/f, ops ssave/sload/hash/rstart/rstop/play, BR_RECAUTO): replays MATCH bit for bit
    (RAM/VDP/68000/Z80) incl. pages opened mid-credit; save+load mid-run changes nothing; per-frame finder for drift.
  * main.c: big buffers from the heap (in .bss they grew the .xex 27 -> 50 MB; now 29.7), states in memory + log
    thread writes (big_queue), replays (30, KEEP THE REPLAY? after >= 10 s), playback 1/2..x3 (sound thread mixes all
    board frames, the last heard), manual pages via the log thread (man_service) into 2x2 textures, quad.frag push
    constants (brightness 0.5..1.5, scanlines on the board's 240 lines, soft 4-tap).
  * tools/mkmanual.py -> release/batrider360_manual.bin (EN+JA, YOKO 488x720 + TATE 720x1064 RGB565 zlib per page).
- Test 8 STAGED 2026-11-01 deploy/test8x (md5 c1f35254) + batrider360_manual.bin; release/batrider360_v0.8.zip. Awaiting SUCCESS/FAILURE.
- Test 8 FAILURE 2026-11-01 (log work/test8_hardware.log): states save/load/delete, replays keep/play/delete all
  worked on hardware. Broken: (1) MANUAL said "NO MANUAL BESIDE THE .XEX": the file was found but refused, two TATE
  pages pack to 812 KB > MAN_ZMAX 512 KB (the check refuses the whole file). (2) slow + stutter in play: repeated
  flips 13 -> 563 (test 7: 9), sound queue re-primed 231 times. Test 8 filled ONE sjobs[] set during the board frame,
  so the main loop waited for the last sound job BEFORE the board frame (waits 7-13 ms in play; test 7: 0) = board +
  sound in series. (3) user: controls can't be customized (only OPTIONS' A/B/X permutation). (4) user: menus should
  use ibara360's grammar and order (survey of ibara360 test 112 menus taken 2026-11-01).
- Test 9 fixes in place (not built yet): sjobs[2][SJOBS] by frame parity, no wait before the board frame;
  MAN_ZMAX 2 MB, man_open logs the refusal's reason; mkmanual.py refuses pages over 2 MB.
- Test 9 design (user 2026-11-01 answers): EVERY menu in ibara360's grammar and order, the ported N64 ones too
  (title menu, OPTIONS, GAME SETTINGS, PRACTICE, pause); a CONTROLS page (A B X Y LB RB LT RT -> OFF / SHOT / BOMB /
  SPECIAL / AUTO SHOT, AUTOFIRE 30/20/15/12/10 Hz, DEFAULTS; default A SHOT, B BOMB, X SPECIAL, RT AUTO SHOT) replaces
  the N64 OPTIONS rows SHOT/BOMB/SPECIAL = BUTTON 1-3. Reference: ibara360 test112 main.c (survey in this session).
- Test 9 STAGED 2026-11-01 deploy/test9x (xex md5 229505ec; + test 8's manual.bin / PDFs / batrider.zip) = v0.9:
  sound jobs double-buffered, MAN_ZMAX 2 MB; core/fe.c's menus rewritten in ibara360's grammar (a 5x7-font box:
  "<GAME>  <PAGE>" yellow, '>' rows, green values, '<  VALUE  >', cyan help, grey notes, green "A = CONFIRM" /
  "B / BACK = ..." lines, ask-twice red questions). Tree: title MENU (PLAY, TRAINING, LOAD STATE, REPLAY DATA,
  OPTIONS); PLAY (START RUN, MODE, CONTINUES, DIP SWITCHES = GAME SETTINGS, ORIGINAL only); TRAINING = PRACTICE (end
  page RETRY / SELECT STAGE / QUIT TO MENU); OPTIONS (GENERAL SCREEN = ROTATION/SCANLINES/BRIGHTNESS %/SMOOTHING,
  GADGETS, CONTROLS, SOUND = MUSIC/SFX % + MUSIC TEST with titles + SOUND TEST, MANUAL, DEBUG MENU = HITBOX /
  INITIALIZE RANKINGS this/both / DEFAULT SETTINGS, LANGUAGE grey in a credit); pause (RESUME, FULL RESTART, SAVE
  STATE, LOAD STATE, OPTIONS, INVULNERABLE, QUIT TO TITLE); LB+RB = OPTIONS anywhere; replay pause RESUME / RESTART
  REPLAY / QUIT REPLAY; SAVE REPLAY? box. The host menu is gone (its rows live in those pages); title has no
  "PRESS LB+RB FOR MENU" line (the menu is on the title). CONTROLS: fe_controls() in main.c's game_bits (FE_SHOT /
  FE_BOMB / FE_SPECIAL bits 0x1000-0x4000 in the recorded pad), options.txt pad_a..pad_rt + autofire (test 8 files:
  their A/B/X map carried over). Replay files version 2 (old ones not listed). States unchanged (test 8's load).
  Harness work/t9 (a: replay MATCH through pause pages + LB+RB OPTIONS; b: title pages, MODE switch) + work/t8
  checks MATCH. Manual text/pictures still describe test 8's menus: update after hardware OK.
- Test 9 SUCCESS 2026-11-01 = previous .xex (log work/test9_hardware.log). User todo: the title's white flash still
  wrong; LB+RB redundant (Start / Back menus); START free in play for the game's own commands (shmups.wiki: hold
  START + B score breakdown, + A/C autofire speed, + directions bullet colours); ibara360's features (4:3, picture
  resize, SFX sound test, ...). Mid-session: a hardware photo of the title stuck white (menu rows white on white), and
  "no N64 logos in this build at all, or any build" (memory: batrider360-no-n64-logos).
- Test 10 STAGED 2026-11-01 deploy/test10x (xex md5 c1db3ada; + test 9's batrider.zip, manual.bin, PDFs) = v0.10:
  (1) title frozen white: title_t counted board frames while a page held the 68000, so the kept copy (TITLE_READY)
  could be taken mid-flash; it now counts the 68000's own frames (harness work/t10/rp: page open across frame 130).
  (2) no N64 logo: the title draws every board layer (the arcade's logo, TOP layer + sprites), its flash is the board's
  palette (work/t10/fl: whitening level 0-31 at title frames 6-21, 55-65, 66-124); tools/mkfront.py makes the splash
  only. A START during the boot left the logo's palettes 84-105 (ROM 0x9C666) out of the game's unfaded palette copy
  0x20E9C8 -> black logo; goto_title puts them in (equal to the splash path's). (3) LB+RB gone. (4) in play START goes
  to the board, BACK alone pauses; the pause closes with B / BACK; the press that closes it is swallowed (no BOMB).
  (5) GENERAL SCREEN in ibara360's order: PICTURE SIZE INTEGER/FULL/CUSTOM, SIZE, X/Y SIZE, X/Y POSITION (per shape,
  held repeat), ROTATION, SCANLINES + SCAN STRENGTH, BRIGHTNESS 5%, SMOOTHING, ASPECT RATIO AUTO/16:9/4:3 (main.c
  game_rect each frame, VdQueryVideoMode for AUTO); GADGETS off in 4:3 or CUSTOM (grey reason). (6) SOUND: MUSIC /
  SFX in tens, ATTRACT, MUTE, MUSIC TEST, SFX TEST. host.txt keys new (test 9's carried over). Harness work/t10/a
  replay MATCH (pause by BACK, LB+RB inert, START held + B in play), c = menu shots.
  Open: manual text/pictures still test 8's (LB+RB, START pause, N64 title); 4:3 GADGETS layout (Baffo's 2021 4:3
  batriderj artwork on G: could be the base); VOICE TEST; no release zip for 0.10 yet.
- Test 10 SUCCESS 2026-11-01 = previous .xex (log work/test10_hardware.log). User: more ibara360 features, menu
  consistency, sound effects; "ORMAL COURSE" on the high score screen; then "redesign the menus using the gadgets
  borders and font".
- Test 11 STAGED 2026-11-01 deploy/test11x (xex md5 f0cae5a9; + test 10's batrider.zip, manual.bin, PDFs) = v0.11:
  (1) menu sounds (core/menusnd.c): SE 0x24 cursor (FM, YM ch 7), 0x22 decide, 0x28 pause played once at boot by
  the board's own sound side (68000 boots 30 frames, then held; split mode; ~0.1 s PC) and mixed by the sound thread
  at SFX volume (0 with MUTE). One rule for every page (fe.c menu_se, ibara360's): a press or held repeat that changed
  what the menus show = cursor, A / START = decide, the pause box opening / closing = pause. The test-10 SEs through
  the board are gone (the 68000's SE routine dropped them in the attract; a page over a credit stops the board).
  (2) held directions repeat as ibara360's (24 frames, then every 9) on every page, LEFT / RIGHT too.
  (3) pause RESTART STAGE < STAGE / STATE > (ibara360's): STAGE = state taken in memory when this credit's stage byte
  0x20E6A9 turns 1..0x7F (0x80 on the select screens); STATE = last slot saved / loaded in this credit; fe_set_restart
  each frame, FE_REQ_RESTART_STAGE -> main.c restart_stage -> fe_restart_loaded (no pause after); a credit's end drops
  both; a new replay starts from the restart. Not in TRAINING (it has RESTART). Harness can't exercise main.c's part.
  (4) "ORMAL COURSE": the ranking's big letters are 2x2 text cells; CHASE!'s hud_clear (batrider-chase hud.s) blanks
  the rail's cols 0-3 lines 4-35 one frame after the ranking drew -> N and the 1ST..5TH labels lost. chasehud.c hooks
  h_vbl's `jsr chase_frame` (0x120C92, before the existing 0x120C98 hook), keeps those cells, and after the clear puts
  back any cell with a tile >= 0x80 (the rail never draws those). Harness work/t11/hs2_sheet.png: NORMAL COURSE + 1ST.
  The same fix is in batrider-chase/src/hud.s (rail_blank) for its MAME build, but chase_patch.h was NOT regenerated:
  batrider-chase's sources changed tonight (chase.s 20:49, bosstab.s 21:05, n64hud.s 21:15, untracked) and its r4
  now reaches 0x165220 (over fe.c's TRAMP 0x165000).
  (5) every menu a GADGETS panel (core/menudraw.c): Baffo's list frame (Secret bosses') in 9 pieces at 0.5
  (tools/mkgadgets.py menu_kit -> GS_MK_*), title plate 2x board font (page name + game small grey when too long),
  black grid body, rows 1x board font 14 texels apart, chosen row on a dark amber band, gadgets' tints (+ grey); the
  title page's menu plain 2x; the replay banner 2x. Upright texels on the 640x480 game texture. 5x7 fallback until
  the board's font is in. Shots work/t11/mc_sheet.png, mp_sheet.png, mc_titles.png.
  Harness: replay MATCH (work/t10/a.txt on test 11 code).
- Test 11 SUCCESS 2026-11-01 = previous .xex (log work/test11_hardware.log). User todo -> test 12: menus' font too
  small (options, pause and their pages); MODE switch in PLAY plays the "BATRIDER" sound; a continue should cancel the
  replay save and the high score screen; values overflow their brackets (SOUND's MUSIC TEST); holding BACK on the
  attract left the title logo grey.
- Test 12 STAGED 2026-11-01 deploy/test12x (xex md5 ae8a217a; + test 11's batrider.zip, manual.bin, PDFs) = v0.12:
  (1) box pages at 1.5x (menudraw md_text sizes in half steps; hm_fit tries 1.5x: rows 20 / lines 17 apart, help in
  two lines, box on screen; else 1x; a line too long for 1.5x is 1x alone). Title plate 2x as before.
  (2) "<  VALUE  >" window to the box's right edge on every page (test 11: 10 letters, MUSIC TEST titles ran out).
  (3) MODE switch: the reset's title calls "BATRIDER" on OKI 1 (title frames 0..~190, work/t12/m); switch_hush keeps
  the board silent (apply_volumes) from the reset until title_t 240 or a credit / the demo (audio.wav: silent, o).
  (4) continue: cont_taken() = CONT_USED (P1+0x116) nonzero while P1 plays, not TRAINING -> not_kept (h_name_entry:
  no name entry, rankings put back) and FeOut.continued -> main.c rec_cont: "replays: not offered (a continue was
  used)". Harness work/t12/c: two CHASE! credits with 3 continues each, counter 0 between credits, no name entry.
  (5) the grey logo was START in the demo (BACK was only the exit chord): logo_palettes() refilled only zero palettes,
  the demo leaves its own in 84-105; it now compares with the ROM (work/t12/d: same picture as the normal title).
  Harness: replay MATCH (work/t10/a.txt on test 12 code, work/t12/ra); menu shots work/t12/o_sheet3.png, s_sheet.png.
- Test 12 FAILURE 2026-11-01 (hardware log: user's batrider360.log): the other items SUCCESS (1.5x menus, values in their
  arrows, no BATRIDER on a MODE switch, grey logo). A CHASE! credit whose CONTINUE? countdown ran out was counted as a
  continue (no SAVE REPLAY?, score reverted). CONT_USED (0x204178) counts CONTINUE? screens opened (only writer
  0x2FAF8), and with a ranked score the name entry raises P1's flag (0x80) while 0x20E6A3 is still FF -> cont_taken
  fired. Harness work/t12/h2 (score poke 09xxxxxx, no input) reproduces it at frame 4429; e/h/hb (low score, button
  presses on the screen) do not.
- Test 13 STAGED 2026-11-01 deploy/test13x (xex md5 21a081aa; + test 12's batrider.zip, manual.bin, PDFs) = v0.13:
  cont_taken also needs CONT_SCREEN (0x20E6A3) clear (a continue taken clears it the frame before P1 flies). Harness
  work/t12/run_v.sh on build/pc_t13: h2 no continue (name entry kept), c both continues caught, e unchanged.
- Test 13 rebuilt 2026-11-02 before any hardware run (user: "apply the same rules as ibara when it comes to score
  validity"; ibara360 test 117's rules) -> deploy/test13x xex md5 f010da64, v0.13:
  fe.c red (score invalid) = a continue taken, ORIGINAL GAME SETTINGS off default, TRAINING, INVULNERABLE, RESTART
  STAGE past stage 1 (STAGE or STATE), LOAD STATE in a credit. Red: GADGETS' TOTAL red + the STAGES row it happened in;
  SAVE REPLAY? still asked, SCORE line red with "*", "* = INVALID SCORE"; replay header flags (offset 36) bit 0 -> the
  REPLAY DATA row red with "*". The rankings are the game's again (its 0x7DE8 refuses a continued player);
  not_kept (no name entry, rankings put back) is INVULNERABLE's alone (my deviation: ibara's invincible is hidden in
  EX-DEBUG). rp_drop (no replay, logged "replays: none for this credit (why)") = TRAINING, INVULNERABLE, RESTART STAGE
  past stage 1, LOAD STATE, QUIT, FULL RESTART (its new run records afresh: credit_flags at h_after_game). A new replay
  starts on fe's credit_gen change or stage 1's RESTART STAGE (rec_again). red/rp_drop ride in not_kept's STIO bits
  1-2 (old states and replays still load). Help texts in ibara's words (INVALID SCORE IF NOT DEFAULT, LOAD ...
  (INVALIDATES SCORE), RESTART STAGE past 1, INVULNERABLE). Harness: work/t12/run_v.sh (c: red at each continue, no
  name-entry block; h2/e: valid), replay MATCH work/t12/ra13 (FULL RESTART resets the flags).
  MANUAL TODO (full rewrite before release): sections "Game settings", the pause box's INVULNERABLE caution, "When a
  score is not kept", the trouble table's no-name-entry line, KEEP THE REPLAY? -> SAVE REPLAY? with the red score.
- Test 13 SUCCESS 2026-11-02 = current .xex (v0.13; user's batrider360.log: CHASE! credit through 5 stages, rankings
  + replay_02 kept, 0 frames over 16 ms).
- Test 14 (in progress): EXIT as the title menu's last row (user: drop BACK + START held). core/fe.c TM_EXIT ->
  settings_flush, exit_t fade over EXIT_FRAMES 30, then FE_REQ_EXIT; main.c breaks out of the loop into the existing
  shutdown (workers, audio, GPU, log thread drained). Harness work/t14/x (sheet.png). Manual EN/JA: the BACK+START
  lines replaced (manual.bin not rebuilt; full rewrite still TODO). Version 0.14. More ibara360 ideas proposed.
- Test 14 user picks 2026-11-02 ("1. yes, remake the training menu in the same style; 2. yes; 3. yes; 4. yes; 5-9 no;
  10. rewrite the manual and github will be updated, but kept private"):
  (1) TRAINING = ibara360's TRAINING page (core/fe.c tr_*): START RUN, MODE, TEAM CHOOSE/SAME, STAGE (7 + EXTRA
  BOSSES), START AT BOSS (the stage's bosses incl. secret ones, table from work/t14/r1/REPORT.md 6), LIVES (1-9 CHASE!
  via TEAM_LAST 3 + CS bank, 1-4 ORIGINAL), BOMBS 0-4, SHOT POWER 1-4, RANK DEFAULT/% (LB/RB 10; CHASE! 51-76%),
  INVINCIBLE (h_hit), INFINITE BOMBS, GAME SPEED 100/75/50 (68000 held, sound runs), HITBOX, DIP SWITCHES. tr_frame
  writes at each rider start (flags 0x8E; TEAM SAME: 40 frames into play), holds rank, ends the run on the stage byte's
  step (the course plays its entry twice) via CONT_COUNT 0xFF; no continues in training; GAME OVER / clear / pause QUIT
  TO TRAINING -> the page, QUIT TO TITLE -> the title menu. Values in save states through test 13's dead prac_cur /
  prac_first / end_cur slots (TR_PACKED marker; layout unchanged, replay MATCH A0867AF9 = test 13's).
  (2) HITBOX OFF/PLAYER/ALL (DEBUG MENU + TRAINING; old ON = PLAYER): enemy (red), shot-only target (blue), bullet
  (magenta) rectangles from the 68000's lists (work/t14/r2/REPORT.md A), vdp.c draw_box; player disc x read unsigned
  (was lost past x 0x8000), all boxes one frame late to sit on the sprites.
  (3) OPTIONS > SECRET COMMANDS: NORMAL/BALL BULLETS, MISSILES colour, SHOT RATE (grey in a credit), SCORE BREAKDOWN;
  stored in Options.cmd[] (test 8's map[] place, CMD_SET 0x1000 marks them; old replays = the game's), commands_frame.
  (4) Boot: logo via xsdk_splash_begin while a loader thread reads the zip, members unpacked on 4 HW threads (zlib
  arena, no malloc in threads), row-at-a-time graphics decode (frames byte-identical), batrider360_menusnd.bin cache,
  NOW LOADING ticker (GADGETS panel, board font from program ROM 0x100000) until the first frame. Expect ~1 s boot
  after the first (test 13: ~4.4 s incl. the 1.5 s logo hold).
  (10) Manual rewrite delegated (tools/manual, release/manual, manual.bin); README.md + allow-list .gitignore for the
  private GitHub repo PaulAtreides67/batrider360 (third_party vendored: ymfm has local edits).
- Test 14 STAGED 2026-11-02 deploy/test14x (xex md5 a17a9a00; manual.bin + PDFs rewritten for 0.14, EN 52 / JA 56
  pages; batrider.zip) = v0.14, release/batrider360_v0.14.zip; private GitHub PaulAtreides67/batrider360 commit 9709369
  + tag v0.14. Awaiting SUCCESS/FAILURE.
- Test 15 (user todo after test 14's SUCCESS 2026-11-02; user: "if it ends up too large, make a player customization
  mini window"): research work/t15/r1 (REPORT.md = the helper's report: SELECT TYPE 0x132F4 / 0x20FD50 / 0x20E488,
  pilots 0-17 + types A-D = SHOT/BOMB/C/START, guests' formations +0x22 incl. specials 0x0A-0x12, option pickups f170 at
  0xD4E-0xD54, 40 fragments a bomb + HUD repaint bits 0xC0 of P1+1). core/fe.c: TRAINING rows START RUN, MODE, TEAM
  (CHOOSE / SAME / SELECT TEAM / TEAM EDIT / RANDOM EDIT / SELECT PLAYER / CUSTOM), PLAYER (opens OV_PLAYER: RIDER 1-3
  ship + type, FORMATION DEFAULT/V/BACK/FRONT/CIRCLE/CONTROL/SPECIAL 1-5, LIVES, BOMBS, BOMB FRAGMENTS 0-39, SHOT
  POWER, OPTION POWER NONE/1/2/3), STAGE, START AT BOSS, RANK, INVINCIBLE, INFINITE BOMBS, GAME SPEED, HITBOX, DIP
  SWITCHES. Hooks h_type (0x132F4: TEAM's choice skips SELECT TYPE as its confirm does) and h_opts (0xD48: level 0 +
  the last N pickups). CUSTOM = SELECT TEAM confirmed by host SHOT taps under the veil, riders written 5 frames after
  the confirm (tr_custom_team, team_ram too for SAME). Training pause without SAVE/LOAD STATE; SECRET COMMANDS GAME ->
  DEFAULT; OPTIONS' SECRET COMMANDS help shortened (3 lines at 1.5x in the 184 box dropped the page to 1x). Harness
  work/t15/v (a: CUSTOM 00A8 009C 0020 + CIRCLE + 2 bombs + 15 frags + option lv 0 verified; b: SELECT PLAYER skip;
  c/d: OPTIONS 1.5x, training pause). Manual not updated for the new TRAINING page (TODO).
- Test 15 STAGED 2026-11-02 deploy/test15x (xex md5 c70d24fe; test 14's manual.bin/PDFs + batrider.zip) = v0.15,
  release/batrider360_v0.15.zip. Awaiting SUCCESS/FAILURE.
- Test 15 SUCCESS 2026-11-02 = current .xex. User todo -> test 16: menus dropping to the small font "randomly in any
  option as you keep updating" (RANK in TRAINING) -> permanent fix; SELECT PLAYER must disable riders 2 and 3; TYPE its
  own toggle; DEBUG MENU INFINITE LIVES + INFINITE BOMBS.
- Test 16 (core/fe.c): hm_fit decides 1.5x per page (title + row count) from its shape with two help lines, kept while
  the page stays; a name / value / help line / line too long for 1.5x is drawn at 1x alone (hm_text_fit, arrows' window
  value at 1x before a cut); "menu:" log line per page decision. PLAYER page rows RIDER/TYPE x3 (LEFT/RIGHT, no LB/RB);
  TEAM SELECT PLAYER: RIDER = CHOOSE (the arcade's screen) or a rider for every life (tr.solo: the screen confirmed by
  host SHOT taps, the word in all slots like 0x8288), TYPE skipped while CHOOSE, riders 2/3 dimmed + skipped; 4 LIVES of
  a one-rider team = its own pilot. DEBUG MENU INFINITE LIVES (db_frame: no team word keeps bit 0, a pilot flying from
  the last slot moved to the front; death routine 0x2FA32) and INFINITE BOMBS (4 + HUD repaint bit 7, also TRAINING's),
  session only, INVULNERABLE's score rules (red, no replay, not kept; also on a LOAD STATE). Harness work/t16/v (s: solo
  SHORTY C = 0018 x3, 3 lives; n: GAME OVER at 5013; l: 50 deaths in 30000 frames, no GAME OVER; m/r: 1.5x OPTIONS /
  DEBUG / PLAY / TRAINING incl. RANK). Manual not updated for test 15/16.
- Test 16 STAGED 2026-11-02 deploy/test16x (xex md5 6a949d08; test 15's manual.bin/PDFs + batrider.zip) = v0.16,
  release/batrider360_v0.16.zip. Awaiting SUCCESS/FAILURE.
- Test 16 SUCCESS 2026-11-02 = current .xex (user's batrider360.log, v0.16). User todo -> test 17: (1) START out of the
  demo into the title keeps the demo's music; (2) title: the original copyright / year / company line back at the
  bottom, plus "CHASE HACK BY COIN-OP" if text room allows; (3) PLAYER FORMATION help "GAREGGA SHIPS ONLY"; (4) the
  guests' secret formations selectable in PLAYER (skip if none); (5) TRAINING INFINITE LIVES above INFINITE BOMBS;
  (6) DEBUG MENU INVINCIBILITY below INFINITE BOMBS (score invalid).
- Test 17 (core/fe.c): (1) the demo-START jump (goto_title from the attract branch) queues the SOUND page's STOP
  (music_cmd 21 00, 20 E0): harness work/t17/v g (START in the gameplay demo: level 0 ~200 frames later) vs test 16's
  build (OKI music on for good). (2) FE_V_TITLE keeps the arcade title's text lines 3-5 (TITLE_COPY_LO/HI: (C) RAIZING,
  (C) 8ING logo tiles pal 0x37, "CO.,LTD." "1998"; dump op "text" in pc/harness.c, o_t), the rest cleared (FREE PLAY);
  "CHASE HACK BY COIN-OP" on line 1 in place of ARRANGED BY / COIN-OP GAME STUDIO (c_sheet.png). (3) PLAYER FORMATION
  help "GAREGGA SHIPS ONLY". (4) The guests' secret formations were already there since test 15 (REPORT.md 3, +0x22
  0x0A-0x12 from the hidden counters): SPECIAL 1-5 renamed SECRET 1-5. (5) TRAINING INFINITE LIVES (TR_INFL, packed
  bit 29, db_frame's lives logic split out as inf_lives): harness tm 14 deaths then the stage clear, tn (off) GAME
  OVER after 3. (6) DEBUG MENU INVINCIBILITY (db_inv, h_hit in a credit outside a playback; cheat like INFINITE LIVES:
  red, no replay, not kept, also on LOAD STATE): harness i, no death in 30000 frames. Harness pc/harness.c BR_LEVEL.
- Test 17 STAGED 2026-11-02 deploy/test17x (xex md5 bc8ffeb0; test 16's manual.bin/PDFs + batrider.zip) = v0.17,
  release/batrider360_v0.17.zip. Manual not updated for tests 15-17. Awaiting SUCCESS/FAILURE.
- Test 17 SUCCESS 2026-11-02 = current .xex (user's batrider360.log, v0.17). User todo -> test 18: title menu PLAY
  help "START A RUN", TRAINING and OPTIONS helps unfinished (cut: the plain title menu wraps 18 letters x 2 lines at
  2x), SHOT / OPTION POWER must drop normally on a death, SHOT POWER 1 started at 4, TEAM into the PLAYER page at the
  top + PLAYER renamed PLAYER EDIT, OPTION POWER help's "(3 = THE GAME'S TRAINING)" line removed.
- Test 18 (core/fe.c): title helps rewritten to 18-letter lines. New hook h_power at 0xD42 (move.b #3,(0x110,a6), the
  training course's forced shot at every rider start, 0xD36-0xD54): the run's first start writes SHOT POWER there then
  h_opts' OPTION POWER; later starts jump to 0xD56 (as a course < 6), so the game's own reset (shot 0, option level 0)
  and the dropped items rule. Cause of "shot 1 -> 4": tr_frame wrote at the 0x8E edge a frame before the routine
  forced 3 (harness work/t18/v u17 reproduces 03 with the user's settings: ORIGINAL, SELECT PLAYER alone, LIVES 1,
  INFINITE LIVES; u18 stays 00). TEAM = PR_TEAM, the PLAYER EDIT page's first row (TR_TEAM gone; TRAINING 12 rows,
  PLAYER EDIT 13). Harness work/t18/v (p/q: power after deaths, s: menus s_sheet.png, u17/u18).
- Test 18 STAGED 2026-11-02 deploy/test18x (xex md5 7a3aa64a; test 17's manual.bin/PDFs + batrider.zip) = v0.18,
  release/batrider360_v0.18.zip. Manual not updated for tests 15-18. Awaiting SUCCESS/FAILURE.
- Test 18 SUCCESS 2026-11-02 = current .xex (v0.18). User todo -> test 19: MEDAL row in TRAINING, ibara360's text.
- Test 19 STAGED 2026-11-02 deploy/test19x (xex md5 96a01463) = v0.19: TRAINING MEDAL row under START AT BOSS (100 ..
  10000, ibara360's 19 values, LB/RB 2 steps, help "STARTING MEDAL VALUE"), written to 0x20E459 (step x 2) at the run's
  first rider start; a death lowers it the game's way (harness work/t19/v: 600 kept until the death, then 300).
- Test 19 SKIPPED by the user 2026-11-02 ("just one small change"; "the game is pretty much ready") = v0.19.
- 2026-11-02 manual rewrite (user: "better grammar and consistency ... nearly incomprehensible to the average
  reader"): manual_en.html rewritten in plain English (one name per thing: "run", on-screen names in capitals,
  "OPTIONS > PAGE"), updated to v0.19 (PLAYER EDIT page, MEDAL, INFINITE LIVES, training pause without states, DEBUG
  MENU cheats + their invalid scores, SECRET COMMANDS DEFAULT); new art from harness work/t19/manual (gen.py ->
  n.txt, make_art.py): title, commands, debug, training, player_edit, training_pause; page breaks tuned (52 pages).
  manual_ja.html re-translated from it by a helper.
- 2026-11-02 private release v0.19 with a mame folder (user): work/mame_rom/batriderj.zip = batrider-chase
  out/roms' four patched program ROMs only (its old nested 2021 layout folder dropped), batrider-chase
  out/artwork/batriderj = the GADGETS for BOTH games (user: "the gadgets for arcade batrider use my improved
  layout": gadgets.lua no longer refuses the stock program, view "GADGETS"; MAME-checked on stock and CHASE!),
  tools/mame_release (two .bat launchers with MAME=/ROMS= to set, README_MAME.txt). mkrelease.sh allows only that zip.
