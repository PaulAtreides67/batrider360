/* batrider360: batriderj's ROMs out of a zip held in memory. */
#ifndef BR_ROMSET_H
#define BR_ROMSET_H
#include <stddef.h>
#include <stdint.h>
#include "board.h"

/* 0 and *out filled (buffers kept for the program's life), or -1 with a message in err. */
int romset_load(const uint8_t *zip, size_t zlen, BrRoms *out, char *err, size_t errlen);
/* Test 14 (faster boot): runs fn(0, arg) .. fn(n - 1, arg), each once, and returns when all are done (the 360 spreads
 * them over its hardware threads); none set = one after another. */
typedef void (*RomsetRunner)(void (*fn)(int, void *), void *arg, int n);
void romset_set_runner(RomsetRunner run);
#endif
