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

PAD = 8                    # blank rows above the design so the head can tip up (howl)
DESIGN_W, DESIGN_H = 64, 44 + PAD
GROUND = 43.0 + PAD
HIP = 27.0 + PAD                 # hip height used for the legs
NECK = (38.0, 18.0)        # pivot used for head scaling / rotation

# stage: (scale, head_scale, leg_scale)
STAGES = [
    ("pup",      0.62, 1.40, 0.82),
    ("juvenile", 0.78, 1.15, 0.94),
    ("adult",    0.92, 1.00, 1.00),
]

# ---------------------------------------------------------------- geometry --
BODY = [(7, 21), (11, 17.5), (19, 16), (27, 16), (34, 14.5), (40, 14), (44, 17), (46, 22), (47, 28),
        (45, 31.5), (43, 34), (40, 32), (37, 34.5), (34, 31.5), (29, 29.5), (23, 28.5), (17, 29.5),
        (12, 30.5), (8, 27)]

# neck (with shoulder ruff) + skull + long muzzle, facing right
HEAD_SIDE = [(34, 13), (38, 9.5), (43, 8.5), (47, 8), (51, 8.5), (54, 10), (58, 11.5), (63.5, 13.2),
             (63.8, 15.4), (61, 16.4), (56, 17), (52, 18.6), (50, 22), (49.5, 25), (47, 24), (46, 28.5),
             (44, 26.5), (42, 31), (39, 27), (36, 28)]
HEAD_SIDE_KEY = dict(eye=(51.0, 11.8), nose=(62.5, 13.8))

EAR_SIDE = [
    [(43.5, 10.5), (45.5, -1.0), (52, 9.5)],                                # tall erect
    [(43.5, 10.5), (44.5, 2.5), (49.5, 1.5), (52, 9.5)],                    # rounded / short
    [(43.5, 10.5), (44.0, 2.0), (48.5, 0.5), (50.5, 3.5), (52, 9.5)],       # folded tip
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
    0: [  # big bushy brush, hangs down; two wag frames
        [(10, 18), (5, 18), (1.5, 22), (0, 29), (0.5, 36), (3, 40), (6.5, 38), (8.5, 32), (11, 25)],
        [(10, 18), (5, 16), (1.5, 20), (0, 27), (1.5, 34), (4.5, 38.5), (8, 36), (9.5, 30), (11, 25)],
    ],
    1: [  # sickle: curls up over the back
        [(10, 19), (5, 18), (1.5, 12), (1, 5), (4, 2.5), (6.5, 8), (11, 14)],
        [(10, 19), (5, 16), (2.5, 10), (3, 3), (6.5, 1.5), (8.5, 7), (11, 14)],
    ],
    2: [  # straight out, low and thick
        [(10, 19), (4, 22), (0, 27), (1, 31), (6, 29), (11, 26)],
        [(10, 19), (4, 20), (0, 24), (1, 29), (6, 28), (11, 26)],
    ],
}


# ------------------------------------------------------------ transforms --
def rot(pt, pivot, ang):
    c, s = math.cos(ang), math.sin(ang)
    x, y = pt[0] - pivot[0], pt[1] - pivot[1]
    return (pivot[0] + x * c - y * s, pivot[1] + x * s + y * c)



def _pl(x):
    """one polygon or a list of polygons / cut markers -> list"""
    return x if (x and isinstance(x[0], (list, tuple)) and (isinstance(x[0], list) or (x[0] and isinstance(x[0][0], str)))) else [x]


def _rotl(x, pivot, ang):
    """rotate a polygon or a list of polygons / cut markers about `pivot`"""
    out = []
    for q in _pl(x):
        if isinstance(q, tuple) and q and isinstance(q[0], str):
            out.append((q[0], [rot(p, pivot, ang) for p in q[1]]) + tuple(q[2:]))
        else:
            out.append([rot(p, pivot, ang) for p in q])
    return out


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
            y += self.dy + PAD
        x *= self.k
        y = (y - GROUND) * self.k + (self.ch - 1)
        return (x, y)

    def render(self, polys, head=False, leg=False):
        """Render polygons (design coords) -> list of rows (set of x per y) as bytes + bbox."""
        im = Image.new("L", (self.cw * SS, self.ch * SS), 0)
        d = ImageDraw.Draw(im)
        for poly in polys:
            if isinstance(poly, tuple) and poly and poly[0] == "cut":          # carve a hole (interior detail)
                pts = [self.tx(p, head=head, leg=leg) for p in poly[1]]
                d.polygon([(x * SS, y * SS) for x, y in pts], fill=0)
                continue
            if isinstance(poly, tuple) and poly and poly[0] == "cutline":      # carve a thin line
                pts = [self.tx(p, head=head, leg=leg) for p in poly[1]]
                d.line([(x * SS, y * SS) for x, y in pts], fill=0, width=max(1, int(round(poly[2] * self.k * SS))))
                continue
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
LEG_X = (13.0, 19.5, 40.5, 34.5)     # hind-near, hind-far, fore-near, fore-far hip positions
LEG_TW = (3.4, 3.0)                  # thigh half-width (near, far)
FRONT_EYE = ((-6, -2), (4, -2))      # eye offsets from the front-face centre
FRONT_NOSE = (-0.5, 6)
FRONT_MOUTH = ((-3, 8), (3, 12))


def leg_poly(hx, dx, lift, lk, near):
    hip = GROUND - (GROUND - HIP) * lk
    foot = GROUND - lift
    mid_y = hip + (foot - hip) * 0.5
    mx = hx + dx * 0.5
    fx = hx + dx
    tw = LEG_TW[0] if near else LEG_TW[1]
    return [(hx - tw, hip), (hx + tw, hip), (mx + 1.8, mid_y), (fx + 1.2, foot - 3.0),
            (fx + 1.5, foot - 1.2), (fx + 4.2, foot - 0.2), (fx + 4.2, foot + 0.9), (fx - 1.6, foot + 0.9),
            (fx - 1.6, foot - 1.2), (fx - 1.0, foot - 3.0), (mx - 1.7, mid_y)]


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
    spec = [(LEG_X[0], offs[0], True), (LEG_X[1], offs[1], False), (LEG_X[2], offs[2], True), (LEG_X[3], offs[3], False)]
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
               "  WolfPart head[5];   // 0 side, 1 side-down (eating), 2 front, 3 front (barking), 4 side-up (howling)\n"
               "  WolfKey  key[5];\n  WolfPart ear[3][5];\n  WolfPart tail[3][2];\n};\n\n")

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

        add("body", st.render(BODY if isinstance(BODY[0], list) else [BODY]))
        for f in range(5):
            add("legs%d" % f, st.render(legs_for_frame(f, lk), leg=True))

        # head poses
        ang_down = math.radians(38)
        down_poly = _rotl(HEAD_SIDE, NECK, ang_down)
        add("head0", st.render(_pl(HEAD_SIDE), head=True))
        add("head1", st.render(down_poly, head=True))
        add("head2", st.render([HEAD_FRONT], head=True))
        add("head3", st.render([HEAD_FRONT], head=True))
        ang_up = math.radians(-32)
        add("head4", st.render(_rotl(HEAD_SIDE, NECK, ang_up), head=True))

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
            e1 = st.point((FCX + FRONT_EYE[0][0], FCY + FRONT_EYE[0][1]), head=True); e2 = st.point((FCX + FRONT_EYE[1][0], FCY + FRONT_EYE[1][1]), head=True)
            n = st.point((FCX + FRONT_NOSE[0], FCY + FRONT_NOSE[1]), head=True)
            if opened:
                m0 = st.point((FCX + FRONT_MOUTH[0][0], FCY + FRONT_MOUTH[0][1]), head=True); m1 = st.point((FCX + FRONT_MOUTH[1][0], FCY + FRONT_MOUTH[1][1]), head=True)
                keys.append((e1[0], e1[1], e2[0], e2[1], n[0], n[1], m0[0], m0[1],
                             max(2, m1[0] - m0[0]), max(2, m1[1] - m0[1])))
            else:
                keys.append((e1[0], e1[1], e2[0], e2[1], n[0], n[1], 0, 0, 0, 0))

        # side, head up (howling)
        e = st.point(rot(HEAD_SIDE_KEY["eye"], NECK, ang_up), head=True)
        n = st.point(rot(HEAD_SIDE_KEY["nose"], NECK, ang_up), head=True)
        keys.append((e[0], e[1], -1, -1, n[0], n[1], 0, 0, 0, 0))

        # ears
        for style in range(3):
            add("ear%d_0" % style, st.render(_pl(EAR_SIDE[style]), head=True))
            add("ear%d_1" % style, st.render(_rotl(EAR_SIDE[style], NECK, ang_down), head=True))
            add("ear%d_2" % style, st.render(EAR_FRONT[style], head=True))
            parts["ear%d_3" % style] = parts["ear%d_2" % style]
            add("ear%d_4" % style, st.render(_rotl(EAR_SIDE[style], NECK, ang_up), head=True))

        for t in range(3):
            for fr in range(2):
                tp = TAILS[t][fr]
                add("tail%d_%d" % (t, fr), st.render(tp if isinstance(tp[0], list) else [tp]))

        def P(key):
            sym, x, y, w, h = parts[key]
            return "{%s, %d, %d, %d, %d}" % (sym, x, y, w, h)

        s = "static const WolfStageArt wolf_art_%s = {\n  %d, %d,\n  %s,\n  {%s},\n  {%s},\n  {%s},\n  {%s},\n  {%s}\n};\n\n" % (
            name, st.cw, st.ch, P("body"),
            ", ".join(P("legs%d" % f) for f in range(5)),
            ", ".join(P("head%d" % i) for i in range(5)),
            ", ".join("{%s}" % ", ".join(str(v) for v in kk) for kk in keys),
            ", ".join("{%s}" % ", ".join(P("ear%d_%d" % (e, i)) for i in range(5)) for e in range(3)),
            ", ".join("{%s}" % ", ".join(P("tail%d_%d" % (t, f)) for f in range(2)) for t in range(3)))
        out.append(s)
        stage_syms.append("&wolf_art_%s" % name)

    out.append("static const WolfStageArt* const wolf_art_stages[3] = { %s };\n" % ", ".join(stage_syms))
    with open(out_path, "w") as f:
        f.write("".join(out))
    print("wrote", out_path)


if __name__ == "__main__":
    import argparse, importlib
    ap = argparse.ArgumentParser()
    ap.add_argument("--species", default="wolf")
    ap.add_argument("out", nargs="?")
    a = ap.parse_args()
    here = os.path.dirname(os.path.abspath(__file__))
    if a.species != "wolf":
        sys.path.insert(0, here)
        getattr(importlib.import_module("pet_species"), "apply_" + a.species)(globals())
    main(a.out or os.path.join(here, "..", "src", "WolfArt_%s.h" % a.species))
