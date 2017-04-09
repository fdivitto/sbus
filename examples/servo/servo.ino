#include <sbus.h>
#include <Servo.h>

// used pins
#define SBUS_PIN 3   // D3
#define SERVO_PIN 2  // D2


SBUS sbus;
Servo servo;

void setup() {
  sbus.begin(SBUS_PIN, sbusBlocking); 
  servo.attach(SERVO_PIN); 

}


void loop() {

  if (sbus.waitFrame()) {

    servo.writeMicroseconds( sbus.getChannel(1) );
    
  }

  delay(20);

}

