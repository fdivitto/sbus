#include "sbus.h"


// used pins
#define SBUS_PIN 3   // D3


SBUS sbus;


void setup() {
  Serial.begin(115200);

  sbus.begin(SBUS_PIN, sbusBlocking);  
}


void loop() {

  if (!sbus.waitFrame()) {
    
    Serial.println("Timeout!");
    
  } else {
    
    for (int i=1; i <= 18; ++i) {
      Serial.print(sbus.getChannel(i)); 
      Serial.print(" ");
    }
    
    if (sbus.signalLossActive())
      Serial.print("SIGNAL_LOSS ");
      
    if (sbus.failsafeActive())
      Serial.print("FAILSAFE");
      
    Serial.println();
    
  }

}

