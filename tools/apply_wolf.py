#!/usr/bin/env python3
"""Drop the wolf pet into a MeshCore checkout.

usage: apply_wolf.py --meshcore /path/to/MeshCore

Copies src/Wolf*.{h,cpp} into examples/companion_radio/ui-new/ (the PlatformIO envs already compile
everything in that folder) and applies patches/wolf_uitask.patch to UITask.cpp / UITask.h.
The patch is made against the MeshCore commit pinned in the workflow (see README).
"""
import argparse, glob, os, shutil, subprocess, sys

ap = argparse.ArgumentParser()
ap.add_argument("--meshcore", required=True)
a = ap.parse_args()

here = os.path.dirname(os.path.abspath(__file__))
root = os.path.dirname(here)
dest = os.path.join(a.meshcore, "examples", "companion_radio", "ui-new")
if not os.path.isdir(dest):
    sys.exit("not a MeshCore checkout (missing %s)" % dest)

for f in sorted(glob.glob(os.path.join(root, "src", "Wolf*"))):
    shutil.copy(f, dest)
    print("copied", os.path.basename(f))

patch = os.path.join(root, "patches", "wolf_uitask.patch")
r = subprocess.run(["git", "apply", "--check", patch], cwd=a.meshcore, capture_output=True, text=True)
if r.returncode != 0:
    print(r.stderr)
    sys.exit("UITask patch does not apply: this MeshCore version differs from the one the patch was made for.")
subprocess.run(["git", "apply", patch], cwd=a.meshcore, check=True)
print("patched UITask.cpp / UITask.h")
