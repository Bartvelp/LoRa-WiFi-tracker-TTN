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

  // Increase bootcount
  uint16_t bootCount = increaseBootCount();
  Serial.println("#boot: " + String(bootCount));
  if (bootCount % 5 == 0) persistDataToFlash(); // Every 5 boots, save to flash

  int airtime = getAirtime();
  if (airtime > 300) return sleepMCU("No uplink available");
  // Check if we want to uplink
  boolean isActive = checkActivity();
  Serial.println("IsActive: " + String(isActive));

  // No uplink if we are inactive
  // Except every 2 hours
  if (!isActive && (bootCount % 24 != 0)) return sleepMCU("Not active");
  // We are going to uplink
  // Enable the LoRa module
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);
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
  // Do some tricks to limit SF when our uptime already is high
  if (airtime > 100 && bootCount % 3 == 0) spreadingFactor = "SF9";
  if (airtime > 200 && bootCount % 3 == 1) spreadingFactor = "SF8";
  if (airtime > 250 && bootCount % 3 == 2) spreadingFactor = "SF7";
  // When airtime is below 10.0 seconds use SF10 by default
  // 10.0 < airtime < 20.0 = 33% SF9, 66% SF10
  // 20.0 < airtime < 25.0 = 33% SF8, 33% SF9, 33% SF10
  // 25.0 < airtime        = 33% SF7, 33% SF8, 33% SF9

  if (bootCount % 12 == 0) spreadingFactor = "SF12"; // Every hour set a high SF
  // We could also determine to get a downlink, but we have no use for this currently
  boolean requestAck = false;
  // Get the stored downlink and uplink counters
  uint32_t uplinkCount = get_uplink_count_from_memory();
  uint32_t downlinkCount = get_downlink_count_from_memory();
  // Init lora with the keys, SF and counters, and queue the payload
  initLoraWAN(DEVADDR, NWKSKEY, APPSKEY, spreadingFactor, uplinkCount, downlinkCount);
  send_data_over_lora(payload, payload_size, requestAck);
  Serial.println("Done queuing custom byte buffer");
  // Wait for transmission (or timeout)
  boolean success = waitForTransmit(6000);
  Serial.println("Succesfully transmitted: " + String(success));
  // Save the new uplink
  saveNewUplink(spreadingFactor, isActive, requestAck, payload_size);
  printSavedState();
  if (bootCount % 5 == 0) persistDataToFlash(); // save the uplink as well
  sleepMCU("Done, successfully transmitted");
}

void loop() {
  // We should never get here
  Serial.println("??? We are in the main loop ???");
  delay(1000);
  // Flash onboard LED
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);
  delay(1000);
  digitalWrite(2, HIGH);
}
