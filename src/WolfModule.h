#pragma once
// WolfModule: glue between the wolf game and the MeshCore companion radio UI.

#include <helpers/ui/DisplayDriver.h>
#include "WolfCore.h"
#include "WolfScene.h"

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

private:
  struct Host;                                  // WolfHost implementation (WolfModule.cpp)
  Host*      _host;
  WolfGame*  _game;
  WolfScene* _scene;
};
