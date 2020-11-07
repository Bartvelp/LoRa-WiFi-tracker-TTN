void sleepMCU(String reason) {
  Serial.println("Going to sleep because: " + reason + " @ms: " + String(millis()));
  unmountFS();
  // We always sleep 5 minutes, a lot of calculations are based on this
  ESP.deepSleep(5 * 60 * 1e6);
}