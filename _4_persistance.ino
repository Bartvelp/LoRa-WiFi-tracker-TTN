typedef struct {
  uint16_t bootup;
  uint8_t time; // in hundreds of ms, aka SF7 = 1, SF12 = 24
  uint8_t state; // 0b11111111 requestedDownlink; wasActive;
} Uplink; // Max 5 bytes

// max 508 bytes available in RTC mem
typedef struct {
  uint16_t bootCounter;
  Uplink lastUplinks[120];
} StoredData;

RtcMemory rtcMemory("/persistance");

boolean canUplink() {
  // Retrieve the data from RTC memory
  bool result = rtcMemory.begin();
  StoredData *storedData = rtcMemory.getData<StoredData>();
  // First increase the boot counter by one
  // Boot counter starts at 1 so we can easily find the empty entries
  if (storedData->bootCounter == 65535) storedData->bootCounter = 1;
  else storedData->bootCounter++;

  // Now filter for the uplinks in the last 24 hours (to abide TTN policies) 
  // First count the number of uplinks older than 24 hours
  int indexOldUplinks = 0;
  for (int i = 0; i < 120; i++) {
    Uplink currentUplink = storedData->lastUplinks[i];
    int bootupsAgo = storedData->bootCounter - currentUplink.bootup;
    int bootupsInAday = 288;
    if (bootupsAgo > bootupsInAday) indexOldUplinks = i;
  }
  // todo
}