#pragma once
// WolfModule: glue between the wolf game and the MeshCore companion radio UI.

#include <helpers/ui/DisplayDriver.h>
#include "WolfCore.h"
#include "WolfScene.h"
#include <stddef.h>

class UITask;

class WolfModule {
public:
  WolfModule();
  ~WolfModule();

  void begin(UITask* ui);                       // load (or create) the wolf, hook up the radio
  void loop();                                  // call every UITask::loop()
  void onMessage();                             // a message arrived (any channel, or a DM)
  void onControl(const uint8_t* payload, int len);   // zero-hop control packet from another radio
  int  render(DisplayDriver& d, uint16_t batt_mv);   // draw the wolf screen; returns ms to next frame
  void nextWolf(char* desc, int desc_len);      // long press on the wolf screen: show the next wolf
  void save();
  bool setPetName(const char* text, char* reply, int reply_len);   // name the pet on screen (blank clears)
  size_t mirrorLine(uint32_t now, char* out, size_t cap, uint16_t batt_mv);   // one dashboard line (0 = nothing to send)

private:
  struct Host;                                  // WolfHost implementation (WolfModule.cpp)
  Host*      _host;
  WolfGame*  _game;
#if PET_MYST
  PetSceneBase* _scene;
  uint8_t       _scene_sp;          // which pet's pictures _scene draws (a mixed family has several)
  uint8_t       _scene_head;        // head species currently worn by _scene (0xFF none)
#else
  WolfScene* _scene;
#endif
  void*      _mirror;               // PetMirror + encoder, created on first use
  void       syncScene();           // (mystery build) switch pictures when the shown pet is another kind
};
