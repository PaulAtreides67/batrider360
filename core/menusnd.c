/* batrider360 test 11: the menu sounds (menusnd.h). */
#include "menusnd.h"

#include <stdlib.h>
#include <string.h>

#include "board.h"

/* the N64 front end's SE codes (work/n64_frontend_spec.md 0.2): 0x24 cursor / back, 0x22 decide, 0x28 pause */
/* test 21: the toggle (0x2B) and the demo's START (0x21) too */
static const uint8_t codes[MS_SOUNDS] = { 0x26, 0x27, 0x28, 0x2B, 0x21 };
enum {
	WARM = 30,          /* frames of the 68000's boot (it starts the sound side) before the first code */
	MAX_FRAMES = 90,    /* the longest kept (1.5 s) */
	QUIET = 8,          /* a sound is over after this many silent frames */
	SILENT = 24,        /* |sample| at most this = silence */
};
static int16_t *pcm[MS_SOUNDS];
static int frames_of[MS_SOUNDS];
static int pos[MS_SOUNDS] = { -1, -1, -1, -1, -1 };   /* the sound thread's voices: next sample, -1 = not playing */

static int quiet(const int16_t *s)
{
	for (int i = 0; i < BR_AUDIO_PER_FRAME; i++)
		if (s[i] > SILENT || s[i] < -SILENT)
			return 0;
	return 1;
}

/* a board frame and its sound (split: no picture drawn) */
static void step(int16_t *out)
{
	static BrSound bs;
	br_run_frame();
	br_sound_take(&bs);
	memset(out, 0, BR_AUDIO_PER_FRAME * sizeof *out);
	br_sound_mix(&bs, out);
}

int menusnd_render(void)
{
	int r = 0;
	int16_t a[BR_AUDIO_PER_FRAME] = { 0 };
	br_set_split(1);
	br_reset();
	br_hold_main(0);
	for (int f = 0; f < WARM; f++)
		step(a);                        /* the 68000's boot starts the sound side */
	br_hold_main(1);
	for (int id = 0; id < MS_SOUNDS; id++) {
		for (int f = 0; f < 60 && !quiet(a); f++)
			step(a);
		int16_t *b = malloc((size_t)MAX_FRAMES * BR_AUDIO_PER_FRAME * sizeof *b);
		if (!b) {
			r = -1;
			break;
		}
		/* the 68000's SE routine 0x2EF0 without its gates: 0 to 0x500021, the code to 0x500023 */
		br_wr16(0x500020, 0x0000);
		br_wr16(0x500022, codes[id]);
		int n = 0, last = 0, still = 0, heard = 0;
		while (n < MAX_FRAMES && still < QUIET) {
			step(b + n * BR_AUDIO_PER_FRAME);
			if (quiet(b + n * BR_AUDIO_PER_FRAME)) {
				still += heard;
			} else {
				heard = 1;
				still = 0;
				last = n + 1;
			}
			n++;
		}
		memcpy(a, b + (n - 1) * BR_AUDIO_PER_FRAME, sizeof a);
		pcm[id] = b;
		frames_of[id] = last;           /* the silent frames after it left out */
	}
	br_hold_main(0);
	br_set_split(0);
	br_reset();
	return r;
}

int menusnd_frames(int id)
{
	return id >= 0 && id < MS_SOUNDS ? frames_of[id] : 0;
}

void menusnd_mix(int16_t *out, unsigned starts, int volume)
{
	for (int id = 0; id < MS_SOUNDS; id++) {
		if (starts >> id & 1)
			pos[id] = 0;
		if (pos[id] < 0 || !pcm[id])
			continue;
		const int end = frames_of[id] * BR_AUDIO_PER_FRAME;
		const int16_t *s = pcm[id] + pos[id];
		const int n = end - pos[id] < BR_AUDIO_PER_FRAME ? end - pos[id] : BR_AUDIO_PER_FRAME;
		for (int i = 0; i < n; i++) {
			int v = out[i] + (s[i] * volume >> 8);
			out[i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
		}
		pos[id] = pos[id] + n >= end ? -1 : pos[id] + n;
	}
}

void menusnd_stop(void)
{
	for (int id = 0; id < MS_SOUNDS; id++)
		pos[id] = -1;
}

/* "BRMS", the format (FILE_VERSION: a change to the codes, the board's sound or the mix makes a new one), the frames
 * per frame's samples, then each sound's frame count (u16) and its samples (s16), all big-endian */
enum { FILE_VERSION = 3 };             /* test 21: the five sounds (3) */

static void put16(uint8_t *p, int v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
static int get16(const uint8_t *p) { return (int16_t)(p[0] << 8 | p[1]); }

int menusnd_pack(uint8_t *buf)
{
	if (BR_AUDIO_PER_FRAME > 1600)
		return 0;
	memcpy(buf, "BRMS", 4);
	put16(buf + 4, FILE_VERSION);
	put16(buf + 6, BR_AUDIO_PER_FRAME);
	int n = 8;
	for (int id = 0; id < MS_SOUNDS; id++) {
		if (!pcm[id])
			return 0;
		put16(buf + n, frames_of[id]);
		n += 2;
		for (int i = 0; i < frames_of[id] * BR_AUDIO_PER_FRAME; i++, n += 2)
			put16(buf + n, pcm[id][i]);
	}
	return n;
}

int menusnd_unpack(const uint8_t *buf, int len)
{
	if (len < 8 || memcmp(buf, "BRMS", 4) || get16(buf + 4) != FILE_VERSION || get16(buf + 6) != BR_AUDIO_PER_FRAME)
		return -1;
	int n = 8;
	int16_t *got[MS_SOUNDS] = { 0 };
	int frames[MS_SOUNDS];
	for (int id = 0; id < MS_SOUNDS; id++) {
		if (n + 2 > len)
			break;
		frames[id] = get16(buf + n);
		n += 2;
		const int k = frames[id] * BR_AUDIO_PER_FRAME;
		if (frames[id] < 0 || frames[id] > MAX_FRAMES || n + 2 * k > len || !(got[id] = malloc((size_t)(k ? k : 1) * 2)))
			break;
		for (int i = 0; i < k; i++, n += 2)
			got[id][i] = (int16_t)get16(buf + n);
	}
	if (!got[MS_SOUNDS - 1] || n != len) {
		for (int id = 0; id < MS_SOUNDS; id++)
			free(got[id]);
		return -1;
	}
	for (int id = 0; id < MS_SOUNDS; id++) {
		pcm[id] = got[id];
		frames_of[id] = frames[id];
	}
	return 0;
}
