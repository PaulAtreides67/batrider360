/* batrider360: save states and replays (snap.h). */
#include "snap.h"

#include <string.h>

#include "board.h"
#include "oki.h"
#include "stio.h"
#include "vdp.h"
#include "z80.h"
#include "zlib.h"

enum { MAGIC = 0x42525331 };            /* "BRS1" */

typedef struct {
	uint32_t magic, version, layout, settings;
} Head;

/* The structs a state copies whole: another layout = another build's state, refused. */
static uint32_t layout(void)
{
	return (uint32_t)(sizeof(Vdp) * 3u + sizeof(z80) * 5u + sizeof(Oki) * 7u + m68k_state_size() * 11u +
		BR_RAM_SIZE);
}

static void all_io(Stio *s, int settings)
{
	br_state_io(s);
	fe_state_io(s, settings);
	chasehud_state_io(s);
	gadgets_state_io(s);
}

size_t snap_save(uint8_t *buf, size_t cap, int settings)
{
	Stio s = { .buf = buf, .cap = cap };
	Head h = { MAGIC, SNAP_VERSION, layout(), (uint32_t)(settings != 0) };
	STIO(&s, h);
	all_io(&s, settings);
	return s.err ? 0 : s.at;
}

static uint8_t backup[SNAP_RAW_MAX];

int snap_load(const uint8_t *buf, size_t n)
{
	Head h;
	if (n < sizeof h)
		return -1;
	memcpy(&h, buf, sizeof h);
	if (h.magic != MAGIC || h.version != SNAP_VERSION || h.layout != layout() || h.settings > 1)
		return -1;
	const size_t bn = snap_save(backup, sizeof backup, 1);
	Stio s = { .buf = (uint8_t *)buf, .cap = n, .load = 1 };
	s.at = sizeof h;
	all_io(&s, (int)h.settings);
	if (!s.err && s.at == n)
		return 0;
	/* broken: everything back as it was */
	Stio b = { .buf = backup, .cap = bn, .load = 1 };
	b.at = sizeof h;
	all_io(&b, 1);
	return -1;
}

/* ---- zlib in a work area ---- */
typedef struct {
	uint8_t *base;
	size_t size, used;
} Arena;

static voidpf arena_alloc(voidpf opaque, uInt items, uInt size)
{
	Arena *a = opaque;
	const size_t n = ((size_t)items * size + 15) & ~(size_t)15;
	if (a->used + n > a->size)
		return Z_NULL;
	void *p = a->base + a->used;
	a->used += n;
	return p;
}

static void arena_free(voidpf opaque, voidpf p)
{
	(void)opaque;
	(void)p;
}

size_t snap_pack(const uint8_t *in, size_t n, uint8_t *out, size_t cap, uint8_t *work, size_t work_size)
{
	Arena a = { work, work_size, 0 };
	z_stream z;
	memset(&z, 0, sizeof z);
	z.zalloc = arena_alloc;
	z.zfree = arena_free;
	z.opaque = &a;
	if (deflateInit2(&z, 1, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY) != Z_OK)
		return 0;
	z.next_in = (Bytef *)in;
	z.avail_in = (uInt)n;
	z.next_out = out;
	z.avail_out = (uInt)cap;
	const int r = deflate(&z, Z_FINISH);
	const size_t got = cap - z.avail_out;
	deflateEnd(&z);
	return r == Z_STREAM_END ? got : 0;
}

size_t snap_unpack(const uint8_t *in, size_t n, uint8_t *out, size_t cap, uint8_t *work, size_t work_size)
{
	Arena a = { work, work_size, 0 };
	z_stream z;
	memset(&z, 0, sizeof z);
	z.zalloc = arena_alloc;
	z.zfree = arena_free;
	z.opaque = &a;
	if (inflateInit(&z) != Z_OK)
		return 0;
	z.next_in = (Bytef *)in;
	z.avail_in = (uInt)n;
	z.next_out = out;
	z.avail_out = (uInt)cap;
	const int r = inflate(&z, Z_FINISH);
	const size_t got = cap - z.avail_out;
	inflateEnd(&z);
	return r == Z_STREAM_END ? got : 0;
}

/* ---- pads ---- */
void snap_pads_add(SnapPads *p, const FePad *pad)
{
	if (p->full)
		return;
	SnapRun *last = p->n ? &p->run[p->n - 1] : NULL;
	if (last && last->held == pad->held && last->press == pad->press && last->count < 0xFFFF) {
		last->count++;
	} else if (p->n < p->cap) {
		p->run[p->n++] = (SnapRun){ pad->held, pad->press, 1 };
	} else {
		p->full = 1;
		return;
	}
	p->frames++;
}

int snap_pads_next(SnapReader *r, FePad *pad)
{
	while (r->i < r->n && r->used >= r->run[r->i].count) {
		r->i++;
		r->used = 0;
	}
	if (r->i >= r->n)
		return 0;
	pad->held = r->run[r->i].held;
	pad->press = r->run[r->i].press;
	r->used++;
	r->frame++;
	return 1;
}
