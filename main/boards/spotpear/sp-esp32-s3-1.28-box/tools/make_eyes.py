"""Generator animowanych oczu robota (GIF) dla Xiaozhi - 21 emocji.
Wlasny projekt: swiecace zaokraglone oczy na czarnym tle."""
import math, os
from PIL import Image, ImageDraw, ImageFilter, ImageFont

W, H = 226, 150          # rozmiar GIF-a (ekran okragly 240x240, czarne tlo)
S = 3                    # nadprobkowanie (wygladzanie krawedzi)
LX, RX, CY = 62, 164, 76 # srodki oczu
EW, EH, ER = 68, 90, 22  # oko: szerokosc, wysokosc, promien rogow

CYAN = (40, 225, 255)
RED = (255, 70, 50)
PINK = (255, 90, 170)
BLUE = (80, 160, 255)
WHITE = (230, 240, 255)
FONT = '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'
OUT = os.path.join(os.path.dirname(__file__), '..', 'mati_eyes')


def sc(v):
    return int(round(v * S))


class Frame:
    def __init__(self):
        self.layers = []  # (maska L, kolor)

    def mask(self):
        return Image.new('L', (W * S, H * S), 0)

    def add(self, m, color):
        self.layers.append((m, color))

    def render(self):
        img = Image.new('RGB', (W * S, H * S), (0, 0, 0))
        for m, color in self.layers:
            glow = m.filter(ImageFilter.GaussianBlur(sc(4))).point(lambda p: int(p * 0.45))
            img.paste(Image.new('RGB', img.size, tuple(int(c * 0.8) for c in color)), (0, 0), glow)
            img.paste(Image.new('RGB', img.size, color), (0, 0), m)
        return img.resize((W, H), Image.LANCZOS)


# ---------- ksztalty ----------
def eye(fr, cx, cy, w=EW, h=EH, r=ER, color=CYAN, lid=None, arc=False, lid_bottom=0.0):
    """lid=(lewy_y, prawy_y) - powieka gorna jako linia (wzgledem gory oka, 0..1 wysokosci),
    arc=True - oko w ksztalcie luku (usmiech oczami), lid_bottom - zakrycie od dolu (0..1)."""
    if h < 2:
        return line_eye(fr, cx, cy, w, color)
    m = fr.mask()
    d = ImageDraw.Draw(m)
    x0, y0, x1, y1 = cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2
    d.rounded_rectangle([sc(x0), sc(y0), sc(x1), sc(y1)], radius=sc(min(r, w / 2, h / 2)), fill=255)
    if arc:  # wyciecie dolu elipsa -> ksztalt "^"
        d.ellipse([sc(x0 - w * 0.15), sc(cy - h * 0.05), sc(x1 + w * 0.15), sc(y1 + h * 0.9)], fill=0)
    if lid:
        a, b = lid
        d.polygon([(sc(x0 - 5), sc(y0 - 5)), (sc(x1 + 5), sc(y0 - 5)),
                   (sc(x1 + 5), sc(y0 + h * b)), (sc(x0 - 5), sc(y0 + h * a))], fill=0)
    if lid_bottom > 0:
        d.rectangle([sc(x0 - 5), sc(y1 - h * lid_bottom), sc(x1 + 5), sc(y1 + 5)], fill=0)
    fr.add(m, color)


def line_eye(fr, cx, cy, w=EW, color=CYAN, bend=0):
    m = fr.mask()
    d = ImageDraw.Draw(m)
    if bend == 0:
        d.rounded_rectangle([sc(cx - w / 2), sc(cy - 3), sc(cx + w / 2), sc(cy + 3)], radius=sc(3), fill=255)
    else:  # zamkniete oko jako luk (bend>0 wypukle w gore)
        pts = [(sc(cx - w / 2 + w * t), sc(cy - bend * math.sin(math.pi * t))) for t in [i / 20 for i in range(21)]]
        d.line(pts, fill=255, width=sc(6), joint='curve')
        for p in (pts[0], pts[-1]):
            d.ellipse([p[0] - sc(3), p[1] - sc(3), p[0] + sc(3), p[1] + sc(3)], fill=255)
    fr.add(m, color)


def chevron(fr, cx, cy, size, left, color=CYAN):
    m = fr.mask()
    d = ImageDraw.Draw(m)
    s = size / 2
    pts = [(cx - s, cy - s), (cx + s, cy), (cx - s, cy + s)] if left else [(cx + s, cy - s), (cx - s, cy), (cx + s, cy + s)]
    d.line([(sc(x), sc(y)) for x, y in pts], fill=255, width=sc(9), joint='curve')
    for x, y in (pts[0], pts[-1]):
        d.ellipse([sc(x - 4.5), sc(y - 4.5), sc(x + 4.5), sc(y + 4.5)], fill=255)
    fr.add(m, color)


def heart(fr, cx, cy, size, color=PINK):
    m = fr.mask()
    d = ImageDraw.Draw(m)
    pts = []
    for i in range(80):
        t = 2 * math.pi * i / 80
        x = 16 * math.sin(t) ** 3
        y = -(13 * math.cos(t) - 5 * math.cos(2 * t) - 2 * math.cos(3 * t) - math.cos(4 * t))
        pts.append((sc(cx + x * size / 34), sc(cy + y * size / 34)))
    d.polygon(pts, fill=255)
    fr.add(m, color)


def blob(fr, cx, cy, rx, ry, color):
    m = fr.mask()
    ImageDraw.Draw(m).ellipse([sc(cx - rx), sc(cy - ry), sc(cx + rx), sc(cy + ry)], fill=255)
    fr.add(m, color)


def tear(fr, cx, cy, size, color=BLUE):
    m = fr.mask()
    d = ImageDraw.Draw(m)
    d.ellipse([sc(cx - size), sc(cy - size), sc(cx + size), sc(cy + size)], fill=255)
    d.polygon([(sc(cx - size * 0.9), sc(cy - size * 0.4)), (sc(cx), sc(cy - size * 2.4)), (sc(cx + size * 0.9), sc(cy - size * 0.4))], fill=255)
    fr.add(m, color)


def text(fr, s, x, y, size, color=CYAN):
    m = fr.mask()
    ImageDraw.Draw(m).text((sc(x), sc(y)), s, font=ImageFont.truetype(FONT, sc(size)), fill=255, anchor='mm')
    fr.add(m, color)


def star(fr, cx, cy, r, color=WHITE):
    m = fr.mask()
    pts = []
    for i in range(8):
        a = math.pi / 4 * i - math.pi / 2
        rr = r if i % 2 == 0 else r * 0.3
        pts.append((sc(cx + rr * math.cos(a)), sc(cy + rr * math.sin(a))))
    ImageDraw.Draw(m).polygon(pts, fill=255)
    fr.add(m, color)


def ease(t):
    return 0.5 - 0.5 * math.cos(math.pi * max(0.0, min(1.0, t)))


# ---------- animacje: kazda zwraca liste (Frame, czas_ms) ----------
def pair(fr, dx=0, dy=0, **kw):
    eye(fr, LX + dx, CY + dy, **kw)
    eye(fr, RX + dx, CY + dy, **kw)


def blink_seq(draw, base_h=EH, steps=(0.6, 0.15, 0.6), ms=40):
    out = []
    for f in steps:
        fr = Frame(); draw(fr, base_h * f); out.append((fr, ms))
    return out


def a_neutral():
    seq = []
    def d(fr, h, dx=0): pair(fr, dx=dx, h=h)
    fr = Frame(); d(fr, EH); seq.append((fr, 1600))
    seq += blink_seq(lambda fr, h: d(fr, h))
    fr = Frame(); d(fr, EH); seq.append((fr, 900))
    for k in range(1, 5):
        fr = Frame(); d(fr, EH, -14 * ease(k / 4)); seq.append((fr, 50))
    fr = Frame(); d(fr, EH, -14); seq.append((fr, 700))
    for k in range(1, 5):
        fr = Frame(); d(fr, EH, -14 + 28 * ease(k / 4)); seq.append((fr, 50))
    fr = Frame(); d(fr, EH, 14); seq.append((fr, 600))
    for k in range(1, 4):
        fr = Frame(); d(fr, EH, 14 - 14 * ease(k / 3)); seq.append((fr, 50))
    return seq


def a_happy(fast=False, amp=4, sparkle=False):
    seq = []
    n = 8
    for k in range(n):
        dy = -amp * abs(math.sin(math.pi * k / n * 2))
        fr = Frame(); pair(fr, dy=dy + 4, arc=True, h=EH * 0.9)
        if sparkle:
            s = 4 + 3 * abs(math.sin(math.pi * k / n * 2))
            star(fr, 18, 28, s); star(fr, 202, 30, 7 - s + 4)
        seq.append((fr, 45 if fast else 90))
    return seq


def a_laughing():
    seq = []
    for k in range(10):
        dy = -6 * abs(math.sin(math.pi * k / 5))
        dx = 3 * math.sin(math.pi * k / 2.5)
        fr = Frame(); pair(fr, dx=dx, dy=dy + 6, arc=True, h=EH * 0.8, w=EW + 4); seq.append((fr, 45))
    return seq


def a_funny():
    seq = []
    for k in range(8):
        dy = -4 * abs(math.sin(math.pi * k / 4))
        fr = Frame(); chevron(fr, LX, CY + dy, 46, True); chevron(fr, RX, CY + dy, 46, False); seq.append((fr, 60))
    return seq


def a_sad(tears=False):
    seq = []
    n = 16
    for k in range(n):
        t = k / n
        droop = 2 * math.sin(2 * math.pi * t)
        fr = Frame()
        eye(fr, LX, CY + 10 + droop, h=EH * 0.75, lid=(0.55, 0.05))   # zewnetrzny kat nizej
        eye(fr, RX, CY + 10 + droop, h=EH * 0.75, lid=(0.05, 0.55))
        if tears:
            for i, (x, ph) in enumerate(((LX - 18, 0.0), (RX + 18, 0.5))):
                p = (t + ph) % 1.0
                tear(fr, x, CY + 34 + p * 50, 6 * (1 - 0.3 * p))
        seq.append((fr, 80))
    return seq


def a_angry():
    seq = []
    for k in range(10):
        sx = (2 if k % 2 else -2) if k < 6 else 0
        fr = Frame()
        eye(fr, LX + sx, CY + 4, h=EH * 0.8, color=RED, lid=(0.05, 0.55))   # wewnetrzny kat nizej
        eye(fr, RX + sx, CY + 4, h=EH * 0.8, color=RED, lid=(0.55, 0.05))
        seq.append((fr, 50 if k < 6 else 400))
    return seq


def a_loving():
    seq = []
    for k in range(10):
        s = 54 + 8 * math.sin(2 * math.pi * k / 10)
        fr = Frame(); heart(fr, LX, CY, s); heart(fr, RX, CY, s); seq.append((fr, 70))
    return seq


def a_embarrassed():
    seq = []
    for k in range(12):
        fr = Frame()
        blob(fr, LX - 14, CY + 44, 16, 7, PINK); blob(fr, RX + 14, CY + 44, 16, 7, PINK)
        h = EH * 0.6 if k not in (7, 8) else EH * 0.12
        pair(fr, dx=-10, dy=10, h=h, w=EW * 0.85)
        seq.append((fr, 120 if k not in (7, 8) else 50))
    return seq


def a_surprised():
    seq = []
    sizes = [0.8, 1.05, 1.2, 1.15, 1.12] + [1.12] * 3
    for i, f in enumerate(sizes):
        fr = Frame(); pair(fr, w=EW * f, h=EH * f, r=EW * f / 2); seq.append((fr, 60 if i < 5 else 300))
    return seq


def a_shocked():
    seq = []
    for k in range(10):
        sx = 3 * math.sin(math.pi * k * 0.9)
        fr = Frame(); pair(fr, dx=sx, dy=-4, w=EW * 0.8, h=EH * 1.15, r=26); seq.append((fr, 45))
    fr = Frame(); pair(fr, dy=-4, w=EW * 0.8, h=EH * 1.15, r=26); seq.append((fr, 500))
    return seq


def a_thinking():
    seq = []
    for k in range(12):
        fr = Frame()
        eye(fr, LX + 12, CY - 8, h=EH * 0.9)
        eye(fr, RX + 12, CY - 8, h=EH * 0.9, lid=(0.35, 0.35))
        for i in range(3):
            on = (k // 2) % 4 > i
            if on:
                blob(fr, 180 + i * 12, 138, 3.5, 3.5, CYAN)
        seq.append((fr, 110))
    return seq


def a_winking():
    seq = []
    fr = Frame(); eye(fr, LX, CY); eye(fr, RX, CY); seq.append((fr, 600))
    for f in (0.5, 0.15):
        fr = Frame(); eye(fr, LX, CY); eye(fr, RX, CY, h=EH * f); seq.append((fr, 50))
    fr = Frame(); eye(fr, LX, CY); line_eye(fr, RX, CY + 6, bend=12); seq.append((fr, 500))
    for f in (0.15, 0.5):
        fr = Frame(); eye(fr, LX, CY); eye(fr, RX, CY, h=EH * f); seq.append((fr, 50))
    return seq


def a_cool():
    seq = []
    n = 14
    for k in range(n):
        fr = Frame()
        pair(fr, w=EW + 10, h=34, r=8, dy=-6)
        x = -20 + 260 * (k / (n - 1))
        m = fr.mask()
        ImageDraw.Draw(m).polygon([(sc(x), sc(40)), (sc(x + 10), sc(40)), (sc(x - 6), sc(80)), (sc(x - 16), sc(80))], fill=255)
        # blysk tylko na "okularach"
        vis = fr.mask()
        dv = ImageDraw.Draw(vis)
        for cx in (LX, RX):
            dv.rounded_rectangle([sc(cx - (EW + 10) / 2), sc(CY - 6 - 17), sc(cx + (EW + 10) / 2), sc(CY - 6 + 17)], radius=sc(8), fill=255)
        from PIL import ImageChops
        fr.add(ImageChops.multiply(m, vis), WHITE)
        seq.append((fr, 70 if k < n - 1 else 900))
    return seq


def a_relaxed():
    seq = []
    for k in range(16):
        b = 0.5 + 0.06 * math.sin(2 * math.pi * k / 16)
        fr = Frame(); pair(fr, dy=6, h=EH * 0.8, lid=(b, b), arc=False); seq.append((fr, 110))
    return seq


def a_kissy():
    seq = []
    for k in range(12):
        fr = Frame()
        line_eye(fr, LX, CY + 6, bend=12)
        eye(fr, RX, CY)
        p = k / 11
        heart(fr, 112 + 10 * math.sin(p * 6), 110 - p * 70, 18 + 6 * p)
        seq.append((fr, 80))
    return seq


def a_confident():
    seq = []
    for k in range(16):
        dx = 10 * math.sin(2 * math.pi * k / 16)
        fr = Frame()
        eye(fr, LX + dx, CY + 4, h=EH * 0.85, lid=(0.45, 0.45))
        eye(fr, RX + dx, CY + 4, h=EH * 0.85, lid=(0.12, 0.3))   # jedna brew w gore - pewna siebie mina
        seq.append((fr, 100))
    return seq


def a_sleepy():
    seq = []
    n = 20
    for k in range(n):
        t = k / n
        close = 0.55 + 0.4 * ease(min(1, t * 2)) if t < 0.5 else 0.95 - 0.4 * ease((t - 0.5) * 2)
        fr = Frame()
        if close > 0.9:
            line_eye(fr, LX, CY + 14); line_eye(fr, RX, CY + 14)
        else:
            pair(fr, dy=8, h=EH * 0.8, lid=(close, close))
        zt = (t * 2) % 1.0
        text(fr, 'z', 196 + 6 * zt, 50 - 30 * zt, 12 + 8 * zt)
        seq.append((fr, 110))
    return seq


def a_silly():
    seq = []
    for k in range(10):
        a = math.sin(2 * math.pi * k / 10)
        fr = Frame()
        eye(fr, LX, CY + 4 * a, w=EW * (1 + 0.2 * a), h=EH * (1 + 0.2 * a), r=26)
        eye(fr, RX, CY - 4 * a, w=EW * (1 - 0.25 * a), h=EH * (1 - 0.25 * a), r=22)
        seq.append((fr, 70))
    return seq


def a_confused():
    seq = []
    for k in range(14):
        tilt = 4 * math.sin(2 * math.pi * k / 14)
        fr = Frame()
        eye(fr, LX, CY + 6 + tilt, h=EH * 0.85)
        eye(fr, RX, CY - 4 - tilt, w=EW * 0.7, h=EH * 0.6, r=16)
        text(fr, '?', 210, 24 + 3 * math.sin(2 * math.pi * k / 7), 26)
        seq.append((fr, 90))
    return seq



# ---------- zachowania "zycia" Matiego ----------
def look(dx, dy, hold=1400, squint=None):
    seq = []
    def d(fr, ox, oy, h=EH):
        kw = {'lid': squint} if squint else {}
        pair(fr, dx=ox, dy=oy, h=h, **kw)
    for k in range(1, 6):
        e = ease(k / 5); fr = Frame(); d(fr, dx * e, dy * e); seq.append((fr, 40))
    fr = Frame(); d(fr, dx, dy); seq.append((fr, hold // 2))
    seq += [(f, ms) for f, ms in blink_seq(lambda fr, h: d(fr, dx, dy, h))]
    fr = Frame(); d(fr, dx, dy); seq.append((fr, hold // 2))
    for k in range(1, 6):
        e = 1 - ease(k / 5); fr = Frame(); d(fr, dx * e, dy * e); seq.append((fr, 40))
    fr = Frame(); d(fr, 0, 0); seq.append((fr, 300))
    return seq


def a_yawn():
    seq = []
    # oczy zaciskaja sie w luki, "usta" (owal) rosna i maleja, potem senne oczy
    for k in range(18):
        t = k / 17
        o = math.sin(math.pi * min(1, t * 1.25))      # otwarcie ust 0..1..0
        fr = Frame()
        if o > 0.35:
            line_eye(fr, LX, CY - 4, bend=-10); line_eye(fr, RX, CY - 4, bend=-10)
        else:
            pair(fr, h=EH * (1 - o * 1.8) if o < 0.35 else 4, dy=0)
        if o > 0.05:
            blob(fr, 113, 122, 10 + 8 * o, 4 + 18 * o, CYAN)
            m = fr.mask(); ImageDraw.Draw(m).ellipse([sc(113 - 6 - 6 * o), sc(122 - 1 - 13 * o), sc(113 + 6 + 6 * o), sc(122 + 1 + 13 * o)], fill=255)
            fr.add(m, (0, 0, 0))
        seq.append((fr, 90 if 0.2 < t < 0.8 else 60))
    for k in range(6):
        fr = Frame(); pair(fr, dy=6, h=EH * 0.8, lid=(0.55, 0.55)); seq.append((fr, 120))
    return seq


def a_nap():
    seq = []
    n = 24
    for k in range(n):
        t = k / n
        b = 2.5 * math.sin(2 * math.pi * t)
        fr = Frame()
        line_eye(fr, LX, CY + 16 + b, bend=-8); line_eye(fr, RX, CY + 16 + b, bend=-8)
        for i in range(3):
            zt = (t + i / 3) % 1.0
            a = 1 - zt
            if a > 0.1:
                text(fr, 'Z', 170 + 30 * zt, 70 - 55 * zt, 10 + 10 * zt, tuple(int(c * a) for c in CYAN))
        seq.append((fr, 130))
    return seq


def cookie(fr, cx, cy, r, bites):
    m = fr.mask(); d = ImageDraw.Draw(m)
    d.ellipse([sc(cx - r), sc(cy - r), sc(cx + r), sc(cy + r)], fill=255)
    for i in range(bites):   # wygryzienia od prawej gory
        a = -0.5 + i * 0.9
        bx, by = cx + r * math.cos(a), cy + r * math.sin(a)
        d.ellipse([sc(bx - r * 0.45), sc(by - r * 0.45), sc(bx + r * 0.45), sc(by + r * 0.45)], fill=0)
    for (ox, oy) in ((-6, -4), (5, 5), (-2, 7), (7, -6)):
        d.ellipse([sc(cx + ox - 2), sc(cy + oy - 2), sc(cx + ox + 2), sc(cy + oy + 2)], fill=0)
    fr.add(m, (230, 170, 90))


def a_eat():
    seq = []
    for k in range(24):
        bites = min(4, k // 5)
        chew = abs(math.sin(math.pi * k / 3))
        fr = Frame()
        pair(fr, dy=-8 + 2 * chew, arc=True, h=EH * 0.75)
        if bites < 4:
            cookie(fr, 113, 122, 20, bites)
        else:
            for i in range(3):
                blob(fr, 100 + i * 12 + 3 * math.sin(k + i), 128 + 4 * ((k + i) % 3), 2.5, 2.5, (230, 170, 90))
        seq.append((fr, 110))
    return seq


def a_play():
    seq = []
    n = 20
    for k in range(n):
        t = k / n
        bx = 113 + 85 * math.sin(2 * math.pi * t)
        by = 135 - 55 * abs(math.sin(4 * math.pi * t))
        fr = Frame()
        dx = (bx - 113) * 0.18; dy = (by - 110) * 0.15
        pair(fr, dx=dx, dy=dy - 6, h=EH * 0.85, w=EW * 0.95)
        blob(fr, bx, by, 9, 9, (255, 200, 60))
        seq.append((fr, 60))
    return seq


def a_sing():
    seq = []
    n = 20
    for k in range(n):
        t = k / n
        sway = 8 * math.sin(2 * math.pi * t)
        fr = Frame()
        line_eye(fr, LX + sway, CY + 4 + abs(sway) * 0.3, bend=12)
        line_eye(fr, RX + sway, CY + 4 - abs(sway) * 0.3, bend=12)
        for i, sym in enumerate(('♪', '♪', '♪')):
            nt = (t + i / 3) % 1.0
            a = min(1.0, 2.2 * math.sin(math.pi * nt))
            x = 30 + 80 * i + 10 * math.sin(2 * math.pi * nt + i)
            col = (int(255 * a), int(215 * a), int(90 * a))
            m = fr.mask()
            ImageDraw.Draw(m).text((sc(x), sc(140 - 120 * nt)), sym, font=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', sc(22)), fill=255, anchor='mm')
            fr.add(m, col)
        seq.append((fr, 90))
    return seq


def a_pet():
    seq = []
    for k in range(16):
        t = k / 16
        fr = Frame()
        blob(fr, LX - 14, CY + 40, 15, 6, PINK); blob(fr, RX + 14, CY + 40, 15, 6, PINK)
        wob = 2 * math.sin(4 * math.pi * t)
        line_eye(fr, LX + wob, CY + 4, bend=14); line_eye(fr, RX + wob, CY + 4, bend=14)
        heart(fr, 113 + 8 * math.sin(6 * t), 70 - 50 * t, 12 + 8 * t)
        seq.append((fr, 90))
    return seq


def a_bored():
    seq = []
    for k in range(20):
        fr = Frame()
        dx = -12 + 24 * ease((k % 20) / 19)
        h = EH * 0.75 if k not in (12, 13) else EH * 0.1
        pair(fr, dx=dx, dy=14, h=h, lid=(0.5, 0.5))
        seq.append((fr, 160 if k not in (12, 13) else 60))
    return seq


def a_hungry():
    seq = []
    for k in range(16):
        t = k / 16
        fr = Frame()
        eye(fr, LX - 10, CY + 6, h=EH * 0.8, lid=(0.4, 0.05))
        eye(fr, RX - 10, CY + 6, h=EH * 0.8, lid=(0.05, 0.4))
        blob(fr, 200, 40, 20, 16, (60, 60, 70)); blob(fr, 180, 66, 4, 4, (60, 60, 70))
        cookie(fr, 200, 40, 10 + math.sin(2 * math.pi * t), 0)
        seq.append((fr, 120))
    return seq


# ---------- kolejne czynnosci (osobowosc) ----------
YEL = (255, 210, 70)
GRN = (120, 230, 120)
DEJ = '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'


def glyph(fr, s, x, y, size, color, bold=False):
    m = fr.mask()
    ImageDraw.Draw(m).text((sc(x), sc(y)), s, font=ImageFont.truetype(FONT if bold else DEJ, sc(size)), fill=255, anchor='mm')
    fr.add(m, color)


def spiral(fr, cx, cy, r, phase, color=CYAN):
    m = fr.mask(); d = ImageDraw.Draw(m)
    pts = []
    for i in range(90):
        t = i / 89
        a = phase + t * 4 * math.pi
        pts.append((sc(cx + r * t * math.cos(a)), sc(cy + r * t * math.sin(a))))
    d.line(pts, fill=255, width=sc(5), joint='curve')
    fr.add(m, color)


def a_dance():
    seq = []
    n = 16
    for k in range(n):
        t = k / n
        dx = 18 * math.sin(2 * math.pi * t)
        dy = -10 * abs(math.sin(4 * math.pi * t))
        fr = Frame()
        pair(fr, dx=dx, dy=dy + 4, arc=True, h=EH * 0.85)
        nt = (t * 2) % 1
        glyph(fr, '♪', 20 + 10 * math.sin(6 * t), 130 - 90 * nt, 20, YEL)
        glyph(fr, '♪', 206 - 10 * math.sin(6 * t), 130 - 90 * ((nt + 0.5) % 1), 20, PINK)
        seq.append((fr, 70))
    return seq


def a_sneeze():
    seq = []
    for f in (1.0, 0.9, 0.8, 0.7, 0.75, 0.65):            # nabieranie powietrza
        fr = Frame(); pair(fr, dy=-6 * (1 - f), h=EH * f, lid=(1 - f, 1 - f)); seq.append((fr, 110))
    fr = Frame(); line_eye(fr, LX, CY, bend=-10); line_eye(fr, RX, CY, bend=-10); seq.append((fr, 160))
    for k in range(5):                                      # APSIK!
        fr = Frame()
        chevron(fr, LX + (k % 2) * 3, CY, 40, True); chevron(fr, RX - (k % 2) * 3, CY, 40, False)
        glyph(fr, 'APSIK!', 113, 132, 18 + k, WHITE, bold=True)
        seq.append((fr, 70))
    fr = Frame(); pair(fr, h=EH * 0.8, w=EW * 0.9); seq.append((fr, 400))
    seq += blink_seq(lambda fr, h: pair(fr, h=h))
    fr = Frame(); pair(fr); seq.append((fr, 500))
    return seq


def a_hiccup():
    seq = []
    for rep in range(3):
        fr = Frame(); pair(fr); seq.append((fr, 500))
        for k, dy in enumerate((-14, -18, -10, -4)):
            fr = Frame(); pair(fr, dy=dy, w=EW * 1.05, h=EH * 0.95)
            if k < 3:
                glyph(fr, 'hik!', 196, 30 + dy, 16, WHITE, bold=True)
            seq.append((fr, 50))
    return seq


def a_daydream():
    seq = []
    n = 20
    for k in range(n):
        t = k / n
        fr = Frame()
        pair(fr, dx=-10, dy=-16, h=EH * 0.75, lid=(0.3, 0.3))
        for i, (x, y) in enumerate(((150, 22), (186, 16), (210, 34), (24, 18))):
            tw = 0.5 + 0.5 * math.sin(2 * math.pi * (t + i * 0.27))
            star(fr, x, y, 3 + 4 * tw, YEL)
        blob(fr, 70 + 6 * math.sin(2 * math.pi * t), 20, 18, 8, (90, 110, 140))
        seq.append((fr, 110))
    return seq


def a_stretch():
    seq = []
    shapes = [(1, 1), (0.8, 1.25), (0.7, 1.35), (0.75, 1.3), (1.2, 0.7), (1.35, 0.55), (1.25, 0.65), (1, 1)]
    for sx, sy in shapes:
        for _ in range(2):
            fr = Frame(); pair(fr, w=EW * sx, h=EH * sy, r=ER * min(sx, sy)); seq.append((fr, 110))
    fr = Frame(); pair(fr, arc=True, h=EH * 0.85, dy=4); seq.append((fr, 600))
    return seq


def a_read():
    seq = []
    for line in range(3):
        for k in range(8):
            dx = -16 + 32 * k / 7
            fr = Frame()
            pair(fr, dx=dx, dy=10 + line * 3, h=EH * 0.7, lid=(0.25, 0.25))
            m = fr.mask(); d = ImageDraw.Draw(m)       # ksiazka
            d.polygon([(sc(70), sc(118)), (sc(112), sc(126)), (sc(112), sc(148)), (sc(70), sc(140))], fill=255)
            d.polygon([(sc(156), sc(118)), (sc(114), sc(126)), (sc(114), sc(148)), (sc(156), sc(140))], fill=255)
            fr.add(m, (230, 230, 210))
            seq.append((fr, 120))
        fr = Frame(); pair(fr, dx=-16, dy=10 + line * 3, h=EH * 0.7, lid=(0.25, 0.25)); seq.append((fr, 80))
    return seq


def a_peekaboo():
    seq = []
    for k in range(1, 9):                          # chowa sie w dol
        fr = Frame(); pair(fr, dy=90 * ease(k / 8)); seq.append((fr, 45))
    fr = Frame(); seq.append((fr, 700))
    for k in range(1, 6):                          # wychyla sie z boku
        fr = Frame(); eye(fr, -40 + 60 * ease(k / 5), CY + 10, h=EH * 0.8); seq.append((fr, 60))
    fr = Frame(); eye(fr, 20, CY + 10, h=EH * 0.8); seq.append((fr, 500))
    for k in range(1, 9):                          # A KUKU!
        fr = Frame(); pair(fr, dy=90 * (1 - ease(k / 8)), w=EW * 1.1, h=EH * 1.1, r=30); seq.append((fr, 40))
    for k in range(6):
        fr = Frame(); pair(fr, arc=True, h=EH * 0.9, dy=4 - 4 * abs(math.sin(k))); glyph(fr, 'a kuku!', 113, 136, 18, YEL, bold=True); seq.append((fr, 90))
    return seq


def a_chase_fly():
    seq = []
    n = 24
    for k in range(n):
        t = k / n
        fx = 113 + 80 * math.sin(2 * math.pi * t * 1.5) * math.cos(2 * math.pi * t * 0.5)
        fy = 70 + 45 * math.sin(2 * math.pi * t * 2.3)
        if k > n - 5:   # mucha siada na nosie -> zez
            fx, fy = 113, 120
        fr = Frame()
        if k > n - 5:
            eye(fr, LX + 14, CY + 6, h=EH * 0.85); eye(fr, RX - 14, CY + 6, h=EH * 0.85)
        else:
            pair(fr, dx=(fx - 113) * 0.15, dy=(fy - 76) * 0.2, h=EH * 0.9)
        blob(fr, fx, fy, 3.5, 3.5, (200, 200, 200))
        wing = 3 + 2 * (k % 2)
        blob(fr, fx - 3, fy - 4, wing, 2, (140, 160, 200)); blob(fr, fx + 3, fy - 4, wing, 2, (140, 160, 200))
        seq.append((fr, 70 if k <= n - 5 else 200))
    return seq


def a_dizzy():
    seq = []
    for k in range(16):
        ph = k * 0.6
        fr = Frame()
        spiral(fr, LX, CY, 32, ph); spiral(fr, RX, CY, 32, ph + 1)
        for i in range(3):
            a = ph * 0.7 + i * 2.1
            star(fr, 113 + 60 * math.cos(a), 20 + 10 * math.sin(a), 5, YEL)
        seq.append((fr, 80))
    return seq


def a_whistle():
    seq = []
    for k in range(16):
        t = k / 16
        fr = Frame()
        pair(fr, dx=10, dy=-4, h=EH * 0.8, lid=(0.35, 0.2))
        blob(fr, 150, 128, 5, 5, CYAN)
        nt = t
        glyph(fr, '♪', 170 + 30 * nt, 120 - 70 * nt, 14 + 6 * nt, tuple(int(c * (1 - nt * 0.7)) for c in YEL))
        seq.append((fr, 110))
    return seq


def a_meditate():
    seq = []
    for k in range(20):
        t = k / 20
        b = math.sin(2 * math.pi * t)
        fr = Frame()
        line_eye(fr, LX, CY + 8 - 3 * b, bend=-6); line_eye(fr, RX, CY + 8 - 3 * b, bend=-6)
        m = fr.mask()
        ImageDraw.Draw(m).ellipse([sc(113 - 50 - 6 * b), sc(76 - 50 - 6 * b), sc(113 + 50 + 6 * b), sc(76 + 50 + 6 * b)], outline=255, width=sc(2))
        fr.add(m, tuple(int(c * (0.35 + 0.2 * b)) for c in (150, 120, 255)))
        if 0.3 < t < 0.7:
            glyph(fr, 'ommm', 113, 140, 14, (170, 150, 255))
        seq.append((fr, 140))
    return seq


def a_doze():
    seq = []
    for rep in range(2):
        for k in range(8):                       # powieki powoli opadaja
            f = 0.3 + 0.65 * ease(k / 7)
            fr = Frame(); pair(fr, dy=6 + 4 * f, h=EH * 0.8, lid=(f, f)); seq.append((fr, 130))
        fr = Frame(); line_eye(fr, LX, CY + 16, bend=-6); line_eye(fr, RX, CY + 16, bend=-6); seq.append((fr, 500))
        fr = Frame(); pair(fr, w=EW * 1.1, h=EH * 1.1, r=30, dy=-4); seq.append((fr, 120))  # ocknal sie!
        fr = Frame(); pair(fr); seq.append((fr, 350))
    return seq


def a_proud():
    seq = []
    for k in range(12):
        t = k / 12
        fr = Frame()
        pair(fr, dy=-6, h=EH * 0.85, lid=(0.15, 0.15))
        s1 = 3 + 5 * abs(math.sin(math.pi * t * 2))
        star(fr, LX + 22, CY - 30, s1, WHITE); star(fr, RX + 22, CY - 30, 8 - s1 + 3, WHITE)
        seq.append((fr, 90))
    return seq


def sheep(fr, cx, cy):
    for ox, oy in ((-8, 0), (0, -4), (8, 0), (0, 3), (-4, -2), (4, -2)):
        blob(fr, cx + ox, cy + oy, 8, 7, WHITE)
    blob(fr, cx + 14, cy - 2, 5, 4, (90, 90, 100))
    m = fr.mask(); d = ImageDraw.Draw(m)
    for lx in (-7, 7):
        d.rectangle([sc(cx + lx - 1.5), sc(cy + 8), sc(cx + lx + 1.5), sc(cy + 15)], fill=255)
    fr.add(m, (90, 90, 100))


def a_count_sheep():
    seq = []
    n = 24
    for k in range(n):
        t = k / n
        fr = Frame()
        pair(fr, dy=-22, h=EH * 0.55, w=EW * 0.8, lid=(0.55 + 0.15 * t, 0.55 + 0.15 * t))
        m = fr.mask(); ImageDraw.Draw(m).rectangle([sc(106), sc(130), sc(120), sc(150)], fill=255); fr.add(m, (120, 80, 50))  # plotek
        x = -20 + 266 * ((t * 2) % 1)
        y = 132 - 26 * math.sin(math.pi * ((t * 2) % 1))
        sheep(fr, x, y)
        glyph(fr, str(1 + int(t * 2)), 206, 22, 18, WHITE, bold=True)
        seq.append((fr, 90))
    return seq


def a_curious():
    seq = []
    pts = [(-20, -10), (20, -12), (14, 12), (-16, 10), (0, 0)]
    cur = (0, 0)
    for tx, ty in pts:
        for k in range(1, 5):
            e = ease(k / 4)
            dx = cur[0] + (tx - cur[0]) * e; dy = cur[1] + (ty - cur[1]) * e
            fr = Frame(); eye(fr, LX + dx, CY + dy, h=EH * 0.95); eye(fr, RX + dx, CY + dy, h=EH * 0.8, lid=(0.2, 0.2)); seq.append((fr, 40))
        fr = Frame(); eye(fr, LX + tx, CY + ty, h=EH * 0.95); eye(fr, RX + tx, CY + ty, h=EH * 0.8, lid=(0.2, 0.2))
        glyph(fr, '?', 206, 24, 22, YEL, bold=True)
        seq.append((fr, 450))
        cur = (tx, ty)
    return seq


# ---------- rytm dnia i gry ----------
BROWN = (190, 120, 60)
ORANGE = (255, 150, 40)
RED2 = (230, 60, 60)


def poly(fr, pts, color, width=0):
    m = fr.mask(); d = ImageDraw.Draw(m)
    p = [(sc(x), sc(y)) for x, y in pts]
    if width:
        d.line(p, fill=255, width=sc(width), joint='curve')
    else:
        d.polygon(p, fill=255)
    fr.add(m, color)


def rrect(fr, x0, y0, x1, y1, r, color, outline=0):
    m = fr.mask(); d = ImageDraw.Draw(m)
    if outline:
        d.rounded_rectangle([sc(x0), sc(y0), sc(x1), sc(y1)], radius=sc(r), outline=255, width=sc(outline))
    else:
        d.rounded_rectangle([sc(x0), sc(y0), sc(x1), sc(y1)], radius=sc(r), fill=255)
    fr.add(m, color)


def steam(fr, x, y, t, color=(160, 170, 190)):
    for i in range(3):
        ph = (t + i / 3) % 1
        pts = [(x - 12 + i * 12 + 4 * math.sin(6 * (ph + j / 8)), y - j * 4 - ph * 14) for j in range(6)]
        if ph < 0.85:
            poly(fr, pts, tuple(int(c * (1 - ph)) for c in color), width=2)


def meal(food_fn, n=20, ms=110, chew=True):
    seq = []
    for k in range(n):
        t = k / n
        c = abs(math.sin(math.pi * k / 3)) if chew else 0
        fr = Frame()
        pair(fr, dy=-14 + 2 * c, arc=True, h=EH * 0.6, w=EW * 0.85)
        food_fn(fr, t, k)
        seq.append((fr, ms))
    return seq


def f_breakfast(fr, t, k):
    rrect(fr, 72, 112, 112, 146, 8, (230, 190, 120))          # tost
    rrect(fr, 76, 116, 108, 142, 6, (250, 225, 170))
    blob(fr, 140, 132, 16, 11, WHITE); blob(fr, 140, 132, 7, 7, (255, 200, 40))  # jajko
    rrect(fr, 166, 116, 188, 146, 4, (200, 90, 70)); poly(fr, [(188, 122), (196, 126), (196, 136), (188, 140)], (200, 90, 70), width=3)
    steam(fr, 177, 110, t)


def f_apple(fr, t, k):
    bites = min(3, k // 6)
    m = fr.mask(); d = ImageDraw.Draw(m)
    d.ellipse([sc(95), sc(108), sc(131), sc(146)], fill=255)
    for i in range(bites):
        d.ellipse([sc(122 - i * 4), sc(110 + i * 11), sc(140 - i * 4), sc(126 + i * 11)], fill=0)
    fr.add(m, RED2)
    poly(fr, [(113, 110), (115, 100)], BROWN, width=3); blob(fr, 121, 103, 6, 3, GRN)


def f_soup(fr, t, k):
    m = fr.mask(); d = ImageDraw.Draw(m)
    d.chord([sc(78), sc(100), sc(148), sc(160)], 0, 180, fill=255); fr.add(m, (230, 230, 240))
    blob(fr, 113, 131, 33, 5, ORANGE)
    sy = 120 - 18 * abs(math.sin(math.pi * t * 2))
    poly(fr, [(150, sy - 10), (170, sy - 30)], (200, 200, 210), width=4); blob(fr, 146, sy - 6, 7, 4, (200, 200, 210))
    steam(fr, 113, 122, t)


def f_cake(fr, t, k):
    poly(fr, [(90, 146), (140, 146), (140, 122), (90, 132)], (240, 200, 160))
    poly(fr, [(90, 132), (140, 122), (140, 116), (90, 126)], PINK)
    blob(fr, 128, 113, 6, 6, RED2)
    if k % 6 < 3:
        star(fr, 150, 108, 5, WHITE)


def f_sandwich(fr, t, k):
    rrect(fr, 80, 126, 146, 146, 8, (220, 170, 100))
    poly(fr, [(80, 126), (146, 126)], GRN, width=5)
    rrect(fr, 82, 116, 144, 126, 4, (255, 210, 80))
    rrect(fr, 80, 104, 146, 118, 8, (220, 170, 100))


def a_drink():
    seq = []
    for k in range(18):
        t = k / 18
        lvl = 1 - t * 0.8
        fr = Frame()
        pair(fr, dy=-16, h=EH * 0.55, w=EW * 0.85, lid=(0.3, 0.3))
        rrect(fr, 98, 98, 128, 148, 4, (180, 220, 255), outline=2)
        rrect(fr, 101, 148 - 46 * lvl, 125, 145, 3, (90, 170, 255))
        if k % 4 < 2:
            blob(fr, 113 + 3 * math.sin(k), 140 - 40 * lvl * 0.5, 2, 2, WHITE)
        seq.append((fr, 110))
    return seq


def a_brush():
    seq = []
    for k in range(20):
        dx = 18 * math.sin(math.pi * k / 2)
        fr = Frame()
        pair(fr, dy=-16, h=EH * 0.5, lid=(0.2, 0.2), w=EW * 0.85)
        poly(fr, [(80 + dx, 126), (146 + dx, 126)], (120, 200, 255), width=6)
        rrect(fr, 66 + dx, 118, 84 + dx, 128, 3, WHITE)
        for i in range(4):
            ph = (k / 20 + i / 4) % 1
            blob(fr, 60 + i * 30 + 6 * math.sin(k + i), 140 - 30 * ph, 4 * (1 - ph) + 1, 4 * (1 - ph) + 1, (220, 240, 255))
        seq.append((fr, 80))
    return seq


def a_water_plant():
    seq = []
    for k in range(20):
        t = k / 20
        fr = Frame()
        pair(fr, dx=-14, dy=-16, h=EH * 0.6, w=EW * 0.8)
        rrect(fr, 140, 128, 176, 150, 3, BROWN)
        g = 1 + 0.25 * t
        poly(fr, [(158, 128), (158, 128 - 22 * g)], GRN, width=3)
        blob(fr, 150, 128 - 14 * g, 8 * g, 4 * g, GRN); blob(fr, 167, 128 - 20 * g, 8 * g, 4 * g, GRN)
        rrect(fr, 92, 92, 120, 112, 5, (120, 160, 200))
        poly(fr, [(120, 98), (138, 90)], (120, 160, 200), width=4)
        for i in range(3):
            ph = (t * 3 + i / 3) % 1
            blob(fr, 140 + 6 * ph, 92 + 30 * ph, 2, 3, (90, 170, 255))
        seq.append((fr, 100))
    return seq


def a_workout():
    seq = []
    for k in range(16):
        up = abs(math.sin(math.pi * k / 4))
        fr = Frame()
        pair(fr, dy=-10 * up + 6, h=EH * (0.7 + 0.15 * up), w=EW * 0.9, lid=(0.25, 0.25))
        y = 136 - 30 * up
        poly(fr, [(70, y), (156, y)], (180, 180, 190), width=5)
        for x in (66, 160):
            rrect(fr, x - 6, y - 12, x + 6, y + 12, 3, (90, 90, 100))
        if k % 8 == 4:
            tear(fr, 196, 40, 5, (120, 200, 255))
        seq.append((fr, 100))
    return seq


def a_sweep():
    seq = []
    for k in range(16):
        sx = 20 * math.sin(math.pi * k / 4)
        fr = Frame()
        pair(fr, dx=sx * 0.4, dy=-14, h=EH * 0.6, w=EW * 0.85)
        poly(fr, [(130 + sx, 92), (110 + sx, 136)], BROWN, width=4)
        poly(fr, [(96 + sx, 136), (126 + sx, 136), (122 + sx, 150), (100 + sx, 150)], (230, 200, 90))
        for i in range(4):
            blob(fr, 70 + i * 28 + (k * 3 + i * 7) % 12, 146, 2, 2, (150, 150, 150))
        seq.append((fr, 100))
    return seq


def a_goodnight():
    seq = []
    for k in range(20):
        t = k / 20
        f = min(1, t * 1.6)
        fr = Frame()
        if f < 0.95:
            pair(fr, dy=8 * f, h=EH * 0.8, lid=(0.2 + 0.75 * f, 0.2 + 0.75 * f))
        else:
            line_eye(fr, LX, CY + 14, bend=-8); line_eye(fr, RX, CY + 14, bend=-8)
        m = fr.mask(); d = ImageDraw.Draw(m)
        d.ellipse([sc(186), sc(8), sc(214), sc(36)], fill=255); d.ellipse([sc(194), sc(4), sc(222), sc(32)], fill=0)
        fr.add(m, (255, 230, 150))
        for i, (x, y) in enumerate(((20, 20), (50, 36), (160, 14))):
            star(fr, x, y, 3 + 2 * math.sin(2 * math.pi * (t + i / 3)), YEL)
        seq.append((fr, 130))
    return seq


def a_sunrise():
    seq = []
    for k in range(18):
        t = k / 18
        fr = Frame()
        sy = 150 - 50 * ease(t)
        blob(fr, 113, sy, 26, 26, (255, 200, 60))
        for i in range(8):
            a = math.pi * i / 4 + t
            poly(fr, [(113 + 32 * math.cos(a), sy + 32 * math.sin(a)), (113 + 42 * math.cos(a), sy + 42 * math.sin(a))], (255, 200, 60), width=3)
        o = ease(min(1, t * 1.5))
        if o < 0.2:
            line_eye(fr, LX, CY - 20, bend=-8); line_eye(fr, RX, CY - 20, bend=-8)
        else:
            pair(fr, dy=-24, h=EH * 0.5 * o + 4, w=EW * 0.8)
        seq.append((fr, 110))
    return seq


def a_invite():
    seq = []
    for k in range(16):
        b = abs(math.sin(math.pi * k / 4))
        fr = Frame()
        pair(fr, dy=-8 - 6 * b, w=EW * 1.05, h=EH * 0.9, r=26)
        glyph(fr, 'Zagramy?', 113, 136, 18, YEL, bold=True)
        glyph(fr, '?', 206, 30 - 6 * b, 24, YEL, bold=True)
        seq.append((fr, 90))
    return seq


def a_game_wait():
    seq = []
    for k in range(10):
        fr = Frame()
        pair(fr, h=EH * 0.6, lid=(0.3, 0.3), dy=-6)
        blob(fr, 113, 134, 10 + (k % 2), 10 + (k % 2), RED2)
        glyph(fr, 'czekaj...', 113, 18, 14, WHITE)
        seq.append((fr, 120))
    return seq


def a_game_go():
    seq = []
    for k in range(6):
        fr = Frame()
        pair(fr, w=EW * 1.1, h=EH * 1.1, r=30, dy=-10, color=GRN)
        glyph(fr, 'TERAZ!', 113, 136, 22 + (k % 2) * 2, GRN, bold=True)
        seq.append((fr, 80))
    return seq


def a_game_tap():
    seq = []
    for k in range(8):
        fr = Frame()
        pair(fr, dx=6 * math.sin(math.pi * k / 2), h=EH * 0.85, dy=-8)
        glyph(fr, 'STUKAJ!', 113, 136, 20, ORANGE, bold=True)
        seq.append((fr, 70))
    return seq


def a_tickle():
    seq = []
    for k in range(12):
        sx = 5 * math.sin(math.pi * k / 1.5)
        fr = Frame()
        line_eye(fr, LX + sx, CY, bend=16); line_eye(fr, RX + sx, CY, bend=16)
        blob(fr, LX - 14, CY + 36, 15, 6, PINK); blob(fr, RX + 14, CY + 36, 15, 6, PINK)
        glyph(fr, 'hihi', 40 + 4 * sx, 30, 16, YEL, bold=True); glyph(fr, 'haha', 180 - 4 * sx, 128, 16, YEL, bold=True)
        seq.append((fr, 70))
    return seq


def a_no_thanks():
    seq = []
    for k in range(14):
        sx = 16 * math.sin(math.pi * k / 3.5) if k < 10 else 0
        fr = Frame()
        pair(fr, dx=sx, h=EH * 0.7, lid=(0.35, 0.35))
        glyph(fr, 'nie, dzieki', 113, 140, 14, WHITE)
        seq.append((fr, 80 if k < 10 else 250))
    return seq


def dice_face(n):
    pips = {1: [(0, 0)], 2: [(-1, -1), (1, 1)], 3: [(-1, -1), (0, 0), (1, 1)], 4: [(-1, -1), (1, -1), (-1, 1), (1, 1)],
            5: [(-1, -1), (1, -1), (0, 0), (-1, 1), (1, 1)], 6: [(-1, -1), (1, -1), (-1, 0), (1, 0), (-1, 1), (1, 1)]}
    def f():
        seq = []
        for k in range(8):
            fr = Frame()
            rot = 0 if k > 2 else (3 - k) * 6
            rrect(fr, 73 + rot, 35, 153 - rot, 115, 14, WHITE)
            for px, py in pips[n]:
                blob(fr, 113 + px * 22, 75 + py * 22, 8, 8, (30, 30, 40))
            glyph(fr, str(n), 113, 136, 18, YEL, bold=True)
            seq.append((fr, 60 if k < 3 else 400))
        return seq
    return f


def coin(side):
    def f():
        seq = []
        for k in range(10):
            fr = Frame()
            w = 44 * abs(math.cos(math.pi * k / 4)) if k < 6 else 44
            blob(fr, 113, 70, max(3, w), 44, (240, 200, 70))
            if k >= 6:
                glyph(fr, 'O' if side == 'heads' else '1', 113, 70, 40, (150, 110, 20), bold=True)
                glyph(fr, 'ORZEL' if side == 'heads' else 'RESZKA', 113, 136, 18, YEL, bold=True)
            seq.append((fr, 70 if k < 6 else 400))
        return seq
    return f


def rps(kind):
    def f():
        seq = []
        for k in range(8):
            fr = Frame()
            bob = 6 * abs(math.sin(math.pi * k / 2)) if k < 4 else 0
            if kind == 'rock':
                blob(fr, 113, 72 - bob, 34, 30, (160, 160, 170)); blob(fr, 100, 64 - bob, 8, 6, (200, 200, 210))
                label = 'KAMIEN'
            elif kind == 'paper':
                rrect(fr, 83, 34 - bob, 143, 110 - bob, 4, WHITE)
                for i in range(4):
                    poly(fr, [(92, 50 + i * 14 - bob), (134, 50 + i * 14 - bob)], (150, 150, 170), width=2)
                label = 'PAPIER'
            else:
                for a in (-0.5, 0.5):
                    poly(fr, [(113, 80 - bob), (113 + 50 * math.sin(a), 80 - 50 * math.cos(a) - bob)], (200, 210, 220), width=6)
                for x in (96, 130):
                    m = fr.mask(); ImageDraw.Draw(m).ellipse([sc(x - 11), sc(88 - bob), sc(x + 11), sc(110 - bob)], outline=255, width=sc(4)); fr.add(m, RED2)
                label = 'NOZYCE'
            glyph(fr, label, 113, 136, 18, YEL, bold=True)
            seq.append((fr, 80 if k < 4 else 400))
        return seq
    return f


def a_trophy():
    seq = []
    for k in range(12):
        fr = Frame()
        pair(fr, dy=-20, arc=True, h=EH * 0.6, w=EW * 0.85)
        poly(fr, [(96, 96), (130, 96), (126, 120), (100, 120)], (255, 200, 50))
        rrect(fr, 106, 120, 120, 132, 2, (255, 200, 50)); rrect(fr, 98, 132, 128, 140, 2, (200, 150, 40))
        star(fr, 60 + (k * 13) % 110, 20 + (k * 7) % 30, 5, WHITE); star(fr, 180 - (k * 11) % 120, 30, 4, YEL)
        glyph(fr, 'REKORD!', 113, 18 if k % 2 else 20, 14, YEL, bold=True)
        seq.append((fr, 90))
    return seq


def a_party():
    seq = []
    cols = [PINK, YEL, GRN, (120, 160, 255), ORANGE]
    for k in range(16):
        fr = Frame()
        pair(fr, dx=12 * math.sin(math.pi * k / 4), dy=-6 * abs(math.sin(math.pi * k / 2)), arc=True, h=EH * 0.85, color=cols[k % 5])
        for i in range(6):
            blob(fr, 20 + i * 38, 140 - ((k * 9 + i * 23) % 40), 3, 3, cols[(k + i) % 5])
        seq.append((fr, 90))
    return seq


# ---------- pakiet 4: hobby i drobne przygody ----------
SKY = (120, 180, 255)
PURPLE = (170, 120, 255)


def look_at(fr, tx, ty, h=EH * 0.85, w=EW, **kw):
    pair(fr, dx=(tx - 113) * 0.12, dy=(ty - 76) * 0.15, h=h, w=w, **kw)


def a_juggle():
    seq = []
    cols = [RED2, YEL, GRN]
    for k in range(18):
        t = k / 18
        fr = Frame()
        pair(fr, dy=-14, h=EH * 0.6, w=EW * 0.85)
        for i in range(3):
            p = (t + i / 3) % 1
            x = 70 + 86 * p if p < 0.5 else 156 - 86 * (p - 0.5) * 2 * 0.5 * 2
            x = 70 + 86 * (2 * p if p < 0.5 else 2 - 2 * p)
            y = 140 - 70 * math.sin(math.pi * (2 * p % 1))
            blob(fr, x, y, 8, 8, cols[i])
        seq.append((fr, 70))
    return seq


def a_bubbles():
    seq = []
    for k in range(18):
        t = k / 18
        fr = Frame()
        pair(fr, dx=-10, dy=-10, h=EH * 0.7, lid=(0.25, 0.25))
        blob(fr, 120, 128, 4, 4, PINK)
        for i in range(4):
            p = (t + i / 4) % 1
            r = 4 + 12 * p
            x = 130 + 70 * p + 8 * math.sin(6 * p + i)
            y = 124 - 100 * p
            m = fr.mask(); ImageDraw.Draw(m).ellipse([sc(x - r), sc(y - r), sc(x + r), sc(y + r)], outline=255, width=sc(2)); fr.add(m, tuple(int(c * (1 - p * 0.6)) for c in SKY))
        seq.append((fr, 90))
    return seq


def a_paint():
    seq = []
    cols = [RED2, YEL, SKY, GRN]
    for k in range(20):
        t = k / 20
        fr = Frame()
        look_at(fr, 113, 130, lid=(0.2, 0.2))
        rrect(fr, 70, 112, 156, 150, 3, (235, 230, 215))
        n = int(t * 4) + 1
        for i in range(n):
            poly(fr, [(76, 120 + i * 8), (76 + (78 if i < n - 1 else 78 * ((t * 4) % 1)), 120 + i * 8)], cols[i], width=5)
        bx = 76 + 78 * ((t * 4) % 1); by = 120 + (n - 1) * 8
        poly(fr, [(bx, by), (bx + 18, by - 26)], BROWN, width=3)
        seq.append((fr, 90))
    return seq


def a_guitar():
    seq = []
    for k in range(16):
        t = k / 16
        fr = Frame()
        sway = 6 * math.sin(2 * math.pi * t)
        line_eye(fr, LX + sway, CY - 8, bend=12); line_eye(fr, RX + sway, CY - 8, bend=12)
        blob(fr, 100, 128, 22, 16, (200, 120, 50)); blob(fr, 100, 128, 6, 6, (60, 30, 10))
        poly(fr, [(118, 122), (180, 104)], (150, 90, 40), width=6)
        for i in range(3):
            v = 1.5 * math.sin(k * 3 + i) if k % 2 else 0
            poly(fr, [(84, 124 + i * 3 + v), (180, 102 + i * 3)], (230, 230, 230), width=1)
        glyph(fr, '♪', 200, 60 - 30 * ((t * 2) % 1), 18, YEL)
        seq.append((fr, 90))
    return seq


def a_drums():
    seq = []
    for k in range(16):
        hit = k % 2
        fr = Frame()
        pair(fr, dy=-14 + 4 * hit, h=EH * 0.65, w=EW * 0.85, lid=(0.3, 0.3))
        blob(fr, 80, 136, 24, 8, RED2); blob(fr, 146, 136, 24, 8, SKY)
        a = -0.6 if hit else -1.0
        for x, s in ((80, 1), (146, -1)):
            poly(fr, [(x + s * 20, 136 - 34 - (0 if hit else 10)), (x + s * 2, 132 - (0 if hit else 14))], (230, 200, 150), width=3)
        if hit:
            star(fr, 80 if k % 4 == 1 else 146, 118, 7, WHITE)
        seq.append((fr, 90))
    return seq


def a_cook():
    seq = []
    for k in range(18):
        t = k / 18
        fr = Frame()
        look_at(fr, 113, 140, h=EH * 0.7)
        poly(fr, [(150, 138), (190, 128)], (80, 80, 90), width=5)
        m = fr.mask(); ImageDraw.Draw(m).chord([sc(70), sc(118), sc(156), sc(158)], 0, 180, fill=255); fr.add(m, (90, 90, 100))
        jump = 16 * abs(math.sin(math.pi * t * 2)) if 0.3 < t < 0.8 else 0
        blob(fr, 113, 136 - jump, 16, 6, WHITE); blob(fr, 113, 135 - jump, 6, 5, (255, 200, 40))
        steam(fr, 113, 122, t)
        seq.append((fr, 90))
    return seq


def a_selfie():
    seq = []
    for k in range(14):
        fr = Frame()
        if k in (8, 9):
            m = fr.mask(); ImageDraw.Draw(m).rectangle([0, 0, sc(W), sc(H)], fill=255); fr.add(m, (255, 255, 255))
            seq.append((fr, 60))
            continue
        if k < 8:
            pair(fr, dx=-8, dy=-4, h=EH * 0.85, lid=(0.15, 0.15))
            rrect(fr, 150, 88, 196, 146, 6, (60, 60, 70)); blob(fr, 173, 104, 8, 8, SKY)
            glyph(fr, str(3 - k // 3) if k < 8 else '', 173, 132, 14, WHITE, bold=True)
        else:
            pair(fr, arc=True, h=EH * 0.85, dy=4)
            star(fr, 30, 30, 6, YEL); star(fr, 196, 36, 5, YEL)
        seq.append((fr, 300 if k < 8 else 160))
    return seq


def a_spy():
    seq = []
    for k in range(16):
        dx = 22 * math.sin(math.pi * k / 8)
        fr = Frame()
        pair(fr, dx=dx, h=EH * 0.35, w=EW * 1.05, r=10, dy=6)
        rrect(fr, 30 + dx, 52, 196 + dx, 70, 6, (40, 40, 50))
        glyph(fr, 'psst...', 113, 136, 14, (150, 150, 160))
        seq.append((fr, 130))
    return seq


def a_boo():
    seq = []
    for k in range(18):
        fr = Frame()
        if k < 8:
            x = -40 + 150 * ease(k / 7)
            m = fr.mask(); d = ImageDraw.Draw(m)
            d.ellipse([sc(x - 30), sc(30), sc(x + 30), sc(90)], fill=255); d.rectangle([sc(x - 30), sc(60), sc(x + 30), sc(120)], fill=255)
            fr.add(m, (230, 230, 240))
            blob(fr, x - 10, 58, 5, 7, (20, 20, 30)); blob(fr, x + 10, 58, 5, 7, (20, 20, 30))
            pair(fr, dx=40, w=EW * 0.7, h=EH * 0.7)
        elif k < 12:
            pair(fr, w=EW * 1.15, h=EH * 1.2, r=32, dy=-4)
            glyph(fr, 'BU!', 113, 136, 24, WHITE, bold=True)
        else:
            line_eye(fr, LX, CY, bend=16); line_eye(fr, RX, CY, bend=16)
            glyph(fr, 'haha', 113, 136, 16, YEL, bold=True)
        seq.append((fr, 80))
    return seq


def a_glitch():
    seq = []
    for k in range(14):
        fr = Frame()
        if k % 3 == 1:
            for i in range(5):
                y = 20 + i * 24 + (k * 7 % 10)
                rrect(fr, 30 + (k * 13 + i * 29) % 60, y, 120 + (k * 17 + i * 11) % 80, y + 8, 1, [CYAN, PINK, GRN][i % 3])
        else:
            off = (k % 4) * 6 - 9
            eye(fr, LX + off, CY, h=EH * 0.9, color=PINK); eye(fr, RX - off, CY, h=EH * 0.9, color=GRN)
            pair(fr, h=EH * 0.9)
        seq.append((fr, 70 if k % 3 == 1 else 110))
    return seq


def a_gum():
    seq = []
    for k in range(16):
        fr = Frame()
        pair(fr, dy=-14, h=EH * 0.55, w=EW * 0.85, lid=(0.15, 0.15))
        if k < 13:
            r = 6 + 2.4 * k
            blob(fr, 113, 130 - r * 0.2, r, r * 0.9, PINK)
            blob(fr, 113 - r * 0.4, 124 - r * 0.5, r * 0.15, r * 0.1, (255, 200, 230))
        else:
            for i in range(6):
                a = i * 1.05
                blob(fr, 113 + 30 * math.cos(a), 124 + 24 * math.sin(a), 5, 3, PINK)
            glyph(fr, 'PYK!', 196, 30, 14, WHITE, bold=True)
        seq.append((fr, 110))
    return seq


def a_magic():
    seq = []
    for k in range(18):
        t = k / 18
        fr = Frame()
        pair(fr, dy=-12, h=EH * 0.75, lid=(0.2, 0.05))
        a = -0.4 + 0.6 * math.sin(math.pi * t * 3)
        poly(fr, [(150, 140), (150 + 40 * math.cos(a - 1.2), 140 - 40 * math.sin(1.2 - a))], (40, 40, 40), width=5)
        tip = (150 + 40 * math.cos(a - 1.2), 140 - 40 * math.sin(1.2 - a))
        for i in range(5):
            p = (t * 2 + i / 5) % 1
            star(fr, tip[0] - 60 * p * (i % 2 * 2 - 1) * 0.6, tip[1] - 30 * p + 40 * p * p, 5 * (1 - p) + 1, [YEL, PINK, SKY, GRN, WHITE][i])
        if t > 0.6:
            blob(fr, 70, 132, 14, 10, WHITE); blob(fr, 64, 116, 3, 9, WHITE); blob(fr, 74, 116, 3, 9, WHITE)
        seq.append((fr, 90))
    return seq


def a_typing():
    seq = []
    for k in range(18):
        fr = Frame()
        pair(fr, dx=-12 + (k % 6) * 5, dy=-20, h=EH * 0.5, w=EW * 0.85, lid=(0.15, 0.15))
        rrect(fr, 62, 96, 164, 140, 4, (60, 60, 75))
        rrect(fr, 66, 100, 160, 136, 2, (20, 30, 40))
        for i in range(min(4, k // 4 + 1)):
            ln = 20 + (i * 37 + k * 7) % 60 if i == k // 4 else 20 + (i * 37) % 60
            poly(fr, [(72, 106 + i * 8), (72 + ln, 106 + i * 8)], GRN, width=2)
        rrect(fr, 56, 142, 170, 150, 2, (120, 120, 130))
        seq.append((fr, 90))
    return seq


def a_bird():
    seq = []
    for k in range(20):
        t = k / 20
        fr = Frame()
        if t < 0.35:
            bx, by = 240 - 140 * ease(t / 0.35), 20 + 30 * ease(t / 0.35)
        elif t < 0.75:
            bx, by = 100, 50
        else:
            bx, by = 100 - 140 * ease((t - 0.75) / 0.25), 50 - 40 * ease((t - 0.75) / 0.25)
        look_at(fr, bx, by, h=EH * 0.8)
        blob(fr, bx, by, 9, 7, SKY); blob(fr, bx - 7, by - 5, 5, 5, SKY)
        poly(fr, [(bx - 12, by - 5), (bx - 17, by - 3), (bx - 12, by - 2)], ORANGE)
        wing = 6 * math.sin(k * 2) if (t < 0.35 or t > 0.75) else 0
        blob(fr, bx + 2, by - 2 - wing, 6, 3, (80, 140, 230))
        if 0.4 < t < 0.7 and k % 2:
            glyph(fr, '♪', bx + 16, by - 18, 14, YEL)
        seq.append((fr, 100))
    return seq


def a_balloon():
    seq = []
    for k in range(20):
        t = k / 20
        fr = Frame()
        y = 110 - 140 * ease(max(0, t - 0.3) / 0.7)
        look_at(fr, 150, y, h=EH * (0.85 if t < 0.5 else 0.7), lid=None if t < 0.6 else (0.05, 0.4))
        blob(fr, 150, y, 16, 20, RED2)
        poly(fr, [(150, y + 20), (146 + 4 * math.sin(k), y + 50)], (200, 200, 200), width=1)
        seq.append((fr, 100))
    return seq


def a_rubik():
    seq = []
    cols = [RED2, GRN, SKY, YEL, ORANGE, WHITE]
    for k in range(16):
        fr = Frame()
        look_at(fr, 113, 130, h=EH * 0.7, lid=(0.3, 0.3))
        solved = k >= 13
        for i in range(3):
            for j in range(3):
                c = cols[0] if solved else cols[(i * 3 + j + k * (j + 1)) % 6]
                rrect(fr, 89 + i * 16, 104 + j * 16, 103 + i * 16, 118 + j * 16, 2, c)
        if solved:
            star(fr, 160, 100, 7, WHITE); star(fr, 70, 110, 5, YEL)
        seq.append((fr, 140 if not solved else 200))
    return seq


def a_rain():
    seq = []
    for k in range(18):
        t = k / 18
        fr = Frame()
        umbrella = t > 0.45
        if umbrella:
            pair(fr, arc=True, h=EH * 0.65, dy=10, w=EW * 0.85)
            m = fr.mask(); ImageDraw.Draw(m).chord([sc(40), sc(4), sc(186), sc(70)], 180, 360, fill=255); fr.add(m, PINK)
            poly(fr, [(113, 38), (113, 66)], (200, 200, 200), width=2)
        else:
            eye(fr, LX, CY + 10, h=EH * 0.6, lid=(0.5, 0.05)); eye(fr, RX, CY + 10, h=EH * 0.6, lid=(0.05, 0.5))
        for i in range(10):
            x = (i * 23 + k * 5) % 226
            y = (i * 37 + k * 18) % 150
            if umbrella and 40 < x < 186 and y < 80:
                continue
            poly(fr, [(x, y), (x - 2, y + 7)], SKY, width=2)
        seq.append((fr, 90))
    return seq


def a_snow():
    seq = []
    for k in range(18):
        fr = Frame()
        pair(fr, dy=-6, h=EH * 0.9, w=EW * 1.0, r=26)
        for i in range(9):
            x = (i * 29 + 6 * math.sin(k * 0.5 + i)) % 226
            y = (i * 19 + k * 8) % 150
            star(fr, x, y, 3 + (i % 3), WHITE)
        seq.append((fr, 110))
    return seq


def a_kite():
    seq = []
    for k in range(18):
        t = k / 18
        kx = 160 + 30 * math.sin(2 * math.pi * t); ky = 30 + 12 * math.sin(4 * math.pi * t)
        fr = Frame()
        look_at(fr, kx, ky, h=EH * 0.85)
        poly(fr, [(kx, ky - 16), (kx + 12, ky), (kx, ky + 18), (kx - 12, ky)], ORANGE)
        poly(fr, [(kx, ky + 18), (113, 150)], (200, 200, 200), width=1)
        for i in range(3):
            blob(fr, kx - 4 + 3 * math.sin(k + i), ky + 26 + i * 8, 3, 2, PINK)
        seq.append((fr, 100))
    return seq


def a_fishing():
    seq = []
    for k in range(20):
        t = k / 20
        fr = Frame()
        bite = t > 0.6
        pair(fr, dx=10, dy=-10 - (6 if bite else 0), h=EH * (0.9 if bite else 0.6), lid=None if bite else (0.3, 0.3))
        blob(fr, 113, 146, 100, 8, (40, 90, 170))
        poly(fr, [(40, 150), (150, 70)], BROWN, width=3)
        fy = 140 - (30 * (t - 0.6) / 0.4 if bite else 2 * math.sin(k))
        poly(fr, [(150, 70), (170, fy)], (200, 200, 200), width=1)
        if bite:
            blob(fr, 170, fy + 8, 12, 6, ORANGE); poly(fr, [(182, fy + 8), (190, fy + 2), (190, fy + 14)], ORANGE)
            glyph(fr, '!', 200, 30, 22, YEL, bold=True)
        seq.append((fr, 110))
    return seq


def a_hula():
    seq = []
    for k in range(16):
        a = 2 * math.pi * k / 8
        fr = Frame()
        pair(fr, dx=6 * math.sin(a), dy=-16, h=EH * 0.6, arc=True, w=EW * 0.85)
        m = fr.mask(); d = ImageDraw.Draw(m)
        cx = 113 + 14 * math.cos(a)
        d.ellipse([sc(cx - 70), sc(118), sc(cx + 70), sc(142)], outline=255, width=sc(5)); fr.add(m, PURPLE)
        seq.append((fr, 70))
    return seq


def a_wish():
    seq = []
    for k in range(20):
        t = k / 20
        fr = Frame()
        pair(fr, dy=10, h=EH * 0.7, w=EW * 0.9, r=24) if t < 0.5 else (line_eye(fr, LX, CY + 10, bend=10), line_eye(fr, RX, CY + 10, bend=10))
        sx = 220 - 220 * ((t * 1.5) % 1); sy = 10 + 30 * ((t * 1.5) % 1)
        star(fr, sx, sy, 6, YEL)
        for i in range(1, 5):
            blob(fr, sx + i * 8, sy - i * 1.2, 2.5 - i * 0.4, 2.5 - i * 0.4, (255, 230, 150))
        if t > 0.5:
            heart(fr, 113, 136, 16, PINK)
        seq.append((fr, 110))
    return seq


def a_flower():
    seq = []
    for k in range(18):
        t = k / 18
        fr = Frame()
        sniff = (k // 3) % 2
        if t < 0.75:
            pair(fr, dy=-10, h=EH * (0.55 if sniff else 0.7), w=EW * 0.85, lid=(0.25, 0.25))
        else:
            line_eye(fr, LX, CY - 6, bend=14); line_eye(fr, RX, CY - 6, bend=14)
            heart(fr, 180, 40, 14, PINK)
        poly(fr, [(113, 150), (113, 118)], GRN, width=3)
        for i in range(6):
            a = i * math.pi / 3 + k * 0.1
            blob(fr, 113 + 9 * math.cos(a), 112 + 9 * math.sin(a), 6, 6, PINK)
        blob(fr, 113, 112, 5, 5, YEL)
        seq.append((fr, 110))
    return seq


def a_clock():
    seq = []
    for k in range(16):
        a = 2 * math.pi * k / 16
        fr = Frame()
        eye(fr, LX - 14 + 8 * math.sin(a), CY - 10, h=EH * 0.6, w=EW * 0.7)
        m = fr.mask(); d = ImageDraw.Draw(m)
        d.ellipse([sc(158), sc(96), sc(206), sc(144)], outline=255, width=sc(3)); fr.add(m, WHITE)
        poly(fr, [(182, 120), (182 + 18 * math.sin(a), 120 - 18 * math.cos(a))], RED2, width=2)
        poly(fr, [(182, 120), (194, 120)], WHITE, width=3)
        eye(fr, RX - 46 + 8 * math.sin(a), CY - 10, h=EH * 0.6, w=EW * 0.7)
        glyph(fr, 'tik' if k % 2 == 0 else 'tak', 60, 136, 14, (170, 170, 180))
        seq.append((fr, 120))
    return seq


def a_love():
    seq = []
    for k in range(14):
        s = 1 + 0.15 * abs(math.sin(math.pi * k / 3.5))
        fr = Frame()
        heart(fr, LX, CY, 50 * s, PINK); heart(fr, RX, CY, 50 * s, PINK)
        for i in range(3):
            p = (k / 14 + i / 3) % 1
            heart(fr, 30 + i * 80, 150 - 140 * p, 10, (255, 120, 180))
        seq.append((fr, 90))
    return seq


def a_charge():
    seq = []
    for k in range(18):
        lvl = min(1, k / 14)
        fr = Frame()
        line_eye(fr, LX, CY - 16, bend=-8); line_eye(fr, RX, CY - 16, bend=-8)
        rrect(fr, 80, 104, 140, 136, 4, (200, 200, 210), outline=3)
        rrect(fr, 140, 112, 146, 128, 1, (200, 200, 210))
        col = RED2 if lvl < 0.3 else (YEL if lvl < 0.7 else GRN)
        rrect(fr, 84, 108, 84 + 52 * lvl, 132, 2, col)
        glyph(fr, '⚡' if k % 2 else '', 170, 120, 18, YEL)
        seq.append((fr, 110))
    return seq


# ---------- pakiet 5 ----------
def a_ice_cream():
    seq = []
    for k in range(16):
        t = k / 16
        fr = Frame()
        pair(fr, dy=-16, arc=True, h=EH * 0.6, w=EW * 0.85)
        poly(fr, [(100, 116), (126, 116), (113, 150)], (220, 170, 100))
        lick = int(t * 3)
        for i, c in enumerate((PINK, (250, 240, 220), (140, 90, 60))[:3 - lick // 2]):
            blob(fr, 113, 110 - i * 12, 14 - i, 11, c)
        seq.append((fr, 120))
    return seq


def a_movie():
    seq = []
    for k in range(18):
        fr = Frame()
        flick = (k % 3) * 0.08
        pair(fr, dy=-8, h=EH * (0.75 + flick), w=EW * 0.95, r=24)
        poly(fr, [(150, 150), (190, 150), (194, 116), (146, 116)], (230, 60, 60))
        for i in range(5):
            jump = 10 * abs(math.sin(k * 0.9 + i)) if (k + i) % 4 == 0 else 0
            blob(fr, 154 + i * 8, 112 - jump - (i % 2) * 4, 5, 5, (255, 240, 200))
        seq.append((fr, 110))
    return seq


def a_piano():
    seq = []
    for k in range(16):
        fr = Frame()
        sway = 5 * math.sin(math.pi * k / 4)
        line_eye(fr, LX + sway, CY - 10, bend=12); line_eye(fr, RX + sway, CY - 10, bend=12)
        rrect(fr, 50, 112, 176, 146, 3, WHITE)
        pressed = (k * 3) % 7
        for i in range(7):
            x = 50 + i * 18
            poly(fr, [(x, 112), (x, 146)], (120, 120, 130), width=1)
            if i == pressed:
                rrect(fr, x + 2, 114, x + 16, 144, 1, SKY)
        for i in (0, 1, 3, 4, 5):
            rrect(fr, 50 + i * 18 + 12, 112, 50 + i * 18 + 24, 130, 1, (30, 30, 40))
        glyph(fr, '♪', 30 + (k * 11) % 160, 40 - (k % 4) * 6, 14, YEL)
        seq.append((fr, 100))
    return seq


def a_butterfly():
    seq = []
    for k in range(20):
        t = k / 20
        bx = 113 + 80 * math.sin(2 * math.pi * t); by = 70 + 40 * math.sin(4 * math.pi * t)
        fr = Frame()
        look_at(fr, bx, by, h=EH * 0.85)
        f = abs(math.sin(k * 1.3))
        for s in (-1, 1):
            blob(fr, bx + s * 8 * f, by - 4, 8 * f + 1, 7, PURPLE)
            blob(fr, bx + s * 6 * f, by + 6, 6 * f + 1, 5, PINK)
        poly(fr, [(bx, by - 8), (bx, by + 10)], (40, 40, 40), width=2)
        seq.append((fr, 90))
    return seq


def a_planet():
    seq = []
    for k in range(18):
        a = 2 * math.pi * k / 18
        fr = Frame()
        px, py = 113 + 70 * math.cos(a), 66 + 30 * math.sin(a)
        look_at(fr, px, py, h=EH * 0.8)
        blob(fr, px, py, 12, 12, ORANGE)
        m = fr.mask(); ImageDraw.Draw(m).ellipse([sc(px - 22), sc(py - 5), sc(px + 22), sc(py + 5)], outline=255, width=sc(2)); fr.add(m, YEL)
        star(fr, 20, 20, 3, WHITE); star(fr, 200, 130, 3, WHITE)
        seq.append((fr, 100))
    return seq


def a_tea():
    seq = []
    for k in range(18):
        t = k / 18
        fr = Frame()
        pair(fr, dy=-14, h=EH * 0.55, w=EW * 0.85, lid=(0.35, 0.35))
        m = fr.mask(); ImageDraw.Draw(m).chord([sc(88), sc(104), sc(138), sc(150)], 0, 180, fill=255); fr.add(m, WHITE)
        m = fr.mask(); ImageDraw.Draw(m).ellipse([sc(134), sc(118), sc(150), sc(134)], outline=255, width=sc(3)); fr.add(m, WHITE)
        blob(fr, 113, 128, 22, 3, (190, 120, 60))
        steam(fr, 113, 120, t)
        seq.append((fr, 120))
    return seq


def a_yoga():
    seq = []
    for k in range(18):
        t = k / 18
        tilt = 14 * math.sin(2 * math.pi * t)
        fr = Frame()
        eye(fr, LX, CY + tilt, h=EH * 0.7, lid=(0.4, 0.4))
        eye(fr, RX, CY - tilt, h=EH * 0.7, lid=(0.4, 0.4))
        if 0.4 < t < 0.6:
            glyph(fr, 'wdech...', 113, 140, 14, (170, 150, 255))
        elif t >= 0.8:
            glyph(fr, 'wydech', 113, 140, 14, (170, 150, 255))
        seq.append((fr, 140))
    return seq


def a_trampoline():
    seq = []
    for k in range(14):
        t = k / 14
        h = abs(math.sin(math.pi * t * 2))
        fr = Frame()
        squash = 1 - 0.3 * max(0, 0.2 - h) * 5
        pair(fr, dy=30 - 60 * h, h=EH * 0.7 * squash, w=EW * (1.25 - 0.25 * squash), arc=h > 0.6)
        poly(fr, [(40, 146 - 6 * max(0, 0.2 - h) * 5), (186, 146 - 6 * max(0, 0.2 - h) * 5)], PURPLE, width=4)
        seq.append((fr, 70))
    return seq


def a_mirror():
    seq = []
    for k in range(16):
        fr = Frame()
        m = fr.mask(); ImageDraw.Draw(m).rounded_rectangle([sc(132), sc(14), sc(214), sc(140)], radius=sc(10), outline=255, width=sc(3)); fr.add(m, (200, 200, 220))
        eye(fr, 60, CY, h=EH * 0.7, w=EW * 0.7)
        if 6 <= k <= 9:
            line_eye(fr, 104, CY + 4, w=EW * 0.6, bend=10)
        else:
            eye(fr, 104, CY, h=EH * 0.7, w=EW * 0.7)
        eye(fr, 158, CY, h=EH * 0.45, w=EW * 0.45, color=(120, 160, 200))
        eye(fr, 190, CY, h=EH * 0.45, w=EW * 0.45, color=(120, 160, 200))
        seq.append((fr, 140))
    return seq


def a_wave_hi():
    seq = []
    for k in range(14):
        a = 0.5 * math.sin(math.pi * k / 3.5)
        fr = Frame()
        pair(fr, dx=-14, dy=-6, arc=True, h=EH * 0.85, w=EW * 0.9)
        cx, cy = 186, 112
        hx, hy = cx + 26 * math.sin(a), cy - 26 * math.cos(a)
        poly(fr, [(cx, cy + 30), (hx, hy)], CYAN, width=6)
        blob(fr, hx, hy, 10, 10, CYAN)
        glyph(fr, 'Czesc!', 70, 138, 16, YEL, bold=True)
        seq.append((fr, 100))
    return seq

ANIMS = {
    'wave_hi': a_wave_hi,
    'trampoline': a_trampoline, 'mirror': a_mirror,
    'ice_cream': a_ice_cream, 'movie': a_movie, 'piano': a_piano, 'butterfly': a_butterfly,
    'planet': a_planet, 'tea': a_tea, 'yoga': a_yoga,
    'juggle': a_juggle, 'bubbles': a_bubbles, 'paint': a_paint, 'guitar': a_guitar,
    'drums': a_drums, 'cook': a_cook, 'selfie': a_selfie, 'spy': a_spy, 'boo': a_boo,
    'glitch': a_glitch, 'gum': a_gum, 'magic': a_magic, 'typing': a_typing, 'bird': a_bird,
    'balloon': a_balloon, 'rubik': a_rubik, 'rain': a_rain, 'snow': a_snow, 'kite': a_kite,
    'fishing': a_fishing, 'hula': a_hula, 'wish': a_wish, 'flower': a_flower, 'clock': a_clock,
    'love': a_love, 'charge': a_charge,
    'breakfast': lambda: meal(f_breakfast), 'snack_apple': lambda: meal(f_apple, n=18),
    'lunch': lambda: meal(f_soup), 'cake': lambda: meal(f_cake, n=16), 'supper': lambda: meal(f_sandwich, n=18),
    'drink': a_drink, 'brush_teeth': a_brush, 'water_plant': a_water_plant, 'workout': a_workout,
    'sweep': a_sweep, 'goodnight': a_goodnight, 'sunrise': a_sunrise, 'invite': a_invite,
    'game_wait': a_game_wait, 'game_go': a_game_go, 'game_tap': a_game_tap, 'tickle': a_tickle,
    'no_thanks': a_no_thanks, 'trophy': a_trophy, 'party': a_party,
    'dice_1': dice_face(1), 'dice_2': dice_face(2), 'dice_3': dice_face(3), 'dice_4': dice_face(4),
    'dice_5': dice_face(5), 'dice_6': dice_face(6), 'coin_heads': coin('heads'), 'coin_tails': coin('tails'),
    'rps_rock': rps('rock'), 'rps_paper': rps('paper'), 'rps_scissors': rps('scissors'),
    'dance': a_dance, 'sneeze': a_sneeze, 'hiccup': a_hiccup, 'daydream': a_daydream,
    'stretch': a_stretch, 'read': a_read, 'peekaboo': a_peekaboo, 'chase_fly': a_chase_fly,
    'dizzy': a_dizzy, 'whistle': a_whistle, 'meditate': a_meditate, 'doze': a_doze,
    'proud': a_proud, 'count_sheep': a_count_sheep, 'curious': a_curious,

    'look_left': lambda: look(-26, 0), 'look_right': lambda: look(26, 0),
    'look_up': lambda: look(0, -18, squint=None), 'look_down': lambda: look(0, 14),
    'yawn': a_yawn, 'nap': a_nap, 'eat': a_eat, 'play': a_play, 'sing': a_sing,
    'pet': a_pet, 'bored': a_bored, 'hungry': a_hungry,

    'neutral': a_neutral, 'happy': a_happy, 'laughing': a_laughing, 'funny': a_funny,
    'sad': a_sad, 'angry': a_angry, 'crying': lambda: a_sad(tears=True), 'loving': a_loving,
    'embarrassed': a_embarrassed, 'surprised': a_surprised, 'shocked': a_shocked,
    'thinking': a_thinking, 'winking': a_winking, 'cool': a_cool, 'relaxed': a_relaxed,
    'delicious': lambda: a_happy(sparkle=True), 'kissy': a_kissy, 'confident': a_confident,
    'sleepy': a_sleepy, 'silly': a_silly, 'confused': a_confused,
}


def save_gif(name, seq):
    frames = [fr.render() for fr, _ in seq]
    durs = [ms for _, ms in seq]
    pal = [f.convert('P', palette=Image.ADAPTIVE, colors=int(os.environ.get('GIF_COLORS', '48'))) for f in frames]
    path = os.path.join(OUT, f'{name}.gif')
    pal[0].save(path, save_all=True, append_images=pal[1:], duration=durs, loop=0, optimize=True, disposal=1)
    return path, frames


if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    import sys
    only = set(sys.argv[1:])
    for name, fn in ANIMS.items():
        if only and name not in only:
            continue
        path, frames = save_gif(name, fn())
        print(f'{name:12} {len(frames):3} klatek {os.path.getsize(path) // 1024:4} KB')
