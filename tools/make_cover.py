"""Xbox 360 cover wrap for batrider360 from the CHASE! kit's N64 box art.

Inputs: the user's 900x611 Xbox 360 cover template (transparent where art goes),
batrider-chase/src/n64/box-art/panels (official key art, cast banner, screenshots, logos),
batrider360/ref/n64_manual_art/logo*.png.  No drawn illustration: only crops of those.
Output: docs/cover/batrider360_cover_wrap.png (4x template) + _front.jpg.
"""
import sys
from PIL import Image, ImageDraw, ImageFont, ImageFilter

S = 4
ROOT = r"C:\New folder"
BOX = ROOT + r"\batrider-chase\src\n64\box-art\panels"
ART = ROOT + r"\batrider360\ref\n64_manual_art"
OUT = ROOT + r"\batrider360\docs\cover"
template_path = sys.argv[1]

NAVY = (11, 16, 46)
RED = (214, 32, 44)
GOLD = (245, 180, 40)
FONT = r"C:\Windows\Fonts\bahnschrift.ttf"


def font(px, weight="Bold"):
    f = ImageFont.truetype(FONT, px)
    try:
        f.set_variation_by_name(weight)
    except Exception:
        pass
    return f


def spaced(d, xy, text, f, fill, track):
    x, y = xy
    for ch in text:
        d.text((x, y), ch, font=f, fill=fill)
        x += d.textlength(ch, font=f) + track
    return x


tpl = Image.open(template_path).convert("RGBA")
W, H = tpl.size[0] * S, tpl.size[1] * S
tpl = tpl.resize((W, H), Image.LANCZOS)
canvas = Image.new("RGBA", (W, H), NAVY + (255,))

front = Image.open(BOX + r"\box_front.png").convert("RGB")
back = Image.open(BOX + r"\box_back.png").convert("RGB")
bk = front.size[0] / 1400.0  # the box previews are 1400 wide

# ---- front: key art, cropped clear of the N64 badge, full panel height
FX0, FX1 = 474 * S, W
fw, fh = FX1 - FX0, H
cx0 = int(605 * bk)  # right of the box's own logo (no leftovers of it)
cx1 = int(1195 * bk)  # left of the N64 badge
ch = int((cx1 - cx0) * fh / fw)
key = front.crop((cx0, 0, cx1, ch)).resize((fw, fh), Image.LANCZOS)
canvas.paste(key, (FX0, 0))

# logo over the box logo's leftovers, below the green band
logo = Image.open(ART + r"\logo.png").convert("RGB")
la = Image.open(ART + r"\logo_alpha.png").convert("L")
logo.putalpha(la)
lw = int(fw * 0.80)
logo_f = logo.resize((lw, int(logo.size[1] * lw / logo.size[0])), Image.LANCZOS)
shadow = Image.new("RGBA", logo_f.size, (0, 0, 0, 0))
shadow.putalpha(logo_f.getchannel("A").point(lambda v: v * 0.7))
shadow = shadow.filter(ImageFilter.GaussianBlur(14))
lx, ly = FX0 + int(fw * 0.04), 84 * S
canvas.alpha_composite(shadow, (lx + 10, ly + 14))
canvas.alpha_composite(logo_f, (lx, ly))

# tagline, as on the N64 box front, on a dark fade so it reads over the art
fade_h = 120 * S
fade = Image.new("RGBA", (fw, fade_h), NAVY + (0,))
fade.putalpha(Image.linear_gradient("L").resize((fw, fade_h)).point(lambda v: int(v * 0.85)))
canvas.alpha_composite(fade, (FX0, H - fade_h))
d = ImageDraw.Draw(canvas)
tf = font(int(13 * S))
ty = H - 64 * S
spaced(d, (FX0 + 18 * S, ty), "THE 1998 ARCADE GAME", tf, (255, 255, 255), 3 * S)
spaced(d, (FX0 + 18 * S, ty + 18 * S), "+ CHASE! ARRANGE", tf, GOLD, 3 * S)

# ---- spine (template spine is white below its XBOX 360 strip)
SX0, SX1, SY0 = 427 * S, 474 * S, 166 * S
d.rectangle((SX0, SY0, SX1 - 1, H), fill=NAVY)
d.rectangle((SX0, SY0, SX1 - 1, SY0 + 3 * S), fill=RED)
sw = SX1 - SX0
rl = logo.rotate(90, expand=True)
rh = int(sw * 0.92 * rl.size[1] / rl.size[0])
rl = rl.resize((int(sw * 0.92), rh), Image.LANCZOS)
canvas.alpha_composite(rl, (SX0 + (sw - rl.size[0]) // 2, H - 14 * S - rh))
sf = font(int(13 * S))
strip = Image.new("RGBA", (H, sw), (0, 0, 0, 0))
sd = ImageDraw.Draw(strip)
x = spaced(sd, (0, 0), "ARMED POLICE BATRIDER ", sf, (255, 255, 255), 3 * S)
x = spaced(sd, (x, 0), "CHASE!", sf, GOLD, 3 * S)
strip = strip.crop((0, 0, int(x), sw))
bb = strip.getbbox()
strip = strip.crop((0, bb[1], int(x), bb[3])).rotate(90, expand=True)
canvas.alpha_composite(strip, (SX0 + (sw - strip.size[0]) // 2,
                               H - 14 * S - rh - 22 * S - strip.size[1]))

# ---- back
BW = 426 * S
BOT = 458 * S  # template badges/legal start at 466
d.rectangle((0, 0, BW - 1, BOT), fill=NAVY)
d.rectangle((0, BOT, BW - 1, H), fill=(255, 255, 255))
M = 14 * S

# cast banner with "Chase the chain."
ban = back.crop((int(22 * bk), int(22 * bk), int(1378 * bk), int(322 * bk)))
bh = 96 * S
bw = BW - 2 * M
sc = bh / ban.size[1]
bwide = int(ban.size[0] * sc)
ban = ban.resize((bwide, bh), Image.LANCZOS)
ban = ban.crop((0, 0, bw, bh))  # left part keeps the tagline box
canvas.paste(ban, (M, M))
d.rectangle((M, M + bh, M + bw - 1, M + bh + 3 * S), fill=RED)

y = M + bh + 14 * S
bf = font(int(9.6 * S), "Regular")
para = ("Sixteen kills a step, eight steps, a second and a half to find the next one. "
        "Fly forward and the timer is yours; hang back and it is not. And when the screen "
        "fills with bullets, one button turns every one of them to gold.")


def wrap(text, f, width):
    lines, cur = [], ""
    for w in text.split():
        t = (cur + " " + w).strip()
        if d.textlength(t, font=f) <= width:
            cur = t
        else:
            lines.append(cur)
            cur = w
    lines.append(cur)
    return lines


for ln in wrap(para, bf, bw):
    d.text((M, y), ln, font=bf, fill=(230, 232, 240))
    y += int(12.6 * S)
y += 4 * S
bullets = [  # the N64 box back's own five, then the 360 port's gadgets
    "Chain kills sixteen at a time, up to x8 — and keep moving forward to hold it.",
    "Medals fly to you. A Chase Break turns every bullet on the screen to gold.",
    "Nine Batriders and nine guest riders; teams of three.",
    "The original 1998 arcade game too, with its own settings and rankings.",
    "Practice any of seven stages and eighteen bosses.",
    "Gadgets beside the play field: rank, item and medal drops, boss timers and more.",
]
for b in bullets:
    lines = wrap(b, bf, bw - 12 * S)
    d.text((M + 2 * S, y), "\u2022", font=bf, fill=GOLD)
    for ln in lines:
        d.text((M + 12 * S, y), ln, font=bf, fill=(230, 232, 240))
        y += int(12.6 * S)
    y += 2 * S

# three screenshots from the N64 box back; they get the height left above the footer
y += 6 * S
lf = font(int(6.2 * S), "Regular")
legal = ("ARMED POLICE BATRIDER © 1998 RAIZING / 8ing CO., LTD. CHASE! arranged by COIN-OP GAME STUDIO. "
         "Xbox 360 version. Unofficial and non-commercial; not licensed or endorsed by the owners of the "
         "game or of the system. All trademarks are the property of their owners.")
legal_lines = wrap(legal, lf, bw)
rh2 = 30 * S
foot = rh2 + 5 * S + len(legal_lines) * int(8 * S)
shots = [(693, 896), (916, 1119), (1138, 1340)]
gap = 8 * S
sw3 = (bw - 2 * gap) // 3
shh = BOT - 8 * S - foot - 10 * S - y
for i, (x0, x1) in enumerate(shots):
    s = back.crop((int(x0 * bk) + 3, int(365 * bk) + 3, int(x1 * bk) - 3, int(635 * bk) - 3))
    full = int(s.size[1] * sw3 / s.size[0])
    s = s.resize((sw3, full), Image.LANCZOS)
    if full > shh:  # keep the top (score bar) and as much play field as fits
        s = s.crop((0, 0, sw3, shh))
    sx = M + i * (sw3 + gap)
    canvas.paste(s, (sx, y))
    d.rectangle((sx - 2, y - 2, sx + sw3 + 1, y + s.size[1] + 1), outline=(120, 130, 170), width=2)
y += min(shh, full) + 10 * S

# Coin-Op / Raizing / 8ing row from the box back
row = back.crop((int(80 * bk), int(840 * bk), int(505 * bk), int(942 * bk)))
row = row.resize((int(row.size[0] * rh2 / row.size[1]), rh2), Image.LANCZOS)
canvas.paste(row, (M, y))
ly2 = y + rh2 + 5 * S
for ln in legal_lines:
    d.text((M, ly2), ln, font=lf, fill=(170, 176, 200))
    ly2 += int(8 * S)

# the template's spine is plain white below its XBOX 360 strip: let the drawn spine show
tpl.paste((0, 0, 0, 0), (SX0, SY0, SX1, H))
canvas.alpha_composite(tpl)
out = canvas
import os
os.makedirs(OUT, exist_ok=True)
out.convert("RGB").save(OUT + r"\batrider360_cover_wrap.png")
out.convert("RGB").crop((FX0, 0, W, H)).save(OUT + r"\batrider360_cover_front.jpg", quality=93)
# Aurora wants the whole wrap, spine included, at 900x600
out.convert("RGB").resize((900, 600), Image.LANCZOS).save(OUT + r"\batrider360_cover_aurora.png")
print(W, H, "back text bottom", ly2 // S, "limit", BOT // S)
