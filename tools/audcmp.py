# per-frame loudness + spectrum of my audio.wav (800 samples/frame) vs MAME's wav (48 kHz, 59.637 frames/s)
import sys, wave, numpy as np
def load(p):
    w = wave.open(p); n = w.getnframes(); ch = w.getnchannels()
    a = np.frombuffer(w.readframes(n), dtype='<i2').astype(np.float32)
    return a.reshape(-1, ch).mean(axis=1), w.getframerate()
mine, _ = load(sys.argv[1]); mame, sr = load(sys.argv[2]); off = int(sys.argv[3])
per_mame = sr / (27e6 / 4 / 432 / 262)
def frame(a, i, per):
    s = int(i * per); return a[s:s + int(per)]
rows = []
for f in range(900, 2900, 100):
    a = frame(mine, f - 1 - off, 800); b = frame(mame, f, per_mame)
    if len(a) < 700 or len(b) < 700: continue
    ra, rb = np.sqrt((a ** 2).mean()), np.sqrt((b ** 2).mean())
    print('frame %5d  rms mine %7.0f  mame %7.0f' % (f, ra, rb))
# whole-run spectra over the aligned stretch (frames 900..2900)
A = mine[int((900 - 1 - off) * 800):int((2900 - 1 - off) * 800)]
B = mame[int(900 * per_mame):int(2900 * per_mame)]
def spec(x):
    n = 1 << 14; segs = [np.abs(np.fft.rfft(x[i:i + n] * np.hanning(n))) for i in range(0, len(x) - n, n)]
    return np.mean(segs, axis=0)
sa, sb = spec(A), spec(B)
bands = [0, 100, 250, 500, 1000, 2000, 4000, 8000, 16000, 24000]
fq = np.fft.rfftfreq(1 << 14, 1 / 48000)
for lo, hi in zip(bands, bands[1:]):
    m = (fq >= lo) & (fq < hi)
    print('%5d-%5d Hz  mine/mame energy %.2f' % (lo, hi, (sa[m] ** 2).sum() / max((sb[m] ** 2).sum(), 1e-9)))
tot_a, tot_b = (sa ** 2).sum(), (sb ** 2).sum()
for lo, hi in ((8000, 16000), (16000, 24000)):
    m = (fq >= lo) & (fq < hi)
    print('share of total energy %5d-%5d Hz: mine %.4f%%  mame %.4f%%' % (lo, hi, 100 * (sa[m] ** 2).sum() / tot_a, 100 * (sb[m] ** 2).sum() / tot_b))
