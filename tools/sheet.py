# contact sheet of harness/MAME frames: sheet.py <out.png> <cols> <files...> (PPM/PNG, shown upright: ROT270)
import sys
from PIL import Image, ImageDraw
out, cols, files = sys.argv[1], int(sys.argv[2]), sys.argv[3:]
ims = []
for f in files:
    im = Image.open(f).convert('RGB')
    if im.width > im.height:
        im = im.transpose(Image.ROTATE_90)
    ims.append((f.split('/')[-1], im))
w, h = ims[0][1].size
rows = (len(ims) + cols - 1) // cols
sheet = Image.new('RGB', (cols * (w + 4), rows * (h + 16)), (40, 40, 40))
d = ImageDraw.Draw(sheet)
for i, (n, im) in enumerate(ims):
    x, y = (i % cols) * (w + 4), (i // cols) * (h + 16)
    sheet.paste(im, (x, y + 14))
    d.text((x + 2, y + 1), n, fill=(255, 255, 0))
sheet.save(out)
