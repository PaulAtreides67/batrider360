/* batrider360: the save-state serializer (test 8). One Stio walks every module's state the same way to save it (into
 * buf) or load it (out of buf), so a module lists its fields once. Native byte order and struct layout: a state is
 * read back by the same .xex build family only (snap.c's SNAP_VERSION). */
#ifndef BR_STIO_H
#define BR_STIO_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct {
	uint8_t *buf;                      /* save: written up to cap; load: read up to cap */
	size_t cap, at;
	int load;                          /* 0 save, 1 load */
	int err;                           /* save: ran out of room; load: ran past the end */
} Stio;

static inline void stio(Stio *s, void *p, size_t n)
{
	if (s->err || s->at + n > s->cap) {
		s->err = 1;
		return;
	}
	if (s->load)
		memcpy(p, s->buf + s->at, n);
	else
		memcpy(s->buf + s->at, p, n);
	s->at += n;
}

#define STIO(s, v) stio((s), &(v), sizeof(v))

#ifdef __cplusplus
extern "C" {
#endif
/* each module's part (snap.c calls them in this order) */
void br_state_io(Stio *s);             /* board.c: RAM, latches, Z80, OKIs, VDP, the 68000, both YM2151s */
void m68k_state_io(Stio *s);           /* m68kstate.c */
unsigned m68k_state_size(void);
void ym_state_io(Stio *s);             /* ym.cpp: both chips (the sound thread must be idle) */
void fe_state_io(Stio *s, int settings); /* fe.c: the front end's credit state (+ OPTIONS and LANGUAGE for replays) */
void chasehud_state_io(Stio *s);
void gadgets_state_io(Stio *s);
#ifdef __cplusplus
}
#endif
#endif
