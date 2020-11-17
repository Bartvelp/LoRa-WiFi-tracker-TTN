boolean checkActivity() {
  pinMode(5, INPUT);
  boolean isActive = digitalRead(5);
  // Now we sink the current
  pinMode(5, OUTPUT);
  digitalWrite(5, LOW);
  return isActive;
}