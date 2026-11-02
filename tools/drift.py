# for each of my RAM dumps, the MAME frame whose RAM is closest (in 0x200000-0x20FFFF minus the text/palette area)
import sys, glob, re
mine, mame = sys.argv[1], sys.argv[2]
M = {int(re.search(r'(\d+)\.bin', f).group(1)): open(f, 'rb').read() for f in glob.glob(mame + '/ram_*.bin')}
for f in sorted(glob.glob(mine + '/r*.bin')):
    n = int(re.search(r'r(\d+)\.bin', f).group(1)) + 1
    if n % 50: continue
    a = open(f, 'rb').read()
    best = min(M, key=lambda k: sum(1 for x, y in zip(a[0x4000:], M[k][0x4000:]) if x != y))
    d = sum(1 for x, y in zip(a[0x4000:], M[best][0x4000:]) if x != y)
    print('mine %5d ~ mame %5d (offset %+d, %d bytes differ)' % (n, best, best - n, d))
