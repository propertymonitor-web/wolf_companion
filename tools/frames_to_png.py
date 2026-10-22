#!/usr/bin/env python3
"""Turn host_sim frame dumps into a contact-sheet PNG (and optional GIF) for visual review.

usage: frames_to_png.py frames.txt out.png [--cols N] [--scale S] [--filter PREFIX] [--gif out.gif]
"""
import sys, argparse
from PIL import Image, ImageDraw, ImageFont


def parse(path):
    frames, cur = [], None
    for line in open(path):
        line = line.rstrip("\n")
        if line.startswith("FRAME "):
            cur = {"label": line[6:], "rows": [], "txt": []}
        elif line == "END":
            frames.append(cur); cur = None
        elif cur is not None:
            if line.startswith("TXT "):
                _, x, y, rest = line.split(" ", 3)
                cur["txt"].append((int(x), int(y), rest))
            else:
                cur["rows"].append(line)
    return frames


def render(fr, scale):
    im = Image.new("RGB", (128, 64), (0, 0, 0))
    px = im.load()
    for y, row in enumerate(fr["rows"]):
        for x, c in enumerate(row):
            if c == "1":
                px[x, y] = (120, 200, 255)
    big = im.resize((128 * scale, 64 * scale), Image.NEAREST)
    d = ImageDraw.Draw(big)
    try:
        font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", 6 * scale)
    except Exception:
        font = ImageFont.load_default()
    for x, y, s in fr["txt"]:
        d.text((x * scale, y * scale - scale), s, fill=(120, 200, 255), font=font)
    return big


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("frames"); ap.add_argument("out")
    ap.add_argument("--cols", type=int, default=4)
    ap.add_argument("--scale", type=int, default=3)
    ap.add_argument("--filter", default="")
    ap.add_argument("--gif", default="")
    ap.add_argument("--every", type=int, default=1)
    a = ap.parse_args()
    frames = [f for f in parse(a.frames) if f["label"].startswith(a.filter)][::a.every]
    imgs = [render(f, a.scale) for f in frames]
    if a.gif:
        imgs[0].save(a.gif, save_all=True, append_images=imgs[1:], duration=110, loop=0)
    W, H = imgs[0].size
    cols = min(a.cols, len(imgs)); rows = (len(imgs) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * (W + 6), rows * (H + 18)), (30, 30, 30))
    d = ImageDraw.Draw(sheet)
    for i, (im, fr) in enumerate(zip(imgs, frames)):
        x, y = (i % cols) * (W + 6), (i // cols) * (H + 18)
        sheet.paste(im, (x, y + 14))
        d.text((x + 2, y), fr["label"], fill=(255, 255, 255))
    sheet.save(a.out)
    print("wrote", a.out, len(imgs), "frames")


if __name__ == "__main__":
    main()
