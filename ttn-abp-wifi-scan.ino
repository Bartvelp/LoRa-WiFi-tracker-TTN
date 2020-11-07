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
  uint16_t lastBootCount = mountFS();
  if (lastBootCount % 5 == 0) persistDataToFlash(); // Every 5 boots, save to flash

  // Increase bootcount
  uint16_t bootCount = increaseBootCount();
  Serial.println("#boot: " + String(bootCount));

  boolean uplinkAvailable = canUplink();
  if (!uplinkAvailable) return sleepMCU("No uplink available");
  // Check if we want to uplink
  boolean isActive = checkActivity();
  // No uplink if we are inactive
  // Except every 2 hours
  if (!isActive && (bootCount % 24 != 0)) return sleepMCU("Not active");
  // We are going to uplink
  // Scan surrounding wifi networks
  int numNetworksFound = scanWiFi();
  // Create payload
  // Create an array of bytes, size: 1 for voltage, (6 for MAC + 1 for RSSI) * numNetworksFound
  int payload_size = 1 + 7 * numNetworksFound;
  uint8_t payload[payload_size];
  uint8_t battery_voltage = get_battery_voltage();
  generatePayload(payload, numNetworksFound, battery_voltage);
  Serial.println("Done generating payload, size: " + String(payload_size));
  // Now determine the spreading factor we are going to use
  String spreadingFactor = "SF10";
  if (bootCount % 12 == 0) spreadingFactor = "SF12"; // Every hour set a high SF
  // We could also determine to get a downlink, but we have no use for this currently
  boolean requestAck = false;
  // Init lora with the keys and SF, and queue the payload
  initLoraWAN(DEVADDR, NWKSKEY, APPSKEY, spreadingFactor);
  send_data_over_lora(payload, payload_size, requestAck);
  Serial.println("Done queuing custom byte buffer");
  // Wait for transmission (or timeout)
  boolean success = waitForTransmit(6000);
  Serial.println("Succesfully transmitted: " + String(success));
  // Save the new uplink
  saveNewUplink(spreadingFactor, isActive, requestAck);
  printSavedState();
  sleepMCU("Done, successfully transmitted");
}

void loop() {
  // We should never get here
  Serial.println("??? We are in the main loop ???");
  delay(1000);
}
