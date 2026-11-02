# batrider360: GADGETS layout mock-up for the user (YOKO, 1920x1080): Baffo's v0.4 panels compressed, the drop counters
# F2-F7 as one Item Count row, and the new CHASE! panels built from Baffo's frames + Batrider's own text font.
#   gadget_mock.py <game frame .ppm> <glyphs .bin (harness 'glyphs')> <out.png>
import sys
from PIL import Image, ImageDraw

A = '/p/ref/gadgets_baffo04/0.4/'
frame, glyph_file, out = sys.argv[1:4]
GL = open(glyph_file, 'rb').read()


def img(f):
    return Image.open(A + f).convert('RGBA')


def scaled(im, s):
    return im.resize((round(im.width * s), round(im.height * s)), Image.LANCZOS)


TINTS = {  # bright end, dim end of the font's gradient (pixel values 2..8), Baffo's title colours
    'white': ((255, 255, 255), (150, 150, 160)),
    'red': ((255, 120, 120), (200, 0, 0)),
    'yellow': ((255, 255, 170), (230, 190, 0)),
    'amber': ((255, 210, 120), (230, 120, 0)),
    'blue': ((170, 220, 255), (40, 110, 255)),
    'green': ((180, 255, 180), (0, 170, 0)),
}


def text(s, scale, tint='white'):
    """Batrider's 8x8 text font (tile = ASCII), its shades 2..8 as a gradient, 9 = its shadow.
    The glyphs lie on their side in the board's own orientation (ROT270): turned upright here."""
    hi, lo = TINTS[tint]
    im = Image.new('RGBA', (8 * len(s) * scale, 8 * scale), (0, 0, 0, 0))
    px = im.load()
    for i, ch in enumerate(s):
        g = GL[ord(ch) * 64: ord(ch) * 64 + 64]
        for y in range(8):
            for x in range(8):
                v = g[x * 8 + (7 - y)]           # upright (x, y) = board (7 - y, x): a quarter turn anticlockwise
                if not v:
                    continue
                if v >= 9:
                    c = (30, 30, 30, 255)
                else:
                    t = (v - 2) / 6
                    c = tuple(round(hi[k] + (lo[k] - hi[k]) * t) for k in range(3)) + (255,)
                for yy in range(scale):
                    for xx in range(scale):
                        px[(i * 8 + x) * scale + xx, y * scale + yy] = c
    return im


def blank(im, box):
    """black out a title box's lettering (inside its frame)"""
    d = ImageDraw.Draw(im)
    d.rectangle(box, fill=(0, 0, 0, 255))
    return im


def centre(dst, im, box):
    x0, y0, x1, y1 = box
    dst.alpha_composite(im, (x0 + (x1 - x0 - im.width) // 2, y0 + (y1 - y0 - im.height) // 2))


def digits(dst, value, x, y, h, right=True):
    """Baffo's digit images (50x42), height h, right-aligned at x when right"""
    s = str(value)
    w = round(50 * h / 42)
    ims = [img(c + '.png').resize((w, h), Image.LANCZOS) for c in s]
    if right:
        x -= w * len(s) * 0.62
    for i, im in enumerate(ims):
        dst.alpha_composite(im, (round(x + i * w * 0.62), y))


# ---- the panels at Baffo's own size ----

def item_count():
    """F2..F7 in one row, Item Drops' frame: each slot the item's icon and its count (red at the cap)"""
    p = img('next_drop.png')
    blank(p, (124, 50, 392, 84))
    centre(p, text('Item Count', 3, 'white'), (118, 44, 398, 90))
    slots = [(28, 121), (109, 121), (190, 121), (271, 121), (352, 121), (433, 121)]
    src = [('powerup_drops.png', 2, 3), ('big_powerup_drops.png', 1, 3), ('option_drops.png', 3, 3),
           ('bomb_drops.png', 4, 5), ('big_bomb_drops.png', 0, 3), ('1up_drops.png', 0, 3)]
    for (sx, sy), (f, n, cap) in zip(slots, src):
        icon = img(f).crop((14, 22, 104, 112)).resize((40, 40), Image.LANCZOS)
        p.alpha_composite(icon, (sx + 2, sy + 2))
        d = text(str(n), 2, 'red' if n == cap else 'white')
        p.alpha_composite(d, (sx + 62 - d.width - 4, sy + 62 - d.height - 4))
    return p


def chain_panel(timer=0.62, chain=57):
    """Rank Bar's frame: the bar = the chain timer (blue, red near the end, white resting), the box = the chain"""
    p = img('rank_bar.png')
    blank(p, (112, 32, 400, 70))
    centre(p, text('Chain', 3, 'blue'), (108, 28, 404, 76))
    d = ImageDraw.Draw(p)
    d.rectangle((17, 104, 17 + round(490 * timer), 133), fill=(40, 110, 255, 255))
    blank(p, (170, 140, 345, 168))
    t = text('%5d' % chain, 3, 'white')
    centre(p, t, (165, 138, 350, 170))
    return p


def mult_panel(mult=4, ready=True, forward=True):
    """Medal Lv.'s frame: x1..x8, the amber C when a Chase Break is ready, the forward mark"""
    p = img('next_medal.png')
    blank(p, (14, 14, 186, 50))
    centre(p, text('Multiplier', 2, 'amber'), (10, 10, 190, 54))
    box = (60, 84, 140, 160)
    centre(p, text('x%d' % mult, 4, 'amber' if mult == 8 else 'white'), box)
    if ready:
        p.alpha_composite(text('C', 3, 'amber'), (150, 80))
    arrow(p, 158, 136, forward)
    return p


def arrow(p, x, y, forward):
    """the forward mark: an amber arrow up while flying in the upper half, a blue one down in the lower half"""
    d = ImageDraw.Draw(p)
    c, e = ((255, 190, 40, 255), (120, 60, 0, 255)) if forward else ((60, 140, 255, 255), (0, 30, 120, 255))
    pts = [(x, y - 13), (x + 12, y + 1), (x + 5, y + 1), (x + 5, y + 13), (x - 5, y + 13), (x - 5, y + 1), (x - 12, y + 1)]
    if not forward:
        pts = [(px, 2 * y - py) for px, py in pts]
    d.polygon(pts, fill=c, outline=e)


def lives_panel(lives=4):
    """a drop counter's frame: the 1UP icon and the lives (riders flying + banked extends), red at one"""
    p = img('1up_drops.png')
    blank(p, (124, 30, 202, 104))
    centre(p, text(str(lives), 5, 'red' if lives == 1 else 'white'), (120, 26, 206, 108))
    return p


def extend_panel(left=412300):
    """the rank bar's % box: points to the next extend"""
    p = img('rank_bar.png').crop((158, 136, 356, 177))
    blank(p, (8, 6, 190, 34))
    t = text('%7d' % left, 2, 'white')
    centre(p, t, (8, 4, 190, 36))
    lab = text('Next Extend', 2, 'green')
    q = Image.new('RGBA', (max(p.width, lab.width), p.height + lab.height + 4), (0, 0, 0, 0))
    q.alpha_composite(lab, (0, 0))
    q.alpha_composite(p, (0, lab.height + 4))
    return q


GRID = (28, 28, 28)


def list_frame(title, tint, n, width=246, pitch=39):
    """Secret bosses' frame, any width, n rows of the given pitch: the title drawn, the body blank with its grid"""
    src = img('secret_conditions.png')
    h = 112 + n * pitch + 33
    col = Image.new('RGBA', (246, h), (0, 0, 0, 0))
    for y in range(112, h - 33, 39):
        col.alpha_composite(src.crop((0, 112, 246, 151)), (0, y))
    col.alpha_composite(src.crop((0, 0, 246, 112)), (0, 0))
    col.alpha_composite(src.crop((0, 580, 246, 613)), (0, h - 33))
    p = Image.new('RGBA', (width, h), (0, 0, 0, 0))
    p.alpha_composite(col.crop((0, 0, 110, h)), (0, 0))
    for x in range(110, width - 110, 26):
        p.alpha_composite(col.crop((110, 0, 136, h)), (x, 0))
    p.alpha_composite(col.crop((136, 0, 246, h)), (width - 110, 0))
    d = ImageDraw.Draw(p)
    d.rectangle((12, 12, width - 12, 66), fill=(0, 0, 0, 255))
    centre(p, text(title, 3, tint), (12, 12, width - 12, 66))
    d.rectangle((22, 108, width - 22, h - 26), fill=(0, 0, 0, 255))
    for x in range(20, width - 20, 41):
        d.rectangle((x, 108, x + 1, h - 26), fill=GRID)
    for y in range(147, h - 26, 41):
        d.rectangle((22, y, width - 22, y + 1), fill=GRID)
    return p, d


def list_panel(title, tint, rows, width=246):
    """rows of (label, value[, tint]) or (label, None/True/False) for a checkbox (Baffo's square, checked.png when True)"""
    p, d = list_frame(title, tint, len(rows), width)
    for i, r in enumerate(rows):
        y = 128 + i * 39
        p.alpha_composite(text(r[0], 2, r[2] if len(r) > 2 else 'white'), (26, y))
        v = r[1]
        if isinstance(v, bool) or v is None:
            bx = width - 53
            d.rectangle((bx, y - 6, bx + 27, y + 21), outline=(148, 148, 148, 255), width=2)
            if v:
                p.alpha_composite(img('checked.png'), (bx + 3, y - 8))
        else:
            t = text(str(v), 2, r[2] if len(r) > 2 else 'white')
            p.alpha_composite(t, (width - 28 - t.width, y))
    return p


def stages_panel(rows, width=262, pitch=80):
    """ibara's STAGES: each stage's name, then the score made in it (the stage being played in yellow), the total"""
    p, d = list_frame('Stages', 'red', len(rows), width, pitch)
    for i, (name, score, tint) in enumerate(rows):
        y = 122 + i * pitch
        p.alpha_composite(text(name, 2, tint), (28, y))
        t = text(score, 3, 'yellow' if tint == 'yellow' else 'white')
        p.alpha_composite(t, (width - 28 - t.width, y + 24))
    return p


# ---- the view ----
game = Image.open(frame).convert('RGB').transpose(Image.ROTATE_90)
view = img('background_yoko.png')
view.paste(game.resize((810, 1080), Image.NEAREST), (555, 0))


def put(im, s, x, y, tag):
    im = scaled(im, s)
    view.alpha_composite(im, (x, y))
    return (x, y, x + im.width, y + im.height, tag)


boxes = []
R = 1384
# right: rank, the next drops, the counts, medals, then STATUS + WEAPON in the CHASE! panels' old place
boxes.append(put(img('rank_bar.png'), 0.82, R + 40, 0, 'A rank'))
boxes.append(put(img('next_drop.png'), 0.82, R + 42, 130, 'B drops'))
boxes.append(put(item_count(), 0.82, R + 42, 315, 'F2-F7 count'))
boxes.append(put(img('medal_drops.png'), 0.82, R + 52, 510, 'F1 medals'))
boxes.append(put(img('next_medal.png'), 0.82, R + 270, 492, 'D medal lv'))
boxes.append(put(list_panel('Status', 'yellow', [
    ('Stage', 'M.CITY'), ('Time', '1:52'), ('Enemies', 12), ('Bullets', 87)], 317), 0.82, R + 6, 690, 'STATUS'))
boxes.append(put(list_panel('Weapon', 'green', [
    ('Shot', 'LV 3', 'yellow'), ('Options', 2), ('Formation', 'WIDE'), ('Bombs', 3),
    ('Sp.Power', True, 'red'), ('Sp.Option', False, 'red')], 317), 0.82, R + 268, 690, 'WEAPON'))
# left: player + input, secret bosses + boss info + shot charge, then CHASE!
boxes.append(put(img('profile.png'), 0.72, 14, 10, 'G player'))
boxes.append(put(img('input_display.png'), 0.62, 370, 30, 'H input'))
boxes.append(put(img('secret_conditions.png'), 0.72, 14, 284, 'E secret'))
boxes.append(put(img('boss_info.png'), 0.72, 200, 284, 'C boss'))
boxes.append(put(img('shot_charge.png'), 0.72, 372, 285, 'I charge'))
boxes.append(put(stages_panel([
    ('M.CITY', '1053200', 'white'), ('AIRPORT', '2210450', 'white'), ('SKY HIGH', '987600', 'yellow'),
    ('S.SYSTEM', '-', 'green'), ('HIGH WAY', '-', 'green'), ('Z.CITY', '-', 'green'), ('BUILDING', '-', 'green'),
    ('TOTAL', '4251250', 'amber')]), 0.57, 393, 140, 'STAGES'))
boxes.append(put(chain_panel(), 0.82, 40, 735, 'CHASE chain'))
boxes.append(put(mult_panel(), 0.82, 40, 880, 'CHASE mult'))
boxes.append(put(lives_panel(), 0.82, 215, 880, 'CHASE lives'))
boxes.append(put(extend_panel(), 0.9, 215, 985, 'CHASE extend'))

view.convert('RGB').save(out)
lab = view.copy()
d = ImageDraw.Draw(lab)
for x0, y0, x1, y1, tag in boxes:
    d.rectangle((x0, y0, x1 - 1, y1 - 1), outline=(255, 0, 255, 255), width=2)
lab.convert('RGB').save(out.replace('.png', '_boxes.png'))
