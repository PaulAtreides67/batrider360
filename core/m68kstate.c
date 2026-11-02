/* batrider360: the 68000's part of a save state (test 8). Musashi's whole context (m68k_get_context) ends in pointers
 * to its cycle tables and the host's callbacks, which differ between builds; everything before them is plain data
 * (registers, flags, interrupt level, stopped, prefetch, the 68000's masks and cycle counts), so only that is kept and
 * the live context's pointers stay. Taken between frames (m68k_execute not running). */
#include <stddef.h>

#include "m68kcpu.h"
#include "stio.h"

void m68k_state_io(Stio *s)
{
	stio(s, &m68ki_cpu, offsetof(m68ki_cpu_core, cyc_instruction));
}

unsigned m68k_state_size(void)
{
	return (unsigned)offsetof(m68ki_cpu_core, cyc_instruction);
}
