#!/usr/bin/env python3
"""Generate src/WolfProps.h: small 1-bit props for the wolf's random activities
(bunny, bird, butterfly, moon).  Sprites are drawn facing RIGHT, left-facing copies are mirrored.
Every bitmap is MSB-first, rows padded to whole bytes, set bit = lit pixel."""
import os, sys
from PIL import Image, ImageDraw

SS = 8


def canvas(w, h):
    im = Image.new("L", (w * SS, h * SS), 0)
    return im, ImageDraw.Draw(im)


def P(v):
    return v * SS


def ell(d, cx, cy, rx, ry, fill=255):
    d.ellipse((P(cx - rx), P(cy - ry), P(cx + rx), P(cy + ry)), fill=fill)


def poly(d, pts, fill=255):
    d.polygon([(P(x), P(y)) for x, y in pts], fill=fill)


def fin(im, w, h):
    return im.resize((w, h), Image.BOX).point(lambda v: 255 if v >= 120 else 0)


# --------------------------------------------------------------- bunny ----
def bunny_sit():
    w, h = 13, 11
    im, d = canvas(w, h)
    ell(d, 5.5, 8, 4.4, 2.8)                     # body
    ell(d, 9, 5.6, 2.4, 2.1)                     # head
    poly(d, [(8.0, 4.0), (8.2, 0.2), (9.2, 0.2), (9.6, 4.0)])      # ears
    poly(d, [(9.8, 4.0), (10.6, 0.8), (11.5, 1.0), (11.2, 4.4)])
    ell(d, 1.4, 8.1, 1.1, 1.1)                   # cotton tail
    ell(d, 10.3, 5.2, 0.5, 0.5, 0)               # eye
    return fin(im, w, h)


def bunny_hop():
    w, h = 15, 8
    im, d = canvas(w, h)
    ell(d, 6.5, 4.6, 5.0, 2.4)                   # stretched body
    ell(d, 11.8, 3.6, 2.1, 1.8)                  # head
    poly(d, [(11.0, 2.4), (8.2, 0.4), (8.6, -0.2), (11.8, 1.6)])   # ears laid back
    poly(d, [(12.2, 2.0), (9.8, 0.0), (10.3, -0.4), (13.0, 1.4)])
    ell(d, 1.5, 4.2, 1.1, 1.1)                   # tail
    poly(d, [(2.5, 6.2), (3.2, 7.8), (6.2, 7.8), (6.0, 6.0)])      # hind foot out
    poly(d, [(10.5, 5.6), (13.8, 7.2), (14.0, 6.2), (11.5, 4.9)])  # front legs reaching
    ell(d, 12.7, 3.3, 0.45, 0.45, 0)
    return fin(im, w, h)


# ---------------------------------------------------------------- bird ----
def bird(frame):
    # frame 0 = standing, 1 = pecking, 2 = wings up, 3 = wings down
    w, h = 13, 10
    im, d = canvas(w, h)
    if frame in (0, 1):
        ell(d, 5.5, 5.2, 3.6, 2.5)                              # body
        hx, hy = (9.2, 3.0) if frame == 0 else (9.6, 5.6)
        ell(d, hx, hy, 1.7, 1.6)                                # head
        poly(d, [(hx + 1.0, hy - 0.3), (hx + 3.2, hy + 0.3), (hx + 1.0, hy + 0.9)])  # beak
        poly(d, [(2.5, 4.6), (-0.2, 6.2), (-0.2, 7.2), (3.2, 6.8)])               # tail
        d.line((P(5), P(7.5), P(5), P(9.7)), fill=255, width=P(1) // 2)           # legs
        d.line((P(6.5), P(7.5), P(6.5), P(9.7)), fill=255, width=P(1) // 2)
        ell(d, hx + 0.4, hy - 0.4, 0.4, 0.4, 0)
    else:
        ell(d, 6.5, 5.0, 3.2, 1.9)
        ell(d, 10.0, 4.0, 1.5, 1.4)
        poly(d, [(10.9, 3.8), (12.9, 4.3), (10.9, 4.9)])
        poly(d, [(3.8, 4.6), (0.6, 5.4), (0.6, 6.3), (4.4, 5.9)])
        if frame == 2:
            poly(d, [(5.0, 4.2), (3.2, 0.0), (7.0, 0.4), (8.2, 4.0)])           # wing up
        else:
            poly(d, [(5.0, 5.4), (3.6, 9.4), (7.4, 9.0), (8.0, 5.6)])           # wing down
        ell(d, 10.3, 3.6, 0.4, 0.4, 0)
    return fin(im, w, h)


# ----------------------------------------------------------- butterfly ----
def butterfly(frame):
    w, h = 9, 7
    im, d = canvas(w, h)
    if frame == 0:     # wings open
        ell(d, 2.6, 2.6, 2.4, 2.1); ell(d, 6.4, 2.6, 2.4, 2.1)
        ell(d, 2.9, 5.2, 1.7, 1.4); ell(d, 6.1, 5.2, 1.7, 1.4)
        d.rectangle((P(4.2), P(1.0), P(4.8), P(6.2)), fill=0)
        d.rectangle((P(4.3), P(1.4), P(4.7), P(6.2)), fill=255)
    else:              # wings folded up
        ell(d, 4.5, 3.4, 1.2, 2.8)
        poly(d, [(3.5, 1.2), (3.0, 0.0), (3.6, 0.9)]); poly(d, [(5.5, 1.2), (6.0, 0.0), (5.4, 0.9)])
    return fin(im, w, h)



# --------------------------------------------------------------- mouse ----
def mouse_sit():
    w, h = 10, 7
    im, d = canvas(w, h)
    ell(d, 4.5, 4.4, 3.4, 2.3)                    # body
    ell(d, 7.7, 4.0, 1.7, 1.5)                    # head
    ell(d, 7.0, 2.1, 1.0, 1.0)                    # ear
    poly(d, [(9.0, 4.2), (10.2, 4.7), (9.0, 5.0)])  # nose
    d.line((P(1.5), P(5.0), P(-0.3), P(6.4)), fill=255, width=P(1) // 2)   # tail
    ell(d, 8.2, 3.6, 0.35, 0.35, 0)
    return fin(im, w, h)


def mouse_run():
    w, h = 12, 6
    im, d = canvas(w, h)
    ell(d, 5.5, 3.6, 4.2, 1.9)
    ell(d, 9.6, 3.3, 1.6, 1.3)
    ell(d, 8.8, 1.7, 0.9, 0.9)
    poly(d, [(10.8, 3.4), (12.2, 3.9), (10.8, 4.2)])
    d.line((P(1.4), P(3.8), P(-0.5), P(2.4)), fill=255, width=P(1) // 2)
    return fin(im, w, h)


# ---------------------------------------------------------------- frog ----
def frog_sit():
    w, h = 10, 8
    im, d = canvas(w, h)
    ell(d, 4.8, 5.0, 3.6, 2.6)
    ell(d, 7.4, 3.3, 2.0, 1.7)
    ell(d, 8.0, 1.7, 0.9, 0.9); ell(d, 6.6, 1.9, 0.9, 0.9)       # eye bumps
    poly(d, [(1.0, 7.9), (3.4, 6.0), (5.0, 7.9)])                # folded hind leg
    ell(d, 8.2, 1.6, 0.35, 0.35, 0)
    return fin(im, w, h)


def frog_hop():
    w, h = 13, 7
    im, d = canvas(w, h)
    ell(d, 6.0, 3.4, 3.8, 1.8)
    ell(d, 9.8, 2.6, 1.9, 1.5)
    ell(d, 10.4, 1.3, 0.8, 0.8)
    poly(d, [(2.0, 4.0), (0.0, 6.8), (1.2, 6.9), (4.0, 4.6)])    # legs stretched back
    poly(d, [(10.5, 4.0), (12.8, 6.0), (12.8, 6.9), (10.2, 5.0)])
    ell(d, 10.6, 1.2, 0.3, 0.3, 0)
    return fin(im, w, h)


# ----------------------------------------------------------------- bee ----
def bee(frame):
    w, h = 11, 8
    im, d = canvas(w, h)
    ell(d, 5.5, 5.0, 3.8, 2.3)                                   # body
    ell(d, 9.3, 4.6, 1.4, 1.3)                                   # head
    for x in (3.6, 5.6):                                         # stripes
        d.rectangle((P(x), P(2.4), P(x + 0.7), P(7.8)), fill=0)
    poly(d, [(1.6, 5.2), (0.0, 5.6), (1.6, 6.0)])                # stinger
    if frame == 0:
        ell(d, 5.0, 1.8, 2.2, 1.5); ell(d, 7.4, 1.6, 1.6, 1.2)    # wings up
    else:
        ell(d, 4.6, 2.9, 2.6, 1.0); ell(d, 7.2, 2.9, 1.8, 0.9)    # wings level
    ell(d, 9.8, 4.2, 0.3, 0.3, 0)
    return fin(im, w, h)

# ---------------------------------------------------------------- moon ----
def moon():
    w, h = 13, 13
    im, d = canvas(w, h)
    ell(d, 6.5, 6.5, 5.6, 5.6)
    ell(d, 9.0, 5.0, 4.8, 4.8, 0)
    return fin(im, w, h)


# --------------------------------------------------------------- output ---
def pack(im):
    w, h = im.size
    stride = (w + 7) // 8
    out = bytearray()
    px = im.load()
    for y in range(h):
        for bx in range(stride):
            b = 0
            for bit in range(8):
                x = bx * 8 + bit
                if x < w and px[x, y] >= 128:
                    b |= 0x80 >> bit
            out.append(b)
    return bytes(out)


def emit(name, im, out):
    w, h = im.size
    data = pack(im)
    rows = ",\n".join("  " + ", ".join("0x%02X" % b for b in data[i:i + 16]) for i in range(0, len(data), 16))
    out.append("static const uint8_t %s[] = {\n%s\n};\n#define %s_W %d\n#define %s_H %d\n" %
               (name, rows, name.upper(), w, name.upper(), h))


def flip(im):
    return im.transpose(Image.FLIP_LEFT_RIGHT)


def main(path):
    out = ["// GENERATED by tools/gen_wolf_props.py -- do not edit by hand.\n#pragma once\n#include <stdint.h>\n\n"]
    for n, im in [("bunny_sit", bunny_sit()), ("bunny_hop", bunny_hop())]:
        emit(n + "_r", im, out); emit(n + "_l", flip(im), out)
    for f in range(4):
        im = bird(f)
        emit("bird%d_r" % f, im, out); emit("bird%d_l" % f, flip(im), out)
    for f in range(2):
        emit("fly%d" % f, butterfly(f), out)
    for n, im in [("mouse_sit", mouse_sit()), ("mouse_run", mouse_run()), ("frog_sit", frog_sit()), ("frog_hop", frog_hop())]:
        emit(n + "_r", im, out); emit(n + "_l", flip(im), out)
    for f in range(2):
        emit("bee%d" % f, bee(f), out)
    emit("moon", moon(), out)
    open(path, "w").write("\n".join(out))
    print("wrote", path)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "src", "WolfProps.h"))
