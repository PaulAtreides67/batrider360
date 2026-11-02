// batrider360: the YM2151 through ymfm (third_party/ymfm, BSD-3), with its two timers counted in chip clocks
// (the Z80 program may poll the timer flags in the status byte; no IRQ line is wired on this board).
// Two chips: chip 0 is the board's (writes, status reads, timers; it also makes the sound unless the board runs split),
// chip 1 makes the sound on another thread from chip 0's writes replayed at the same sample positions (ym2_*).
#include "ym.h"
#include "stio.h"
#include "ymfm_opm.h"

#include <vector>

namespace {

class Iface : public ymfm::ymfm_interface {
public:
	int32_t left[2] = {-1, -1};        // clocks until each timer expires, -1 = stopped

	void ymfm_set_timer(uint32_t tnum, int32_t duration_in_clocks) override
	{
		left[tnum] = duration_in_clocks;
	}

	void advance(int32_t clocks)
	{
		for (int t = 0; t < 2; t++) {
			if (left[t] < 0)
				continue;
			left[t] -= clocks;
			if (left[t] < 0) {
				int32_t over = -left[t];
				left[t] = -1;
				m_engine->engine_timer_expired(t);   // re-arms through ymfm_set_timer
				if (left[t] >= 0)
					left[t] = left[t] > over ? left[t] - over : 0;
			}
		}
	}
};

struct Chip {
	Iface iface;
	ymfm::ym2151 chip{iface};
	ymfm::ym2151::output_data out;

	void reset()
	{
		iface.left[0] = iface.left[1] = -1;
		chip.reset();
	}

	// n samples (one per 64 chip clocks), L+R summed, at the board's 0.25 per output: 16-bit range.
	void generate(int32_t *dst, int n)
	{
		for (int i = 0; i < n; i++) {
			chip.generate(&out, 1);
			dst[i] = (out.data[0] + out.data[1]) / 4;
			iface.advance(64);
		}
	}
};

Chip chips[2];

}

extern "C" void ym_reset(void)
{
	chips[0].reset();
	chips[1].reset();
}

extern "C" void ym_write(int port, uint8_t data)
{
	chips[0].chip.write(port & 1, data);
}

extern "C" uint8_t ym_read(int port)
{
	return chips[0].chip.read(port & 1);
}

extern "C" void ym_generate(int32_t *dst, int n)
{
	chips[0].generate(dst, n);
}

extern "C" void ym_tick(int n)
{
	for (int i = 0; i < n; i++) {
		chips[0].chip.skip_clocks(1);      // as generate() counts them (timer B's load phase)
		chips[0].iface.advance(64);
	}
}

extern "C" void ym2_write(int port, uint8_t data)
{
	chips[1].chip.write(port & 1, data);
}

extern "C" void ym2_generate(int32_t *dst, int n)
{
	chips[1].generate(dst, n);
}

// Test 8, save states: each chip as ymfm saves it (its own byte order), with its length, then its two timers' clocks.
extern "C" void ym_state_io(Stio *s)
{
	for (Chip &c : chips) {
		std::vector<uint8_t> b;
		uint32_t n = 0;
		if (!s->load) {
			ymfm::ymfm_saved_state st(b, true);
			c.chip.save_restore(st);
			n = (uint32_t)b.size();
		}
		STIO(s, n);
		if (s->err || n > s->cap - s->at) {
			s->err = 1;
			return;
		}
		b.resize(n);
		stio(s, b.data(), n);
		if (s->load) {
			ymfm::ymfm_saved_state st(b, false);
			c.chip.save_restore(st);
		}
		STIO(s, c.iface.left);
	}
}
