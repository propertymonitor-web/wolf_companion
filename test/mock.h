#pragma once
// Mock display and radio host shared by the PC test programs.
#include <helpers/ui/DisplayDriver.h>
#include "WolfCore.h"
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <deque>

ColorVal UIColor::window_bkg = 0, UIColor::title_bkg = 1, UIColor::title_txt = 1, UIColor::primary_txt = 1,
         UIColor::secondary_txt = 1, UIColor::warning_txt = 1, UIColor::popup_bkg = 0, UIColor::popup_txt = 1,
         UIColor::corp_blue = 1;

struct MockDisplay : DisplayDriver {
  uint8_t px[128 * 64];
  struct Txt { int x, y; std::string s; };
  std::vector<Txt> txt;
  int cx = 0, cy = 0;
  MockDisplay() : DisplayDriver(128, 64) { memset(px, 0, sizeof(px)); }
  bool isOn() override { return true; }
  void turnOn() override {}
  void turnOff() override {}
  void clear() override { memset(px, 0, sizeof(px)); txt.clear(); }
  void startFrame(ColorVal) override { clear(); }
  void setTextSize(int) override {}
  void setColor(ColorVal) override {}
  void setCursor(int x, int y) override { cx = x; cy = y; }
  void print(const char* s) override { txt.push_back({cx, cy, s}); }
  void fillRect(int x, int y, int w, int h) override {
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) set(x + i, y + j);
  }
  void drawRect(int x, int y, int w, int h) override {
    for (int i = 0; i < w; i++) { set(x + i, y); set(x + i, y + h - 1); }
    for (int j = 0; j < h; j++) { set(x, y + j); set(x + w - 1, y + j); }
  }
  void drawXbm(int x, int y, const uint8_t* bits, int w, int h) override {
    int stride = (w + 7) / 8;
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++)
      if (bits[j * stride + (i >> 3)] & (0x80 >> (i & 7))) set(x + i, y + j);
  }
  uint16_t getTextWidth(const char* s) override { return (uint16_t)(strlen(s) * 6); }
  void endFrame() override {}
  void set(int x, int y) { if (x >= 0 && x < 128 && y >= 0 && y < 64) px[y * 128 + x] = 1; }
  void dump(FILE* f, const char* label) {
    fprintf(f, "FRAME %s\n", label);
    for (int y = 0; y < 64; y++) { for (int x = 0; x < 128; x++) fputc(px[y * 128 + x] ? '1' : '0', f); fputc('\n', f); }
    for (auto& t : txt) fprintf(f, "TXT %d %d %s\n", t.x, t.y, t.s.c_str());
    fprintf(f, "END\n");
  }
};

struct MockHost : WolfHost {
  uint32_t rng, tail, prefix;
  std::vector<uint8_t> saved, backup;
  std::deque<std::vector<uint8_t>> out;
  std::string last_alert;
  MockHost(uint32_t seed, uint32_t tailv, uint32_t pre) : rng(seed), tail(tailv), prefix(pre) {}
  uint32_t random32() override { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
  uint32_t identityTail() override { return tail; }
  void ownerPrefix(uint8_t o[4]) override { memcpy(o, &prefix, 4); }
  bool loadState(uint8_t* b, int mx, int& len) override {
    if (saved.empty()) return false;
    len = (int)saved.size() < mx ? (int)saved.size() : mx;
    memcpy(b, saved.data(), len);
    return true;
  }
  void saveState(const uint8_t* b, int len) override { saved.assign(b, b + len); backup.assign(b, b + len); }
  bool loadBackup(uint8_t* b, int mx, int& len) override {
    if (backup.empty()) return false;
    len = (int)backup.size() < mx ? (int)backup.size() : mx;
    memcpy(b, backup.data(), len);
    return true;
  }
  void broadcast(const uint8_t* b, int len) override { out.push_back(std::vector<uint8_t>(b, b + len)); }
  void alert(const char* t) override { last_alert = t; printf("  [alert] %s\n", t); }
};

