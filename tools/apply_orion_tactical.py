#!/usr/bin/env python3
"""Add TACTICAL mode (a sweeping radar on the OLED instead of the pet) to a MeshCore tree that already has
LODESTAR ORION, the pet (apply_orion_wolf.py) and the Mesh Analyzer (apply_orion_mesh.py).

usage: apply_orion_tactical.py --meshcore /path/to/MeshCore      (build with -DTACTICAL=1)

The dashboard turns it on and off with "nova tac 1" / "nova tac 0".  The pet is untouched: its clock runs from
WolfModule::loop() whatever the screen shows.  Every anchor must match exactly once.
"""
import argparse, os, shutil, sys

ap = argparse.ArgumentParser()
ap.add_argument("--meshcore", required=True)
a = ap.parse_args()
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
cr = os.path.join(a.meshcore, "examples", "companion_radio")
if not os.path.exists(os.path.join(cr, "ui-new", "WolfModule.h")): sys.exit("apply the pet first (apply_orion_wolf.py)")
if not os.path.exists(os.path.join(cr, "MeshScope.h")): sys.exit("apply the Mesh Analyzer first (apply_orion_mesh.py)")
shutil.copy(os.path.join(root, "src", "TacRadar.h"), os.path.join(cr, "ui-new")); print("copied TacRadar.h")


def patch(path, edits):
    s = open(path).read()
    for name, old, new in edits:
        n = s.count(old)
        if n != 1: sys.exit("anchor '%s' matched %d times in %s (need exactly 1)" % (name, n, os.path.basename(path)))
        s = s.replace(old, new); print("  +", name)
    open(path, "w").write(s)


print("MyMesh.cpp")
patch(os.path.join(cr, "MyMesh.cpp"), [
    ("tac declarations", "static meshscope::Scope nv_scope;   // MESH ANALYZER\n",
     "static meshscope::Scope nv_scope;   // MESH ANALYZER\n#include \"ui-new/TacRadar.h\"\nbool tacSetMode(int mode, char* reply, int reply_len);                              // WolfModule.cpp\nvoid tacPacket(uint8_t type, const uint8_t* payload, int len, uint32_t now);        // WolfModule.cpp\n"),
    ("tac functions", "void MyMesh::novaTick(uint32_t nv_now) {\n", r'''
// TACTICAL radar: who this radio has heard (the wardriving log) and where it is now
int tacCollect(tac::Contact* out, int max, tac::Self& me, uint32_t now) {
  const lodestar::Position& p = nova_core.link.position();
  me.fix = nova_core.link.hasFix(now); me.lat_e6 = p.lat_e6; me.lon_e6 = p.lon_e6;
  int n = 0;
  for (int i = 0; i < NOVA_FOUND_SLOTS && n < max; i++) {
    const auto* f = nova_core.found.at(i);
    if (!f) continue;
    tac::Contact& c = out[n++];
    c.id = (uint8_t)i; c.type = f->type; c.k0 = f->key[0]; c.snr = (int8_t)(f->snr_q / 4);
    c.has_pos = f->has_adv; c.lat_e6 = f->adv_lat; c.lon_e6 = f->adv_lon;
    c.age_ms = now - f->last_ms;
  }
  return n;
}

void MyMesh::novaTick(uint32_t nv_now) {
'''),
    ("tac rx", "pkt->getPathHashSize(), pkt->path, pkt->payload, pkt->payload_len, len);   // MESH ANALYZER\n",
     "pkt->getPathHashSize(), pkt->path, pkt->payload, pkt->payload_len, len);   // MESH ANALYZER\n  tacPacket(pkt->getPayloadType(), pkt->payload, pkt->payload_len, nv_now);   // TACTICAL: a packet from a contact lights it on the radar\n"),
    ("tac command", "    if (strncmp(nv_line + 4, \" json\", 5) == 0)",
     "    if (strncmp(nv_line + 4, \" tac\", 4) == 0 && (nv_line[8] == 0 || nv_line[8] == ' ')) {   // nova tac 1 / nova tac 0 / nova tac\n"
     "      char nv_tr[64]; int nv_tm = nv_line[8] == ' ' ? (nv_line[9] == '1' ? 1 : nv_line[9] == '0' ? 0 : -1) : -1;\n"
     "      bool nv_tch = tacSetMode(nv_tm, nv_tr, sizeof(nv_tr));\n"
     "      if (nv_tch || nv_tm < 0) { Serial.print(\"# \"); Serial.println(nv_tr); }\n"
     "      continue;\n    }\n    if (strncmp(nv_line + 4, \" json\", 5) == 0)"),
])
print("tactical added")
