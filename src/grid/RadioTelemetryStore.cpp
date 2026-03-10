#include "RadioTelemetryStore.h"

#include <string.h>

#include <algorithm>

#include <freertos/FreeRTOS.h>

namespace grid {
namespace radio_telemetry {

namespace {
constexpr size_t kMaxRawPackets = 24;
constexpr size_t kHexPayloadBytes = 24;

portMUX_TYPE gStoreMux = portMUX_INITIALIZER_UNLOCKED;

bool gHasMetrics = false;
int16_t gLastRssi = -127;
int8_t gLastSnr = 0;
uint32_t gLastTimestamp = 0;
uint32_t gPacketSeq = 0;
uint32_t gPacketCount = 0;
uint32_t gLastRawTimestamp = 0;
uint32_t gRxCallHits = 0;
uint32_t gDispatcherRxRawHits = 0;

RawPacketEntry gRawPackets[kMaxRawPackets];
size_t gRawHead = 0;
size_t gRawCount = 0;

char toHexNibble(uint8_t nibble) {
  return nibble < 10 ? static_cast<char>('0' + nibble) : static_cast<char>('A' + (nibble - 10));
}
}  // namespace

void updateMetrics(int16_t rssi, int8_t snr, uint32_t timestamp) {
  portENTER_CRITICAL(&gStoreMux);
  gLastRssi = rssi;
  gLastSnr = snr;
  gLastTimestamp = timestamp;
  gHasMetrics = true;
  portEXIT_CRITICAL(&gStoreMux);
}

bool getLatestMetrics(int16_t& rssi, int8_t& snr, uint32_t& timestamp) {
  bool has = false;
  portENTER_CRITICAL(&gStoreMux);

  has = gHasMetrics;
  if (has) {
    rssi = gLastRssi;
    snr = gLastSnr;
    timestamp = gLastTimestamp;
  }
  portEXIT_CRITICAL(&gStoreMux);
  return has;
}

void recordRawPacket(uint32_t timestamp, int16_t rssi, int8_t snr, const uint8_t* raw, int len) {
  if (raw == nullptr || len <= 0) {
    return;
  }

  RawPacketEntry entry{};
  entry.timestamp = timestamp;
  entry.rssi = rssi;
  entry.snr = snr;
  entry.byteLen = static_cast<uint16_t>(len);

  const size_t capped = std::min(kHexPayloadBytes, static_cast<size_t>(len));
  size_t out = 0;
  for (size_t i = 0; i < capped && out + 2 < sizeof(entry.hex); ++i) {
    const uint8_t b = raw[i];
    entry.hex[out++] = toHexNibble((b >> 4) & 0x0F);
    entry.hex[out++] = toHexNibble(b & 0x0F);
    if (i + 1 < capped && out + 1 < sizeof(entry.hex)) {
      entry.hex[out++] = ' ';
    }
  }
  entry.hex[out] = '\0';

  portENTER_CRITICAL(&gStoreMux);
  gLastRssi = rssi;
  gLastSnr = snr;
  gLastTimestamp = timestamp;
  gHasMetrics = true;

  gRawPackets[gRawHead] = entry;
  gRawHead = (gRawHead + 1) % kMaxRawPackets;
  if (gRawCount < kMaxRawPackets) {
    gRawCount++;
  }
  gPacketCount++;
  gLastRawTimestamp = timestamp;
  gPacketSeq++;
  portEXIT_CRITICAL(&gStoreMux);
}

uint32_t packetSequence() {
  uint32_t seq = 0;
  portENTER_CRITICAL(&gStoreMux);
  seq = gPacketSeq;
  portEXIT_CRITICAL(&gStoreMux);
  return seq;
}

uint32_t packetCount() {
  uint32_t count = 0;
  portENTER_CRITICAL(&gStoreMux);
  count = gPacketCount;
  portEXIT_CRITICAL(&gStoreMux);
  return count;
}

uint32_t lastRawPacketTimestamp() {
  uint32_t ts = 0;
  portENTER_CRITICAL(&gStoreMux);
  ts = gLastRawTimestamp;
  portEXIT_CRITICAL(&gStoreMux);
  return ts;
}

void snapshotRawPackets(std::vector<RawPacketEntry>& out, size_t maxCount) {
  out.clear();
  out.reserve(maxCount);

  portENTER_CRITICAL(&gStoreMux);
  const size_t take = std::min(maxCount, gRawCount);

  for (size_t i = 0; i < take; ++i) {
    const size_t newest = (gRawHead + kMaxRawPackets - 1 - i) % kMaxRawPackets;
    out.push_back(gRawPackets[newest]);
  }
  portEXIT_CRITICAL(&gStoreMux);
}

void bumpRxCallHits() {
  portENTER_CRITICAL(&gStoreMux);
  gRxCallHits++;
  portEXIT_CRITICAL(&gStoreMux);
}

uint32_t getRxCallHits() {
  uint32_t n = 0;
  portENTER_CRITICAL(&gStoreMux);
  n = gRxCallHits;
  portEXIT_CRITICAL(&gStoreMux);
  return n;
}

void bumpDispatcherRxRawHits() {
  portENTER_CRITICAL(&gStoreMux);
  gDispatcherRxRawHits++;
  portEXIT_CRITICAL(&gStoreMux);
}

uint32_t getDispatcherRxRawHits() {
  uint32_t n = 0;
  portENTER_CRITICAL(&gStoreMux);
  n = gDispatcherRxRawHits;
  portEXIT_CRITICAL(&gStoreMux);
  return n;
}

}  // namespace radio_telemetry
}  // namespace grid


