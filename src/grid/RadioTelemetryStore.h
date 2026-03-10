#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

namespace grid {
namespace radio_telemetry {

struct RawPacketEntry {
  uint32_t timestamp;
  int16_t rssi;
  int8_t snr;
  uint16_t byteLen;
  char hex[73];
};

void updateMetrics(int16_t rssi, int8_t snr, uint32_t timestamp);
bool getLatestMetrics(int16_t& rssi, int8_t& snr, uint32_t& timestamp);

void recordRawPacket(uint32_t timestamp, int16_t rssi, int8_t snr, const uint8_t* raw, int len);
uint32_t packetSequence();
uint32_t packetCount();
uint32_t lastRawPacketTimestamp();
void snapshotRawPackets(std::vector<RawPacketEntry>& out, size_t maxCount);

// Counts how many times logRxRaw entered the GRID path (incremented before recordRawPacket).
// If this stays 0, the radio is not receiving any packets from other nodes.
void     bumpRxCallHits();
uint32_t getRxCallHits();

// Counts raw frames seen directly in Dispatcher::checkRecv() before MyMesh hooks.
void     bumpDispatcherRxRawHits();
uint32_t getDispatcherRxRawHits();

}  // namespace radio_telemetry
}  // namespace grid
