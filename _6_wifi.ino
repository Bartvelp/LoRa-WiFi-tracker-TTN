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