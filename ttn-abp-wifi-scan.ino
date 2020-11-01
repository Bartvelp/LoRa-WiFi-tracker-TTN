#include "ESP8266WiFi.h"

/*
 * W A R N I N G
 * Configure EU band in Arduino\libraries\MCCI_LoRaWAN_LMIC_library\project_config\lmic_project_config.h
 * W A R N I N G
*/

// LoRaWAN Keys, should be in big-endian (aka msb).
uint8_t NWKSKEY[16] = {***REMOVED***};
uint8_t APPSKEY[16] = {***REMOVED***};
uint32_t DEVADDR = 0x***REMOVED***;
// DEVADDR is a 4 byte unsigned int, so 0xNumberInTTN, change this address for every node!

void setup() {
    Serial.begin(115200);
    Serial.println(F("Starting"));
    Serial.println();
    Serial.println("Start done millis: " + String(millis()));
    // Set WiFi to station mode and disconnect from an AP if it was previously connected
    WiFi.mode(WIFI_STA); // 70 ma
    WiFi.disconnect();
    int n = WiFi.scanNetworks();
    WiFi.mode( WIFI_OFF ); // 20 ma
    WiFi.forceSleepBegin();
    delay( 1 );
    Serial.println("Scan done millis: " + String(millis()));
    // Done scanning wifi
    pinMode(2, OUTPUT);

    initLoraWAN(DEVADDR, NWKSKEY, APPSKEY);
    // Start job
    unsigned char array_size = 4; // Calculate dynmically evuentually
    uint8_t mydata[array_size];
    mydata[0] = 0xFF;
    mydata[1] = 0xFF;
    mydata[2] = 0xFF;
    mydata[3] = 0xFF;
    send_data_over_lora(&mydata, array_size)
}

void loop() {
    unsigned long now;
    now = millis();
    if ((now & 512) != 0) {
      digitalWrite(2, HIGH);
    }
    else {
      digitalWrite(2, LOW);
    }
      
    os_runloop_once();
}
