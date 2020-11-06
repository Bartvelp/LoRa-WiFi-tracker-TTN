void sleepMCU(String reason) {
  Serial.println("Going to sleep because: " + reason);
  unmountFS();
  while (true) {
    yield();
  }
}