/* batrider360: OKIM6295 with Raizing's ROM banking. */
#ifndef BR_OKI_H
#define BR_OKI_H
#include <stdint.h>

typedef struct {
	int playing;
	uint32_t base, sample, count;
	int signal, step, volume;
} OkiVoice;

typedef struct {
	const uint8_t *rom;                /* 1 MB */
	uint8_t page[4];                   /* GAL banks: page of the phrase table quarter / 64 KB window n */
	int command;
	OkiVoice voice[4];
} Oki;

void oki_init(Oki *o, const uint8_t *rom);
void oki_reset(Oki *o);
uint8_t oki_read(const Oki *o);
void oki_write(Oki *o, uint8_t command);
void oki_generate(Oki *o, int32_t *dst, int n);
#endif
