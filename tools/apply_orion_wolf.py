#!/usr/bin/env python3
"""Add the wolf pet to a MeshCore tree that LODESTAR NOVA/ORION has already been applied to.

usage: apply_orion_wolf.py --meshcore /path/to/MeshCore

Run tools/apply_nova.py (from the ORION repo) first.  This script copies src/Wolf*.{h,cpp} into
examples/companion_radio/ui-new/ and edits UITask.cpp / UITask.h with anchored substitutions.
Every anchor must match exactly once, so a MeshCore change that moves things fails loudly
instead of producing a quietly wrong build.
"""
import argparse, glob, os, shutil, sys

ap = argparse.ArgumentParser()
ap.add_argument("--meshcore", required=True)
a = ap.parse_args()
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ui = os.path.join(a.meshcore, "examples", "companion_radio", "ui-new")
if not os.path.isdir(ui):
    sys.exit("not a MeshCore checkout (missing %s)" % ui)
if not os.path.exists(os.path.join(ui, "NovaPages.h")):
    sys.exit("apply NOVA/ORION first (NovaPages.h is missing)")

for f in sorted(glob.glob(os.path.join(root, "src", "Wolf*")) + [os.path.join(root, "src", "PetSpecies.h")]):
    shutil.copy(f, ui)
    print("copied", os.path.basename(f))


def patch(path, edits):
    s = open(path).read()
    for name, old, new in edits:
        n = s.count(old)
        if n != 1:
            sys.exit("anchor '%s' matched %d times in %s (need exactly 1)" % (name, n, os.path.basename(path)))
        s = s.replace(old, new)
        print("  +", name)
    open(path, "w").write(s)


print("UITask.cpp")
patch(os.path.join(ui, "UITask.cpp"), [
    ("include", '#include "icons.h"\n', '#include "icons.h"\n#include "WolfModule.h"\n'),
    ("enum WOLF", "  enum HomePage {\n    FIRST,",
     "  enum HomePage {\n    WOLF,     // the wolf pet: first screen after the splash, never times out\n    FIRST,"),
    ("onWolfPage", "  HomeScreen(UITask* task, mesh::RTCClock* rtc,",
     "  bool onWolfPage() const { return _page == HomePage::WOLF; }\n\n  HomeScreen(UITask* task, mesh::RTCClock* rtc,"),
    ("render", "  int render(DisplayDriver& display) override {\n    display.setColor(UIColor::title_bkg);\n    display.fillRect(0, 0, display.width(), 12);",
     "  int render(DisplayDriver& display) override {\n    if (_page == HomePage::WOLF) return _task->renderWolf(display);   // full-screen wolf, no title bar\n    display.setColor(UIColor::title_bkg);\n    display.fillRect(0, 0, display.width(), 12);"),
    ("long press", "    if (c == KEY_ENTER && _page == HomePage::BLUETOOTH) {",
     "    if (c == KEY_ENTER && _page == HomePage::WOLF) {   // long press: look at the next wolf\n      _task->wolfNextWolf();\n      return true;\n    }\n    if (c == KEY_ENTER && _page == HomePage::BLUETOOTH) {"),
    ("begin", "  msg_preview = new MsgPreviewScreen(this, &rtc_clock);\n  setCurrScreen(splash);",
     "  msg_preview = new MsgPreviewScreen(this, &rtc_clock);\n  wolf = new WolfModule();\n  wolf->begin(this);\n  setCurrScreen(splash);"),
    ("onMessage", "  if (!(txt_type == TXT_TYPE_PLAIN || txt_type == TXT_TYPE_SIGNED_PLAIN)) return;\n",
     "  if (!(txt_type == TXT_TYPE_PLAIN || txt_type == TXT_TYPE_SIGNED_PLAIN)) return;\n  if (wolf) wolf->onMessage();\n"),
    ("onChannelMessage", "void UITask::onChannelMessageRecv(mesh::Packet *pkt, ChannelDetails& channel_details, const char* text) {\n",
     "void UITask::onChannelMessageRecv(mesh::Packet *pkt, ChannelDetails& channel_details, const char* text) {\n  if (wolf) wolf->onMessage();\n"),
    ("onControl", "void UITask::onControlDataRecv(const mesh::Packet* packet) {\n",
     "void UITask::onControlDataRecv(const mesh::Packet* packet) {\n  if (wolf) wolf->onControl(packet->payload, packet->payload_len);\n"),
    ("shutdown", "void UITask::shutdown(bool restart){\n", "void UITask::shutdown(bool restart){\n  if (wolf) wolf->save();\n"),
    ("loop", "void UITask::loop() {\n  char c = 0;\n", "void UITask::loop() {\n  char c = 0;\n  if (wolf) wolf->loop();\n"),
    ("auto off", "    if (millis() > _auto_off) {\n      _display->turnOff();",
     "    if (millis() > _auto_off && !wolfPageActive()) {   // the wolf screen never times out\n      _display->turnOff();"),
    ("double click", '  MESH_DEBUG_PRINTLN("UITask: double-click triggered");\n  checkDisplayOn(c);\n  return c;\n}\n',
     '''  MESH_DEBUG_PRINTLN("UITask: double-click triggered");
  if (_display != NULL && _display->isOn() && wolfPageActive()) {
    _display->turnOff();     // on the wolf screen, a double click turns the screen off
    return 0;
  }
  checkDisplayOn(c);
  return c;
}

bool UITask::wolfPageActive() const {
  return curr != NULL && curr == home && ((HomeScreen *) home)->onWolfPage();
}

int UITask::renderWolf(DisplayDriver& display) {
  return wolf ? wolf->render(display, getBattMilliVolts()) : 1000;
}

void UITask::wolfNextWolf() {
  if (!wolf) return;
  char desc[32];
  wolf->nextWolf(desc, sizeof(desc));
  showAlert(desc, 1800);
  _next_refresh = 0;
}
'''),
])

print("UITask.h")
patch(os.path.join(ui, "UITask.h"), [
    ("include", '#include "../NodePrefs.h"\n', '#include "../NodePrefs.h"\n#include "WolfModule.h"\n'),
    ("member", "  UIScreen* curr;\n", "  UIScreen* curr;\n  WolfModule* wolf;\n"),
    ("init", "    curr = NULL;\n", "    curr = NULL;\n    wolf = NULL;\n"),
    ("methods", "  bool isButtonPressed() const;\n",
     "  bool isButtonPressed() const;\n\n  // wolf pet\n  bool wolfPageActive() const;           // true while the wolf screen is showing\n  int  renderWolf(DisplayDriver& display);\n  void wolfNextWolf();\n"),
])
print("wolf added")
