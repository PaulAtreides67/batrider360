/* batrider360: GADGETS, the side panels. Every value Baffo's Batrider MAME layout v0.4 shows (design baf, Lua by
 * Olifante & DUBS, rank start bar by Miquel: ref/gadgets_baffo04/0.4/default.lay), plus the new STATUS, WEAPON,
 * STAGES and CHASE! panels, computed once a board frame from the 68000's RAM (0x200000-0x20FFFF), and the panels
 * drawn into the 360's 720p canvas around the game. */
#ifndef BR_GADGETS_H
#define BR_GADGETS_H
#include <stdint.h>

enum {
	GAD_NEXT_DROPS = 6,                /* Baffo's next-drop row */
	GAD_STAGES = 7,                    /* rows of the STAGES panel = stage numbers 1..7 */
	GAD_CONDS = 12,                    /* secret-boss conditions */
};

/* The seven drop counters, in the order of the gadgets' F1..F7 (F1 medals, F2..F7 the Item Count row). */
enum { GAD_MEDAL, GAD_POWERUP, GAD_BIG_POWERUP, GAD_OPTION, GAD_BOMB, GAD_BIG_BOMB, GAD_1UP, GAD_DROPS };
/* What the next drops are (Baffo's drop_viewer pictures). */
enum { GAD_ITEM_MEDAL, GAD_ITEM_POWERUP, GAD_ITEM_OPTION, GAD_ITEM_BIG_POWERUP };
/* Secret-boss condition states (Baffo's checkbox: 0 empty, 1 checked, 2 the minus = impossible for this team). */
enum { GAD_COND_OPEN, GAD_COND_MET, GAD_COND_NA };
/* The CHASE! chain timer's colour, as CHASE!'s own rail draws it. */
enum { GAD_TIMER_BLUE, GAD_TIMER_RED, GAD_TIMER_REST };

typedef struct {
	int active;                        /* P status 0x204062 / 0x2041D4 bit 15: the player is in the game */
	int pilot, next, third;            /* rider ids 0..17 ((word >> 4) & 31) or -1: current 0x204074 / 0x2041E6,
	                                      the next two in team order from the team list 0x20407C / 0x2041EE */
	int next_down, third_down;         /* team word bit 0: that rider has been shot down */
	int type;                          /* 0..3 = A..D, (current >> 2) & 3: the button the rider was chosen with */
	uint32_t score;                    /* 0x204156 / 0x2042C8 */
	int shot_charge;                   /* 0x204179 / 0x2042EB, 0..0x48 (full) */
	int in;                            /* Baffo's input byte: stick bits 0-3 (1 up 2 down 4 left 8 right), 0x10 A
	                                      (shot), 0x20 B (bomb), 0x40 C (special), 0x80 S (start); 0x203456 / 0x203457
	                                      = the port as the game read it, start from SYS 0x203459 */
	/* WEAPON (player struct + 0x104..0x118; found 2026-11-01 from the item pickups at 0x2FC64.., see gadgets.c) */
	int shot_lv;                       /* +0x110 + 1: 1..4 */
	int shot_p, shot_p_need;           /* +0x111 small P items collected toward the next level, of 2 / 3 / 4 */
	int options;                       /* +0x118: options flying (0..4) */
	int option_lv;                     /* +0x104: option power 0..2 (2 = SPECIAL OPTION / full) */
	int bombs;                         /* +0x112: bombs in stock, 0..4 */
	int bomb_gauge;                    /* +0x113: small-bomb gauge 0..40 (40 = a bomb) */
	int rate;                          /* +0x10C: 0..7, the game's START+button rate popup "#60".."#7" (0x8972) */
	int formation;                     /* the options' formation (test 6), only the Battle Garegga guests (riders 9-12)
	                                      have one: player +0x22 = 0 V (also the default and after a respawn), 2 BACK,
	                                      4 FRONT, 6 CIRCLE, 8 CONTROL (the commands 623A 282A 828A 41236A 421A, or C
	                                      stepping 0-2-4-6-8; matcher 0x3080C, table 0x30862), 0x0A..0x12 the special
	                                      ones an option item sets at a counter of 5 (0x2FEC8); here 0..4 in that
	                                      order, 5 = special, -1 = none (another rider) */
} GadgetPlayer;

typedef struct {
	int in_credit;                     /* 0x20E69D & 0xC0 == 0: a real credit (not attract, demo, name entry) */
	int course;                        /* 0x20E6A6: 0 TRAINING, 2 NORMAL, 4 ADVANCE, 6 SPECIAL; >= 8 = stage-edit
	                                      numbers (idx + 4) * 2: the demo, and PRACTICE credits */

	/* RANK (Baffo + Miquel): rank 0x20F9D0 (u32, lower = harder), rank start (easy-end cap) 0x20F9D8 */
	uint32_t rank, rank_start;
	int32_t rank_ppm;                  /* Baffo's six digits: |1 - rank / 0xFF0000| * 1000000, shown as ab.cdef % */
	int32_t rank_bar_q16;              /* bar fill 0..65536: max(0, 1 - rank / 0xFF0000) (red) */
	int32_t rank_start_bar_q16;        /* the same for rank start (dark red, drawn first) */

	/* DROPS: order counter 0x20E457; counters 0x20E45A.. (see the GAD_ enum), Baffo's caps (red at the cap) */
	int next_drop;
	int next_items[GAD_NEXT_DROPS];    /* GAD_ITEM_ of drops next_drop+0..+5 (25-step cycle) */
	int drops[GAD_DROPS];
	int drop_at_cap[GAD_DROPS];
	int medal_rung;                    /* 0x20E459: even 0..0x24 = the next medal's step; odd = the top step */
	int medal_step;                    /* 0..18 (Baffo's medal_display state / 2) */
	int medal_value;                   /* 100 .. 10000 */

	/* BOSS (Baffo's BOSSES table over the 96 enemies at 0x20869A, 114 bytes each) */
	int boss_hp;                       /* -1 = none */
	int32_t boss_timer;                /* frames, -1 = none; Baffo shows boss_timer / 60 */
	int boss_secs;                     /* -1 = none */
	const char *boss_name;             /* the BOSSES name of the part the HP came from (else the timer's), or NULL */

	/* PLAYERS */
	GadgetPlayer p[2];
	int current;                       /* 0 = P1, 1 = P2: whose input changed last (Baffo's current_player) */

	/* SECRET BOSSES: Baffo's 12 checkboxes in its order (gad_cond_names) */
	int cond[GAD_CONDS];
	int sp_power, sp_option;           /* 0x20E463 / 0x20E464: SPECIAL POWER UP / SPECIAL OPTION taken */

	/* STATUS */
	int stage_no;                      /* 0x20E6A9: 1.. the stage's place in the course (0 = none) */
	int stage_scene;                   /* 0x20E6AA: the map: 1..7 stages, 8..11 secret bosses, 24..37 bosses */
	const char *stage_name;            /* gad_scene_name(stage_scene) */
	uint32_t stage_frames;             /* time in this stage, frames (the game keeps no stage clock: counted here
	                                      from the game's own vblank counter 0x20FD46 while the stage runs) */
	int stage_min, stage_sec;
	int enemies;                       /* enemies alive: flags bit 15 and HP > 0 */
	int bullets;                       /* enemy bullets: 96 at 0x20B15A, 64 bytes, (flags & 0xB8) == 0x88, not +1 bit 0
	                                      (CHASE!'s own count) */

	/* STAGES (P1's score made in each stage number, P2's beside it) */
	int stage_row_scene[GAD_STAGES];   /* the map of each row: 1, the stage order 0x20F9E0 + 1 (x3), 5, 6, 7; 0 = ? */
	uint32_t stage_score[2][GAD_STAGES];
	uint32_t stage_time[GAD_STAGES];   /* frames */
	int stage_played[GAD_STAGES];
	uint32_t stage_total[2];
	int stage_cur;                     /* row of the stage being played, -1 = none */
	/* test 13 (ibara360's red SCORE): set by main.c after gadgets_update: the credit's score is invalid (TOTAL red), the
	 * rows it went invalid in (red) */
	int invalid;
	uint8_t stage_red[GAD_STAGES];

	/* CHASE! (its state block 0x201000, batrider-chase src/chase.s) — zero unless chase_mode */
	int chase;                         /* cs_state 0x201000 == 2: a CHASE! credit in play */
	uint32_t chain, max_chain;         /* cs_chain 0x201004, cs_maxchain 0x201008 */
	int mult;                          /* 1..8 = min(chain, 112) / 16 + 1 */
	int timer, timer_max;              /* cs_timer 0x20100C of 90; while resting cs_rest 0x201014 of 300 */
	int32_t timer_q16;                 /* the rail's fill 0..65536 */
	int timer_state;                   /* GAD_TIMER_ */
	int timer_frozen;                  /* cs_pause 0x201016: frozen after a death / bomb */
	int break_ready;                   /* the HUD's C: mult >= 4 and not resting */
	int forward;                       /* cs_fwd 0x201012: flying in the upper half (amber up arrow) */
	int lives;                         /* riders flying (team 0x20407C, TEAMSZ 0x20F9FD) + banked cs_bank 0x201026 */
	int banked;
	uint32_t next_extend;              /* cs_nextext 0x201022 */
	uint32_t to_extend;                /* next_extend - P1 score, 0 = none */
	int msg;                           /* cs_msg 0x20101E while cs_msgt: 1 EXTEND, 2 MAX LIVES, 3 CASH IN; else 0 */

	/* MUSIC (test 6): the tune playing, from the board (br_music: the 68000's 0x20,nn sound command), set by the
	 * caller with gadgets_music after gadgets_update */
	int music;                         /* 0x00..0x19 (MUSIC TEST's numbers), -1 = none */
	char music_title[24];              /* the sound ROM's own title (snd.u77 0x3E200), "" = none */
	const char *music_by;              /* the composer(s), short form; "" = none */
} GadgetState;

extern const int gad_drop_caps[GAD_DROPS];        /* 0 = none (medals) */
extern const char *const gad_cond_names[GAD_CONDS];
const char *gad_scene_name(int scene);            /* "" when unknown */

void gadgets_reset(void);              /* a new credit: forget the stage scores, times and input history */
void gadgets_update(GadgetState *g, const uint8_t *ram, int chase_mode);
void gadgets_music(GadgetState *g, int tune, const char *title);   /* br_music(), br_music_title() */

/* ---- drawing (gadgets_draw.c) ---- */
enum { GAD_YOKO, GAD_TATE };
enum { GAD_YOKO_W = 1280, GAD_YOKO_H = 720, GAD_TATE_W = 720, GAD_TATE_H = 1280 };
/* The game's rectangle in the canvas (drawn by the GPU, left alone here). */
enum { GAD_YOKO_GX = 370, GAD_YOKO_GY = 0, GAD_YOKO_GW = 540, GAD_YOKO_GH = 720,
       GAD_TATE_GX = 0, GAD_TATE_GY = 160, GAD_TATE_GW = 720, GAD_TATE_GH = 960 };

int gadgets_init(void);                /* unpack the baked art; 0 = ok */
/* canvas 0xAARRGGBB, viewer-upright, GAD_YOKO_W x H or GAD_TATE_W x H by screen; chase = show the CHASE! panels;
 * glyphs = br_text_glyphs() (the board's font, cached on the first call that has it). */
void gadgets_draw(uint32_t *canvas, int screen, int chase, const GadgetState *g, const uint8_t *glyphs);
void gadgets_blank(uint32_t *canvas, int screen);
/* Test 11: Baffo's list frame in nine pieces for the menus (core/menudraw.c): top band (title plate), body band,
 * bottom band, each left / middle / right; RGBA, NULL before gadgets_init. Read-only after gadgets_init. */
enum { GAD_MK_TL, GAD_MK_TM, GAD_MK_TR, GAD_MK_BL, GAD_MK_BM, GAD_MK_BR, GAD_MK_EL, GAD_MK_EM, GAD_MK_ER, GAD_MK_N };
const uint8_t *gadgets_menu_piece(int i, int *w, int *h);

#endif
