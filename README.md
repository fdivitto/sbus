Arduino library for SBUS (Futaba)
===============================

SBUS library supports synchronous (blocking) or asynchronous (non-blocking) decoding of Futaba SBUS **without** requiring a **TTL inverter**.

SBUS uses the TIMER 2 and the Pin Change interrupt. It doesn't use UARTs, so those pins can be used:
```
 Arduino Uno/Nano/Mini: All pins are usable
 Arduino Mega: 10, 11, 12, 13, 50, 51, 52, 53, A8 (62), A9 (63), A10 (64),
               A11 (65), A12 (66), A13 (67), A14 (68), A15 (69)
 Arduino Leonardo/Micro: 8, 9, 10, 11, 14 (MISO), 15 (SCK), 16 (MOSI)
 HoodLoader2: All (broken out 1-7) pins are usable
 Attiny 24/44/84: All pins are usable
 Attiny 25/45/85: All pins are usable
 Attiny 13: All pins are usable
 Attiny 441/841: All pins are usable
 ATmega644P/ATmega1284P: All pins are usable
```

**Does not require a TTL inverter.**

Non blocking mode usage:

```
SBUS sbus;

void setup() {
  sbus.begin(3, sbusNonBlocking);  	// use pin 3
}

void loop() {
  uint16_t channel_1 = sbus.getChannel(1);
  uint16_t channel_2 = sbus.getChannel(2);
  ...etc...
}
```

sbus.getChannel() gets the channel number (1..18) and returns the channel value in the range 988..2012 (cleanflight friendly).
Non blocking mode generates an interrupt for every received byte. It may miss some frames if other interrupts delay pin change detection.
Non blocking mode is more CPU intensive, because every frame is catched, even it is not actually used.

Blocking usage:

```
SBUS sbus;

void setup() {
  sbus.begin(3, sbusBlocking);  // use pin 3
}

void loop() {
  if (sbus.waitFrame()) {    
    uint16_t channel_1 = sbus.getChannel(1);
    uint16_t channel_2 = sbus.getChannel(2);
     ...etc...
  }
}
```

sbus.waitFrame() needs to be called. It returns "false" on timeout (default is 2 seconds). sbus.waitFrame() waits for the next frame the returns.




