/* batrider360: YM2151 (ymfm) for C. */
#ifndef BR_YM_H
#define BR_YM_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void ym_reset(void);                   /* both chips */
void ym_write(int port, uint8_t data); /* chip 0: the board's */
uint8_t ym_read(int port);
void ym_generate(int32_t *dst, int n);
void ym_tick(int n);                   /* chip 0's timers over n samples, without making them (split board) */
void ym2_write(int port, uint8_t data); /* chip 1: the sound thread's copy */
void ym2_generate(int32_t *dst, int n);
#ifdef __cplusplus
}
#endif
#endif
