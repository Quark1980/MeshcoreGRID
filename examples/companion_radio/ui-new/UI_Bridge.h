#pragma once

#include "../../../src/MeshCore.h"
#include "../../../src/helpers/ChannelDetails.h"
#include "../../../src/helpers/ContactInfo.h"
#include "../../companion_radio/MyMesh.h"
#include "../../companion_radio/NodePrefs.h"

#include <Arduino.h>
#include <stdint.h>

struct NearbyNode {
  char name[32];
  uint32_t last_heard;
  float rssi;
};

class UI_Bridge {
public:
  static UI_Bridge &getInstance();

  // System Info
  const char *getNodeName();
  uint16_t getBatteryVoltage();
  float getLastRSSI();
  float getLastSNR();
  uint32_t getCurrentTime();
  uint32_t getFreeHeap();

  // Node & Mesh Data
  int getNearbyNodes(AdvertPath dest[], int max_num);
  int getAvailableChannels(ChannelDetails dest[], int max_num);
  int getAvailableContacts(ContactInfo dest[], int max_num);
  bool getChannel(uint8_t idx, ChannelDetails &ch);
  bool getContactByIdx(uint8_t idx, ContactInfo &ci);
  int getChannelCount();
  int getContactCount();

  // Stats
  uint32_t getPacketsRecv();
  uint32_t getPacketsSent();
  uint32_t getNumSentFlood();
  uint32_t getNumRecvFlood();
  uint32_t getNumSentDirect();
  uint32_t getNumRecvDirect();
  uint32_t getTotalAirTime();
  uint32_t getReceiveAirTime();
  int16_t getNoiseFloor();
  void resetStats();

  // Actions (The "Toll Booth")
  void savePrefs();

  // Channel Selection & Unread Logic
  void setChannelOpened(uint8_t idx, bool is_group);
  bool isChannelUnread(uint8_t idx, bool is_group);
  void notifyNewMessage(uint8_t idx, bool is_group);
  void switchToChannel(uint8_t idx, bool is_group);

  // Stats & Messaging
  bool sendMessage(ContactInfo &ci, const char *text, uint32_t &expected_ack, uint32_t &est_timeout,
                   uint32_t &pkt_hash);
  bool sendGroupMessage(uint8_t channel_idx, const char *text, uint32_t &pkt_hash);

  uint32_t getBLEPin();

  bool advert();

  // Stability
  void checkMemoryStability();
  bool isLowMemory();

private:
  UI_Bridge();
  UI_Bridge(const UI_Bridge &) = delete;
  UI_Bridge &operator=(const UI_Bridge &) = delete;

  uint32_t last_message_timestamps_channels[16];
  uint32_t last_opened_timestamps_channels[16];
  uint32_t last_message_timestamps_contacts[100];
  uint32_t last_opened_timestamps_contacts[100];
};
