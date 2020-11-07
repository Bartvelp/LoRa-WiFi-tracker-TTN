// Due to some arduino quirck, this file must contain all includes
// Main file, ttn-abp-wifi-scan.ino
#include "ESP8266WiFi.h"

// lora_function.ino
#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h> 

// persistance
#include "rtc_memory.h"
#include <LittleFS.h>

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
  .nss = 15, // Make D8/GPIO15, is nSS on ESP8266
  .rxtx = LMIC_UNUSED_PIN,      // D4/GPIO2. For placeholder only,
  .rst = LMIC_UNUSED_PIN, // Connect to esp8266 RST, or Make D2/GPIO4
  .dio = {5, 4, LMIC_UNUSED_PIN},   // D1/GPIO5; D2/GPIO4, when D4 is actually connected, ESP won't boot
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
  // Since we have not returned yet, we have reaced the timeout
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

void send_data_over_lora(uint8_t* data, uint8_t data_size, bool request_ack = false){
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