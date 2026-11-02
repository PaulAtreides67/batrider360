# compare harness output (f/r<N-1>) with MAME's harness (snap/batriderj/NNNN.png in frame order, ram_<N>.bin)
import sys, glob, os
from PIL import Image, ImageChops
mine, mame, frames = sys.argv[1], sys.argv[2], [int(x) for x in sys.argv[3].split(',')]
off = int(sys.argv[4]) if len(sys.argv) > 4 else 0   # my frame N - off matches MAME's N
snaps = sorted(glob.glob(mame + '/snap/batriderj/*.png'))
rows = []
for i, f in enumerate(frames):
    a = Image.open('%s/f%05d.ppm' % (mine, f - 1 - off)).convert('RGB')
    b = Image.open(snaps[i]).convert('RGB')
    if b.size != a.size:
        b = b.transpose(Image.ROTATE_270) if b.size == (a.height, a.width) else b.resize(a.size)
    diff = ImageChops.difference(a, b)
    npx = sum(1 for p in diff.getdata() if max(p) > 8)
    ra = open('%s/r%05d.bin' % (mine, f - 1 - off), 'rb').read(); rb = open('%s/ram_%06d.bin' % (mame, f), 'rb').read()
    nb = sum(1 for x, y in zip(ra, rb) if x != y)
    print('frame %5d  pixels differing %6d  RAM bytes differing %5d' % (f, npx, nb))
    rows.append((a, b, diff.point(lambda v: 255 if v > 8 else 0)))
W, H = rows[0][0].size
sheet = Image.new('RGB', (len(rows) * (H + 4), 3 * (W + 4)), (60, 0, 60))
for i, (a, b, d) in enumerate(rows):
    for j, im in enumerate((a, b, d)):
        sheet.paste(im.transpose(Image.ROTATE_90), (i * (H + 4), j * (W + 4)))
sheet.save(mine + '/cmp.png')
