/* batrider360: the ORIGINAL and CHASE! programs from batriderj's. */
#ifndef BR_PROGPATCH_H
#define BR_PROGPATCH_H
#include <stdint.h>
#include "board.h"

/* out (BR_PRG_SIZE) = orig with the boot patch, and CHASE!'s when chase is set. 0 = done; -1 = CHASE!'s result
 * failed its CRC; -2 = the boot site held other bytes. */
int progpatch_make(const uint8_t *orig, uint8_t *out, int chase);
/* From the boot's WARNING, logo or intro straight to the attract's title (call between frames). */
void progpatch_goto_title(void);
#endif
