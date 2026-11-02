/* batrider360: save states and replays (test 8), platform-free: the 360's main.c and the PC harness use it the same
 * way. A state is the whole emulated board between two frames plus the front end's credit state (stio.h lists who
 * keeps what). A replay is a state taken as a credit starts plus every pad fe_frame took from then on (the frames the
 * front end marks out.rec); fed back through fe_frame from that state, the board runs the same credit again, frame
 * for frame. Call everything on the board's thread with the sound thread idle (its YM2151 copy is part of a state). */
#ifndef BR_SNAP_H
#define BR_SNAP_H
#include <stddef.h>
#include <stdint.h>

#include "fe.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
	SNAP_VERSION = 1,                  /* a state of another version is refused (the layout changed) */
	SNAP_RAW_MAX = 384 * 1024,         /* a state before packing (~190 KB) */
};

/* The state into buf (settings: also OPTIONS and LANGUAGE, for a replay's start). Bytes, 0 = did not fit. */
size_t snap_save(uint8_t *buf, size_t cap, int settings);
/* 0 = loaded; -1 = refused (version, layout) or broken: the board and front end are then as they were. */
int snap_load(const uint8_t *buf, size_t n);

/* zlib (deflate level 1 / inflate) without malloc: each caller hands its own work area (one per thread). */
enum { SNAP_ZWORK_DEFLATE = 400 * 1024, SNAP_ZWORK_INFLATE = 48 * 1024 };
size_t snap_pack(const uint8_t *in, size_t n, uint8_t *out, size_t cap, uint8_t *work, size_t work_size);
size_t snap_unpack(const uint8_t *in, size_t n, uint8_t *out, size_t cap, uint8_t *work, size_t work_size);

/* A replay's pads as runs of the same (held, press). */
typedef struct {
	uint16_t held, press, count;
} SnapRun;
typedef struct {
	SnapRun *run;                      /* the caller's array */
	uint32_t n, cap;
	uint32_t frames;
	int full;                          /* cap reached: later pads dropped (the replay ends there) */
} SnapPads;
void snap_pads_add(SnapPads *p, const FePad *pad);
typedef struct {
	const SnapRun *run;
	uint32_t n, i, used;               /* run i, of which used pads are given out */
	uint32_t frame;
} SnapReader;
int snap_pads_next(SnapReader *r, FePad *pad);   /* 0 = the replay is over */

/* big-endian helpers for the files */
static inline void snap_be32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }
static inline uint32_t snap_rd32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }

#ifdef __cplusplus
}
#endif
#endif
