/*
 * W A R N I N G
 * Configure EU band in Arduino\libraries\MCCI_LoRaWAN_LMIC_library\project_config\lmic_project_config.h
 * W A R N I N G
*/

// LoRaWAN Keys, should be in big-endian (aka msb).
uint8_t NWKSKEY[16] = nwkskey;
uint8_t APPSKEY[16] = appskey;
uint32_t DEVADDR = devaddr;
// DEVADDR is a 4 byte (32 bit) unsigned int, so 0xNumberInTTN, change this address for every node!

void setup() {
  Serial.begin(115200);
  Serial.println();
  uint16_t bootCount = increaseBootCount();
  boolean uplinkAvailable = canUplink();
  if (!uplinkAvailable) return sleepMCU("No uplink available");
  // Set WiFi to station mode and disconnect from an AP if it was previously connected
  WiFi.mode(WIFI_STA); // 70 ma
  WiFi.disconnect();
  int numNetworksFound = WiFi.scanNetworks();
  WiFi.mode( WIFI_OFF ); // 20 ma
  WiFi.forceSleepBegin();
  delay(1);
  Serial.println("Done scanning @ " + String(millis()));
  Serial.println(String(numNetworksFound) + " network(s) found");

  // Done scanning wifi
  if (numNetworksFound > 6) numNetworksFound = 6; 
  // Max 6 networks for 1 + 7 * 6 = 43 bytes payload size
  // Create an array of bytes 1 for voltage, (6 for MAC + 1 for RSSI) * numNetworksFound
  int payload_size = 1 + 7 * numNetworksFound;
  uint8_t payload[payload_size];
  uint8_t battery_voltage = get_battery_voltage();
  generatePayload(payload, numNetworksFound, battery_voltage);
  Serial.println("Done generating payload");

  initLoraWAN(DEVADDR, NWKSKEY, APPSKEY);
  // send_data_over_lora(mydata, array_size);
  // Serial.println("Done queuing custom byte buffer");
}

void loop() {
    unsigned long now = millis();
    if ((now & 512) != 0) {
      digitalWrite(2, HIGH);
    } else {
      digitalWrite(2, LOW);
    }
      
    os_runloop_once();
}
