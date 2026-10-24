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

ANIMS = {
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
    pal = [f.convert('P', palette=Image.ADAPTIVE, colors=48) for f in frames]
    path = os.path.join(OUT, f'{name}.gif')
    pal[0].save(path, save_all=True, append_images=pal[1:], duration=durs, loop=0, optimize=True, disposal=1)
    return path, frames


if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    for name, fn in ANIMS.items():
        path, frames = save_gif(name, fn())
        print(f'{name:12} {len(frames):3} klatek {os.path.getsize(path) // 1024:4} KB')
