#include "UI_Bridge.h"

#include <Arduino.h>
#include <string.h>
#include <target.h>

UI_Bridge &UI_Bridge::getInstance() {
  static UI_Bridge instance;
  return instance;
}

UI_Bridge::UI_Bridge() {
  memset(last_message_timestamps_channels, 0, sizeof(last_message_timestamps_channels));
  memset(last_opened_timestamps_channels, 0, sizeof(last_opened_timestamps_channels));
  memset(last_message_timestamps_contacts, 0, sizeof(last_message_timestamps_contacts));
  memset(last_opened_timestamps_contacts, 0, sizeof(last_opened_timestamps_contacts));
}

const char *UI_Bridge::getNodeName() {
  return the_mesh.getNodeName();
}

uint16_t UI_Bridge::getBatteryVoltage() {
  return board.getBattMilliVolts();
}

float UI_Bridge::getLastRSSI() {
  return radio_driver.getLastRSSI();
}

float UI_Bridge::getLastSNR() {
  return radio_driver.getLastSNR();
}

uint32_t UI_Bridge::getCurrentTime() {
  return rtc_clock.getCurrentTime();
}

uint32_t UI_Bridge::getFreeHeap() {
#ifdef ESP32
  return ESP.getFreeHeap();
#else
  return 0;
#endif
}

int UI_Bridge::getNearbyNodes(AdvertPath dest[], int max_num) {
  return the_mesh.getRecentlyHeard(dest, max_num);
}

int UI_Bridge::getAvailableChannels(ChannelDetails dest[], int max_num) {
  int count = 0;
  for (int i = 0; i < 16 && count < max_num; i++) {
    if (the_mesh.getChannel(i, dest[count])) {
      count++;
    }
  }
  return count;
}

int UI_Bridge::getAvailableContacts(ContactInfo dest[], int max_num) {
  int count = 0;
  for (int i = 0; i < 100 && count < max_num; i++) {
    if (the_mesh.getContactByIdx(i, dest[count])) {
      count++;
    }
  }
  return count;
}

bool UI_Bridge::getChannel(uint8_t idx, ChannelDetails &ch) {
  return the_mesh.getChannel(idx, ch);
}

bool UI_Bridge::getContactByIdx(uint8_t idx, ContactInfo &ci) {
  return the_mesh.getContactByIdx(idx, ci);
}

int UI_Bridge::getChannelCount() {
  int count = 0;
  ChannelDetails ch;
  for (int i = 0; i < 16; i++) {
    if (the_mesh.getChannel(i, ch)) count++;
  }
  return count;
}

int UI_Bridge::getContactCount() {
  return the_mesh.getNumContacts();
}

uint32_t UI_Bridge::getPacketsRecv() {
  return radio_driver.getPacketsRecv();
}

uint32_t UI_Bridge::getPacketsSent() {
  return radio_driver.getPacketsSent();
}

uint32_t UI_Bridge::getNumSentFlood() {
  return the_mesh.getNumSentFlood();
}

uint32_t UI_Bridge::getNumRecvFlood() {
  return the_mesh.getNumRecvFlood();
}

uint32_t UI_Bridge::getNumSentDirect() {
  return the_mesh.getNumSentDirect();
}

uint32_t UI_Bridge::getNumRecvDirect() {
  return the_mesh.getNumRecvDirect();
}

uint32_t UI_Bridge::getTotalAirTime() {
  return the_mesh.getTotalAirTime();
}

uint32_t UI_Bridge::getReceiveAirTime() {
  return the_mesh.getReceiveAirTime();
}

int16_t UI_Bridge::getNoiseFloor() {
  return radio_driver.getNoiseFloor();
}

void UI_Bridge::resetStats() {
  radio_driver.resetStats();
  the_mesh.resetStats();
}

void UI_Bridge::savePrefs() {
  the_mesh.savePrefs();
}

void UI_Bridge::setChannelOpened(uint8_t idx, bool is_group) {
  uint32_t now = getCurrentTime();
  if (is_group) {
    if (idx < 16) last_opened_timestamps_channels[idx] = now;
  } else {
    if (idx < 100) last_opened_timestamps_contacts[idx] = now;
  }
}

bool UI_Bridge::isChannelUnread(uint8_t idx, bool is_group) {
  if (is_group) {
    if (idx < 16) return last_message_timestamps_channels[idx] > last_opened_timestamps_channels[idx];
  } else {
    if (idx < 100) return last_message_timestamps_contacts[idx] > last_opened_timestamps_contacts[idx];
  }
  return false;
}

void UI_Bridge::notifyNewMessage(uint8_t idx, bool is_group) {
  uint32_t now = getCurrentTime();
  if (is_group) {
    if (idx < 16) last_message_timestamps_channels[idx] = now;
  } else {
    if (idx < 100) last_message_timestamps_contacts[idx] = now;
  }
}

void UI_Bridge::switchToChannel(uint8_t idx, bool is_group) {
  // Logic to signal UITask will be implemented in UITask integration
  setChannelOpened(idx, is_group);
}

bool UI_Bridge::sendMessage(ContactInfo &ci, const char *text, uint32_t &expected_ack, uint32_t &est_timeout,
                            uint32_t &pkt_hash) {
  return the_mesh.sendMessage(ci, getCurrentTime(), 0, text, expected_ack, est_timeout, pkt_hash);
}

bool UI_Bridge::sendGroupMessage(uint8_t channel_idx, const char *text, uint32_t &pkt_hash) {
  ChannelDetails ch;
  if (the_mesh.getChannel(channel_idx, ch)) {
    return the_mesh.sendGroupMessage(getCurrentTime(), ch.channel, getNodeName(), text, strlen(text),
                                     pkt_hash);
  }
  return false;
}

uint32_t UI_Bridge::getBLEPin() {
  return the_mesh.getBLEPin();
}

bool UI_Bridge::advert() {
  return the_mesh.advert();
}

bool UI_Bridge::isLowMemory() {
  return getFreeHeap() < 30000; // 30KB threshold
}

void UI_Bridge::checkMemoryStability() {
  // Memory stability logic is handled via UITask using isLowMemory()
}
