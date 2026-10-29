#!/usr/bin/env python3
"""Add the MESH ANALYZER stream to a MeshCore tree that LODESTAR NOVA/ORION has been applied to.

usage: apply_orion_mesh.py --meshcore /path/to/MeshCore

The radio streams every packet it hears or sends ("nova mesh" keeps it going, like "nova pet"); the dashboard draws it.
Every anchor must match exactly once.
"""
import argparse, os, shutil, sys

ap = argparse.ArgumentParser()
ap.add_argument("--meshcore", required=True)
a = ap.parse_args()
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
cr = os.path.join(a.meshcore, "examples", "companion_radio")
if not os.path.exists(os.path.join(cr, "ui-new", "NovaPages.h")):
    sys.exit("apply NOVA/ORION first")
shutil.copy(os.path.join(root, "src", "MeshScope.h"), cr)
print("copied MeshScope.h")


def patch(path, edits):
    s = open(path).read()
    for name, old, new in edits:
        n = s.count(old)
        if n != 1:
            sys.exit("anchor '%s' matched %d times in %s (need exactly 1)" % (name, n, os.path.basename(path)))
        s = s.replace(old, new)
        print("  +", name)
    open(path, "w").write(s)


print("MyMesh.cpp")
patch(os.path.join(cr, "MyMesh.cpp"), [
    ("include", "// ---- LODESTAR NOVA ---------------------------------------------------------------\nnova::Nova nova_core;\n",
     "// ---- LODESTAR NOVA ---------------------------------------------------------------\nnova::Nova nova_core;\n#include \"MeshScope.h\"\nstatic meshscope::Scope nv_scope;   // MESH ANALYZER\n"),
    ("rx hook", "  nova_core.diag.onRx(pkt->getSNR(), nv_now);\n",
     "  nova_core.diag.onRx(pkt->getSNR(), nv_now);\n"
     "  nv_scope.onRx(nv_now, _radio->getLastRSSI(), pkt->getSNR(), pkt->getPayloadType(), pkt->getRouteType(), pkt->getPathHashCount(),\n"
     "                pkt->getPathHashSize(), pkt->path, pkt->payload, pkt->payload_len, len);   // MESH ANALYZER\n"),
    ("tx hook", "  nova_core.diag.onTx();\n",
     "  nova_core.diag.onTx();\n  nv_scope.onTx(millis(), pkt->getPayloadType(), pkt->getRouteType(), len);   // MESH ANALYZER\n"),
    ("state", "  static uint32_t nv_next_json = 0;\n",
     "  static uint32_t nv_next_json = 0;\n  static uint32_t nv_mesh_until = 0, nv_mesh_next = 0;   // the mesh analyzer stream, while a page asks for it\n"),
    ("command", "    if (strncmp(nv_line + 4, \" json\", 5) == 0)",
     "    if (strncmp(nv_line + 4, \" mesh\", 5) == 0 && (nv_line[9] == 0 || nv_line[9] == ' ')) { if ((int32_t)(nv_mesh_until - nv_now) <= 0) nv_mesh_next = nv_now; nv_mesh_until = nv_now + 15000u; continue; }\n"
     "    if (strncmp(nv_line + 4, \" json\", 5) == 0)"),
    ("stream", "  static bool nv_was_on = false;\n",
     """  if ((int32_t)(nv_mesh_until - nv_now) > 0 && (int32_t)(nv_now - nv_mesh_next) >= 0) {   // twice a second, only when there is news
    nv_mesh_next = nv_now + 500u;
    static char nv_mesh[2400];
    static uint32_t nv_mesh_force = 0;
    bool nv_f = (int32_t)(nv_now - nv_mesh_force) >= 0;       // a line every 3 s even when quiet: the noise floor still moves
    if (nv_f) nv_mesh_force = nv_now + 3000u;
    size_t nv_mn = nv_scope.line(nv_now, nv_mesh, sizeof(nv_mesh), _radio->getNoiseFloor(), _prefs.sf, _prefs.bw, _prefs.cr, nv_f);
    if (nv_mn > 0) {
      Serial.write((const uint8_t*)nv_mesh, nv_mn);
      Serial.write('\\n');
    }
  }
  static bool nv_was_on = false;
"""),
])
print("mesh analyzer added")
