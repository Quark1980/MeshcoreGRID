#pragma once

#include <stdint.h>
#include <string>

#include "lvgl.h"

struct MeshMessage {
  uint8_t packetType;
  int16_t rssi;
  int8_t snr;
  uint8_t hopCount;
  uint8_t timesHeard;
  uint32_t timestamp;
  uint32_t threadId;
  bool isPrivate;
  bool isLocal;
  std::string sender;
  std::string text;
};

constexpr uint8_t GRID_EVT_NODE_ADVERT = 0xA1;
constexpr uint8_t GRID_EVT_RADIO_STATS = 0xA2;
constexpr uint8_t GRID_EVT_GROUP_REPEAT = 0xA3;  // fired when a sent group packet is re-heard via mesh

class MeshApp {
public:
  virtual ~MeshApp() = default;
  virtual void release() { delete this; }

  virtual void onStart(lv_obj_t* layout) = 0;
  virtual void onLoop() = 0;
  virtual void onClose() = 0;
  virtual void onMessageReceived(MeshMessage msg) = 0;
};
