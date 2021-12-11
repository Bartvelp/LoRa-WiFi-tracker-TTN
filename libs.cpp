#include "Arduino.h"
#include "libs.h"

// Activity
boolean checkActivity() {
  pinMode(5, INPUT);
  boolean isActive = digitalRead(5);
  // Now we sink the current
  pinMode(5, OUTPUT);
  digitalWrite(5, LOW);
  return isActive;
}

// scanWiFi
#include "ESP8266WiFi.h"

#define MAX_NETWORKS 6
// Max 6 networks for 1 + 7 * 6 = 43 bytes payload size

int scanWiFi() {
  // Set WiFi to station mode and disconnect from an AP if it was previously connected
  WiFi.mode(WIFI_STA); // 70 ma
  WiFi.disconnect();
  int numNetworksFound = WiFi.scanNetworks();
  WiFi.mode(WIFI_OFF); // 20 ma
  WiFi.forceSleepBegin();
  delay(1);
  Serial.println("Done scanning @ " + String(millis()));
  Serial.println(String(numNetworksFound) + " network(s) found");

  // Done scanning wifi
  if (numNetworksFound > MAX_NETWORKS) numNetworksFound = MAX_NETWORKS;
  return numNetworksFound;
}

// Sleep
void sleepMCU(String reason) {
  Serial.println("Going to sleep because: " + reason + " @ms: " + String(millis()));
  unmountFS();
  // We always sleep 5 minutes, a lot of calculations are based on this
  ESP.deepSleep(5 * 60 * 1e6);
}

// persistance
#include "rtc_memory.h"
#include <LittleFS.h>

#define NUM_STORED_UPLINKS 120

typedef struct {
  uint16_t bootup;
  uint8_t time; // in hundreds of ms, aka SF7 = 1, SF12 = 24
  uint8_t state; // 0b11111111 requestedDownlink; wasActive;
} Uplink; // Max 5 bytes

// max 508 bytes available in RTC mem
typedef struct {
  uint16_t bootCounter;
  uint32_t uplinkCount;
  uint32_t downlinkCount;
  Uplink lastUplinks[NUM_STORED_UPLINKS];
} StoredData;

RtcMemory rtcMemory("/persistance");

uint16_t increaseBootCount() {
  StoredData *storedData = rtcMemory.getData<StoredData>();
  // First increase the boot counter by one
  // Boot counter starts at 1 so we can easily find the empty entries
  if (storedData->bootCounter == 65535) storedData->bootCounter = 1;
  else storedData->bootCounter++;
  rtcMemory.save();
  return storedData->bootCounter;
}

uint16_t mountFS() {
  LittleFS.begin();
  bool result = rtcMemory.begin();
  StoredData *storedData = rtcMemory.getData<StoredData>();
  return storedData->bootCounter;
}

void unmountFS() {
  // Do this here to seperate concerns
  LittleFS.end();
}

void persistDataToFlash() {
  boolean success = rtcMemory.persist();
  Serial.println("Persisted to flash: " + String(success));
}

int getAirtime() {
  // Can uplink in terms of airtime in the past 24 hours
  // Retrieve the data from RTC memory
  StoredData *storedData = rtcMemory.getData<StoredData>();

  // Now filter for the uplinks in the last 24 hours (to abide TTN policies) 
  // First count the number of uplinks older than 24 hours
  int airTimeInLast24Hours = 0; // in hundreds of ms

  for (int i = 0; i < NUM_STORED_UPLINKS; i++) {
    Uplink currentUplink = storedData->lastUplinks[i];
    int bootupsAgo = storedData->bootCounter - currentUplink.bootup;
    int bootupsInAday = 288;
    if (bootupsAgo < bootupsInAday) {
      // This is an uplink from the last 24 hours
      airTimeInLast24Hours += currentUplink.time;
    }
  }
  // We can have max 30 seconds in 24 hours, so return true below 30 seconds
  Serial.println("Got airtime: " + String(airTimeInLast24Hours));
  return airTimeInLast24Hours;
}

boolean saveNewUplink(String spreadingFactor, boolean wasActive, boolean requestedDownlink, int payload_size) {
  StoredData *storedData = rtcMemory.getData<StoredData>();
  // storedData->lastUplinks = [older, newer]
  // get the first index that is free to fill
  int firstFreeIndex = -1;
  for (int i = 0; i < NUM_STORED_UPLINKS; i++) {
    Uplink currentUplink = storedData->lastUplinks[i];
    if (currentUplink.bootup == 0) {
      firstFreeIndex = i;
      break;
    }
  }
  if (firstFreeIndex == -1) {
    // We need to remove the oldest entry, aka the first 
    // Shift everything left to push out the first entry
    // Don't do the last one, we store our new data in that one
    for (int i = 0; i < (NUM_STORED_UPLINKS - 1); i++) {
      // Get the uplink over 1 place to the right
      int nextUplinkIndex = i + 1;
      Uplink nextUplink = storedData->lastUplinks[nextUplinkIndex];
      storedData->lastUplinks[i] = nextUplink; // save it in it's new place
    }
    firstFreeIndex = NUM_STORED_UPLINKS - 1; // This one is now available
  }

  // Build new uplink
  Uplink newUplink;
  newUplink.bootup = storedData->bootCounter;
  newUplink.time = calcOnAirTime(spreadingFactor, payload_size);
  newUplink.state = getState(wasActive, requestedDownlink);
  // save new uplink
  storedData->lastUplinks[firstFreeIndex] = newUplink;
  // Get the uplink and downlink count from LMIC
  storedData->uplinkCount = get_uplink_count();
  storedData->downlinkCount = get_downlink_count();
  rtcMemory.save(); // Store it in RTC memory
}

uint32_t get_uplink_count_from_memory() {
  StoredData *storedData = rtcMemory.getData<StoredData>();
  return storedData->uplinkCount;
}

uint32_t get_downlink_count_from_memory() {
  StoredData *storedData = rtcMemory.getData<StoredData>();
  return storedData->downlinkCount;
}

uint8_t calcOnAirTime(String spreadingFactor, int payload_size) {
  payload_size = payload_size + 13; // Add lorawan bytes
  int sf = spreadingFactor.substring(2).toInt();
  int low_data_rate = (sf >= 11) ? 1 : 0;

  float ms_per_symbol = (pow(2.0, sf) / (125 * 1000)) * 1000; //ms
  float ms_in_preamble = (8 + 4.25) * ms_per_symbol;          //ms

  int step_size = ceil((8 * payload_size - 4 * sf + 28 + 16) / (4 * (sf - 2 * low_data_rate)));
  // Unknown why but this is 1 too low;
  step_size++;
  int num_symbols_in_payload = 8 + max(step_size * (1 + 4), 0);
  float ms_on_air = ms_in_preamble + (num_symbols_in_payload * ms_per_symbol);
  return ceil(ms_on_air / 100);
}

uint8_t getState(boolean wasActive, boolean requestedDownlink) {
  // bit[7] = wasActive
  // bit[6] = requestedDownlink
  if (wasActive) {
    if (requestedDownlink) return 0b00000011;
    else return 0b00000001;
  } else {
    if (requestedDownlink) return 0b00000010;
    else return 0b00000000;
  }
}

void printSavedState() {
  // Retrieve the data from RTC memory
  StoredData *storedData = rtcMemory.getData<StoredData>();
  Serial.println("-----------------");
  Serial.println("SavedState:");
  Serial.println("Bootcounter: " + String(storedData->bootCounter));
  Serial.println("uplinkCount: " + String(storedData->uplinkCount));
  Serial.println("downlinkCount: " + String(storedData->downlinkCount));

  for (int i = 0; i < NUM_STORED_UPLINKS; i++) {
    Uplink curUp = storedData->lastUplinks[i];
    if (curUp.bootup == 0) continue;
    String logString = "Uplink[" + String(i) + "] boot, uptime, state: " + String(curUp.bootup) + " ";
    logString += String(curUp.time) + " " + String(curUp.state);
    Serial.println(logString);
  }
  Serial.println("-----------------");
}


// payload generator
void generatePayload(uint8_t *payload, int numNetworksFound, uint8_t battery_voltage, uint16_t bootCount) {
  payload[0] = battery_voltage;
  payload[1] = highByte(bootCount);
  payload[2] = lowByte(bootCount);
  for (int i = 0; i < numNetworksFound; i++) {
    // Print some stats about the wifi networks
    Serial.println(WiFi.SSID(i) + " MAC: " + WiFi.BSSIDstr(i) + " dBm: " + String(WiFi.RSSI(i)));
    // WiFi.BSSID(i) returns a pointer to the memory location (an uint8_t array with the size of 6 elements) where the BSSID is saved.
    for (int j = 0; j < 6; j++) {
      // Loop over the wifi MAC address
      uint8_t *macAddress = WiFi.BSSID(i);
      int payloadIndex = 1 + 2 + i * 7 + j;
      payload[payloadIndex] = macAddress[j];
    }
    // Set RSSI as a single byte
    int payloadIndex = 1 + 2 + i * 7 + 6;
    uint8_t signalStrength = 200 + WiFi.RSSI(i); // Create a valid uint8_t, revert in backend
    payload[payloadIndex] = signalStrength;
  }
}

// Battery
uint8_t get_battery_voltage() {
  // TODO: implement this
  uint16_t ADC_reading = analogRead(A0);
  Serial.println("Got ADC: " + String(ADC_reading));
  float Vbat = ((ADC_reading / 1023.0) * (510 + 150)) / 150;
  Vbat = Vbat - 0.1; // Correction factor
  Serial.println("Got Vbat: " + String(Vbat, 2));
  // Ranges from 2.0 to 4.5V, turn in => 2.0 == 0, 4.5 == 250
  uint8_t batteryByte = (Vbat - 2) * 100;
  Serial.println("Got batbyte: " + String(batteryByte));
  return batteryByte;
}


// LoRa
// lora_function.ino
#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h> 


// These callbacks are only used in over-the-air activation, so they are
// left empty here (we cannot leave them out completely unless
// DISABLE_JOIN is set in arduino-lmic/project_config/lmic_project_config.h,
// otherwise the linker will complain).
void os_getArtEui (u1_t* buf) { }
void os_getDevEui (u1_t* buf) { }
void os_getDevKey (u1_t* buf) { }

boolean done_transmitting = false;
boolean received_ack = false;
// Pin mapping
const lmic_pinmap lmic_pins = {
  .nss = 0, // Make D8/GPIO15, is nSS on ESP8266
  .rxtx = LMIC_UNUSED_PIN,      // D4/GPIO2. For placeholder only,
  .rst = LMIC_UNUSED_PIN, // Connect to esp8266 RST, or Make D2/GPIO4
  .dio = {15, 15, LMIC_UNUSED_PIN},   // D1/GPIO5; D2/GPIO4, when D4 is actually connected, ESP won't boot
};

void initLoraWAN(uint32_t DEVADDR, uint8_t* NWKSKEY, uint8_t* APPSKEY, String SpreadingFactor, uint32_t uplinkCount, uint32_t downlinkCount) {
  // LMIC init
  os_init();
  // Reset the MAC state. Session and pending data transfers will be discarded.
  LMIC_reset();

  LMIC_setSession(0x13, DEVADDR, NWKSKEY, APPSKEY);
  // Set up the channels used by the Things Network, which corresponds
  // to the defaults of most gateways. Without this, only three base
  // channels from the LoRaWAN specification are used, which certainly
  // works, so it is good for debugging, but can overload those
  // frequencies, so be sure to configure the full frequency range of
  // your network here (unless your network autoconfigures them).
  // Setting up channels should happen after LMIC_setSession, as that
  // configures the minimal channel set.
  Serial.println("Defined EU bands");
  LMIC_setupChannel(0, 868100000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI);  // g-band
  LMIC_setupChannel(1, 868300000, DR_RANGE_MAP(DR_SF12, DR_SF7B), BAND_CENTI); // g-band
  LMIC_setupChannel(2, 868500000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI);  // g-band
  LMIC_setupChannel(3, 867100000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI);  // g-band
  LMIC_setupChannel(4, 867300000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI);  // g-band
  LMIC_setupChannel(5, 867500000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI);  // g-band
  LMIC_setupChannel(6, 867700000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI);  // g-band
  LMIC_setupChannel(7, 867900000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI);  // g-band867.9
  LMIC_setupChannel(8, 868800000, DR_RANGE_MAP(DR_FSK, DR_FSK), BAND_MILLI);   // g2-band
  // TTN defines an additional channel at 869.525Mhz using SF9 for class B
  // devices' ping slots. LMIC does not have an easy way to define set this
  // frequency and support for class B is spotty and untested, so this
  // frequency is not configured here.

  // Disable link check validation
  LMIC_setLinkCheckMode(0);
  LMIC_setAdrMode(false); // disable ADR downlinks, since this node is mobile
  // TTN uses SF9 for its RX2 window.
  LMIC.dn2Dr = DR_SF9;
  LMIC.seqnoUp = uplinkCount; // set framecounter
  LMIC.seqnoDn = downlinkCount;

  // Set data rate and transmit power for uplink (note: txpow seems to be ignored by the library)
  LMIC_setDrTxpow(getSF(SpreadingFactor), 20);
}

dr_t getSF(String SF) {
  if (SF == "SF7") return DR_SF7;
  if (SF == "SF8") return DR_SF8;
  if (SF == "SF9") return DR_SF9;
  if (SF == "SF10") return DR_SF10;
  if (SF == "SF11") return DR_SF11;
  if (SF == "SF12") return DR_SF12;
}

boolean waitForTransmit(int timeoutInMS) {
  // Returns whether transmit was successfull, aka false at timeout
  uint32_t startTime = millis();
  while (millis() - startTime < timeoutInMS) {
    os_runloop_once();
    yield();
    if (done_transmitting) return true;
  }
  // Since we have not returned yet, we have reached the timeout
  return false;
}

void onEvent (ev_t ev) {
    Serial.print(millis());
    Serial.print(": ");
    switch(ev) {
        case EV_SCAN_TIMEOUT:
            Serial.println(F("EV_SCAN_TIMEOUT"));
            break;
        case EV_BEACON_FOUND:
            Serial.println(F("EV_BEACON_FOUND"));
            break;
        case EV_BEACON_MISSED:
            Serial.println(F("EV_BEACON_MISSED"));
            break;
        case EV_BEACON_TRACKED:
            Serial.println(F("EV_BEACON_TRACKED"));
            break;
        case EV_JOINING:
            Serial.println(F("EV_JOINING"));
            break;
        case EV_JOINED:
            Serial.println(F("EV_JOINED"));
            break;
        case EV_JOIN_FAILED:
            Serial.println(F("EV_JOIN_FAILED"));
            break;
        case EV_REJOIN_FAILED:
            Serial.println(F("EV_REJOIN_FAILED"));
            break;
        case EV_TXCOMPLETE:
            Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
            done_transmitting = true;
            if (LMIC.txrxFlags & TXRX_ACK)
              Serial.println(F("Received ack"));
              received_ack = true;
            if (LMIC.dataLen) {
              Serial.println(F("Received "));
              Serial.println(LMIC.dataLen);
              Serial.println(F(" bytes of payload"));
            }
            break;
        case EV_LOST_TSYNC:
            Serial.println(F("EV_LOST_TSYNC"));
            break;
        case EV_RESET:
            Serial.println(F("EV_RESET"));
            break;
        case EV_RXCOMPLETE:
            // data received in ping slot
            Serial.println(F("EV_RXCOMPLETE"));
            break;
        case EV_LINK_DEAD:
            Serial.println(F("EV_LINK_DEAD"));
            break;
        case EV_LINK_ALIVE:
            Serial.println(F("EV_LINK_ALIVE"));
            break;
        case EV_TXSTART:
            Serial.println(F("EV_TXSTART"));
            break;
        default:
            Serial.print(F("Unknown event: "));
            Serial.println((unsigned) ev);
            break;
    }
}

uint32_t get_uplink_count() {
  return LMIC.seqnoUp;
}
uint32_t get_downlink_count() {
  return LMIC.seqnoDn;
}

void send_data_over_lora(uint8_t* data, uint8_t data_size, bool request_ack){
    int ackRequest = request_ack ? 1 : 0;
    // Check if there is not a current TX/RX job running
    if (LMIC.opmode & OP_TXRXPEND) {
        Serial.println(F("OP_TXRXPEND, not sending"));
    } else {
        // Prepare upstream data transmission at the next possible time.
        LMIC_setTxData2(1, data, data_size, ackRequest); // (u1_t port, xref2u1_t data, u1_t dlen, u1_t confirmed), set confirmed to 1 to get an ack
        Serial.println(F("Packet queued"));
    }
}
