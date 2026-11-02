"""GADGETS art for the 360: Baffo's Batrider MAME layout v0.4 (ref/gadgets_baffo04/0.4: design baf, Lua Olifante &
DUBS, rank start bar Miquel) laid out on the 360's 720p canvases, plus the new panels built from Baffo's own frames.

usage: python3 tools/mkgadgets.py <project dir>
       python3 tools/mkgadgets.py <project dir> --previews <yoko chase .ppm> <yoko original .ppm> <tate chase .ppm>
         (the PC harness's BR_GADGETS_PNG canvases -> work/gadgets_yoko_chase.png, gadgets_yoko_original.png,
          gadgets_tate_chase.png)

Writes build/gen/gadgets_art.h for core/gadgets_draw.c:
- per screen (YOKO 1280x720, TATE 720x1280, viewer-upright) the static canvas: Baffo's wallpaper, every panel frame
  (RGB, deflated), and the CHASE! panels as a separate RGBA layer (shown only in a CHASE! credit);
- the rectangles the live values go in (ga_rect), the sprites they use (pre-scaled, RGBA, deflated: Baffo's digits,
  portraits, items, medals, stick, button, checkbox, minus);
- the titles and row labels (ga_text): drawn by the 360 at run time with the board's own font, because the font is
  ROM data and must not be in the .xex (official-art rule: only Baffo's frames + the game's font/art).
Also work/gadgets_layout_<screen>.png (frames + rectangles outlined) for checking the placement.
"""
import sys
import zlib

from PIL import Image, ImageDraw

P = sys.argv[1]
A = P + '/ref/gadgets_baffo04/0.4/'


def img(f):
    return Image.open(A + f).convert('RGBA')


def scaled(im, s):
    return im.resize((max(1, round(im.width * s)), max(1, round(im.height * s))), Image.LANCZOS)


def blank(im, box):
    ImageDraw.Draw(im).rectangle(box, fill=(0, 0, 0, 255))
    return im


# ---------------------------------------------------------------- frames (native Baffo pixels)

GRID = (28, 28, 28, 255)


def list_frame(n, width, pitch=39):
    """Secret bosses' frame, any width, n rows: title box and body blank (titles and rows drawn by the 360)"""
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
    d.rectangle((22, 108, width - 22, h - 26), fill=(0, 0, 0, 255))
    for x in range(20, width - 20, 41):
        d.rectangle((x, 108, x + 1, h - 26), fill=GRID)
    for y in range(108 + pitch, h - 26, pitch):
        d.rectangle((22, y, width - 22, y + 1), fill=GRID)
    return p


COUNT_SLOTS = [(28 + 81 * i, 121) for i in range(6)]
COUNT_SRC = ['powerup_drops.png', 'big_powerup_drops.png', 'option_drops.png', 'bomb_drops.png',
             'big_bomb_drops.png', '1up_drops.png']


def count_frame():
    """Item Drops' frame for the Item Count row: title blank, each slot its drop counter's icon (from Baffo's
    counter panels) and room for the count"""
    p = img('next_drop.png')
    blank(p, (124, 50, 392, 84))
    for (sx, sy), f in zip(COUNT_SLOTS, COUNT_SRC):
        icon = img(f).crop((14, 22, 104, 112)).resize((40, 40), Image.LANCZOS)
        p.alpha_composite(icon, (sx + 2, sy + 2))
    return p


def chain_frame():
    p = img('rank_bar.png')
    blank(p, (112, 32, 400, 70))
    blank(p, (170, 140, 345, 168))
    return p


def mult_frame():
    p = img('next_medal.png')
    blank(p, (14, 14, 186, 50))
    return p


def lives_frame():
    p = img('1up_drops.png')
    blank(p, (124, 30, 202, 104))
    return p


def extend_box():
    p = img('rank_bar.png').crop((158, 136, 356, 177))
    blank(p, (8, 6, 190, 34))
    return p


def music_box(wn, hn):
    """the Music panel (test 6): extend_box's frame (Baffo's rank_bar box) stretched to wn x hn native pixels"""
    src = img('rank_bar.png').crop((158, 136, 356, 177))
    c = 10                                  # corner kept as is
    p = Image.new('RGBA', (wn, hn), (0, 0, 0, 0))
    cols = [(0, c, 0), (c, src.width - c, None), (src.width - c, src.width, wn - c)]
    rows = [(0, c, 0), (c, src.height - c, None), (src.height - c, src.height, hn - c)]
    for (x0, x1, dx) in cols:
        for (y0, y1, dy) in rows:
            piece = src.crop((x0, y0, x1, y1))
            w = (wn - 2 * c) if dx is None else x1 - x0
            h = (hn - 2 * c) if dy is None else y1 - y0
            p.alpha_composite(piece.resize((w, h), Image.NEAREST), (c if dx is None else dx, c if dy is None else dy))
    blank(p, (8, 6, wn - 9, hn - 7))
    return p


def music_panel(sc, x, y, w, h, lines):
    """the tune playing: the sound ROM's title and its composer, drawn by the 360 (gadgets_draw.c)"""
    m = sc.put(music_box(w, h), 1, x, y)
    if lines == 1:
        sc.rect('MUSIC_0', m(10, 4, w - 20, h - 8))
    else:
        ih = (h - 12) // 2
        sc.rect('MUSIC_0', m(10, 6, w - 20, ih))
        sc.rect('MUSIC_1', m(10, 6 + ih, w - 20, ih))
    return m


MEDAL_EXTRA = 250


def medal_plate():
    """One medal gadget (as ibara360's): Medal Lv.'s title over Medal drops' frame widened. Left: the F1 icon and
    its count box (Baffo's); right: a second box for the next medal's picture, then its value."""
    md = img('medal_drops.png')
    w = md.width + MEDAL_EXTRA
    body = Image.new('RGBA', (w, md.height), (0, 0, 0, 0))
    body.alpha_composite(md.crop((0, 0, 206, md.height)), (0, 0))
    for x in range(206, w - 20, 8):
        body.alpha_composite(md.crop((198, 0, 206, md.height)), (x, 0))
    body.alpha_composite(md.crop((206, 0, md.width, md.height)), (w - 20, 0))
    blank(body, (204, 22, w - 24, 112))
    box = md.crop((116, 16, 208, 108))
    body.alpha_composite(box, (214, 16))
    title = img('next_medal.png').crop((0, 0, 199, 62))
    p = Image.new('RGBA', (w, 52 + md.height), (0, 0, 0, 0))
    p.alpha_composite(body, (0, 52))
    p.alpha_composite(title, ((w - 199) // 2, 0))
    return p


# ---------------------------------------------------------------- layout

RECTS = []          # names, in order
SPR = {}            # name -> RGBA image
TEXT = []           # (screen, layer, x, y, scale, tint, string)
TINTS = ['white', 'red', 'yellow', 'amber', 'blue', 'green', 'cyan']


def sprite(name, im):
    if name not in SPR:
        SPR[name] = im
    return name


class Screen:
    def __init__(self, idx, w, h, wallpaper, game):
        self.idx, self.w, self.h, self.game = idx, w, h, game
        self.bg = img(wallpaper).resize((w, h), Image.LANCZOS)
        self.chase = Image.new('RGBA', (w, h), (0, 0, 0, 0))
        self.rects = {}

    def put(self, im, s, x, y, layer=0):
        """a frame at canvas (x, y), scale s; returns a mapper native -> canvas rect"""
        sim = scaled(im, s)
        (self.chase if layer else self.bg).alpha_composite(sim, (round(x), round(y)))

        def m(nx, ny, nw=0, nh=0):
            return (round(x + nx * s), round(y + ny * s), max(1, round(nw * s)), max(1, round(nh * s)))
        m.s, m.x, m.y, m.w, m.h = s, x, y, sim.width, sim.height
        return m

    def rect(self, name, r):
        if name not in RECTS:
            RECTS.append(name)
        self.rects[name] = r

    def text(self, layer, x, y, scale, tint, s):
        TEXT.append((self.idx, layer, round(x), round(y), scale, TINTS.index(tint), s))

    def ctext(self, layer, box, scale, tint, s):
        x0, y0, w, h = box
        self.text(layer, x0 + (w - 8 * scale * len(s)) // 2, y0 + (h - 8 * scale) // 2, scale, tint, s)


def digit_sprites(tag, w, h):
    for c in '0123456789abcdef':
        sprite('D%s_%s' % (tag, c), img(c + '.png').resize((w, h), Image.LANCZOS))
    sprite('D%s_minus' % tag, img('minus.png').resize((w, h), Image.LANCZOS))


PORTRAIT_FILES = ['b1', 'b2', 'b3', 'b4', 'b5', 'b6', 'b7', 'b8', 'b9', 'b10', 'b11', 'b10', 'b11', 'b12', 'b13',
                  'b14', 'b15', 'b16']


def portraits(tag, w, h):
    for i, f in enumerate(PORTRAIT_FILES):
        sprite('PT%s_%d' % (tag, i), img(f + '.png').resize((w, h), Image.LANCZOS))


ITEM_FILES = ['item_medal.jpg', 'item_small_shot.jpg', 'item_option.jpg', 'item_large_shot.jpg']
MEDAL_FILES = ['Item_%d.png' % v for v in (100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 2000, 3000, 4000,
                                           5000, 6000, 7000, 8000, 9000, 10000)]
STICK = {0: 'c', 1: 'n', 2: 's', 4: 'w', 8: 'e', 5: 'nw', 6: 'sw', 10: 'se', 9: 'ne'}


def items(tag, size):
    for i, f in enumerate(ITEM_FILES):
        sprite('IT%s_%d' % (tag, i), img(f).resize((size, size), Image.LANCZOS))


def medals(tag, size):
    for i, f in enumerate(MEDAL_FILES):
        sprite('MD%s_%d' % (tag, i), img(f).resize((size, size), Image.NEAREST))


def input_panel(sc, tag, s, x, y):
    m = sc.put(img('input_display.png'), s, x, y)
    sc.rect('IN_STICK', m(22, 43, 71, 71))
    for n, (bx, by) in zip('ABCS', [(125, 34), (203, 34), (125, 93), (203, 93)]):
        sc.rect('IN_' + n, m(bx, by, 36, 36))
    st = sc.rects['IN_STICK']
    for k, f in STICK.items():
        sprite('ST%s_%d' % (tag, k), img('stick_%s.png' % f).resize((st[2], st[3]), Image.LANCZOS))
    b = sc.rects['IN_A']
    sprite('BT%s' % tag, img('square.png').resize((b[2], b[3]), Image.LANCZOS))
    return m


def profile_panel(sc, tag, s, x, y):
    m = sc.put(img('profile.png'), s, x, y)
    sc.rect('PL_MAIN', m(38, 121, 164, 191))
    sc.rect('PL_NEXT', m(214, 121, 109, 127))
    sc.rect('PL_THIRD', m(332, 121, 109, 127))
    sc.rect('PL_TYPE', m(400, 265, 50, 42))
    r = sc.rects
    portraits(tag + 'M', r['PL_MAIN'][2], r['PL_MAIN'][3])
    portraits(tag + 'S', r['PL_NEXT'][2], r['PL_NEXT'][3])
    digit_sprites(tag + 'T', r['PL_TYPE'][2], r['PL_TYPE'][3])
    return m


def rank_panel(sc, tag, s, x, y):
    m = sc.put(img('rank_bar.png'), s, x, y)
    sc.rect('RANK_BAR', m(17, 104, 490, 30))
    for i, dx in enumerate([189, 216, 250, 272, 296, 320]):
        sc.rect('RANK_D%d' % i, m(dx, 145, 28.6, 24))
    r = sc.rects['RANK_D0']
    digit_sprites(tag + 'R', r[2], r[3])
    return m


def drops_panel(sc, tag, s, x, y):
    m = sc.put(img('next_drop.png'), s, x, y)
    for i in range(6):
        sc.rect('NEXT_%d' % i, m(33 + 81 * i, 131, 49, 49))
    items(tag, sc.rects['NEXT_0'][2])
    return m


def count_panel(sc, s, x, y):
    m = sc.put(count_frame(), s, x, y)
    sc.ctext(0, m(118, 44, 280, 46), 2 if s > 0.5 else 1, 'white', 'Item Count')
    for i, (sx, sy) in enumerate(COUNT_SLOTS):
        sc.rect('CNT_%d' % i, m(sx + 2, sy + 2, 58, 58))
    return m


def medal_panel(sc, tag, s, x, y):
    p = medal_plate()
    m = sc.put(p, s, x, y)
    sc.rect('MEDAL_CNT', m(124, 52 + 22, 80, 80))
    sc.rect('MEDAL_PIC', m(224, 52 + 26, 72, 72))
    sc.rect('MEDAL_VAL', m(310, 52 + 30, p.width - 330, 64))
    medals(tag, sc.rects['MEDAL_PIC'][2])
    return m


def boss_panel(sc, tag, s, x, y):
    m = sc.put(img('boss_info.png'), s, x, y)
    for i, dx in enumerate([46, 84, 122]):
        sc.rect('BOSS_HP%d' % i, m(dx, 210, 38, 32))
    for i, dx in enumerate([46, 84, 122]):
        sc.rect('BOSS_T%d' % i, m(dx, 349, 38, 32))
    r = sc.rects['BOSS_HP0']
    digit_sprites(tag + 'B', r[2], r[3])
    return m


def secret_panel(sc, tag, s, x, y):
    m = sc.put(img('secret_conditions.png'), s, x, y)
    for i in range(12):
        sc.rect('COND_%d' % i, m(197, 123 + 39 * i, 23, 23))
    r = sc.rects['COND_0']
    sprite('CK%s_1' % tag, img('checked.png').resize((r[2], round(r[3] * 31 / 23)), Image.LANCZOS))
    sprite('CK%s_2' % tag, img('minus.png').resize((r[2], r[3]), Image.LANCZOS))
    return m


def charge_panel(sc, s, x, y):
    m = sc.put(img('shot_charge.png'), s, x, y)
    sc.rect('CHARGE_V', m(7, 16, 13, 414))
    return m


LIST_ROWS = {'STATUS': ['Course', 'Stage', 'Time', 'Enemies', 'Bullets'],
             'WEAPON': ['Shot', 'Options', 'Formation', 'Bombs', 'Sp.Power', 'Sp.Option']}
LIST_TINT = {'STATUS': ('Status', 'yellow'), 'WEAPON': ('Weapon', 'green')}
ROW_TINT = {'Sp.Power': 'red', 'Sp.Option': 'red'}


def list_panel(sc, name, s, x, y, cw):
    """a list panel of canvas width cw: title + row labels drawn by the 360, values right-aligned in VAL rects"""
    rows = LIST_ROWS[name]
    nw = round(cw / s)
    m = sc.put(list_frame(len(rows), nw), s, x, y)
    title, tint = LIST_TINT[name]
    fs = 2 if s >= 0.5 else 1
    sc.ctext(0, m(12, 12, nw - 24, 54), fs, tint, title)
    for i, lab in enumerate(rows):
        r = m(26, 108 + 39 * i, nw - 52, 39)
        sc.text(0, r[0], r[1] + (r[3] - 8) // 2, 1, ROW_TINT.get(lab, 'white'), lab)
        sc.rect('%s_%d' % (name, i), r)
    if name == 'WEAPON':                    # Sp.Power / Sp.Option's tick
        h = sc.rects['WEAPON_0'][3] - 4
        sprite('WK%s' % 'YT'[sc.idx], img('checked.png').resize((max(4, h * 21 // 31), h), Image.LANCZOS))
    return m


def stages_panel(sc, s, x, y, cw, ch):
    """STAGES (ibara's): 7 rows of name + score, then TOTAL; rows of two text lines"""
    nw = round(cw / s)
    n = 8
    pitch = max(39, round((ch / s - 112 - 33) / n))
    m = sc.put(list_frame(n, nw, pitch), s, x, y)
    sc.ctext(0, m(12, 12, nw - 24, 54), 2 if s >= 0.5 else 1, 'red', 'Stages')
    for i in range(n):
        sc.rect('STG_%d' % i, m(24, 108 + pitch * i, nw - 48, pitch))
    return m


def chase_panels(sc, s, x, y, mult_dx, mult_dy, lives_dx, lives_dy, ext_dx, ext_dy):
    m = sc.put(chain_frame(), s, x, y, 1)
    fs = 2 if s >= 0.5 else 1
    sc.ctext(1, m(108, 28, 296, 48), fs, 'blue', 'Chain')
    sc.rect('CH_BAR', m(17, 104, 490, 30))
    sc.rect('CH_CHAIN', m(170, 138, 175, 32))
    mm = sc.put(mult_frame(), s, x + mult_dx, y + mult_dy, 1)
    tb = mm(10, 10, 180, 44)
    sc.ctext(1, tb, 1, 'amber', 'Multiplier' if tb[2] >= 84 else 'Mult.')
    sc.rect('CH_MULT', mm(56, 84, 84, 76))
    sc.rect('CH_C', mm(140, 70, 50, 46))
    sc.rect('CH_FWD', mm(140, 118, 50, 46))
    ml = sc.put(lives_frame(), s, x + lives_dx, y + lives_dy, 1)
    sc.rect('CH_LIVES', ml(120, 26, 86, 82))
    me = sc.put(extend_box(), s, x + ext_dx, y + ext_dy, 1)
    lab_y = y + ext_dy - 10
    sc.text(1, x + ext_dx + 2, lab_y, 1, 'green', 'Next Extend')
    sc.rect('CH_EXT', me(8, 4, 182, 32))


def yoko():
    sc = Screen(0, 1280, 720, 'background_yoko.png', (370, 0, 540, 720))
    # LEFT: Current Player + input, shot charge, Status + Weapon, CHASE! at the bottom
    profile_panel(sc, 'Y', 0.5, 4, 4)
    input_panel(sc, 'Y', 0.42, 249, 24)
    charge_panel(sc, 0.4, 352, 6)
    list_panel(sc, 'STATUS', 0.55, 4, 194, 176)
    list_panel(sc, 'WEAPON', 0.55, 186, 194, 176)
    chase_panels(sc, 0.62, 18, 430, 0, 126, 128, 132, 128, 222)
    music_panel(sc, 4, 404, 358, 25, 1)
    # RIGHT: Rank Bar, Item Drops, Item Count packed; the medal gadget; Secret bosses, Boss Info, Stages
    rank_panel(sc, 'Y', 0.6, 938, 0)
    drops_panel(sc, 'Y', 0.6, 939, 86)
    count_panel(sc, 0.6, 939, 198)
    medal_panel(sc, 'Y', 0.5, 920, 332)
    secret_panel(sc, 'Y', 0.45, 912, 442)
    boss_panel(sc, 'Y', 0.5, 1026, 440)
    stages_panel(sc, 0.5, 1140, 440, 138, 278)
    return sc


def tate():
    sc = Screen(1, 720, 1280, 'background_tate.png', (0, 160, 720, 960))
    # TOP strip (720 x 160): Current Player + input, Rank Bar over Item Drops, Boss Info, shot charge
    profile_panel(sc, 'T', 0.42, 2, 3)
    input_panel(sc, 'T', 0.42, 208, 2)
    medal_panel(sc, 'T', 0.26, 208, 74)
    rank_panel(sc, 'T', 0.42, 338, 0)
    drops_panel(sc, 'T', 0.42, 339, 62)
    boss_panel(sc, 'T', 0.34, 562, 4)
    charge_panel(sc, 0.34, 644, 4)
    # BOTTOM strip: Item Count, Status, Weapon, CHASE!
    count_panel(sc, 0.42, 2, 1122)
    list_panel(sc, 'STATUS', 0.4, 222, 1121, 138)
    list_panel(sc, 'WEAPON', 0.4, 360, 1121, 138)
    chase_panels(sc, 0.38, 500, 1120, 0, 66, 82, 68, 82, 128)
    music_panel(sc, 4, 1228, 214, 48, 2)
    return sc


# ---------------------------------------------------------------- output

MENU_S = 0.5    # the menus' frames on the 480 x 640 upright game texture (test 11)


def menu_kit():
    """Test 11 (user: the menus with the gadgets' borders and font): Secret bosses' frame (list_frame's source) in
    nine pieces at MENU_S, for core/menudraw.c to build a frame of any size the way list_frame does: top band (title
    plate), a body row band repeated down, the bottom band; each left (110 native), a middle (26, repeated across),
    right (110)."""
    src = img('secret_conditions.png')
    bands = {'T': (0, 112), 'B': (112, 151), 'E': (580, 613)}
    cols = {'L': (0, 110), 'M': (110, 136), 'R': (136, 246)}
    for bn, (y0, y1) in bands.items():
        for cn, (x0, x1) in cols.items():
            sprite('MK_%s%s' % (bn, cn), scaled(src.crop((x0, y0, x1, y1)), MENU_S))


def c_bytes(name, data):
    z = zlib.compress(bytes(data), 9)
    out = ['static const unsigned char %s[%d] = {' % (name, len(z))]
    for i in range(0, len(z), 24):
        out.append(''.join('%d,' % b for b in z[i:i + 24]))
    out.append('};')
    return '\n'.join(out), len(z)


def main():
    screens = [yoko(), tate()]
    menu_kit()
    L = ['/* generated by tools/mkgadgets.py from ref/gadgets_baffo04/0.4 (Baffo\'s Batrider layout v0.4): do not edit */',
         '#ifndef BR_GADGETS_ART_H', '#define BR_GADGETS_ART_H', '#include <stdint.h>', '']
    L.append('enum {')
    L += ['\tGA_%s,' % n for n in RECTS]
    L.append('\tGA_N };')
    L.append('/* x, y, w, h on each screen (w 0 = not on that screen) */')
    L.append('static const int16_t ga_rect[2][GA_N][4] = {')
    for sc in screens:
        L.append('\t{ ' + ' '.join('{%d,%d,%d,%d},' % sc.rects.get(n, (0, 0, 0, 0)) for n in RECTS) + ' },')
    L.append('};')
    # sprites
    names = list(SPR)
    blob = bytearray()
    L.append('enum {')
    L += ['\tGS_%s,' % n for n in names]
    L.append('\tGS_N };')
    L.append('static const struct { uint16_t w, h; uint32_t off; } ga_spr[GS_N] = {')
    for n in names:
        im = SPR[n]
        L.append('\t{ %d, %d, %d },' % (im.width, im.height, len(blob)))
        blob += im.tobytes()                # RGBA
    L.append('};')
    L.append('enum { GA_SPR_RAW = %d };' % len(blob))
    s, n = c_bytes('ga_spr_z', blob)
    L.append(s)
    total = n
    for sc in screens:
        rgb = Image.new('RGB', (sc.w, sc.h))
        rgb.paste(sc.bg.convert('RGB'))
        s, n = c_bytes('ga_bg%d_z' % sc.idx, rgb.tobytes())
        L.append(s)
        total += n
        bb = sc.chase.getbbox() or (0, 0, 1, 1)
        L.append('static const int16_t ga_chase%d_box[4] = { %d, %d, %d, %d };' % (sc.idx, bb[0], bb[1],
                                                                               bb[2] - bb[0], bb[3] - bb[1]))
        s, n = c_bytes('ga_chase%d_z' % sc.idx, sc.chase.crop(bb).tobytes())
        L.append(s)
        total += n
    pix = max(max(1, (sc.chase.getbbox() or (0, 0, 1, 1))[2] - (sc.chase.getbbox() or (0, 0, 1, 1))[0]) *
              max(1, (sc.chase.getbbox() or (0, 0, 1, 1))[3] - (sc.chase.getbbox() or (0, 0, 1, 1))[1]) for sc in screens)
    L.append('enum { GA_CHASE_PIX = %d };' % pix)
    L.append('typedef struct { uint8_t screen, layer, scale, tint; int16_t x, y; const char *s; } GaText;')
    L.append('static const GaText ga_text[] = {')
    for (scr, layer, x, y, scale, tint, st) in TEXT:
        L.append('\t{ %d, %d, %d, %d, %d, %d, "%s" },' % (scr, layer, scale, tint, x, y, st))
    L.append('};')
    L.append('#endif')
    open(P + '/build/gen/gadgets_art.h', 'w', newline='\n').write('\n'.join(L) + '\n')
    for sc in screens:
        im = sc.bg.copy()
        im.alpha_composite(sc.chase)
        d = ImageDraw.Draw(im)
        for n, (x, y, w, h) in sc.rects.items():
            d.rectangle((x, y, x + w - 1, y + h - 1), outline=(255, 0, 255, 255))
        gx, gy, gw, gh = sc.game
        d.rectangle((gx, gy, gx + gw - 1, gy + gh - 1), outline=(0, 255, 255, 255), width=2)
        for (scr, layer, x, y, scale, tint, st) in TEXT:
            if scr == sc.idx:
                d.rectangle((x, y, x + 8 * scale * len(st) - 1, y + 8 * scale - 1), outline=(255, 255, 0, 255))
        im.convert('RGB').save(P + '/work/gadgets_layout_%s.png' % ('yoko', 'tate')[sc.idx])
    print('gadgets_art.h: %d rects, %d sprites (%d bytes raw), %d bytes deflated' % (len(RECTS), len(names),
                                                                                    len(blob), total))


if len(sys.argv) > 2 and sys.argv[2] == '--previews':
    for src, dst in zip(sys.argv[3:6], ['yoko_chase', 'yoko_original', 'tate_chase']):
        Image.open(src).convert('RGB').save(P + '/work/gadgets_%s.png' % dst)
else:
    main()
