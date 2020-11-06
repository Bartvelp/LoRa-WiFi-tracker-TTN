#define NUM_STORED_UPLINKS 120

typedef struct {
  uint16_t bootup;
  uint8_t time; // in hundreds of ms, aka SF7 = 1, SF12 = 24
  uint8_t state; // 0b11111111 requestedDownlink; wasActive;
} Uplink; // Max 5 bytes

// max 508 bytes available in RTC mem
typedef struct {
  uint16_t bootCounter;
  Uplink lastUplinks[NUM_STORED_UPLINKS];
} StoredData;

RtcMemory rtcMemory("/persistance");

uint16_t increaseBootCount() {
  bool result = rtcMemory.begin();
  StoredData *storedData = rtcMemory.getData<StoredData>();
  // First increase the boot counter by one
  // Boot counter starts at 1 so we can easily find the empty entries
  if (storedData->bootCounter == 65535) storedData->bootCounter = 1;
  else storedData->bootCounter++;
  rtcMemory.save();
  return storedData->bootCounter;
}

boolean persistData() {
  bool result = rtcMemory.begin();
  StoredData *storedData = rtcMemory.getData<StoredData>();
  rtcMemory.persist();
}

boolean canUplink() {
  // Can uplink in terms of airtime in the past 24 hours
  // Retrieve the data from RTC memory
  bool result = rtcMemory.begin();
  StoredData *storedData = rtcMemory.getData<StoredData>();

  // Now filter for the uplinks in the last 24 hours (to abide TTN policies) 
  // First count the number of uplinks older than 24 hours
  float airTimeInLast24Hours = 0.0; // in seconds

  for (int i = 0; i < NUM_STORED_UPLINKS; i++) {
    Uplink currentUplink = storedData->lastUplinks[i];
    int bootupsAgo = storedData->bootCounter - currentUplink.bootup;
    int bootupsInAday = 288;
    if (bootupsAgo < bootupsInAday) {
      // This is an uplink from the last 24 hours
      airTimeInLast24Hours += currentUplink.time / 10; // convert 100's ms to s
    }
  }
  // We can have max 30 seconds in 24 hours
  return (airTimeInLast24Hours > 30);
}

boolean saveNewUplink(String spreadingFactor, boolean wasActive, boolean requestedDownlink) {
  bool result = rtcMemory.begin();
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
  newUplink.time = calcOnAirTime(spreadingFactor);
  newUplink.state = getState(wasActive, requestedDownlink);
  // save new uplink
  storedData->lastUplinks[firstFreeIndex] = newUplink; 
  rtcMemory.save(); // Store it in RTC memory
}

uint8_t calcOnAirTime(String spreadingFactor) {
  // TODO, also use num bytes send
  // In hundreds of ms
  if (spreadingFactor == "SF7") return 1;
  if (spreadingFactor == "SF8") return 2;
  if (spreadingFactor == "SF9") return 4;
  if (spreadingFactor == "SF10") return 7;
  if (spreadingFactor == "SF11") return 14;
  if (spreadingFactor == "SF12") return 24;
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