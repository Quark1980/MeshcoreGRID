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
  struct ChannelSummary {
    uint8_t id;
    std::string name;
  };

  struct ContactSummary {
    uint32_t id;
    std::string name;
    uint32_t lastSeen;
    bool heardRecently;
  };

  using Observer = std::function<void(const MeshMessage&)>;
  using ChannelProvider = std::function<void(std::vector<ChannelSummary>&)>;
  using ContactProvider = std::function<void(std::vector<ContactSummary>&)>;
  using BleStateGetter = std::function<bool()>;
  using BleToggleHandler = std::function<bool(bool)>;
  using RadioMetricsProvider = std::function<bool(int16_t&, int8_t&)>;

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
                    uint32_t timestamp = 0,
                    uint32_t threadId = 0,
                    bool isPrivate = false);

  void setChannelProvider(ChannelProvider provider);
  void setContactProvider(ContactProvider provider);
  void setBleControl(BleStateGetter getter, BleToggleHandler setter);
  void setRadioMetricsProvider(RadioMetricsProvider provider);
  bool refreshRadioMetrics();
  bool isBleEnabled() const;
  bool setBleEnabled(bool enabled);
  bool hasRadioMetrics() const;
  int16_t lastRssi() const;
  int8_t lastSnr() const;
  std::vector<ChannelSummary> getChannels() const;
  std::vector<ContactSummary> getContacts() const;

  void setThreadFilter(uint32_t id, bool isPrivate);
  void clearThreadFilter();
  bool isCurrentThread(uint32_t id, bool isPrivate) const;

  int getUnreadCount(uint32_t id, bool isPrivate) const;
  void clearUnread(uint32_t id, bool isPrivate);
  std::vector<MeshMessage> getThreadHistory(uint32_t id, bool isPrivate) const;

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
    uint32_t threadId;
    bool isPrivate;
    char sender[24];
    char text[MAX_PACKET_PAYLOAD];
  };

  MeshBridge();

  static void meshTaskTrampoline(void* ctx);
  static void uiTaskTrampoline(void* ctx);

  static bool parseTextPayload(const mesh::Packet& pkt, std::string& outText, uint32_t& outTimestamp);
  static uint32_t makeThreadKey(uint32_t id, bool isPrivate);
  bool messageMatchesFilter(const MeshMessage& msg) const;
  bool isTextPacketType(uint8_t packetType) const;
  void appendThreadHistory(const MeshMessage& msg);

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
  ChannelProvider _channelProvider;
  ContactProvider _contactProvider;
  BleStateGetter _bleStateGetter;
  BleToggleHandler _bleToggleHandler;
  RadioMetricsProvider _radioMetricsProvider;

  bool _threadFilterEnabled;
  uint32_t _threadFilterId;
  bool _threadFilterPrivate;
  bool _hasRadioMetrics;
  int16_t _lastRssi;
  int8_t _lastSnr;

  std::map<uint8_t, std::vector<Observer>> _observers;
  std::map<uint32_t, int> _unreadCounts;
  std::map<uint32_t, std::vector<MeshMessage>> _threadHistory;
  MeshApp* _activeApp;
};
