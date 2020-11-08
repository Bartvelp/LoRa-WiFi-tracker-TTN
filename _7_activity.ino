boolean checkActivity() {
  // TODO implement
  uint8_t randomNumber = random(0, 255);
  Serial.println("Got random num: " + String(randomNumber));
  return (randomNumber < 65);
}