#!/usr/bin/env python3
"""Generate src/WolfArt.h: silhouette parts for the wolf sprite rig.

Every part is a 1-bit *silhouette* (set bit = part of the wolf).  At runtime
the firmware unions the parts, draws an outline, fills the inside with a coat
pattern chosen by the wolf's genes, then adds eyes / nose / mouth.

Wolves are authored facing RIGHT in "design units" (adult, 64 x 40, ground at
y = 39).  Three growth stages are produced by scaling the design; the pup also
gets a proportionally bigger head and shorter legs so it reads as a pup.
"""
import math, sys, os
from PIL import Image, ImageDraw

SS = 6  # supersampling factor

DESIGN_W, DESIGN_H = 64, 44
GROUND = 43.0
HIP = 27.0                 # hip height used for the legs
NECK = (38.0, 18.0)        # pivot used for head scaling / rotation

# stage: (scale, head_scale, leg_scale)
STAGES = [
    ("pup",      0.62, 1.40, 0.82),
    ("juvenile", 0.78, 1.15, 0.94),
    ("adult",    0.92, 1.00, 1.00),
]

# ---------------------------------------------------------------- geometry --
BODY = [(8, 20), (11, 17), (18, 16), (26, 16), (34, 15), (40, 16), (44, 20), (45, 26),
        (42, 31), (36, 32), (30, 29), (24, 28), (18, 29), (12, 31), (8, 28)]

# neck + skull + muzzle, facing right
HEAD_SIDE = [(36, 15), (42, 10), (47, 7), (52, 6.5), (56, 9), (61, 12.5), (63, 14), (63, 16),
             (60, 17.5), (55, 18), (51, 20), (47, 22), (45, 27), (41, 30), (37, 26)]
HEAD_SIDE_KEY = dict(eye=(53.0, 11.0), nose=(62.0, 14.5))

EAR_SIDE = [
    [(45.5, 11), (46.5, 0.5), (52.5, 9)],                              # erect / pointed
    [(45.5, 11), (46.5, 3.5), (50, 3.0), (53, 9)],                     # rounded / short
    [(45.5, 11), (45.5, 3.5), (49.5, 2.5), (51, 4.5), (52.5, 9)],      # folded tip
]

# front-facing head (turned toward the viewer)
FCX, FCY = 47.0, 17.0
HEAD_FRONT = [(-10, -5), (-7, -9), (7, -9), (10, -5), (11, 1), (9, 7), (5, 11), (0, 14),
              (-5, 11), (-9, 7), (-11, 1)]
HEAD_FRONT = [(FCX + x, FCY + y) for x, y in HEAD_FRONT]
EAR_FRONT = [
    [[(FCX - 10, FCY - 4), (FCX - 9.5, FCY - 17), (FCX - 3, FCY - 8)],
     [(FCX + 10, FCY - 4), (FCX + 9.5, FCY - 17), (FCX + 3, FCY - 8)]],
    [[(FCX - 10, FCY - 4), (FCX - 10.5, FCY - 13), (FCX - 3, FCY - 8)],
     [(FCX + 10, FCY - 4), (FCX + 10.5, FCY - 13), (FCX + 3, FCY - 8)]],
    [[(FCX - 10, FCY - 4), (FCX - 8, FCY - 13), (FCX - 3, FCY - 8), (FCX - 13, FCY - 8)],
     [(FCX + 10, FCY - 4), (FCX + 8, FCY - 13), (FCX + 3, FCY - 8), (FCX + 13, FCY - 8)]],
]

TAILS = {
    0: [  # bushy, hangs down; two wag frames
        [(11, 19), (5, 19), (1, 23), (0, 30), (2, 36), (6, 36), (9, 29), (11, 25)],
        [(11, 19), (5, 17), (1, 21), (0, 28), (3, 35), (7, 34), (9, 28), (11, 25)],
    ],
    1: [  # sickle: curls up over the back
        [(11, 19), (6, 18), (2, 13), (1, 6), (4, 4), (6, 9), (11, 14)],
        [(11, 19), (6, 16), (3, 11), (3, 4), (6, 3), (8, 8), (11, 14)],
    ],
    2: [  # short low stub
        [(11, 20), (6, 20), (3, 23), (4, 28), (8, 28), (11, 26)],
        [(11, 20), (6, 18), (3, 21), (4, 26), (8, 27), (11, 26)],
    ],
}


# ------------------------------------------------------------ transforms --
def rot(pt, pivot, ang):
    c, s = math.cos(ang), math.sin(ang)
    x, y = pt[0] - pivot[0], pt[1] - pivot[1]
    return (pivot[0] + x * c - y * s, pivot[1] + x * s + y * c)


class Stage:
    def __init__(self, name, k, hk, lk):
        self.name, self.k, self.hk, self.lk = name, k, hk, lk
        self.cw = int(math.ceil(DESIGN_W * k)) + 1
        self.ch = int(math.ceil(DESIGN_H * k)) + 1
        self.dy = (GROUND - HIP) * (1.0 - lk)   # body drops when legs are shorter

    def tx(self, p, head=False, leg=False):
        x, y = p
        if head:
            x = NECK[0] + (x - NECK[0]) * self.hk
            y = NECK[1] + (y - NECK[1]) * self.hk
        if not leg:
            y += self.dy
        x *= self.k
        y = (y - GROUND) * self.k + (self.ch - 1)
        return (x, y)

    def render(self, polys, head=False, leg=False):
        """Render polygons (design coords) -> list of rows (set of x per y) as bytes + bbox."""
        im = Image.new("L", (self.cw * SS, self.ch * SS), 0)
        d = ImageDraw.Draw(im)
        for poly in polys:
            pts = [self.tx(p, head=head, leg=leg) for p in poly]
            d.polygon([(x * SS, y * SS) for x, y in pts], fill=255)
        return self.finish(im)

    def finish(self, im):
        im = im.resize((self.cw, self.ch), Image.BOX).point(lambda v: 255 if v >= 110 else 0)
        return im

    def point(self, p, head=False):
        x, y = self.tx(p, head=head)
        return (int(round(x)), int(round(y)))


def pack(im):
    """Crop to bbox, return (bytes, x, y, w, h) with MSB-first rows."""
    bb = im.getbbox()
    if not bb:
        return (bytes([0]), 0, 0, 1, 1)
    x0, y0, x1, y1 = bb
    w, h = x1 - x0, y1 - y0
    stride = (w + 7) // 8
    out = bytearray()
    px = im.load()
    for y in range(h):
        for bx in range(stride):
            b = 0
            for bit in range(8):
                x = bx * 8 + bit
                if x < w and px[x0 + x, y0 + y] >= 128:
                    b |= 0x80 >> bit
            out.append(b)
    return (bytes(out), x0, y0, w, h)


# ----------------------------------------------------------------- legs ----
def leg_poly(hx, dx, lift, lk, near):
    hip = GROUND - (GROUND - HIP) * lk
    foot = GROUND - lift
    mid_y = hip + (foot - hip) * 0.55
    mx = hx + dx * 0.5
    fx = hx + dx
    tw = 3.1 if near else 2.8
    return [(hx - tw, hip), (hx + tw, hip), (mx + 2.0, mid_y), (fx + 1.7, foot - 1.0),
            (fx + 3.9, foot - 0.2), (fx + 3.9, foot + 0.9), (fx - 1.7, foot + 0.9),
            (fx - 1.7, foot - 1.0), (mx - 1.9, mid_y)]


def legs_for_frame(frame, lk):
    # frame 0 = standing; 1..4 walking
    if frame == 0:
        offs = [(0, 0)] * 4
    else:
        a = 2 * math.pi * (frame - 1) / 4.0
        def sw(phase):
            ang = a + phase
            return (round(3.4 * math.sin(ang)), round(2.4 * max(0.0, math.cos(ang))))
        offs = [sw(0), sw(math.pi), sw(math.pi), sw(0)]  # nh, fh, nf, ff
    spec = [(13.0, offs[0], True), (19.5, offs[1], False), (40.5, offs[2], True), (34.5, offs[3], False)]
    return [leg_poly(hx, o[0], o[1], lk, near) for hx, o, near in spec]


# ---------------------------------------------------------------- output ---
def c_array(name, data):
    lines = []
    for i in range(0, len(data), 16):
        lines.append("  " + ", ".join("0x%02X" % b for b in data[i:i + 16]))
    return "static const uint8_t %s[] = {\n%s\n};\n" % (name, ",\n".join(lines))


def main(out_path):
    out = []
    out.append("// GENERATED by tools/gen_wolf_art.py -- do not edit by hand.\n")
    out.append("#pragma once\n#include <stdint.h>\n\n")
    out.append("struct WolfPart { const uint8_t* bits; int8_t x, y; uint8_t w, h; };\n")
    out.append("struct WolfKey {            // detail points for one head pose (canvas coords)\n"
               "  int8_t eye1x, eye1y, eye2x, eye2y;   // eye2 = -1 when only one eye is visible\n"
               "  int8_t nosex, nosey;\n"
               "  int8_t mx, my, mw, mh;               // open-mouth rectangle\n};\n")
    out.append("struct WolfStageArt {\n  uint8_t cw, ch;\n  WolfPart body;\n  WolfPart legs[5];\n"
               "  WolfPart head[4];   // 0 side, 1 side-down (eating), 2 front, 3 front (barking)\n"
               "  WolfKey  key[4];\n  WolfPart ear[3][4];\n  WolfPart tail[3][2];\n};\n\n")

    stage_syms = []
    for (name, k, hk, lk) in STAGES:
        st = Stage(name, k, hk, lk)
        pre = "ws_" + name
        parts = {}

        def add(key, im):
            data, x, y, w, h = pack(im)
            sym = "%s_%s" % (pre, key)
            out.append(c_array(sym, data))
            parts[key] = (sym, x, y, w, h)

        add("body", st.render([BODY]))
        for f in range(5):
            add("legs%d" % f, st.render(legs_for_frame(f, lk), leg=True))

        # head poses
        ang_down = math.radians(38)
        down_poly = [rot(p, NECK, ang_down) for p in HEAD_SIDE]
        add("head0", st.render([HEAD_SIDE], head=True))
        add("head1", st.render([down_poly], head=True))
        add("head2", st.render([HEAD_FRONT], head=True))
        add("head3", st.render([HEAD_FRONT], head=True))

        keys = []
        # side
        e = st.point(HEAD_SIDE_KEY["eye"], head=True); n = st.point(HEAD_SIDE_KEY["nose"], head=True)
        keys.append((e[0], e[1], -1, -1, n[0], n[1], 0, 0, 0, 0))
        # side down
        e = st.point(rot(HEAD_SIDE_KEY["eye"], NECK, ang_down), head=True)
        n = st.point(rot(HEAD_SIDE_KEY["nose"], NECK, ang_down), head=True)
        keys.append((e[0], e[1], -1, -1, n[0], n[1], 0, 0, 0, 0))
        # front (closed) and front (bark: mouth open)
        for opened in (False, True):
            e1 = st.point((FCX - 6, FCY - 2), head=True); e2 = st.point((FCX + 4, FCY - 2), head=True)
            n = st.point((FCX - 0.5, FCY + 6), head=True)
            if opened:
                m0 = st.point((FCX - 3, FCY + 8), head=True); m1 = st.point((FCX + 3, FCY + 12), head=True)
                keys.append((e1[0], e1[1], e2[0], e2[1], n[0], n[1], m0[0], m0[1],
                             max(2, m1[0] - m0[0]), max(2, m1[1] - m0[1])))
            else:
                keys.append((e1[0], e1[1], e2[0], e2[1], n[0], n[1], 0, 0, 0, 0))

        # ears
        for style in range(3):
            add("ear%d_0" % style, st.render([EAR_SIDE[style]], head=True))
            add("ear%d_1" % style, st.render([[rot(p, NECK, ang_down) for p in EAR_SIDE[style]]], head=True))
            add("ear%d_2" % style, st.render(EAR_FRONT[style], head=True))
            parts["ear%d_3" % style] = parts["ear%d_2" % style]

        for t in range(3):
            for fr in range(2):
                add("tail%d_%d" % (t, fr), st.render([TAILS[t][fr]]))

        def P(key):
            sym, x, y, w, h = parts[key]
            return "{%s, %d, %d, %d, %d}" % (sym, x, y, w, h)

        s = "static const WolfStageArt wolf_art_%s = {\n  %d, %d,\n  %s,\n  {%s},\n  {%s},\n  {%s},\n  {%s},\n  {%s}\n};\n\n" % (
            name, st.cw, st.ch, P("body"),
            ", ".join(P("legs%d" % f) for f in range(5)),
            ", ".join(P("head%d" % i) for i in range(4)),
            ", ".join("{%s}" % ", ".join(str(v) for v in kk) for kk in keys),
            ", ".join("{%s}" % ", ".join(P("ear%d_%d" % (e, i)) for i in range(4)) for e in range(3)),
            ", ".join("{%s}" % ", ".join(P("tail%d_%d" % (t, f)) for f in range(2)) for t in range(3)))
        out.append(s)
        stage_syms.append("&wolf_art_%s" % name)

    out.append("static const WolfStageArt* const wolf_art_stages[3] = { %s };\n" % ", ".join(stage_syms))
    with open(out_path, "w") as f:
        f.write("".join(out))
    print("wrote", out_path)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "src", "WolfArt.h"))
