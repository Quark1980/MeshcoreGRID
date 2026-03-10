#pragma once

#include <stdint.h>
#include <stddef.h>

#include <functional>
#include <map>
#include <string>
#include <vector>

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <esp_heap_caps.h>

#include "Mesh.h"
#include "Packet.h"
#include "MeshApp.h"

class MeshBridge {
public:
  using Observer = std::function<void(const MeshMessage&)>;

  static MeshBridge& instance();

  bool begin(mesh::Mesh* meshInstance,
             TaskFunction_t meshTaskFn,
             void* meshTaskCtx,
             TaskFunction_t uiTaskFn,
             void* uiTaskCtx,
             UBaseType_t meshTaskPrio = 4,
             UBaseType_t uiTaskPrio = 3,
             uint32_t queueDepth = 32);

  void stop();

  bool sendTextFlood(const mesh::Identity& dest, const uint8_t* secret, const char* text, uint32_t senderTimestamp);

  void onPacketFromMeshCore(const mesh::Packet& pkt, int16_t rssi, int8_t snr);
  void publishEvent(uint8_t eventType,
                    const char* text,
                    const char* sender = "",
                    int16_t rssi = 0,
                    int8_t snr = 0,
                    uint32_t timestamp = 0);

  void subscribe(uint8_t packetType, Observer cb);
  void clearSubscribers(uint8_t packetType);

  bool pollForUi(MeshMessage& outMsg, TickType_t timeoutTicks = 0);
  void dispatchForUi(uint32_t maxMessagesPerTick = 6);

  void setActiveApp(class MeshApp* app);

  QueueHandle_t rxQueue() const { return _rxQueue; }

private:
  struct BridgeEvent {
    uint8_t packetType;
    int16_t rssi;
    int8_t snr;
    uint32_t timestamp;
    char sender[24];
    char text[MAX_PACKET_PAYLOAD];
  };

  MeshBridge();

  static void meshTaskTrampoline(void* ctx);
  static void uiTaskTrampoline(void* ctx);

  static bool parseTextPayload(const mesh::Packet& pkt, std::string& outText, uint32_t& outTimestamp);

  mesh::Mesh* _mesh;
  QueueHandle_t _rxQueue;
  TaskHandle_t _meshTask;
  TaskHandle_t _uiTask;

  struct BridgeTaskConfig {
    TaskFunction_t fn;
    void* ctx;
  };

  BridgeTaskConfig _meshCfg;
  BridgeTaskConfig _uiCfg;

  std::map<uint8_t, std::vector<Observer>> _observers;
  MeshApp* _activeApp;
};
