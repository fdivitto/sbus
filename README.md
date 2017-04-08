Arduino library for SBUS (Futaba)
===============================

SBUS library supports synchronous (blocking) or asynchronous (non-blocking) decoding of Futaba SBUS **without** requiring a **TTL inverter**.

SBUS uses the TIMER 2 and the Pin Change interrupt. It doesn't use UARTs, so every pin can be used.

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




