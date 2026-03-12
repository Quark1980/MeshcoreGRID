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
  _txQueue(nullptr),
      _meshTask(nullptr),
      _uiTask(nullptr),
      _meshCfg{nullptr, nullptr},
      _uiCfg{nullptr, nullptr},
  _channelProvider(nullptr),
  _contactProvider(nullptr),
  _bleStateGetter(nullptr),
  _bleConnectionGetter(nullptr),
  _bleToggleHandler(nullptr),
  _radioMetricsProvider(nullptr),
  _threadFilterEnabled(false),
  _threadFilterId(0),
  _threadFilterPrivate(false),
      _hasRadioMetrics(false),
      _lastRssi(-127),
      _lastSnr(0),
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
  _bootNodeAdverts.clear();

  if (_rxQueue == nullptr) {
    _rxQueue = xQueueCreate(queueDepth, sizeof(BridgeEvent));
  }
  if (_rxQueue == nullptr) {
    return false;
  }

  if (_txQueue == nullptr) {
    _txQueue = xQueueCreate(queueDepth, sizeof(OutboxItem));
  }
  if (_txQueue == nullptr) {
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
  if (_txQueue) {
    vQueueDelete(_txQueue);
    _txQueue = nullptr;
  }
  _bootNodeAdverts.clear();
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

bool MeshBridge::enqueueOutboxText(uint32_t threadId, bool isPrivate, const char* text, uint32_t timestamp) {
  if (_txQueue == nullptr || text == nullptr || text[0] == '\0') {
    return false;
  }

  OutboxItem item{};
  item.threadId = threadId;
  item.isPrivate = isPrivate;
  item.timestamp = timestamp;
  strncpy(item.text, text, sizeof(item.text) - 1);

  return xQueueSend(_txQueue, &item, 0) == pdTRUE;
}

bool MeshBridge::dequeueOutboxText(OutboxItem& outItem, TickType_t timeoutTicks) {
  if (_txQueue == nullptr) {
    return false;
  }
  return xQueueReceive(_txQueue, &outItem, timeoutTicks) == pdTRUE;
}

MeshMessage MeshBridge::recordLocalMessage(uint32_t threadId,
                                          bool isPrivate,
                                          const char* sender,
                                          const char* text,
                                          uint32_t timestamp) {
  MeshMessage msg{};
  msg.packetType = isPrivate ? PAYLOAD_TYPE_TXT_MSG : PAYLOAD_TYPE_GRP_TXT;
  msg.rssi = 0;
  msg.snr = 0;
  msg.hopCount = 0;
  msg.timesHeard = 0;
  msg.timestamp = timestamp;
  msg.threadId = threadId;
  msg.isPrivate = isPrivate;
  msg.isLocal = true;
  msg.sender = sender ? sender : "You";
  msg.text = text ? text : "";
  appendThreadHistory(msg);
  return msg;
}

void MeshBridge::onPacketFromMeshCore(const mesh::Packet& pkt, int16_t rssi, int8_t snr) {
  BridgeEvent ev = {};
  ev.packetType = pkt.getPayloadType();
  ev.rssi = rssi;
  ev.snr = snr;
  ev.hopCount = pkt.getPathHashCount();

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
                              uint32_t timestamp,
                              uint32_t threadId,
                              bool isPrivate) {
  if (_rxQueue == nullptr) {
    return;
  }

  BridgeEvent ev = {};
  ev.packetType = eventType;
  ev.rssi = rssi;
  ev.snr = snr;
  ev.hopCount = 0;
  ev.timestamp = timestamp;
  ev.threadId = threadId;
  ev.isPrivate = isPrivate;
  if (sender) {
    strncpy(ev.sender, sender, sizeof(ev.sender) - 1);
  }
  if (text) {
    strncpy(ev.text, text, sizeof(ev.text) - 1);
  }

  if (eventType == GRID_EVT_NODE_ADVERT) {
    NodeAdvertSummary advert;
    advert.sender = sender ? sender : "";
    advert.text = text ? text : "";
    advert.timestamp = timestamp;
    _bootNodeAdverts.push_back(std::move(advert));
  }

  xQueueSend(_rxQueue, &ev, 0);
}

void MeshBridge::subscribe(uint8_t packetType, Observer cb) {
  _observers[packetType].push_back(cb);
}

void MeshBridge::clearSubscribers(uint8_t packetType) {
  _observers.erase(packetType);
}

void MeshBridge::setChannelProvider(ChannelProvider provider) {
  _channelProvider = provider;
}

void MeshBridge::setContactProvider(ContactProvider provider) {
  _contactProvider = provider;
}

void MeshBridge::setBleControl(BleStateGetter getter, BleToggleHandler setter) {
  _bleStateGetter = getter;
  _bleToggleHandler = setter;
}

void MeshBridge::setBleConnectionGetter(BleConnectionGetter getter) {
  _bleConnectionGetter = getter;
}

void MeshBridge::setRadioMetricsProvider(RadioMetricsProvider provider) {
  _radioMetricsProvider = provider;
}

bool MeshBridge::refreshRadioMetrics() {
  if (!_radioMetricsProvider) {
    return false;
  }

  int16_t rssi = 0;
  int8_t snr = 0;
  if (!_radioMetricsProvider(rssi, snr)) {
    return false;
  }

  _lastRssi = rssi;
  _lastSnr = snr;
  _hasRadioMetrics = true;
  return true;
}

bool MeshBridge::isBleEnabled() const {
  if (_bleStateGetter) {
    return _bleStateGetter();
  }
  return false;
}

bool MeshBridge::isBleConnected() const {
  if (_bleConnectionGetter) {
    return _bleConnectionGetter();
  }
  return false;
}

bool MeshBridge::setBleEnabled(bool enabled) {
  if (_bleToggleHandler) {
    return _bleToggleHandler(enabled);
  }
  return false;
}

bool MeshBridge::hasRadioMetrics() const {
  return _hasRadioMetrics;
}

int16_t MeshBridge::lastRssi() const {
  return _lastRssi;
}

int8_t MeshBridge::lastSnr() const {
  return _lastSnr;
}

std::vector<MeshBridge::ChannelSummary> MeshBridge::getChannels() const {
  std::vector<ChannelSummary> channels;
  if (_channelProvider) {
    _channelProvider(channels);
  }
  return channels;
}

std::vector<MeshBridge::ContactSummary> MeshBridge::getContacts() const {
  std::vector<ContactSummary> contacts;
  if (_contactProvider) {
    _contactProvider(contacts);
  }
  return contacts;
}

std::vector<MeshBridge::NodeAdvertSummary> MeshBridge::getBootNodeAdverts() const {
  return _bootNodeAdverts;
}

void MeshBridge::setThreadFilter(uint32_t id, bool isPrivate) {
  _threadFilterEnabled = true;
  _threadFilterId = id;
  _threadFilterPrivate = isPrivate;
  clearUnread(id, isPrivate);
}

void MeshBridge::clearThreadFilter() {
  _threadFilterEnabled = false;
}

bool MeshBridge::isCurrentThread(uint32_t id, bool isPrivate) const {
  return _threadFilterEnabled && _threadFilterId == id && _threadFilterPrivate == isPrivate;
}

int MeshBridge::getUnreadCount(uint32_t id, bool isPrivate) const {
  const uint32_t key = makeThreadKey(id, isPrivate);
  auto it = _unreadCounts.find(key);
  if (it == _unreadCounts.end()) {
    return 0;
  }
  return it->second;
}

int MeshBridge::getTotalUnreadCount() const {
  int total = 0;
  for (const auto& entry : _unreadCounts) {
    total += entry.second;
  }
  return total;
}

void MeshBridge::clearUnread(uint32_t id, bool isPrivate) {
  const uint32_t key = makeThreadKey(id, isPrivate);
  _unreadCounts.erase(key);
}

std::vector<MeshMessage> MeshBridge::getThreadHistory(uint32_t id, bool isPrivate) const {
  const uint32_t key = makeThreadKey(id, isPrivate);
  auto it = _threadHistory.find(key);
  if (it == _threadHistory.end()) {
    return {};
  }
  return it->second;
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
  outMsg.hopCount = ev.hopCount;
  outMsg.timesHeard = 0;
  outMsg.timestamp = ev.timestamp;
  outMsg.threadId = ev.threadId;
  outMsg.isPrivate = ev.isPrivate;
  outMsg.isLocal = false;
  outMsg.sender = ev.sender;
  outMsg.text = ev.text;
  return true;
}

void MeshBridge::dispatchForUi(uint32_t maxMessagesPerTick) {
  MeshMessage msg;
  uint32_t count = 0;

  while (count < maxMessagesPerTick && pollForUi(msg, 0)) {
    bool mergedIntoLocal = false;
    _lastRssi = msg.rssi;
    _lastSnr = msg.snr;
    _hasRadioMetrics = true;

    if (isTextPacketType(msg.packetType)) {
      mergedIntoLocal = appendThreadHistory(msg, &msg);
    }

    if (isTextPacketType(msg.packetType) && !msg.isLocal && !_threadFilterEnabled) {
      const uint32_t key = makeThreadKey(msg.threadId, msg.isPrivate);
      _unreadCounts[key]++;
    } else if (isTextPacketType(msg.packetType) && !msg.isLocal && !messageMatchesFilter(msg)) {
      const uint32_t key = makeThreadKey(msg.threadId, msg.isPrivate);
      _unreadCounts[key]++;
    }

    if (_activeApp != nullptr && (messageMatchesFilter(msg) || mergedIntoLocal)) {
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

uint32_t MeshBridge::makeThreadKey(uint32_t id, bool isPrivate) {
  return (id & 0x7FFFFFFF) | (isPrivate ? 0x80000000u : 0u);
}

bool MeshBridge::messageMatchesFilter(const MeshMessage& msg) const {
  if (!_threadFilterEnabled) {
    return true;
  }
  if (!isTextPacketType(msg.packetType)) {
    return true;
  }
  return msg.threadId == _threadFilterId && msg.isPrivate == _threadFilterPrivate;
}

bool MeshBridge::isTextPacketType(uint8_t packetType) const {
  return packetType == PAYLOAD_TYPE_TXT_MSG || packetType == PAYLOAD_TYPE_GRP_TXT;
}

bool MeshBridge::appendThreadHistory(const MeshMessage& msg, MeshMessage* resolvedMsg) {
  constexpr size_t kMaxPerThread = 24;
  constexpr size_t kMaxThreads = 16;
  constexpr uint32_t kEchoTimestampToleranceMs = 5000;

  const uint32_t key = makeThreadKey(msg.threadId, msg.isPrivate);
  auto& history = _threadHistory[key];

  if (!msg.isLocal) {
    for (auto it = history.rbegin(); it != history.rend(); ++it) {
      auto& entry = *it;
      if (!entry.isLocal) {
        continue;
      }
      if (entry.text != msg.text) {
        continue;
      }

      const uint32_t tsA = entry.timestamp;
      const uint32_t tsB = msg.timestamp;
      const uint32_t diff = (tsA > tsB) ? (tsA - tsB) : (tsB - tsA);
      const bool timestampClose = (tsA == tsB) || (diff <= kEchoTimestampToleranceMs);
      const bool fallbackToLatestSameText = (it == history.rbegin() && entry.timesHeard == 0);
      if (!timestampClose && !fallbackToLatestSameText) {
        continue;
      }

      entry.timesHeard = static_cast<uint8_t>(entry.timesHeard + 1);
      entry.hopCount = msg.hopCount;
      entry.snr = msg.snr;
      entry.rssi = msg.rssi;

      if (resolvedMsg != nullptr) {
        *resolvedMsg = entry;
      }
      return true;
    }
  }

  history.push_back(msg);
  if (history.size() > kMaxPerThread) {
    history.erase(history.begin(), history.begin() + (history.size() - kMaxPerThread));
  }

  if (_threadHistory.size() > kMaxThreads) {
    auto oldest = _threadHistory.begin();
    size_t minSize = oldest->second.size();
    for (auto it = _threadHistory.begin(); it != _threadHistory.end(); ++it) {
      if (it->second.size() < minSize) {
        minSize = it->second.size();
        oldest = it;
      }
    }
    _threadHistory.erase(oldest);
  }

  if (resolvedMsg != nullptr) {
    *resolvedMsg = history.back();
  }
  return false;
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
