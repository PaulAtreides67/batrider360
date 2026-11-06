/* batrider360: the N64 CHASE! front end (work/n64_frontend_spec.md), platform-free: the 360's main.c and the PC
 * harness drive it the same way.
 *
 * What it owns: the board's inputs (br_set_inputs), DIP switches (br_set_dips), volumes (br_set_volume), the 68000
 * held or not (br_hold_main), the program (ORIGINAL / CHASE!: br_set_program + br_reset on a MODE change), the
 * 68000 hooks (br_set_hook; 0x4AFC at the spec's hook sites of both programs, trampolines at 0x165000), the board
 * RAM writes of the N64 front end (title timers, continues, practice, team restore, rankings).
 *
 * Calls, all on the board's thread, in this order each display frame:
 *   fe_frame(&pad, &out);                    // inputs + front end; says whether the board runs
 *   if (out.run) br_run_frame();
 *   fe_after_board_frame();                  // hooks' bookkeeping; fills out.job's hitbox discs
 *   copy *out.vsrc into the video thread's Vdp and out.job into its FeVideoJob (struct copy), then on the video
 *   thread: fe_video(&vdp_copy, &job_copy, tex640x480);
 * With the split board (main.c), out.vsrc is br_vdp_snapshot() or the front end's kept title copy; the harness runs
 * split too (br_vdp_snapshot is only kept up to date then).
 *
 * Files (FePlatform.load / save, beside the .xex on the 360): host.txt (host menu), options.txt (OPTIONS + GAME
 * SETTINGS), rankings_chase.bin, rankings_original.bin (0x230 bytes of the four tables' top fives, magic + digest).
 * save() may queue the write (the 360's file thread); fe never reads a file back after fe_init. */
#ifndef BR_FE_H
#define BR_FE_H
#include <stdint.h>
#include "vdp.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Pad bits (held and press = new this frame). Directions from the d-pad or the stick past its dead zone. */
enum {
	FE_UP = 0x001, FE_DOWN = 0x002, FE_LEFT = 0x004, FE_RIGHT = 0x008,
	FE_A = 0x010, FE_B = 0x020, FE_X = 0x040, FE_Y = 0x080,
	FE_START = 0x100, FE_BACK = 0x200, FE_LB = 0x400, FE_RB = 0x800,
	/* test 9: the game's buttons as CONTROLS has set the pad (fe_controls); the menus read the pad's own A / B */
	FE_SHOT = 0x1000, FE_BOMB = 0x2000, FE_SPECIAL = 0x4000,
	/* test 21 (user): CONTROLS' A+START = the game's START held with its SHOT (fe_controls): the secret commands */
	FE_MSTART = 0x8000,
};
/* Test 9 (user: "controls can't be customized"; ibara360's CONTROLS page): the pad's buttons CONTROLS sets, as bits of
 * fe_controls' mask. The platform calls fe_controls once a display frame with the buttons down (LT / RT past half)
 * and ORs its FE_SHOT / FE_BOMB / FE_SPECIAL (A+START: FE_SHOT | FE_MSTART) into the pad it hands fe_frame, so a replay holds them as played. */
enum { FE_K_A, FE_K_B, FE_K_X, FE_K_Y, FE_K_LB, FE_K_RB, FE_K_LT, FE_K_RT, FE_KEYS };
uint16_t fe_controls(unsigned down);
typedef struct {
	uint16_t held, press;
} FePad;

struct FeSlot;
struct FeReplayInfo;
typedef struct {
	int (*load)(const char *name, void *buf, int max);    /* bytes read, < 0 = none */
	int (*save)(const char *name, const void *buf, int len);
	void (*log)(const char *fmt, ...);
	/* Called before fe resets the board for a MODE change (br_set_program + br_reset): return once no other thread
	 * uses the board's sound state (main.c: wait for the sound thread's last job). May be NULL. */
	void (*quiesce)(void);
	/* Test 8 (may be NULL): what the SAVE / LOAD STATE page shows of slot i (0..FE_SLOTS-1) of a MODE, and the i-th
	 * newest replay (returns how many there are). */
	void (*slot)(int mode, int i, struct FeSlot *s);
	int (*replay)(int i, struct FeReplayInfo *r);
} FePlatform;

/* Test 8: save states (FE_SLOTS a MODE, each with a picture: FE_THUMB_W x FE_THUMB_H RGB565, the board's
 * orientation) and replays (the main loop keeps them; the front end only shows them and asks). */
enum { FE_SLOTS = 10, FE_THUMB_W = 80, FE_THUMB_H = 60 };
typedef struct FeSlot {
	int used;
	char text[32];                     /* "ST3 12345670" */
	const uint16_t *pic;               /* NULL = none */
} FeSlot;
typedef struct FeReplayInfo {
	char text[32];                     /* one row of the REPLAYS page */
	int red;                           /* test 13: its score was invalid (the row red) */
} FeReplayInfo;
/* FeOut.req: what the main loop does after this frame; req_arg: MODE * FE_SLOTS + slot for the states, the replay's
 * place (0 = newest) for the replays */
enum {
	FE_REQ_NONE, FE_REQ_SAVE_STATE, FE_REQ_LOAD_STATE, FE_REQ_DELETE_STATE, FE_REQ_PLAY_REPLAY, FE_REQ_DELETE_REPLAY,
	FE_REQ_KEEP_REPLAY, FE_REQ_DROP_REPLAY,
	FE_REQ_RESTART_STAGE,              /* test 11: req_arg 0 = the stage's start, 1 = the last state saved or loaded */
	FE_REQ_EXIT,                       /* test 14: the title's EXIT faded out: leave for the dashboard */
};

enum { FE_CHASE, FE_ORIGINAL };                 /* MODE = index into progs[] */
enum { FE_YOKO, FE_TATE_A, FE_TATE_B };          /* host menu SCREEN */
enum { FE_V_GAME, FE_V_TITLE, FE_V_SPLASH, FE_V_BLACK, FE_V_WHITE };

/* test 21: FE_HM_ROWS 16 (OPTIONS' GADGETS page: GADGETS, the 14 panels, ALL ON) */
enum { FE_COLS = 30, FE_LINES = 40, FE_DISCS = 2, FE_HM_ROWS = 16, FE_HM_CHARS = 40, FE_HM_LINES = 6, FE_BOXES = 192 };

/* Test 9: every menu as ibara360 draws its own (user: "use similar grammar and menu sorting to the menus i used for
 * ibara360"): a box over the darkened picture, the title yellow ("BATRIDER  OPTIONS"), the rows with a yellow '>'
 * on the chosen one, names white (yellow chosen, grey unusable, red armed), values green (yellow chosen, red changed),
 * '<' '>' around a value that LEFT / RIGHT change, the help cyan (red for a question), then grey notes and green
 * button lines. The 5x7 font, 6 board pixels a column, 10 a row. */
enum { FE_C_WHITE, FE_C_YELLOW, FE_C_GREEN, FE_C_CYAN, FE_C_DIM, FE_C_RED };
typedef struct {
	char name[24], value[28];
	uint8_t name_c, value_c, arrows;
} FeRow;

/* What the video thread draws (filled by fe_frame / fe_after_board_frame; copy it with the Vdp). */
typedef struct {
	int kind;                  /* FE_V_* */
	int fade;                  /* 0 black .. 256 */
	uint32_t title_gen;        /* nonzero: the kept title copy number (the drawn title is cached while it stays) */
	int text;                  /* ov holds cells */
	uint32_t ov[FE_COLS][FE_LINES];  /* text layer cells written over the copy: bit 31 set, bit 30 opaque glyph,
	                                    low 16 bits the cell word (pal << 10 | ASCII) */
	int ndisc;
	VdpDisc disc[FE_DISCS];    /* hitbox discs (board coordinates, 1/128 px) */
	int nbox;
	VdpBox box[FE_BOXES];      /* test 14: HITBOX ALL's enemy and bullet boxes */
	int hm;                    /* a menu box drawn (test 9: every menu) */
	int hm_w, hm_vcol;         /* the box's width (board pixels), the values' column */
	int hm_plain, hm_top;      /* plain: no box or darkening (the title's menu); top: its upper edge (0 = centred) */
	int hm_n, hm_sel;          /* rows, selected row (-1 none) */
	char hm_title[FE_HM_CHARS], hm_corner[24];   /* corner: grey at the title line's right ("" none) */
	FeRow hm_row[FE_HM_ROWS];
	char hm_help[2 * FE_HM_CHARS];               /* up to two lines ('\n' or wrapped at a word) */
	uint8_t hm_help_c;
	int hm_nl;                                   /* lines under the help */
	char hm_line[FE_HM_LINES][FE_HM_CHARS];
	uint8_t hm_line_c[FE_HM_LINES];
	const uint16_t *pic;       /* test 8: a save state's picture beside the rows (FeSlot.pic), NULL = none */
	char banner[FE_HM_CHARS];  /* test 8: a line across the top (a replay's speed and time), "" = none */
} FeVideoJob;

typedef struct {
	int run;                   /* call br_run_frame this frame (0: the whole board stands, sound included) */
	int held;                  /* the 68000 is held this frame (fe applied br_hold_main), Z80/YM/OKI run */
	const Vdp *vsrc;           /* copy into the video job's Vdp */
	FeVideoJob job;
	int screen;                /* FE_YOKO / FE_TATE_A / FE_TATE_B */
	int gadgets;               /* OPTIONS' GADGETS on */
	int in_credit;             /* a credit (or its name entry) is running */
	int chase_credit;          /* ... and it is CHASE! */
	int original;              /* v0.20: the ORIGINAL program runs (16:9 GADGETS take the arcade layout) */
	int invalid;               /* test 13: ... and its score is invalid (ibara360's red SCORE: SAVE REPLAY? red, "*") */
	int rp_drop;               /* test 13: ... and it gets no replay (rp_why: the first reason) */
	const char *rp_why;
	unsigned credit_gen;       /* test 13: counts credits, a FULL RESTART's new run too (main.c: a new replay) */
	int mode;                  /* host menu MODE (the program START plays) */
	int host_menu;             /* a page over the game is open (OPTIONS and its pages, states, replays, manual) */
	int paused;                /* the pause box is open */
	/* test 8 */
	int rec;                   /* this frame's pad belongs in the replay (0: a page over the game, the board stood) */
	int req, req_arg;          /* FE_REQ_* for the main loop */
	int manual;                /* the manual is shown (instead of the game): page man_page (from 0) in man_lang */
	int man_page, man_lang;    /* (YOKO shows man_page and the next as a spread) */
	int smooth;                /* SMOOTHING 0/1 */
	/* test 10 (ibara360's GENERAL SCREEN): BRIGHTNESS percent (100 = as it is), the scanlines' darkness percent (0 =
	 * none), the display's shape (wide: 16:9, else 4:3, ASPECT RATIO with AUTO resolved) and where the game goes:
	 * picture FE_PIC_*, and for FE_PIC_CUSTOM the shape's size, X / Y size (thousandths) and X / Y position (720ths
	 * of the screen's height, as the player sees the game: right, down) */
	int bright, scan, wide;
	int picture, pic_size, pic_sx, pic_sy, pic_x, pic_y;
	/* test 11: the menu sounds started this frame (1 << MS_* of menusnd.h; the host mixes them, menusnd_mix) and their
	 * volume 0..256 (SFX, 0 with MUTE); fe_playback_input's sounds come out in the next fe_frame's */
	unsigned se;
	int se_vol;
	/* test 21 (user): GADGETS per panel (GAD_P_ bits of gadgets.h; the GADGETS page), and the run is ADVANCE SPECIAL (in a
	 * credit only: the secret bosses all count as met in its GADGETS' Secret bosses panel, except ENVY / BLUNT / SOBUT) */
	unsigned gad_mask;
	int special;
} FeOut;

/* Test 10: GENERAL SCREEN's picture (ibara360's PICTURE SIZE / ASPECT RATIO, its limits). Shapes = 16:9, 4:3, tate on
 * 16:9, tate on 4:3 (each keeps its own CUSTOM picture). GADGETS show on 16:9 with INTEGER or FULL only. */
enum { FE_PIC_INTEGER, FE_PIC_FULL, FE_PIC_CUSTOM };
enum { FE_ASPECT_AUTO, FE_ASPECT_16_9, FE_ASPECT_4_3 };
enum { FE_SHAPES = 4, FE_PIC_SIZE_MIN = 300, FE_PIC_SIZE_MAX = 3000, FE_PIC_STRETCH_MIN = 500, FE_PIC_STRETCH_MAX = 2000,
	FE_PIC_POS_MAX = 2000 };
/* The console's own display shape (main.c, before fe_init: ASPECT RATIO's AUTO); 1 = 16:9 */
void fe_display(int wide);

/* progs[FE_CHASE], progs[FE_ORIGINAL]: the two programs (progpatch_make); hooks are written into both. Call after
 * br_init. Loads the files, sets the program of the saved MODE, resets the board. 0 = ok, -1 = a hook site or the
 * trampoline area held other bytes (the log says which). */
int fe_init(const FePlatform *p, uint8_t *progs[2]);
void fe_frame(const FePad *pad, FeOut *out);
void fe_after_board_frame(void);
/* Video thread: the finished picture (640x480, the board's orientation doubled, 0xAARRGGBB) from v (a copy of
 * out.vsrc; fe writes its text pages into it) and job. One caller at a time (static scratch). */
void fe_video(Vdp *v, const FeVideoJob *job, uint32_t *tex);

/* Test 8. The manual's pages per language (0 = no manual file: the MANUAL rows say so). */
void fe_manual_pages(int en, int ja);
/* After the main loop loaded a state: the pages close; in a credit the pause box comes up over the loaded game and the
 * rankings are put back as the files have them. quiet (a replay starting or ending): only the pages close. */
void fe_state_loaded(int quiet);
/* A SAVE / LOAD STATE request that failed (the slot page says so). */
void fe_state_failed(int saving);
/* A credit ended with a replay kept in memory: the SAVE REPLAY? question with up to 3 lines about it. */
void fe_replay_prompt(const char *l1, const char *l2, const char *l3, int red_score); /* test 13: red_score = line 2 red */
/* Replays: the main loop feeds fe_frame the recorded pads; the player's own pad goes to fe_playback_input. */
enum { FE_PB_NONE, FE_PB_SLOWER, FE_PB_FASTER, FE_PB_QUIT, FE_PB_RESTART };
void fe_playback(int on);
int fe_playback_input(const FePad *pad);
/* test 11: the menu sounds fe_playback_input started (FeOut.se's bits); taken = cleared */
unsigned fe_menu_sounds_take(void);
/* Test 11: the pause's RESTART STAGE. Each frame: stage = the number of the stage whose start the main loop holds for
 * this credit (0 = none), state = nonzero while a save state saved or loaded in this credit is there (a new value for
 * each save or load: it then becomes the row's first choice). After loading for FE_REQ_RESTART_STAGE (instead of
 * fe_state_loaded): fe_restart_loaded, which leaves the stage running. Test 13: its stage = the stage restarted (0 for
 * a STATE): past stage 1 the score goes red and the credit gets no replay. */
void fe_set_restart(int stage, unsigned state);
void fe_restart_loaded(int stage);
int fe_playback_paused(void);
/* the speed / time banner or the replay's pause box over this frame's picture */
void fe_playback_decorate(FeOut *out, const char *speed, uint32_t frame, uint32_t frames);
/* After a replay: the REPLAYS page again, with a message */
void fe_open_replays(const char *msg);

#ifdef __cplusplus
}
#endif
#endif
