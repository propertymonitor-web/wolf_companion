#!/usr/bin/env python3
"""Generate src/WolfArt_butterfly.h: silhouette parts for the butterfly's life cycle.

Stages
  caterpillar  (0-6 months)   side view, facing right: crawl frames, rearing, curled up
  chrysalis    (6-9 months)   hangs from a silk thread; three frames (still, tilted left, tilted right)
  butterfly    (9+ months)    seen from the front: wings flap through 4 frames, three wing shapes,
                              three antenna styles.  Young butterflies (< 24 months) are smaller.

Every part is a 1-bit silhouette (set bit = part of the insect).  The firmware unions the parts,
draws an outline and fills the inside with a pattern chosen by the pet's genes.
Props (flower, leaf, ladybug) are drawn lit as they are.
"""
import math, os, sys
from PIL import Image, ImageDraw

SS = 8


def canvas(w, h):
    im = Image.new("L", (w * SS, h * SS), 0)
    return im, ImageDraw.Draw(im)


def S(v):
    return v * SS


def poly(d, pts, fill=255):
    d.polygon([(S(x), S(y)) for x, y in pts], fill=fill)


def ell(d, cx, cy, rx, ry, fill=255):
    d.ellipse((S(cx - rx), S(cy - ry), S(cx + rx), S(cy + ry)), fill=fill)


def fin(im, w, h):
    return im.resize((w, h), Image.BOX).point(lambda v: 255 if v >= 112 else 0)


def pack(im):
    """Crop to bbox -> (bytes, x, y, w, h), rows MSB-first."""
    bb = im.getbbox()
    if not bb:
        return (bytes([0]), 0, 0, 1, 1)
    x0, y0, x1, y1 = bb
    w, h = x1 - x0, y1 - y0
    stride = (w + 7) // 8
    px = im.load()
    out = bytearray()
    for y in range(h):
        for bx in range(stride):
            b = 0
            for bit in range(8):
                x = bx * 8 + bit
                if x < w and px[x0 + x, y0 + y] >= 128:
                    b |= 0x80 >> bit
            out.append(b)
    return (bytes(out), x0, y0, w, h)


def c_array(name, data):
    rows = ",\n".join("  " + ", ".join("0x%02X" % b for b in data[i:i + 16]) for i in range(0, len(data), 16))
    return "static const uint8_t %s[] = {\n%s\n};\n" % (name, rows)


# ------------------------------------------------------------ butterfly ---
# canvas: axis at x = AX, thorax at y = TY.  Right-hand wings in design units (x outward, y down).
BW, BH = 46, 40
AX, TY = 23.0, 15.0
FORE = [
    [(1, -1), (4, -9), (11, -15), (19, -14), (21, -8), (17, -2), (9, 1)],            # 0 rounded
    [(1, -1), (3, -8), (9, -14), (17, -15), (21, -10), (19, -3), (10, 1)],           # 1 broad
    [(1, -1), (5, -10), (13, -17), (22, -13), (20, -6), (14, -2), (7, 1)],           # 2 pointed
]
HIND = [
    [(1, 2), (8, 2), (15, 4), (18, 9), (17, 14), (14, 17), (15, 24), (12, 20), (8, 15), (3, 9)],   # 0 swallowtail
    [(1, 2), (9, 3), (16, 6), (18, 11), (14, 16), (8, 17), (3, 11)],                                # 1 round
    [(1, 2), (7, 3), (12, 6), (13, 10), (9, 14), (4, 11)],                                          # 2 small
]
FLAP = [1.0, 0.70, 0.38, 0.14]          # horizontal squash per flap frame


def wing_pair(pts, k, scale):
    """Both wings (right + mirrored left) squashed by k, as one silhouette."""
    im, d = canvas(BW, BH)
    for sgn in (1, -1):
        poly(d, [(AX + sgn * x * k * scale, TY + y * scale) for x, y in pts])
    return fin(im, BW, BH)


def body(scale):
    im, d = canvas(BW, BH)
    ell(d, AX, TY - 7 * scale, 2.4 * scale, 2.4 * scale)                         # head
    ell(d, AX, TY - 0.5 * scale, 2.0 * scale, 4.2 * scale)                      # thorax
    poly(d, [(AX - 1.7 * scale, TY + 3 * scale), (AX + 1.7 * scale, TY + 3 * scale),
             (AX + 1.1 * scale, TY + 16 * scale), (AX, TY + 17.5 * scale), (AX - 1.1 * scale, TY + 16 * scale)])
    return fin(im, BW, BH)


def antennae(style, scale):
    im, d = canvas(BW, BH)
    hy = TY - 8.5 * scale
    for sgn in (1, -1):
        if style == 0:            # straight V with a club
            pts = [(AX + sgn * 1.0, hy), (AX + sgn * 5.0 * scale, hy - 7 * scale)]
            d.line([(S(x), S(y)) for x, y in pts], fill=255, width=int(S(0.7)))
            ell(d, pts[1][0], pts[1][1], 1.1, 1.1)
        elif style == 1:          # long, drooping outwards
            pts = [(AX + sgn * 1.0, hy), (AX + sgn * 4 * scale, hy - 6 * scale), (AX + sgn * 8 * scale, hy - 5 * scale)]
            d.line([(S(x), S(y)) for x, y in pts], fill=255, width=int(S(0.7)))
        else:                     # short and feathery
            pts = [(AX + sgn * 1.0, hy), (AX + sgn * 3 * scale, hy - 5 * scale)]
            d.line([(S(x), S(y)) for x, y in pts], fill=255, width=int(S(0.9)))
            ell(d, pts[1][0], pts[1][1], 0.9, 0.9)
    return fin(im, BW, BH)


# ---------------------------------------------------------- caterpillar ---
CW, CH = 34, 20


def caterpillar(frame):
    """frame 0..3 = crawl (a hump travels down the body), 4 = rearing up, 5 = curled up."""
    im, d = canvas(CW, CH)
    base = CH - 3.2
    if frame == 5:                                   # curled into a ring
        ell(d, 15, base - 5, 7.5, 5.6)
        ell(d, 15, base - 5, 3.2, 2.2, 0)
        ell(d, 21.5, base - 2.5, 3.1, 2.8)           # head tucked in
        return fin(im, CW, CH)
    n = 8
    pts = []
    for i in range(n):
        x = 4.5 + i * 3.2
        hump = 0.0
        if frame <= 3:
            hump = 2.6 * max(0.0, math.sin((i / 2.2) - frame * math.pi / 2 + 0.4))
        y = base - 2.4 - hump
        if frame == 4 and i >= n - 3:                # front end raised
            y -= (i - (n - 4)) * 2.8
        pts.append((x, y))
    for (x, y) in pts:
        ell(d, x, y, 2.5, 2.4)
    hx, hy = pts[-1][0] + 2.6, pts[-1][1] - 0.4
    ell(d, hx, hy, 2.7, 2.6)                          # head
    for i in range(0, n, 2):                           # tiny legs
        x, y = pts[i]
        d.rectangle((S(x - 0.5), S(y + 1.6), S(x + 0.5), S(base + 1.2)), fill=255)
    return fin(im, CW, CH)


# ------------------------------------------------------------ chrysalis ---
YW, YH = 14, 22


def chrysalis(frame):
    im, d = canvas(YW, YH)
    tilt = (0, -1.6, 1.6)[frame]
    cx = 7 + tilt
    poly(d, [(cx - 0.5, 0), (cx + 0.5, 0), (7 + tilt * 0.3 + 0.5, 4), (7 + tilt * 0.3 - 0.5, 4)])        # silk thread
    ell(d, cx, 11, 3.8, 6.4)                                                                              # body
    poly(d, [(cx - 2, 15), (cx + 2, 15), (cx + tilt * 0.6, 21.5)])                                       # tapered tail
    ell(d, cx, 6.5, 2.3, 2.2)
    return fin(im, YW, YH)


# ----------------------------------------------------------------- props ---
def flower(frame):
    w, h = 13, 20
    im, d = canvas(w, h)
    d.rectangle((S(6), S(h - [6, 11, 12][frame]), S(7), S(h)), fill=255)                      # stem
    poly(d, [(6.5, h - 4), (2.5, h - 7), (5, h - 3)])                                           # leaf
    if frame == 0:
        ell(d, 6.5, h - 7, 1.4, 1.8)
    elif frame == 1:
        ell(d, 6.5, h - 12, 2.2, 2.4)
    else:
        for k in range(6):
            a = k * math.pi / 3
            ell(d, 6.5 + 3.5 * math.cos(a), 7 + 3.3 * math.sin(a), 1.9, 1.9)
        ell(d, 6.5, 7, 1.6, 1.6, 0)
        ell(d, 6.5, 7, 0.7, 0.7)
    return fin(im, w, h)


def leaf(frame):
    w, h = 14, 6
    im, d = canvas(w, h)
    keep = [1.0, 0.62, 0.3][frame]
    poly(d, [(0, 5.5), (2, 2), (6, 0.5), (11, 1.5), (13.5, 5.5)])
    if keep < 1.0:                                      # bites taken out of the right-hand end
        cutx = 13.5 * keep
        d.rectangle((S(cutx), 0, S(w), S(h)), fill=0)
        for yy in (1.0, 3.0, 5.0):
            ell(d, cutx, yy, 1.1, 1.0, 0)
    return fin(im, w, h)


def ladybug(frame):
    """0/1 crawling (legs), 2/3 flying (wings out).  Faces right."""
    w, h = 12, 8
    im, d = canvas(w, h)
    if frame < 2:
        ell(d, 5.0, 4.4, 4.4, 3.2)
        ell(d, 9.6, 4.6, 1.5, 1.4)
        for x in ((2.6, 5.0, 7.2) if frame == 0 else (3.2, 5.6, 7.8)):
            d.rectangle((S(x), S(6.5), S(x + 0.5), S(7.9)), fill=255)
        d.line((S(5.0), S(1.4), S(5.0), S(7.4)), fill=0, width=int(S(0.5)))              # wing-case seam
        ell(d, 3.2, 3.6, 0.55, 0.55, 0); ell(d, 6.8, 3.6, 0.55, 0.55, 0)             # spots
    else:
        ell(d, 5.0, 4.8, 3.2, 2.4)
        ell(d, 8.6, 4.8, 1.3, 1.2)
        y0 = 1.0 if frame == 2 else 3.0
        poly(d, [(4, 4), (0.5, y0), (2.5, y0 - 0.8 if y0 > 1 else 0), (6, 3.4)])
        poly(d, [(5.5, 4), (9, y0 + 0.6), (7, y0 - 0.4), (4.5, 3.4)])
    return fin(im, w, h)


# ---------------------------------------------------------------- output ---
def main(path):
    out = ["// GENERATED by tools/gen_insect_art.py -- do not edit by hand.\n#pragma once\n#include <stdint.h>\n\n"]
    out.append("struct InsPart { const uint8_t* bits; int8_t x, y; uint8_t w, h; };\n")
    out.append("#define INS_BW %d\n#define INS_BH %d\n#define INS_AX %d\n#define INS_TY %d\n" % (BW, BH, int(AX), int(TY)))
    out.append("#define INS_CW %d\n#define INS_CH %d\n#define INS_YW %d\n#define INS_YH %d\n\n" % (CW, CH, YW, YH))

    def emit(name, im):
        data, x, y, w, h = pack(im)
        out.append(c_array(name, data))
        return "{%s, %d, %d, %d, %d}" % (name, x, y, w, h)

    # butterfly: [scale][variant][frame] for fore + hind wings; body; antennae
    scales = [("young", 0.72), ("adult", 1.0)]
    names = {}
    for sname, sc in scales:
        body_p = emit("bf_%s_body" % sname, body(sc))
        ant = [emit("bf_%s_ant%d" % (sname, a), antennae(a, sc)) for a in range(3)]
        fore = [[emit("bf_%s_fore%d_%d" % (sname, v, f), wing_pair(FORE[v], FLAP[f], sc)) for f in range(4)] for v in range(3)]
        hind = [[emit("bf_%s_hind%d_%d" % (sname, v, f), wing_pair(HIND[v], FLAP[f], sc)) for f in range(4)] for v in range(3)]
        out.append("static const struct { InsPart body; InsPart ant[3]; InsPart fore[3][4]; InsPart hind[3][4]; } bf_%s = {\n" % sname)
        out.append("  %s,\n  {%s},\n" % (body_p, ", ".join(ant)))
        out.append("  {%s},\n" % ", ".join("{%s}" % ", ".join(r) for r in fore))
        out.append("  {%s}\n};\n\n" % ", ".join("{%s}" % ", ".join(r) for r in hind))

    cat = [emit("cat_f%d" % f, caterpillar(f)) for f in range(6)]
    out.append("static const InsPart cat_frames[6] = {%s};\n\n" % ", ".join(cat))
    chry = [emit("chry_f%d" % f, chrysalis(f)) for f in range(3)]
    out.append("static const InsPart chry_frames[3] = {%s};\n\n" % ", ".join(chry))

    # lit props (not outlined)
    def prop(name, im):
        w, h = im.size
        out.append(c_array(name, pack_full(im)))
        out.append("#define %s_W %d\n#define %s_H %d\n" % (name.upper(), w, name.upper(), h))

    for f in range(3):
        prop("flower%d" % f, flower(f))
    for f in range(3):
        prop("leaf%d" % f, leaf(f))
    for f in range(4):
        prop("lady%d_r" % f, ladybug(f))
        prop("lady%d_l" % f, ladybug(f).transpose(Image.FLIP_LEFT_RIGHT))
    open(path, "w").write("".join(out))
    print("wrote", path)


def pack_full(im):
    w, h = im.size
    stride = (w + 7) // 8
    px = im.load()
    out = bytearray()
    for y in range(h):
        for bx in range(stride):
            b = 0
            for bit in range(8):
                x = bx * 8 + bit
                if x < w and px[x, y] >= 128:
                    b |= 0x80 >> bit
            out.append(b)
    return bytes(out)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "WolfArt_butterfly.h"))
