# Lora wifi scanner

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
