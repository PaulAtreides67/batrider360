/* batrider360: the ROMs from MAME's batrider.zip (merged: batriderj's program in batriderj/prg0b.u22) or a split
 * batriderj.zip holding prg0b.u22 at its top. Each member is inflated with zlib and checked against MAME's CRC. */
#include "romset.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zlib.h"

typedef struct {
	const char *names[2];              /* merged name, split name */
	uint32_t size, crc;
} Member;

/* raizing_batrider.cpp ROM_START( batriderj ) */
static const Member members[] = {
	{{"batriderj/prg0b.u22", "prg0b.u22"}, 0x80000, 0x4f3fc729},
	{{"prg1b.u23", "prg1b.u23"}, 0x80000, 0x8e70b492},
	{{"prg2.u21", "prg2.u21"}, 0x80000, 0xbdaa5fbf},
	{{"prg3.u24", "prg3.u24"}, 0x80000, 0x7aa9f941},
	{{"snd.u77", "snd.u77"}, 0x40000, 0x56682696},
	{{"rom-1.bin", "rom-1.bin"}, 0x400000, 0x0df69ca2},
	{{"rom-3.bin", "rom-3.bin"}, 0x400000, 0x60167d38},
	{{"rom-2.bin", "rom-2.bin"}, 0x400000, 0x1bfea593},
	{{"rom-4.bin", "rom-4.bin"}, 0x400000, 0xbee03c94},
	{{"rom-5.bin", "rom-5.bin"}, 0x100000, 0x4274daf6},
	{{"rom-6.bin", "rom-6.bin"}, 0x100000, 0x2a1c2426},
};
enum { NMEMBERS = sizeof members / sizeof members[0] };

static uint32_t le16(const uint8_t *p) { return p[0] | p[1] << 8; }
static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

static int fail(char *err, size_t n, const char *fmt, const char *what)
{
	snprintf(err, n, fmt, what);
	return -1;
}

/* test 14: zlib's memory from a block of the member's own, so the unpacking threads never call malloc (inflate with
 * Z_FINISH into the whole member takes its state alone, no window) */
enum { ARENA = 64 * 1024 };
typedef struct { uint8_t *p; uint32_t used; } Arena;

static voidpf arena_alloc(voidpf opaque, uInt items, uInt size)
{
	Arena *a = opaque;
	const uint32_t n = ((uint32_t)items * size + 15) & ~15u;
	if (a->used + n > ARENA)
		return Z_NULL;
	a->used += n;
	return a->p + a->used - n;
}

static void arena_free(voidpf opaque, voidpf p) { (void)opaque; (void)p; }

/* the member's data, stored or deflated, into dst (size bytes) */
static int extract(const uint8_t *zip, size_t zlen, uint32_t local, uint32_t method, uint32_t csize, uint8_t *dst,
	uint32_t size, Arena *arena)
{
	if ((size_t)local + 30 > zlen || le32(zip + local) != 0x04034b50)
		return -1;
	size_t data = local + 30 + le16(zip + local + 26) + le16(zip + local + 28);
	if (data + csize > zlen)
		return -1;
	if (method == 0)
		return csize == size ? (memcpy(dst, zip + data, size), 0) : -1;
	if (method != 8)
		return -1;
	z_stream s;
	memset(&s, 0, sizeof s);
	s.zalloc = arena_alloc;
	s.zfree = arena_free;
	s.opaque = arena;
	if (inflateInit2(&s, -MAX_WBITS) != Z_OK)
		return -1;
	s.next_in = (Bytef *)(zip + data);
	s.avail_in = csize;
	s.next_out = dst;
	s.avail_out = size;
	int r = inflate(&s, Z_FINISH);
	inflateEnd(&s);
	return r == Z_STREAM_END && s.total_out == size ? 0 : -1;
}

/* test 14: one member's unpacking and CRC check (result 0, -1 not unpacked, -2 CRC) */
typedef struct {
	const uint8_t *zip;
	size_t zlen;
	uint32_t local, method, csize;
	uint8_t *dst;
	uint32_t size, crc;
	int result;
	Arena arena;
} Job;

static RomsetRunner runner;

void romset_set_runner(RomsetRunner run) { runner = run; }

/* the largest members first (the runner's threads take them in this order) */
static const uint8_t biggest_first[] = { 5, 6, 7, 8, 9, 10, 0, 1, 2, 3, 4 };
_Static_assert(sizeof biggest_first == NMEMBERS, "every member once");

static void unpack(int i, void *arg)
{
	Job *j = (Job *)arg + biggest_first[i];
	j->result = extract(j->zip, j->zlen, j->local, j->method, j->csize, j->dst, j->size, &j->arena) ? -1 :
		(uint32_t)crc32(0, j->dst, j->size) != j->crc ? -2 : 0;
}

int romset_load(const uint8_t *zip, size_t zlen, BrRoms *out, char *err, size_t errlen)
{
	/* end of central directory, within the last 64 KB + 22 bytes */
	size_t eocd = 0;
	for (size_t i = zlen >= 22 ? zlen - 22 : 0, stop = zlen > 0x10016 ? zlen - 0x10016 : 0;; i--) {
		if (le32(zip + i) == 0x06054b50) {
			eocd = i + 1;
			break;
		}
		if (i == stop)
			break;
	}
	if (!eocd)
		return fail(err, errlen, "%s: not a zip file", "batrider.zip");
	eocd--;
	uint32_t count = le16(zip + eocd + 10), cd = le32(zip + eocd + 16);

	/* the program: ROM_LOAD16_BYTE pairs, prg0b/prg1b at 0, prg2/prg3 at 0x100000; the graphics members (5-8) are
	 * unpacked straight into the graphics (test 14) */
	static uint8_t *prg, *gfx;
	if (!prg && !(prg = malloc(BR_PRG_SIZE)))
		return fail(err, errlen, "%s: out of memory", "program");
	if (!gfx && !(gfx = malloc(BR_GFX_SIZE)))
		return fail(err, errlen, "%s: out of memory", "graphics");
	static uint8_t *bufs[NMEMBERS];
	int found[NMEMBERS] = {0};
	for (int m = 0; m < NMEMBERS; m++) {
		if (m >= 5 && m < 9)
			bufs[m] = gfx + (m - 5) * 0x400000;
		else if (!bufs[m] && !(bufs[m] = malloc(members[m].size)))
			return fail(err, errlen, "%s: out of memory", members[m].names[0]);
	}

	/* test 14 (faster boot): the members found first, then unpacked and checked by the runner (in parallel on the
	 * 360) */
	Job jobs[NMEMBERS];
	size_t p = cd;
	for (uint32_t e = 0; e < count; e++) {
		if (p + 46 > zlen || le32(zip + p) != 0x02014b50)
			return fail(err, errlen, "%s: damaged central directory", "batrider.zip");
		uint32_t method = le16(zip + p + 10), crc = le32(zip + p + 16), csize = le32(zip + p + 20);
		uint32_t usize = le32(zip + p + 24), nlen = le16(zip + p + 28), local = le32(zip + p + 42);
		const char *name = (const char *)zip + p + 46;
		for (int m = 0; m < NMEMBERS; m++) {
			if (found[m])
				continue;
			int hit = 0;
			for (int k = 0; k < 2; k++)
				if (strlen(members[m].names[k]) == nlen && !memcmp(name, members[m].names[k], nlen))
					hit = 1;
			if (!hit || usize != members[m].size || crc != members[m].crc)
				continue;
			jobs[m] = (Job){ zip, zlen, local, method, csize, bufs[m], usize, members[m].crc, 0, { 0, 0 } };
			found[m] = 1;
		}
		p += 46 + nlen + le16(zip + p + 30) + le16(zip + p + 32);
	}
	for (int m = 0; m < NMEMBERS; m++)
		if (!found[m])
			return fail(err, errlen, "%s: missing or not the expected dump", members[m].names[0]);
	uint8_t *arenas = malloc((size_t)NMEMBERS * ARENA);
	if (!arenas)
		return fail(err, errlen, "%s: out of memory", "zlib");
	for (int m = 0; m < NMEMBERS; m++)
		jobs[m].arena.p = arenas + (size_t)m * ARENA;
	if (runner)
		runner(unpack, jobs, NMEMBERS);
	else
		for (int m = 0; m < NMEMBERS; m++)
			unpack(m, jobs);
	free(arenas);
	for (int m = 0; m < NMEMBERS; m++) {
		if (jobs[m].result == -1)
			return fail(err, errlen, "%s: could not be unpacked", members[m].names[0]);
		if (jobs[m].result)
			return fail(err, errlen, "%s: CRC mismatch after unpacking", members[m].names[0]);
	}

	for (int h = 0; h < 2; h++)
		for (int i = 0; i < 0x80000; i++) {
			prg[h * 0x100000 + i * 2] = bufs[h * 2][i];
			prg[h * 0x100000 + i * 2 + 1] = bufs[h * 2 + 1][i];
		}
	out->prg = prg;
	out->snd = bufs[4];
	out->gfx = gfx;
	out->oki[0] = bufs[9];
	out->oki[1] = bufs[10];
	for (int i = 0; i < 4; i++) {
		free(bufs[i]);
		bufs[i] = NULL;
	}
	return 0;
}
