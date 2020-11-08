#define NUM_STORED_UPLINKS 120

typedef struct {
  uint16_t bootup;
  uint8_t time; // in hundreds of ms, aka SF7 = 1, SF12 = 24
  uint8_t state; // 0b11111111 requestedDownlink; wasActive;
} Uplink; // Max 5 bytes

// max 508 bytes available in RTC mem
typedef struct {
  uint16_t bootCounter;
  uint32_t uplinkCount;
  uint32_t downlinkCount;
  Uplink lastUplinks[NUM_STORED_UPLINKS];
} StoredData;

RtcMemory rtcMemory("/persistance");

uint16_t increaseBootCount() {
  StoredData *storedData = rtcMemory.getData<StoredData>();
  // First increase the boot counter by one
  // Boot counter starts at 1 so we can easily find the empty entries
  if (storedData->bootCounter == 65535) storedData->bootCounter = 1;
  else storedData->bootCounter++;
  rtcMemory.save();
  return storedData->bootCounter;
}

uint16_t mountFS() {
  LittleFS.begin();
  bool result = rtcMemory.begin();
  StoredData *storedData = rtcMemory.getData<StoredData>();
  return storedData->bootCounter;
}

void unmountFS() {
  // Do this here to seperate concerns
  LittleFS.end();
}

void persistDataToFlash() {
  boolean success = rtcMemory.persist();
  Serial.println("Persisted to flash: " + String(success));
}

boolean canUplink() {
  // Can uplink in terms of airtime in the past 24 hours
  // Retrieve the data from RTC memory
  StoredData *storedData = rtcMemory.getData<StoredData>();

  // Now filter for the uplinks in the last 24 hours (to abide TTN policies) 
  // First count the number of uplinks older than 24 hours
  int airTimeInLast24Hours = 0; // in hundreds of ms

  for (int i = 0; i < NUM_STORED_UPLINKS; i++) {
    Uplink currentUplink = storedData->lastUplinks[i];
    int bootupsAgo = storedData->bootCounter - currentUplink.bootup;
    int bootupsInAday = 288;
    if (bootupsAgo < bootupsInAday) {
      // This is an uplink from the last 24 hours
      airTimeInLast24Hours += currentUplink.time;
    }
  }
  // We can have max 30 seconds in 24 hours, so return true below 30 seconds
  Serial.println("Got airtime: " + String(airTimeInLast24Hours));
  return (airTimeInLast24Hours < 300);
}

boolean saveNewUplink(String spreadingFactor, boolean wasActive, boolean requestedDownlink, int payload_size) {
  StoredData *storedData = rtcMemory.getData<StoredData>();
  // storedData->lastUplinks = [older, newer]
  // get the first index that is free to fill
  int firstFreeIndex = -1;
  for (int i = 0; i < NUM_STORED_UPLINKS; i++) {
    Uplink currentUplink = storedData->lastUplinks[i];
    if (currentUplink.bootup == 0) {
      firstFreeIndex = i;
      break;
    }
  }
  if (firstFreeIndex == -1) {
    // We need to remove the oldest entry, aka the first 
    // Shift everything left to push out the first entry
    // Don't do the last one, we store our new data in that one
    for (int i = 0; i < (NUM_STORED_UPLINKS - 1); i++) {
      // Get the uplink over 1 place to the right
      int nextUplinkIndex = i + 1;
      Uplink nextUplink = storedData->lastUplinks[nextUplinkIndex];
      storedData->lastUplinks[i] = nextUplink; // save it in it's new place
    }
    firstFreeIndex = NUM_STORED_UPLINKS - 1; // This one is now available
  }

  // Build new uplink
  Uplink newUplink;
  newUplink.bootup = storedData->bootCounter;
  newUplink.time = calcOnAirTime(spreadingFactor, payload_size);
  newUplink.state = getState(wasActive, requestedDownlink);
  // save new uplink
  storedData->lastUplinks[firstFreeIndex] = newUplink;
  // Get the uplink and downlink count from LMIC
  storedData->uplinkCount = get_uplink_count();
  storedData->downlinkCount = get_downlink_count();
  rtcMemory.save(); // Store it in RTC memory
}

uint32_t get_uplink_count_from_memory() {
  StoredData *storedData = rtcMemory.getData<StoredData>();
  return storedData->uplinkCount;
}

uint32_t get_downlink_count_from_memory() {
  StoredData *storedData = rtcMemory.getData<StoredData>();
  return storedData->downlinkCount;
}

uint8_t calcOnAirTime(String spreadingFactor, int payload_size) {
  payload_size = payload_size + 13; // Add lorawan bytes
  int sf = spreadingFactor.substring(2).toInt();
  int low_data_rate = (sf >= 11) ? 1 : 0;

  float ms_per_symbol = (pow(2.0, sf) / (125 * 1000)) * 1000; //ms
  float ms_in_preamble = (8 + 4.25) * ms_per_symbol;          //ms

  int step_size = ceil((8 * payload_size - 4 * sf + 28 + 16) / (4 * (sf - 2 * low_data_rate)));
  // Unknown why but this is 1 too low;
  step_size++;
  int num_symbols_in_payload = 8 + max(step_size * (1 + 4), 0);
  float ms_on_air = ms_in_preamble + (num_symbols_in_payload * ms_per_symbol);
  return ceil(ms_on_air / 100);
}

uint8_t getState(boolean wasActive, boolean requestedDownlink) {
  // bit[7] = wasActive
  // bit[6] = requestedDownlink
  if (wasActive) {
    if (requestedDownlink) return 0b00000011;
    else return 0b00000001;
  } else {
    if (requestedDownlink) return 0b00000010;
    else return 0b00000000;
  }
}

void printSavedState() {
  // Retrieve the data from RTC memory
  StoredData *storedData = rtcMemory.getData<StoredData>();
  Serial.println("-----------------");
  Serial.println("SavedState:");
  Serial.println("Bootcounter: " + String(storedData->bootCounter));
  Serial.println("uplinkCount: " + String(storedData->uplinkCount));
  Serial.println("downlinkCount: " + String(storedData->downlinkCount));

  for (int i = 0; i < NUM_STORED_UPLINKS; i++) {
    Uplink curUp = storedData->lastUplinks[i];
    if (curUp.bootup == 0) continue;
    String logString = "Uplink[" + String(i) + "] boot, uptime, state: " + String(curUp.bootup) + " ";
    logString += String(curUp.time) + " " + String(curUp.state);
    Serial.println(logString);
  }
  Serial.println("-----------------");
}