/* batrider360: the N64 CHASE! front end (fe.h), from work/n64_frontend_spec.md (section numbers below are its).
 * Everything here runs on the board's thread except fe_video (the video thread, from copies). */
#include "fe.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "board.h"
#include "front.h"
#include "m68k.h"
#include "progpatch.h"
#include "stio.h"

#include "chasehud.h"
#include "menusnd.h"
#include "menudraw.h"

/* ---- the board's addresses (spec 0.6 and the sections that use them) ---- */
enum {
	SCENE = 0x20E6AA, SCENE_TITLE = 0x28,
	ATTRACT = 0x20E69D,                 /* bit 7: attract / title (0x80, 0xA0); clear: a credit or its name entry */
	DEMO_NO = 0x20E69E, IN_PLAY = 0x20E6A0, CONT_COUNT = 0x20E6A1, CONT_SCREEN = 0x20E6A3, COURSE = 0x20E6A6,
	CREDIT_P1 = 0x2034C2, CREDIT_P2 = 0x2034C3,
	TITLE_TIMER = 0x20342E, TITLE_TIMER2 = 0x203430,
	CONT_OK = 0x20F9FF, CONT_USED = 0x204178,
	P1 = 0x204062, P2 = 0x2041D4, TEAM = 0x20407C, TEAM_LAST = 0x20F9FD,
	/* test 14 (work/t14/r1/REPORT.md): P1's shot level / bombs / gauge, the current pilot word, the rank and its cap,
	 * the course's stage byte, CHASE!'s state and its extends bank */
	PL_PILOT = 0x12, PL_SHOT = 0x110, PL_SHOTP = 0x111, PL_BOMBS = 0x112, PL_GAUGE = 0x113,
	/* test 15 (work/t15/r1/REPORT.md): SELECT TYPE's choice x 2, the guests' formation, the options' level, P1's HUD
	 * repaint bits (0x40 gauge, 0x80 bombs) */
	TYPE_MODE = 0x20E488, PL_FORM = 0x22, PL_OPTLV = 0x104, PL_REPAINT = 1,
	RANK_NOW = 0x20F9D0, RANK_CAP = 0x20F9D8, STAGE_NO = 0x20E6A9, CS_STATE = 0x201000, CS_BANK = 0x201026,
	SOUND_ON = 0x20FA07, DEMO_SOUND = 0x20F9F7, SE_WAIT = 0x20E589,
	MEDAL = 0x20E459,                   /* test 19: the next medal's step x 2 (gadgets.c's; 0 = 100 .. 0x24 = 10000) */
	RANK0 = 0x20FA24, RANK_STRIDE = 0xC4, RANK_KEEP = 0x8C, RANK_BYTES = 4 * RANK_KEEP,
	PRACTICE_TABLE = 0x1391C, PRACTICE_N = 25,
	LANG_SITE = 0x13A34,
	TRAMP = 0x165000, TRAMP_END = 0x165400,  /* free ROM (batrider-chase NOTES: R4's tail past CHASE!'s code) */
	ILLEGAL = 0x4AFC,
};

/* DIPs (spec 0.4): DSWC 0xE0 = Special Course, Player Select, Guest Players ON (batriderj reads 0 = On... MAME's
 * PORT_MODIFY for batriderj: 1 = On); Stage Edit, Continue Play, Invulnerability, Flip OFF, as the N64 forces them
 * (user 2026-11-01: the N64's DIP handling, not test 2's 0xE4). */
/* ORIGINAL's low byte: free play 0x1C + SW1:6-8 = Hit Score, Sound Effect, Music on (the N64's 0xDC leaves Hit
 * Score off: no points for shot hits; test 7) */
enum { DSWC = 0xE0, DSW_CHASE = 0xC6FC, DSW_ORIGINAL_LOW = 0xFC };

/* timings (frames) */
enum {
	SPLASH_END = 0xE5, SPLASH_SKIP_BEFORE = 0xBE, SPLASH_OUT = 0x28, SPLASH_IN = 20,
	/* the arcade's title scene: white, its logo out of the white onto black, the background wiped in, a white flash
	 * (55-65), the whole title out of the white (66-124: palette steps measured in work/t10/fl); test 10 draws the
	 * arcade's own logo, so all of it comes from the board's palette */
	TITLE_WHITE = 5, FLASH_TEXT = 56, TITLE_SHOW = 60, TITLE_READY = 130,
	IDLE_DEMO = 1800,
	RESET_GOTO = 10,                    /* after a reset for the other MODE: into the attract's title here */
	FADE_FRAMES = 16, PULSE = 4, PULSE_RETRY = 30, LAUNCH_TIMEOUT = 900,
	EXIT_FRAMES = 30,                   /* test 14: the title's EXIT fades out, then FE_REQ_EXIT */
	PAUSE_SE_FRAMES = 6,
	VEIL_AFTER = 12,                /* the 68000 held, the sound side running: SE 0x28 starts, then all stands */
	LATCH_WAIT = 8,
};

/* text palettes (spec 0.1) */
enum { P_WHITE = 0x30, P_BLUE = 0x31, P_RED = 0x32, P_GREEN = 0x33, P_AMBER = 0x34, P_CYAN = 0x35 };
enum { OV_SET = 0x80000000u, OV_OPAQUE = 0x40000000u };

static FePlatform plat;
static uint8_t *prog[2];
static uint32_t tramp_of[16];

#define LOG(...) do { if (plat.log) plat.log(__VA_ARGS__); } while (0)

/* ---- settings ---- */
/* language 0 EN, 1 JA; smooth 0/1. Test 10 (user: ibara360's GENERAL SCREEN and SOUND): music / effects / attract
 * volumes in percent (tens; test 9's eighths carried over), mute; bright = BRIGHTNESS percent (5% steps; test 8's 0..10
 * carried over); scan = SCANLINES on / off, strength = SCAN STRENGTH percent (test 8's LIGHT / STRONG = 35 / 60);
 * picture INTEGER / FULL / CUSTOM, aspect AUTO / 16:9 / 4:3, and CUSTOM's size, X / Y size (thousandths) and X / Y
 * position (720ths of the screen's height) per display shape (fe.h FE_SHAPES) */
enum { BRIGHT_MIN = 50, BRIGHT_MAX = 150, STRENGTH_MIN = 5, STRENGTH_MAX = 100 };
typedef struct {
	int hitbox, music, effects, language, screen, gadgets, mode, bright, smooth, scan;
	int strength, attract, mute, picture, aspect;
	int pic_size[FE_SHAPES], pic_sx[FE_SHAPES], pic_sy[FE_SHAPES], pic_x[FE_SHAPES], pic_y[FE_SHAPES];
} Host;
/* test 14: cmd[] (test 8's button map until test 9's CONTROLS; the same place, so the replays' layout stays) = SECRET
 * COMMANDS: cmd[0] SHOT RATE (0 the game's, 1..8 = its autofire value 0..7), cmd[1] SCORE BREAKDOWN, cmd[2] CMD_SET |
 * the bullet colours (3 bits each, NML, BAL, MSL: 0 the game's, 1..7 = colour 0..6). A replay of test 13 or before holds
 * the old map { 0, 1, 2 }: no CMD_SET, every command the game's. */
typedef struct { int continues, cmd[3], music_test, sound_test, gs[4]; } Options;
enum { CMD_SET = 0x1000 };   /* (above the colours' 9 bits) */
static const Host host_default = { 1, 100, 100, 0, FE_YOKO, 1, FE_CHASE, 100, 0, 0, 35, 100, 0, FE_PIC_FULL,
	FE_ASPECT_AUTO, { 1000, 1000, 1000, 1000 }, { 1000, 1000, 1000, 1000 }, { 1000, 1000, 1000, 1000 }, { 0 }, { 0 } };
static const Options opt_default = { 3, { 0, 0, CMD_SET }, 0x01, 0x20, { 0, 0, 0, 0 } };
static Host host;
/* test 14: HITBOX (DEBUG MENU and TRAINING): test 13's ON = PLAYER, ALL adds the enemies' and their bullets' */
enum { HITBOX_OFF, HITBOX_PLAYER, HITBOX_ALL, HITBOX_MODES };
static const char *const hitbox_names[HITBOX_MODES] = { "OFF", "PLAYER", "ALL" };
static Options opt;

/* Test 9: CONTROLS (ibara360's page): each pad button OFF, SHOT, BOMB, SPECIAL or AUTO SHOT (the shot down and up at
 * AUTOFIRE's rate while held); defaults A SHOT, B BOMB, X SPECIAL, RT AUTO SHOT (user 2026-11-01) */
enum { ACT_OFF, ACT_SHOT, ACT_BOMB, ACT_SPECIAL, ACT_AUTO, ACTS };
static const char *const act_names[ACTS] = { "OFF", "SHOT", "BOMB", "SPECIAL", "AUTO SHOT" };
static const char *const key_names[FE_KEYS] = { "A", "B", "X", "Y", "LB", "RB", "LT", "RT" };
static const char *const key_words[FE_KEYS] = { "pad_a", "pad_b", "pad_x", "pad_y", "pad_lb", "pad_rb", "pad_lt",
	"pad_rt" };
enum { AF_RATES = 5 };
static const int af_rates[AF_RATES] = { 30, 20, 15, 12, 10 };
typedef struct { int key[FE_KEYS], af_hz; } Controls;
static const Controls ctl_default = {
	{ ACT_SHOT, ACT_BOMB, ACT_SPECIAL, ACT_OFF, ACT_OFF, ACT_OFF, ACT_OFF, ACT_AUTO }, 30 };
static Controls ctl;
static int af_t;                        /* display frames AUTO SHOT has been held */

uint16_t fe_controls(unsigned down)
{
	static const uint16_t bit[ACTS] = { 0, FE_SHOT, FE_BOMB, FE_SPECIAL, 0 };
	uint16_t o = 0;
	int autoshot = 0;
	for (int k = 0; k < FE_KEYS; k++)
		if (down >> k & 1) {
			o |= bit[ctl.key[k]];
			autoshot |= ctl.key[k] == ACT_AUTO;
		}
	if (autoshot) {
		/* a cycle of 60 / rate frames, the shot down for its first half (rounded down): 30 Hz = down, up, ... */
		const int p = 60 / ctl.af_hz;
		if (af_t % p < p / 2)
			o |= FE_SHOT;
		af_t++;
	} else {
		af_t = 0;
	}
	return o;
}

/* ---- state (spec 0.5's fe struct, the 360's names) ---- */
/* test 9: PG_PLAY (PLAY: START RUN, MODE, CONTINUES, DIP SWITCHES) where test 8 had the N64 OPTIONS page; the
 * values keep their numbers (a state holds page) */
enum { PG_TITLE, PG_PRACTICE, PG_PLAY, PG_END, PG_SETTINGS_UNUSED };
/* the title's menu rows (test 9: ibara360's MENU; test 14: EXIT at the bottom, user: instead of BACK + START held)
 * and the TRAINING page's (test 14, its section below) */
enum { TM_PLAY, TM_TRAINING, TM_LOAD, TM_REPLAYS, TM_OPTIONS, TM_EXIT, TM_ROWS };
enum { TR_START, TR_MODE, TR_PLAYER, TR_STAGE, TR_BOSS, TR_MEDAL, TR_RANK, TR_INV, TR_INFL, TR_INFB, TR_SPEED, TR_HITBOX,
	TR_DIPS, TR_ROWS };
/* test 15: TEAM's values (the arcade's SELECT TYPE choices skipped to, CUSTOM = the PLAYER page's riders) and the
 * PLAYER page's rows */
enum { TT_CHOOSE, TT_SAME, TT_SELECT_TEAM, TT_TEAM_EDIT, TT_RANDOM_EDIT, TT_SELECT_PLAYER, TT_CUSTOM, TT_N };
/* (test 16: a TYPE row under each rider; test 18: TEAM moved here from the TRAINING page, at the top) */
enum { PR_TEAM, PR_RIDER, PR_TYPE, PR_RIDER2, PR_TYPE2, PR_RIDER3, PR_TYPE3, PR_FORM, PR_LIVES, PR_BOMBS, PR_FRAGS, PR_SHOT,
	PR_OPTS, PR_ROWS };
static int tr_cur;
static int tr_quit_title;               /* the training pause's QUIT TO TITLE: the run's end goes to the title's menu */
/* a training run's own (h_credit clears them): P1's flags last frame, the first rider started, the stage byte then,
 * CHASE! lives still to bank, the stage cleared */
static int tr_rider, tr_started, tr_stage0, tr_bank, tr_cleared, tr_play_t;
static int tr_powered;                  /* test 18: the run's first rider start had SHOT / OPTION POWER */
/* test 15: CUSTOM's team: the SELECT TEAM screen confirmed by the host (tr_custom 1), the riders written
 * TR_CUSTOM_LATE frames after the game's team (work/t15/r1/REPORT.md 2: any of F+1..F+19) */
static int tr_custom, tr_custom_t;
enum { TR_FIRST_LATE = 40, TR_CUSTOM_LATE = 5 };
/* the TRAINING page's run values (the game's own start values first) */
enum { TR_LIVES_MIN = 1, TR_LIVES_MAX = 9, TR_BOMBS_MAX = 4, TR_SHOT_MAX = 4, TR_SPEEDS = 3, TR_RANK_GAME = -1,
	TR_CHASE_RANK_MIN = 51, TR_CHASE_RANK_MAX = 76, TR_PILOTS = 18, TR_TYPES = 4, TR_FORMS = 11, TR_FRAGS = 40,
	TR_OPTS_FULL = 3, TR_MEDALS = 19 };
static const int tr_speed_pct[TR_SPEEDS] = { 100, 75, 50 };
static const int tr_medals[TR_MEDALS] = { 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 2000, 3000, 4000, 5000,
	6000, 7000, 8000, 9000, 10000 };
typedef struct {
	int team, stage, boss, lives, bombs, shot, rank, inv, infb, speed;
	int pilot[3], type[3], form, frags, opts;   /* test 15: the PLAYER page's */
	int solo;                           /* test 16: TEAM SELECT PLAYER flies RIDER 1 alone (0 = CHOOSE, the screen) */
	int infl;                           /* test 17: INFINITE LIVES */
	int medal;                          /* test 19: MEDAL, the starting medal value's step (tr_medals) */
} Training;
/* TRAINING's own: 3 lives, 1 bomb, full power, medals at 100; CUSTOM's riders the first team's, type A */
#define TR_DEFAULT { 1, 0, 0, 3, 1, 4, TR_RANK_GAME, 0, 0, 0, { 0, 1, 2 }, { 0, 0, 0 }, 0, 0, TR_OPTS_FULL, 0, 0, 0 }
static Training tr = TR_DEFAULT;
enum { SPL_WAIT, SPL_ON, SPL_DONE };
static int prog_now;                    /* the program running = the MODE of the board */
static int booting;                     /* since a reset, until the attract loop is reached (no credit tests) */
static int splash = SPL_WAIT, splash_t, splash_skipped;
static int active;                      /* title menu owns input (hook 0x1AEA .. 0x1AF0) */
static int page, title_cur, opt_cur, gs_cur, next_page;   /* opt_cur: PLAY's row */
static int phase, prac_target, pulse, pulse_t, launch_t = -1, idle;
static int exit_t = -1;                 /* test 14: frames since the title's EXIT (-1 none) */
static int title_t = -1, title_keep, title_quiet;   /* frames into the title scene; keep showing the kept copy */
static uint32_t title_gen;
static Vdp title_vdp;
static int practice, replay, restart_req, restore_team, team_captured;
static int veil;                        /* black: -1 until a RETRY / RESTART's team is back, then frames left */
static uint16_t team[4];
static int opt_dirty, music_played;
static int invuln, not_kept, snap_taken;
/* test 16: the DEBUG MENU's INFINITE LIVES / INFINITE BOMBS (this session only; set outside a credit, a credit with
 * either on is INVULNERABLE's kind: red, no replay, not in the rankings); test 17: INVINCIBILITY the same way */
static int db_lives, db_bombs, db_inv;
static uint8_t snap[RANK_BYTES];
/* Test 13 (user: "apply the same rules as ibara when it comes to score validity", ibara360 test 117's): red = this
 * credit's score is invalid (a continue, GAME SETTINGS off their defaults in ORIGINAL, TRAINING, INVULNERABLE, RESTART
 * STAGE past stage 1, LOAD STATE): GADGETS' TOTAL red and SAVE REPLAY? still asked, its score red with "*"; the
 * game's own ranking rules stand (not_kept is INVULNERABLE's alone now). rp_drop = this credit gets no replay
 * (TRAINING, INVULNERABLE, RESTART STAGE past stage 1, LOAD STATE, QUIT, FULL RESTART's old run), rp_why the first
 * reason. Both ride in not_kept's bits 1-2 in the states (the layout unchanged). credit_gen counts credits (a FULL
 * RESTART's too) for main.c's new replay; not in the states. */
static int red, rp_drop;
static const char *rp_why = "";
static unsigned credit_gen;
static void state_loaded(int quiet);
static int switch_due, resetting = -1;  /* resetting: board frames since a MODE change's reset (-1 none) */
static int start_due;                   /* test 9: PLAY's START RUN waits for a MODE change's reset (the title hook) */
/* Test 12 (user: "switching between ORIGINAL and CHASE! in PLAY starts the BATRIDER sound"): the reset's title calls
 * "BATRIDER" again (OKI 1, title frames 0..~190: harness work/t12/m). The board is silent from a MODE change's reset
 * until its title has run HUSH_END frames or a credit / the demo begins. */
enum { HUSH_END = 240 };
static int switch_hush;
static int paused, pause_cur, pause_t;
static int pause_armed;                 /* test 9: FULL RESTART / QUIT TO TITLE asked once (not in states: 0 at a load) */
static int host_dirty;

/* Test 8: pages over everything. In a credit the whole board stands while one is open and those frames stay out of
 * the replay (fe_frame's early return), so a page never changes the run. None of them makes a sound for the same
 * reason (an SE would go through the board). Test 9: the pages ibara360 has (OPTIONS and its pages, DIP SWITCHES) are
 * these pages too; B goes back to the page under it (ovl_stack), the last one to the pause menu / the title's / the
 * game. Not in save states or replays. */
enum {
	OV_NONE, OV_OPTIONS, OV_SCREEN, OV_CONTROLS, OV_SOUND, OV_DEBUG, OV_DIPS, OV_STATES, OV_REPLAYS, OV_MANUAL,
	OV_PROMPT, OV_COMMANDS, OV_PLAYER,
};
static int ovl, ovl_stack[8], ovl_depth;
static int ov_cur[OV_PLAYER + 1];                   /* each page's row */
static int ov_armed;                                /* the row asked once (A = YES, B = NO), -1 none */
static char ov_msg[FE_HM_CHARS];                    /* a result in the help's place until the cursor moves */
static int st_save, st_mode;                        /* SAVE / LOAD STATE: which, the MODE shown */
static int rp_first, rp_n;                          /* REPLAY DATA: the first shown, how many */
static int rk_both;                                 /* DEBUG MENU's INITIALIZE RANKINGS: THIS MODE / BOTH MODES */
static int man_pages[2], man_page;
static char prompt_line[3][FE_HM_CHARS];
static int prompt_red;                  /* test 13: the credit's score was invalid (line 2 red, "*") */
static int req_kind, req_arg;                       /* the next FeOut.req */
static int ov_rep[4];                               /* the pages' own repeat (rep_t is the replay's); test 11: LEFT /
                                                     * RIGHT too, as ibara360's */
static int pb_on, pb_paused, pb_cur, pb_banner;     /* a replay playing (fe_playback) */
static int rep_t[4];                    /* auto-repeat counters, up down left right */
/* Test 11 (user: ibara360's menu grammar): a held direction repeats as ibara360's menus do (its test 93, the user's
 * pick: after 0.4 s, then 7 a second); test 10 had the N64's (spec 0.3: after 18 frames every 5) on the title and the
 * pause, every 4 on the pages. */
enum { REP_DELAY = 24, REP_EVERY = 9 };
static int rep_due(int t) { return t >= REP_DELAY && (t - REP_DELAY) % REP_EVERY == 0; }
static int rep_fired;                   /* test 11: a held direction repeated this frame (menu_se) */
static uint32_t frame_no;
static FeOut *cur_out;

/* rankings per MODE (spec 5) */
static uint8_t rk[2][RANK_BYTES], rk_def[RANK_BYTES];
static int rk_have[2], rk_def_ok, rk_load_due, rk_reset_due;

/* ---- small board helpers ---- */
static uint32_t rd32(uint32_t a) { return (uint32_t)br_rd16(a) << 16 | br_rd16(a + 2); }
static void wr32(uint32_t a, uint32_t v) { br_wr16(a, (uint16_t)(v >> 16)); br_wr16(a + 2, (uint16_t)v); }

static void push_return(uint32_t ret)
{
	const uint32_t sp = br_reg(M68K_REG_SP) - 4;
	br_set_reg(M68K_REG_SP, sp);
	wr32(sp, ret);
}

static int in_credit(void)
{
	return !booting && resetting < 0 && !(br_rd8(ATTRACT) & 0x80);
}

static int stage_running(void)
{
	return in_credit() && (br_rd8(IN_PLAY) & 0x80) && !br_rd8(CONT_SCREEN) && !br_rd8(CONT_COUNT);
}

/* test 12: this credit has taken a continue (CONT_USED counts them; read only while P1 plays, its struct being this
 * credit's then). Not TRAINING's free ones. From RAM alone, so states and replays carry it.
 * Test 13: CONT_USED counts CONTINUE? screens opened (0x2FAF8), not continues taken, and a countdown run out with a
 * ranked score puts P1's flag back up for the name entry with CONT_SCREEN still set (work/t12/h2) -> P1 must fly
 * with the screen gone (a continue taken clears it the frame before: work/t12/c). */
static int cont_taken(void)
{
	return !practice && in_credit() && (br_rd16(P1) & 0x8000) && br_rd8(CONT_USED) && !br_rd8(CONT_SCREEN);
}

static void score_red(const char *why)
{
	if (!red)
		LOG("fe: score invalid from board frame %u (%s)", (unsigned)br_frame_count(), why);
	red = 1;
}

static void replay_drop(const char *why)
{
	if (!rp_drop) {
		rp_why = why;
		LOG("fe: no replay for this credit (%s)", why);
	}
	rp_drop = 1;
}

/* a credit (or a FULL RESTART's new run) begins: what it starts with */
static int gs_any(void);
static void credit_flags(void)
{
	const char *cheat = db_lives ? "INFINITE LIVES" : db_bombs ? "INFINITE BOMBS" : db_inv ? "INVINCIBILITY" : "";
	red = practice || invuln || cheat[0] || (prog_now == FE_ORIGINAL && gs_any());
	rp_drop = practice || invuln || cheat[0];
	rp_why = practice ? "TRAINING" : invuln ? "INVULNERABLE" : cheat;
	if (cheat[0]) {
		not_kept = 1;
		LOG("fe: %s ON (score not kept)", cheat);
	}
	credit_gen++;
}

/* ---- sound: the 68000's own routines' latch writes, one a frame (spec 0.2) ---- */
typedef struct { uint8_t se, hi, lo; } Snd;
static Snd sq[8];
static int sq_n, sq_wait;

static void snd_queue(int se, int hi, int lo)
{
	if (sq_n < 8)
		sq[sq_n++] = (Snd){ (uint8_t)se, (uint8_t)hi, (uint8_t)lo };
}
static void music_cmd(int hi, int lo) { snd_queue(0, hi, lo); }

/* Before a frame the board runs: 0x2EF0 (SE, gated by sound on / demo sounds / its per-code wait table) or 0x2E7E
 * (a command: the latch, then the Z80's echo at 0x500008 before the next). */
static void snd_pump(void)
{
	if (!sq_n)
		return;
	if (sq_wait > 0) {
		const Snd *c = &sq[0];
		if (((br_rd16(0x500008) & 0xff) == c->hi && (br_rd16(0x50000A) & 0xff) == c->lo) || --sq_wait == 0) {
			sq_wait = 0;
			memmove(sq, sq + 1, --sq_n * sizeof sq[0]);
		}
		return;
	}
	const Snd c = sq[0];
	if (c.se) {
		memmove(sq, sq + 1, --sq_n * sizeof sq[0]);
		const int a = br_rd8(ATTRACT);
		if (!br_rd8(SOUND_ON) || ((a & 0x80) && !(a & 0x40) && !br_rd8(DEMO_SOUND)) || br_rd8(SE_WAIT + c.lo))
			return;
		br_wr8(SE_WAIT + c.lo, br_rd8(SE_WAIT - 0x100 + c.lo));
		br_wr16(0x500020, 0x0000);
		br_wr16(0x500022, c.lo);
		return;
	}
	br_wr16(0x500020, (uint16_t)c.hi);
	br_wr16(0x500022, c.lo);
	sq_wait = LATCH_WAIT;
}

/* ---- files ---- */
static void save_host(void)
{
	char b[640];
	int n = snprintf(b, sizeof b, "hitbox=%d|vol_music=%d|vol_sfx=%d|attract=%d|mute=%d|language=%d|screen=%d|"
		"gadgets=%d|mode=%d|brightness=%d|smooth=%d|scanlines=%d|strength=%d|picture=%d|aspect=%d|", host.hitbox,
		host.music, host.effects, host.attract, host.mute, host.language, host.screen, host.gadgets, host.mode,
		host.bright, host.smooth, host.scan, host.strength, host.picture, host.aspect);
	for (int k = 0; k < FE_SHAPES; k++)
		n += snprintf(b + n, sizeof b - n, "size%d=%d|xsize%d=%d|ysize%d=%d|xpos%d=%d|ypos%d=%d|", k, host.pic_size[k],
			k, host.pic_sx[k], k, host.pic_sy[k], k, host.pic_x[k], k, host.pic_y[k]);
	b[n++] = '\n';
	if (plat.save)
		plat.save("host.txt", b, n);
	LOG("fe: host settings saved: %.*s", n - 1, b);
}

static int gs_any(void)
{
	return opt.gs[0] | opt.gs[1] | opt.gs[2] | opt.gs[3];
}

static void save_options(void)
{
	char b[400];
	int n = snprintf(b, sizeof b, "continues=%d|sound=0|shot_rate=%d|breakdown=%d|bullets=%d|", opt.continues,
		opt.cmd[0], opt.cmd[1], opt.cmd[2] & ~CMD_SET);
	if (gs_any())
		n += snprintf(b + n, sizeof b - n, "diff=%d|timer=%d|lives=%d|bonus=%d|", opt.gs[0], opt.gs[1], opt.gs[2],
			opt.gs[3]);
	/* test 9: CONTROLS (0 OFF, 1 SHOT, 2 BOMB, 3 SPECIAL, 4 AUTO SHOT) and AUTOFIRE */
	for (int k = 0; k < FE_KEYS; k++)
		n += snprintf(b + n, sizeof b - n, "%s=%d|", key_words[k], ctl.key[k]);
	n += snprintf(b + n, sizeof b - n, "autofire=%d|", ctl.af_hz);
	b[n++] = '\n';
	if (plat.save)
		plat.save("options.txt", b, n);
	LOG("fe: options saved: %.*s", n - 1, b);
}

/* "key=value|..." -> the value of key (or def) */
static int field(const char *s, const char *key, int def, int lo, int hi)
{
	const size_t k = strlen(key);
	for (const char *p = s; (p = strstr(p, key)); p++)
		if ((p == s || p[-1] == '|') && p[k] == '=') {
			int v = 0, neg = p[k + 1] == '-';
			for (const char *q = p + k + 1 + neg; *q >= '0' && *q <= '9'; q++)
				v = v * 10 + *q - '0';
			v = neg ? -v : v;
			return v < lo || v > hi ? def : v;
		}
	return def;
}

static void load_settings(void)
{
	char b[640];
	host = host_default;
	opt = opt_default;
	ctl = ctl_default;
	int n = plat.load ? plat.load("host.txt", b, sizeof b - 1) : -1;
	if (n > 0) {
		b[n] = 0;
		host.hitbox = field(b, "hitbox", 1, 0, HITBOX_MODES - 1);
		/* test 9's file: music / effects in eighths, bright 0..10, scan 0..2 (test 10 writes the new keys) */
		const int m8 = field(b, "music", 8, 0, 8), e8 = field(b, "effects", 8, 0, 8), sc = field(b, "scan", 0, 0, 2);
		host.music = field(b, "vol_music", (m8 * 25 / 2 + 5) / 10 * 10, 0, 100) / 10 * 10;
		host.effects = field(b, "vol_sfx", (e8 * 25 / 2 + 5) / 10 * 10, 0, 100) / 10 * 10;
		host.attract = field(b, "attract", 100, 0, 100) / 10 * 10;
		host.mute = field(b, "mute", 0, 0, 1);
		host.language = field(b, "language", 0, 0, 1);
		host.bright = field(b, "brightness", 50 + field(b, "bright", 5, 0, 10) * 10, BRIGHT_MIN, BRIGHT_MAX) / 5 * 5;
		host.smooth = field(b, "smooth", 0, 0, 1);
		host.scan = field(b, "scanlines", sc != 0, 0, 1);
		host.strength = field(b, "strength", sc == 2 ? 60 : 35, STRENGTH_MIN, STRENGTH_MAX) / 5 * 5;
		host.picture = field(b, "picture", FE_PIC_FULL, FE_PIC_INTEGER, FE_PIC_CUSTOM);
		host.aspect = field(b, "aspect", FE_ASPECT_AUTO, FE_ASPECT_AUTO, FE_ASPECT_4_3);
		for (int k = 0; k < FE_SHAPES; k++) {
			char key[12];
			snprintf(key, sizeof key, "size%d", k);
			host.pic_size[k] = field(b, key, 1000, FE_PIC_SIZE_MIN, FE_PIC_SIZE_MAX);
			snprintf(key, sizeof key, "xsize%d", k);
			host.pic_sx[k] = field(b, key, 1000, FE_PIC_STRETCH_MIN, FE_PIC_STRETCH_MAX);
			snprintf(key, sizeof key, "ysize%d", k);
			host.pic_sy[k] = field(b, key, 1000, FE_PIC_STRETCH_MIN, FE_PIC_STRETCH_MAX);
			snprintf(key, sizeof key, "xpos%d", k);
			host.pic_x[k] = field(b, key, 0, -FE_PIC_POS_MAX, FE_PIC_POS_MAX);
			snprintf(key, sizeof key, "ypos%d", k);
			host.pic_y[k] = field(b, key, 0, -FE_PIC_POS_MAX, FE_PIC_POS_MAX);
		}
		host.screen = field(b, "screen", FE_YOKO, 0, 2);
		host.gadgets = field(b, "gadgets", 1, 0, 1);
		host.mode = field(b, "mode", FE_CHASE, 0, 1);
	}
	n = plat.load ? plat.load("options.txt", b, sizeof b - 1) : -1;
	if (n > 0) {
		b[n] = 0;
		const int c = field(b, "continues", 3, 3, 7);
		opt.continues = c == 5 || c == 7 ? c : 3;
		opt.cmd[0] = field(b, "shot_rate", 0, 0, 8);
		opt.cmd[1] = field(b, "breakdown", 0, 0, 1);
		opt.cmd[2] = CMD_SET | field(b, "bullets", 0, 0, 0777);
		opt.gs[0] = field(b, "diff", 0, 0, 3);
		opt.gs[1] = field(b, "timer", 0, 0, 3);
		opt.gs[2] = field(b, "lives", 0, 0, 3);
		opt.gs[3] = field(b, "bonus", 0, 0, 3);
		/* test 9: CONTROLS; a file of test 8 or before has none: its SHOT / BOMB / SPECIAL buttons (A / B / X) */
		if (field(b, key_words[0], -1, 0, ACTS - 1) < 0) {
			const int m[3] = { field(b, "shot", 0, 0, 2), field(b, "bomb", 1, 0, 2), field(b, "special", 2, 0, 2) };
			if (m[0] != m[1] && m[1] != m[2] && m[0] != m[2])
				for (int i = 0; i < 3; i++)
					ctl.key[FE_K_A + m[i]] = ACT_SHOT + i;
		} else {
			for (int k = 0; k < FE_KEYS; k++)
				ctl.key[k] = field(b, key_words[k], ctl_default.key[k], 0, ACTS - 1);
		}
		const int hz = field(b, "autofire", 30, 10, 30);
		for (int r = 0; r < AF_RATES; r++)
			if (af_rates[r] == hz)
				ctl.af_hz = hz;
	}
}

static const char *const rk_file[2] = { "rankings_chase.bin", "rankings_original.bin" };

static uint32_t fnv(const uint8_t *p, int n)
{
	uint32_t h = 0x811C9DC5u;
	while (n--)
		h = (h ^ *p++) * 0x01000193u;
	return h;
}

/* file: "BRK1", have (1 byte), 0x230 bytes, FNV-1a of the 0x230 (big-endian) */
static void rk_save(int m)
{
	static uint8_t b[4 + 1 + RANK_BYTES + 4];
	memcpy(b, "BRK1", 4);
	b[4] = (uint8_t)rk_have[m];
	memcpy(b + 5, rk[m], RANK_BYTES);
	const uint32_t h = fnv(rk[m], RANK_BYTES);
	b[5 + RANK_BYTES] = (uint8_t)(h >> 24); b[6 + RANK_BYTES] = (uint8_t)(h >> 16);
	b[7 + RANK_BYTES] = (uint8_t)(h >> 8); b[8 + RANK_BYTES] = (uint8_t)h;
	if (plat.save)
		plat.save(rk_file[m], b, sizeof b);
	LOG("fe: %s saved (%s)", rk_file[m], rk_have[m] ? "rankings" : "erased");
}

static void rk_load_files(void)
{
	static uint8_t b[4 + 1 + RANK_BYTES + 4];
	for (int m = 0; m < 2; m++) {
		rk_have[m] = 0;
		const int n = plat.load ? plat.load(rk_file[m], b, sizeof b) : -1;
		if (n != (int)sizeof b || memcmp(b, "BRK1", 4))
			continue;
		const uint32_t h = (uint32_t)b[5 + RANK_BYTES] << 24 | (uint32_t)b[6 + RANK_BYTES] << 16 |
			(uint32_t)b[7 + RANK_BYTES] << 8 | b[8 + RANK_BYTES];
		if (h != fnv(b + 5, RANK_BYTES)) {
			LOG("fe: %s: digest wrong, not used", rk_file[m]);
			continue;
		}
		rk_have[m] = b[4] != 0;
		memcpy(rk[m], b + 5, RANK_BYTES);
	}
}

static void rk_read(uint8_t *d)
{
	for (int t = 0; t < 4; t++)
		for (int i = 0; i < RANK_KEEP; i++)
			d[t * RANK_KEEP + i] = br_rd8(RANK0 + t * RANK_STRIDE + i);
}

static void rk_write(const uint8_t *s)
{
	for (int t = 0; t < 4; t++)
		for (int i = 0; i < RANK_KEEP; i++)
			br_wr8(RANK0 + t * RANK_STRIDE + i, s[t * RANK_KEEP + i]);
}

/* Not-kept credit: the tables back as they were at its start (spec 5.3). */
static void rk_restore_snapshot(void)
{
	if (snap_taken && not_kept) {
		rk_write(snap);
		LOG("fe: rankings put back (the credit's score is not kept)");
	}
	snap_taken = not_kept = 0;
	invuln = 0;
}

/* After a kept name entry the tables differ from the mode's copy: keep them (spec 5.2). */
static void rk_check_save(void)
{
	static uint8_t now[RANK_BYTES];
	if (!rk_def_ok)
		return;
	rk_read(now);
	const uint8_t *ref = rk_have[prog_now] ? rk[prog_now] : rk_def;
	/* entry byte 0x1A bit 7 flips when the 68000 shows a table (PC harness): not a new score */
	int differ = 0;
	for (int i = 0; i < RANK_BYTES && !differ; i++)
		differ = (now[i] ^ ref[i]) & (i % RANK_KEEP % 28 == 0x1A ? 0x7F : 0xFF);
	if (differ) {
		memcpy(rk[prog_now], now, RANK_BYTES);
		rk_have[prog_now] = 1;
		rk_save(prog_now);
	}
}

/* ---- programs, hooks ---- */
static int hex(char c) { return c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10; }
static int hexbytes(const char *s, uint8_t *out)
{
	int n = 0;
	for (; s[0] && s[1]; s += 2)
		out[n++] = (uint8_t)(hex(s[0]) << 4 | hex(s[1]));
	return n;
}

static uint32_t h_splash(void), h_title(void), h_credit(void), h_course(void), h_after_game(void),
	h_setup(void), h_name_entry(void), h_hit(void), h_text_dma(void), h_type(void), h_opts(void), h_power(void);

/* Hook sites: the displaced instruction (checked), the trampoline's copy of it (PC-relative ones made absolute),
 * where the trampoline jumps back to, and the C side. A handler returns the PC to go on at, 0 = the trampoline. */
typedef struct {
	uint32_t site;
	const char *old, *tramp;
	uint32_t back;
	uint32_t (*fn)(void);
} Hook;
static const Hook hooks[] = {
	{ 0x001AD8, "4EB900018070", "4EB900018070", 0x001ADE, h_splash },      /* jsr 0x18070 (the opening) */
	{ 0x001AEA, "4EB9000127F4", "4EB9000127F4", 0x001AF0, h_title },       /* jsr 0x127F4 (the title) */
	{ 0x001AF0, "1039002034C2", "1039002034C2", 0x001AF6, h_credit },      /* move.b 0x2034C2,d0 (credit?) */
	{ 0x001B00, "4EB9000134DC", "4EB9000134DC", 0x001B06, h_course },      /* jsr 0x134DC SELECT COURSE */
	{ 0x001B18, "4EBA0016", "4EB900001B30", 0x001B1C, h_after_game },      /* jsr (pc,0x1B30) */
	{ 0x010A1C, "201D", "201D", 0x010A1E, h_setup },                       /* move.l (a5)+,d0 (player setup) */
	{ 0x013F4E, "303900204062", "303900204062", 0x013F54, h_name_entry },  /* move.w 0x204062,d0 */
	{ 0x02FC1A, "4A390020F9FE", "4A390020F9FE", 0x02FC20, h_hit },         /* tst.b 0x20F9FE (rider hit) */
	{ 0x001C28, "33C000500082", "33C000500082", 0x001C2E, h_text_dma },    /* move.w d0,0x500082 (vblank) */
	{ 0x0132F4, "4A390020FA22", "4A390020FA22", 0x0132FA, h_type },        /* tst.b 0x20FA22 (SELECT TYPE) */
	{ 0x000D48, "1D7C00020104", "1D7C00020104", 0x000D4E, h_opts },        /* move.b #2,(0x104,a6) (TRAINING) */
	{ 0x000D42, "1D7C00030110", "1D7C00030110", 0x000D48, h_power },       /* move.b #3,(0x110,a6) (TRAINING) */
};
enum { NHOOKS = sizeof hooks / sizeof hooks[0] };

static int fe_hook(int opcode)
{
	if (opcode != ILLEGAL)
		return 0;
	const uint32_t ppc = br_reg(M68K_REG_PPC) & 0xFFFFFF;
	if (ppc == CHASEHUD_PRE_SITE && prog_now == FE_CHASE) {
		/* test 11: before h_vbl's jsr chase_frame (chasehud.h), then the jsr */
		const uint32_t to = chasehud_pre();
		push_return(CHASEHUD_PRE_SITE + 6);
		br_set_reg(M68K_REG_PC, to);
		return 1;
	}
	if (ppc == CHASEHUD_VBL_SITE && prog_now == FE_CHASE) {
		/* CHASE!'s h_vbl after its chase_frame: the HUD's host parts from the N64's spot (chasehud.h) */
		chasehud_frame();
		br_set_reg(M68K_REG_PC, CHASEHUD_VBL_NEXT);
		return 1;
	}
	for (int i = 0; i < NHOOKS; i++)
		if (hooks[i].site == ppc) {
			const uint32_t pc = hooks[i].fn();
			br_set_reg(M68K_REG_PC, pc ? pc : tramp_of[i]);
			return 1;
		}
	LOG("fe: illegal instruction at %06X (not a hook)", (unsigned)ppc);
	return 0;
}

static int install_hooks(uint8_t *p, const char *name)
{
	for (uint32_t a = TRAMP; a < TRAMP_END; a++)
		if (p[a]) {
			LOG("fe: %s: the trampoline area %06X is not free (%06X)", name, TRAMP, (unsigned)a);
			return -1;
		}
	uint32_t t = TRAMP;
	for (int i = 0; i < NHOOKS; i++) {
		uint8_t old[16], tr[16];
		const int n = hexbytes(hooks[i].old, old), m = hexbytes(hooks[i].tramp, tr);
		if (memcmp(p + hooks[i].site, old, n)) {
			LOG("fe: %s: hook site %06X holds other bytes", name, (unsigned)hooks[i].site);
			return -1;
		}
		tramp_of[i] = t;
		memcpy(p + t, tr, m);
		t += m;
		const uint8_t jmp[6] = { 0x4E, 0xF9, 0, (uint8_t)(hooks[i].back >> 16), (uint8_t)(hooks[i].back >> 8),
			(uint8_t)hooks[i].back };
		memcpy(p + t, jmp, 6);
		t += 6;
		p[hooks[i].site] = ILLEGAL >> 8;
		p[hooks[i].site + 1] = ILLEGAL & 0xFF;
	}
	return 0;
}

/* LANGUAGE (spec 6.4): the region routine returns 0 (export text) at once for ENGLISH. */
static void apply_language(void)
{
	static const uint8_t en[4] = { 0x70, 0x00, 0x4E, 0x75 }, ja[4] = { 0x10, 0x39, 0x00, 0x20 };
	for (int m = 0; m < 2; m++)
		memcpy(prog[m] + LANG_SITE, host.language ? ja : en, 4);
}

/* MUSIC / EFFECTS (spec 6.3). PC harness (BR_SNDSTAT): the title's tune is OKI 1 alone (the YM is silent there),
 * the select screens' voices OKI 0, play uses all three. So MUSIC = YM2151 + OKI 1, EFFECTS = OKI 0. */
static int vol_set[2] = { -1, -1 };
static const char *gadgets_off_why(void);
static int aspect_wide(void);
static int pic_shape(void);
static uint16_t pad_press, swallow;     /* test 10: this frame's new buttons; ones the game ignores until up */

/* Test 10 (ibara360's SOUND page): volumes in percent; ATTRACT = their share outside a credit (the attract, the title,
 * the demos) except while the SOUND page is open (its tests play at the page's volumes); MUTE = none at all. Called
 * every frame (fe_frame's end) and at once after a change; the board hears only a change. */
static void apply_volumes(void)
{
	const int share = host.mute || switch_hush ? 0 : in_credit() || ovl == OV_SOUND ? 100 : host.attract;
	const int m = host.music * share * 256 / 10000, e = host.effects * share * 256 / 10000;
	if (m == vol_set[0] && e == vol_set[1])
		return;
	vol_set[0] = m;
	vol_set[1] = e;
	br_set_volumes(m, e, m);
}

static uint16_t dsw_for(int m)
{
	return m == FE_CHASE ? DSW_CHASE :
		(uint16_t)(opt.gs[0] << 8 | opt.gs[1] << 10 | opt.gs[2] << 12 | opt.gs[3] << 14 | DSW_ORIGINAL_LOW);
}

/* The other program: reset, into the attract's title at RESET_GOTO (test 2's way); the new MODE's rankings are put
 * in at the title's hook. */
static void switch_program(int m)
{
	if (plat.quiesce)
		plat.quiesce();
	prog_now = m;
	br_set_program(prog[m]);
	br_hold_main(0);
	br_reset();
	chasehud_reset();
	br_set_dips(dsw_for(m), DSWC);
	booting = 1;
	resetting = 0;
	rk_load_due = 1;
	rk_def_ok = 0;
	snap_taken = not_kept = 0;
	sq_n = sq_wait = 0;
	if (splash != SPL_DONE)
		splash = SPL_DONE;
	LOG("fe: MODE %s: board reset with its program", m == FE_CHASE ? "CHASE!" : "ORIGINAL");
}

/* Test 10: a START during the boot reaches the title before the attract's start has put the arcade logo's palettes
 * (84-105, ROM 0x9C666) into the game's unfaded palette copy (0x20E9C8, which the title's fades work from): the
 * arcade logo, drawn since test 10, came out black. They go in here when they are not there (harness work/t10 pc/pf:
 * the copy then equals the splash path's for those palettes). */
enum { PAL_COPY = 0x20E9C8, LOGO_PAL = 84, LOGO_PALS = 22, LOGO_PAL_ROM = 0x9C666 };
/* test 17: the arcade title's copyright text lines (0 = bottom), kept in the title picture */
enum { TITLE_COPY_LO = 3, TITLE_COPY_HI = 5 };

static void logo_palettes(void)
{
	/* Test 12 (hardware: a START in the demo left the logo grey for good): the demo leaves its own colours there, so
	 * the test is "not the ROM's" now, no longer "zero" (harness work/t12/d) */
	const uint32_t dst = PAL_COPY + LOGO_PAL * 32;
	int differ = 0;
	for (int i = 0; i < LOGO_PALS * 32 && !differ; i++)
		differ = br_rd8(dst + i) != prog[prog_now][LOGO_PAL_ROM + i];
	if (!differ)
		return;
	for (int i = 0; i < LOGO_PALS * 32; i++)
		br_wr8(dst + i, prog[prog_now][LOGO_PAL_ROM + i]);
	LOG("fe: the title logo's palettes put in (START during the boot or the demo)");
}

static void goto_title(void)
{
	logo_palettes();
	progpatch_goto_title();
	booting = 0;
	title_t = -1;
	LOG("fe: START -> the title (board frame %u)", (unsigned)br_frame_count());
}

/* ---- hooks ---- */

/* 0x1AD8, before the attract's opening: the COIN-OP GAME STUDIO splash the first time (spec 9.2). The 68000 waits
 * on the hook (fe_frame holds it) until the splash is over. */
static uint32_t h_splash(void)
{
	booting = 0;
	if (splash == SPL_WAIT) {
		splash = SPL_ON;
		splash_t = 0;
		splash_skipped = 0;
		LOG("fe: splash (board frame %u)", (unsigned)br_frame_count());
	}
	return splash == SPL_ON ? 0x1AD8 : 0;
}

/* N64 fn 0x8028D500: front end on. */
static void front_end_on(void)
{
	if (rk_load_due) {
		rk_read(rk_def);                /* the 68000's freshly made tables = the game's own */
		rk_def_ok = 1;
		rk_load_due = 0;
		if (rk_have[prog_now])
			rk_write(rk[prog_now]);
	} else {
		rk_restore_snapshot();
		if (rk_reset_due) {
			rk_write(rk_def);
			rk_reset_due = 0;
		} else {
			rk_check_save();
		}
	}
	veil = 0;
	br_wr8(0x20FD4C, 0);
	br_wr8(0x20FD4D, 0);
	br_wr8(0x20FD42, 0);
	active = 1;
	idle = 0;
	if (!title_quiet) {
		/* (a MODE change's reset on the title keeps the page and a START already chosen) */
		phase = pulse = 0;
		launch_t = -1;
		page = next_page;
		title_t = -1;
	}
	title_quiet = 0;
	next_page = 0;
	if (host.mode != prog_now) {
		switch_due = 1;                 /* a MODE change owed until the title (spec 5.2) */
	} else if (start_due) {
		/* test 9: PLAY's START RUN given while the other MODE's board was coming up (test 14: or TRAINING's, 2) */
		phase = start_due;
		start_due = 0;
		pulse = PULSE;
		pulse_t = 0;
		launch_t = 0;
		LOG("fe: START %s", prog_now == FE_CHASE ? "CHASE!" : "ORIGINAL");
	}
}

static uint32_t h_title(void)
{
	booting = 0;
	if (resetting >= 0) {
		resetting = -1;
		title_quiet = title_keep;
	}
	if (splash != SPL_DONE)
		splash = SPL_DONE;
	front_end_on();
	return 0;
}

/* 0x1AF0: the title returned; a credit when 0x2034C2/C3 say so (spec 1.4 step 1), else the demo. */
static uint32_t h_credit(void)
{
	active = 0;
	title_keep = 0;
	switch_hush = 0;
	if (!(br_rd8(CREDIT_P1) | br_rd8(CREDIT_P2))) {
		LOG("fe: the title timed out: demonstration");
		return 0;
	}
	practice = phase == 2;
	phase = pulse = 0;
	tr_rider = tr_started = tr_bank = tr_cleared = tr_play_t = tr_powered = 0;
	tr_stage0 = -1;
	tr_custom_t = 0;
	if (!practice)
		tr_custom = 0;
	rk_restore_snapshot();
	rk_read(snap);
	snap_taken = 1;
	not_kept = 0;
	invuln = 0;
	credit_flags();
	if (!practice || !replay)
		team_captured = 0;
	if (!practice)
		restore_team = replay = 0;
	br_set_dips(dsw_for(prog_now), DSWC);
	swallow = 0;                    /* (a replay starts with none, as its playback does) */
	LOG("fe: credit: %s%s%s%s%s, DSW %04X (board frame %u)", prog_now == FE_CHASE ? "CHASE!" : "ORIGINAL",
		practice ? " PRACTICE" : "", red && !practice ? " (score invalid: " : "",
		red && !practice ? rp_why[0] ? rp_why : "GAME SETTINGS" : "", red && !practice ? ")" : "", dsw_for(prog_now),
		(unsigned)br_frame_count());
	/* the 68000 reads the DIPs again (0x15304), then the displaced instruction */
	push_return(tramp_of[2]);
	return 0x15304;
}

/* 0x1B00, before SELECT COURSE: a practice's course (spec 1.4 step 2). */
static uint32_t h_course(void)
{
	if (!practice)
		return 0;
	br_wr8(COURSE, (uint8_t)((prac_target + 4) * 2));
	br_wr8(DEMO_NO, (uint8_t)(prac_target - 1));
	if (replay) {
		replay = 0;
		restore_team = 1;
		return 0x1B12;
	}
	if (restore_team)
		return 0x1B12;
	team_captured = 0;
	return 0x1B0C;
}

/* 0x1B18, the game returned: RESTART, a practice's end page, or the name entry (spec 1.6, 2.4). */
static uint32_t h_after_game(void)
{
	if (restart_req) {
		restart_req = 0;
		restore_team = team_captured;
		if (!restore_team)
			veil = 0;
		LOG("fe: RESTART (team %s)", restore_team ? "restored" : "chosen again");
		credit_flags();                 /* test 13: a new run, a new replay (ibara360's FULL RESTART) */
		return 0x1B12;
	}
	if (practice) {
		practice = 0;
		br_wr8(ATTRACT, 0x80);
		/* test 14: the TRAINING page again (START RUN = retry), or the title's menu after QUIT TO TITLE */
		next_page = tr_quit_title ? PG_TITLE : PG_PRACTICE;
		title_cur = TM_TRAINING;
		tr_cur = TR_START;
		LOG("fe: practice over: %s", tr_quit_title ? "the title's menu" : "the TRAINING page");
		tr_quit_title = 0;
		return 0x1AEA;
	}
	return 0;
}

/* The team (spec 1.4 steps 3-4). The N64 keeps the four team words when play starts and, for a REPLAY / RESTART,
 * writes them back at the player-setup command 0x10A1C, sets 0x20E6A0 (team chosen) and goes on at 0x10A68. On the
 * 360 (PC harness, spec unknown 3):
 * - the team words alone are not enough: a credit's start clears the player structs, which the TEAM SELECT builds
 *   (flags, the rider's sprite / hit lists, ...): with the words alone the ship never came. So both player structs
 *   (0x172 bytes each) are kept the moment the team is chosen (0x20E6A0 bit 7, P1 active) and written back.
 * - A5: the command's argument is the long at (A5), the demo branch of the stage script. The command's demo path
 *   (0x10A2C) takes it (A5 = that long), writes the team and runs 0xF1AC (the ship's start, A6 = the player); the
 *   same here (A6 = P1), then 0x10A68. Its credit path (A5 += 4, 0x10614) left the script in the TEAM SELECT branch
 *   with no ship; A5 += 4 into 0x10A68 crashed the 68000. With the demo branch the stage starts at once (no
 *   WELCOME TO wipe), the pad drives the ship, the team is the one chosen.
 * The N64's 0xDAC repaint (team icons) is part of the kept struct here. */
enum { PSTRUCT = 0x172 };
static uint8_t team_ram[2 * PSTRUCT];

static void team_capture(void)
{
	for (int i = 0; i < 2 * PSTRUCT; i++)
		team_ram[i] = br_rd8(P1 + i);
	for (int i = 0; i < 4; i++)
		team[i] = br_rd16(TEAM + i * 2);
	team_captured = 1;
	LOG("fe: team %04X %04X %04X %04X", team[0], team[1], team[2], team[3]);
}

static uint32_t h_setup(void)
{
	if (!restore_team)
		return 0;
	restore_team = 0;
	for (int i = 0; i < 2 * PSTRUCT; i++)
		br_wr8(P1 + i, team_ram[i]);
	br_wr8(IN_PLAY, 0xFF);
	veil = VEIL_AFTER;                  /* the old select screen's tiles show ~10 frames before the stage's fade-in */
	LOG("fe: team put back (%04X %04X %04X %04X), A6 %06X", team[0], team[1], team[2], team[3],
		(unsigned)br_reg(M68K_REG_A6));
	br_set_reg(M68K_REG_A5, rd32(br_reg(M68K_REG_A5)));
	br_set_reg(M68K_REG_A6, P1);
	return 0x10A68;
}

/* Test 15 (work/t15/r1/REPORT.md 1): 0x132F4 = SELECT TYPE's start (jsr from 0x1B0C). A training run whose TEAM is one
 * of its choices skips the screen as its confirm 0x395EA leaves it (0x20E488 = choice x 2, 0x20E6A0 = 2) and returns;
 * CUSTOM goes to SELECT TEAM (confirmed by the host, the riders written after it: tr_frame). */
static int tr_team_eff(void);
static uint32_t h_type(void)
{
	const int t = practice ? tr_team_eff() : TT_CHOOSE;
	if (t < TT_SELECT_TEAM)
		return 0;
	br_wr8(TYPE_MODE, (uint8_t)(t == TT_CUSTOM ? 0 : (t - TT_SELECT_TEAM) * 2));
	br_wr8(IN_PLAY, 2);
	const uint32_t sp = br_reg(M68K_REG_SP);
	br_set_reg(M68K_REG_SP, sp + 4);
	LOG("fe: TRAINING: SELECT TYPE skipped (%02X)", br_rd8(TYPE_MODE));
	return rd32(sp);
}

/* Test 15 (REPORT.md 4): 0xD48, a training course's rider start: option level 2 and four option pickups (0xD4E..0xD54,
 * one f170 each; level L = L + 1 pickups, the guests' third and fourth options with them). OPTION POWER below full:
 * level 0 and only the last tr.opts pickups (none = no options). */
static uint32_t h_opts(void)
{
	if (!practice || tr.opts >= TR_OPTS_FULL)
		return 0;
	br_wr8(br_reg(M68K_REG_A6) + PL_OPTLV, 0);
	return 0xD56 - 2 * (uint32_t)tr.opts;
}

/* Test 18 (user: "shot power was set to 1, yet my rider spawned with shot power 4"; "upon death, shot power and option
 * power should decrease normally"): 0xD42, the training course's forced shot 3 at every rider start. The run's first
 * start gets SHOT POWER here, in the routine that would overwrite it (tr_frame's write a frame edge away could come
 * before it), then h_opts' OPTION POWER; every later start skips 0xD42-0xD54 as a course below 6 does, so a lost
 * rider's power drops as in the game. */
static uint32_t h_power(void)
{
	if (!practice)
		return 0;
	if (tr_powered)
		return 0xD56;
	tr_powered = 1;
	const uint32_t a6 = br_reg(M68K_REG_A6);
	br_wr8(a6 + PL_SHOT, (uint8_t)(tr.shot - 1));
	br_wr8(a6 + PL_SHOTP, 0);
	LOG("fe: TRAINING: SHOT POWER %d, OPTION POWER %d at the first rider start", tr.shot, tr.opts);
	return 0xD48;
}

/* 0x13F4E, "anyone to enter a name?": not for a credit whose score is not kept (spec 5.3). */
static uint32_t h_name_entry(void)
{
	if (not_kept) {
		br_wr16(P1, 0);
		br_wr16(P2, 0);
		rk_restore_snapshot();
		LOG("fe: no name entry (score not kept)");
	}
	return 0;
}

/* 0x1C28, the vblank's text DMA: in a credit P2's HUD cells are cleared first (one player; N64 fn 0x802968B8,
 * else PRESS START / TO PLAY stays on lines 37/38). */
static uint32_t h_text_dma(void)
{
	if (!booting && resetting < 0 && !(br_rd8(ATTRACT) & 0x80))
		for (uint32_t a = 0x203DF6; a <= 0x203E0B; a++)
			br_wr8(a, 0);
	return 0;
}

/* 0x2FC1A, a rider hit: INVULNERABLE takes the arcade's own invincibility path (spec 2.5); test 14: TRAINING's
 * INVINCIBLE in a training run too; test 17: the DEBUG MENU's INVINCIBILITY in a credit (not a replay's playback) */
static uint32_t h_hit(void)
{
	return invuln || (practice && tr.inv) || (db_inv && !pb_on && in_credit()) ? 0x2FC24 : 0;
}

/* ---- init ---- */
int fe_init(const FePlatform *p, uint8_t *progs[2])
{
	plat = *p;
	prog[0] = progs[0];
	prog[1] = progs[1];
	if (install_hooks(prog[FE_CHASE], "CHASE!") || install_hooks(prog[FE_ORIGINAL], "ORIGINAL"))
		return -1;
	if (chasehud_patch(prog[FE_CHASE]))
		LOG("fe: CHASE!'s vblank site for the HUD holds other bytes: no CHASE! HUD extras");
	load_settings();
	rk_load_files();
	apply_language();
	apply_volumes();
	br_set_hook(fe_hook);
	prog_now = host.mode;
	br_set_program(prog[prog_now]);
	br_reset();
	chasehud_reset();
	br_set_dips(dsw_for(prog_now), DSWC);
	booting = 1;
	rk_load_due = 1;
	LOG("fe: %d hooks in both programs; MODE %s, SCREEN %d, GADGETS %d, continues %d, commands %d %d %X, settings %d%d%d%d",
		NHOOKS, prog_now == FE_CHASE ? "CHASE!" : "ORIGINAL", host.screen, host.gadgets, opt.continues, opt.cmd[0],
		opt.cmd[1], opt.cmd[2], opt.gs[0], opt.gs[1], opt.gs[2], opt.gs[3]);
	return 0;
}

/* ---- text pages into the job's overlay (spec 0.1: line 0 = bottom, col 0..29) ---- */
static FeVideoJob *J;
static int sel_pal;                     /* the "selected" palette: the title's pink on title pages, 0x34 in play */

static void ov_clear(void)
{
	memset(J->ov, 0, sizeof J->ov);
	J->text = 1;
}

static void put(int line, int col, const char *s, int pal, int opaque)
{
	for (; *s; s++, col++)
		if (col >= 0 && col < FE_COLS && line >= 0 && line < FE_LINES)
			J->ov[col][line] = OV_SET | (opaque ? OV_OPAQUE : 0) | (uint32_t)(pal << 10 | (uint8_t)*s);
}

static const char *prac_name(int i)
{
	static char b[16];
	const uint32_t o = (uint32_t)prog[prog_now][PRACTICE_TABLE + 2 + i * 2] << 8 | prog[prog_now][PRACTICE_TABLE + 3 + i * 2];
	int n = 0;
	for (const uint8_t *s = prog[prog_now] + PRACTICE_TABLE + o; *s && n < 15; s++)
		b[n++] = (char)*s;
	b[n] = 0;
	return b;
}

static const char *mode_word(int m) { return m == FE_CHASE ? "CHASE!" : "ORIGINAL"; }

/* test 9: a menu's title names the game first, as ibara360's do ("IBARA  OPTIONS", two spaces) */
static const char *game_word(int m) { return m == FE_CHASE ? "BATRIDER CHASE!" : "BATRIDER"; }

/* ---- test 9: the menu box (fe.h FeRow; fe_video's hm_draw draws it) ---- */
static void m_begin(int w, int vcol, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
static void m_begin(int w, int vcol, const char *fmt, ...)
{
	FeVideoJob *j = J;
	j->hm = 1;
	j->hm_w = w;
	j->hm_vcol = vcol;
	j->hm_plain = j->hm_top = 0;
	j->hm_n = 0;
	j->hm_sel = -1;
	j->hm_corner[0] = 0;
	j->hm_help[0] = 0;
	j->hm_help_c = FE_C_CYAN;
	j->hm_nl = 0;
	j->pic = NULL;
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(j->hm_title, FE_HM_CHARS, fmt, ap);
	va_end(ap);
}

/* a row: sel = the chosen one (yellow, '>'), grey = it cannot act now, arrows = LEFT / RIGHT change its value */
static FeRow *m_row(const char *name, const char *value, int sel, int grey, int arrows)
{
	FeVideoJob *j = J;
	if (j->hm_n >= FE_HM_ROWS)
		return &j->hm_row[FE_HM_ROWS - 1];
	FeRow *r = &j->hm_row[j->hm_n];
	snprintf(r->name, sizeof r->name, "%s", name);
	snprintf(r->value, sizeof r->value, "%s", value ? value : "");
	r->name_c = grey ? FE_C_DIM : sel ? FE_C_YELLOW : FE_C_WHITE;
	r->value_c = grey ? FE_C_DIM : sel ? FE_C_YELLOW : FE_C_GREEN;
	r->arrows = (uint8_t)arrows;
	if (sel)
		j->hm_sel = j->hm_n;
	j->hm_n++;
	return r;
}

static void m_help(int c, const char *s)
{
	snprintf(J->hm_help, sizeof J->hm_help, "%s", s);
	J->hm_help_c = (uint8_t)c;
}

static void m_line(int c, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void m_line(int c, const char *fmt, ...)
{
	FeVideoJob *j = J;
	if (j->hm_nl >= FE_HM_LINES)
		return;
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(j->hm_line[j->hm_nl], FE_HM_CHARS, fmt, ap);
	va_end(ap);
	j->hm_line_c[j->hm_nl++] = (uint8_t)c;
}

static int in_pause(void)
{
	return paused && in_credit();
}

/* what B goes back to: the page under this one, else the pause menu / the title's page / the game */
static const char *under_word(void)
{
	if (ovl_depth > 0)
		return ovl_stack[ovl_depth - 1] == OV_OPTIONS ? "OPTIONS" : "MENU";
	if (in_pause())
		return "PAUSE MENU";
	if (active && launch_t < 0)
		return page == PG_PLAY ? "PLAY" : page == PG_PRACTICE ? "TRAINING" : "MENU";
	return "CLOSE";
}

static void m_back(void)
{
	m_line(FE_C_GREEN, "B / BACK = %s", under_word());
}

/* ---- pages over everything (test 8; test 9: a stack, B = the one under it) ---- */
static void settings_flush(void)
{
	if (host_dirty)
		save_host();
	if (opt_dirty)
		save_options();
	host_dirty = opt_dirty = 0;
}

static void ovl_open(int which)
{
	if (ovl && ovl_depth < (int)(sizeof ovl_stack / sizeof ovl_stack[0]))
		ovl_stack[ovl_depth++] = ovl;
	ovl = which;
	ov_rep[0] = ov_rep[1] = 0;
	ov_armed = -1;
	ov_msg[0] = 0;
}

static void ovl_back(void)
{
	ovl = ovl_depth > 0 ? ovl_stack[--ovl_depth] : OV_NONE;
	ov_rep[0] = ov_rep[1] = 0;
	ov_armed = -1;
	ov_msg[0] = 0;
	if (!ovl) {
		settings_flush();
		LOG("fe: pages closed");
	}
}

static void ovl_close_all(void)
{
	ovl_depth = 0;
	ovl_back();
}

/* The pages' up / down (the replay's repeat counters rep_t are left alone: pages are not in replays). */
static uint16_t ov_dirs(const FePad *pad)
{
	uint16_t r = 0;
	for (int i = 0; i < 4; i++) {
		const uint16_t bit = (uint16_t)(1 << i);
		if (!(pad->held & bit)) {
			ov_rep[i] = 0;
			continue;
		}
		if (pad->press & bit)
			r |= bit;
		else if (rep_due(ov_rep[i]))
			r |= bit, rep_fired = 1;
		ov_rep[i]++;
	}
	return r;
}

/* up / down over n rows on page p (the result: whether it moved) */
static int ov_move(int p, uint16_t d, int n)
{
	if (!(d & (FE_UP | FE_DOWN)) || n <= 0)
		return 0;
	ov_cur[p] = (ov_cur[p] + (d & FE_UP ? n - 1 : 1)) % n;
	ov_armed = -1;
	ov_msg[0] = 0;
	return 1;
}

static void open_states(int save)
{
	st_save = save;
	st_mode = prog_now;
	ov_cur[OV_STATES] = 0;
	ovl_open(OV_STATES);
	LOG("fe: %s STATE page", save ? "SAVE" : "LOAD");
}

static void open_replays(void)
{
	ov_cur[OV_REPLAYS] = rp_first = 0;
	ovl_open(OV_REPLAYS);
}

static int open_manual(void)
{
	const int n = man_pages[host.language];
	if (!n)
		return 0;
	if (man_page >= n)
		man_page = 0;
	ovl_open(OV_MANUAL);
	LOG("fe: MANUAL (%s, page %d)", host.language ? "JA" : "EN", man_page + 1);
	return 1;
}

/* ---- the title's menu (test 9: ibara360's MENU rows) ---- */

static void draw_title_page(void)
{
	static const char *const names[TM_ROWS] = { "PLAY", "TRAINING", "LOAD STATE", "REPLAY DATA", "OPTIONS", "EXIT" };
	/* (test 18: two lines of 18 letters at the plain menu's 2x; the user's PLAY line, the others cut to fit) */
	static const char *const helps[TM_ROWS] = { "START A RUN", "PRACTICE A STAGE\nOR A BOSS FIGHT",
		"CONTINUE FROM A\nSAVE STATE", "WATCH RECORDED\nREPLAYS", "SCREEN, GADGETS,\nSOUND, CONTROLS",
		"BACK TO THE\nDASHBOARD" };
	if (launch_t < 0 && exit_t < 0 && !ovl) {
		m_begin(168, 0, "%s", "");
		J->hm_plain = 1;
		J->hm_top = 140;
		for (int i = 0; i < TM_ROWS; i++)
			m_row(names[i], NULL, i == title_cur, 0, 0);
		m_help(FE_C_CYAN, helps[title_cur]);
		m_line(FE_C_GREEN, "A = CONFIRM");
	}
	/* test 17: the arcade's copyright lines are back above (fe_video), the hack's credit under them */
	put(1, 4, "CHASE HACK BY", P_WHITE, 0);
	put(1, 18, "COIN-OP", P_BLUE, 0);
}

/* ---- PLAY (the title's page): START RUN, MODE, CONTINUES, DIP SWITCHES ---- */
enum { PL_START, PL_MODE, PL_CONT, PL_DIPS, PL_ROWS };

static void play_build(void)
{
	char v[16];
	const int dips = host.mode == FE_ORIGINAL, starting = start_due || switch_due || resetting >= 0;
	m_begin(184, 13, "%s  PLAY", game_word(host.mode));
	m_row("START RUN", NULL, opt_cur == PL_START, 0, 0);
	m_row("MODE", mode_word(host.mode), opt_cur == PL_MODE, 0, 1);
	snprintf(v, sizeof v, "%d", opt.continues);
	m_row("CONTINUES", v, opt_cur == PL_CONT, 0, 1);
	FeRow *r = m_row("DIP SWITCHES", dips && gs_any() ? "CHANGED" : NULL, opt_cur == PL_DIPS, !dips, 0);
	if (dips && gs_any() && opt_cur != PL_DIPS)
		r->value_c = FE_C_RED;
	static const char *const helps[PL_ROWS] = { "PLAY WITH THE SETTINGS BELOW",
		"CHASE! OR THE ORIGINAL GAME\nSCORES KEPT FOR EACH MODE", "CONTINUES AFTER A GAME OVER",
		"DIFFICULTY, TIMER, LIVES,\nBONUS LIFE" };
	m_help(FE_C_CYAN, opt_cur == PL_START && starting ? "CHANGING THE MODE..." :
		opt_cur == PL_DIPS && !dips ? "THE ORIGINAL GAME'S ONLY" : helps[opt_cur]);
	m_line(FE_C_GREEN, "UP/DOWN ROW, LEFT/RIGHT SET");
	m_line(FE_C_GREEN, "%s", opt_cur == PL_START ? "A = CONFIRM" : opt_cur == PL_DIPS ? "A = OPEN" : "A = NEXT VALUE");
	m_line(FE_C_GREEN, "B / BACK = MENU");
}

static void start_pulse(void)
{
	pulse = PULSE;
	pulse_t = 0;
	launch_t = 0;
}

static void play_start(void)
{
	settings_flush();
	page = PG_TITLE;
	title_cur = TM_PLAY;
	if (switch_due || resetting >= 0) {
		start_due = 1;                  /* the title hook starts it, once the other MODE's board is up */
		LOG("fe: START %s once its board is up", mode_word(host.mode));
		return;
	}
	phase = 1;
	start_pulse();
	LOG("fe: START %s", mode_word(prog_now));
}

static void play_input(int up, int down, int left, int right, uint16_t press)
{
	const int a = press & FE_A;
	if (up) { opt_cur = (opt_cur + PL_ROWS - 1) % PL_ROWS; }
	if (down) { opt_cur = (opt_cur + 1) % PL_ROWS; }
	if (press & (FE_B | FE_BACK)) {
		settings_flush();
		page = PG_TITLE;
		return;
	}
	if ((press & FE_START) || (a && opt_cur == PL_START)) {
		play_start();
		return;
	}
	switch (opt_cur) {
	case PL_MODE:
		if (left || right || a) {
			/* the board takes the other program at once (switch_due; the title stays, title_keep) */
			host.mode = !host.mode;
			host_dirty = 1;
			switch_due = host.mode != prog_now;
			LOG("fe: MODE %s", mode_word(host.mode));
		}
		break;
	case PL_CONT:
		if (left || right || a) {
			static const int nx[8] = { 0, 0, 0, 5, 0, 7, 0, 3 }, pv[8] = { 0, 0, 0, 7, 0, 3, 0, 5 };
			opt.continues = left ? pv[opt.continues] : nx[opt.continues];
			opt_dirty = 1;
		}
		break;
	case PL_DIPS:
		if (a && host.mode == FE_ORIGINAL) {
			ov_cur[OV_DIPS] = 0;
			ovl_open(OV_DIPS);
		}
		break;
	}
}

/* ---- TRAINING ---- */
/* Test 14 (user: "remake the training menu in the same style" as ibara360's): one page, ibara360's TRAINING rows in
 * its grammar: START RUN, the run's MODE and TEAM, STAGE and START AT BOSS (the N64 PRACTICE's 25 entries: a stage's
 * start or one of its bosses, the secret bosses as their own STAGE), the run's LIVES / BOMBS / SHOT POWER / RANK
 * (written by tr_frame), INVINCIBLE, INFINITE BOMBS, GAME SPEED, HITBOX (the DEBUG MENU's setting) and DIP SWITCHES.
 * START plays from any row; A on a value row is its next value, LB / RB RANK in tens. A run's GAME OVER, its stage's
 * end and the training pause's QUIT TO TRAINING bring the page back (test 9's RETRY / SELECT STAGE page is gone:
 * START RUN is the retry). The page's values stay for the session and go into save states. */
_Static_assert((int)TR_ROWS <= (int)FE_HM_ROWS, "the TRAINING page's rows fit the box");
/* the practice table's entries (work/t14/r1/REPORT.md 6): each stage's start; its bosses, the secret ones whose scripts
 * lie in that stage after its own (HIGH WAY's ENVY / SOBUT / BLUNT take turns there by the course's stage order);
 * TR_EXTRA = the bosses with maps of their own */
enum { TR_STAGES = 7, TR_EXTRA = TR_STAGES, TR_BOSS_MAX = 5 };
static const uint8_t tr_stage_entry[TR_STAGES] = { 0, 2, 4, 6, 8, 12, 14 };
static const uint8_t tr_boss_list[TR_STAGES + 1][TR_BOSS_MAX] = { { 1 }, { 3, 16 }, { 5, 19 }, { 7, 22 },
	{ 9, 10, 11, 17, 18 }, { 13 }, { 15 }, { 20, 21, 23, 24 } };
static const uint8_t tr_boss_n[TR_STAGES + 1] = { 1, 2, 2, 2, 5, 1, 1, 4 };

/* the values as one int for save states (fe_state_io) and the menu sounds' signature: 4 bits each, TR_PACKED set */
enum { TR_PACKED = 1 << 30 };
static int tr_pack(void)
{
	/* (test 15: TEAM SAME keeps test 14's bit 24, the other TEAM values bits 26-28; the PLAYER page's riders, formation,
	 * fragments and options stay out: no save states in TRAINING since test 15) */
	return TR_PACKED | tr.stage | tr.boss << 4 | tr.lives << 8 | tr.bombs << 12 | tr.shot << 16 | tr.speed << 20 |
		tr.inv << 22 | tr.infb << 23 | (tr.team == TT_SAME) << 24 | tr_quit_title << 25 |
		(tr.team == TT_SAME ? 0 : tr.team) << 26 | tr.infl << 29;
}

static void tr_unpack(int a, int rank)
{
	tr.stage = a & 15;
	tr.boss = a >> 4 & 15;
	tr.lives = a >> 8 & 15;
	tr.bombs = a >> 12 & 15;
	tr.shot = a >> 16 & 15;
	tr.speed = a >> 20 & 3;
	tr.inv = a >> 22 & 1;
	tr.infb = a >> 23 & 1;
	tr.infl = a >> 29 & 1;
	tr.team = a >> 24 & 1 ? TT_SAME : (a >> 26 & 7) < TT_N ? a >> 26 & 7 : TT_CHOOSE;
	tr_quit_title = a >> 25 & 1;
	tr.rank = rank;
}

/* the practice entry STAGE / START AT BOSS name */
static int tr_entry(void)
{
	if (tr.stage == TR_EXTRA)
		return tr_boss_list[TR_EXTRA][tr.boss];
	return tr.boss ? tr_boss_list[tr.stage][tr.boss - 1] : tr_stage_entry[tr.stage];
}

/* START AT BOSS's choices for the STAGE (OFF first, except for the EXTRA BOSSES) */
static int tr_bosses(void)
{
	return tr.stage == TR_EXTRA ? tr_boss_n[TR_EXTRA] : tr_boss_n[tr.stage] + 1;
}

/* LIVES' most: 4 riders (TEAMSZ 3) in the original game, CHASE!'s extends bank on top */
static int tr_lives_max(void)
{
	return host.mode == FE_CHASE ? TR_LIVES_MAX : 4;
}

/* the values the MODE allows (CHASE!'s rank routine holds rank between 51% and 76%) */
static void tr_clamp(void)
{
	if (tr.lives > tr_lives_max())
		tr.lives = tr_lives_max();
	if (host.mode == FE_CHASE && tr.rank != TR_RANK_GAME)
		tr.rank = tr.rank < TR_CHASE_RANK_MIN ? TR_CHASE_RANK_MIN : tr.rank > TR_CHASE_RANK_MAX ? TR_CHASE_RANK_MAX :
			tr.rank;
}

/* test 15: TEAM SAME before a team was flown is the game's SELECT TYPE (CHOOSE) */
static int tr_team_eff(void)
{
	return tr.team == TT_SAME && !team_captured ? TT_CHOOSE : tr.team;
}

/* Test 16 (user: "player select ... should disable rider 2 and 3 completely"): TEAM SELECT PLAYER with a RIDER
 * picked = that rider (and TYPE) in every slot, as the arcade's SELECT PLAYER copies its pilot (REPORT.md 1, 0x8288):
 * its screen confirmed by the host and the words written after it, CUSTOM's way (tr_custom_team) */
static int tr_solo(void)
{
	return tr_team_eff() == TT_SELECT_PLAYER && tr.solo;
}

static int tr_row_grey(int i)
{
	return i == TR_DIPS && host.mode != FE_ORIGINAL;
}

/* test 15: the riders by the SELECT PLAYER screen's names (work/t15/r1/REPORT.md 2); the Battle Garegga guests 9-12 by
 * their machines (two pilots each) */
static const char *const tr_pilot_names[TR_PILOTS] = { "D.D", "SHORTY", "JYUJI", "MARIA", "GOLDEN", "STRAWMAN",
	"BIRTHDAY", "TAG-T", "ADAM", "SILVER SWORD", "GRASSHOPPER", "FLYING BARON", "WILD SNAIL", "GAIN", "CHITTA",
	"MIYAMOTO", "BORNNAM", "CARPET" };
static const char *const tr_team_names[TT_N] = { "CHOOSE", "SAME", "SELECT TEAM", "TEAM EDIT", "RANDOM EDIT",
	"SELECT PLAYER", "CUSTOM" };
/* FORMATION: DEFAULT (the game's), the five a guest's C and commands give, the five special ones its option items give
 * after the hidden counters (REPORT.md 3); +0x22's values */
static const char *const tr_form_names[TR_FORMS] = { "DEFAULT", "V", "BACK", "FRONT", "CIRCLE", "CONTROL", "SECRET 1",
	"SECRET 2", "SECRET 3", "SECRET 4", "SECRET 5" };
enum { TR_GUEST_FIRST = 9, TR_GUEST_LAST = 12 };

static void tr_build(void)
{
	char v[TR_ROWS][28];
	memset(v, 0, sizeof v);
	const int starting = start_due || switch_due || resetting >= 0;
	m_begin(228, 17, "%s  TRAINING", game_word(host.mode));
	snprintf(v[TR_MODE], sizeof v[0], "%s", mode_word(host.mode));
	snprintf(v[TR_STAGE], sizeof v[0], "%s", tr.stage == TR_EXTRA ? "EXTRA BOSSES" :
		prac_name(tr_stage_entry[tr.stage]));
	snprintf(v[TR_BOSS], sizeof v[0], "%s", tr.stage != TR_EXTRA && !tr.boss ? "OFF" : prac_name(tr_entry()));
	snprintf(v[TR_MEDAL], sizeof v[0], "%d", tr_medals[tr.medal]);
	if (tr.rank == TR_RANK_GAME)
		snprintf(v[TR_RANK], sizeof v[0], "DEFAULT");
	else
		snprintf(v[TR_RANK], sizeof v[0], "%d%%", tr.rank);
	snprintf(v[TR_INV], sizeof v[0], "%s", tr.inv ? "ON" : "OFF");
	snprintf(v[TR_INFL], sizeof v[0], "%s", tr.infl ? "ON" : "OFF");
	snprintf(v[TR_INFB], sizeof v[0], "%s", tr.infb ? "ON" : "OFF");
	snprintf(v[TR_SPEED], sizeof v[0], "%d%%", tr_speed_pct[tr.speed]);
	snprintf(v[TR_HITBOX], sizeof v[0], "%s", hitbox_names[host.hitbox]);
	if (host.mode == FE_ORIGINAL && gs_any())
		snprintf(v[TR_DIPS], sizeof v[0], "CHANGED");
	static const char *const names[TR_ROWS] = { "START RUN", "MODE", "PLAYER EDIT", "STAGE", "START AT BOSS", "MEDAL", "RANK",
		"INVINCIBLE", "INFINITE LIVES", "INFINITE BOMBS", "GAME SPEED", "HITBOX", "DIP SWITCHES" };
	for (int i = 0; i < TR_ROWS; i++) {
		FeRow *r = m_row(names[i], v[i][0] ? v[i] : NULL, i == tr_cur, tr_row_grey(i), v[i][0] && i != TR_DIPS);
		if (i == TR_DIPS && v[i][0] && tr_cur != i)
			r->value_c = FE_C_RED;
	}
	static const char *const helps[TR_ROWS] = { "PLAY WITH THE SETTINGS BELOW", "CHASE! OR THE ORIGINAL GAME",
		"TEAM, RIDERS, FORMATION, LIVES,\nBOMBS, SHOT AND OPTION POWER", "THE STAGE TO TRAIN ON",
		"OFF = FROM THE STAGE'S START\nOR STRAIGHT TO ONE OF ITS BOSSES", "STARTING MEDAL VALUE",
		"DEFAULT = THE GAME'S OWN\nOR HELD AT A PERCENTAGE (LB/RB 10%)",
		"NO DAMAGE", "THE TEAM NEVER RUNS OUT", "THE BOMBS NEVER RUN OUT", "SLOW THE GAME DOWN\n(THE MUSIC KEEPS ITS SPEED)",
		"OFF, THE RIDERS' HITBOX,\nOR EVERY HITBOX", "DIFFICULTY, TIMER, LIVES,\nBONUS LIFE" };
	m_help(FE_C_CYAN, tr_cur == TR_START && starting ? "CHANGING THE MODE..." :
		tr_cur == TR_DIPS && host.mode != FE_ORIGINAL ? "THE ORIGINAL GAME'S ONLY" :
		tr_cur == TR_RANK && host.mode == FE_CHASE ? "DEFAULT = THE GAME'S OWN, OR HELD\nAT 51-76% (CHASE!'S OWN LIMITS)" :
		helps[tr_cur]);
	m_line(FE_C_GREEN, "UP/DOWN ROW, LEFT/RIGHT SET");
	m_line(FE_C_GREEN, "%s", tr_cur == TR_DIPS || tr_cur == TR_PLAYER ? "A = OPEN, START = PLAY" : "START = PLAY");
	m_line(FE_C_GREEN, "B / BACK = MENU");
}

/* ---- the PLAYER page (test 15, user: "a player customization mini window for all the extra options"): CUSTOM's three
 * riders (LEFT/RIGHT the rider, LB/RB / A its type A-D = the button that confirms it in the arcade: SHOT, BOMB, C,
 * START), the Garegga guests' FORMATION, and the run's LIVES / BOMBS / BOMB FRAGMENTS / SHOT POWER / OPTION POWER.
 * A rider changed makes TEAM CUSTOM.
 * Test 16 (user: "player type should be its own toggle"): a TYPE row under each rider, LEFT/RIGHT like every row (the
 * colours and the speed: 0xD84[type] +0, -0x10, -0x20, +0x20). With TEAM SELECT PLAYER the first pair is RIDER (CHOOSE
 * = the arcade's screen, or one rider for every life) and TYPE, riders 2 and 3 dimmed and skipped. ---- */
static int pr_off(int i)
{
	if (tr_team_eff() != TT_SELECT_PLAYER)
		return 0;
	return (i >= PR_RIDER2 && i <= PR_TYPE3) || (i == PR_TYPE && !tr.solo);
}

static void player_build(void)
{
	if (pr_off(ov_cur[OV_PLAYER]))
		ov_cur[OV_PLAYER] = PR_TEAM;
	const int cur = ov_cur[OV_PLAYER], team = tr_team_eff(), sp = team == TT_SELECT_PLAYER;
	char v[PR_ROWS][28];
	snprintf(v[PR_TEAM], sizeof v[0], "%s", tr_team_names[team]);
	for (int k = 0; k < 3; k++) {
		snprintf(v[PR_RIDER + 2 * k], sizeof v[0], "%s", sp && !k && !tr.solo ? "CHOOSE" : tr_pilot_names[tr.pilot[k]]);
		snprintf(v[PR_TYPE + 2 * k], sizeof v[0], "%c", 'A' + tr.type[k]);
	}
	for (int i = 0; i < PR_ROWS; i++)
		if (pr_off(i))
			snprintf(v[i], sizeof v[0], "-");
	snprintf(v[PR_FORM], sizeof v[0], "%s", tr_form_names[tr.form]);
	snprintf(v[PR_LIVES], sizeof v[0], "%d", tr.lives);
	snprintf(v[PR_BOMBS], sizeof v[0], "%d", tr.bombs);
	snprintf(v[PR_FRAGS], sizeof v[0], "%d/%d", tr.frags, TR_FRAGS);
	snprintf(v[PR_SHOT], sizeof v[0], "%d", tr.shot);
	snprintf(v[PR_OPTS], sizeof v[0], tr.opts ? "%d" : "NONE", tr.opts);
	m_begin(228, 17, "%s  PLAYER EDIT", game_word(host.mode));
	static const char *const names[PR_ROWS] = { "TEAM", "RIDER 1", "TYPE 1", "RIDER 2", "TYPE 2", "RIDER 3", "TYPE 3",
		"FORMATION", "LIVES", "BOMBS", "BOMB FRAGMENTS", "SHOT POWER", "OPTION POWER" };
	const int custom = tr_team_eff() == TT_CUSTOM || sp;
	for (int i = 0; i < PR_ROWS; i++) {
		FeRow *r = m_row(sp && i == PR_RIDER ? "RIDER" : sp && i == PR_TYPE ? "TYPE" : names[i], v[i], cur == i,
			pr_off(i), !pr_off(i));
		if (i >= PR_RIDER && i <= PR_TYPE3 && !custom && cur != i)
			r->value_c = FE_C_DIM;
	}
	/* (test 18: SHOT and OPTION POWER at the run's first rider start only, h_power) */
	static const char *const helps[PR_ROWS] = { "", "", "", "", "", "", "",
		"GAREGGA SHIPS ONLY",
		"LIVES AT THE START", "BOMBS AT EACH RIDER'S START", "40 FRAGMENTS MAKE A BOMB\n(AT EACH RIDER'S START)",
		"SHOT POWER AT THE START", "NONE, OR THE OPTIONS' LEVEL" };
	static const char *const team_helps[TT_N] = { "THE GAME'S SELECT TYPE SCREEN",
		"THE LAST TEAM FLOWN", "STRAIGHT TO SELECT TEAM", "STRAIGHT TO TEAM EDIT", "STRAIGHT TO RANDOM EDIT",
		"STRAIGHT TO SELECT PLAYER", "THE RIDERS BELOW" };
	if (cur == PR_TEAM)
		m_help(FE_C_CYAN, tr_solo() ? "THE RIDER BELOW FOR EVERY LIFE\n(THE GAME'S SELECT PLAYER)" : team_helps[team]);
	else if (sp && cur == PR_RIDER)
		m_help(FE_C_CYAN, "CHOOSE = THE GAME'S SCREEN,\nOR THIS RIDER FOR EVERY LIFE");
	else if (cur >= PR_RIDER && cur <= PR_TYPE3 && ((cur - PR_RIDER) & 1))
		m_help(FE_C_CYAN, custom ? "COLOURS AND SPEED: A NORMAL,\nB SLOWER, C SLOWEST, D FASTEST" :
			"COLOURS AND SPEED (A-D)\n(SETS TEAM TO CUSTOM)");
	else if (cur >= PR_RIDER && cur <= PR_TYPE3)
		m_help(FE_C_CYAN, custom ? "THE TEAM'S RIDERS IN ORDER" : "THE TEAM'S RIDERS IN ORDER\n(SETS TEAM TO CUSTOM)");
	else if (cur == PR_LIVES && host.mode == FE_ORIGINAL)
		m_help(FE_C_CYAN, "LIVES AT THE START\n(4 AT MOST IN THE ORIGINAL GAME)");
	else
		m_help(FE_C_CYAN, helps[cur]);
	m_line(FE_C_GREEN, "%s", cur == PR_FRAGS ? "LEFT/RIGHT 1, LB/RB 10" : "LEFT/RIGHT CHANGE");
	m_line(FE_C_GREEN, "START = PLAY");
	m_back();
}

static int wrap(int v, int d, int n) { return ((v + d) % n + n) % n; }

static void player_input(const FePad *pad)
{
	const uint16_t press = pad->press, d = ov_dirs(pad);
	if (press & (FE_B | FE_BACK)) {
		ovl_back();
		return;
	}
	/* (the dimmed rows skipped; a cursor left on one by a TEAM change goes to the first row) */
	if (pr_off(ov_cur[OV_PLAYER]))
		ov_cur[OV_PLAYER] = PR_TEAM;
	if (ov_move(OV_PLAYER, d, PR_ROWS)) {
		while (pr_off(ov_cur[OV_PLAYER]))
			ov_move(OV_PLAYER, d, PR_ROWS);
		return;
	}
	const int cur = ov_cur[OV_PLAYER], lr = d & FE_LEFT ? -1 : d & FE_RIGHT ? 1 : 0;
	const int lbrb = press & FE_LB ? -1 : press & FE_RB ? 1 : 0, s = lr ? lr : press & FE_A ? 1 : 0;
	if (cur == PR_TEAM) {
		/* test 18: the TRAINING page's TEAM row (test 15: SAME only once a team was flown) */
		if (!s)
			return;
		tr.team = wrap(tr_team_eff(), s, TT_N);
		if (tr.team == TT_SAME && !team_captured)
			tr.team = wrap(tr.team, s, TT_N);
		return;
	}
	if (cur <= PR_TYPE3) {
		const int k = (cur - PR_RIDER) / 2;
		if (!s)
			return;
		if (cur == PR_RIDER && tr_team_eff() == TT_SELECT_PLAYER) {
			/* CHOOSE, then the 18 riders */
			const int n = wrap(tr.solo ? tr.pilot[0] + 1 : 0, s, TR_PILOTS + 1);
			tr.solo = n > 0;
			if (n)
				tr.pilot[0] = n - 1;
			return;
		}
		if ((cur - PR_RIDER) & 1)
			tr.type[k] = wrap(tr.type[k], s, TR_TYPES);
		else
			tr.pilot[k] = wrap(tr.pilot[k], s, TR_PILOTS);
		if (tr_team_eff() != TT_SELECT_PLAYER)
			tr.team = TT_CUSTOM;
		return;
	}
	switch (cur) {
	case PR_FORM: tr.form = wrap(tr.form, s, TR_FORMS); break;
	case PR_LIVES: tr.lives = tr.lives + s < TR_LIVES_MIN ? tr_lives_max() : tr.lives + s > tr_lives_max() ?
		TR_LIVES_MIN : tr.lives + s; break;
	case PR_BOMBS: tr.bombs = wrap(tr.bombs, s, TR_BOMBS_MAX + 1); break;
	case PR_FRAGS:
		if (lbrb)
			tr.frags = tr.frags + 10 * lbrb < 0 ? 0 : tr.frags + 10 * lbrb > TR_FRAGS - 1 ? TR_FRAGS - 1 :
				tr.frags + 10 * lbrb;
		else
			tr.frags = wrap(tr.frags, s, TR_FRAGS);
		break;
	case PR_SHOT: tr.shot = tr.shot + s < 1 ? TR_SHOT_MAX : tr.shot + s > TR_SHOT_MAX ? 1 : tr.shot + s; break;
	case PR_OPTS: tr.opts = wrap(tr.opts, s, TR_OPTS_FULL + 1); break;
	}
}

/* LIVES at the run's first rider start (REPORT.md 1): lives = the team entries from the flying pilot's up to TEAM_LAST.
 * The team as chosen is C (flying), A, B with TEAM_LAST 2: 1 = A|1, B|1, C (A and B shot down); 2 = A|1, C, B; 3 as it
 * is; 4 = TEAM_LAST 3 and a fourth pilot; more (CHASE!) the extends bank once its state runs (tr_frame). */
static void tr_lives(void)
{
	uint16_t t[4];
	for (int k = 0; k < 4; k++)
		t[k] = br_rd16(TEAM + k * 2);
	if (br_rd8(TEAM_LAST) != 2 || br_rd16(P1 + PL_PILOT) != t[0]) {
		LOG("fe: TRAINING: LIVES left alone (team %04X %04X %04X, pilot %04X, last %d)", t[0], t[1], t[2],
			br_rd16(P1 + PL_PILOT), br_rd8(TEAM_LAST));
		return;
	}
	const uint16_t c = t[0], a = t[1], b = t[2];
	if (tr.lives == 1) {
		t[0] = a | 1; t[1] = b | 1; t[2] = c;
	} else if (tr.lives == 2) {
		t[0] = a | 1; t[1] = c; t[2] = b;
	} else if (tr.lives >= 4) {
		/* the fourth: the first pilot id not in the team (test 16: a one-rider team's own pilot) */
		uint16_t x = c;
		for (int id = 0; id < 16 && (c != a || c != b); id++) {
			x = (uint16_t)(id << 4);
			if ((c & 0xFFF0) != x && (a & 0xFFF0) != x && (b & 0xFFF0) != x)
				break;
		}
		t[3] = x;
		br_wr8(TEAM_LAST, 3);
		tr_bank = tr.lives - 4;
	}
	for (int k = 0; k < 4; k++)
		br_wr16(TEAM + k * 2, t[k]);
	LOG("fe: TRAINING: %d lives (team %04X %04X %04X %04X)", tr.lives, t[0], t[1], t[2], t[3]);
}

/* the run's values (work/t14/r1/REPORT.md), each frame of a training run outside the pause and the pages: at every
 * rider start (P1's flags turn 0x8E in play) SHOT POWER and BOMBS (TRAINING's courses give full power there), at the
 * first LIVES; INFINITE BOMBS keeps 4; a RANK percentage held (RANK_CAP kept above it: the game clamps to it); the stage
 * cleared (the course plays its entry twice: the stage byte steps between) ends the run as the pause's QUIT does */
/* INFINITE BOMBS: 4 kept (test 16: the HUD's bombs repainted when one was used, REPORT.md 5 bit 7) */
static void inf_bombs(void)
{
	if (br_rd8(P1 + PL_BOMBS) == 4)
		return;
	br_wr8(P1 + PL_BOMBS, 4);
	br_wr8(P1 + PL_REPAINT, (uint8_t)(br_rd8(P1 + PL_REPAINT) | 0x80));
}

/* test 15: TEAM CUSTOM's riders over the team SELECT TEAM gave (REPORT.md 2: the words written between the confirm and
 * the rider start make the ships, shots and HUD icons; the kept struct of TEAM SAME gets them too) */
static void tr_custom_team(void)
{
	const int last = br_rd8(TEAM_LAST);
	for (int k = 0; k < 3 && k <= last; k++) {
		const int s = tr_solo() ? 0 : k;   /* test 16: SELECT PLAYER's rider in every slot */
		const uint16_t w = (uint16_t)(tr.pilot[s] << 4 | tr.type[s] << 2);
		br_wr16(TEAM + k * 2, w);
		team[k] = w;
		team_ram[TEAM - P1 + k * 2] = (uint8_t)(w >> 8);
		team_ram[TEAM - P1 + k * 2 + 1] = (uint8_t)w;
	}
	tr_custom = 0;
	veil = VEIL_AFTER;
	LOG("fe: TRAINING: CUSTOM team %04X %04X %04X", team[0], team[1], team[2]);
}

static void inf_lives(void);
static void tr_frame(void)
{
	const int f = br_rd8(P1), in_play = br_rd8(IN_PLAY) == 0xFF;
	tr_play_t = in_play ? tr_play_t + 1 : 0;
	if (tr_custom && in_play && ++tr_custom_t == TR_CUSTOM_LATE)
		tr_custom_team();
	/* (TEAM SAME puts back a flying rider's struct: no 0x8E then, the first start is TR_FIRST_LATE frames into play) */
	if ((f == 0x8E && tr_rider != 0x8E && in_play) || (!tr_started && (f & 0x80) && tr_play_t == TR_FIRST_LATE)) {
		/* (test 18: SHOT POWER at the first start only, also h_power's; TEAM SAME's put-back struct skips 0xD42) */
		if (!tr_started) {
			br_wr8(P1 + PL_SHOT, (uint8_t)(tr.shot - 1));
			br_wr8(P1 + PL_SHOTP, 0);
			if (f != 0x8E)
				tr_powered = 1;     /* (TEAM SAME: no rider start ran; its later ones are the game's) */
		}
		br_wr8(P1 + PL_BOMBS, (uint8_t)tr.bombs);
		/* test 15: BOMB FRAGMENTS, the HUD's bombs and gauge repainted (REPORT.md 5); a guest's FORMATION */
		br_wr8(P1 + PL_GAUGE, (uint8_t)tr.frags);
		br_wr8(P1 + PL_REPAINT, (uint8_t)(br_rd8(P1 + PL_REPAINT) | 0xC0));
		const int id = br_rd16(P1 + PL_PILOT) >> 4 & 31;
		if (tr.form && id >= TR_GUEST_FIRST && id <= TR_GUEST_LAST)
			br_wr16(P1 + PL_FORM, (uint16_t)(2 * (tr.form - 1)));
		if (!tr_started) {
			tr_started = 1;
			br_wr8(MEDAL, (uint8_t)(2 * tr.medal));   /* test 19: MEDAL (the run's own from here) */
			tr_lives();
		}
	}
	tr_rider = f;
	if (!tr_started)
		return;
	if (tr_bank > 0 && prog_now == FE_CHASE && br_rd16(CS_STATE) == 2) {
		br_wr16(CS_BANK, (uint16_t)(br_rd16(CS_BANK) + tr_bank));
		LOG("fe: TRAINING: %d lives in CHASE!'s bank", tr_bank);
		tr_bank = 0;
	}
	if (tr.infb && (f & 0x80))
		inf_bombs();
	if (tr.infl && (f & 0x80))
		inf_lives();                    /* test 17: TRAINING's INFINITE LIVES (the DEBUG MENU's way) */
	if (tr.rank != TR_RANK_GAME && (f & 0x80)) {
		const uint32_t r = 0xFF0000u / 100 * (uint32_t)(100 - tr.rank);
		br_wr16(RANK_NOW, (uint16_t)(r >> 16));
		br_wr16(RANK_NOW + 2, (uint16_t)r);
		if (rd32(RANK_CAP) < r) {
			br_wr16(RANK_CAP, (uint16_t)(r >> 16));
			br_wr16(RANK_CAP + 2, (uint16_t)r);
		}
	}
	/* (the stage byte is 0 until the stage begins, then 1: the first stage seen is the one played) */
	const int st = br_rd8(STAGE_NO) & 0x7F;
	if (tr_stage0 <= 0)
		tr_stage0 = st;
	else if (!tr_cleared && st != tr_stage0) {
		tr_cleared = 1;
		br_wr8(CONT_COUNT, 0xFF);
		LOG("fe: TRAINING: the stage cleared (board frame %u): the run ends", (unsigned)br_frame_count());
	}
}

/* Test 16: the DEBUG MENU's INFINITE LIVES / INFINITE BOMBS, each frame of a credit while P1 flies (not in a replay's
 * playback). Lives (work/t14/r1/REPORT.md 1, 0x2FA32): a death marks the first team word equal to the flying pilot's
 * (bit 0) and the next word flies; it is GAME OVER only when that word is the last (TEAMSZ). So: no word marked, and
 * a pilot flying from the last slot only is moved to the front (the others after it, their order kept). */
static void inf_lives(void)
{
	const int last = br_rd8(TEAM_LAST);
	if (last < 1 || last > 3)
		return;
	uint16_t t[4];
	int changed = 0, k = 0;
	for (int i = 0; i <= last; i++) {
		t[i] = br_rd16(TEAM + i * 2);
		if (t[i] != 0xFFFF && (t[i] & 1)) {
			t[i] &= (uint16_t)~1;
			changed = 1;
		}
	}
	const uint16_t cur = br_rd16(P1 + PL_PILOT);
	while (k <= last && t[k] != cur)
		k++;
	if (k == last) {
		for (int i = last; i > 0; i--)
			t[i] = t[i - 1];
		t[0] = cur;
		changed = 1;
	}
	if (!changed)
		return;
	for (int i = 0; i <= last; i++)
		br_wr16(TEAM + i * 2, t[i]);
	br_wr8(P1 + PL_REPAINT, (uint8_t)(br_rd8(P1 + PL_REPAINT) | 0x10));   /* the team icons */
}

static void db_frame(void)
{
	if (pb_on || !(br_rd8(P1) & 0x80))
		return;
	if (db_bombs)
		inf_bombs();
	if (db_lives)
		inf_lives();
}

/* the training run starts (START RUN, or START on any row): test 9's RETRY with SAME, the team select with CHOOSE */
static void tr_start(void)
{
	settings_flush();
	prac_target = tr_entry();
	replay = tr_team_eff() == TT_SAME;
	tr_custom = tr_team_eff() == TT_CUSTOM || tr_solo();   /* test 15: black until its riders are in (tr_custom_team) */
	veil = replay || tr_custom ? -1 : 0;
	tr_quit_title = 0;
	page = PG_TITLE;
	title_cur = TM_TRAINING;
	if (switch_due || resetting >= 0) {
		start_due = 2;                  /* the title hook starts it, once the other MODE's board is up */
		LOG("fe: TRAINING %s once its board is up", mode_word(host.mode));
		return;
	}
	phase = 2;
	start_pulse();
	LOG("fe: TRAINING %d %s (%s; team %s%s, riders %d%c %d%c %d%c, form %d, lives %d, bombs %d, frags %d, shot %d, "
		"opts %d, rank %d, inv %d, infl %d, infb %d, speed %d%%, hitbox %d)", prac_target, prac_name(prac_target),
		mode_word(prog_now), tr_team_names[tr_team_eff()], tr_solo() ? " (RIDER 1 ALONE)" : "", tr.pilot[0], 'A' + tr.type[0], tr.pilot[1], 'A' + tr.type[1],
		tr.pilot[2], 'A' + tr.type[2], tr.form, tr.lives, tr.bombs, tr.frags, tr.shot, tr.opts, tr.rank, tr.inv,
		tr.infl, tr.infb, tr_speed_pct[tr.speed], host.hitbox);
}

/* a value row's step d (+1 RIGHT / A, -1 LEFT; RANK +-10 for LB / RB) */
static void tr_step(int row, int d)
{
	switch (row) {
	case TR_MODE:
		host.mode = !host.mode;
		host_dirty = 1;
		switch_due = host.mode != prog_now;
		tr_clamp();
		LOG("fe: MODE %s", mode_word(host.mode));
		break;
	case TR_STAGE:
		tr.stage = (tr.stage + TR_STAGES + 1 + d) % (TR_STAGES + 1);
		tr.boss = 0;
		break;
	case TR_BOSS: tr.boss = (tr.boss + tr_bosses() + d) % tr_bosses(); break;
	case TR_MEDAL: tr.medal = (tr.medal + TR_MEDALS + d) % TR_MEDALS; break;
	case TR_RANK:
		/* DEFAULT one step below 0%, the ends held (ibara360's tr_rank_step) */
		if (tr.rank == TR_RANK_GAME)
			tr.rank = d > 0 ? 0 : TR_RANK_GAME;
		else if (d < 0 && tr.rank == 0)
			tr.rank = TR_RANK_GAME;
		else if (d < 0 && host.mode == FE_CHASE && tr.rank == TR_CHASE_RANK_MIN)
			tr.rank = TR_RANK_GAME;
		else
			tr.rank = tr.rank + d < 0 ? 0 : tr.rank + d > 100 ? 100 : tr.rank + d;
		tr_clamp();
		break;
	case TR_INV: tr.inv = !tr.inv; break;
	case TR_INFL: tr.infl = !tr.infl; break;
	case TR_INFB: tr.infb = !tr.infb; break;
	case TR_SPEED: tr.speed = (tr.speed + TR_SPEEDS + d) % TR_SPEEDS; break;
	case TR_HITBOX:
		host.hitbox = (host.hitbox + HITBOX_MODES + d) % HITBOX_MODES;
		host_dirty = 1;
		break;
	}
}

static void tr_input(int up, int down, int left, int right, uint16_t press)
{
	const int a = press & FE_A;
	if (up) { tr_cur = (tr_cur + TR_ROWS - 1) % TR_ROWS; }
	if (down) { tr_cur = (tr_cur + 1) % TR_ROWS; }
	if (press & (FE_B | FE_BACK)) {
		settings_flush();
		page = PG_TITLE;
		return;
	}
	if ((press & FE_START) || (a && tr_cur == TR_START)) {
		tr_start();
		return;
	}
	if (tr_cur == TR_DIPS) {
		if (a && host.mode == FE_ORIGINAL) {
			ov_cur[OV_DIPS] = 0;
			ovl_open(OV_DIPS);
		}
		return;
	}
	if (tr_cur == TR_PLAYER) {
		if (a)
			ovl_open(OV_PLAYER);
		return;
	}
	if (tr_cur == TR_RANK && (press & (FE_LB | FE_RB)))
		tr_step(TR_RANK, press & FE_LB ? -10 : 10);
	else if (tr_cur == TR_MEDAL && (press & (FE_LB | FE_RB)))   /* (ibara360's tr_quick: 2 of its 19 values) */
		tr_step(TR_MEDAL, press & FE_LB ? -2 : 2);
	else if (left || right || a)
		tr_step(tr_cur, left ? -1 : 1);
}

/* ---- OPTIONS and its pages (over the title or the pause menu) ---- */
enum { O_SCREEN, O_GADGETS, O_CONTROLS, O_COMMANDS, O_SOUND, O_MANUAL, O_DEBUG, O_LANG, O_IDS };

/* the rows there are now: DEBUG MENU outside a credit (as ibara360's pause OPTIONS has none) */
static int opt_rows(int *id)
{
	int n = 0;
	for (int i = 0; i < O_IDS; i++)
		if (i != O_DEBUG || !in_credit())
			id[n++] = i;
	return n;
}

static void options_build(void)
{
	static const char *const names[O_IDS] = { "GENERAL SCREEN", "GADGETS", "CONTROLS", "SECRET COMMANDS", "SOUND",
		"MANUAL", "DEBUG MENU", "LANGUAGE" };
	int id[O_IDS];
	const int n = opt_rows(id);
	int *cur = &ov_cur[OV_OPTIONS];
	if (*cur >= n)
		*cur = n - 1;
	if (paused)
		m_begin(184, 16, "PAUSE  OPTIONS");
	else
		m_begin(184, 16, "%s  OPTIONS", game_word(prog_now));
	for (int i = 0; i < n; i++) {
		const int r = id[i], sel = i == *cur;
		if (r == O_GADGETS)
			m_row(names[r], host.gadgets ? "ON" : "OFF", sel, 0, 1)->value_c =
				host.gadgets && gadgets_off_why()[0] && !sel ? FE_C_DIM : sel ? FE_C_YELLOW : FE_C_GREEN;
		else if (r == O_LANG)
			m_row(names[r], host.language ? "JAPANESE" : "ENGLISH", sel, in_credit(), 1);
		else
			m_row(names[r], NULL, sel, r == O_MANUAL && !man_pages[host.language], 0);
	}
	const char *help = "";
	switch (id[*cur]) {
	case O_SCREEN: help = "CUSTOMIZE GAME SCREEN"; break;
	case O_GADGETS: help = "ORIGINAL WORK BY BAF,\nOLIFANTE AND DUBS"; break;
	case O_CONTROLS: help = "CUSTOMIZE CONTROLS"; break;
	/* (test 15: lines of at most 27 letters, or the page misses 1.5x in OPTIONS' box and drops to 1x) */
	case O_COMMANDS: help = "BULLET COLOURS, SHOT RATE,\nSCORE BREAKDOWN"; break;
	case O_SOUND: help = in_credit() ? "VOLUMES, MUTE" : "VOLUMES, MUTE, SOUND TEST"; break;
	case O_MANUAL:
		help = !man_pages[host.language] ? "NO BATRIDER360_MANUAL.BIN\nBESIDE THE XEX" :
			host.language ? "THE JAPANESE MANUAL" : "THE ENGLISH MANUAL";
		break;
	case O_DEBUG: help = "HITBOX, LIVES, BOMBS,\nRANKINGS, DEFAULTS"; break;
	case O_LANG: help = in_credit() ? "NOT DURING A CREDIT" : "THE GAME'S TEXTS AND\nTHE MANUAL"; break;
	}
	m_help(FE_C_CYAN, help);
	if (id[*cur] == O_GADGETS && host.gadgets && gadgets_off_why()[0])
		m_line(FE_C_DIM, "%s", gadgets_off_why());
	m_line(FE_C_GREEN, "%s", id[*cur] == O_GADGETS || id[*cur] == O_LANG ? "LEFT/RIGHT CHANGE" : "A = CONFIRM");
	m_back();
}

static void options_input(const FePad *pad)
{
	const uint16_t press = pad->press, d = ov_dirs(pad);
	int id[O_IDS];
	const int n = opt_rows(id);
	int *cur = &ov_cur[OV_OPTIONS];
	if (*cur >= n)
		*cur = n - 1;
	if (press & (FE_B | FE_BACK)) {
		ovl_back();
		return;
	}
	if (ov_move(OV_OPTIONS, d, n))
		return;
	const int a = press & FE_A, any = a || (d & (FE_LEFT | FE_RIGHT));
	switch (id[*cur]) {
	case O_SCREEN: if (a) { ov_cur[OV_SCREEN] = 0; ovl_open(OV_SCREEN); } break;
	case O_CONTROLS: if (a) { ov_cur[OV_CONTROLS] = 0; ovl_open(OV_CONTROLS); } break;
	case O_COMMANDS: if (a) { ov_cur[OV_COMMANDS] = 0; ovl_open(OV_COMMANDS); } break;
	case O_SOUND: if (a) { ov_cur[OV_SOUND] = 0; ovl_open(OV_SOUND); } break;
	case O_DEBUG: if (a) { ov_cur[OV_DEBUG] = 0; rk_both = 0; ovl_open(OV_DEBUG); } break;
	case O_MANUAL: if (a) open_manual(); break;
	case O_GADGETS:
		if (any) { host.gadgets = !host.gadgets; host_dirty = 1; }
		break;
	case O_LANG:
		/* not in a credit: the program's text routine changes (a replay holds the LANGUAGE it began with) */
		if (any && !in_credit()) { host.language = !host.language; apply_language(); host_dirty = 1; }
		break;
	}
}

/* ---- SECRET COMMANDS (test 14, user: the game's own START commands as menu rows; work/t14/r2/REPORT.md B) ----
 * In play the arcade takes START held + a press: RIGHT / LEFT / DOWN the colour of the normal bullets, the ball bullets
 * and the missiles; A / C its autofire faster / slower; B the score breakdown. The rows set them for every credit (the
 * game's own value with DEFAULT, test 15's name for test 14's GAME), the commands still work in play. SHOT RATE changes the run, so it is grey in a credit
 * (a replay keeps the value it began with); the others only change the look. None touches score, rank or rankings. */
enum { SC_NML, SC_BAL, SC_MSL, SC_RATE, SC_BREAKDOWN, SC_ROWS };
enum { BULLET_COLOURS = 7, SHOT_RATES = 8, NML_COLOUR = 0x20FD52, PL_RATE = 0x10C, PL_FLAGS2 = 1 };
static const char *const colour_names[BULLET_COLOURS] = { "YELLOW", "BLUE", "GREEN", "PURPLE", "PINK", "RED", "ORANGE" };
static const char *const rate_names[SHOT_RATES] = { "60", "30", "20", "15", "12", "10", "8", "7" };

static int cmd_colour(int k) { return opt.cmd[2] & CMD_SET ? opt.cmd[2] >> (3 * k) & 7 : 0; }

static void commands_build(void)
{
	const int cur = ov_cur[OV_COMMANDS];
	char v[SC_ROWS][28];
	if (paused)
		m_begin(228, 17, "PAUSE  SECRET COMMANDS");
	else
		m_begin(228, 17, "%s  SECRET COMMANDS", game_word(prog_now));
	for (int k = 0; k < 3; k++) {
		const int c = cmd_colour(k);
		if (c)
			snprintf(v[k], sizeof v[0], "%s", colour_names[c - 1]);
		else
			snprintf(v[k], sizeof v[0], "DEFAULT");
	}
	const int rate = opt.cmd[2] & CMD_SET ? opt.cmd[0] : 0;
	if (rate)
		snprintf(v[SC_RATE], sizeof v[0], "%s A SECOND", rate_names[rate - 1]);
	else
		snprintf(v[SC_RATE], sizeof v[0], "DEFAULT");
	snprintf(v[SC_BREAKDOWN], sizeof v[0], "%s", opt.cmd[2] & CMD_SET && opt.cmd[1] ? "ON" : "OFF");
	static const char *const names[SC_ROWS] = { "NORMAL BULLETS", "BALL BULLETS", "MISSILES", "SHOT RATE",
		"SCORE BREAKDOWN" };
	for (int i = 0; i < SC_ROWS; i++)
		m_row(names[i], v[i], cur == i, i == SC_RATE && in_credit(), 1);
	static const char *const helps[SC_ROWS] = { "NORMAL BULLETS, DEFAULT = BLUE\n(START + RIGHT IN PLAY)",
		"BALL BULLETS, DEFAULT = ORANGE\n(START + LEFT IN PLAY)", "MISSILES, DEFAULT = PINK\n(START + DOWN IN PLAY)",
		"YOUR AUTOFIRE, SHOTS A SECOND\n(START + A / C IN PLAY)", "EACH SCORE ADDED, LISTED ON\nTHE LEFT (START + B IN PLAY)" };
	m_help(FE_C_CYAN, cur == SC_RATE && in_credit() ? "NOT DURING A CREDIT\n(START + A / C IN PLAY)" : helps[cur]);
	m_line(FE_C_GREEN, "LEFT/RIGHT CHANGE");
	m_back();
}

static void commands_input(const FePad *pad)
{
	const uint16_t press = pad->press, d = ov_dirs(pad);
	if (press & (FE_B | FE_BACK)) {
		ovl_back();
		return;
	}
	if (ov_move(OV_COMMANDS, d, SC_ROWS))
		return;
	const int a = press & FE_A, step = d & FE_LEFT ? -1 : 1, cur = ov_cur[OV_COMMANDS];
	if (!a && !(d & (FE_LEFT | FE_RIGHT)))
		return;
	if (!(opt.cmd[2] & CMD_SET))
		opt.cmd[0] = opt.cmd[1] = 0, opt.cmd[2] = CMD_SET;
	if (cur <= SC_MSL) {
		const int c = (cmd_colour(cur) + BULLET_COLOURS + 1 + step) % (BULLET_COLOURS + 1);
		opt.cmd[2] = (opt.cmd[2] & ~(7 << (3 * cur))) | c << (3 * cur);
	} else if (cur == SC_RATE) {
		if (in_credit())
			return;
		opt.cmd[0] = (opt.cmd[0] + SHOT_RATES + 1 + step) % (SHOT_RATES + 1);
	} else {
		opt.cmd[1] = !opt.cmd[1];
	}
	opt_dirty = 1;
}

/* each frame of a credit (not the pages, not the pause): the colours while in play (the game puts its own back at
 * every play start), the shot rate and the breakdown at each rider's arrival (its flags turn 0x8E) */
static int cmd_last[2];

static void commands_frame(void)
{
	if (!(opt.cmd[2] & CMD_SET))
		return;
	if (br_rd8(IN_PLAY) == 0xFF)
		for (int k = 0; k < 3; k++)
			if (cmd_colour(k))
				br_wr8(NML_COLOUR + 2 * k, (uint8_t)(cmd_colour(k) - 1));
	const uint32_t players[2] = { P1, P2 };
	for (int k = 0; k < 2; k++) {
		const uint32_t p = players[k];
		const int f = br_rd8(p);
		if (f == 0x8E && cmd_last[k] != 0x8E) {
			if (opt.cmd[0])
				br_wr8(p + PL_RATE, (uint8_t)(opt.cmd[0] - 1));
			if (opt.cmd[1] && !(br_rd8(p + PL_FLAGS2) & 0x04))
				br_wr8(p + PL_FLAGS2, (uint8_t)(br_rd8(p + PL_FLAGS2) | 0x0C));
		}
		cmd_last[k] = f;
	}
}

/* GENERAL SCREEN. Test 10 (user: "use more of ibara's grammar and menus for other missing features like 4:3 support,
 * picture resizing"): ibara360's rows in its order: PICTURE SIZE (INTEGER / FULL / CUSTOM), CUSTOM's SIZE, X SIZE,
 * Y SIZE, X POSITION, Y POSITION (kept per display shape; a change makes PICTURE SIZE CUSTOM; LEFT / RIGHT held
 * repeats, faster after a second; A = the row's default), ROTATION, SCANLINES, SCAN STRENGTH, BRIGHTNESS (5% steps),
 * SMOOTHING, ASPECT RATIO (AUTO = the console's). */
enum { GS_PICTURE, GS_SIZE, GS_XSIZE, GS_YSIZE, GS_XPOS, GS_YPOS, GS_ROT, GS_SCAN, GS_STRENGTH, GS_BRIGHT, GS_SMOOTH,
	GS_ASPECT, GS_ROWS };
static const char *const rot_names[3] = { "OFF", "LEFT", "RIGHT" };
static const char *const pic_names[3] = { "INTEGER", "FULL", "CUSTOM" };
enum { PIC_REPEAT = 20, PIC_FAST = 60 };
static int disp_wide = 1;               /* the console's display shape (fe_display) */
static int pic_held;                    /* frames LEFT / RIGHT held on a picture row */

void fe_display(int wide)
{
	disp_wide = wide != 0;
}

static int aspect_wide(void)
{
	return host.aspect == FE_ASPECT_AUTO ? disp_wide : host.aspect == FE_ASPECT_16_9;
}

/* the display shape a CUSTOM picture is kept for: 16:9, 4:3, tate on 16:9, tate on 4:3 */
static int pic_shape(void)
{
	return (host.screen != FE_YOKO) * 2 + !aspect_wide();
}

/* why GADGETS cannot show now ("" = they can): 16:9 with INTEGER or FULL only (ibara360's rule for CUSTOM) */
static const char *gadgets_off_why(void)
{
	return !aspect_wide() ? "OFF: NO ROOM IN 4:3" : host.picture == FE_PIC_CUSTOM ? "OFF: PICTURE SIZE IS CUSTOM" : "";
}

static int *pic_val(int row, int *lo, int *hi, int *step, int *fast, int *def)
{
	const int sh = pic_shape();
	*def = 1000;
	*step = 5;                          /* 0.5% */
	*fast = 20;
	*lo = FE_PIC_STRETCH_MIN;
	*hi = FE_PIC_STRETCH_MAX;
	switch (row) {
	case GS_SIZE:
		*lo = FE_PIC_SIZE_MIN;
		*hi = FE_PIC_SIZE_MAX;
		return &host.pic_size[sh];
	case GS_XSIZE: return &host.pic_sx[sh];
	case GS_YSIZE: return &host.pic_sy[sh];
	default:                            /* the positions: 720ths of the screen's height */
		*def = 0;
		*step = 1;
		*fast = 6;
		*lo = -FE_PIC_POS_MAX;
		*hi = FE_PIC_POS_MAX;
		return row == GS_XPOS ? &host.pic_x[sh] : &host.pic_y[sh];
	}
}

static void screen_build(void)
{
	char v[GS_ROWS][28];
	const int cur = ov_cur[OV_SCREEN], sh = pic_shape();
	if (paused)
		m_begin(228, 16, "PAUSE  SCREEN");
	else
		m_begin(228, 16, "%s  GENERAL SCREEN", game_word(prog_now));
	snprintf(v[GS_PICTURE], sizeof v[0], "%s", pic_names[host.picture]);
	const int *k[3] = { host.pic_size, host.pic_sx, host.pic_sy };
	for (int i = 0; i < 3; i++)
		snprintf(v[GS_SIZE + i], sizeof v[0], "%d.%d%%", k[i][sh] / 10, k[i][sh] % 10);
	snprintf(v[GS_XPOS], sizeof v[0], "%+d", host.pic_x[sh]);
	snprintf(v[GS_YPOS], sizeof v[0], "%+d", host.pic_y[sh]);
	snprintf(v[GS_ROT], sizeof v[0], "%s", rot_names[host.screen]);
	snprintf(v[GS_SCAN], sizeof v[0], "%s", host.scan ? "ON" : "OFF");
	snprintf(v[GS_STRENGTH], sizeof v[0], "%d%%", host.strength);
	snprintf(v[GS_BRIGHT], sizeof v[0], "%d%%", host.bright);
	snprintf(v[GS_SMOOTH], sizeof v[0], "%s", host.smooth ? "ON" : "OFF");
	snprintf(v[GS_ASPECT], sizeof v[0], "%s", host.aspect == FE_ASPECT_4_3 ? "4:3" : host.aspect == FE_ASPECT_16_9 ?
		"16:9" : disp_wide ? "AUTO (16:9)" : "AUTO (4:3)");
	static const char *const names[GS_ROWS] = { "PICTURE SIZE", "SIZE", "X SIZE", "Y SIZE", "X POSITION",
		"Y POSITION", "ROTATION", "SCANLINES", "SCAN STRENGTH", "BRIGHTNESS", "SMOOTHING", "ASPECT RATIO" };
	for (int i = 0; i < GS_ROWS; i++) {
		FeRow *r = m_row(names[i], v[i], cur == i, 0, 1);
		if (i >= GS_SIZE && i <= GS_YPOS && host.picture != FE_PIC_CUSTOM && cur != i)
			r->value_c = FE_C_DIM;      /* CUSTOM's own values, kept while INTEGER / FULL show */
		if (i == GS_STRENGTH && !host.scan && cur != i)
			r->value_c = FE_C_DIM;
	}
	static const char *const rot_help[3] = { "ROTATE THE GAME WINDOW\nOFF = UPRIGHT TV",
		"ROTATE THE GAME WINDOW\nTV'S TOP EDGE ON YOUR LEFT", "ROTATE THE GAME WINDOW\nTV'S TOP EDGE ON YOUR RIGHT" };
	static const char *const helps[GS_ROWS] = { "INTEGER = SHARP, FULL = FULL HEIGHT\nCUSTOM = MANUALLY ADJUST",
		"PICTURE SIZE\nA = DEFAULT", "PICTURE WIDTH\nA = DEFAULT", "PICTURE HEIGHT\nA = DEFAULT",
		"PICTURE PLACE, LEFT OR RIGHT\nA = CENTRE", "PICTURE PLACE, UP OR DOWN\nA = CENTRE", NULL,
		"RASTER SCANNING PATTERN", "SCANLINES INTENSITY", "CHANGE BRIGHTNESS\nA = RESET", "SOFT BLENDS THE PIXELS",
		"ADJUST ASPECT RATIO,\nAUTO = CONSOLE'S ASPECT RATIO" };
	m_help(FE_C_CYAN, cur == GS_ROT ? rot_help[host.screen] : helps[cur]);
	if (host.gadgets && gadgets_off_why()[0])
		m_line(FE_C_DIM, "GADGETS %s", gadgets_off_why());
	m_line(FE_C_GREEN, "LEFT/RIGHT CHANGE");
	m_back();
}

/* a CUSTOM row: LEFT / RIGHT one step, held a step every frame after PIC_REPEAT frames, larger after PIC_FAST; A the
 * default (ibara360 test 89's pic_input) */
static void pic_input(int row, const FePad *pad)
{
	int lo, hi, step, fast, def, d = 0;
	int *const v = pic_val(row, &lo, &hi, &step, &fast, &def);
	const int was = *v, lr = (pad->held & (FE_LEFT | FE_RIGHT)) != 0;
	pic_held = lr ? pic_held + 1 : 0;
	if (pad->press & FE_A)
		*v = def;
	else if (pad->press & (FE_LEFT | FE_RIGHT))
		d = pad->press & FE_RIGHT ? 1 : -1;
	else if (lr && pic_held > PIC_REPEAT)
		d = pad->held & FE_RIGHT ? 1 : -1;
	*v += d * (pic_held > PIC_FAST ? fast : step);
	*v = *v < lo ? lo : *v > hi ? hi : *v;
	if (*v != was) {
		if (host.picture != FE_PIC_CUSTOM) {
			host.picture = FE_PIC_CUSTOM;
			LOG("fe: PICTURE SIZE CUSTOM");
		}
		host_dirty = 1;
	}
}

static void screen_input(const FePad *pad)
{
	const uint16_t press = pad->press, d = ov_dirs(pad);
	if (press & (FE_B | FE_BACK)) {
		ovl_back();
		return;
	}
	if (ov_move(OV_SCREEN, d, GS_ROWS)) {
		pic_held = 0;
		return;
	}
	const int cur = ov_cur[OV_SCREEN];
	if (cur >= GS_SIZE && cur <= GS_YPOS) {
		pic_input(cur, pad);
		return;
	}
	const int a = press & FE_A, step = d & FE_LEFT ? -1 : (d & FE_RIGHT) || a ? 1 : 0;
	if (!step)
		return;
	switch (cur) {
	case GS_PICTURE: host.picture = (host.picture + 3 + step) % 3; break;
	case GS_ROT: host.screen = (host.screen + 3 + step) % 3; break;
	case GS_SCAN: host.scan = !host.scan; break;
	case GS_STRENGTH:
		host.strength = a ? 35 : host.strength + 5 * step;
		host.strength = host.strength < STRENGTH_MIN ? STRENGTH_MIN : host.strength > STRENGTH_MAX ? STRENGTH_MAX :
			host.strength;
		break;
	case GS_BRIGHT:
		host.bright = a ? 100 : host.bright + 5 * step;
		host.bright = host.bright < BRIGHT_MIN ? BRIGHT_MIN : host.bright > BRIGHT_MAX ? BRIGHT_MAX : host.bright;
		break;
	case GS_SMOOTH: host.smooth = !host.smooth; break;
	case GS_ASPECT: host.aspect = (host.aspect + 3 + step) % 3; break;
	}
	host_dirty = 1;
}

/* CONTROLS (test 9): A B X Y LB RB LT RT, AUTOFIRE, DEFAULTS */
enum { CT_AUTOFIRE = FE_KEYS, CT_DEFAULTS, CT_ROWS };

static int af_index(void)
{
	for (int r = 0; r < AF_RATES; r++)
		if (af_rates[r] == ctl.af_hz)
			return r;
	return 0;
}

static void controls_build(void)
{
	char v[16], h[2 * FE_HM_CHARS];
	const int cur = ov_cur[OV_CONTROLS], p = 60 / ctl.af_hz;
	if (paused)
		m_begin(228, 10, "PAUSE  CONTROLS");
	else
		m_begin(228, 10, "%s  CONTROLS", game_word(prog_now));
	for (int k = 0; k < FE_KEYS; k++) {
		FeRow *r = m_row(key_names[k], act_names[ctl.key[k]], cur == k, 0, 0);
		if (ctl.key[k] == ACT_OFF && cur != k)
			r->value_c = FE_C_DIM;
	}
	snprintf(v, sizeof v, "%d HZ", ctl.af_hz);
	m_row("AUTOFIRE", v, cur == CT_AUTOFIRE, 0, 0);
	m_row("DEFAULTS", NULL, cur == CT_DEFAULTS, 0, 0);
	if (ov_msg[0])
		snprintf(h, sizeof h, "%s", ov_msg);
	else if (cur == CT_AUTOFIRE)
		snprintf(h, sizeof h, "SHOT DOWN %d, UP %d FRAMES", p / 2, p - p / 2);
	else if (cur == CT_DEFAULTS)
		snprintf(h, sizeof h, "A = RESTORE DEFAULT");
	else if (ctl.key[cur] == ACT_AUTO)
		snprintf(h, sizeof h, "AUTO SHOT = SHOT AT %d HZ", ctl.af_hz);
	else
		snprintf(h, sizeof h, "LEFT/RIGHT: SELECT FUNCTION");
	m_help(FE_C_CYAN, h);
	m_line(FE_C_DIM, "BACK = PAUSE, START = THE GAME'S");
	m_back();
}

static void controls_input(const FePad *pad)
{
	const uint16_t press = pad->press, d = ov_dirs(pad);
	if (press & (FE_B | FE_BACK)) {
		ovl_back();
		return;
	}
	if (ov_move(OV_CONTROLS, d, CT_ROWS))
		return;
	const int cur = ov_cur[OV_CONTROLS], a = press & FE_A;
	const int step = d & FE_LEFT ? -1 : (d & FE_RIGHT) || a ? 1 : 0;
	if (!step)
		return;
	if (cur < FE_KEYS) {
		ctl.key[cur] = (ctl.key[cur] + ACTS + step) % ACTS;
	} else if (cur == CT_AUTOFIRE) {
		/* RIGHT (and A) = the faster rate, LEFT the slower (ibara360 test 57), round */
		ctl.af_hz = af_rates[(af_index() + AF_RATES - step) % AF_RATES];
	} else if (step > 0) {
		memcpy(ctl.key, ctl_default.key, sizeof ctl.key);
		snprintf(ov_msg, sizeof ov_msg, "DEFAULT BUTTONS RESTORED");
	} else {
		return;
	}
	opt_dirty = 1;
	LOG("fe: CONTROLS %s", cur < FE_KEYS ? key_names[cur] : cur == CT_AUTOFIRE ? "AUTOFIRE" : "DEFAULTS");
}

/* SOUND. Test 10 (ibara360's page): MUSIC, SFX (0-100% in tens), ATTRACT (their share outside a credit), MUTE;
 * outside a credit MUSIC TEST and SFX TEST (test 9's SOUND TEST: the board's sound codes 0x20-0xD7, no names for them
 * as ibara360's SFX TEST; the board stands under a page in a credit) */
enum { SN_MUSIC, SN_SFX, SN_ATTRACT, SN_MUTE, SN_MTEST, SN_STEST, SN_IDS };

static int sound_rows(void)
{
	return in_credit() ? SN_MTEST : SN_IDS;
}

static void sound_build(void)
{
	char v[SN_IDS][28], t[24];
	const int n = sound_rows();
	int *cur = &ov_cur[OV_SOUND];
	if (*cur >= n)
		*cur = n - 1;
	if (paused)
		m_begin(228, 12, "PAUSE  SOUND");
	else
		m_begin(228, 12, "%s  SOUND", game_word(prog_now));
	snprintf(v[SN_MUSIC], sizeof v[0], "%d%%", host.music);
	snprintf(v[SN_SFX], sizeof v[0], "%d%%", host.effects);
	snprintf(v[SN_ATTRACT], sizeof v[0], "%d%%", host.attract);
	snprintf(v[SN_MUTE], sizeof v[0], "%s", host.mute ? "ON" : "OFF");
	br_music_title(opt.music_test, t, sizeof t - 4);
	snprintf(v[SN_MTEST], sizeof v[0], "%02X %s", opt.music_test, t[0] ? t : "-");
	snprintf(v[SN_STEST], sizeof v[0], "%02X", opt.sound_test);
	static const char *const names[SN_IDS] = { "MUSIC", "SFX", "ATTRACT", "MUTE", "MUSIC TEST", "SFX TEST" };
	for (int i = 0; i < n; i++)
		m_row(names[i], v[i], *cur == i, 0, 1);
	static const char *const helps[SN_IDS] = { "MUSIC VOLUME", "SOUND EFFECTS AND VOICES VOLUME",
		"ATTRACT SCREEN VOLUME", "ALL SOUND OFF", "LEFT/RIGHT CHOOSE, A = PLAY", "LEFT/RIGHT CHOOSE, A = PLAY" };
	m_help(FE_C_CYAN, helps[*cur]);
	if (in_credit())
		m_line(FE_C_DIM, "HEARD ONCE THE GAME GOES ON");
	m_line(FE_C_GREEN, "LEFT/RIGHT CHANGE");
	m_back();
}

static void sound_stop_test(void)
{
	if (music_played) {
		music_cmd(0x21, 0x00);
		music_cmd(0x20, 0xE0);
		music_played = 0;
	}
}

static void sound_input(const FePad *pad)
{
	const uint16_t press = pad->press, d = ov_dirs(pad);
	const int n = sound_rows();
	if (press & (FE_B | FE_BACK)) {
		sound_stop_test();
		ovl_back();
		return;
	}
	if (ov_move(OV_SOUND, d, n))
		return;
	const int left = d & FE_LEFT, right = d & FE_RIGHT, a = press & FE_A;
	switch (ov_cur[OV_SOUND]) {
	case SN_MUSIC:
	case SN_SFX:
	case SN_ATTRACT: {
		if (!left && !right)
			break;
		int *v = ov_cur[OV_SOUND] == SN_MUSIC ? &host.music : ov_cur[OV_SOUND] == SN_SFX ? &host.effects : &host.attract;
		*v += right ? 10 : -10;
		*v = *v < 0 ? 0 : *v > 100 ? 100 : *v;
		apply_volumes();
		host_dirty = 1;
		break;
	}
	case SN_MUTE:
		if (left || right || a) {
			host.mute = !host.mute;
			apply_volumes();
			host_dirty = 1;
		}
		break;
	case SN_MTEST:
		if (left || right)
			opt.music_test = (opt.music_test + (right ? 1 : 0x19)) % 0x1A;
		if (a) {
			music_cmd(0x20, opt.music_test);
			music_played = 1;
			LOG("fe: MUSIC TEST %02X", opt.music_test);
		}
		break;
	case SN_STEST:
		if (left || right)
			opt.sound_test = 0x20 + (opt.sound_test - 0x20 + (right ? 1 : 0xB7)) % 0xB8;
		if (a) {
			music_cmd(0x20, opt.sound_test);
			LOG("fe: SFX TEST %02X", opt.sound_test);
		}
		break;
	}
}

/* DEBUG MENU (outside a credit): HITBOX, INFINITE LIVES, INFINITE BOMBS (test 16), INVINCIBILITY (test 17),
 * INITIALIZE RANKINGS (this MODE's or both), DEFAULT SETTINGS */
enum { DB_HITBOX, DB_LIVES, DB_BOMBS, DB_INV, DB_RANKS, DB_DEFAULTS, DB_ROWS };

static void rk_init(int both)
{
	rk_have[prog_now] = 0;
	rk_save(prog_now);
	if (both) {
		rk_have[!prog_now] = 0;
		rk_save(!prog_now);
	}
	if (rk_def_ok)
		rk_write(rk_def);
	LOG("fe: INITIALIZE RANKINGS %s", both ? "BOTH MODES" : mode_word(prog_now));
}

/* every option, DIP switch and button as they come (MODE stays: it is PLAY's) */
static void settings_default(void)
{
	const int m = host.mode;
	host = host_default;
	host.mode = m;
	opt = opt_default;
	ctl = ctl_default;
	db_lives = db_bombs = db_inv = 0;
	apply_language();
	apply_volumes();
	host_dirty = opt_dirty = 1;
	LOG("fe: DEFAULT SETTINGS");
}

static void debug_build(void)
{
	const int cur = ov_cur[OV_DEBUG];
	m_begin(228, 21, "%s  DEBUG MENU", game_word(prog_now));
	m_row("HITBOX", hitbox_names[host.hitbox], cur == DB_HITBOX, 0, 1);
	m_row("INFINITE LIVES", db_lives ? "ON" : "OFF", cur == DB_LIVES, 0, 1);
	m_row("INFINITE BOMBS", db_bombs ? "ON" : "OFF", cur == DB_BOMBS, 0, 1);
	m_row("INVINCIBILITY", db_inv ? "ON" : "OFF", cur == DB_INV, 0, 1);
	FeRow *r = m_row("INITIALIZE RANKINGS", rk_both ? "BOTH MODES" : "THIS MODE", cur == DB_RANKS, 0, 1);
	if (ov_armed == DB_RANKS)
		r->name_c = FE_C_RED;
	r = m_row("DEFAULT SETTINGS", NULL, cur == DB_DEFAULTS, 0, 0);
	if (ov_armed == DB_DEFAULTS)
		r->name_c = FE_C_RED;
	if (ov_armed >= 0)
		m_help(FE_C_RED, "CONFIRM? A = YES, B = NO");
	else if (ov_msg[0])
		m_help(FE_C_CYAN, ov_msg);
	else
		m_help(FE_C_CYAN, cur == DB_HITBOX ? "PLAYER = THE RIDERS' HITBOXES\nALL = THE ENEMIES' AND BULLETS' TOO" :
			cur == DB_LIVES ? "THE TEAM NEVER RUNS OUT\n(INVALIDATES SCORE)" :
			cur == DB_BOMBS ? "THE BOMBS NEVER RUN OUT\n(INVALIDATES SCORE)" :
			cur == DB_INV ? "NO DAMAGE\n(INVALIDATES SCORE)" :
			cur == DB_RANKS ? "CLEAR THE HIGH SCORE TABLES\nOF THIS MODE OR OF BOTH" :
			"EVERY OPTION, DIP SWITCH AND\nBUTTON BACK TO ITS DEFAULT");
	m_line(FE_C_GREEN, "LEFT/RIGHT CHANGE");
	m_back();
}

static void debug_input(const FePad *pad)
{
	const uint16_t press = pad->press, d = ov_dirs(pad);
	const int cur = ov_cur[OV_DEBUG], a = press & FE_A;
	if (ov_armed >= 0) {
		if (press & (FE_B | FE_BACK)) {
			ov_armed = -1;
		} else if (a) {
			if (ov_armed == DB_RANKS) {
				rk_init(rk_both);
				snprintf(ov_msg, sizeof ov_msg, "RANKINGS INITIALIZED");
			} else {
				settings_default();
				snprintf(ov_msg, sizeof ov_msg, "SETTINGS BACK TO DEFAULT");
			}
			ov_armed = -1;
		} else {
			ov_move(OV_DEBUG, d, DB_ROWS);
			if (d & (FE_LEFT | FE_RIGHT))
				ov_armed = -1;
		}
		return;
	}
	if (press & (FE_B | FE_BACK)) {
		ovl_back();
		return;
	}
	if (ov_move(OV_DEBUG, d, DB_ROWS))
		return;
	const int lr = d & (FE_LEFT | FE_RIGHT);
	switch (cur) {
	case DB_HITBOX:
		if (lr || a) {
			host.hitbox = (host.hitbox + (d & FE_LEFT ? HITBOX_MODES - 1 : 1)) % HITBOX_MODES;
			host_dirty = 1;
		}
		break;
	case DB_LIVES:
	case DB_BOMBS:
	case DB_INV:
		if (lr || a) {
			int *v = cur == DB_LIVES ? &db_lives : cur == DB_BOMBS ? &db_bombs : &db_inv;
			*v = !*v;
			LOG("fe: DEBUG %s %s", cur == DB_LIVES ? "INFINITE LIVES" : cur == DB_BOMBS ? "INFINITE BOMBS" :
				"INVINCIBILITY", *v ? "ON" : "OFF");
		}
		break;
	case DB_RANKS:
		if (lr) { rk_both = !rk_both; ov_msg[0] = 0; }
		else if (a) ov_armed = DB_RANKS;
		break;
	case DB_DEFAULTS:
		if (a) ov_armed = DB_DEFAULTS;
		break;
	}
}

/* DIP SWITCHES (PLAY's, ORIGINAL only; spec 4.4's value strings by DIP index and the order LEFT / RIGHT walk) */
static const char *const gs_names[4] = { "DIFFICULTY", "TIMER", "LIVES", "BONUS LIFE" };
static const char *const gs_vals[4][4] = {
	{ "NORMAL", "EASY", "HARD", "VERY HARD" }, { "NORMAL", "LOW", "HIGH", "HIGHEST" },
	{ "3", "4", "2", "1" }, { "1500000", "1000000", "2000000", "NONE" },
};
static const int gs_order[4][4] = { { 1, 0, 2, 3 }, { 3, 2, 0, 1 }, { 3, 2, 0, 1 }, { 3, 2, 0, 1 } };
enum { DIP_DEFAULT = 4, DIP_ROWS };

static void dips_build(void)
{
	static const char *const helps[DIP_ROWS] = { "THE ARCADE'S DIFFICULTY", "THE ARCADE'S TIMER SETTING",
		"CHANGE STARTING LIVES", "CHANGE REQUIRED SCORE FOR\nGETTING MORE LIVES", "A = REVERT TO INITIAL SETTINGS" };
	const int cur = ov_cur[OV_DIPS];
	m_begin(228, 16, "%s  DIP SWITCHES", game_word(FE_ORIGINAL));
	for (int i = 0; i < 4; i++) {
		FeRow *r = m_row(gs_names[i], gs_vals[i][opt.gs[i]], cur == i, 0, 0);
		if (opt.gs[i] && cur != i)
			r->value_c = FE_C_RED;
	}
	m_row("DEFAULT", NULL, cur == DIP_DEFAULT, 0, 0);
	m_help(FE_C_CYAN, helps[cur]);
	m_line(FE_C_DIM, "INVALID SCORE IF NOT DEFAULT");   /* test 13: ibara360's rule and words */
	m_line(FE_C_GREEN, "LEFT/RIGHT CHANGE");
	m_back();
}

static void dips_input(const FePad *pad)
{
	const uint16_t press = pad->press, d = ov_dirs(pad);
	if (press & (FE_B | FE_BACK)) {
		ovl_back();
		return;
	}
	if (ov_move(OV_DIPS, d, DIP_ROWS))
		return;
	const int cur = ov_cur[OV_DIPS], a = press & FE_A;
	if (cur == DIP_DEFAULT) {
		if (a && gs_any()) {
			memset(opt.gs, 0, sizeof opt.gs);
			opt_dirty = 1;
		}
		return;
	}
	if (!(d & (FE_LEFT | FE_RIGHT)) && !a)
		return;
	const int *o = gs_order[cur];
	int k = 0;
	while (o[k] != opt.gs[cur])
		k++;
	opt.gs[cur] = o[(k + (d & FE_LEFT ? 3 : 1)) % 4];
	opt_dirty = 1;
}

/* ---- test 8's pages, test 9's grammar ---- */

/* SAVE / LOAD STATE: the ten slots of a MODE, the chosen one's picture beside them. LOAD walks both MODEs with
 * LEFT / RIGHT (a state of the other MODE brings its program with it); X deletes one. Overwriting, loading and
 * deleting ask first (ov_armed: the slot, + FE_SLOTS for a delete). */
static int st_last;                      /* the slot of the last request (fe_state_failed's message) */

static void st_build(void)
{
	FeSlot s[FE_SLOTS];
	for (int i = 0; i < FE_SLOTS; i++) {
		memset(&s[i], 0, sizeof s[i]);
		if (plat.slot)
			plat.slot(st_mode, i, &s[i]);
	}
	const int cur = ov_cur[OV_STATES];
	m_begin(228, 0, "%s  %s", game_word(st_mode), st_save ? "SAVE STATE" : "LOAD STATE");
	for (int i = 0; i < FE_SLOTS; i++) {
		char t[28];
		snprintf(t, sizeof t, "%02d %s", i + 1, s[i].used ? s[i].text : "EMPTY");
		FeRow *r = m_row(t, NULL, i == cur, 0, 0);
		if (ov_armed == i || ov_armed == i + FE_SLOTS)
			r->name_c = FE_C_RED;
	}
	J->pic = s[cur].used ? s[cur].pic : NULL;
	if (ov_armed >= FE_SLOTS)
		m_help(FE_C_RED, "DELETE? A = YES, B = NO");
	else if (ov_armed >= 0)
		m_help(FE_C_RED, "CONFIRM? A = YES, B = NO");
	else if (ov_msg[0])
		m_help(FE_C_CYAN, ov_msg);
	else if (st_save)
		m_help(FE_C_CYAN, s[cur].used ? "A = OVERWRITE" : "A = SAVE");
	else
		m_help(FE_C_CYAN, s[cur].used ? "A = LOAD" : "NOTHING TO LOAD");
	m_line(FE_C_GREEN, "UP/DOWN SLOT, A = CONFIRM");
	if (!st_save)
		m_line(FE_C_GREEN, "X = DELETE, LEFT/RIGHT = MODE");
	m_back();
}

static void st_request(int kind, int slot)
{
	req_kind = kind;
	req_arg = st_mode * FE_SLOTS + slot;
	st_last = slot;
	if (kind == FE_REQ_SAVE_STATE)
		snprintf(ov_msg, sizeof ov_msg, "SAVED IN SLOT %02d", slot + 1);
	else if (kind == FE_REQ_DELETE_STATE)
		snprintf(ov_msg, sizeof ov_msg, "SLOT %02d DELETED", slot + 1);
	LOG("fe: %s %s slot %d", kind == FE_REQ_SAVE_STATE ? "save" : kind == FE_REQ_LOAD_STATE ? "load" : "delete",
		mode_word(st_mode), slot + 1);
}

static void st_input(const FePad *pad)
{
	const uint16_t press = pad->press, d = ov_dirs(pad);
	const int cur = ov_cur[OV_STATES];
	FeSlot s = { 0 };
	if (plat.slot)
		plat.slot(st_mode, cur, &s);
	if (ov_armed >= 0) {
		if (press & FE_A) {
			st_request(ov_armed >= FE_SLOTS ? FE_REQ_DELETE_STATE : st_save ? FE_REQ_SAVE_STATE :
				FE_REQ_LOAD_STATE, ov_armed % FE_SLOTS);
			ov_armed = -1;
		} else if ((press & (FE_B | FE_BACK)) || d) {
			ov_armed = -1;
			ov_move(OV_STATES, d, FE_SLOTS);
		}
		return;
	}
	if (press & (FE_B | FE_BACK)) {
		ovl_back();
		return;
	}
	if (ov_move(OV_STATES, d, FE_SLOTS))
		return;
	if (!st_save && (d & (FE_LEFT | FE_RIGHT))) {
		st_mode = !st_mode;
		ov_msg[0] = 0;
		return;
	}
	if (press & FE_A) {
		ov_msg[0] = 0;
		if (st_save && !s.used)
			st_request(FE_REQ_SAVE_STATE, cur);
		else if (s.used)
			ov_armed = cur;
	} else if ((press & FE_X) && !st_save && s.used) {
		ov_armed = cur + FE_SLOTS;
	}
}

/* REPLAY DATA: the kept ones, newest first, twelve rows at a time. */
enum { RP_SHOWN = 12, RP_MAX = 30 };

static void rp_build(void)
{
	FeReplayInfo r;
	rp_n = plat.replay ? plat.replay(0, &r) : 0;
	int *cur = &ov_cur[OV_REPLAYS];
	if (*cur >= rp_n)
		*cur = rp_n ? rp_n - 1 : 0;
	m_begin(228, 0, "%s  REPLAY DATA", game_word(prog_now));
	snprintf(J->hm_corner, sizeof J->hm_corner, "%d/%d", rp_n, RP_MAX);
	if (!rp_n) {
		m_help(FE_C_CYAN, ov_msg[0] ? ov_msg : "NO REPLAYS YET");
		m_line(FE_C_DIM, "SAVE ONE AFTER A CREDIT");
		m_back();
		return;
	}
	if (*cur < rp_first)
		rp_first = *cur;
	if (*cur >= rp_first + RP_SHOWN)
		rp_first = *cur - RP_SHOWN + 1;
	for (int n = 0; n < RP_SHOWN && rp_first + n < rp_n; n++) {
		r.text[0] = 0;
		r.red = 0;
		plat.replay(rp_first + n, &r);
		FeRow *row = m_row(r.text, NULL, rp_first + n == *cur, 0, 0);
		if (ov_armed == rp_first + n || r.red)
			row->name_c = FE_C_RED;     /* (test 13: an invalid score's replay red, ibara360's) */
	}
	if (ov_armed >= 0)
		m_help(FE_C_RED, "DELETE? A = YES, B = NO");
	else
		m_help(FE_C_CYAN, ov_msg[0] ? ov_msg : "A = PLAY, X = DELETE");
	m_line(FE_C_GREEN, "UP/DOWN = SELECT");
	m_back();
}

static void rp_input(const FePad *pad)
{
	const uint16_t press = pad->press, d = ov_dirs(pad);
	const int cur = ov_cur[OV_REPLAYS];
	if (ov_armed >= 0) {
		if (press & FE_A) {
			req_kind = FE_REQ_DELETE_REPLAY;
			req_arg = ov_armed;
			snprintf(ov_msg, sizeof ov_msg, "REPLAY %02d DELETED", ov_armed + 1);
			ov_armed = -1;
		} else if ((press & (FE_B | FE_BACK)) || d) {
			ov_armed = -1;
			ov_move(OV_REPLAYS, d, rp_n);
		}
		return;
	}
	if (press & (FE_B | FE_BACK)) {
		ovl_back();
		return;
	}
	if (!rp_n || ov_move(OV_REPLAYS, d, rp_n))
		return;
	if (press & FE_A) {
		req_kind = FE_REQ_PLAY_REPLAY;
		req_arg = cur;
		LOG("fe: play replay %d", cur + 1);
	} else if (press & FE_X) {
		ov_armed = cur;
	}
}

/* MANUAL: the main loop shows the pages (out->manual); YOKO a spread of two, TATE one. */
static void man_input(const FePad *pad)
{
	const uint16_t p = pad->press;
	const int n = man_pages[host.language], step = host.screen == FE_YOKO ? 2 : 1;
	if (p & (FE_B | FE_BACK)) {
		ovl_back();
		return;
	}
	if (host.screen == FE_YOKO)
		man_page &= ~1;
	if ((p & (FE_RIGHT | FE_RB | FE_A)) && man_page + step < n)
		man_page += step;
	if ((p & (FE_LEFT | FE_LB)) && man_page > 0)
		man_page = man_page >= step ? man_page - step : 0;
}

/* SAVE REPLAY? after a credit (ibara360's box) */
static void prompt_build(void)
{
	m_begin(228, 0, "%s  REPLAY", game_word(prog_now));
	m_help(FE_C_CYAN, "SAVE REPLAY?");
	for (int i = 0; i < 3; i++)
		if (prompt_line[i][0])
			m_line(i == 1 && prompt_red ? FE_C_RED : FE_C_WHITE, "%s", prompt_line[i]);
	m_line(FE_C_GREEN, "A = YES, B = NO");
	if (prompt_red)
		m_line(FE_C_DIM, "* = INVALID SCORE");   /* test 13: ibara360's */
}

static void prompt_input(const FePad *pad)
{
	const uint16_t press = pad->press;
	if (!(press & (FE_A | FE_B | FE_BACK)))
		return;
	const int keep = (press & FE_A) != 0;
	req_kind = keep ? FE_REQ_KEEP_REPLAY : FE_REQ_DROP_REPLAY;
	ovl_close_all();
	LOG("fe: replay %s", keep ? "kept" : "not kept");
}

static void ovl_input(const FePad *pad)
{
	/* START on a page: every page closes (back to the pause menu, the title's page or the game) */
	if (ovl != OV_PROMPT && (pad->press & FE_START)) {
		const int play = ovl == OV_PLAYER;   /* test 15: the TRAINING page's START = PLAY there too */
		if (ovl == OV_SOUND)
			sound_stop_test();
		ovl_close_all();
		if (play)
			tr_start();
		return;
	}
	switch (ovl) {
	case OV_OPTIONS: options_input(pad); break;
	case OV_SCREEN: screen_input(pad); break;
	case OV_CONTROLS: controls_input(pad); break;
	case OV_SOUND: sound_input(pad); break;
	case OV_DEBUG: debug_input(pad); break;
	case OV_COMMANDS: commands_input(pad); break;
	case OV_DIPS: dips_input(pad); break;
	case OV_STATES: st_input(pad); break;
	case OV_REPLAYS: rp_input(pad); break;
	case OV_MANUAL: man_input(pad); break;
	case OV_PROMPT: prompt_input(pad); break;
	case OV_PLAYER: player_input(pad); break;
	}
}

static void ovl_build(void)
{
	switch (ovl) {
	case OV_OPTIONS: options_build(); break;
	case OV_SCREEN: screen_build(); break;
	case OV_CONTROLS: controls_build(); break;
	case OV_SOUND: sound_build(); break;
	case OV_DEBUG: debug_build(); break;
	case OV_COMMANDS: commands_build(); break;
	case OV_DIPS: dips_build(); break;
	case OV_STATES: st_build(); break;
	case OV_REPLAYS: rp_build(); break;
	case OV_PROMPT: prompt_build(); break;
	case OV_PLAYER: player_build(); break;
	}
}

/* ---- title menu pages' input (spec 1.3, 1.6, 8; test 9: A chooses, B / BACK goes back) ---- */
static void title_input(int up, int down, int left, int right, uint16_t press)
{
	const int choose = press & (FE_A | FE_START);
	switch (page) {
	case PG_TITLE:
		if (up) { title_cur = (title_cur + TM_ROWS - 1) % TM_ROWS; }
		if (down) { title_cur = (title_cur + 1) % TM_ROWS; }
		if (!choose)
			break;
		switch (title_cur) {
		case TM_PLAY: page = PG_PLAY; opt_cur = 0; break;
		case TM_TRAINING: page = PG_PRACTICE; break;
		case TM_LOAD: open_states(0); break;
		case TM_REPLAYS: open_replays(); break;
		case TM_OPTIONS: ovl_open(OV_OPTIONS); break;
		case TM_EXIT:
			settings_flush();
			exit_t = 0;
			LOG("fe: EXIT");
			break;
		}
		break;
	case PG_PLAY:
		play_input(up, down, left, right, press);
		break;
	case PG_PRACTICE:
		tr_input(up, down, left, right, press);
		break;
	}
}

/* ---- the pause menu (spec 2; test 9: ibara360's rows; recorded frames, so a replay goes through it too) ---- */
/* Test 11 (user: ibara360's features): RESTART STAGE as ibara360's pause has it, its value < STAGE / STATE >: STAGE =
 * the state main.c took when this credit's stage began, STATE = the save state last saved or loaded in this credit
 * (fe_set_restart says which there are; grey without). A asks once like FULL RESTART. The ids keep test 10's numbers
 * (PS_STAGE last), the rows' order is pause_rows'. TRAINING has its RESTART (the training stage) and no RESTART STAGE. */
enum { PS_RESUME, PS_RESTART, PS_SAVE, PS_LOAD, PS_OPTIONS, PS_INVULN, PS_QUIT, PS_STAGE, PS_QTITLE, PS_ROWS };
static const uint8_t ps_game[] = { PS_RESUME, PS_STAGE, PS_RESTART, PS_SAVE, PS_LOAD, PS_OPTIONS, PS_INVULN, PS_QUIT };
/* test 14: TRAINING's (ibara360's training pause): INVINCIBLE is on the TRAINING page, QUIT goes back to it;
 * test 15 (user): no SAVE / LOAD STATE there */
static const uint8_t ps_training[] = { PS_RESUME, PS_RESTART, PS_OPTIONS, PS_QUIT, PS_QTITLE };
static int rs_stage, rs_state, rs_to_state;      /* RESTART STAGE: the stage's start there (its number), a STATE */

void fe_set_restart(int stage, unsigned state)
{
	/* a new stage's start offers STAGE first, a state just saved or loaded STATE (ibara360's pm_to_state) */
	if (stage && stage != rs_stage)
		rs_to_state = 0;
	if (state && state != (unsigned)rs_state)
		rs_to_state = 1;
	rs_stage = stage;
	rs_state = (int)state;
}

void fe_restart_loaded(int stage)
{
	state_loaded(0);
	/* test 13 (ibara360's test 87 rule): stage 1's start is a new run with a new replay (main.c), past it the score
	 * goes red and the credit gets no replay; a STATE's restart (stage 0) already did both in fe_state_loaded */
	if (stage != 1 && in_credit()) {
		score_red("RESTART STAGE");
		replay_drop("RESTART STAGE");
	}
	paused = 0;                     /* the stage goes on at once (fe_state_loaded paused it as LOAD STATE does) */
	pause_t = 0;
	swallow = pad_press;
}

static const uint8_t *pause_rows(int *n)
{
	*n = practice ? (int)sizeof ps_training : (int)sizeof ps_game;
	return practice ? ps_training : ps_game;
}

/* RESTART STAGE's value now: 0 STAGE, 1 STATE, -1 neither there */
static int rs_pick(void)
{
	if (rs_to_state ? rs_state : rs_stage)
		return rs_to_state;
	return rs_stage ? 0 : rs_state ? 1 : -1;
}

static void pause_build(void)
{
	static const char *const names[PS_ROWS] = { "RESUME", "FULL RESTART", "SAVE STATE", "LOAD STATE", "OPTIONS",
		"INVULNERABLE", "QUIT TO TITLE", "RESTART STAGE", "QUIT TO TITLE" };
	if (practice)
		m_begin(228, 15, "%s  TRAINING PAUSE", game_word(prog_now));
	else
		m_begin(228, 15, "%s  PAUSE", game_word(prog_now));
	int n;
	const uint8_t *rows = pause_rows(&n);
	const int pick = rs_pick();
	for (int k = 0; k < n; k++) {
		const int i = rows[k];
		const char *v = i == PS_INVULN ? invuln ? "ON" : "OFF" : i == PS_STAGE ? pick < 0 ? "NONE" : pick ? "STATE" :
			"STAGE" : NULL;
		FeRow *r = m_row(i == PS_RESTART && practice ? "RESTART" : i == PS_QUIT && practice ? "QUIT TO TRAINING" :
			names[i], v, i == pause_cur, i == PS_STAGE &&
			pick < 0, i == PS_INVULN || (i == PS_STAGE && rs_stage && rs_state));
		if (pause_armed && i == pause_cur)
			r->name_c = FE_C_RED;
	}
	static const char *const helps[PS_ROWS] = { "RESUME GAME", "FULL RESTART FROM STAGE 1\nWITH THE SAME TEAM",
		"SAVE POSITION WITHIN THE GAME", "LOAD A SAVED POSITION\n(INVALIDATES SCORE)",
		"SCREEN, GADGETS, CONTROLS,\nSOUND, MANUAL", "INVALID SCORE, NO RANKING\nAND NO REPLAY ONCE IT IS ON",
		"RETURN TO THE TITLE", "", "RETURN TO THE TITLE" };   /* test 13: ibara360's score words */
	char h[2 * FE_HM_CHARS];
	if (pause_cur == PS_STAGE) {
		if (pick < 0)
			snprintf(h, sizeof h, "NO STAGE START OR SAVE STATE\nIN THIS CREDIT YET");
		else if (pick)
			snprintf(h, sizeof h, "RELOAD THE LAST STATE SAVED\nOR LOADED (INVALIDATES SCORE)");
		else if (rs_stage == 1)
			snprintf(h, sizeof h, "RESTART FROM THE START\nOF STAGE 1");
		else
			snprintf(h, sizeof h, "RESTART FROM STAGE %d'S START\n(INVALIDATES SCORE)", rs_stage);
	} else {
		snprintf(h, sizeof h, "%s", pause_cur == PS_RESTART && practice ? "RESTART THE TRAINING STAGE" :
			pause_cur == PS_QUIT && practice ? "RETURN TO THE TRAINING PAGE" : helps[pause_cur]);
	}
	if (pause_armed)
		m_help(FE_C_RED, pause_cur == PS_QUIT || pause_cur == PS_QTITLE ? "QUIT? A = YES, B = NO" :
			"RESTART? A = YES, B = NO");
	else
		m_help(FE_C_CYAN, h);
	m_line(FE_C_GREEN, "%s", pause_armed ? "B / BACK = NO" : "B / BACK = RESUME");
}

static void pause_close(int choice)
{
	paused = 0;
	pause_armed = 0;
	swallow = pad_press;            /* test 10: the B / A closing the box is no BOMB / SHOT (held ones go on) */
	if (choice == 1) {
		restart_req = 1;
		veil = -1;
	}
	if (choice == 1 || choice == 2)
		br_wr8(CONT_COUNT, 0xFF);
	if (choice == 1 || choice == 2)
		replay_drop(choice == 1 ? "FULL RESTART" : "QUIT");   /* test 13: ibara360's */
	if (choice == 3) {
		req_kind = FE_REQ_RESTART_STAGE;
		req_arg = rs_pick();
	}
	LOG("fe: pause closed: %s", choice == 0 ? "RESUME" : choice == 1 ? "RESTART" : choice == 3 ? rs_pick() ?
		"RESTART STAGE: STATE" : "RESTART STAGE: STAGE" : "QUIT");
}

static void pause_input(int up, int down, int left, int right, uint16_t press)
{
	const int a = press & FE_A;
	if (up || down) {
		int n, k = 0;
		const uint8_t *rows = pause_rows(&n);
		while (k < n - 1 && rows[k] != pause_cur)
			k++;
		pause_cur = rows[(k + (up ? n - 1 : 1)) % n];
		pause_armed = 0;
		return;
	}
	if (pause_armed) {
		/* asked once: A does it, B / BACK takes it back */
		if (a) {
			tr_quit_title = pause_cur == PS_QTITLE;
			pause_close(pause_cur == PS_RESTART ? 1 : pause_cur == PS_STAGE ? 3 : 2);
		}
		else if (press & (FE_B | FE_BACK)) {
			pause_armed = 0;
		}
		return;
	}
	if (pause_cur == PS_INVULN && (a || left || right)) {
		invuln = !invuln;
		if (invuln) {
			not_kept = 1;
			LOG("fe: INVULNERABLE ON (score not kept)");
			score_red("INVULNERABLE");
			replay_drop("INVULNERABLE");    /* test 13: ibara360's EX-DEBUG cheats */
		}
		return;
	}
	if (pause_cur == PS_STAGE && (left || right)) {
		if (rs_stage && rs_state)
			rs_to_state = !rs_pick();
		return;
	}
	if (press & (FE_BACK | FE_B)) {
		pause_close(0);
	} else if (a) {
		if (pause_cur == PS_RESUME)
			pause_close(0);
		else if (pause_cur == PS_RESTART || pause_cur == PS_QUIT || pause_cur == PS_QTITLE ||
			(pause_cur == PS_STAGE && rs_pick() >= 0)) {
			pause_armed = 1;
		}
	}
	/* (SAVE STATE, LOAD STATE and OPTIONS open their pages in fe_frame, before anything here: those frames stay out
	 * of replays) */
}

/* A on SAVE STATE, LOAD STATE or OPTIONS opens that page over the pause menu. */
static int pause_page(int a)
{
	if (!a || pause_armed || pause_cur < PS_SAVE || pause_cur > PS_OPTIONS)
		return 0;
	if (pause_cur == PS_SAVE || pause_cur == PS_LOAD)
		open_states(pause_cur == PS_SAVE);
	else
		ovl_open(OV_OPTIONS);
	return 1;
}

/* ---- the frame ---- */

static uint16_t repeat_dirs(const FePad *pad)
{
	uint16_t r = 0;
	for (int i = 0; i < 4; i++) {
		const uint16_t bit = (uint16_t)(1 << i);
		if (!(pad->held & bit)) {
			rep_t[i] = 0;
			continue;
		}
		if (pad->press & bit)
			r |= bit;
		else if (rep_due(rep_t[i]))
			r |= bit, rep_fired = 1;
		rep_t[i]++;
	}
	return r;
}

/* ---- test 11: the menu sounds (menusnd.h), one rule for every page as ibara360's menu_se: a frame with a button
 * pressed (or a direction repeating) that changed what the menus show makes the cursor sound, decide for A / START,
 * the pause sound when the pause box opens or closes. The test-10 calls through the board (SE 0x24 / 0x22 / 0x28) are
 * gone: the board dropped them in the attract and stood under a page in a credit. ---- */
static unsigned se_due;                 /* sounds for this frame's FeOut.se */

static uint32_t menu_sig(void)
{
	const int v[] = { ovl, ovl_depth, page, title_cur, tr_cur, tr_pack(), opt_cur, gs_cur, tr.rank, paused,
		pause_cur, pause_armed, ov_armed, man_page, st_mode, rp_first, rk_both, launch_t >= 0, invuln,
		pb_paused, pb_cur, rs_to_state };
	uint32_t h = fnv((const uint8_t *)v, (int)sizeof v);
	h ^= fnv((const uint8_t *)ov_cur, (int)sizeof ov_cur) * 3;
	h ^= fnv((const uint8_t *)&host, (int)sizeof host) * 5;
	h ^= fnv((const uint8_t *)&opt, (int)sizeof opt) * 7;
	h ^= fnv((const uint8_t *)&ctl, (int)sizeof ctl) * 11;
	h ^= fnv((const uint8_t *)&tr, (int)sizeof tr) * 13;   /* test 15: the PLAYER page's values */
	return h;
}

static void menu_se(uint32_t sig0, int paused0, uint16_t press, int moved)
{
	if (!press && !moved)
		return;
	if (paused != paused0)
		se_due |= 1u << MS_PAUSE;
	else if (menu_sig() != sig0)
		se_due |= 1u << (press & (FE_A | FE_START) ? MS_DECIDE : MS_CURSOR);
}

/* the game's buttons 1-3: SHOT / BOMB / SPECIAL as CONTROLS set them (test 9; spec 4.2's map was A / B / X) */
static uint16_t game_buttons(uint16_t held)
{
	return (uint16_t)((held & FE_SHOT ? BR_B1 : 0) | (held & FE_BOMB ? BR_B2 : 0) | (held & FE_SPECIAL ? BR_B3 : 0));
}

static void title_display(FeOut *out)
{
	FeVideoJob *j = &out->job;
	out->vsrc = br_vdp_snapshot();
	if (title_keep && title_gen) {
		out->vsrc = &title_vdp;
		j->kind = FE_V_TITLE;
		j->title_gen = title_gen;
		return;
	}
	if (title_t < TITLE_WHITE) {
		j->kind = FE_V_WHITE;     /* the arcade's white: its first frames can hold the scene before's tiles */
		return;
	}
	j->kind = FE_V_TITLE;
	if (title_t < TITLE_READY) {
		j->title_gen = 0;
		return;
	}
	if (title_t == TITLE_READY) {
		memcpy(&title_vdp, br_vdp_snapshot(), sizeof title_vdp);
		title_gen++;
	}
	out->vsrc = &title_vdp;
	j->title_gen = title_gen;
}

void fe_frame(const FePad *pad, FeOut *out)
{
	cur_out = out;
	J = &out->job;
	frame_no++;
	const uint32_t sig0 = menu_sig();
	const int paused0 = paused;
	rep_fired = 0;
	FeVideoJob *j = J;
	j->text = 0;
	j->ndisc = 0;
	j->nbox = 0;
	j->hm = 0;
	j->fade = 256;
	j->title_gen = 0;
	j->kind = FE_V_GAME;
	j->pic = NULL;
	j->banner[0] = 0;
	out->vsrc = br_vdp_snapshot();
	out->run = 1;
	out->held = 0;
	out->rec = 1;
	out->manual = 0;

	const uint16_t press = pad->press;
	pad_press = pad->press;
	swallow &= pad->held;
	const uint16_t held = pad->held & ~swallow;

	/* The pages over everything (test 8; test 9: OPTIONS and its pages, DIP SWITCHES, states, replays, the manual;
	 * test 10: no LB+RB shortcut any more, user: the title's menu and BACK's pause reach them). Outside a credit the
	 * 68000 holds and the music plays on; in
	 * a credit the whole board stands and fe_frame stops here, so a frame with a page changes nothing of the run (not
	 * even the pads' repeat counters) and the replay leaves it out. */
	int menu_input = 1;
	if (ovl) {
		ovl_input(pad);
		menu_input = 0;
	} else if (paused && in_credit() && pause_page(press & FE_A)) {
		menu_input = 0;
	}
	if (!menu_input && in_credit()) {
		out->run = 0;
		out->rec = 0;
		if (veil)
			j->kind = FE_V_BLACK;
		goto fields;                    /* (the pause box is not drawn under a page: its rows showed through) */
	}

	const uint16_t dirs = repeat_dirs(pad);
	const int up = dirs & FE_UP, down = dirs & FE_DOWN, left = dirs & FE_LEFT, right = dirs & FE_RIGHT;

	if (switch_due && !ovl) {
		switch_due = 0;
		title_keep = active && title_gen;
		switch_program(host.mode);
		switch_hush = 1;
		apply_volumes();
	}
	if (resetting == RESET_GOTO)
		goto_title();

	uint16_t in = 0;
	uint8_t sys = 0;
	int hold = ovl != OV_NONE;

	if (splash == SPL_ON) {
		/* spec 9.2: 229 frames; SHOT / START before 190 jumps to the 40-frame fade-out and then the title */
		hold = 1;
		j->kind = FE_V_SPLASH;
		if (menu_input && (press & (FE_A | FE_START)) && splash_t < SPLASH_SKIP_BEFORE) {
			splash_t = SPLASH_END - (splash_t < SPLASH_OUT ? splash_t : SPLASH_OUT);
			splash_skipped = 1;
		}
		j->fade = splash_t < SPLASH_IN ? splash_t * 256 / SPLASH_IN :
			splash_t > SPLASH_END - SPLASH_OUT ? (SPLASH_END - splash_t) * 256 / SPLASH_OUT : 256;
		if (!ovl && ++splash_t >= SPLASH_END) {
			splash = SPL_DONE;
			hold = 0;
			if (splash_skipped)
				goto_title();
			LOG("fe: splash over%s", splash_skipped ? " (skipped): the title" : "");
		}
	} else if (active) {
		/* the title (spec 8): the 68000's title runs on, its timers held while a page is open */
		title_display(out);
		sel_pal = FRONT_PAL_PINK;
		ov_clear();
		const int ready = title_keep || title_t >= TITLE_SHOW;
		if (launch_t >= 0) {
			j->fade = launch_t < FADE_FRAMES ? (FADE_FRAMES - launch_t) * 256 / FADE_FRAMES : 0;
			launch_t++;
			if (pulse_t >= PULSE_RETRY && launch_t < LAUNCH_TIMEOUT) {
				pulse = PULSE;
				pulse_t = 0;
			}
			pulse_t++;
		} else if (exit_t >= 0) {
			j->fade = exit_t < EXIT_FRAMES ? (EXIT_FRAMES - exit_t) * 256 / EXIT_FRAMES : 0;
			if (++exit_t > EXIT_FRAMES)
				req_kind = FE_REQ_EXIT;
		} else if (menu_input && ready && !start_due) {
			const int any = press || dirs;
			idle = any ? 0 : idle + 1;
			title_input(up, down, left, right, press);
		}
		if (launch_t >= 0 || start_due || exit_t >= 0)
			;
		else if (page == PG_PRACTICE)
			tr_build();
		else if (page == PG_PLAY)
			play_build();
		if (page == PG_TITLE && (title_keep || title_t >= FLASH_TEXT))
			draw_title_page();      /* the arcade's text comes with the flash */
		if (page != PG_TITLE || idle < IDLE_DEMO)
			br_wr16(TITLE_TIMER, 0x012C);
		br_wr16(TITLE_TIMER2, 0x04B0);
		if (pulse > 0) {
			sys = BR_START1;
			pulse--;
		}
	} else if (in_credit()) {
		if (launch_t >= 0 && launch_t < FADE_FRAMES) {
			/* the title fading out while the credit's first screen comes up */
			out->vsrc = title_gen ? &title_vdp : br_vdp_snapshot();
			j->kind = title_gen ? FE_V_TITLE : FE_V_BLACK;
			j->title_gen = title_gen;
			j->fade = (FADE_FRAMES - launch_t) * 256 / FADE_FRAMES;
			launch_t++;
		}
		if (veil && j->kind == FE_V_GAME) {
			j->kind = FE_V_BLACK;
			if (veil > 0)
				veil--;
		}
		sel_pal = P_AMBER;
		if (paused) {
			if (menu_input)
				pause_input(up, down, left, right, press);
		} else if (menu_input && (press & FE_BACK) && stage_running()) {
			paused = 1;
			pause_cur = 0;
			pause_armed = 0;
			pause_t = 0;
			LOG("fe: paused (board frame %u)", (unsigned)br_frame_count());
		} else if (!ovl) {
			/* test 10 (user: "in-game, free up the start button for the game's secret commands"): START is the
			 * board's in play too (hold START + BOMB: the score breakdown; + SHOT / SPECIAL: the autofire speed;
			 * + RIGHT / DOWN / LEFT: the bullets' colours), BACK alone pauses */
			in = (uint16_t)((held & (FE_UP | FE_DOWN | FE_LEFT | FE_RIGHT)) | game_buttons(held));
			if (held & FE_START)
				sys = BR_START1;
		}
		/* test 15: TEAM CUSTOM's SELECT TEAM confirmed by SHOT taps (the riders come after it, tr_custom_team) */
		if (practice && tr_custom && !paused && br_rd8(IN_PLAY) != 0xFF)
			in = (uint16_t)(frame_no & 4 ? BR_B1 : 0);
		if (paused) {
			pause_build();
			if (pause_t >= PAUSE_SE_FRAMES)
				out->run = 0;           /* the whole board stands: the music waits (spec 0.2) */
			else
				hold = 1;
			pause_t++;
		} else if (!ovl) {
			commands_frame();           /* test 14: SECRET COMMANDS */
		}
		if (!paused && practice && !ovl) {
			/* test 14: TRAINING's GAME SPEED: the 68000 held every 4th frame (75%) or every other (50%), the sound
			 * side running (the music keeps its speed); the run's values (tr_frame) */
			if (tr.speed == 1 ? frame_no % 4 == 3 : tr.speed == 2 ? (frame_no & 1) : 0)
				hold = 1;
			tr_frame();
		}
		if (!paused && !ovl && in_credit())
			db_frame();                 /* test 16: the DEBUG MENU's INFINITE LIVES / BOMBS */
		/* CONTINUES (spec 3); test 14: none in TRAINING (ibara360's: a GAME OVER is the TRAINING page again) */
		const int used = br_rd8(CONT_USED);
		br_wr8(CONT_OK, practice ? 0 : !br_rd8(CONT_SCREEN) ? (used < opt.continues ? 0xFF : 0) :
			(opt.continues < used ? 0 : 0xFF));
		/* Test 12 took a continue's score out of the rankings and its replay away; test 13 (ibara360's rules): the
		 * score red, the replay still offered, the ranking the game's (its 0x7DE8 refuses a continued player) */
		if (cont_taken())
			score_red("a continue");
	} else {
		/* boot, attract, demo: START -> the title; test 17 (hardware: the demo's music went on under the title): the
		 * jump skips the demo's own end, so the music is stopped as the SOUND page's STOP does */
		if (menu_input && (press & FE_START) && resetting < 0) {
			if (splash == SPL_WAIT)
				splash = SPL_DONE;
			goto_title();
			music_cmd(0x21, 0x00);
			music_cmd(0x20, 0xE0);
		}
		if (br_rd8(SCENE) == SCENE_TITLE && !booting)
			j->kind = FE_V_BLACK;       /* the arcade title's white flash before the title hook (test 2's prep) */
		if (title_keep && title_gen) {
			out->vsrc = &title_vdp;
			j->kind = FE_V_TITLE;
			j->title_gen = title_gen;
		}
	}

	if (out->run)
		snd_pump();                     /* (the pause's SE goes out while the 68000 is held) */
	out->held = out->run && hold;
	br_hold_main(out->held);
	br_set_inputs(in, sys);

fields:
	if (ovl)
		ovl_build();
	if (ovl == OV_MANUAL) {
		out->manual = 1;
		out->man_page = man_page;
		out->man_lang = host.language;
	}
	out->req = req_kind;
	out->req_arg = req_arg;
	out->invalid = red;
	out->rp_drop = rp_drop;
	out->rp_why = rp_why;
	out->credit_gen = credit_gen;
	req_kind = FE_REQ_NONE;
	apply_volumes();
	out->bright = host.bright;
	out->smooth = host.smooth;
	out->scan = host.scan ? host.strength : 0;
	out->wide = aspect_wide();
	out->picture = host.picture;
	{
		const int sh = pic_shape();
		out->pic_size = host.pic_size[sh];
		out->pic_sx = host.pic_sx[sh];
		out->pic_sy = host.pic_sy[sh];
		out->pic_x = host.pic_x[sh];
		out->pic_y = host.pic_y[sh];
	}
	out->screen = host.screen;
	out->gadgets = host.gadgets && !gadgets_off_why()[0];
	out->in_credit = in_credit();
	out->chase_credit = out->in_credit && prog_now == FE_CHASE;
	out->mode = host.mode;
	out->host_menu = ovl != OV_NONE;
	out->paused = paused;
	if (!pb_on)
		menu_se(sig0, paused0, press & 0x0FFF, rep_fired);
	out->se = se_due;
	se_due = 0;
	out->se_vol = host.mute ? 0 : host.effects * 256 / 100;
}

/* ---- after the board's frame ---- */

/* Hitbox discs (spec 6.2). The shape is the one the 68000 tests enemy bullets with (0x98AA): list = player +0x0A,
 * two word-offset steps by player +0x14 and +0x16, then {half along, offset along, half across, offset across}
 * (1/128 px) from the position +0x4A (along the 320 axis) / +0x4C (across, in the playfield: the screen follows the
 * ship sideways, 0x2034E2). Radius = the larger half size, at least 171 (N64). */
static int hit_level = 5;            /* 1 + the ship's sprite priority (4 in every credit measured in the harness) */

/* Test 14 (work/t14/r2/REPORT.md): x read unsigned (the ship's x runs to 0x8E00: the int16_t read lost the disc in the
 * screen's last fifth), values past 0xC000 negative; the boxes read after board frame f go with the next frame's
 * picture (the sprites trail the RAM by one frame), so each call hands over the last call's. HITBOX ALL adds the
 * enemies' boxes (entries up to 0x204056; colliding with the player, or only shot at) and the bullets' (up to
 * 0x204058), the 68000's own lists and tests. */
enum { CAMERA = 0x2034E2, ENEMIES = 0x20869A, ENEMY_SIZE = 114, ENEMY_TOP = 0x204056, BULLETS = 0x20B15A,
	BULLET_SIZE = 64, BULLET_TOP = 0x204058 };
static VdpDisc hb_disc[FE_DISCS];
static VdpBox hb_box[FE_BOXES];
static int hb_ndisc, hb_nbox;

static int hb_x(int v) { v &= 0xFFFF; return v >= 0xC000 ? v - 0x10000 : v; }

static void hb_put(uint32_t a, int px, int py, int cam, int kind)
{
	if (hb_nbox >= FE_BOXES)
		return;
	VdpBox *b = &hb_box[hb_nbox++];
	b->x = (int16_t)hb_x(px + (int16_t)br_rd16(a + 2));
	b->y = (int16_t)(py + (int16_t)br_rd16(a + 6) - cam);
	b->hw = (int16_t)br_rd16(a);
	b->hh = (int16_t)br_rd16(a + 4);
	b->kind = (uint8_t)kind;
}

static void hitbox_read(void)
{
	hb_ndisc = hb_nbox = 0;
	if (!host.hitbox || !in_credit() || br_rd8(ATTRACT) >= 0x40)
		return;
	const int cam = br_rd16(CAMERA) & 0xFF80;
	const uint32_t players[2] = { P1, P2 };
	for (int k = 0; k < 2 && hb_ndisc < FE_DISCS; k++) {
		const uint32_t p = players[k];
		if (!(br_rd8(p) & 0x80) || br_rd8(p + 0x115) == 0xFF)
			continue;
		uint32_t a = rd32(p + 0x0A) & 0xFFFFFF;
		if (!a || a >= 0x200000)
			continue;
		a += br_rd16(a + br_rd16(p + 0x14));
		a += br_rd16(a + br_rd16(p + 0x16));
		const int hx = (int16_t)br_rd16(a), dx = (int16_t)br_rd16(a + 2);
		const int hy = (int16_t)br_rd16(a + 4), dy = (int16_t)br_rd16(a + 6);
		if (!hx)
			continue;
		VdpDisc *d = &hb_disc[hb_ndisc++];
		d->x = hb_x(br_rd16(p + 0x4A) + dx);
		d->y = (int16_t)br_rd16(p + 0x4C) + dy - cam;
		d->r = hx > hy ? hx : hy;
		d->r = d->r > 171 ? d->r : 171;
		d->level = hit_level;
	}
	if (host.hitbox != HITBOX_ALL)
		return;
	int top = br_rd16(ENEMY_TOP);
	for (int i = 0; i <= top && i < 96; i++) {
		const uint32_t e = ENEMIES + (uint32_t)i * ENEMY_SIZE;
		const int s0 = br_rd8(e);
		if (!(s0 & 0x0A) || (br_rd8(e + 1) & 0x40) || br_rd8(e + 0x24) < br_rd8(e + 0x6E))
			continue;
		uint32_t a = rd32(e + 0x1A) & 0xFFFFFF;
		if (!a || a >= 0x210000)
			continue;
		for (int k = 0; k < 32 && br_rd16(a); k++, a += 8)
			hb_put(a, br_rd16(e + 0x2A), br_rd16(e + 0x2C), cam, s0 & 0x08 ? VDP_BOX_ENEMY : VDP_BOX_TARGET);
	}
	top = br_rd16(BULLET_TOP);
	for (int i = 0; i <= top && i < 96; i++) {
		const uint32_t b = BULLETS + (uint32_t)i * BULLET_SIZE;
		if (!(br_rd8(b) & 0x08))
			continue;
		const uint32_t a = rd32(b + 0x1A) & 0xFFFFFF;
		if (a && a < 0x210000 && br_rd16(a))
			hb_put(a, br_rd16(b + 0x2A), br_rd16(b + 0x2C), cam, VDP_BOX_BULLET);
	}
}

static void hitbox(FeVideoJob *j)
{
	memcpy(j->disc, hb_disc, sizeof hb_disc);
	memcpy(j->box, hb_box, (size_t)hb_nbox * sizeof hb_box[0]);
	j->ndisc = hb_ndisc;
	j->nbox = hb_nbox;
	hitbox_read();
}

void fe_after_board_frame(void)
{
	FeOut *out = cur_out;
	if (resetting >= 0 && out && out->run && !out->held)
		resetting++;                    /* the 68000's own frames since the reset */
	/* title scene frames (the logo's fade-in, the kept copy): only the 68000's own frames count. Test 9's hardware
	 * froze the title white: a page opened during the flash held the 68000 (its palette still whitened) while
	 * title_t ran on to TITLE_READY, and that frozen picture became the kept copy for good. */
	if (active && (!out || (out->run && !out->held))) {
		if (title_t < 0 && br_rd8(SCENE) == SCENE_TITLE) {
			title_t = 0;
			LOG("fe: title scene (board frame %u)", (unsigned)br_frame_count());
		} else if (title_t >= 0)
			title_t++;
	}
	if (switch_hush && resetting < 0 && title_t >= HUSH_END)
		switch_hush = 0;
	if (out) {
		out->in_credit = in_credit();
		/* the team as play starts (spec 1.4 step 3) */
		if (out->in_credit && !team_captured && (br_rd8(IN_PLAY) & 0x80) && !br_rd8(CONT_SCREEN) &&
			(br_rd16(P1) & 0x8000))
			team_capture();
		out->chase_credit = out->in_credit && prog_now == FE_CHASE;
		if (out->job.kind == FE_V_GAME)
			hitbox(&out->job);
	}
}

/* ---- video thread ---- */
static uint32_t vpic[BR_W * BR_H];
static uint32_t title_cache[FRONT_TEX_W * FRONT_TEX_H];
static uint32_t cache_gen, cache_hash;

static uint32_t ov_hash(const FeVideoJob *job)
{
	uint32_t h = 0x811C9DC5u;
	if (job->text)
		for (int c = 0; c < FE_COLS; c++)
			for (int l = 0; l < FE_LINES; l++)
				h = (h ^ job->ov[c][l]) * 0x01000193u;
	return h;
}

/* The job's cells into v's text layer; opaque ones through glyph copies with their transparent pixels made colour 9
 * in tile slots the layer does not use (N64 fn 0x8028D860). */
static void apply_ov(Vdp *v, const FeVideoJob *job)
{
	static uint8_t used[1024];
	static uint16_t remap[256];
	memset(used, 0, sizeof used);
	memset(remap, 0, sizeof remap);
	for (int i = 0; i < 0x1000; i++)
		used[v->txram[i] & 0x3ff] = 1;
	int next = 0x80;
	for (int c = 0; c < FE_COLS; c++)
		for (int l = 0; l < FE_LINES; l++) {
			const uint32_t o = job->ov[c][l];
			if (!(o & OV_SET))
				continue;
			uint16_t w = (uint16_t)o;
			if (o & OV_OPAQUE) {
				const int ch = w & 0xff;
				if (!remap[ch]) {
					while (next < 0x34A && (used[next] || (next >= 0x1A0 && next <= 0x200)))
						next++;
					if (next >= 0x34A)
						continue;
					const uint8_t *g = v->txgfx + ch * 64;
					uint8_t *d = v->txgfx + next * 64;
					for (int k = 0; k < 64; k++)
						d[k] = g[k] ? g[k] : 9;
					used[next] = 1;
					remap[ch] = (uint16_t)next++;
				}
				w = (uint16_t)((w & 0xFC00) | remap[ch]);
			}
			v->txram[c * 64 + l] = w;
		}
}

/* the menu box (test 9: ibara360's), in upright board pixels (ux across 0..239, uy down 0..319), 2x2 texels each */
static inline uint32_t *texel(uint32_t *tex, int ux, int uy)
{
	return tex + (ux * 2) * FRONT_TEX_W + (BR_W - 1 - uy) * 2;
}

static void hm_fill(uint32_t *tex, int x0, int y0, int x1, int y1, uint32_t c, int dark)
{
	for (int uy = y0; uy < y1; uy++)
		for (int ux = x0; ux < x1; ux++) {
			uint32_t *t = texel(tex, ux, uy);
			for (int k = 0; k < 4; k++) {
				uint32_t *p = t + (k >> 1) * FRONT_TEX_W + (k & 1);
				*p = dark ? 0xFF000000u | ((*p >> 2) & 0x3F3F3F) : c;
			}
		}
}

/* ibara360's 5x7 font (its main.c), bit 4 = leftmost column; ASCII 32..95, lower case folded to upper */
static const uint8_t font5[64][7] = {
	{0x00,0x00,0x00,0x00,0x00,0x00,0x00}, {0x04,0x04,0x04,0x04,0x04,0x00,0x04}, /*   ! */
	{0x0A,0x0A,0x00,0x00,0x00,0x00,0x00}, {0x0A,0x1F,0x0A,0x0A,0x0A,0x1F,0x0A}, /* " # */
	{0x04,0x0F,0x14,0x0E,0x05,0x1E,0x04}, {0x19,0x19,0x02,0x04,0x08,0x13,0x13}, /* $ % */
	{0x0C,0x12,0x14,0x08,0x15,0x12,0x0D}, {0x04,0x04,0x00,0x00,0x00,0x00,0x00}, /* & ' */
	{0x02,0x04,0x08,0x08,0x08,0x04,0x02}, {0x08,0x04,0x02,0x02,0x02,0x04,0x08}, /* ( ) */
	{0x00,0x04,0x15,0x0E,0x15,0x04,0x00}, {0x00,0x04,0x04,0x1F,0x04,0x04,0x00}, /* * + */
	{0x00,0x00,0x00,0x00,0x0C,0x04,0x08}, {0x00,0x00,0x00,0x1F,0x00,0x00,0x00}, /* , - */
	{0x00,0x00,0x00,0x00,0x00,0x0C,0x0C}, {0x01,0x01,0x02,0x04,0x08,0x10,0x10}, /* . / */
	{0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}, {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}, /* 0 1 */
	{0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}, {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E}, /* 2 3 */
	{0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}, {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}, /* 4 5 */
	{0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}, {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}, /* 6 7 */
	{0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}, {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}, /* 8 9 */
	{0x00,0x0C,0x0C,0x00,0x0C,0x0C,0x00}, {0x00,0x0C,0x0C,0x00,0x0C,0x04,0x08}, /* : ; */
	{0x02,0x04,0x08,0x10,0x08,0x04,0x02}, {0x00,0x00,0x1F,0x00,0x1F,0x00,0x00}, /* < = */
	{0x08,0x04,0x02,0x01,0x02,0x04,0x08}, {0x0E,0x11,0x01,0x02,0x04,0x00,0x04}, /* > ? */
	{0x0E,0x11,0x17,0x15,0x17,0x10,0x0E}, {0x0E,0x11,0x11,0x11,0x1F,0x11,0x11}, /* @ A */
	{0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}, {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}, /* B C */
	{0x1C,0x12,0x11,0x11,0x11,0x12,0x1C}, {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}, /* D E */
	{0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}, {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}, /* F G */
	{0x11,0x11,0x11,0x1F,0x11,0x11,0x11}, {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}, /* H I */
	{0x07,0x02,0x02,0x02,0x02,0x12,0x0C}, {0x11,0x12,0x14,0x18,0x14,0x12,0x11}, /* J K */
	{0x10,0x10,0x10,0x10,0x10,0x10,0x1F}, {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}, /* L M */
	{0x11,0x11,0x19,0x15,0x13,0x11,0x11}, {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}, /* N O */
	{0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}, {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}, /* P Q */
	{0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}, {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}, /* R S */
	{0x1F,0x04,0x04,0x04,0x04,0x04,0x04}, {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}, /* T U */
	{0x11,0x11,0x11,0x11,0x11,0x0A,0x04}, {0x11,0x11,0x11,0x15,0x15,0x15,0x0A}, /* V W */
	{0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}, {0x11,0x11,0x11,0x0A,0x04,0x04,0x04}, /* X Y */
	{0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}, {0x0E,0x08,0x08,0x08,0x08,0x08,0x0E}, /* Z [ */
	{0x10,0x10,0x08,0x04,0x02,0x01,0x01}, {0x0E,0x02,0x02,0x02,0x02,0x02,0x0E}, /* \ ] */
	{0x04,0x0A,0x11,0x00,0x00,0x00,0x00}, {0x00,0x00,0x00,0x00,0x00,0x00,0x1F}, /* ^ _ */
};
/* FE_C_*: white, yellow, green, cyan, grey, red (ibara360's card.h) */
static const uint32_t m_rgb[6] = { 0xFFFFFFFFu, 0xFFFFFF00u, 0xFF00FF00u, 0xFF00FFFFu, 0xFF909090u, 0xFFFF4040u };

static void text5(uint32_t *tex, int ux, int uy, const char *s, uint32_t c)
{
	for (; *s; s++, ux += 6) {
		char ch = *s;
		if (ch >= 'a' && ch <= 'z')
			ch -= 32;
		if (ch < 32 || ch > 95)
			ch = '?';
		const uint8_t *g = font5[ch - 32];
		for (int r = 0; r < 7; r++)
			for (int col = 0; col < 5; col++)
				if ((g[r] & (0x10 >> col)) && ux + col >= 0 && ux + col < BR_H && uy + r >= 0 && uy + r < BR_W) {
					uint32_t *t = texel(tex, ux + col, uy + r);
					t[0] = t[1] = t[FRONT_TEX_W] = t[FRONT_TEX_W + 1] = c;
				}
	}
}

/* a help in lines of at most w characters: broken at its '\n' or else before the word that would pass w; two at most
 * (ibara360's sm_help); *rest (if given): what did not fit in them ("" = all of it did) */
static int help_lines(const char *s, int w, char out[2][FE_HM_CHARS], const char **rest)
{
	int k = 0;
	if (w > FE_HM_CHARS - 1)
		w = FE_HM_CHARS - 1;
	for (; k < 2 && *s; k++) {
		int n = 0, cut = -1;
		while (s[n] && s[n] != '\n' && n < w) {
			if (s[n] == ' ')
				cut = n;
			n++;
		}
		if (s[n] && s[n] != '\n' && s[n] != ' ' && cut > 0)
			n = cut;
		memcpy(out[k], s, (size_t)n);
		out[k][n] = 0;
		s += n;
		while (*s == ' ' || *s == '\n')
			s++;
	}
	if (rest)
		*rest = s;
	return k;
}

/* the box: title (6 down), rows from 20 down 10 apart, the help 4 under them, then the lines (ibara360's layout) */
static void hm_draw5(uint32_t *tex, const FeVideoJob *job)
{
	const int w = job->hm_w ? job->hm_w : 228, cols = (w - 16) / 6;
	char hl[2][FE_HM_CHARS];
	const int nh = help_lines(job->hm_help, cols, hl, NULL);
	const int rows_y = job->hm_title[0] ? 20 : 6;
	const int help_y = rows_y + 10 * job->hm_n + (job->hm_n ? 4 : 0);
	const int lines_y = help_y + 10 * nh + (nh ? 4 : 0);
	int h = lines_y + 10 * job->hm_nl + 3;
	if (job->pic && h < rows_y + FE_THUMB_W + 8)
		h = rows_y + FE_THUMB_W + 8;
	const int bx = (BR_H - w) / 2, by = job->hm_top ? job->hm_top : (BR_W - h) / 2, x0 = bx + 8;
	if (!job->hm_plain) {
		hm_fill(tex, bx, by, bx + w, by + h, 0, 1);
		hm_fill(tex, bx, by, bx + w, by + 1, m_rgb[FE_C_WHITE], 0);
		hm_fill(tex, bx, by + h - 1, bx + w, by + h, m_rgb[FE_C_WHITE], 0);
		hm_fill(tex, bx, by, bx + 1, by + h, m_rgb[FE_C_WHITE], 0);
		hm_fill(tex, bx + w - 1, by, bx + w, by + h, m_rgb[FE_C_WHITE], 0);
	}
	if (job->hm_title[0])
		text5(tex, x0, by + 6, job->hm_title, m_rgb[FE_C_YELLOW]);
	if (job->hm_corner[0])
		text5(tex, bx + w - 8 - 6 * (int)strlen(job->hm_corner), by + 6, job->hm_corner, m_rgb[FE_C_DIM]);
	for (int i = 0; i < job->hm_n; i++) {
		const FeRow *r = &job->hm_row[i];
		const int y = by + rows_y + 10 * i, sel = i == job->hm_sel;
		if (sel)
			text5(tex, x0, y, ">", m_rgb[FE_C_YELLOW]);
		text5(tex, x0 + 6, y, r->name, m_rgb[r->name_c]);
		if (!job->hm_vcol || !r->value[0])
			continue;
		const int vx = x0 + 6 * job->hm_vcol;
		if (r->arrows) {
			/* "<  VALUE  >": the arrows at fixed columns (grey, yellow on the chosen row), the value centred */
			const uint32_t ac = m_rgb[sel ? FE_C_YELLOW : FE_C_DIM];
			const int len = (int)strlen(r->value) > 10 ? 10 : (int)strlen(r->value);
			text5(tex, vx, y, "<", ac);
			text5(tex, vx + 6 + 3 * (10 - len), y, r->value, m_rgb[r->value_c]);
			text5(tex, vx + 6 * 11, y, ">", ac);
		} else {
			text5(tex, vx, y, r->value, m_rgb[r->value_c]);
		}
	}
	for (int k = 0; k < nh; k++)
		text5(tex, x0, by + help_y + 10 * k, hl[k], m_rgb[job->hm_help_c]);
	for (int k = 0; k < job->hm_nl; k++)
		text5(tex, x0, by + lines_y + 10 * k, job->hm_line[k], m_rgb[job->hm_line_c[k]]);
	if (job->pic) {
		/* a save state's picture at the right (upright: FE_THUMB_H across, FE_THUMB_W down) */
		const int px = bx + w - 8 - FE_THUMB_H, py = by + rows_y - 2;
		for (int uy = 0; uy < FE_THUMB_W; uy++)
			for (int ux = 0; ux < FE_THUMB_H; ux++) {
				const uint16_t c = job->pic[ux * FE_THUMB_W + (FE_THUMB_W - 1 - uy)];
				const uint32_t rgb = 0xFF000000u | (uint32_t)(c >> 11) * 255 / 31 << 16 |
					(uint32_t)(c >> 5 & 63) * 255 / 63 << 8 | (uint32_t)(c & 31) * 255 / 31;
				hm_fill(tex, px + ux, py + uy, px + ux + 1, py + uy + 1, rgb, 0);
			}
	}
}

/* test 8: a replay's speed and time across the bottom (test 9: the 5x7 font) */
static void banner_draw5(uint32_t *tex, const char *s)
{
	hm_fill(tex, 0, BR_W - 16, BR_H, BR_W - 4, 0, 1);
	text5(tex, (BR_H - 6 * (int)strlen(s)) / 2, BR_W - 13, s, m_rgb[FE_C_YELLOW]);
}

/* Test 11 (user: "redesign the menus using the gadgets borders and font"): the box is a GADGETS panel (menudraw.h):
 * Baffo's frame, the title on its plate (2x and centred; 1x or at the left when it does not fit or a corner note is
 * there), the rows in the board's 8x8 font from the body's top a row's pitch apart (the chosen one on a dark amber
 * band), grid lines between them, the help and the lines under a grid line each. The title page's plain menu: no frame,
 * 2x text. Upright texels (the 5x7 box's units were two of them). hm_draw5 above is used until the board's font is in.
 * Test 12 (user: "make the options and pause menu font bigger, it's too small to navigate"): a box's rows, help and
 * lines at 1.5x (12 texels a letter) when the whole page fits so, else 1x as in test 11 (a line too long for 1.5x is
 * 1x alone); and (user: "some options don't fit into brackets, like in sound test") the "<  VALUE  >" window reaches
 * the box's right edge (test 11's was 10 letters wherever the box ended). */
enum { ROW_P = 14, LINE_P = 12, ROW_P3 = 20, LINE_P3 = 17, SEL_BAND = 0xFF3A2A08u };
static const uint8_t md_tint_of[6] = { MD_WHITE, MD_YELLOW, MD_GREEN, MD_CYAN, MD_GREY, MD_RED };

typedef struct {
	int sc, cw, rp, lp;                 /* the letters' size (half steps) and width, the row and line pitch */
	int win;                            /* the arrows' window in letters */
	int nh, hs, h;                      /* help lines and their size, the box's height */
	char hl[2][FE_HM_CHARS];
} HmFit;

/* Test 16 (user: "rank in training reverts to smaller font; this seems to be happening randomly in any option as you
 * keep updating, find a permanent fix"): test 12 measured all that showed (the chosen row's help, every value) and fell
 * to 1x for the whole page when one piece did not fit, so a page changed size as its cursor or values moved. Now the
 * size comes from the page's shape alone (its rows, lines and value column; two help lines whatever the row's help
 * is), is chosen when a page opens (its title and row count) and kept while it stays; a name, value or help too long
 * for 1.5x is drawn at 1x by itself (as test 12's lines were). */
enum { WIN_MIN = 6 };                   /* the arrows' window at 1.5x needs at least this many letters */

/* the page's layout at size sc (help lines at sc, or at 1x when they do not fit so); returns 1 = the box fits on the
 * screen with nh_min help lines at least and the arrows' window is WIN_MIN letters or more */
static int hm_fit(const FeVideoJob *job, int sc, int x0, int right, HmFit *f, int nh_min)
{
	const int plain = job->hm_plain;
	f->sc = sc;
	f->cw = 4 * sc;
	f->rp = plain ? 20 : sc == 3 ? ROW_P3 : ROW_P;
	f->lp = plain ? 18 : sc == 3 ? LINE_P3 : LINE_P;
	const int cw = f->cw, vx = x0 + 2 + cw * job->hm_vcol;
	/* all the room to the box's edge, so the arrows stay put while a value changes (MUSIC TEST's titles) */
	f->win = (right - vx) / cw - 2;
	if (f->win < 1)
		f->win = 1;
	const char *rest;
	f->hs = sc;
	f->nh = help_lines(job->hm_help, (right - x0) / cw, f->hl, &rest);
	if (*rest && sc == 3) {
		f->hs = 2;
		f->nh = help_lines(job->hm_help, (right - x0) / 8, f->hl, &rest);
	}
	const int rows_y = plain ? (job->hm_title[0] ? 26 : 0) : MD_BODY_Y + 8;
	int h = 0;
	for (int pass = 0; pass < 2; pass++) {   /* the box as drawn (f->nh), then with nh_min help lines for the test */
		const int nh = pass && nh_min > f->nh ? nh_min : f->nh;
		const int help_y = rows_y + f->rp * job->hm_n + (job->hm_n && nh ? 6 : 0);
		const int lines_y = help_y + f->lp * nh + (nh && job->hm_nl ? 6 : 0);
		h = lines_y + f->lp * job->hm_nl + (plain ? 0 : MD_BODY_END + 4);
		if (job->pic && h < rows_y + 2 * FE_THUMB_W + MD_BODY_END + 8)
			h = rows_y + 2 * FE_THUMB_W + MD_BODY_END + 8;
		if (!pass)
			f->h = h;
	}
	return (job->hm_top ? 2 * job->hm_top : 0) + h <= MD_H - 8 && (!job->hm_vcol || f->win >= WIN_MIN);
}

/* a piece of text at size sc when it ends by limit, else 1x (the y moved to keep it on the row's middle) */
static void hm_text_fit(int x, int y, int sc, int limit, int tint, const char *s)
{
	const int ls = sc == 3 && x + md_text_w(3, s) > limit ? 2 : sc;
	md_text(x, y + (sc - ls) * 2, ls, tint, s);
}

static void hm_draw(uint32_t *tex, const FeVideoJob *job, const uint8_t *glyphs)
{
	if (!md_font(glyphs)) {
		hm_draw5(tex, job);
		return;
	}
	md_target(tex);
	const int plain = job->hm_plain;
	const int w = 2 * (job->hm_w ? job->hm_w : 228), bx = (MD_W - w) / 2, x0 = bx + 18, right = bx + w - 16;
	const int px = bx + w - 18 - 2 * FE_THUMB_H;   /* a save state's picture's left */
	static HmFit f;                     /* (video thread only) */
	static char page_title[FE_HM_CHARS];
	static int page_n = -1, page_sc;
	if (plain) {
		hm_fit(job, 4, x0, right, &f, 0);
	} else {
		/* a new page: 1.5x when its shape fits so with two help lines; the same page keeps its size (1x for a frame
		 * only if the box as drawn would leave the screen) */
		if (job->hm_n != page_n || strcmp(job->hm_title, page_title)) {
			page_n = job->hm_n;
			snprintf(page_title, sizeof page_title, "%s", job->hm_title);
			page_sc = hm_fit(job, 3, x0, right, &f, 2) ? 3 : 2;
			LOG("menu: \"%s\" at %s (rows %d, box %d high from %d, arrows' window %d)", job->hm_title,
				page_sc == 3 ? "1.5x" : "1x", job->hm_n, f.h, job->hm_top, f.win);
		}
		if (page_sc == 2 || !hm_fit(job, 3, x0, right, &f, 0))
			hm_fit(job, 2, x0, right, &f, 0);
	}
	const int sc = f.sc, cw = f.cw, rp = f.rp, lp = f.lp, nh = f.nh, h = f.h;
	const int row_right = job->pic ? px - 6 : right;
	const int rows_y = plain ? (job->hm_title[0] ? 26 : 0) : MD_BODY_Y + 8;
	const int help_y = rows_y + rp * job->hm_n + (job->hm_n && nh ? 6 : 0);
	const int lines_y = help_y + lp * nh + (nh && job->hm_nl ? 6 : 0);
	const int by = job->hm_top ? 2 * job->hm_top : (MD_H - h) / 2;
	if (!plain) {
		md_panel(bx, by, w, h);
		/* the title on the plate */
		/* "<GAME>  <PAGE>" whole at 2x when it fits; else the page's name at 2x and the game's small and grey at the
		 * plate's right (where a corner note goes when there is one; 1.5x when that fits) */
		const char *title = job->hm_title, *note = job->hm_corner;
		const char *split = strstr(title, "  ");
		char game[FE_HM_CHARS];
		const int cl = note[0] ? md_text_w(2, note) + 12 : 0;
		if (md_text_w(4, title) + cl > w - 28 && split) {
			snprintf(game, sizeof game, "%.*s", (int)(split - title), title);
			title = split + 2;
			if (!note[0])
				note = game;
		}
		const int ns = note[0] && md_text_w(4, title) + md_text_w(3, note) + 12 <= w - 28 ? 3 : 2;
		const int clen = note[0] ? md_text_w(ns, note) + 12 : 0;
		const int ts = md_text_w(4, title) + clen <= w - 28 ? 4 : 2;
		const int tw = md_text_w(ts, title);
		const int tx = clen ? bx + 14 : bx + (w - tw) / 2;
		md_text(tx, by + MD_PLATE_Y + (MD_PLATE_H - 4 * ts) / 2, ts, MD_YELLOW, title);
		if (clen)
			md_text(bx + w - 14 - md_text_w(ns, note), by + MD_PLATE_Y + (MD_PLATE_H - 4 * ns) / 2, ns, MD_GREY, note);
	} else if (job->hm_title[0]) {
		md_text(x0, by, 4, MD_YELLOW, job->hm_title);
	}
	const int gx = bx + MD_BODY_X, gw = w - 2 * MD_BODY_X;
	for (int i = 0; i < job->hm_n; i++) {
		const FeRow *r = &job->hm_row[i];
		const int y = by + rows_y + rp * i, sel = i == job->hm_sel;
		if (!plain) {
			if (sel)
				md_fill(gx, y - 3, gw, rp - 1, SEL_BAND);
			md_hline(gx, y + rp - 4, gw);
		}
		if (sel)
			md_text(x0, y, sc, MD_YELLOW, ">");
		const int valued = job->hm_vcol && r->value[0], vx = x0 + 2 + cw * job->hm_vcol;
		hm_text_fit(x0 + cw + 2, y, sc, valued ? vx - 2 : row_right, md_tint_of[r->name_c], r->name);
		if (!valued)
			continue;
		if (r->arrows) {
			/* "<  VALUE  >": the arrows at the window's ends (grey, yellow on the chosen row), the value centred
			 * (test 16: a value longer than the window at 1x in it, cut only when that is too long too) */
			const int at = sel ? MD_YELLOW : MD_GREY;
			const int span = cw * f.win, vs = (int)strlen(r->value) * cw <= span ? sc : 2;
			char v[sizeof r->value];
			snprintf(v, sizeof v, "%.*s", span / (4 * vs), r->value);
			const int vw = md_text_w(vs, v);
			md_text(vx, y, sc, at, "<");
			md_text(vx + cw + (span - vw) / 2, y + (sc - vs) * 2, vs, md_tint_of[r->value_c], v);
			md_text(vx + cw * (f.win + 1), y, sc, at, ">");
		} else {
			hm_text_fit(vx, y, sc, right, md_tint_of[r->value_c], r->value);
		}
	}
	for (int k = 0; k < nh; k++)
		md_text(x0, by + help_y + lp * k + (sc - f.hs) * 2, f.hs, md_tint_of[job->hm_help_c], f.hl[k]);
	if (!plain && job->hm_nl)
		md_hline(gx, by + lines_y - 5, gw);
	for (int k = 0; k < job->hm_nl; k++)
		hm_text_fit(x0, by + lines_y + lp * k, sc, right, md_tint_of[job->hm_line_c[k]], job->hm_line[k]);
	if (job->pic) {
		/* a save state's picture at the right, twice its size (upright: FE_THUMB_H across, FE_THUMB_W down) */
		const int py = by + rows_y - 3;
		md_fill(px - 1, py - 1, 2 * FE_THUMB_H + 2, 2 * FE_THUMB_W + 2, 0xFF505050u);
		for (int uy = 0; uy < 2 * FE_THUMB_W; uy++)
			for (int ux = 0; ux < 2 * FE_THUMB_H; ux++) {
				const uint16_t c = job->pic[(ux >> 1) * FE_THUMB_W + (FE_THUMB_W - 1 - (uy >> 1))];
				md_fill(px + ux, py + uy, 1, 1, 0xFF000000u | (uint32_t)(c >> 11) * 255 / 31 << 16 |
					(uint32_t)(c >> 5 & 63) * 255 / 63 << 8 | (uint32_t)(c & 31) * 255 / 31);
			}
	}
}

/* test 8: a replay's speed and time across the bottom (test 11: the board's font on a dark band) */
static void banner_draw(uint32_t *tex, const char *s, const uint8_t *glyphs)
{
	if (!md_font(glyphs)) {
		banner_draw5(tex, s);
		return;
	}
	md_target(tex);
	md_darken(0, MD_H - 32, MD_W, 24, 48);
	md_text((MD_W - md_text_w(4, s)) / 2, MD_H - 28, 4, MD_YELLOW, s);
}

void fe_video(Vdp *v, const FeVideoJob *job, uint32_t *tex)
{
	const int keep = vdp_layers;
	switch (job->kind) {
	case FE_V_SPLASH:
		front_splash(tex);
		break;
	case FE_V_TITLE: {
		const uint32_t h = ov_hash(job);
		if (job->title_gen && job->title_gen == cache_gen && h == cache_hash) {
			memcpy(tex, title_cache, sizeof title_cache);
			break;
		}
		/* test 17 (user: the original copyright back): the arcade title's text lines 3-5 kept (the (C) / RAIZING /
		 * 8ING logo tiles, "1998", "CO.,LTD."; harness work/t17/v o_t), the rest of its text (FREE PLAY) cleared */
		for (int i = 0; i < 0x1000; i++)
			if (i % 64 < TITLE_COPY_LO || i % 64 > TITLE_COPY_HI)
				v->txram[i] = 0x20;
		front_text_palettes(v);
		if (job->text)
			apply_ov(v, job);
		/* test 10 (user: no N64 logos in any build): every layer, the arcade's own logo (TOP layer + sprites)
		 * included, so its white flash comes from the board's palette as on the arcade */
		vdp_layers = 0x1f;
		vdp_render(v, vpic);
		vdp_layers = keep;
		front_doubled(vpic, tex);
		if (job->title_gen) {
			memcpy(title_cache, tex, sizeof title_cache);
			cache_gen = job->title_gen;
			cache_hash = h;
		}
		break;
	}
	case FE_V_GAME:
		if (job->text)
			apply_ov(v, job);
		vdp_layers = 0x1f;
		vdp_render_discs(v, vpic, job->disc, job->ndisc, job->box, job->nbox);
		vdp_layers = keep;
		front_doubled(vpic, tex);
		break;
	case FE_V_WHITE:
		for (int i = 0; i < FRONT_TEX_W * FRONT_TEX_H; i++)
			tex[i] = 0xFFFFFFFFu;
		break;
	default:
		for (int i = 0; i < FRONT_TEX_W * FRONT_TEX_H; i++)
			tex[i] = 0xFF000000u;
		break;
	}
	front_fade(tex, job->fade);
	if (job->banner[0])
		banner_draw(tex, job->banner, v->txgfx);
	if (job->hm)
		hm_draw(tex, job, v->txgfx);
}

/* ---- test 8: save states, replays, the manual ---- */

/* The front end's part of a state: everything a credit runs on. Not the host settings, OPTIONS or the files'
 * rankings (the player's own, kept as they are now), nor the pages; with settings (a replay's start), OPTIONS
 * (button map, continues, GAME SETTINGS) and LANGUAGE too, as they were when it was recorded. */
void fe_state_io(Stio *s, int settings)
{
	const int prog_was = prog_now;
	STIO(s, prog_now);
	STIO(s, booting);
	STIO(s, splash);
	STIO(s, splash_t);
	STIO(s, splash_skipped);
	STIO(s, active);
	STIO(s, page);
	STIO(s, title_cur);
	/* test 14: the TRAINING page in test 13's places (its practice list's prac_cur, prac_first, end_cur: the layout
	 * stays, test 13's states load; theirs have no TR_PACKED and leave the page as it is) */
	int tr_a = tr_pack(), tr_b = tr.rank;
	STIO(s, tr_cur);
	STIO(s, tr_a);
	STIO(s, opt_cur);
	STIO(s, gs_cur);
	STIO(s, tr_b);
	if (s->load && (tr_a & TR_PACKED))
		tr_unpack(tr_a, tr_b);
	STIO(s, next_page);
	STIO(s, phase);
	STIO(s, prac_target);
	STIO(s, pulse);
	STIO(s, pulse_t);
	STIO(s, launch_t);
	STIO(s, idle);
	STIO(s, title_t);
	STIO(s, title_keep);
	STIO(s, title_quiet);
	STIO(s, practice);
	STIO(s, replay);
	STIO(s, restart_req);
	STIO(s, restore_team);
	STIO(s, team_captured);
	STIO(s, veil);
	STIO(s, team);
	STIO(s, team_ram);
	STIO(s, music_played);
	STIO(s, invuln);
	int kept_bits = not_kept | red << 1 | rp_drop << 2;   /* test 13: red and rp_drop in not_kept's bits 1-2 */
	STIO(s, kept_bits);
	if (s->load) {
		not_kept = kept_bits & 1;
		red = kept_bits >> 1 & 1;
		rp_drop = kept_bits >> 2 & 1;
	}
	STIO(s, snap_taken);
	STIO(s, snap);
	STIO(s, switch_due);
	STIO(s, resetting);
	STIO(s, paused);
	STIO(s, pause_cur);
	STIO(s, pause_t);
	STIO(s, rep_t);
	STIO(s, sq);
	STIO(s, sq_n);
	STIO(s, sq_wait);
	STIO(s, rk_def);
	STIO(s, rk_def_ok);
	STIO(s, rk_load_due);
	STIO(s, rk_reset_due);
	STIO(s, hit_level);
	if (settings) {
		STIO(s, opt);
		STIO(s, host.language);
	}
	if (!s->load || s->err)
		return;
	if (prog_now < 0 || prog_now > 1) {
		s->err = 1;
		prog_now = prog_was;
		return;
	}
	if (prog_now != prog_was)
		br_set_program(prog[prog_now]);
	if (settings)
		apply_language();
}

/* Test 13: a LOAD STATE in a credit = its score red and no replay (ibara360's); a playback's (quiet) is neither */
void fe_state_loaded(int quiet)
{
	state_loaded(quiet);
	if (!quiet && in_credit()) {
		score_red("LOAD STATE");
		replay_drop("LOAD STATE");
		if (db_lives || db_bombs || db_inv) {   /* test 16: the DEBUG MENU's cheats apply to the loaded run too */
			not_kept = 1;
			LOG("fe: DEBUG MENU cheats on the loaded run (score not kept)");
		}
	}
}

static void state_loaded(int quiet)
{
	ovl_depth = 0;
	ovl = OV_NONE;
	ov_armed = -1;
	ov_msg[0] = 0;
	pause_armed = 0;
	swallow = 0;
	req_kind = FE_REQ_NONE;
	settings_flush();
	if (quiet)
		return;
	/* the rankings as the files have them now (a state's RAM holds the tables of the time it was saved: kept, a
	 * newer score would be lost at the next title's check) */
	if (rk_def_ok) {
		const uint8_t *t = rk_have[prog_now] ? rk[prog_now] : rk_def;
		rk_write(t);
		if (snap_taken && !not_kept)
			memcpy(snap, t, RANK_BYTES);
	}
	if (in_credit() && stage_running() && !paused) {
		paused = 1;
		pause_t = PAUSE_SE_FRAMES;      /* the board stands at once, no SE */
	}
	pause_cur = PS_RESUME;              /* (a state saved from the pause menu has its cursor on SAVE STATE) */
	LOG("fe: state loaded: %s, %s", mode_word(prog_now), in_credit() ? (paused ? "paused" : "in a credit") : "no credit");
}

void fe_state_failed(int saving)
{
	snprintf(ov_msg, sizeof ov_msg, "NOT %s: SLOT %02d", saving ? "SAVED" : "LOADED", st_last + 1);
}

void fe_manual_pages(int en, int ja)
{
	man_pages[0] = en;
	man_pages[1] = ja;
}

void fe_replay_prompt(const char *l1, const char *l2, const char *l3, int red_score)
{
	prompt_red = red_score;
	snprintf(prompt_line[0], FE_HM_CHARS, "%s", l1);
	snprintf(prompt_line[1], FE_HM_CHARS, "%s", l2);
	snprintf(prompt_line[2], FE_HM_CHARS, "%s", l3);
	ovl_depth = 0;
	ovl = OV_NONE;
	ovl_open(OV_PROMPT);
}

void fe_open_replays(const char *msg)
{
	ovl_depth = 0;
	ovl = OV_NONE;
	open_replays();
	snprintf(ov_msg, sizeof ov_msg, "%s", msg);
}

void fe_playback(int on)
{
	swallow = 0;                    /* (as when its credit began) */
	pb_on = on;
	pb_paused = 0;
	pb_cur = 0;
	pb_banner = on ? 180 : 0;
}

int fe_playback_paused(void)
{
	return pb_paused;
}

/* test 9: the replay's pause menu as ibara360's (RESUME, RESTART REPLAY, QUIT REPLAY; A acts at once) */
enum { PB_RESUME, PB_RESTART, PB_QUIT, PB_ROWS };

static int playback_input(const FePad *pad);

unsigned fe_menu_sounds_take(void)
{
	const unsigned s = se_due;
	se_due = 0;
	return s;
}

int fe_playback_input(const FePad *pad)
{
	const uint32_t sig0 = menu_sig();
	const int r = playback_input(pad);
	menu_se(sig0, 0, pad->press & 0x0FFF, 0);
	return r;
}

static int playback_input(const FePad *pad)
{
	const uint16_t p = pad->press;
	if (p & FE_LB) {
		pb_banner = 120;
		return FE_PB_SLOWER;
	}
	if (p & FE_RB) {
		pb_banner = 120;
		return FE_PB_FASTER;
	}
	if (!pb_paused) {
		if (p & (FE_START | FE_BACK)) {
			pb_paused = 1;
			pb_cur = 0;
		}
		return FE_PB_NONE;
	}
	if (p & FE_UP)
		pb_cur = (pb_cur + PB_ROWS - 1) % PB_ROWS;
	if (p & FE_DOWN)
		pb_cur = (pb_cur + 1) % PB_ROWS;
	if ((p & FE_A) && pb_cur == PB_QUIT)
		return FE_PB_QUIT;
	if ((p & FE_A) && pb_cur == PB_RESTART) {
		pb_paused = 0;
		return FE_PB_RESTART;
	}
	if (p & (FE_A | FE_B | FE_START | FE_BACK)) {
		pb_paused = 0;
		pb_banner = 120;
	}
	return FE_PB_NONE;
}

void fe_playback_decorate(FeOut *out, const char *speed, uint32_t frame, uint32_t frames)
{
	FeVideoJob *j = &out->job;
	char t[2][12];
	const uint32_t f[2] = { frame / 60, frames / 60 };
	for (int i = 0; i < 2; i++)
		snprintf(t[i], sizeof t[i], "%u:%02u", (unsigned)(f[i] / 60 % 1000), (unsigned)(f[i] % 60));
	if (pb_paused) {
		J = j;
		m_begin(228, 0, "%s  REPLAY", game_word(prog_now));
		snprintf(j->hm_corner, sizeof j->hm_corner, "%s / %s", t[0], t[1]);
		m_row("RESUME", NULL, pb_cur == PB_RESUME, 0, 0);
		m_row("RESTART REPLAY", NULL, pb_cur == PB_RESTART, 0, 0);
		m_row("QUIT REPLAY", NULL, pb_cur == PB_QUIT, 0, 0);
		m_line(FE_C_DIM, "SPEED %s", speed);
		m_line(FE_C_GREEN, "B / BACK / START = RESUME");
		m_line(FE_C_GREEN, "LB / RB = REPLAY SPEED");
		j->banner[0] = 0;
		return;
	}
	if (pb_banner > 0) {
		pb_banner--;
		snprintf(j->banner, FE_HM_CHARS, "REPLAY %s  %s / %s  BACK = PAUSE", speed, t[0], t[1]);
	}
}
