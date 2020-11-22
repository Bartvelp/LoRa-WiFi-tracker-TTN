uint8_t get_battery_voltage() {
  // TODO: implement this
  uint16_t ADC_reading = analogRead(A0);
  Serial.println("Got ADC: " + String(ADC_reading));
  float Vbat = ((ADC_reading / 1023.0) * (510 + 150)) / 150;
  Vbat = Vbat - 0.1; // Correction factor
  Serial.println("Got Vbat: " + String(Vbat, 2));
  // Ranges from 2.0 to 4.5V, turn in => 2.0 == 0, 4.5 == 250
  uint8_t batteryByte = (Vbat - 2) * 100;
  Serial.println("Got batbyte: " + String(batteryByte));
  return batteryByte;
}