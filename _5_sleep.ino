void sleepMCU(String reason) {
  Serial.println("Going to sleep because: " + reason);
  unmountFS();
  // We always sleep 5 minutes, a lot of calculations are based on this
  delay(5 * 60 * 1000);
  ESP.restart();
}