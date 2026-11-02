/* batrider360: the two programs the title can start, made from batriderj's at load.
 * ORIGINAL = batriderj as dumped; CHASE! = with the batrider-chase project's 68000 patch (build/gen/chase_patch.h,
 * tools/mkpatch.py). Both skip the power-on ROM/RAM check, as the N64 version does. */
#include "progpatch.h"

#include <string.h>

#include "m68k.h"
#include "zlib.h"
#include "chase_patch.h"

typedef struct {
	uint32_t addr;
	const char *old, *new_;            /* hex */
} Site;

/* 0x0002CA jsr 0x176CE: the ROM RAM CHECK screen (until its OK lines are up, ~800 frames); WARNING follows. */
static const Site boot_sites[] = {
	{ 0x0002CA, "4EB9000176CE", "4E714E714E71" },
};

static int hex(char c)
{
	return c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10;
}

static int apply(uint8_t *prg, const Site *s)
{
	const size_t n = strlen(s->old) / 2;
	for (size_t i = 0; i < n; i++)
		if (prg[s->addr + i] != (uint8_t)(hex(s->old[i * 2]) << 4 | hex(s->old[i * 2 + 1])))
			return -1;
	for (size_t i = 0; i < n; i++)
		prg[s->addr + i] = (uint8_t)(hex(s->new_[i * 2]) << 4 | hex(s->new_[i * 2 + 1]));
	return 0;
}

/* Between frames the 68000 waits for vblank in the main program's context, so the main loop can be entered from
 * there: 0x1AEA calls the attract's title (0x127F4) and goes round title / demo as after the intro. */
void progpatch_goto_title(void)
{
	br_set_reg(M68K_REG_SP, 0x210000);
	br_set_reg(M68K_REG_PC, 0x001AEA);
}

int progpatch_make(const uint8_t *orig, uint8_t *out, int chase)
{
	memcpy(out, orig, BR_PRG_SIZE);
	if (chase) {
		for (unsigned i = 0; i < sizeof chase_runs / sizeof chase_runs[0]; i++)
			memcpy(out + chase_runs[i].addr, chase_bytes + chase_runs[i].off, chase_runs[i].len);
		if ((uint32_t)crc32(0, out, BR_PRG_SIZE) != CHASE_CRC_AFTER)
			return -1;
	}
	for (unsigned i = 0; i < sizeof boot_sites / sizeof boot_sites[0]; i++)
		if (apply(out, &boot_sites[i]))
			return -2;
	return 0;
}
