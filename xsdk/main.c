// batrider360 test 1c: Armed Police Batrider (batriderj), the arcade board emulated on the Xbox 360 (xsdk .xex).
// Loads MAME's batrider.zip from beside the .xex, runs the board one frame per display frame (60 Hz; the board's
// 59.637 Hz plays 0.6% fast), shows the picture upright in the middle of 1280x720 (YOKO), plays its sound through
// XAudio and reads the first connected pad. Coin = BACK, start = START. (Test 14: the title's EXIT row ends it.)
// Everything it does goes to batrider360.log beside the .xex.
// Test 1b: three cores. The 68000, Z80 and OKIs run here (hardware thread 0); the picture (GP9001 drawing, colours,
// texture fill) on hardware thread 2 from the VDP state copied at the picture's line, one frame behind; the YM2151's
// sound and the resampler on hardware thread 4 from the frame's logged writes (core/board.c, split board: the PC
// harness gives the same frames and sound as the unsplit board). Test 1's sticks: ibara360's half-travel dead zone.
// Test 1c: the stick tests signed again (an unsigned enum made any tilt a direction); the sound queue held at its
// target by stretching each frame by 0.5% at most (the display's 59.94 Hz left it short).
// Test 2: CHASE! and the N64 front end's title. Two programs from batriderj's: ORIGINAL and CHASE! (the
// batrider-chase 68000 patch), both without the power-on ROM/RAM check, in free play. Power-on: the board's WARNING,
// COIN-OP GAME STUDIO's logo, the game's opening, then the N64 title (the arcade title's background, the BATRIDER
// CHASE! logo, START / PRACTICE / OPTIONS); START at any of them goes to the title. Half a minute idle on the title:
// the game's demonstration. MODE (CHASE! / ORIGINAL; the host menu's line on the N64, LB / RB on the title until
// test 3) picks the program START plays. The main loop's own parts are timed (test 1c's stutter at a run's start).
// Test 3: the stutters were the log: a line to batrider360.log now and then held the main loop 0.5-1.3 s (test 2's
// 2 s stutter in the demo). The log lines go through a ring to their own thread now. The title drawn without
// divides and kept while it stands still (test 2: 18.4 ms a frame on the video thread = repeated frames and an
// empty sound queue on the title, underruns when a run started); the sound queue refilled with silence when it
// runs low instead of over ~10 s.
// Test 4: the N64 front end whole (core/fe.c: PRACTICE, OPTIONS + GAME SETTINGS, the pause box, CONTINUES, rankings
// per MODE, the host menu on LB + RB, the hitbox; the title no longer stops the board, so its music plays on), the
// CHASE! HUD's top row and boss bar (core/chasehud.c), the GADGETS (core/gadgets*.c) on a canvas behind the game on
// hardware thread 3, the host menu's SCREEN (YOKO / TATE A / TATE B), the files beside the .xex written by the log
// thread. The intro's stray tiles were CHASE!'s code in ROM the opening reads as its map (batrider-chase: moved).
// Test 5: speed. Test 4 ran slow with GADGETS on and a little less with them off: the video thread took 16-21 ms a
// frame (9 ms in test 2) because the gadget thread shared its core (hardware threads 2 and 3) and spun there between
// jobs. The gadget thread moves to hardware thread 5, the sound and gadget workers sleep on an event between jobs, a
// gadget canvas is drawn only when it would change, and the stats count every wait for a worker.
// Test 8 (version 0.8): the manual on the screen (batrider360_manual.bin beside the .xex, pages read and unpacked by
// the log thread), save states (10 slots a MODE, kept in memory, written by the log thread), replays (every credit's
// pads from a state taken as it starts; kept on a YES after it; played back from the host menu's REPLAYS at 1/2 to x3),
// BRIGHTNESS / SMOOTHING / SCANLINES in the pixel shader's push constants, "PRESS LB+RB FOR MENU" on the title.
// Test 9 (version 0.9): test 8 ran slow (the main loop waited for the sound thread before each board frame: one set of
// sound jobs) and refused its manual (a page over MAN_ZMAX); every menu in ibara360's grammar and order (core/fe.c),
// CONTROLS sets each pad button (game_bits; the replays record the result, file version 2), AUTO SHOT.
// Test 10 (version 0.10): the title shows the arcade's own logo (user: no N64 logos), the title frozen white fixed
// (core/fe.c), no LB+RB shortcut, START goes to the game in play (BACK pauses), GENERAL SCREEN's PICTURE SIZE and
// ASPECT RATIO (game_rect: the game's quad each frame), SCAN STRENGTH, BRIGHTNESS percent, SOUND's ATTRACT / MUTE.
// Test 11 (version 0.11): the menu sounds on every page (core/menusnd.c: the game's SEs made once at boot, mixed in
// sound_job), ibara360's held-direction repeat, the pause's RESTART STAGE < STAGE / STATE > (restart_stage), every
// menu as a GADGETS panel (core/menudraw.c: Baffo's frame, the board's font), the ranking's "ORMAL COURSE" fixed
// (core/chasehud.c: CHASE!'s rail clear no longer wipes what the next screen drew).
// Test 12 (version 0.12): the box menus at 1.5x with the value window to the box's edge (core/fe.c hm_draw), the board
// silent through a MODE change's title (no second "BATRIDER"), a credit that took a continue keeps no score and asks
// no SAVE REPLAY? (rec_cont), the title logo's palettes put back after a START in the demo (it stayed grey).
// Test 13 (version 0.13): a CONTINUE? countdown run out with a ranked score no longer counts as a continue (the name
// entry put P1's flag back up while CONT_USED, the screens opened, was 1; core/fe.c cont_taken). Then (user: "apply the
// same rules as ibara when it comes to score validity"): ibara360's rules (core/fe.c red / rp_drop): a continue, GAME
// SETTINGS off their defaults, TRAINING, INVULNERABLE, RESTART STAGE past stage 1 or LOAD STATE make the score invalid
// (GADGETS' TOTAL and that stage red, SAVE REPLAY? still asked with the score red and "*", the replay's row red on
// REPLAY DATA); no replay after TRAINING, INVULNERABLE, RESTART STAGE past stage 1, LOAD STATE, QUIT; FULL RESTART and
// stage 1's RESTART STAGE start a new one. The rankings are the game's again (INVULNERABLE's alone kept out).
// Test 14 (version 0.14): EXIT at the bottom of the title's menu (fade, settings written, FE_REQ_EXIT); the BACK +
// START hold is gone (user). Then the user's picks: ibara360's TRAINING page (core/fe.c tr_*), HITBOX ALL (the enemies'
// and bullets' boxes), SECRET COMMANDS (the arcade's START commands as rows), the faster boot with NOW LOADING (below,
// "the faster boot").
// Test 15 (version 0.15): TRAINING's TEAM skips the arcade's SELECT TYPE (SELECT TEAM / TEAM EDIT / RANDOM EDIT /
// SELECT PLAYER) or flies CUSTOM riders; the PLAYER page (riders + types A-D, guests' FORMATION, LIVES, BOMBS, BOMB
// FRAGMENTS, SHOT POWER, OPTION POWER); no states in the training pause; SECRET COMMANDS' DEFAULT; OPTIONS' help fits.
// Test 16 (version 0.16): a menu keeps its size while it is open (pieces too long for 1.5x at 1x alone), SELECT PLAYER
// with one rider for every life (riders 2 and 3 off), TYPE rows, the DEBUG MENU's INFINITE LIVES / INFINITE BOMBS.
// Test 17 (version 0.17): the demo's music stopped by START into the title, the arcade's copyright lines back on the
// title with "CHASE HACK BY COIN-OP" under them, FORMATION's SECRET 1-5, TRAINING INFINITE LIVES, DEBUG INVINCIBILITY.
// Test 18 (version 0.18): the title menu's help lines fit (PLAY = START A RUN), TEAM at the top of the PLAYER EDIT page
// (was the TRAINING page's PLAYER), TRAINING's SHOT / OPTION POWER at the first rider start only (0xD42 hook: the
// course's forced full power no longer wins, a lost rider's power drops as in the game).

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "batrider360_shaders.h"
#include "board.h"
#include "fe.h"
#include "front.h"
#include "gadgets.h"
#include "menudraw.h"
#include "menusnd.h"
#include "progpatch.h"
#include "romset.h"
#include "snap.h"
#include "vdp.h"
#include "xsdk/gpu.h"
#include "xsdk/kernel.h"
#include "xsdk/log.h"
#include "xsdk/present.h"
#include "xsdk/shader.h"
#include "xsdk/splash.h"
#include "xsdk/texture.h"
#include "xsdk/thread.h"

enum {
	SPLASH_MS = 1500,
	BUFFERS = 2,
	SWAP_INTERVAL = 1,
	BEGIN_TIMEOUT_MS = 5000,
	// The board's picture doubled (nearest), then drawn 540x720 with linear filtering: sharp, without uneven pixels.
	TEX_W = BR_W * 2,
	TEX_H = BR_H * 2,
	TEXTURES = 3,
	VERTEX_FLOATS = 5,
	STRIDE_BYTES = VERTEX_FLOATS * 4,
	UV_OFFSET_BYTES = 12,
	QUAD_VERTICES = 6,
	// Sound: XAudio's render driver takes 256 samples x 6 channels of big-endian floats ~187.5 times a second.
	FRAME_CHANNELS = 6,
	CHANNEL_SAMPLES = 256,
	AUDIO_FRAMES = 2,
	RING = 16384,
	RING_TARGET = 2400,                 // three board frames queued
	STATS_EVERY = 600,
	STICK_DEAD_ZONE = 16384,            // half travel, as ibara360 since its test 3: the user's sticks drift
	VIDEO_THREAD = 2,
	SOUND_THREAD = 4,
	WORKER_STACK = 256 * 1024,
	LOG_THREAD = 5,
	LOG_LINES = 256,
	LOG_CHARS = 512,                    // as xsdk_log_write: 510 characters, then its '\n'
	LOG_SLEEP_MS = 10,
	YIELD_TICKS = 50000,                // a busy wait yields the hardware thread every ~1 ms
};

// Kept out of the enum: a value above INT_MAX made the whole enum unsigned, and with it the stick tests
// (thumb_ly > STICK_DEAD_ZONE, thumb_lx < -STICK_DEAD_ZONE) compared unsigned = any sign tilt was a direction
// (tests 1 and 1b's drift).
_Static_assert(-STICK_DEAD_ZONE < 0, "the enum must stay signed");
#define CLEAR_ARGB 0xFF000000u
#define ERROR_ARGB 0xFF600000u

#define TIMEBASE_HZ (3192000000ULL / 64) // as ibara360's xplat.h

static const XsdkVertexElement layout[] = {
	{ .usage = XSDK_USAGE_POSITION, .format = XSDK_VERTEX_FLOAT3, .offset_bytes = 0 },
	{ .usage = XSDK_USAGE_TEXCOORD, .format = XSDK_VERTEX_FLOAT2, .offset_bytes = UV_OFFSET_BYTES },
};

static XsdkShader vs, ps;
static XsdkTexture tex[TEXTURES];
static XsdkSamplerState samp;
enum { CANVAS_W = 1280, CANVAS_H = 720, CANVAS_BUFFERS = 3 };
static XsdkTexture ctex[2][CANVAS_BUFFERS];  // [0] YOKO 1280x720, [1] TATE 720x1280 (as the player sees it)

// ---- sound: the board's frames go into a ring of float bit patterns, the driver's callback copies them out (it
// may not do floating point, docs/PITFALLS.md) ----

static uint32_t ring[RING];
static volatile uint32_t ring_w, ring_r;
static float *aframes;
static uint32_t driver;
static volatile uint32_t underruns, overruns;

static void play_frame(const uint32_t *context)
{
	(void)context;
	static uint32_t next, last;
	const uint32_t current = __atomic_load_n(&driver, __ATOMIC_ACQUIRE);
	if (!current)
		return;
	uint32_t *const f = (uint32_t *)(aframes + next * FRAME_CHANNELS * CHANNEL_SAMPLES);
	next = (next + 1) % AUDIO_FRAMES;
	uint32_t r = __atomic_load_n(&ring_r, __ATOMIC_RELAXED);
	const uint32_t w = __atomic_load_n(&ring_w, __ATOMIC_ACQUIRE);
	for (uint32_t i = 0; i < CHANNEL_SAMPLES; i++) {
		if (r != w) {
			last = ring[r % RING];
			r++;
		} else if (i == 0) {
			__atomic_fetch_add(&underruns, 1, __ATOMIC_RELAXED);
		}
		f[i] = last;
		f[CHANNEL_SAMPLES + i] = last;
	}
	__atomic_store_n(&ring_r, r, __ATOMIC_RELEASE);
	XAudioSubmitRenderDriverFrame(current, (const float *)f);
}

static bool start_audio(void)
{
	const uint32_t bytes = AUDIO_FRAMES * FRAME_CHANNELS * CHANNEL_SAMPLES * 4;
	aframes = MmAllocatePhysicalMemoryEx(0, bytes, PAGE_READWRITE, 0, UINT32_MAX, 0);
	if (!aframes) {
		xsdk_log_write("sound: MmAllocatePhysicalMemoryEx failed");
		return false;
	}
	memset(aframes, 0, bytes);
	const XAUDIO_RENDER_DRIVER_CLIENT client = { .callback = play_frame, .context = 0 };
	uint32_t registered = 0;
	const uint32_t result = XAudioRegisterRenderDriverClient(&client, &registered);
	if (result != ERROR_SUCCESS) {
		xsdk_log_write("sound: XAudioRegisterRenderDriverClient failed: 0x%08X", (unsigned)result);
		return false;
	}
	__atomic_store_n(&driver, registered, __ATOMIC_RELEASE);
	return true;
}

static void stop_audio(void)
{
	const uint32_t registered = __atomic_exchange_n(&driver, 0, __ATOMIC_ACQ_REL);
	if (registered)
		XAudioUnregisterRenderDriverClient(registered);
}

// ---- timing and the workers ----

static uint64_t tb_read(void)
{
	uint32_t u, l, u2;
	for (;;) {
		__asm__ volatile("mftbu %0" : "=r"(u));
		__asm__ volatile("mftbl %0" : "=r"(l));
		__asm__ volatile("mftbu %0" : "=r"(u2));
		if (u == u2)
			return (uint64_t)u << 32 | l;
	}
}

static uint32_t tb_us(uint64_t ticks)
{
	return (uint32_t)(ticks * 1000000 / TIMEBASE_HZ);
}

// A worker takes job n when kick reaches n and sets done to n when it has finished it. Test 5: a worker with a wake
// event sleeps on it between jobs instead of spinning (test 4: the idle gadget worker's spin on hardware thread 3 took
// half of core 1 from the video thread, 9 -> 17 ms a frame); without one (the video thread, alone on its core) it
// spins as before.
typedef struct {
	volatile uint32_t kick, done;
	volatile uint32_t alive;
	uint32_t us, max_us;                 // the worker's time on its jobs since the last stats line
	void (*job)(uint32_t n);
	HANDLE wake;                         // 0 = spin
} Worker;

static void wait_for(volatile uint32_t *word, uint32_t value)
{
	if (__atomic_load_n(word, __ATOMIC_ACQUIRE) == value)
		return;
	uint64_t last = tb_read();
	__asm__ volatile("or 1,1,1");        // low SMT priority while spinning
	while (__atomic_load_n(word, __ATOMIC_ACQUIRE) != value) {
		if (tb_read() - last >= YIELD_TICKS) {
			NtYieldExecution();
			__asm__ volatile("or 1,1,1");
			last = tb_read();
		}
	}
	__asm__ volatile("or 2,2,2");
}

static uint32_t worker_main(void *context)
{
	Worker *w = context;
	uint32_t n = 0;
	__atomic_store_n(&w->alive, 1, __ATOMIC_RELEASE);
	// a set event is never lost (auto-reset, set after kick is stored); the timeout only guards against the unforeseen
	static const int64_t wake_timeout = -100LL * 10000; // 100 ms, in 100 ns units, relative
	for (;;) {
		if (w->wake)
			while (__atomic_load_n(&w->kick, __ATOMIC_ACQUIRE) != n + 1)
				NtWaitForSingleObjectEx(w->wake, USER_MODE, 0, &wake_timeout);
		else
			wait_for(&w->kick, n + 1);
		n++;
		const uint64_t t = tb_read();
		w->job(n);
		const uint32_t us = tb_us(tb_read() - t);
		w->us += us;
		if (us > w->max_us)
			w->max_us = us;
		__atomic_store_n(&w->done, n, __ATOMIC_RELEASE);
	}
	return 0;
}

// ---- the log file on its own thread (test 3) ----
// Test 2's stutters were log lines: a line to batrider360.log usually takes ~1.5 ms, but now and then one held the
// main loop 0.5-1.3 s (the 2 s stutter in the demo: the stats lines at frame 16800 took 1290 ms). The link wraps
// xsdk_log_write (CMakeLists.txt, -Wl,--wrap; xsdk's own "log: ..." lines from its log.c are not wrapped): once the log
// thread runs, a line goes into a ring and the log thread writes it; before that and at the exit, to the file at once.
// Nothing here allocates (ibara360 test 89: a malloc in the log path before xsdk's log lock exists hung the start).
void __real_xsdk_log_write(const char *format, ...) __attribute__((format(printf, 1, 2)));
static char log_ring[LOG_LINES][LOG_CHARS];
static uint32_t log_w, log_r, log_dropped;
static volatile uint32_t log_lock_word, log_drain_word, log_write_max;
static volatile bool log_async;

// Held ~1 us around a ring copy; a contended take yields its hardware thread each try (ibara360 test 47: a holder
// switched out on the same hardware thread must get it back at once).
static void spin_lock(volatile uint32_t *word)
{
	while (__atomic_exchange_n(word, 1, __ATOMIC_ACQUIRE))
		NtYieldExecution();
}

static void spin_unlock(volatile uint32_t *word)
{
	__atomic_store_n(word, 0, __ATOMIC_RELEASE);
}

void __wrap_xsdk_log_write(const char *format, ...)
{
	char line[LOG_CHARS];
	va_list ap;
	va_start(ap, format);
	vsnprintf(line, sizeof line - 1, format, ap);
	va_end(ap);
	if (!__atomic_load_n(&log_async, __ATOMIC_ACQUIRE)) {
		__real_xsdk_log_write("%s", line);
		return;
	}
	spin_lock(&log_lock_word);
	if (log_w - log_r < LOG_LINES) {
		memcpy(log_ring[log_w % LOG_LINES], line, sizeof line);
		log_w++;
	} else {
		log_dropped++;
	}
	spin_unlock(&log_lock_word);
}

// The ring's lines to the file, outside the ring's lock; the longest write goes into the stats (log_write_max).
static void log_drain(void)
{
	char line[LOG_CHARS];
	spin_lock(&log_drain_word);
	for (;;) {
		spin_lock(&log_lock_word);
		const bool more = log_r != log_w;
		if (more) {
			memcpy(line, log_ring[log_r % LOG_LINES], sizeof line);
			log_r++;
		}
		const uint32_t dropped = log_dropped;
		log_dropped = 0;
		spin_unlock(&log_lock_word);
		if (dropped)
			__real_xsdk_log_write("log: %u lines dropped (the ring was full)", (unsigned)dropped);
		if (!more)
			break;
		const uint64_t t = tb_read();
		__real_xsdk_log_write("%s", line);
		const uint32_t us = tb_us(tb_read() - t);
		if (us > log_write_max)
			log_write_max = us;
	}
	spin_unlock(&log_drain_word);
}

// ---- saved files (test 4: options, host settings, rankings) ----
// The front end hands a file over and goes on; the log thread writes it (a write to the HDD can take a second, test
// 2). A newer copy of the same file replaces one not yet written. Files live beside the .xex as batrider360_<name>.
enum { SAVE_SLOTS = 8, SAVE_MAX = 4096 };
typedef struct {
	char name[48];
	uint32_t len;
	bool full;
	uint8_t data[SAVE_MAX];
} SaveSlot;
static SaveSlot saves[SAVE_SLOTS];
static volatile uint32_t save_lock_word;
static uint32_t saves_written, saves_failed, saves_dropped;

static void save_path(char *p, size_t n, const char *name)
{
	snprintf(p, n, "game:/batrider360_%s", name);
}

static bool playing;                     // a replay plays (test 8): nothing it does is saved

static int plat_save(const char *name, const void *buf, int len)
{
	if (playing)
		return 0;
	if (len < 0 || len > SAVE_MAX || strlen(name) >= sizeof saves[0].name)
		return -1;
	spin_lock(&save_lock_word);
	int k = -1;
	for (int i = 0; i < SAVE_SLOTS && k < 0; i++)
		if (saves[i].full && !strcmp(saves[i].name, name))
			k = i;
	for (int i = 0; i < SAVE_SLOTS && k < 0; i++)
		if (!saves[i].full)
			k = i;
	if (k >= 0) {
		strcpy(saves[k].name, name);
		memcpy(saves[k].data, buf, (size_t)len);
		saves[k].len = (uint32_t)len;
		saves[k].full = true;
	} else {
		saves_dropped++;
	}
	spin_unlock(&save_lock_word);
	return k >= 0 ? 0 : -1;
}

// At the start only (the main thread, before the game runs): the whole file, or -1.
static int plat_load(const char *name, void *buf, int max)
{
	char p[96];
	save_path(p, sizeof p, name);
	FILE *f = fopen(p, "rb");
	if (!f)
		return -1;
	const int n = (int)fread(buf, 1, (size_t)max, f);
	fclose(f);
	return n;
}

static void save_drain(void)
{
	static SaveSlot s;                   // one slot copied out under the lock, written outside it
	static volatile uint32_t drain_word; // the log thread and the exit's drain never share s
	spin_lock(&drain_word);
	for (int i = 0; i < SAVE_SLOTS; i++) {
		spin_lock(&save_lock_word);
		const bool full = saves[i].full;
		if (full) {
			memcpy(&s, &saves[i], sizeof s);
			saves[i].full = false;
		}
		spin_unlock(&save_lock_word);
		if (!full)
			continue;
		char p[96];
		save_path(p, sizeof p, s.name);
		FILE *f = fopen(p, "wb");
		bool ok = f && fwrite(s.data, 1, s.len, f) == s.len;
		if (f && fclose(f))
			ok = false;
		if (ok)
			saves_written++;
		else
			saves_failed++;
		__real_xsdk_log_write("save: %s %s (%u bytes)", p, ok ? "written" : "FAILED", (unsigned)s.len);
	}
	spin_unlock(&drain_word);
}

// ---- big files (test 8: save states, replays) and file deletes, written by the log thread ----
// The data stays the caller's (a slot's buffer, the replay buffer) until the job's flag drops back to 0; the caller
// does not touch it before (big_wait).
enum { BIG_JOBS = 4 };
typedef struct {
	char name[48];
	const uint8_t *data;
	uint32_t len;
	bool del;
	volatile uint32_t *flag;
} BigJob;
static BigJob big[BIG_JOBS];
static volatile uint32_t big_full[BIG_JOBS];

static void big_wait(volatile uint32_t *flag)
{
	while (__atomic_load_n(flag, __ATOMIC_ACQUIRE))
		NtYieldExecution();
}

static void big_queue(const char *name, const uint8_t *data, uint32_t len, bool del, volatile uint32_t *flag)
{
	for (;;) {
		for (int k = 0; k < BIG_JOBS; k++)
			if (!__atomic_load_n(&big_full[k], __ATOMIC_ACQUIRE)) {
				snprintf(big[k].name, sizeof big[k].name, "%s", name);
				big[k].data = data;
				big[k].len = len;
				big[k].del = del;
				big[k].flag = flag;
				__atomic_store_n(flag, 1, __ATOMIC_RELEASE);
				__atomic_store_n(&big_full[k], 1, __ATOMIC_RELEASE);
				return;
			}
		NtYieldExecution();
	}
}

static void big_drain(void)
{
	for (int k = 0; k < BIG_JOBS; k++) {
		if (!__atomic_load_n(&big_full[k], __ATOMIC_ACQUIRE))
			continue;
		BigJob *j = &big[k];
		char p[96];
		save_path(p, sizeof p, j->name);
		bool ok;
		if (j->del) {
			ok = remove(p) == 0;
		} else {
			FILE *f = fopen(p, "wb");
			ok = f && fwrite(j->data, 1, j->len, f) == j->len;
			if (f && fclose(f))
				ok = false;
		}
		if (ok)
			saves_written++;
		else
			saves_failed++;
		__real_xsdk_log_write("save: %s %s (%u bytes)", p, ok ? (j->del ? "deleted" : "written") :
			(j->del ? "NOT deleted" : "FAILED"), (unsigned)j->len);
		__atomic_store_n(j->flag, 0, __ATOMIC_RELEASE);
		__atomic_store_n(&big_full[k], 0, __ATOMIC_RELEASE);
	}
}

// ---- the manual on the screen (test 8): batrider360_manual.bin (tools/mkmanual.py) beside the .xex ----
// Every page as RGB565 at two sizes (YOKO 488x720, two side by side; TATE 720x1064, one across the turned TV), each
// zlib-packed alone. The front end says which pages it wants (out.manual); the log thread reads and unpacks them into
// the texture the main loop does not show, then hands it over (man_done = which buffer | what it holds).
enum {
	MAN_YW = 488, MAN_YH = 720, MAN_TW = 720, MAN_TH = 1064, MAN_GAP = 8, MAN_TEX_YW = 1024,
	MAN_SPREAD = 2 * MAN_YW + MAN_GAP, MAN_LANGS = 2, MAN_PAGES = 128,
	MAN_ZMAX = 2 * 1024 * 1024,                  // test 9: 512 KB refused test 8's file (a TATE page packs to 812 KB)
	MAN_NONE = 0x3FFFFFFF,
};
static XsdkTexture mtex[2][2];                   // [0 YOKO spread 1024x720, 1 TATE page 720x1064][buffer]
static uint32_t man_count[MAN_LANGS];
static uint32_t man_off[MAN_LANGS][MAN_PAGES][2], man_len[MAN_LANGS][MAN_PAGES][2];
static volatile uint32_t man_want = MAN_NONE;    // lang << 16 | tate << 15 | first page
static volatile uint32_t man_done = MAN_NONE;    // buffer << 30 | the man_want it holds
// (test 8's big buffers come from the heap at the start, big_buffers: in .bss they made the .xex 23 MB longer)
static uint32_t *man_stage;                      // MAN_TW * MAN_TH >= MAN_TEX_YW * MAN_YH
static uint8_t *man_px;                          // MAN_TW * MAN_TH * 2
static uint8_t *man_z;                           // MAN_ZMAX
static uint8_t man_work[SNAP_ZWORK_INFLATE];

static uint32_t rd_be32(const uint8_t *p)
{
	return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

// At the start: the file's table (the pages are read when shown).
static void man_open(void)
{
	FILE *f = man_stage ? fopen("game:/batrider360_manual.bin", "rb") : NULL;
	if (!f) {
		xsdk_log_write("manual: game:/batrider360_manual.bin not found (MANUAL says so)");
		return;
	}
	uint8_t h[16];
	// test 9: the log says which check refused the file (test 8 only said "not a manual file of this version")
	const char *why = NULL;
	if (fread(h, 1, 16, f) != 16 || memcmp(h, "BRMN", 4))
		why = "no BRMN header";
	else if (rd_be32(h + 4) != 1)
		why = "another file version";
	for (int l = 0; !why && l < MAN_LANGS; l++) {
		man_count[l] = rd_be32(h + 8 + 4 * l);
		if (man_count[l] > MAN_PAGES)
			why = "too many pages";
	}
	char bad[64];
	for (int l = 0; !why && l < MAN_LANGS; l++)
		for (uint32_t p = 0; !why && p < man_count[l]; p++)
			for (int s = 0; !why && s < 2; s++) {
				uint8_t e[8];
				if (fread(e, 1, 8, f) != 8) {
					why = "the page table is cut short";
					break;
				}
				man_off[l][p][s] = rd_be32(e);
				man_len[l][p][s] = rd_be32(e + 4);
				if (man_len[l][p][s] > MAN_ZMAX) {
					snprintf(bad, sizeof bad, "%s page %u (%s) packs to %u bytes, over %u", l ? "JA" : "EN",
						(unsigned)p + 1, s ? "TATE" : "YOKO", (unsigned)man_len[l][p][s], (unsigned)MAN_ZMAX);
					why = bad;
				}
			}
	fclose(f);
	if (why) {
		man_count[0] = man_count[1] = 0;
		xsdk_log_write("manual: batrider360_manual.bin refused: %s", why);
		return;
	}
	fe_manual_pages((int)man_count[0], (int)man_count[1]);
	xsdk_log_write("manual: EN %u pages, JA %u pages", (unsigned)man_count[0], (unsigned)man_count[1]);
}

// On the log thread: the wanted pages into the hidden texture.
static void man_service(void)
{
	static FILE *f;
	const uint32_t want = __atomic_load_n(&man_want, __ATOMIC_ACQUIRE);
	const uint32_t done = __atomic_load_n(&man_done, __ATOMIC_ACQUIRE);
	if (want == MAN_NONE || want == (done & MAN_NONE))
		return;
	const uint32_t lang = want >> 16 & 1, tate = want >> 15 & 1, first = want & 0x7FFF;
	const uint32_t w = tate ? MAN_TW : MAN_YW, h = tate ? MAN_TH : MAN_YH, tw = tate ? MAN_TW : MAN_TEX_YW;
	if (!f)
		f = fopen("game:/batrider360_manual.bin", "rb");
	const uint64_t t = tb_read();
	for (uint32_t i = 0; i < tw * h; i++)
		man_stage[i] = 0xFF000000u;
	for (uint32_t k = 0; k < (tate ? 1u : 2u); k++) {
		const uint32_t p = first + k;
		if (!f || p >= man_count[lang])
			continue;
		const uint32_t n = man_len[lang][p][tate];
		if (fseek(f, (long)man_off[lang][p][tate], SEEK_SET) || fread(man_z, 1, n, f) != n ||
			snap_unpack(man_z, n, man_px, MAN_TW * MAN_TH * 2, man_work, sizeof man_work) != w * h * 2) {
			__real_xsdk_log_write("manual: page %u (%s) did not read", (unsigned)p + 1, lang ? "JA" : "EN");
			continue;
		}
		uint32_t *d = man_stage + k * (MAN_YW + MAN_GAP);
		for (uint32_t y = 0; y < h; y++)
			for (uint32_t x = 0; x < w; x++) {
				const uint32_t c = (uint32_t)man_px[(y * w + x) * 2] << 8 | man_px[(y * w + x) * 2 + 1];
				d[y * tw + x] = 0xFF000000u | ((c >> 11) * 255 / 31) << 16 | ((c >> 5 & 63) * 255 / 63) << 8 |
					(c & 31) * 255 / 31;
			}
	}
	const uint32_t b = done == MAN_NONE ? 0 : (done >> 30 ^ 1) & 1;
	xsdk_texture_upload(&mtex[tate][b], 0, man_stage, tw * 4);
	__atomic_store_n(&man_done, b << 30 | want, __ATOMIC_RELEASE);
	__real_xsdk_log_write("manual: %s page %u%s shown (%u ms)", lang ? "JA" : "EN", (unsigned)first + 1,
		tate ? "" : " and the next", (unsigned)(tb_us(tb_read() - t) / 1000));
}

static uint32_t log_main(void *context)
{
	(void)context;
	for (;;) {
		log_drain();
		save_drain();
		big_drain();
		man_service();
		xsdk_thread_sleep(LOG_SLEEP_MS);
	}
	return 0;
}

static void start_log_thread(void)
{
	HANDLE h;
	const NTSTATUS st = xsdk_thread_create_on(&h, WORKER_STACK, log_main, NULL, LOG_THREAD);
	if (!NT_SUCCESS(st)) {
		xsdk_log_write("log thread: xsdk_thread_create_on failed: 0x%08X (lines stay on the main thread)", (unsigned)st);
		return;
	}
	NtClose(h);
	xsdk_log_write("log thread: running on hardware thread %u", (unsigned)LOG_THREAD);
	__atomic_store_n(&log_async, true, __ATOMIC_RELEASE);
}

// At the exit: the lines still in the ring, then the file at once again.
static void stop_log_thread(void)
{
	__atomic_store_n(&log_async, false, __ATOMIC_RELEASE);
	log_drain();
	save_drain();                        // a file the front end handed over in the last frames
	big_drain();
}

static bool start_worker(Worker *w, void (*job)(uint32_t), uint32_t hardware_thread, const char *name, bool sleeps)
{
	w->job = job;
	if (sleeps) {
		const NTSTATUS e = NtCreateEvent(&w->wake, NULL, SYNCHRONIZATION_EVENT, 0);
		if (!NT_SUCCESS(e)) {
			xsdk_log_write("%s thread: NtCreateEvent failed: 0x%08X (it spins between jobs)", name, (unsigned)e);
			w->wake = 0;
		}
	}
	HANDLE h;
	const NTSTATUS st = xsdk_thread_create_on(&h, WORKER_STACK, worker_main, w, hardware_thread);
	if (!NT_SUCCESS(st)) {
		xsdk_log_write("%s thread: xsdk_thread_create_on failed: 0x%08X", name, (unsigned)st);
		return false;
	}
	NtClose(h);
	const uint64_t t0 = tb_read();
	while (!__atomic_load_n(&w->alive, __ATOMIC_ACQUIRE))
		if (tb_us(tb_read() - t0) > 1000000) {
			xsdk_log_write("%s thread: did not start", name);
			return false;
		}
	xsdk_log_write("%s thread: running on hardware thread %u, %s between jobs", name, (unsigned)hardware_thread,
		w->wake ? "sleeps" : "spins");
	return true;
}

// Waits until *word == value; the time spent goes to *waited (test 5: every wait for a worker is counted; test 4's
// stats missed the one before the copies, the one that held the main loop).
static void timed_wait(volatile uint32_t *word, uint32_t value, uint32_t *waited)
{
	const uint64_t t = tb_read();
	wait_for(word, value);
	*waited += tb_us(tb_read() - t);
}

// Job n, once job n - 1 is done (the time spent waiting goes to *waited).
static void hand_over(Worker *w, uint32_t n, uint32_t *waited)
{
	timed_wait(&w->done, n - 1, waited);
	__atomic_store_n(&w->kick, n, __ATOMIC_RELEASE);
	if (w->wake)
		NtSetEvent(w->wake, NULL);
}

// The board's 800 samples of a frame, as floats, stretched to 800 +- up to 4 so the queue stays at RING_TARGET:
// the display runs at 59.94 Hz, not 60, so 800 a frame came up ~48 samples a second short and the queue ran dry
// (test 1b: 140 underruns = 5 ms holes). 0.5% at most = no audible pitch change. Starts with RING_TARGET of silence.
// Test 3: below a quarter of RING_TARGET (only after a stall: it has run dry or nearly) it is filled with silence back
// to RING_TARGET at once; the 4 samples a frame took ~10 s to rebuild it (test 2: underruns at every run start).
static uint32_t reprimes;

static void push_audio(const int16_t *s)
{
	static bool primed;
	static float avg = RING_TARGET;
	uint32_t w = __atomic_load_n(&ring_w, __ATOMIC_RELAXED);
	const uint32_t r = __atomic_load_n(&ring_r, __ATOMIC_ACQUIRE);
	if (!primed || w - r < RING_TARGET / 4) {
		if (primed)
			reprimes++;
		const uint32_t add = RING_TARGET - (w - r);
		for (uint32_t i = 0; i < add; i++)
			ring[(w + i) % RING] = 0; // +0.0f
		w += add;
		avg = RING_TARGET;
		primed = true;
	}
	const uint32_t fill = w - r;
	if (fill > RING - BR_AUDIO_PER_FRAME - 8) {
		overruns++;
		return;
	}
	avg += ((float)fill - avg) * (1.0f / 16.0f);
	int adj = (int)((RING_TARGET - avg) * (1.0f / 128.0f));
	adj = adj < -4 ? -4 : adj > 4 ? 4 : adj;
	const int n = BR_AUDIO_PER_FRAME + adj;
	const float step = (float)(BR_AUDIO_PER_FRAME - 1) / (float)(n - 1);
	for (int i = 0; i < n; i++) {
		const float p = i * step;
		int k = (int)p;
		if (k > BR_AUDIO_PER_FRAME - 2)
			k = BR_AUDIO_PER_FRAME - 2;
		const float f = p - k;
		const float v = (s[k] + (s[k + 1] - s[k]) * f) * (0.9f / 32768.0f);
		uint32_t bits;
		memcpy(&bits, &v, 4);
		ring[w % RING] = bits;
		w++;
	}
	__atomic_store_n(&ring_w, w, __ATOMIC_RELEASE);
}

// ---- pictures ----

static void write_gpu(volatile uint32_t *memory, const void *data, uint32_t size_bytes)
{
	for (uint32_t i = 0; i < size_bytes / 4; i++) {
		uint32_t word;
		memcpy(&word, (const uint8_t *)data + i * 4, 4);
		memory[i] = word;
	}
	xsdk_gpu_invalidate(xsdk_gpu_address(memory), size_bytes);
}

// Test 4: the host menu's SCREEN. YOKO = the game upright (ROT270) in the middle of an upright TV, 540x720 at 720p
// (3:4, the full height), the gadgets beside it. TATE A / TATE B = a TV on its side, its top edge on the player's left
// / right: the board's own picture unturned (A) or turned half round (B), 960x720 (3x), the gadgets in the 160-pixel
// strips above and below the game as the player sees it. quads[screen] = the gadget canvas (whole screen). uv = (u
// along the texture's x, v along its y); the corners go top left, top right, bottom left, bottom left, top right,
// bottom right (on the framebuffer).
// Test 10 (user: ibara360's 4:3 support and picture resizing): the game's quad is made each frame from GENERAL
// SCREEN's PICTURE SIZE (INTEGER / FULL / CUSTOM) and ASPECT RATIO (a 4:3 display shows the 1280x720 picture squeezed
// to 4:3: a square is 4/3 as many pixels across); the manual's quads exist for both shapes.
enum { SCREEN_YOKO, SCREEN_TATE_A, SCREEN_TATE_B, SCREENS };
enum { GAME_QUADS = 8 };                      // written in turn (a frame's change), the GPU reads at most BUFFERS back
static volatile uint32_t *quads[SCREENS];
static volatile uint32_t *gquads[GAME_QUADS];
static volatile uint32_t *mquads[2][SCREENS]; // test 8: the manual's pages; test 10: [wide]
static const float uv_canvas[SCREENS][4][2] = {
	{ { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 } },
	// TATE A: the framebuffer's top edge is the player's left, its right edge the player's top. The canvas is drawn the
	// way the player sees it (720 across, 1280 down): framebuffer x = 1279 - canvas y, framebuffer y = canvas x.
	{ { 0, 1 }, { 0, 0 }, { 1, 1 }, { 1, 0 } },
	{ { 1, 0 }, { 1, 1 }, { 0, 0 }, { 0, 1 } },    // TATE B: the same turned half round
};
// the game: YOKO's top left = board (319, 0), top right = (319, 239), bottom left = (0, 0); TATE A the board's picture
// as it comes (its x = the player's up); TATE B turned half round
static const float uv_game[SCREENS][4][2] = {
	{ { 1, 0 }, { 1, 1 }, { 0, 0 }, { 0, 1 } },
	{ { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 } },
	{ { 1, 1 }, { 0, 1 }, { 1, 0 }, { 0, 0 } },
};

static void put_quad(volatile uint32_t *q, const float r[4], uint32_t width, uint32_t height, const float uv[4][2])
{
	const float cx0 = r[0] / width * 2 - 1, cx1 = r[1] / width * 2 - 1;
	const float cy0 = 1 - r[2] / height * 2, cy1 = 1 - r[3] / height * 2;
	const float v[QUAD_VERTICES][VERTEX_FLOATS] = {
		{ cx0, cy0, 0, uv[0][0], uv[0][1] }, { cx1, cy0, 0, uv[1][0], uv[1][1] }, { cx0, cy1, 0, uv[2][0], uv[2][1] },
		{ cx0, cy1, 0, uv[2][0], uv[2][1] }, { cx1, cy0, 0, uv[1][0], uv[1][1] }, { cx1, cy1, 0, uv[3][0], uv[3][1] },
	};
	write_gpu(q, v, sizeof v);
}

// A rectangle as the player sees it (centre cx, cy, size w x h in square units: the screen's height is its own
// pixels) -> framebuffer pixels r = { x0, x1, y0, y1 }. xs = framebuffer pixels a square unit across (4:3: 4/3).
// TATE A: the player's right = the framebuffer's down, the player's down = its left; TATE B the other way round.
static void player_rect(int screen, uint32_t width, uint32_t height, float xs, float cx, float cy, float w, float h,
	float r[4])
{
	const float px0 = cx - w / 2, px1 = cx + w / 2, py0 = cy - h / 2, py1 = cy + h / 2;
	if (screen == SCREEN_YOKO) {
		r[0] = px0 * xs; r[1] = px1 * xs; r[2] = py0; r[3] = py1;
	} else if (screen == SCREEN_TATE_A) {
		r[0] = width - py1 * xs; r[1] = width - py0 * xs; r[2] = px0; r[3] = px1;
	} else {
		r[0] = py0 * xs; r[1] = py1 * xs; r[2] = height - px1; r[3] = height - px0;
	}
}

// the player's view of the screen in square units (pw across, ph down)
static void player_view(int screen, uint32_t width, uint32_t height, float xs, float *pw, float *ph)
{
	*pw = screen == SCREEN_YOKO ? width / xs : (float)height;
	*ph = screen == SCREEN_YOKO ? (float)height : width / xs;
}

// The game's place this frame (GENERAL SCREEN): FULL = the 3:4 picture as large as the screen holds; INTEGER = the
// board's 240x320 at the largest whole multiple; CUSTOM = FULL's at SIZE x X / Y SIZE, its centre moved by X / Y
// POSITION (720ths of the screen's height, as the player sees the game; ibara360's alone_layout).
static void game_rect(const FeOut *o, uint32_t width, uint32_t height, float r[4])
{
	const float xs = o->wide ? 1.0f : 4.0f / 3.0f;
	float pw, ph, w, h;
	player_view(o->screen, width, height, xs, &pw, &ph);
	if (o->picture == FE_PIC_INTEGER) {
		int k = (int)(ph / 320) < (int)(pw / 240) ? (int)(ph / 320) : (int)(pw / 240);
		k = k < 1 ? 1 : k;
		w = 240.0f * k;
		h = 320.0f * k;
	} else {
		h = ph;
		w = h * 3 / 4;
		if (w > pw) {
			w = pw;
			h = w * 4 / 3;
		}
	}
	float cx = pw / 2, cy = ph / 2;
	if (o->picture == FE_PIC_CUSTOM) {
		const float u = height / 720.0f;
		w *= o->pic_size / 1000.0f * (o->pic_sx / 1000.0f);
		h *= o->pic_size / 1000.0f * (o->pic_sy / 1000.0f);
		cx += o->pic_x * u;
		cy += o->pic_y * u;
	}
	player_rect(o->screen, width, height, xs, cx, cy, w, h, r);
}

static void make_quads(uint32_t width, uint32_t height)
{
	const float full[4] = { 0, (float)width, 0, (float)height };
	for (int sc = 0; sc < SCREENS; sc++)
		put_quad(quads[sc], full, width, height, uv_canvas[sc]);
	// test 8, the manual: YOKO a spread of two pages (984x720 of the 1024x720 texture) in the middle; TATE one page
	// (720x1064, as the player sees it, turned as the canvas is) in the middle of the turned screen. Test 10: on a 4:3
	// display made smaller to fit (984 or 1064 square units do not)
	const float mu = (float)MAN_SPREAD / MAN_TEX_YW;
	const float muv[SCREENS][4][2] = {
		{ { 0, 0 }, { mu, 0 }, { 0, 1 }, { mu, 1 } }, { { 0, 1 }, { 0, 0 }, { 1, 1 }, { 1, 0 } },
		{ { 1, 0 }, { 1, 1 }, { 0, 0 }, { 0, 1 } },
	};
	for (int wide = 0; wide < 2; wide++)
		for (int sc = 0; sc < SCREENS; sc++) {
			const float xs = wide ? 1.0f : 4.0f / 3.0f;
			float pw, ph, w = sc == SCREEN_YOKO ? MAN_SPREAD : MAN_TW, h = sc == SCREEN_YOKO ? MAN_YH : MAN_TH, r[4];
			player_view(sc, width, height, xs, &pw, &ph);
			const float k = pw / w < ph / h ? pw / w : ph / h;
			if (k < 1) {
				w *= k;
				h *= k;
			}
			player_rect(sc, width, height, xs, pw / 2, ph / 2, w, h, r);
			put_quad(mquads[wide][sc], r, width, height, muv[sc]);
		}
}

static bool make_video(void)
{
	NTSTATUS st = xsdk_shader_load(&vs, quad_vert.container, quad_vert.container_bytes);
	if (NT_SUCCESS(st))
		st = xsdk_shader_load(&ps, quad_frag.container, quad_frag.container_bytes);
	if (NT_SUCCESS(st))
		st = xsdk_shader_set_vertex_layout(&vs, layout, sizeof layout / sizeof layout[0], (const uint32_t[]){ STRIDE_BYTES });
	if (!NT_SUCCESS(st)) {
		xsdk_log_write("video: loading the shaders failed: 0x%08X", (unsigned)st);
		return false;
	}
	for (int t = 0; t < TEXTURES; t++) {
		st = xsdk_texture_create(&tex[t], XSDK_FORMAT_A8R8G8B8, TEX_W, TEX_H, 1, true);
		if (!NT_SUCCESS(st)) {
			xsdk_log_write("video: xsdk_texture_create failed: 0x%08X", (unsigned)st);
			return false;
		}
	}
	const XsdkSampler s = { .mag_filter = XSDK_FILTER_LINEAR, .min_filter = XSDK_FILTER_LINEAR,
		.mip_filter = XSDK_FILTER_POINT, .address_u = XSDK_ADDRESS_CLAMP, .address_v = XSDK_ADDRESS_CLAMP };
	xsdk_texture_sampler_state(&samp, &s);
	volatile uint32_t **all[SCREENS * 3 + GAME_QUADS];
	int na = 0;
	for (int s = 0; s < SCREENS; s++) {
		all[na++] = &quads[s];
		all[na++] = &mquads[0][s];
		all[na++] = &mquads[1][s];
	}
	for (int k = 0; k < GAME_QUADS; k++)
		all[na++] = &gquads[k];
	for (int k = 0; k < na; k++)
		if (!(*all[k] = xsdk_gpu_alloc(QUAD_VERTICES * STRIDE_BYTES))) {
			xsdk_log_write("video: xsdk_gpu_alloc failed");
			return false;
		}
	for (int k = 0; k < 2; k++) {
		st = xsdk_texture_create(&mtex[0][k], XSDK_FORMAT_A8R8G8B8, MAN_TEX_YW, MAN_YH, 1, true);
		if (NT_SUCCESS(st))
			st = xsdk_texture_create(&mtex[1][k], XSDK_FORMAT_A8R8G8B8, MAN_TW, MAN_TH, 1, true);
		if (!NT_SUCCESS(st)) {
			xsdk_log_write("video: the manual's texture: xsdk_texture_create failed: 0x%08X", (unsigned)st);
			return false;
		}
	}
	// the gadget canvases, two of each (one drawn while the other is shown): YOKO 1280x720, TATE 720x1280
	for (int k = 0; k < CANVAS_BUFFERS; k++) {
		st = xsdk_texture_create(&ctex[0][k], XSDK_FORMAT_A8R8G8B8, CANVAS_W, CANVAS_H, 1, true);
		if (NT_SUCCESS(st))
			st = xsdk_texture_create(&ctex[1][k], XSDK_FORMAT_A8R8G8B8, CANVAS_H, CANVAS_W, 1, true);
		if (!NT_SUCCESS(st)) {
			xsdk_log_write("video: the gadget canvas: xsdk_texture_create failed: 0x%08X", (unsigned)st);
			return false;
		}
	}
	return true;
}

_Static_assert((int)TEX_W == (int)FRONT_TEX_W && (int)TEX_H == (int)FRONT_TEX_H,"the front end draws into the game's texture");
static uint32_t texels[TEX_H * TEX_W];

// The video thread's job n: frame n's picture into texture n % TEXTURES (the GPU side, the texture cache
// invalidate, stays on the main thread: it writes the GPU's ring). Test 4: the front end (core/fe.c) finishes the
// picture: the game with its pages, pause box and hitbox; the title (kept while it stands); the splash; black; the
// fade; the host menu over it. vjob is the frame's VDP copy (out.vsrc), fjob the front end's job, both copied by
// the main thread before the hand-over.
static Vdp vjob;
static FeVideoJob fjob;

static void video_job(uint32_t n)
{
	fe_video(&vjob, &fjob, texels);
	xsdk_texture_upload(&tex[n % TEXTURES], 0, texels, TEX_W * 4);
}

// The sound thread's job: the frame's YM2151 sound and the mix, into the ring for XAudio; silence while the board
// stands still (the splash, the pause box: the music waits).
// Test 8: up to three board frames a display frame (a replay at x2 / x3): all go through the YM2151 copy, the last is
// heard.
// Test 9: two sets, frame n's board frames into set n & 1 while the sound thread still mixes set (n - 1) & 1. Test 8
// had one set, so the main loop waited for the last sound job before the board frame (7-13 ms in play: repeated flips).
enum { SJOBS = 3 };
static BrSound sjobs[2][SJOBS];
static int sjob_n[2];
static uint32_t sjob_set;                // the set this frame's board frames go into
static int16_t smix[BR_AUDIO_PER_FRAME];
static bool sound_on;
// test 11: the menu sounds started in frame n go with set n & 1 (menusnd.h), at the front end's SFX volume
static unsigned sjob_se[2];
static int sjob_se_vol[2];

static void sound_job(uint32_t n)
{
	const uint32_t s = n & 1;
	if (!sjob_n[s])
		memset(smix, 0, sizeof smix);
	for (int i = 0; i < sjob_n[s]; i++)
		br_sound_mix(&sjobs[s][i], smix);
	menusnd_mix(smix, sjob_se[s], sjob_se_vol[s]);
	if (sound_on)
		push_audio(smix);
}

// ---- the gadgets (test 4): Baffo's panels + the CHASE! ones on a canvas behind the game ----
// The main thread reads the frame's numbers from the board's RAM (gadgets_update, right after the board frame); the
// gadget thread draws them on the canvas the way the player sees it and uploads it into canvas texture
// n % CANVAS_BUFFERS. The main thread never waits for it: a new job is handed over only when the last one is done (a
// canvas is ~3.7 MB to fill and upload), and the newest finished canvas is drawn; three of them, so the one the GPU
// may still read is never the one being written. Test 5: on hardware thread 5 (core 2, beside the sound thread and
// the log thread's short writes); test 4 had it on 3, core 1's second thread, where its 11 ms a frame slowed the
// video thread past the frame. A job only when what it would draw changed (main loop).
enum { GADGET_THREAD = 5 };
static uint32_t canvas_px[CANVAS_W * CANVAS_H];
static GadgetState gstate, gjob;
static int gjob_screen, gjob_chase;
static bool gjob_on;
static int gjob_drawn[CANVAS_BUFFERS];     // per canvas: the screen it was drawn for, -1 = none

static void gadget_job(uint32_t n)
{
	if (!gjob_on) {
		gjob_drawn[n % CANVAS_BUFFERS] = -1;
		return;
	}
	const int tate = gjob_screen != SCREEN_YOKO;
	gadgets_draw(canvas_px, tate, gjob_chase, &gjob, br_text_glyphs());
	xsdk_texture_upload(&ctex[tate][n % CANVAS_BUFFERS], 0, canvas_px, (tate ? CANVAS_H : CANVAS_W) * 4);
	gjob_drawn[n % CANVAS_BUFFERS] = gjob_screen;
}

static Worker video, sound, gadget;

static uint64_t board_clock(void)
{
	return tb_read();
}

// Test 8: the host menu's look for the next draws (quad.frag): brightness, and for the game alone the scanlines
// (between the board's 240 lines, across the texture's v) and the soft blend of the doubled texels. Test 10:
// BRIGHTNESS in percent (100 = x1), SCAN STRENGTH in percent (100 = black between the lines; 0 = none).
static void set_look(int bright, int scan, int soft)
{
	const float c[8] = { (float)bright / 100.0f, (float)scan / 100.0f, soft ? 1.0f : 0.0f, (float)BR_H,
		soft ? 1.0f / TEX_W : 0.0f, soft ? 1.0f / TEX_H : 0.0f, 0.0f, 0.0f };
	xsdk_gpu_set_pixel_constants(QUAD_FRAG_PUSH, c, 2);
}

// The gadget canvas (canvas: NULL = none, GADGETS off or nothing drawn yet), then the game texture t over it; or
// (test 8) the manual's page texture alone (page: NULL = black while the first pages load).
static void draw(const FeOut *o, const XsdkTexture *canvas, int t, const XsdkTexture *page, uint32_t address,
	uint32_t width, uint32_t height)
{
	const int screen = o->screen;
	// test 10: the game's quad, written again only when its place changed (the next buffer: the GPU may still read
	// the last ones)
	static float last[4] = { -1, -1, -1, -1 };
	static int gq = -1;
	float r[4];
	game_rect(o, width, height, r);
	if (gq < 0 || memcmp(r, last, sizeof r)) {
		gq = (gq + 1) % GAME_QUADS;
		put_quad(gquads[gq], r, width, height, uv_game[screen]);
		memcpy(last, r, sizeof r);
	}
	xsdk_gpu_set_target(width, height);
	xsdk_gpu_set_shaders(&vs, &ps);
	xsdk_built_shader_set_literals(&quad_vert);
	xsdk_built_shader_set_literals(&quad_frag);
	if (o->manual) {
		set_look(100, 0, 0);
		if (page) {
			xsdk_gpu_set_texture(QUAD_FRAG_SAMPLER_0_0, page, &samp);
			xsdk_gpu_set_vertices(0, xsdk_gpu_address(mquads[o->wide != 0][screen]), QUAD_VERTICES * STRIDE_BYTES);
			xsdk_gpu_draw(XSDK_PRIMITIVE_TRIANGLE_LIST, QUAD_VERTICES);
		}
		xsdk_gpu_resolve(address, width, height, CLEAR_ARGB);
		return;
	}
	set_look(o->bright, 0, 0);
	if (canvas) {
		xsdk_gpu_set_texture(QUAD_FRAG_SAMPLER_0_0, canvas, &samp);
		xsdk_gpu_set_vertices(0, xsdk_gpu_address(quads[screen]), QUAD_VERTICES * STRIDE_BYTES);
		xsdk_gpu_draw(XSDK_PRIMITIVE_TRIANGLE_LIST, QUAD_VERTICES);
	}
	set_look(o->bright, o->scan, o->smooth);
	xsdk_gpu_set_texture(QUAD_FRAG_SAMPLER_0_0, &tex[t], &samp);
	xsdk_gpu_set_vertices(0, xsdk_gpu_address(gquads[gq]), QUAD_VERTICES * STRIDE_BYTES);
	xsdk_gpu_draw(XSDK_PRIMITIVE_TRIANGLE_LIST, QUAD_VERTICES);
	xsdk_gpu_resolve(address, width, height, CLEAR_ARGB);
}


// ---- save states (test 8): 10 slots a MODE, all kept in memory (read at the start), written by the log thread ----
// File batrider360_state_<c|o><01..10>.bin: "BRST", version 1, MODE, the slot's text (32 bytes), the picture
// (FE_THUMB_W x FE_THUMB_H RGB565, big-endian, the board's orientation), the packed state's length, the packed state
// (core/snap.c: a state of another build family is refused when it is loaded).
enum { SLOT_HEAD = 4 + 4 + 4 + 32 + FE_THUMB_W * FE_THUMB_H * 2 + 4, SLOT_CAP = 192 * 1024 };
typedef struct {
	bool used;
	char text[32];
	uint16_t pic[FE_THUMB_W * FE_THUMB_H];
	uint32_t len;                        // bytes of data (the whole file)
	uint8_t data[SLOT_CAP];
	volatile uint32_t writing;           // its file write is queued
} Slot;
static Slot (*slots)[FE_SLOTS];            // [2], from the heap
static uint8_t state_raw[SNAP_RAW_MAX];
static uint8_t zwork[SNAP_ZWORK_DEFLATE];
// Test 11: the pause's RESTART STAGE (fe_set_restart). STAGE = a state taken in memory when this credit's stage byte
// (0x20E6A9) turns to another stage; STATE = the slot last saved or loaded in this credit. A credit's end drops both.
enum { STAGE_BYTE = 0x20E6A9 };
static uint8_t *stage_raw;               // SNAP_RAW_MAX from the heap
static size_t stage_raw_n;
static int stage_seen, stage_have;       // the stage byte last seen in this credit; the stage stage_raw starts
static int state_mode = -1, state_slot = -1;
static unsigned state_gen;               // + 1 at each save or load (fe_set_restart: STATE becomes the first choice)
static uint32_t board_pic[BR_W * BR_H];

static void slot_name(char *n, size_t size, int mode, int i)
{
	snprintf(n, size, "state_%c%02d.bin", mode == FE_CHASE ? 'c' : 'o', i + 1);
}

// The slot's text and picture from its file's bytes (data, len).
static bool slot_parse(Slot *s, int mode)
{
	const uint8_t *d = s->data;
	if (s->len < SLOT_HEAD || memcmp(d, "BRST", 4) || rd_be32(d + 4) != 1 || rd_be32(d + 8) != (uint32_t)mode ||
		rd_be32(d + SLOT_HEAD - 4) != s->len - SLOT_HEAD)
		return false;
	memcpy(s->text, d + 12, 32);
	s->text[31] = 0;
	for (int i = 0; i < FE_THUMB_W * FE_THUMB_H; i++)
		s->pic[i] = (uint16_t)(d[44 + 2 * i] << 8 | d[45 + 2 * i]);
	return true;
}

static void slots_read(void)
{
	int n = 0;
	for (int m = 0; m < 2; m++)
		for (int i = 0; i < FE_SLOTS; i++) {
			Slot *s = &slots[m][i];
			char name[32], p[96];
			slot_name(name, sizeof name, m, i);
			save_path(p, sizeof p, name);
			FILE *f = fopen(p, "rb");
			if (!f)
				continue;
			s->len = (uint32_t)fread(s->data, 1, sizeof s->data, f);
			fclose(f);
			s->used = slot_parse(s, m);
			n += s->used;
			if (!s->used)
				xsdk_log_write("states: %s is not a state file of this version (left alone)", p);
		}
	xsdk_log_write("states: %d slots in use", n);
}

static void plat_slot(int mode, int i, FeSlot *out)
{
	const Slot *s = &slots[mode & 1][i];
	out->used = s->used;
	snprintf(out->text, sizeof out->text, "%s", s->used ? s->text : "");
	out->pic = s->used ? s->pic : NULL;
}

// ---- replays (test 8) ----
// File batrider360_replay_<01..30>.bin: a 64-byte head ("BRRP", version 2 (test 9: the pads carry CONTROLS' SHOT /
// BOMB / SPECIAL bits; version 1's had the pad's A / B / X only and are not shown), number, MODE, stage, score, frames,
// packed state length, pad runs), the packed start state (with OPTIONS and LANGUAGE), the runs (held, press, count:
// 16 bits each, big-endian). The newest has the highest number; at 30 the oldest gives way.
enum { RP_VERSION = 2, REPLAYS = 30, RP_HEAD = 64, REC_RUNS = 600000, RP_BUF = RP_HEAD + SNAP_RAW_MAX + REC_RUNS * 6 };
typedef struct {
	bool used;
	uint32_t seq, mode, stage, score, frames;
	bool red;                            // test 13: the header's flags (offset 36) bit 0: the score was invalid
} ReplayInfo;
static ReplayInfo rpi[REPLAYS];
static uint8_t *rp_buf;                  // RP_BUF: a replay file being written, or read to play
static volatile uint32_t rp_writing;
static SnapPads rec = { NULL, 0, REC_RUNS, 0, 0 };   // .run: REC_RUNS from the heap
static uint8_t rec_start[SNAP_RAW_MAX];
static size_t rec_start_n;
static bool recording;
static int rec_gone;
static uint32_t rec_mode, rec_stage, rec_score;
static bool rec_red;                   // test 13: the recorded credit's score went invalid (SAVE REPLAY? red)
static unsigned rec_gen;               // test 13: fe's credit_gen of the last replay begun (a new credit = a new one)
static bool rec_again;                 // test 13: stage 1's RESTART STAGE: a new replay from it

static void replay_name(char *n, size_t size, int k)
{
	snprintf(n, size, "replay_%02d.bin", k + 1);
}

static void replays_read(void)
{
	int n = 0;
	for (int k = 0; k < REPLAYS; k++) {
		char name[32], p[96];
		replay_name(name, sizeof name, k);
		save_path(p, sizeof p, name);
		FILE *f = fopen(p, "rb");
		if (!f)
			continue;
		uint8_t h[RP_HEAD];
		const bool ok = fread(h, 1, RP_HEAD, f) == RP_HEAD && !memcmp(h, "BRRP", 4) && rd_be32(h + 4) == RP_VERSION;
		fclose(f);
		if (!ok)
			continue;
		rpi[k] = (ReplayInfo){ true, rd_be32(h + 8), rd_be32(h + 12), rd_be32(h + 16), rd_be32(h + 20),
			rd_be32(h + 24), (rd_be32(h + 36) & 1) != 0 };
		n++;
	}
	xsdk_log_write("replays: %d kept", n);
}

// The k-th newest (its file index), -1 = none; *count = how many there are.
static int replay_nth(int nth, int *count)
{
	int order[REPLAYS], n = 0;
	for (int k = 0; k < REPLAYS; k++)
		if (rpi[k].used) {
			int at = n++;
			while (at > 0 && rpi[order[at - 1]].seq < rpi[k].seq) {
				order[at] = order[at - 1];
				at--;
			}
			order[at] = k;
		}
	if (count)
		*count = n;
	return nth >= 0 && nth < n ? order[nth] : -1;
}

static void mmss(char *b, size_t n, uint32_t frames)
{
	const uint32_t s = frames / 60;
	snprintf(b, n, "%u:%02u", (unsigned)(s / 60 % 1000), (unsigned)(s % 60));
}

static int plat_replay(int i, FeReplayInfo *r)
{
	int n;
	const int k = replay_nth(i, &n);
	if (k >= 0 && r) {
		char t[12];
		mmss(t, sizeof t, rpi[k].frames);
		snprintf(r->text, sizeof r->text, "%s ST%u %9u%s %6s", rpi[k].mode == FE_CHASE ? "CHA" : "ORG",
			(unsigned)rpi[k].stage, (unsigned)rpi[k].score, rpi[k].red ? "*" : " ", t);
		r->red = rpi[k].red;
	}
	return n;
}

// ---- test 8: what the front end asks for (out.req), done between frames with the workers idle ----

static void quiesce_workers(void)
{
	wait_for(&sound.done, sound.kick);   // its YM2151 copy is in a state
	wait_for(&video.done, video.kick);   // vdp_render's scratch (the slot picture)
}

static void state_save(int mode, int i)
{
	Slot *s = &slots[mode][i];
	big_wait(&s->writing);
	quiesce_workers();
	const uint64_t t = tb_read();
	const size_t raw = snap_save(state_raw, sizeof state_raw, 0);
	const size_t packed = raw ? snap_pack(state_raw, raw, s->data + SLOT_HEAD, sizeof s->data - SLOT_HEAD, zwork,
		sizeof zwork) : 0;
	if (!packed) {
		s->used = false;
		fe_state_failed(1);
		xsdk_log_write("states: slot %d did not save (state %u bytes)", i + 1, (unsigned)raw);
		return;
	}
	// the picture: the game as the board last drew it, a quarter across and down (4 x 4 averaged)
	vdp_render(br_vdp_snapshot(), board_pic);
	for (int y = 0; y < FE_THUMB_H; y++)
		for (int x = 0; x < FE_THUMB_W; x++) {
			uint32_t r = 0, g = 0, b = 0;
			for (int k = 0; k < 16; k++) {
				const uint32_t c = board_pic[(y * 4 + (k >> 2)) * BR_W + x * 4 + (k & 3)];
				r += c >> 16 & 255;
				g += c >> 8 & 255;
				b += c & 255;
			}
			s->pic[y * FE_THUMB_W + x] = (uint16_t)((r / 16 >> 3) << 11 | (g / 16 >> 2) << 5 | (b / 16 >> 3));
		}
	state_mode = mode;
	state_slot = i;
	state_gen++;
	if (gstate.stage_no > 0)
		snprintf(s->text, sizeof s->text, "ST%d %u", gstate.stage_no, (unsigned)gstate.p[0].score);
	else
		snprintf(s->text, sizeof s->text, "%u", (unsigned)gstate.p[0].score);
	uint8_t *d = s->data;
	memcpy(d, "BRST", 4);
	snap_be32(d + 4, 1);
	snap_be32(d + 8, (uint32_t)mode);
	memset(d + 12, 0, 32);
	memcpy(d + 12, s->text, strlen(s->text));
	for (int k = 0; k < FE_THUMB_W * FE_THUMB_H; k++) {
		d[44 + 2 * k] = (uint8_t)(s->pic[k] >> 8);
		d[45 + 2 * k] = (uint8_t)s->pic[k];
	}
	snap_be32(d + SLOT_HEAD - 4, (uint32_t)packed);
	s->len = (uint32_t)(SLOT_HEAD + packed);
	s->used = true;
	char name[32];
	slot_name(name, sizeof name, mode, i);
	big_queue(name, s->data, s->len, false, &s->writing);
	xsdk_log_write("states: %s slot %d saved (%s; state %u bytes, packed %u) in %u us",
		mode == FE_CHASE ? "CHASE!" : "ORIGINAL", i + 1, s->text, (unsigned)raw, (unsigned)packed,
		(unsigned)tb_us(tb_read() - t));
}

static bool state_load(int mode, int i)
{
	Slot *s = &slots[mode][i];
	if (!s->used)
		return false;
	quiesce_workers();
	const uint64_t t = tb_read();
	const size_t raw = snap_unpack(s->data + SLOT_HEAD, s->len - SLOT_HEAD, state_raw, sizeof state_raw, zwork,
		sizeof zwork);
	if (!raw || snap_load(state_raw, raw)) {
		fe_state_failed(0);
		xsdk_log_write("states: %s slot %d did not load (another version's state?)",
			mode == FE_CHASE ? "CHASE!" : "ORIGINAL", i + 1);
		return false;
	}
	fe_state_loaded(0);
	recording = false;                   // test 13: a credit in the loaded state gets no replay (fe: rp_drop)
	state_mode = mode;
	state_slot = i;
	state_gen++;
	// test 11: the stage's start stays RESTART STAGE's while the loaded state is in the same stage
	stage_seen = br_rd8(STAGE_BYTE);
	if (stage_have != stage_seen)
		stage_have = 0;
	xsdk_log_write("states: %s slot %d loaded (%s) in %u us", mode == FE_CHASE ? "CHASE!" : "ORIGINAL", i + 1,
		s->text, (unsigned)tb_us(tb_read() - t));
	return true;
}

static void state_delete(int mode, int i)
{
	Slot *s = &slots[mode][i];
	big_wait(&s->writing);
	s->used = false;
	char name[32];
	slot_name(name, sizeof name, mode, i);
	big_queue(name, NULL, 0, true, &s->writing);
}

// The kept credit into the next replay file (a free one, else the oldest's).
static void replay_keep(void)
{
	big_wait(&rp_writing);
	int k = -1;
	uint32_t seq = 0;
	for (int i = 0; i < REPLAYS; i++)
		if (rpi[i].used && rpi[i].seq > seq)
			seq = rpi[i].seq;
	for (int i = 0; i < REPLAYS && k < 0; i++)
		if (!rpi[i].used)
			k = i;
	if (k < 0)
		k = replay_nth(REPLAYS - 1, NULL);
	const size_t packed = snap_pack(rec_start, rec_start_n, rp_buf + RP_HEAD, SNAP_RAW_MAX, zwork, sizeof zwork);
	if (!packed || k < 0) {
		xsdk_log_write("replays: not kept (the start state did not pack)");
		return;
	}
	uint8_t *h = rp_buf;
	memset(h, 0, RP_HEAD);
	memcpy(h, "BRRP", 4);
	const uint32_t v[8] = { RP_VERSION, seq + 1, rec_mode, rec_stage, rec_score, rec.frames, (uint32_t)packed, rec.n };
	for (int f = 0; f < 8; f++)
		snap_be32(h + 4 + 4 * f, v[f]);
	snap_be32(h + 36, rec_red ? 1u : 0u);   // test 13: flags (older files: 0)
	uint8_t *r = rp_buf + RP_HEAD + packed;
	for (uint32_t n = 0; n < rec.n; n++, r += 6) {
		const SnapRun *u = &rec.run[n];
		const uint16_t w[3] = { u->held, u->press, u->count };
		for (int q = 0; q < 3; q++) {
			r[2 * q] = (uint8_t)(w[q] >> 8);
			r[2 * q + 1] = (uint8_t)w[q];
		}
	}
	rpi[k] = (ReplayInfo){ true, seq + 1, rec_mode, rec_stage, rec_score, rec.frames, rec_red };
	char name[32];
	replay_name(name, sizeof name, k);
	big_queue(name, rp_buf, (uint32_t)(r - rp_buf), false, &rp_writing);
	xsdk_log_write("replays: kept as %s (%u frames, %u runs, state packed %u bytes)", name, (unsigned)rec.frames,
		(unsigned)rec.n, (unsigned)packed);
}

static void replay_delete(int nth)
{
	const int k = replay_nth(nth, NULL);
	if (k < 0)
		return;
	big_wait(&rp_writing);
	rpi[k].used = false;
	char name[32];
	replay_name(name, sizeof name, k);
	big_queue(name, NULL, 0, true, &rp_writing);
}

// ---- playing a replay ----
static uint8_t ret_state[SNAP_RAW_MAX];  // the board as it was before the replay (back after it)
static size_t ret_n;
static int pb_speed = 1;                 // 0 = 1/2, 1 = x1, 2 = x2, 3 = x3 (board frames a display frame)
static SnapReader pb;
static uint32_t pb_frames;
static SnapRun *pb_runs;                 // REC_RUNS
static const char *const pb_speed_names[4] = { "1/2", "x1", "x2", "x3" };

static int pb_nth;                       // the replay playing (its place, 0 = newest): RESTART REPLAY

static bool replay_play(int nth)
{
	const int k = replay_nth(nth, NULL);
	if (k < 0)
		return false;
	big_wait(&rp_writing);
	char name[32], p[96];
	replay_name(name, sizeof name, k);
	save_path(p, sizeof p, name);
	FILE *f = fopen(p, "rb");
	const size_t n = f ? fread(rp_buf, 1, RP_BUF, f) : 0;
	if (f)
		fclose(f);
	const uint32_t packed = n >= RP_HEAD ? rd_be32(rp_buf + 28) : 0, runs = n >= RP_HEAD ? rd_be32(rp_buf + 32) : 0;
	if (n < RP_HEAD || memcmp(rp_buf, "BRRP", 4) || rd_be32(rp_buf + 4) != RP_VERSION || runs > REC_RUNS || RP_HEAD + packed + (size_t)runs * 6 > n) {
		xsdk_log_write("replays: %s did not read", p);
		fe_open_replays("THAT REPLAY DID NOT READ");
		return false;
	}
	quiesce_workers();
	const size_t raw = snap_unpack(rp_buf + RP_HEAD, packed, state_raw, sizeof state_raw, zwork, sizeof zwork);
	ret_n = snap_save(ret_state, sizeof ret_state, 1);
	if (!raw || !ret_n || snap_load(state_raw, raw)) {
		xsdk_log_write("replays: %s did not load (another version's replay?)", p);
		fe_open_replays("THAT REPLAY DID NOT LOAD");
		return false;
	}
	const uint8_t *r = rp_buf + RP_HEAD + packed;
	for (uint32_t i = 0; i < runs; i++, r += 6)
		pb_runs[i] = (SnapRun){ (uint16_t)(r[0] << 8 | r[1]), (uint16_t)(r[2] << 8 | r[3]), (uint16_t)(r[4] << 8 | r[5]) };
	pb = (SnapReader){ pb_runs, runs, 0, 0, 0 };
	pb_frames = rd_be32(rp_buf + 24);
	pb_speed = 1;
	pb_nth = nth;
	playing = true;
	recording = false;
	fe_state_loaded(1);
	fe_playback(1);
	xsdk_log_write("replays: playing %s (%u frames)", name, (unsigned)pb_frames);
	return true;
}

static void replay_end(const char *msg)
{
	quiesce_workers();
	if (snap_load(ret_state, ret_n))
		xsdk_log_write("replays: the board did not come back (state refused)");
	playing = false;
	fe_playback(0);
	fe_state_loaded(1);
	fe_open_replays(msg);
	xsdk_log_write("replays: %s at frame %u of %u", msg, (unsigned)pb.frame, (unsigned)pb_frames);
}

// Test 9: the replay pause menu's RESTART REPLAY: the board back as it was before the replay, then the same one again.
static void replay_restart(void)
{
	quiesce_workers();
	if (snap_load(ret_state, ret_n))
		xsdk_log_write("replays: the board did not come back (state refused)");
	playing = false;
	fe_playback(0);
	xsdk_log_write("replays: RESTART REPLAY");
	replay_play(pb_nth);
}

// The big buffers, once at the start (main thread, before the log thread: nothing else allocates then).
static bool big_buffers(void)
{
	man_stage = malloc(MAN_TW * MAN_TH * 4);
	man_px = malloc(MAN_TW * MAN_TH * 2);
	man_z = malloc(MAN_ZMAX);
	slots = calloc(2, sizeof *slots);
	rp_buf = malloc(RP_BUF);
	rec.run = malloc(REC_RUNS * sizeof(SnapRun));
	pb_runs = malloc(REC_RUNS * sizeof(SnapRun));
	stage_raw = malloc(SNAP_RAW_MAX);
	return man_stage && man_px && man_z && slots && rp_buf && rec.run && pb_runs && stage_raw;
}

// ---- one board frame (test 8: also a replay's, several a display frame) ----
static FeOut out;                         // the front end's latest frame
typedef struct {
	uint32_t emu_us, emu_max, slow, fe_us, fe_max;
} StepStats;
static StepStats ss;

static void board_step(const FePad *pad)
{
	uint64_t t = tb_read();
	fe_frame(pad, &out);
	uint32_t us = tb_us(tb_read() - t);
	ss.fe_us += us;
	if (us > ss.fe_max)
		ss.fe_max = us;
	if (recording && out.rec)
		snap_pads_add(&rec, pad);
	if (out.run) {
		t = tb_read();
		br_run_frame();
		us = tb_us(tb_read() - t);
		ss.emu_us += us;
		if (us > ss.emu_max)
			ss.emu_max = us;
		if (us > 16000)
			ss.slow++;
		if (sjob_n[sjob_set] < SJOBS)
			br_sound_take(&sjobs[sjob_set][sjob_n[sjob_set]++]);
	}
	fe_after_board_frame();
	sjob_se[sjob_set] |= out.se;
	sjob_se_vol[sjob_set] = out.se_vol;
	if (out.run && !out.held) {
		gadgets_update(&gstate, br_ram(), out.chase_credit);
		// test 13: an invalid score's TOTAL red, and the stage it went invalid in (ibara360's continue stage)
		static uint8_t stage_red[GAD_STAGES];
		static unsigned red_gen;
		static int red_prev;
		if (out.credit_gen != red_gen) {
			memset(stage_red, 0, sizeof stage_red);
			red_gen = out.credit_gen;
			red_prev = 0;
		}
		const int inv = out.in_credit && out.invalid;
		if (inv && !red_prev && gstate.stage_cur >= 0)
			stage_red[gstate.stage_cur] = 1;
		red_prev = inv;
		gstate.invalid = inv;
		memcpy(gstate.stage_red, stage_red, sizeof stage_red);
	}
}

// Test 11: the pause's RESTART STAGE (FE_REQ_RESTART_STAGE): 0 = the stage's start, 1 = the last state
static void restart_stage(int to_state)
{
	if (to_state) {
		if (state_slot >= 0 && state_load(state_mode, state_slot)) {
			fe_restart_loaded(0);
			xsdk_log_write("restart stage: STATE (%s slot %d)", state_mode == FE_CHASE ? "CHASE!" : "ORIGINAL",
				state_slot + 1);
		}
		return;
	}
	if (!stage_have || !stage_raw_n)
		return;
	quiesce_workers();
	const uint64_t t = tb_read();
	if (snap_load(stage_raw, stage_raw_n)) {
		xsdk_log_write("restart stage: the stage %d state did not load", stage_have);
		return;
	}
	fe_restart_loaded(stage_have);
	recording = false;                   // test 13: stage 1's start begins a new replay, a later stage none (fe)
	rec_again = stage_have == 1;
	xsdk_log_write("restart stage: STAGE %d in %u us", stage_have, (unsigned)tb_us(tb_read() - t));
}

// Test 8, between frames once this frame's sound is through: what the front end asked for (out.req).
static void do_request(int req, int arg)
{
	switch (req) {
	case FE_REQ_RESTART_STAGE: restart_stage(arg); break;
	case FE_REQ_SAVE_STATE: state_save(arg / FE_SLOTS, arg % FE_SLOTS); break;
	case FE_REQ_LOAD_STATE: state_load(arg / FE_SLOTS, arg % FE_SLOTS); break;
	case FE_REQ_DELETE_STATE: state_delete(arg / FE_SLOTS, arg % FE_SLOTS); break;
	case FE_REQ_PLAY_REPLAY: replay_play(arg); break;
	case FE_REQ_DELETE_REPLAY: replay_delete(arg); break;
	case FE_REQ_KEEP_REPLAY: replay_keep(); break;
	case FE_REQ_DROP_REPLAY: xsdk_log_write("replays: not kept (%u frames)", (unsigned)rec.frames); break;
	}
}

// ---- the ROMs ----

static uint8_t *read_file(const char *path, size_t *len)
{
	FILE *f = fopen(path, "rb");
	if (!f)
		return NULL;
	fseek(f, 0, SEEK_END);
	const long n = ftell(f);
	fseek(f, 0, SEEK_SET);
	uint8_t *b = n > 0 ? malloc((size_t)n) : NULL;
	if (b && fread(b, 1, (size_t)n, f) != (size_t)n) {
		free(b);
		b = NULL;
	}
	fclose(f);
	*len = (size_t)n;
	return b;
}

static bool load_roms(BrRoms *roms)
{
	static const char *const paths[] = { "game:/batrider.zip", "game:/batriderj.zip", "game:/roms/batrider.zip" };
	for (unsigned i = 0; i < sizeof paths / sizeof paths[0]; i++) {
		size_t len = 0;
		uint8_t *zip = read_file(paths[i], &len);
		if (!zip) {
			xsdk_log_write("roms: %s not found", paths[i]);
			continue;
		}
		char err[160];
		const int r = romset_load(zip, len, roms, err, sizeof err);
		free(zip);
		if (!r) {
			xsdk_log_write("roms: batriderj loaded from %s (%u bytes)", paths[i], (unsigned)len);
			return true;
		}
		xsdk_log_write("roms: %s: %s", paths[i], err);
	}
	return false;
}

// ---- pads ----

// The first connected pad. Test 4: the front end (core/fe.c) maps it: stick or d-pad, A shot, B bomb, X special
// (through OPTIONS' button map), START start / the pause box, BACK the pause box, LB + RB the host menu.
static uint16_t pad_buttons(INPUT_GAMEPAD *g)
{
	for (uint32_t p = 0; p < 4; p++) {
		INPUT_STATE s = { 0 };
		if (XamInputGetState(p, 0, &s) == ERROR_SUCCESS) {
			*g = s.gamepad;
			return 1;
		}
	}
	memset(g, 0, sizeof *g);
	return 0;
}

// The pad as the front end's bits: the d-pad or the stick past its dead zone (half travel; test 1c: signed tests).
static uint16_t fe_bits(const INPUT_GAMEPAD *g)
{
	const uint16_t b = g->buttons;
	uint16_t f = 0;
	if (b & INPUT_GAMEPAD_DPAD_UP || g->thumb_ly > STICK_DEAD_ZONE)
		f |= FE_UP;
	if (b & INPUT_GAMEPAD_DPAD_DOWN || g->thumb_ly < -STICK_DEAD_ZONE)
		f |= FE_DOWN;
	if (b & INPUT_GAMEPAD_DPAD_LEFT || g->thumb_lx < -STICK_DEAD_ZONE)
		f |= FE_LEFT;
	if (b & INPUT_GAMEPAD_DPAD_RIGHT || g->thumb_lx > STICK_DEAD_ZONE)
		f |= FE_RIGHT;
	static const struct { uint16_t xinput, fe; } map[] = {
		{ INPUT_GAMEPAD_A, FE_A }, { INPUT_GAMEPAD_B, FE_B }, { INPUT_GAMEPAD_X, FE_X }, { INPUT_GAMEPAD_Y, FE_Y },
		{ INPUT_GAMEPAD_START, FE_START }, { INPUT_GAMEPAD_BACK, FE_BACK },
		{ INPUT_GAMEPAD_LEFT_SHOULDER, FE_LB }, { INPUT_GAMEPAD_RIGHT_SHOULDER, FE_RB },
	};
	for (unsigned i = 0; i < sizeof map / sizeof map[0]; i++)
		if (b & map[i].xinput)
			f |= map[i].fe;
	return f;
}

// Test 9: the buttons CONTROLS sets (fe.h FE_K_*), the triggers past XInput's threshold (ibara360's AF_TRIGGER), as
// SHOT / BOMB / SPECIAL bits for the pad fe_frame gets (and a replay records). Once a frame: AUTO SHOT counts frames.
enum { TRIGGER_DOWN = 30 };
static uint16_t game_bits(const INPUT_GAMEPAD *g)
{
	const uint16_t b = g->buttons;
	static const uint16_t key[FE_KEYS - 2] = { INPUT_GAMEPAD_A, INPUT_GAMEPAD_B, INPUT_GAMEPAD_X, INPUT_GAMEPAD_Y,
		INPUT_GAMEPAD_LEFT_SHOULDER, INPUT_GAMEPAD_RIGHT_SHOULDER };
	unsigned down = 0;
	for (int k = 0; k < FE_KEYS - 2; k++)
		if (b & key[k])
			down |= 1u << k;
	if (g->left_trigger > TRIGGER_DOWN)
		down |= 1u << FE_K_LT;
	if (g->right_trigger > TRIGGER_DOWN)
		down |= 1u << FE_K_RT;
	return fe_controls(down);
}

static uint32_t us_since(const struct timespec *a)
{
	struct timespec n;
	clock_gettime(CLOCK_MONOTONIC, &n);
	return (uint32_t)((n.tv_sec - a->tv_sec) * 1000000 + (n.tv_nsec - a->tv_nsec) / 1000);
}

static void error_screen(uint32_t width, uint32_t height)
{
	// No ROMs: a dark red screen until BACK (the log says what was looked for).
	for (;;) {
		INPUT_GAMEPAD g;
		pad_buttons(&g);
		if (g.buttons & INPUT_GAMEPAD_BACK)
			return;
		uint32_t address;
		if (!NT_SUCCESS(xsdk_present_begin(&address, BEGIN_TIMEOUT_MS)))
			return;
		xsdk_gpu_set_target(width, height);
		xsdk_gpu_resolve(address, width, height, ERROR_ARGB);
		xsdk_present_end();
	}
}

// ---- the front end's platform ----

static void fe_log(const char *fmt, ...)
{
	char line[LOG_CHARS];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(line, sizeof line, fmt, ap);
	va_end(ap);
	xsdk_log_write("%s", line);
}

// Before the front end resets the board for a MODE change: the sound thread's chip copy is reset with the board's.
static void fe_quiesce(void)
{
	wait_for(&sound.done, sound.kick);
}

static const char *const screen_names[] = { "YOKO", "TATE A", "TATE B" };
_Static_assert((int)SCREEN_YOKO == (int)FE_YOKO && (int)SCREEN_TATE_A == (int)FE_TATE_A &&
	(int)SCREEN_TATE_B == (int)FE_TATE_B, "the host menu's SCREEN indexes the quads");

// ---- test 14: the faster boot (user: "faster boot with a NOW LOADING ticker") ----
// Test 13's boot: the xsdk logo held 1.5 s doing nothing, then the zip read and unpacked on one thread (1034 ms), the
// graphics decoded a pixel at a time (936 ms), the menu sounds rendered by the board (938 ms). Now: the logo fades in
// while a loader thread reads the zip; its members unpack on four hardware threads (romset_set_runner); the graphics
// decode a row at a time (vdp.c); the menu sounds come from a file made at the first boot (batrider360_menusnd.bin);
// from the ROMs on a NOW LOADING ticker (a GADGETS panel in the board's own font, its glyphs read straight from the
// program ROM at 0x100000, the same the 68000 copies to the text layer) shows until the main loop's first frame.

enum { BOOT_HELPERS = 3, TICKER_THREAD = 2, TICKER_STEP = 5, TICKER_BLOCKS = 12, TICKER_FADE = 12, GLYPHS_ROM = 0x100000 };
static const uint32_t boot_hw[BOOT_HELPERS] = { 2, 4, 1 };   // the other two cores first (the main thread is on 0)

static struct {
	void (*fn)(int, void *);
	void *arg;
	int n, next;
} run_job;

static void run_take(void)
{
	for (;;) {
		const int i = __atomic_fetch_add(&run_job.next, 1, __ATOMIC_ACQ_REL);
		if (i >= run_job.n)
			return;
		run_job.fn(i, run_job.arg);
	}
}

static uint32_t run_main(void *context)
{
	(void)context;
	run_take();
	return 0;
}

// romset's runner: the calling thread and BOOT_HELPERS threads take the jobs in turn
static void boot_runner(void (*fn)(int, void *), void *arg, int n)
{
	run_job.fn = fn;
	run_job.arg = arg;
	run_job.n = n;
	__atomic_store_n(&run_job.next, 0, __ATOMIC_RELEASE);
	HANDLE h[BOOT_HELPERS];
	int k = 0;
	for (int i = 0; i < BOOT_HELPERS; i++)
		if (NT_SUCCESS(xsdk_thread_create_on(&h[k], WORKER_STACK, run_main, NULL, boot_hw[i])))
			k++;
	run_take();
	for (int i = 0; i < k; i++) {
		NtWaitForSingleObjectEx(h[i], 0, 0, NULL);
		NtClose(h[i]);
	}
}

static bool load_roms(BrRoms *roms);
static BrRoms boot_roms;
static bool boot_have_roms;
static uint32_t boot_rom_ms;

static uint32_t loader_main(void *context)
{
	(void)context;
	struct timespec t0;
	clock_gettime(CLOCK_MONOTONIC, &t0);
	boot_have_roms = load_roms(&boot_roms);
	boot_rom_ms = (uint32_t)(us_since(&t0) / 1000);
	return 0;
}

// the menu sounds: the file of an earlier boot, else rendered and the file written for the next
static void menu_sounds(void)
{
	static const char path[] = "game:/batrider360_menusnd.bin";
	struct timespec t0;
	clock_gettime(CLOCK_MONOTONIC, &t0);
	size_t len = 0;
	uint8_t *b = read_file(path, &len);
	const bool cached = b && len <= MENUSND_FILE_MAX && !menusnd_unpack(b, (int)len);
	free(b);
	if (!cached) {
		const int ms = menusnd_render();
		uint8_t *f = ms ? NULL : malloc(MENUSND_FILE_MAX);
		const int n = f ? menusnd_pack(f) : 0;
		FILE *o = n ? fopen(path, "wb") : NULL;
		const bool written = o && fwrite(f, 1, (size_t)n, o) == (size_t)n;
		if (o)
			fclose(o);
		free(f);
		xsdk_log_write("menu sounds: %s (%s)", ms ? "out of memory" : "made", written ? "batrider360_menusnd.bin "
			"written for the next boot" : "batrider360_menusnd.bin NOT written");
	}
	xsdk_log_write("menu sounds: %s in %u ms (cursor %d, decide %d, pause %d frames)", cached ?
		"read from batrider360_menusnd.bin" : "made", (unsigned)(us_since(&t0) / 1000), menusnd_frames(MS_CURSOR),
		menusnd_frames(MS_DECIDE), menusnd_frames(MS_PAUSE));
}

// The ticker: the display's frames from its own thread while the main thread loads (the workers are not up yet, so
// it has the GPU, texels and tex[] to itself). A GADGETS panel, NOW LOADING on its plate, a lit run of blocks
// crossing a row of them; it fades in, and out once ticker_stop asks.
static volatile int tk_state;          // 1 running, 2 fade out asked, 0 done
static HANDLE tk_thread;
static uint32_t tk_w, tk_h, tk_frames;
static int tk_wide;

static void ticker_picture(uint32_t frame, int level)
{
	md_target(texels);
	md_fill(0, 0, MD_W, MD_H, 0xFF000000u);
	const int bw = 12, gap = 6, row = TICKER_BLOCKS * (bw + gap) - gap;
	const int w = row + 2 * 26, h = MD_TOP + MD_BOTTOM + 34, x = (MD_W - w) / 2, y = (MD_H - h) / 2;
	md_panel(x, y, w, h);
	static const char text[] = "NOW LOADING";
	md_text(x + (w - md_text_w(3, text)) / 2, y + MD_PLATE_Y + (MD_PLATE_H - 12) / 2, 3, MD_YELLOW, text);
	const int head = (int)(frame / TICKER_STEP) % (TICKER_BLOCKS + 3);
	for (int i = 0; i < TICKER_BLOCKS; i++) {
		const int d = head - i;      // the run: its head, then two dimmer blocks behind
		const uint32_t c = d == 0 ? 0xFFFFC040u : d == 1 ? 0xFFC08020u : d == 2 ? 0xFF704010u : 0xFF282828u;
		md_fill(x + 26 + i * (bw + gap), y + MD_BODY_Y + 10, bw, bw, c);
	}
	front_fade(texels, level);
}

static uint32_t ticker_main(void *context)
{
	(void)context;
	FeOut o;
	memset(&o, 0, sizeof o);
	o.screen = FE_YOKO;
	o.picture = FE_PIC_FULL;
	o.wide = tk_wide;
	o.bright = 100;
	int out = -1;                      // frames of the fade out, -1 not begun
	for (uint32_t k = 0; out < TICKER_FADE; k++) {
		if (out < 0 && __atomic_load_n(&tk_state, __ATOMIC_ACQUIRE) == 2)
			out = 0;
		const int in = k < TICKER_FADE ? (int)k * 256 / TICKER_FADE : 256;
		const int level = out < 0 ? in : in * (TICKER_FADE - out) / TICKER_FADE;
		ticker_picture(k, level);
		const int t = (int)(k % TEXTURES);
		xsdk_texture_upload(&tex[t], 0, texels, TEX_W * 4);
		uint32_t address;
		if (!NT_SUCCESS(xsdk_present_begin(&address, BEGIN_TIMEOUT_MS)))
			break;
		draw(&o, NULL, t, NULL, address, tk_w, tk_h);
		xsdk_present_end();
		tk_frames = k + 1;
		if (out >= 0)
			out++;
	}
	__atomic_store_n(&tk_state, 0, __ATOMIC_RELEASE);
	return 0;
}

// the board's font from the program ROM's glyphs (vdp_text_glyphs' pixels: 1024 x 64, a nibble each; the ROM holds
// them in pixel order, two a byte), then the ticker's thread
static void ticker_start(const uint8_t *prg, uint32_t width, uint32_t height, int wide)
{
	uint8_t *glyphs = malloc(1024 * 64);   // (on the heap: .bss grows the .xex)
	if (glyphs)
		for (int i = 0; i < 1024 * 32; i++) {
			glyphs[i * 2] = prg[GLYPHS_ROM + i] >> 4;
			glyphs[i * 2 + 1] = prg[GLYPHS_ROM + i] & 15;
		}
	const int font = md_font(glyphs);
	free(glyphs);
	if (!font) {
		xsdk_log_write("now loading: no letters in the program ROM at 0x%X (no ticker)", (unsigned)GLYPHS_ROM);
		return;
	}
	tk_w = width;
	tk_h = height;
	tk_wide = wide;
	tk_state = 1;
	if (!NT_SUCCESS(xsdk_thread_create_on(&tk_thread, WORKER_STACK, ticker_main, NULL, TICKER_THREAD))) {
		tk_state = 0;
		xsdk_log_write("now loading: the ticker's thread did not start");
	}
}

static void ticker_stop(void)
{
	if (!tk_state)
		return;
	__atomic_store_n(&tk_state, 2, __ATOMIC_RELEASE);
	NtWaitForSingleObjectEx(tk_thread, 0, 0, NULL);
	NtClose(tk_thread);
	xsdk_log_write("now loading: %u frames shown", (unsigned)tk_frames);
}

int main(void)
{
	xsdk_log_write("batrider360 version 0.19 (test 19): TRAINING's MEDAL row (the starting medal value, ibara360's)");
	struct timespec boot_t0, t0;
	clock_gettime(CLOCK_MONOTONIC, &boot_t0);
	NTSTATUS st = xsdk_gpu_open();
	if (!NT_SUCCESS(st)) {
		xsdk_log_write("xsdk_gpu_open failed: 0x%08X", (unsigned)st);
		return 1;
	}
	// test 14: the zip read and unpacked on the loader's thread while the logo fades in (test 13: held 1.5 s first)
	romset_set_runner(boot_runner);
	HANDLE loader;
	const bool loader_up = NT_SUCCESS(xsdk_thread_create_on(&loader, WORKER_STACK, loader_main, NULL, LOG_THREAD));
	if (!loader_up)
		xsdk_log_write("roms: the loader's thread did not start (loading here)");
	const NTSTATUS splash = xsdk_splash_begin();
	if (!NT_SUCCESS(splash))
		xsdk_log_write("xsdk_splash_begin failed: 0x%08X", (unsigned)splash);
	if (!make_video())
		return 1;
	if (front_init()) {
		xsdk_log_write("front: the pictures did not unpack");
		return 1;
	}
	if (gadgets_init()) {
		xsdk_log_write("gadgets: the pictures did not unpack");
		return 1;
	}
	if (loader_up) {
		NtWaitForSingleObjectEx(loader, 0, 0, NULL);
		NtClose(loader);
	} else {
		loader_main(NULL);
	}
	BrRoms roms = boot_roms;
	static uint8_t *progs[2];
	bool have_roms = boot_have_roms;
	xsdk_log_write("roms: %u ms (the logo up after %u ms)", (unsigned)boot_rom_ms, (unsigned)(us_since(&boot_t0) /
		1000));

	// the display from here (it takes over from the logo): the ticker while the rest loads
	uint32_t width = 0, height = 0;
	st = xsdk_present_open(XSDK_DISPLAY_720P, BUFFERS, SWAP_INTERVAL, &width, &height);
	if (!NT_SUCCESS(st)) {
		xsdk_log_write("xsdk_present_open failed: 0x%08X", (unsigned)st);
		return 1;
	}
	xsdk_log_write("video: %ux%u", (unsigned)width, (unsigned)height);
	make_quads(width, height);
	// test 10: the console's display shape for ASPECT RATIO's AUTO (ibara360's VdQueryVideoMode)
	VIDEO_MODE vm;
	memset(&vm, 0, sizeof vm);
	VdQueryVideoMode(&vm);
	if (have_roms)
		ticker_start(roms.prg, width, height, vm.is_widescreen != 0);
	if (have_roms) {
		clock_gettime(CLOCK_MONOTONIC, &t0);
		if (br_init(&roms)) {
			xsdk_log_write("board: br_init failed (out of memory?)");
			return 1;
		}
		xsdk_log_write("board: graphics decoded in %u ms", (unsigned)(us_since(&t0) / 1000));
		for (int m = 0; m < 2 && have_roms; m++) {
			progs[m] = malloc(BR_PRG_SIZE);
			const int e = progs[m] ? progpatch_make(roms.prg, progs[m], m == FE_CHASE) : -3;
			if (e) {
				xsdk_log_write("programs: %s failed (%d: -1 CHASE!'s CRC, -2 boot site, -3 memory)",
					m == FE_CHASE ? "CHASE!" : "ORIGINAL", e);
				have_roms = false;
			}
		}
		if (have_roms) {
			xsdk_log_write("programs: ORIGINAL and CHASE! made (no ROM/RAM check)");
			// test 11: the menu sounds, played once by the board's sound side (the front end resets the board after);
			// test 14: from the file an earlier boot wrote
			menu_sounds();
			// The front end takes the board from here: its hooks in both programs, the saved MODE's program, the
			// DIPs per MODE (the N64's: free play, CHASE! 0xC6FC, ORIGINAL from GAME SETTINGS, DSWC 0xE0 = the
			// secret characters and modes on, Stage Edit off), the files beside the .xex.
			static const FePlatform plat = { plat_load, plat_save, fe_log, fe_quiesce, plat_slot, plat_replay };
			fe_display(vm.is_widescreen != 0);
			xsdk_log_write("display: the console says %s", vm.is_widescreen ? "16:9" : "4:3");
			if (fe_init(&plat, progs)) {
				xsdk_log_write("front end: fe_init failed (a hook site held other bytes, see above)");
				have_roms = false;
			} else {
				// test 8: the manual's table, the save states (all of them, into memory), the replays' heads
				if (!big_buffers()) {
					xsdk_log_write("test 8: out of memory for the states / replays / manual buffers");
					return 1;
				}
				man_open();
				slots_read();
				replays_read();
			}
		}
	}
	ticker_stop();
	xsdk_log_write("boot: %u ms to the first frame", (unsigned)(us_since(&boot_t0) / 1000));
	if (!have_roms) {
		xsdk_log_write("roms: put MAME's batrider.zip (merged set, 0.2xx) beside batrider360.xex");
		error_screen(width, height);
		xsdk_present_close();
		xsdk_gpu_close();
		return 1;
	}
	sound_on = start_audio();
	start_log_thread();
	br_set_split(1);
	br_set_clock(board_clock);
	if (!start_worker(&video, video_job, VIDEO_THREAD, "video", false) ||
		!start_worker(&sound, sound_job, SOUND_THREAD, "sound", true) ||
		!start_worker(&gadget, gadget_job, GADGET_THREAD, "gadget", true))
		return 1;

	uint32_t draw_us = 0, wait_us = 0, vwait = 0, swait = 0, gwait = 0;
	// test 2: the main loop's other parts (test 1c's stutter came from outside the timed ones); test 4: the front end
	// (board_step's StepStats since test 8)
	uint32_t pad_us = 0, pad_max = 0, log_us = 0, log_max = 0, loop_max = 0, loop_frame = 0;
	uint16_t last_bits = 0;
	int last_screen = -1, last_gadgets = -1, last_wide = -1, last_pic = -1;
	uint32_t gkick = 0, gskipped = 0;       // the gadget jobs handed over; frames that found the last one unfinished
	uint32_t gsame = 0;                     // test 5: frames with nothing new to draw (no job)
	static GadgetState glast;               // what the last job was handed (gjob may still be read by the gadget thread)
	int glast_screen = -1, glast_chase = -1, glast_on = -1;
	for (int k = 0; k < CANVAS_BUFFERS; k++)
		gjob_drawn[k] = -1;
	static FeOut shown;                     // what this frame shows: the front end's last frame (+ a replay's banner)
	uint64_t loop_t = tb_read();
	for (uint32_t frame = 0;; frame++) {
		uint64_t t = tb_read();
		const uint32_t loop = tb_us(t - loop_t);
		if (frame > 1 && loop > loop_max) {
			loop_max = loop;
			loop_frame = frame;
		}
		loop_t = t;
		INPUT_GAMEPAD g;
		pad_buttons(&g);
		const uint16_t bits = fe_bits(&g) | game_bits(&g);
		const FePad pad = { bits, (uint16_t)(bits & ~last_bits) };
		last_bits = bits;
		uint32_t us = tb_us(tb_read() - t);
		pad_us += us;
		if (us > pad_max)
			pad_max = us;

		// ---- the front end and the board's frame (test 8: a replay's 0..3 of them, from its recorded pads) ----
		const uint32_t n = frame + 1;
		// set n & 1 was last mixed by job n - 2, done before job n - 1 was handed over
		sjob_set = n & 1;
		sjob_n[sjob_set] = 0;
		sjob_se[sjob_set] = 0;
		const char *pb_over = NULL;
		bool pb_restart = false;
		fe_set_restart(playing ? 0 : stage_have, playing || state_slot < 0 ? 0 : state_gen);
		if (playing) {
			const int act = fe_playback_input(&pad);
			sjob_se[sjob_set] |= fe_menu_sounds_take();
			if (act == FE_PB_SLOWER && pb_speed > 0)
				pb_speed--;
			else if (act == FE_PB_FASTER && pb_speed < 3)
				pb_speed++;
			if (act == FE_PB_QUIT)
				pb_over = "REPLAY STOPPED";
			if (act == FE_PB_RESTART)
				pb_restart = true;
			const int steps = fe_playback_paused() || pb_over || pb_restart ? 0 : pb_speed ? pb_speed : (int)(frame & 1);
			for (int k = 0; k < steps; k++) {
				FePad rp;
				if (!snap_pads_next(&pb, &rp)) {
					pb_over = "REPLAY OVER";
					break;
				}
				board_step(&rp);
			}
			shown = out;
			fe_playback_decorate(&shown, pb_speed_names[pb_speed], pb.frame, pb_frames);
		} else {
			board_step(&pad);
			shown = out;
		}
		{
			// test 6: the Music panel follows the sound side, which plays on while the 68000 is held
			char title[24];
			const int tune = br_music();
			br_music_title(tune, title, sizeof title);
			gadgets_music(&gstate, tune, title);
		}
		if (shown.screen != last_screen || shown.gadgets != last_gadgets || shown.wide != last_wide ||
			shown.picture != last_pic) {
			static const char *const pic_words[] = { "INTEGER", "FULL", "CUSTOM" };
			xsdk_log_write("screen: %s, %s, picture %s, gadgets %s", screen_names[shown.screen], shown.wide ? "16:9" :
				"4:3", pic_words[shown.picture], shown.gadgets ? "on" : "off");
			last_screen = shown.screen;
			last_gadgets = shown.gadgets;
			last_wide = shown.wide;
			last_pic = shown.picture;
		}

		// the workers' job n is this frame (job 0 never runs)
		hand_over(&sound, n, &swait);

		// ---- test 8: replays and states, between frames ----
		// A credit's replay starts from a state taken after the frame it began in (its pads from the next frame) and
		// ends 60 frames after it is over (it flickers out for a frame at a QUIT). States are taken and loaded once
		// this frame's sound is through the sound thread's YM2151 copy, which is part of them.
		bool rec_begin = false;
		if (!playing) {
			// test 13: a new credit (fe's credit_gen) or stage 1's RESTART STAGE, not one without a replay (rp_drop)
			rec_begin = out.in_credit && !recording && !out.rp_drop && (out.credit_gen != rec_gen || rec_again);
			if (recording && out.rp_drop) {
				recording = false;
				xsdk_log_write("replays: none for this credit (%s, %u frames dropped)", out.rp_why, (unsigned)rec.frames);
			}
			rec_gone = out.in_credit ? 0 : rec_gone + 1;
			if (recording && out.in_credit) {
				rec_mode = out.chase_credit ? FE_CHASE : FE_ORIGINAL;
				if (gstate.stage_no > 0)
					rec_stage = (uint32_t)gstate.stage_no;
				rec_score = gstate.p[0].score;
				rec_red |= out.invalid != 0;
			}
			if (recording && rec_gone >= 60) {
				recording = false;
				xsdk_log_write("replays: the credit is over (%u frames%s)", (unsigned)rec.frames,
					rec.full ? ", cut short: the pad buffer was full" : "");
				if (rec.frames >= 600) {
					char l[3][32], tm[12];
					mmss(tm, sizeof tm, rec.frames);
					snprintf(l[0], sizeof l[0], "%s  STAGE %u", rec_mode == FE_CHASE ? "CHASE!" : "ORIGINAL",
						(unsigned)rec_stage);
					snprintf(l[1], sizeof l[1], "SCORE %u%s", (unsigned)rec_score, rec_red ? "*" : "");
					snprintf(l[2], sizeof l[2], "TIME %s%s", tm, rec.full ? " (CUT)" : "");
					fe_replay_prompt(l[0], l[1], l[2], rec_red);
				if (rec_red)
					xsdk_log_write("replays: SAVE REPLAY? with the score invalid");
				}
			}
		}
		// test 11: RESTART STAGE's stage start (taken with the replay's start, once the frame's sound is through)
		bool stage_take = false;
		if (!playing && !out.in_credit) {
			stage_seen = stage_have = 0;
			state_slot = -1;
		} else if (!playing) {
			const int st = br_rd8(STAGE_BYTE);
			if (st > 0 && st < 0x80 && st != stage_seen) {   // (0x80 on the select screens)
				stage_seen = st;
				stage_take = true;
			}
		}
		if (rec_begin || pb_over || pb_restart || out.req || stage_take) {
			timed_wait(&sound.done, n, &swait);
			if (pb_restart) {
				replay_restart();
			} else if (pb_over) {
				replay_end(pb_over);
			} else if (rec_begin) {
				rec_start_n = snap_save(rec_start, sizeof rec_start, 1);
				rec.n = rec.frames = 0;
				rec.full = 0;
				rec_stage = 0;
				rec_red = false;
				rec_gen = out.credit_gen;
				rec_again = false;
				recording = rec_start_n != 0;
				xsdk_log_write("replays: recording the credit (start state %u bytes)", (unsigned)rec_start_n);
			}
			if (stage_take) {
				stage_raw_n = snap_save(stage_raw, SNAP_RAW_MAX, 0);
				stage_have = stage_raw_n ? stage_seen : 0;
				xsdk_log_write("restart stage: stage %d start kept (%u bytes)", stage_seen, (unsigned)stage_raw_n);
			}
			if (out.req == FE_REQ_EXIT)
				break;                       // test 14: the title's EXIT (the BACK + START hold is gone)
			do_request(out.req, out.req_arg);
			out.req = FE_REQ_NONE;
		}

		timed_wait(&video.done, n - 1, &vwait);
		memcpy(&vjob, shown.vsrc, sizeof vjob);
		fjob = shown.job;
		// job n - 1's texture is done (its done count was just waited for): drawn while job n is made
		hand_over(&video, n, &vwait);
		// the gadget thread's last job finished: the next one (otherwise it goes on and this frame's numbers wait).
		// Test 5: only when the canvas would change: other numbers, screen shape, CHASE! panels, GADGETS on / off,
		// or P1's full shot charge (its bar flashes every drawn frame).
		const uint32_t gdone = __atomic_load_n(&gadget.done, __ATOMIC_ACQUIRE);
		const int gon = shown.gadgets != 0, gshape = shown.screen != SCREEN_YOKO;
		const bool gchanged = gon != glast_on || (gon && (gshape != (glast_screen != SCREEN_YOKO) ||
			shown.chase_credit != glast_chase || gstate.p[0].shot_charge == 0x48 || memcmp(&gstate, &glast, sizeof gstate)));
		if (!gchanged) {
			gsame++;
		} else if (gdone == gkick) {
			gjob = glast = gstate;
			gjob_screen = glast_screen = shown.screen;
			gjob_chase = glast_chase = shown.chase_credit;
			glast_on = gon;
			gjob_on = gon != 0;
			gkick++;
			hand_over(&gadget, gkick, &gwait);
		} else {
			gskipped++;
		}

		t = tb_read();
		uint32_t address;
		st = xsdk_present_begin(&address, BEGIN_TIMEOUT_MS);
		if (!NT_SUCCESS(st)) {
			xsdk_log_write("xsdk_present_begin failed: 0x%08X", (unsigned)st);
			break;
		}
		wait_us += tb_us(tb_read() - t);
		t = tb_read();
		if (n > 1) {
			XsdkTexture *tx = &tex[(n - 1) % TEXTURES];
			xsdk_gpu_invalidate(tx->address, tx->size_bytes);
			// the newest finished canvas, if it was drawn for a canvas of the same shape as the screen now (a SCREEN
			// change between YOKO and TATE shows the game at once and its gadgets a job later; none while GADGETS is off)
			XsdkTexture *cv = NULL;
			const int gs = gdone ? gjob_drawn[gdone % CANVAS_BUFFERS] : -1;
			if (shown.gadgets && gs >= 0 && (gs != SCREEN_YOKO) == (shown.screen != SCREEN_YOKO)) {
				cv = &ctex[gs != SCREEN_YOKO][gdone % CANVAS_BUFFERS];
				xsdk_gpu_invalidate(cv->address, cv->size_bytes);
			}
			// test 8: the manual's pages the front end shows (the log thread loads them; the last ones meanwhile)
			const XsdkTexture *page = NULL;
			if (shown.manual) {
				const uint32_t tate = shown.screen != SCREEN_YOKO;
				const uint32_t first = (uint32_t)(tate ? shown.man_page : shown.man_page & ~1);
				__atomic_store_n(&man_want, (uint32_t)shown.man_lang << 16 | tate << 15 | first, __ATOMIC_RELEASE);
				const uint32_t d = __atomic_load_n(&man_done, __ATOMIC_ACQUIRE);
				if (d != MAN_NONE && (d >> 15 & 1) == tate) {
					page = &mtex[tate][d >> 30 & 1];
					xsdk_gpu_invalidate(page->address, page->size_bytes);
				}
			}
			draw(&shown, cv, (int)((n - 1) % TEXTURES), page, address, width, height);
		} else {
			xsdk_gpu_set_target(width, height);
			xsdk_gpu_resolve(address, width, height, CLEAR_ARGB);
		}
		xsdk_present_end();
		draw_us += tb_us(tb_read() - t);

		if ((frame + 1) % STATS_EVERY == 0) {
			t = tb_read();
			BrProfile pr;
			br_profile_take(&pr);
			xsdk_log_write("frame %u: board %u us avg (max %u; 68000 %u, Z80 %u, OKI+YM timers %u), %u frames over 16 ms; "
				"video thread %u us (max %u), sound thread %u us (max %u), gadget thread %u us (max %u), waits for them "
				"%u / %u / %u us; draw %u us, present wait %u us; sound queue %u, underruns %u, overruns %u",
				(unsigned)(frame + 1), (unsigned)(ss.emu_us / STATS_EVERY), (unsigned)ss.emu_max,
				(unsigned)(tb_us(pr.m68k) / STATS_EVERY), (unsigned)(tb_us(pr.z80) / STATS_EVERY),
				(unsigned)(tb_us(pr.sound) / STATS_EVERY), (unsigned)ss.slow,
				(unsigned)(video.us / STATS_EVERY), (unsigned)video.max_us, (unsigned)(sound.us / STATS_EVERY),
				(unsigned)sound.max_us, (unsigned)(gadget.us / STATS_EVERY), (unsigned)gadget.max_us,
				(unsigned)(vwait / STATS_EVERY), (unsigned)(swait / STATS_EVERY), (unsigned)(gwait / STATS_EVERY),
				(unsigned)(draw_us / STATS_EVERY), (unsigned)(wait_us / STATS_EVERY),
				(unsigned)(ring_w - ring_r), (unsigned)underruns, (unsigned)overruns);
			XsdkPresentStats ps;
			xsdk_present_stats(&ps);
			xsdk_log_write("present: %u vblanks, %u flips, %u repeated (no frame ready), %u late flips, longest begin "
				"wait %u us", (unsigned)ps.vblanks, (unsigned)ps.flips, (unsigned)ps.missed, (unsigned)ps.late,
				(unsigned)ps.longest_wait_us);
			xsdk_log_write("main loop: longest frame %u us at frame %u; pad reads %u us avg (max %u); front end %u us "
				"avg (max %u); the last stats lines took %u us (into the ring; max %u); the log thread's longest file "
				"write %u us; sound queue refilled %u times; files written %u, failed %u, dropped %u; gadget frames late %u, unchanged %u; %s%s%s",
				(unsigned)loop_max, (unsigned)loop_frame, (unsigned)(pad_us / STATS_EVERY), (unsigned)pad_max,
				(unsigned)(ss.fe_us / STATS_EVERY), (unsigned)ss.fe_max, (unsigned)log_us, (unsigned)log_max,
				(unsigned)log_write_max, (unsigned)reprimes, (unsigned)saves_written, (unsigned)saves_failed,
				(unsigned)saves_dropped, (unsigned)gskipped, (unsigned)gsame, out.in_credit ? (out.chase_credit ? "CHASE! credit" : "ORIGINAL credit") :
				"no credit", out.paused ? ", paused" : "", out.host_menu ? ", host menu" : "");
			if (playing || recording)
				xsdk_log_write("replays: %s, frame %u", playing ? "playing" : "recording", (unsigned)(playing ? pb.frame : rec.frames));
			log_write_max = 0;
			memset(&ss, 0, sizeof ss);
			draw_us = wait_us = vwait = swait = gwait = 0;
			pad_us = pad_max = loop_max = gskipped = gsame = 0;
			video.us = video.max_us = sound.us = sound.max_us = gadget.us = gadget.max_us = 0;
			us = tb_us(tb_read() - t);
			log_us = us;
			log_max = us;
		}
	}
	xsdk_log_write("exit: the title's EXIT");
	// the workers finish their last jobs before the sound and the GPU go away
	wait_for(&sound.done, sound.kick);
	wait_for(&video.done, video.kick);
	wait_for(&gadget.done, gadget.kick);
	if (sound_on)
		stop_audio();
	xsdk_present_close();
	xsdk_gpu_close();
	stop_log_thread();
	xsdk_log_write("exit: status 0");
	return 0;
}
