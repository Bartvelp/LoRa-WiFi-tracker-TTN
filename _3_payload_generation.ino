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
