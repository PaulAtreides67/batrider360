# Baffo's Batrider v0.4 layout composed as MAME draws it (YOKO + TATE views of default.lay), labelled, with a game frame.
import sys
from PIL import Image, ImageDraw, ImageFont
A = '/p/ref/gadgets_baffo04/0.4/'
game = Image.open(sys.argv[1]).convert('RGB').transpose(Image.ROTATE_90)   # ROT270: upright 240x320
out = sys.argv[2]
font = ImageFont.load_default(size=22)
small = ImageFont.load_default(size=17)
# name, image, yoko bounds (x,y,w,h), tate bounds (x,y,w,h)
G = [
 ('A rank bar', 'rank_bar.png', (1386,0,524,177), (1805,11,115,341)),
 ('B next drops', 'next_drop.png', (1384,144,520,221), (1682,10,144,338)),
 ('C boss HP/timer', 'boss_info.png', (266,593,228,447), (0,550,224,114)),
 ('D medal level', 'next_medal.png', (1412,840,199,216), (1760,750,162,150)),
 ('E secret bosses', 'secret_conditions.png', (15,427,246,613), (0,427,233,93.5)),
 ('F1 medal drops', 'medal_drops.png', (1395,385,226,134), (1700,385,53.6,90)),
 ('F2 big P drops', 'big_powerup_drops.png', (1395,535,226,134), (1700,670,53.6,90)),
 ('F3 big B drops', 'big_bomb_drops.png', (1395,682,226,134), (1700,860,53.6,90)),
 ('F4 option drops', 'option_drops.png', (1664,385,226,134), (1700,480,53.6,90)),
 ('F5 P drops', 'powerup_drops.png', (1664,535,226,134), (1700,575,53.6,90)),
 ('F6 bomb drops', 'bomb_drops.png', (1664,682,226,134), (1700,765,53.6,90)),
 ('F7 1UP drops', '1up_drops.png', (1664,832,226,134), (1700,955,53.6,90)),
 ('G current player', 'profile.png', (32,48,482,364), (1770,500,140,185)),
 ('H input P1/P2', 'input_display.png', (266,427,278,160), (70,40,164,304)),
 ('I shot charge', 'shot_charge.png', (510,594,27,446), None),
]
def view(tate):
    bg = Image.open(A + ('background_tate.png' if tate else 'background_yoko.png')).convert('RGBA')
    if tate:
        bg = bg.transpose(Image.ROTATE_270).resize((1920, 1080))
        g = game.transpose(Image.ROTATE_270).resize((1440, 1080), Image.NEAREST)
        bg.paste(g, (240, 0))
    else:
        bg.paste(game.resize((810, 1080), Image.NEAREST), (555, 0))
    d = ImageDraw.Draw(bg)
    for name, f, yb, tb in G:
        b = tb if tate else yb
        if b is None:
            if not tate: continue
            b = (0, 0, 25, 400); im = Image.open(A + f).convert('RGBA').resize((25, 400))
        else:
            im = Image.open(A + f).convert('RGBA')
            if tate: im = im.transpose(Image.ROTATE_270)
            im = im.resize((max(1, round(b[2])), max(1, round(b[3]))))
        bg.alpha_composite(im, (round(b[0]), round(b[1])))
        x, y = round(b[0]), round(b[1])
        d.rectangle([x, y, x + round(b[2]) - 1, y + round(b[3]) - 1], outline=(255, 0, 255), width=3)
        tw = d.textlength(name.split()[0], font=font)
        d.rectangle([x + 4, y + 4, x + 10 + tw, y + 30], fill=(255, 0, 255))
        d.text((x + 7, y + 5), name.split()[0], fill=(255, 255, 255), font=font)
    return bg.convert('RGB')
y, t = view(False), view(True)
sheet = Image.new('RGB', (1920, 2160 + 60), (30, 30, 30))
sheet.paste(y, (0, 0)); sheet.paste(t, (0, 1080 + 60))
d = ImageDraw.Draw(sheet)
legend = '   '.join(n for n, *_ in G)
d.text((10, 1090), 'TOP: YOKO view   BOTTOM: TATE view (rotated as MAME shows it)   ' , fill=(255, 255, 0), font=font)
d.text((10, 1115), legend, fill=(255, 255, 255), font=small)
sheet.save(out)
y.save(out.replace('.png', '_yoko.png')); t.save(out.replace('.png', '_tate.png'))
