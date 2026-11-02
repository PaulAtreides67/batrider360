/* batrider360: GADGETS' numbers, from the 68000's RAM once a board frame.
 * Baffo's part is a port of the Lua in ref/gadgets_baffo04/0.4/default.lay (Olifante & DUBS, rank start by Miquel),
 * value for value, including its quirks (noted where they matter). The rest was found for the 360:
 * - WEAPON: the player struct's +0x104..+0x118, read off the item pickups (0x2FC64 small P: +0x111 counts to the
 *   table 2/3/4 at 0x2FCDE, then +0x110 steps up to 3, or straight to 3 with SPECIAL POWER UP 0x20E463; 0x2FD24
 *   small B: +0x113 gauge to 40 = one more bomb +0x112, at most 4; 0x2FEC8 option: +0x104 to 2, SPECIAL OPTION
 *   0x20E464; +0x118 = options spawned (0x6718/0x6762)), the respawn reset 0xE62.., and the SPECIAL course's FULL
 *   POWER UP (0xD42: +0x110 = 3, +0x104 = 2). Checked in the PC harness against the bombs on the screen.
 * - STATUS: stage number 0x20E6A9 and map 0x20E6AA (the HUD's stage name is the ROM table at 0x11C0C indexed by
 *   it; checked on the screen: 1 M.CITY, 2 AIRPORT, 3 SKY HIGH, 4 S.SYSTEM); the stage order 0x20F9E0 holds the
 *   maps of stages 2..4 minus one. The game keeps no stage clock, so the time is counted here.
 * No allocation; all state is static (one board). */
#include "gadgets.h"

#include <string.h>

#include "stio.h"

/* ---- RAM ---- */
enum {
	P1 = 0x204062, P2 = 0x2041D4,      /* player structs, 0x172 bytes */
	PL_PILOT = 0x12, PL_TEAM = 0x1A, PL_FORM = 0x22, PL_SCORE = 0xF4, PL_OPTLV = 0x104, PL_RATE = 0x10C,
	PL_SHOTLV = 0x110, PL_SHOTP = 0x111, PL_BOMBS = 0x112, PL_BGAUGE = 0x113, PL_CHARGE = 0x117, PL_OPTS = 0x118,
	IN_P1 = 0x203456, IN_P2 = 0x203457, IN_SYS = 0x203459,
	NEXT_DROP = 0x20E457, MEDAL = 0x20E459, DROP_P = 0x20E45A, DROP_B = 0x20E45B, DROP_M = 0x20E45C,
	DROP_BIGP = 0x20E45D, DROP_BIGB = 0x20E45E, DROP_OPT = 0x20E45F, DROP_1UP = 0x20E460,
	SP_POWER = 0x20E463, SP_OPTION = 0x20E464,
	DEMO = 0x20E69D, COURSE = 0x20E6A6, STAGE_NO = 0x20E6A9, SCENE = 0x20E6AA,
	COND_GSQUID = 0x20E6B2, COND_BASHIR, COND_BASHIMK2, COND_GOBROBO, COND_BASHI, COND_BHEART,
	GLOBAL_TIMEOUT = 0x20E9B8,
	RANK = 0x20F9D0, RANK_START = 0x20F9D8, STAGE_ORDER = 0x20F9E0, TEAMSZ = 0x20F9FD, VBLANKS = 0x20FD46,
	ENEMIES = 0x20869A, ENEMY_N = 96, ENEMY_SIZE = 114,
	BULLETS = 0x20B15A, BULLET_N = 96, BULLET_SIZE = 64,
	CS = 0x201000,                     /* CHASE!'s state */
	CS_STATE = CS + 0x000, CS_CHAIN = CS + 0x004, CS_MAXCHAIN = CS + 0x008, CS_TIMER = CS + 0x00C,
	CS_FWD = CS + 0x012, CS_REST = CS + 0x014, CS_PAUSE = CS + 0x016, CS_MSG = CS + 0x01E, CS_MSGT = CS + 0x020,
	CS_NEXTEXT = CS + 0x022, CS_BANK = CS + 0x026,
};
/* CHASE!'s rules (chase.s .equ) */
enum { TIMER_MAX = 90, WARN = 45, CHAIN_CAP = 112, MULT_STEP = 16, CASH_MIN = 4, CASH_REST = 300 };

static const uint8_t *R;
static int rd8(uint32_t a) { return R[a - 0x200000]; }
static int rd16(uint32_t a) { return R[a - 0x200000] << 8 | R[a - 0x200000 + 1]; }
static uint32_t rd32(uint32_t a) { return (uint32_t)rd16(a) << 16 | (uint32_t)rd16(a + 2); }

/* ---- tables ---- */
const int gad_drop_caps[GAD_DROPS] = { 0, 3, 3, 3, 5, 3, 3 };   /* default.lay's colour callbacks */
static const uint32_t drop_addr[GAD_DROPS] = { DROP_M, DROP_P, DROP_BIGP, DROP_OPT, DROP_B, DROP_BIGB, DROP_1UP };

/* default.lay's drop_viewer states 1..25 = the order counter + 1 (mod 25): medal, P, medal, P, option, five times
 * round, the 25th a big P (the manual: "the last of them a large power-up"). */
static int drop_item(int k)
{
	k %= 25;
	if (k == 24)
		return GAD_ITEM_BIG_POWERUP;
	if (k % 5 == 4)
		return GAD_ITEM_OPTION;
	return (k & 1) ? GAD_ITEM_POWERUP : GAD_ITEM_MEDAL;
}

static const int medal_values[19] = { 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000,
	2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000 };

/* Baffo's checkbox order */
const char *const gad_cond_names[GAD_CONDS] = {
	"BASHINET", "BASHI mk2", "BASHI-R", "GOB-ROBO", "B.HEART", "B.HEART mk2", "G.SQUID", "TSUMUJI", "HAYATE",
	"ENVY", "BLUNT", "SOBUT",
};

/* the HUD's names (ROM 0x11C0C, spaces trimmed; HIGH-WAY written as the stage list's HIGH WAY), by map */
const char *gad_scene_name(int s)
{
	static const char *const n[38] = {
		"", "M.CITY", "AIRPORT", "SKY HIGH", "S.SYSTEM", "HIGH WAY", "Z.CITY", "BUILDING",
		"BASHI mk2", "BASHI-R", "B.H.mk2", "G.SQUID", "", "", "", "", "", "", "", "", "", "", "", "",
		"CONFRICT", "BAZZCOK", "BOREDOM", "DEVIATE", "ENVY", "SOBUT", "BLUNT", "GRUBBY", "DISCHARGE",
		"BASHINET", "GOB-ROBO", "B.HEART", "TSUMUJI", "HAYATE",
	};
	return (s >= 0 && s < 38) ? n[s] : "";
}

/* default.lay's SHIP_ORIGINS / POWERS / SHOTS by rider id */
static const uint8_t ship_origin[18] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 2 };
static const uint8_t ship_power[18] = { 0, 2, 3, 3, 0, 3, 2, 1, 1, 0, 1, 0, 1, 1, 0, 2, 0, 2 };
static const uint8_t ship_shot[18] = { 1, 2, 0, 1, 0, 2, 0, 2, 1, 3, 3, 3, 3, 0, 1, 1, 2, 0 };

/* Baffo's BOSSES: per enemy script (enemy +2, u32), the parts whose HP is shown (status & mask == value) and the
 * parts whose timer is shown, with the timer's source (TIMEOUT_LOGICS 1..7, -1 the global 0x20E9B8). Part '*' =
 * any part (TSUMUJI, HAYATE). */
enum { ANY = -2 };
typedef struct { int16_t part; uint16_t mask, value; } HpPart;
typedef struct { int16_t part; int8_t src; } TimePart;
typedef struct {
	uint32_t script;
	const char *name;
	HpPart hp[6];
	TimePart time[6];
} Boss;
#define H(p) { p, 0xF001, 0x8000 }
static const Boss bosses[] = {
	{ 0x0531B4, "CONFRICT", { { 0, 0xF201, 0x8200 }, H(118) }, { { 0, 2 } } },
	{ 0x054E38, "BAZZCOK", { H(0) }, { { 0, 4 }, { 6, 4 }, { 48, 4 }, { 50, 4 }, { 52, 4 }, { 54, 4 } } },
	{ 0x0566E2, "BOREDOM", { H(0) }, { { 0, 5 } } },
	{ 0x0581C8, "DEVIATE", { H(0), H(4), H(6) }, { { 100, 4 } } },
	{ 0x0585B8, "DEVIATE", { { -1 } }, { { 100, 4 } } },
	{ 0x060CC4, "GRUBBY", { H(0), H(2) }, { { 0, 1 } } },
	{ 0x063AB2, "DISCHARGE", { H(0), H(2) }, { { 0, -1 }, { 2, -1 } } },
	{ 0x06563A, "DISCHARGE", { H(0) }, { { 0, 2 } } },
	{ 0x065BFC, "DISCHARGE", { H(2) }, { { 2, 2 } } },
	{ 0x065D5A, "DISCHARGE", { H(2) }, { { 2, 2 } } },
	{ 0x065E82, "DISCHARGE", { H(2) }, { { 2, 2 } } },
	{ 0x075AE2, "AIRPORT", { H(0) }, { { 0, -1 } } },
	{ 0x07B282, "Z.CITY", { H(0), H(2) }, { { 0, -1 }, { 2, -1 } } },
	{ 0x07BB96, "Z.CITY", { H(0) }, { { 0, -1 } } },
	{ 0x07D424, "Z.CITY", { H(0) }, { { 0, -1 } } },
	{ 0x07A37C, "BUILDING", { H(18) }, { { 18, -1 } } },
	{ 0x079B0C, "BUILDING", { H(0) }, { { 0, -1 } } },
	{ 0x0799BE, "BUILDING", { H(28) }, { { 28, -1 } } },
	{ 0x04DE40, "BASHINET", { H(0), H(2) }, { { 2, 2 } } },
	{ 0x0516D0, "BASHImk2", { H(0) }, { { 2, 2 } } },
	{ 0x049960, "BASHI-R", { H(2) }, { { 2, 2 } } },
	{ 0x04C7F4, "GOB-ROBO", { H(0) }, { { 42, 2 } } },
	{ 0x067CAA, "B.HEART", { H(0) }, { { 0, 3 } } },
	{ 0x069456, "B.H.mk2", { H(0) }, { { 0, 3 } } },
	{ 0x06AD02, "G.SQUID", { H(0) }, { { 0, 2 } } },
	{ 0x06AE04, "G.SQUID", { H(116) }, { { 116, 6 } } },
	{ 0x06D604, "TSUMUJI", { H(2), H(10) }, { { ANY, 7 } } },
	{ 0x06E6C2, "HAJATE", { H(0), H(2), H(4), H(6), H(8), H(10) }, { { ANY, 7 } } },
	{ 0x05A348, "ENVY", { H(0) }, { { 0, 2 } } },
	{ 0x05ED6A, "BLUNT", { H(0) }, { { 0, 2 } } },
	{ 0x05C73E, "SOBUT", { H(0) }, { { 0, 2 } } },
};
#undef H
/* Unused slots are zero ({0, 0, 0} for hp, {0, 0} for time): a zero mask / zero source marks the end. DEVIATE
 * 0x0585B8 has no HP parts: its {-1} has mask 0 and ends the list at once. */

/* TIMEOUT_LOGICS of default.lay */
static int32_t timeout(uint32_t e, int src)
{
	switch (src) {
	case 1: return (int32_t)rd32(e + 66);
	case 2: return (int32_t)rd32(e + 70);
	case 3: return -rd16(e + 72);
	case 4: return rd16(e + 74);
	case 5: return rd16(e + 104);
	case 6: return rd16(e + 70);
	case 7: return rd8(e + 96) * 60;
	default: return rd16(GLOBAL_TIMEOUT);
	}
}

/* Composers of tunes 0x00..0x19 (MUSIC TEST's numbers; the titles are the sound ROM's own). The game credits Namiki,
 * Koyano and Sakimoto; the split per tune is the VGMRips pack's GD3 tags (after Santaruru = Namiki's MDX set), the
 * guest bosses' tunes are Mahou Daisakusen's (Motoyama) and Shippu Mahou Daisakusen's (Iwata), arranged by Namiki. */
static const char *const music_by[0x1A] = {
	"", "H.Sakimoto", "M.Namiki", "M.Namiki", "M.Namiki", "M.Namiki", "M.Namiki", "M.Namiki",
	"K.Koyano/H.Sakimoto", "K.Koyano/H.Sakimoto", "M.Namiki", "M.Namiki", "K.Koyano/H.Sakimoto",
	"A.Motoyama/M.Namiki", "M.Iwata/M.Namiki", "M.Namiki", "K.Koyano/H.Sakimoto", "M.Namiki", "M.Namiki",
	"M.Namiki", "M.Namiki", "M.Namiki", "M.Namiki", "M.Namiki", "M.Namiki", "H.Sakimoto",
};

void gadgets_music(GadgetState *g, int tune, const char *title)
{
	g->music = tune >= 0 && tune < 0x1A && title && *title ? tune : -1;
	g->music_title[0] = 0;
	g->music_by = "";
	if (g->music < 0)
		return;
	strncpy(g->music_title, title, sizeof g->music_title - 1);
	g->music_title[sizeof g->music_title - 1] = 0;
	g->music_by = music_by[tune];
}

/* ---- history ---- */
static struct {
	int last_in[2], current;
	int was_in_credit;
	int cur_row;
	uint32_t last_score[2];
	uint16_t last_vbl;
	uint32_t score[2][GAD_STAGES], time[GAD_STAGES];
	int played[GAD_STAGES];
} H;

void gadgets_reset(void)
{
	memset(&H, 0, sizeof H);
	H.last_in[0] = H.last_in[1] = 0xFF;  /* default.lay: p1_last_input = 0xff, so the first frame sets P1 */
	H.cur_row = -1;
}

/* test 8: the history (STAGES' scores and times, the current player) goes into save states */
void gadgets_state_io(Stio *s)
{
	STIO(s, H);
}

static void player(GadgetPlayer *p, uint32_t base, int in_addr, int start_bit)
{
	p->active = (rd16(base) & 0x8000) != 0;
	const int cur = rd16(base + PL_PILOT);
	int team[3];
	for (int i = 0; i < 3; i++)
		team[i] = rd16(base + PL_TEAM + 2 * i);
	/* default.lay: the current rider's place in the first three team words (1 when not found), then the next two
	 * round the three */
	int k = 0;
	for (int i = 0; i < 3; i++)
		if (team[i] == cur)
			k = i;
	const int nx = team[(k + 1) % 3], th = team[(k + 2) % 3];
	p->pilot = p->active && cur != 0xFFFF ? (cur >> 4) & 31 : -1;
	p->next = p->active && nx != 0xFFFF ? (nx >> 4) & 31 : -1;
	p->third = p->active && th != 0xFFFF ? (th >> 4) & 31 : -1;
	p->next_down = nx != 0xFFFF && (nx & 1);
	p->third_down = th != 0xFFFF && (th & 1);
	p->type = (cur >> 2) & 3;
	p->score = rd32(base + PL_SCORE);
	p->shot_charge = rd8(base + PL_CHARGE);
	p->in = (rd8(in_addr) & 0x7F) | ((rd8(IN_SYS) & start_bit) ? 0x80 : 0);
	p->shot_lv = rd8(base + PL_SHOTLV) + 1;
	p->shot_p = rd8(base + PL_SHOTP);
	static const uint8_t need[4] = { 2, 3, 4, 0 };   /* 0x2FCDE */
	p->shot_p_need = need[rd8(base + PL_SHOTLV) & 3];
	p->options = rd8(base + PL_OPTS);
	p->option_lv = rd8(base + PL_OPTLV);
	p->bombs = rd8(base + PL_BOMBS);
	p->bomb_gauge = rd8(base + PL_BGAUGE);
	p->rate = rd8(base + PL_RATE);
	const int f = rd16(base + PL_FORM);
	p->formation = p->pilot < 9 || p->pilot > 12 ? -1 : f <= 8 && !(f & 1) ? f / 2 : 5;
}

/* Condition bits: default.lay's precondition / team / event logic */
static int cond3(int forced, int team_ok, int event)
{
	return forced ? GAD_COND_MET : !team_ok ? GAD_COND_NA : event ? GAD_COND_MET : GAD_COND_OPEN;
}

void gadgets_update(GadgetState *g, const uint8_t *ram, int chase_mode)
{
	R = ram;
	static int inited;
	if (!inited) {
		gadgets_reset();
		inited = 1;
	}
	memset(g, 0, sizeof *g);
	g->music = -1;
	g->music_by = "";

	const int demo = rd8(DEMO);
	g->in_credit = (demo & 0xC0) == 0;
	if (g->in_credit && !H.was_in_credit) {    /* a credit starts: the stage rows start over */
		const int li0 = H.last_in[0], li1 = H.last_in[1], cp = H.current;
		gadgets_reset();
		H.last_in[0] = li0, H.last_in[1] = li1, H.current = cp;
	}
	H.was_in_credit = g->in_credit;
	g->course = rd8(COURSE);

	/* rank */
	g->rank = rd32(RANK);
	g->rank_start = rd32(RANK_START);
	{
		/* (1 - rank / 0xFF0000) * 1e6 in integers; get_digits() drops the fraction and the sign */
		const int64_t v = 1000000 - (int64_t)g->rank * 1000000 / 0xFF0000;
		g->rank_ppm = (int32_t)(v < 0 ? -v : v);
		const int64_t b = 65536 - (int64_t)g->rank * 65536 / 0xFF0000;
		g->rank_bar_q16 = (int32_t)(b < 0 ? 0 : b);
		const int64_t s = 65536 - (int64_t)g->rank_start * 65536 / 0xFF0000;
		g->rank_start_bar_q16 = (int32_t)(s < 0 ? 0 : s);
	}

	/* drops */
	g->next_drop = rd8(NEXT_DROP);
	for (int i = 0; i < GAD_NEXT_DROPS; i++)
		g->next_items[i] = drop_item(g->next_drop + i);
	for (int i = 0; i < GAD_DROPS; i++) {
		g->drops[i] = rd8(drop_addr[i]);
		g->drop_at_cap[i] = gad_drop_caps[i] && g->drops[i] == gad_drop_caps[i];
	}
	g->medal_rung = rd8(MEDAL);
	g->medal_step = (g->medal_rung & 1) || g->medal_rung > 36 ? 18 : g->medal_rung / 2;
	g->medal_value = medal_values[g->medal_step];

	/* players and input: whose input changed last is "current" */
	player(&g->p[0], P1, IN_P1, 0x20);
	player(&g->p[1], P2, IN_P2, 0x40);
	for (int i = 0; i < 2; i++)
		if (g->p[i].in != H.last_in[i]) {
			H.last_in[i] = g->p[i].in;
			H.current = i;
		}
	g->current = H.current;

	/* secret bosses */
	{
		unsigned types = 0, origins = 0, powers = 0, shots = 0;
		for (int pl = 0; pl < 2; pl++) {
			const uint32_t base = pl ? P2 : P1;
			if (!(rd16(base) & 0x8000))
				continue;
			for (int i = 0; i < 3; i++) {
				const int s = rd16(base + PL_TEAM + 2 * i), idx = (s >> 4) & 31;
				if (idx >= 18)                 /* 0xFFFF: the Lua would index nil here; skipped */
					continue;
				types |= 1u << ((s >> 2) & 3);
				origins |= 1u << ship_origin[idx];
				powers |= 1u << ship_power[idx];
				shots |= 1u << ship_shot[idx];
			}
		}
		const int ntypes = __builtin_popcount(types), npowers = __builtin_popcount(powers),
			nshots = __builtin_popcount(shots);
		const int s2 = rd8(STAGE_ORDER), s3 = rd8(STAGE_ORDER + 1), s4 = rd8(STAGE_ORDER + 2);
		g->sp_power = rd8(SP_POWER) != 0;
		g->sp_option = rd8(SP_OPTION) != 0;
		const int o1 = (origins & 1) != 0, o2 = (origins & 2) != 0, o4 = (origins & 4) != 0;
		const int envy = cond3(s2 == 1, o1, (s3 == 1 && g->sp_power) || (s4 == 1 && g->sp_option));
		const int blunt = cond3(s2 == 2, o1, (s3 == 2 && g->sp_power) || (s4 == 2 && g->sp_option));
		const int sobut = cond3(s2 == 3, o1, (s3 == 3 && g->sp_power) || (s4 == 3 && g->sp_option));
		const int gsquid = cond3(0, o2, rd8(COND_GSQUID) == 4);
		const int bashir = cond3(0, o4, rd8(COND_BASHIR) == 3);
		const int bashimk2 = cond3(0, o4, rd8(COND_BASHIMK2) == 2);
		const int gobrobo = cond3(0, o4, rd8(COND_GOBROBO) == 2);
		const int bashi = cond3(0, o4, rd8(COND_BASHI) == 0);
		const int bheart = cond3(0, o2, rd8(COND_BHEART) == 5);
		/* these three have no event: met by the team alone, else the minus (never an empty box) */
		const int hayate = npowers >= 3 && o4 ? GAD_COND_MET : GAD_COND_NA;
		const int tsumuji = nshots >= 3 && o4 ? GAD_COND_MET : GAD_COND_NA;
		const int bheartmk2 = ntypes >= 2 && o2 ? GAD_COND_MET : GAD_COND_NA;
		const int c[GAD_CONDS] = { bashi, bashimk2, bashir, gobrobo, bheart, bheartmk2, gsquid, tsumuji, hayate,
			envy, blunt, sobut };
		memcpy(g->cond, c, sizeof c);
	}

	/* boss: the last enemy (in table order) that matches wins, as in the Lua */
	g->boss_hp = -1;
	g->boss_timer = -1;
	g->boss_secs = -1;
	const char *hp_name = NULL, *t_name = NULL;
	for (int i = 0; i < ENEMY_N; i++) {
		const uint32_t e = ENEMIES + (uint32_t)i * ENEMY_SIZE;
		const int status = rd16(e);
		const uint32_t script = rd32(e + 2);
		const int part = rd16(e + 32), hp = rd16(e + 80);
		if (status & 0x8000 && hp > 0)
			g->enemies++;
		const Boss *b = NULL;
		for (unsigned k = 0; k < sizeof bosses / sizeof bosses[0]; k++)
			if (bosses[k].script == script) {
				b = &bosses[k];
				break;
			}
		if (!b)
			continue;
		for (int k = 0; k < 6 && b->hp[k].mask; k++)
			if (b->hp[k].part == part && hp > 0 && (status & b->hp[k].mask) == b->hp[k].value) {
				g->boss_hp = hp;
				hp_name = b->name;
			}
		/* the part's own timer, else '*' */
		int src = 0;
		for (int k = 0; k < 6 && b->time[k].src; k++)
			if (b->time[k].part == part)
				src = b->time[k].src;
		if (!src)
			for (int k = 0; k < 6 && b->time[k].src; k++)
				if (b->time[k].part == ANY)
					src = b->time[k].src;
		if (src) {
			const int32_t t = timeout(e, src);
			if (t != 0) {
				g->boss_timer = t < 0 ? -t : t;   /* the digits show |t| / 60 */
				t_name = b->name;
			}
		}
	}
	if (g->boss_timer >= 0)
		g->boss_secs = (int)((uint32_t)g->boss_timer / 60 % 1000);
	if (g->boss_hp > 999)
		g->boss_hp %= 1000;                       /* three digits */
	g->boss_name = hp_name ? hp_name : t_name;

	for (int i = 0; i < BULLET_N; i++) {
		const uint32_t b = BULLETS + (uint32_t)i * BULLET_SIZE;
		if ((rd8(b) & 0xB8) == 0x88 && !(rd8(b + 1) & 1))
			g->bullets++;
	}

	/* status + stages */
	g->stage_no = rd8(STAGE_NO);
	g->stage_scene = rd8(SCENE);
	g->stage_name = gad_scene_name(g->stage_scene);
	{
		const uint16_t vbl = (uint16_t)rd16(VBLANKS);
		const uint16_t dv = (uint16_t)(vbl - H.last_vbl);
		H.last_vbl = vbl;
		const int playing = g->in_credit && g->stage_no >= 1 && g->stage_no <= GAD_STAGES &&
			((g->stage_scene >= 1 && g->stage_scene <= 11) || (g->stage_scene >= 24 && g->stage_scene <= 37));
		const int row = playing ? g->stage_no - 1 : -1;
		if (row >= 0) {
			if (row != H.cur_row) {        /* a new stage: its score counts from here */
				H.last_score[0] = g->p[0].score;
				H.last_score[1] = g->p[1].score;
				H.cur_row = row;
			}
			H.played[row] = 1;
			if (dv < 600)                      /* a jump = the counter was reset, not time */
				H.time[row] += dv;
		}
		/* points go to the stage they were made in, the clear bonus included (scored before the next stage);
		 * a score that drops (a continue starts from zero) is a new base, not a loss */
		if (H.cur_row >= 0 && g->in_credit)
			for (int pl = 0; pl < 2; pl++) {
				const uint32_t s = g->p[pl].score;
				if (s > H.last_score[pl])
					H.score[pl][H.cur_row] += s - H.last_score[pl];
				H.last_score[pl] = s;
			}
		g->stage_cur = row;
		for (int r = 0; r < GAD_STAGES; r++) {
			/* the map of each row from the course's order (the scene byte changes a frame apart from the stage
			 * number, and secret bosses have their own scenes, so it is not recorded) */
			int sc = r + 1;
			if (r >= 1 && r <= 3) {
				const int o = rd8(STAGE_ORDER + r - 1);
				sc = (o >= 1 && o <= 3) ? o + 1 : 0;
			}
			g->stage_row_scene[r] = sc;
			g->stage_played[r] = H.played[r];
			g->stage_time[r] = H.time[r];
			for (int pl = 0; pl < 2; pl++) {
				g->stage_score[pl][r] = H.score[pl][r];
				g->stage_total[pl] += H.score[pl][r];
			}
		}
		if (row >= 0) {
			g->stage_frames = H.time[row];
			g->stage_min = (int)(g->stage_frames / 3600);
			g->stage_sec = (int)(g->stage_frames / 60 % 60);
		}
	}

	/* CHASE! */
	if (chase_mode && rd16(CS_STATE) == 2) {
		g->chase = 1;
		g->chain = rd32(CS_CHAIN);
		g->max_chain = rd32(CS_MAXCHAIN);
		g->mult = (int)((g->chain > CHAIN_CAP ? CHAIN_CAP : g->chain) / MULT_STEP) + 1;
		const int rest = rd16(CS_REST);
		if (rest) {
			g->timer = rest;
			g->timer_max = CASH_REST;
			g->timer_state = GAD_TIMER_REST;
		} else {
			g->timer = rd16(CS_TIMER);
			g->timer_max = TIMER_MAX;
			g->timer_state = g->timer < WARN ? GAD_TIMER_RED : GAD_TIMER_BLUE;
		}
		g->timer_q16 = g->timer >= g->timer_max ? 65536 : g->timer * 65536 / g->timer_max;
		g->timer_frozen = rd16(CS_PAUSE) != 0;
		g->break_ready = g->mult >= CASH_MIN && !rest;
		g->forward = rd16(CS_FWD) != 0;
		/* count_lives: P1's riders still flying among the team's first TEAMSZ+1 (at most 4), plus the bank */
		int n = rd8(TEAMSZ) + 1, fly = 0;
		if (n > 4)
			n = 4;
		for (int i = 0; i < n; i++) {
			const int w = rd16(P1 + PL_TEAM + 2 * i);
			if (w != 0xFFFF && !(w & 1))
				fly++;
		}
		g->banked = rd16(CS_BANK);
		g->lives = fly + g->banked;
		g->next_extend = rd32(CS_NEXTEXT);
		g->to_extend = g->next_extend > g->p[0].score ? g->next_extend - g->p[0].score : 0;
		g->msg = rd16(CS_MSGT) ? rd16(CS_MSG) : 0;
		if (g->msg < 0 || g->msg > 3)
			g->msg = 0;
	}
}
