# Lora wifi scanner

Add `keys.ino` with the following stuff

``` c++
#define nwkskey {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
#define appskey {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
#define devaddr 0xAA010203
```

## transmit results

Wifi scan takes 2230 ms
40 bytes payload:
| SF   | total | send | transmit |
| ---- | ----- | ---- | -------- |
| SF7  | 4574  | 2133 | 103      |
| SF8  | 4659  | 2216 | 185      |
| SF9  | 4675  | 2359 | 330      |
| SF10 | 4967  | 2646 | 617      |
| SF11 | 5664  | 3345 | 1151     |
| SF12 | 6817  | 4496 | 2467     |

The best SF seems to be SF10 for my use case

transmit times @ 90 ma, the rest is receiving @ 12 ma

Power consumption @SF10 with 5000 ms:
2200 ms @ 70 ma
650  ms @ 80 ma
3800 ms @ 20 ma
4350 ms @ 12 ma
Average:

``` bash
(wifiScan + LoraTransmit + wifiSleep + loraReceive / totaltime
(2200 * 70 + 650 * 80 + 3800 * 20 + 4350 * 12) / 5000 = 70 ma
```

## Persistance

Keep a counter of bootups, which are every 5 minutes
store array like the following

``` javascript
[{ // Total of 5 bytes
  bootup: 3, // Counter
  uplink: 1, // in hundreds of ms, aka SF7 = 1, SF12 = 24
  requestedDownlink: true,
  confirmed: true, // actually received downlink,
  wasActive: true // Detected activity
}]
```

``` C
typedef struct {
  uint16_t bootup;
  uint8_t uplink; // in hundreds of ms, aka SF7 = 1, SF12 = 24
  uint8_t state; // 0b11111111 requestedDownlink; wasActive;
} uplink; // Max 5 bytes
// max 508 bytes available in RTC mem
typedef struct {
  uint16_t bootCounter;
  uplink[120] lastUplinks;
} storedData;
```

There are about (60 / 5) * 24 = 288 boots in a day.
You can have max about a 100 uplinks in a day, which need to fit in 508 - 2 = 506 bytes of memory
506 / 100 = 5 bytes per uplink, 4 are used in the above struct, so we store the last 120 uplinks
Then filter the array for the last 24 hours by looping through the array and finding the first uplink that is too old
and then shifting the array like:

``` C
int numRemovedUplinks = 0;
for (int i = 0; i < length_uplinks; i++) {
  uplink currentUplink = storedData.lastUplinks[i]
  int bootupAgo = storedData.bootCounter - currentUplink.bootup
  int bootupsInAday = 288;
  if (bootupAgo > bootupsInAday) numRemovedUplinks = i;
}
```
