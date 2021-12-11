#ifndef LIBS_H_INCLUDED
#define LIBS_H_INCLUDED

#include "Arduino.h"
#include <lmic.h>

boolean checkActivity();
int scanWiFi();
void sleepMCU(String reason);
uint16_t increaseBootCount();
uint16_t mountFS();
void unmountFS();
void persistDataToFlash();
int getAirtime();
boolean saveNewUplink(String spreadingFactor, boolean wasActive, boolean requestedDownlink, int payload_size);
uint32_t get_uplink_count_from_memory();
uint32_t get_downlink_count_from_memory();
uint8_t calcOnAirTime(String spreadingFactor, int payload_size);
uint8_t getState(boolean wasActive, boolean requestedDownlink);
void printSavedState();
void generatePayload(uint8_t *payload, int numNetworksFound, uint8_t battery_voltage, uint16_t bootCount);
uint8_t get_battery_voltage();
void initLoraWAN(uint32_t DEVADDR, uint8_t* NWKSKEY, uint8_t* APPSKEY, String SpreadingFactor, uint32_t uplinkCount, uint32_t downlinkCount);
dr_t getSF(String SF);
boolean waitForTransmit(int timeoutInMS);
void onEvent (ev_t ev);
uint32_t get_uplink_count();
uint32_t get_downlink_count();
void send_data_over_lora(uint8_t* data, uint8_t data_size, bool request_ack);

#endif