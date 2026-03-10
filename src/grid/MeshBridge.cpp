#include "MeshBridge.h"

#include <string.h>

#include "MeshCore.h"

namespace {
constexpr uint32_t kBridgeTaskStack = 6144;
}

MeshBridge& MeshBridge::instance() {
  static MeshBridge s;
  return s;
}

MeshBridge::MeshBridge()
    : _mesh(nullptr),
      _rxQueue(nullptr),
      _meshTask(nullptr),
      _uiTask(nullptr),
      _meshCfg{nullptr, nullptr},
      _uiCfg{nullptr, nullptr},
      _activeApp(nullptr) {}

bool MeshBridge::begin(mesh::Mesh* meshInstance,
                       TaskFunction_t meshTaskFn,
                       void* meshTaskCtx,
                       TaskFunction_t uiTaskFn,
                       void* uiTaskCtx,
                       UBaseType_t meshTaskPrio,
                       UBaseType_t uiTaskPrio,
                       uint32_t queueDepth) {
  _mesh = meshInstance;
  _meshCfg = {meshTaskFn, meshTaskCtx};
  _uiCfg = {uiTaskFn, uiTaskCtx};

  if (_rxQueue == nullptr) {
    _rxQueue = xQueueCreate(queueDepth, sizeof(BridgeEvent));
  }
  if (_rxQueue == nullptr) {
    return false;
  }

  if (_meshCfg.fn && _meshTask == nullptr) {
    xTaskCreatePinnedToCore(meshTaskTrampoline,
                            "mesh_core0_task",
                            kBridgeTaskStack,
                            this,
                            meshTaskPrio,
                            &_meshTask,
                            0);
  }

  if (_uiCfg.fn && _uiTask == nullptr) {
    xTaskCreatePinnedToCore(uiTaskTrampoline,
                            "lvgl_core1_task",
                            kBridgeTaskStack,
                            this,
                            uiTaskPrio,
                            &_uiTask,
                            1);
  }

  return true;
}

void MeshBridge::stop() {
  if (_meshTask) {
    vTaskDelete(_meshTask);
    _meshTask = nullptr;
  }
  if (_uiTask) {
    vTaskDelete(_uiTask);
    _uiTask = nullptr;
  }
  if (_rxQueue) {
    vQueueDelete(_rxQueue);
    _rxQueue = nullptr;
  }
}

bool MeshBridge::sendTextFlood(const mesh::Identity& dest,
                               const uint8_t* secret,
                               const char* text,
                               uint32_t senderTimestamp) {
  if (_mesh == nullptr || text == nullptr) {
    return false;
  }

  const size_t textLen = strnlen(text, MAX_PACKET_PAYLOAD - 6);
  const size_t payloadLen = 5 + textLen;
  if (payloadLen > MAX_PACKET_PAYLOAD) {
    return false;
  }

  uint8_t payload[MAX_PACKET_PAYLOAD] = {0};
  memcpy(payload, &senderTimestamp, 4);
  payload[4] = 0;
  memcpy(&payload[5], text, textLen);

  mesh::Packet* pkt = _mesh->createDatagram(PAYLOAD_TYPE_TXT_MSG, dest, secret, payload, payloadLen);
  if (pkt == nullptr) {
    return false;
  }

  _mesh->sendFlood(pkt);
  return true;
}

void MeshBridge::onPacketFromMeshCore(const mesh::Packet& pkt, int16_t rssi, int8_t snr) {
  BridgeEvent ev = {};
  ev.packetType = pkt.getPayloadType();
  ev.rssi = rssi;
  ev.snr = snr;

  uint32_t ts = 0;
  std::string txt;
  parseTextPayload(pkt, txt, ts);
  ev.timestamp = ts;
  strncpy(ev.text, txt.c_str(), sizeof(ev.text) - 1);

  if (_rxQueue != nullptr) {
    xQueueSend(_rxQueue, &ev, 0);
  }
}

void MeshBridge::publishEvent(uint8_t eventType,
                              const char* text,
                              const char* sender,
                              int16_t rssi,
                              int8_t snr,
                              uint32_t timestamp) {
  if (_rxQueue == nullptr) {
    return;
  }

  BridgeEvent ev = {};
  ev.packetType = eventType;
  ev.rssi = rssi;
  ev.snr = snr;
  ev.timestamp = timestamp;
  if (sender) {
    strncpy(ev.sender, sender, sizeof(ev.sender) - 1);
  }
  if (text) {
    strncpy(ev.text, text, sizeof(ev.text) - 1);
  }
  xQueueSend(_rxQueue, &ev, 0);
}

void MeshBridge::subscribe(uint8_t packetType, Observer cb) {
  _observers[packetType].push_back(cb);
}

void MeshBridge::clearSubscribers(uint8_t packetType) {
  _observers.erase(packetType);
}

bool MeshBridge::pollForUi(MeshMessage& outMsg, TickType_t timeoutTicks) {
  if (_rxQueue == nullptr) {
    return false;
  }

  BridgeEvent ev = {};
  if (xQueueReceive(_rxQueue, &ev, timeoutTicks) != pdTRUE) {
    return false;
  }

  outMsg.packetType = ev.packetType;
  outMsg.rssi = ev.rssi;
  outMsg.snr = ev.snr;
  outMsg.timestamp = ev.timestamp;
  outMsg.sender = ev.sender;
  outMsg.text = ev.text;
  return true;
}

void MeshBridge::dispatchForUi(uint32_t maxMessagesPerTick) {
  MeshMessage msg;
  uint32_t count = 0;

  while (count < maxMessagesPerTick && pollForUi(msg, 0)) {
    if (_activeApp != nullptr) {
      _activeApp->onMessageReceived(msg);
    }

    auto it = _observers.find(msg.packetType);
    if (it != _observers.end()) {
      for (auto& cb : it->second) {
        cb(msg);
      }
    }
    ++count;
  }
}

void MeshBridge::setActiveApp(MeshApp* app) {
  _activeApp = app;
}

void MeshBridge::meshTaskTrampoline(void* ctx) {
  auto* self = static_cast<MeshBridge*>(ctx);
  if (self && self->_meshCfg.fn) {
    self->_meshCfg.fn(self->_meshCfg.ctx);
  }
  vTaskDelete(nullptr);
}

void MeshBridge::uiTaskTrampoline(void* ctx) {
  auto* self = static_cast<MeshBridge*>(ctx);
  if (self && self->_uiCfg.fn) {
    self->_uiCfg.fn(self->_uiCfg.ctx);
  }
  vTaskDelete(nullptr);
}

bool MeshBridge::parseTextPayload(const mesh::Packet& pkt, std::string& outText, uint32_t& outTimestamp) {
  outText.clear();
  outTimestamp = 0;

  if (pkt.getPayloadType() != PAYLOAD_TYPE_TXT_MSG || pkt.payload_len < 5) {
    return false;
  }

  memcpy(&outTimestamp, pkt.payload, 4);
  const char* textStart = reinterpret_cast<const char*>(&pkt.payload[5]);
  const size_t textMax = pkt.payload_len - 5;
  outText.assign(textStart, strnlen(textStart, textMax));
  return true;
}
