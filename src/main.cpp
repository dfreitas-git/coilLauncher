
/*   coilLauncher - An Arduino nano controls the triggering and timing of two sets
     of electromagnets to propel a sled down a rail.  Used to experiment with the
     power and timing of triggering and how it affects the launch energy of the sled

     Hall effect sensors detect when the sled has moved into a position right before the
     next set of coils with a final detector in place to calculate the final speed of the sled.

     A three color LED is used to indicate state.  Green: Ready to trigger, Red: coils active,
     Blue: coil cool down period where you are locked out from re-triggering the coils.

     A pot is included to vary the hold off time from when the first set of coils trigger until
     the second set of coils trigger.   The magnets are wound so as to repulse the magnet in 
     the sled so the sled needs to travel just past center of the coil before the coil energizes 
     else we will end up slowing the sled rather than accelerating it.

     A fail-safe timer is used to be sure the coils are never energized for more than a pre-set 
     amount of time in case anything goes wrong (we don't want to melt the coils!).

     dlf  4/4/2025
*/

#include <Arduino.h>

#define LAUNCH_SWITCH 10
#define HALL_SENSOR1 2
#define HALL_SENSOR2 5

#define TRIGGER0 3
#define TRIGGER1 4

#define HOLDOFF_DELAY_PIN A2

// Tri-color LED uses three pins
#define LED_RED_PIN 6
#define LED_BLUE_PIN 8
#define LED_GREEN_PIN 7

// Color defs
#define   BLACK 0
#define   RED 1
#define   BLUE 2
#define   GREEN 3

#define REARM_DELAY 5000   // time (ms) to cool down the coils
#define FAIL_SAFE_LIMIT 1000  // max time we will let the coils be on before turning them off

#define DEBUG   // Uncomment to print debug info


unsigned long coil0TriggerTime  = millis();
unsigned long coil1TriggerTime  = millis();
unsigned long hall1TripTime;
unsigned long hall2TripTime;
unsigned long pulseWidth = 30;   // How long (in ms) to fire the coils for
int holdOffDelay;   // How long after we detect the hall-sensor before firing the next coil

// State vars
boolean readyToLaunch = 1;
boolean launched = 0;
boolean coil0Active = 0;
boolean coil1Active = 0;
boolean hall1Tripped = 0;
boolean hall2Tripped = 0;

// Function prototypes
boolean checkSwitch(uint8_t pin);
void turnLED(uint8_t ledColor);

void setup() {
  // Turn on serial monitor
  Serial.begin(9600);
  delay(1000);

  pinMode(HALL_SENSOR1, INPUT_PULLUP);
  pinMode(HALL_SENSOR2, INPUT_PULLUP);
  pinMode(LAUNCH_SWITCH, INPUT_PULLUP);
  pinMode(TRIGGER0, OUTPUT);
  pinMode(TRIGGER1, OUTPUT);
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_BLUE_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);

  digitalWrite(LED_RED_PIN,1); digitalWrite(LED_GREEN_PIN, 0); digitalWrite(LED_BLUE_PIN, 1);
}

void loop() {

  // Scale the 0-1023 input to 0 to 102 ms to use as the hall to trigger delay holdoff
  holdOffDelay = map(analogRead(HOLDOFF_DELAY_PIN),0,1023,0,102);


  // If we don't reset "launched" (meaning we're done) before the max timout, something is wrong.
  if(launched && (millis() - coil0TriggerTime) > FAIL_SAFE_LIMIT) {
      Serial.println("Launch taking too long.  Shutting down!");
      digitalWrite(TRIGGER0,0);
      digitalWrite(TRIGGER1,0);
      launched = 0;
      coil0Active = 0;
      coil1Active = 0;
  }

  // LED indicator if the coils are powered
  if(coil0Active || coil1Active) {
    turnLED(RED);
  } else if(!coil0Active && !coil1Active && !readyToLaunch) {
    turnLED(BLUE);  // Blue means we are cooling off
  }

  // Check the hall sensors to see if the sled is passing by
  if(launched && !hall1Tripped && digitalRead(HALL_SENSOR1) == 0) {
    hall1Tripped = 1;
    hall1TripTime = millis();
    #ifdef DEBUG
    Serial.print("Hall1 at ");
    Serial.print(hall1TripTime - coil0TriggerTime);
    Serial.println(" ms");
    #endif
  }

  if(launched && !hall2Tripped && digitalRead(HALL_SENSOR2) == 0) {
    hall2Tripped = 1;
    hall2TripTime = millis();
    #ifdef DEBUG
    Serial.print("Hall2 at ");
    Serial.print(hall2TripTime - coil0TriggerTime);
    Serial.println(" ms");
    #endif
  }


  // Once both have  been tripped, we can calculate the speed and we are done with the launch
  if(hall1Tripped && hall2Tripped) {

    // Calculate the speed of the sled (in mm/sec) at hall1 and hall2.  Start to Hall1 = 45mm.  Hall1 to Hall2 = 100mm 
    unsigned long hall1SledSpeed = 45000 / (hall1TripTime - coil0TriggerTime);
    Serial.print("Hall1 Speed: ");
    Serial.print(hall1SledSpeed);
    Serial.println(" mm/s");
    unsigned long hall2SledSpeed = 100000 / (hall2TripTime - hall1TripTime);
    Serial.print("Hall2 Speed: ");
    Serial.print(hall2SledSpeed);
    Serial.println(" mm/s");

    // Reset speed state vars
    hall1Tripped = 0;
    hall2Tripped = 0;
    launched = 0;
  }

  // Wait for things to cool down between triggers,  also make sure they released the trigger switch from the previous shot
  if((millis() - coil0TriggerTime) > REARM_DELAY && checkSwitch(LAUNCH_SWITCH) == 1) {
    readyToLaunch = 1;
    if(!coil0Active && !coil1Active) {
      turnLED(GREEN);
    }
  }
  if(readyToLaunch && !coil0Active && !coil1Active && digitalRead(HALL_SENSOR1) == 1 && digitalRead(HALL_SENSOR2) == 1 && checkSwitch(LAUNCH_SWITCH) == 0) {

    // Fire off coil0
    readyToLaunch = 0;
    launched = 1;
    coil0Active = 1;
    #ifdef DEBUG
    Serial.println("Coil0 Fired: 0ms");
    #endif
    digitalWrite(TRIGGER0,1);
    coil0TriggerTime = millis();
  }

  // Sled approaching coil1
  if(launched && !coil1Active && digitalRead(HALL_SENSOR1) == 0) {

    // Fire off coil1
    digitalWrite(TRIGGER0,0);  // Don't drive both coils simultaneously
    coil0Active = 0;
    coil1Active = 1;
    Serial.print("Holdoff delay ");
    Serial.print(holdOffDelay);
    Serial.println(" ms");
    delay(holdOffDelay);
    digitalWrite(TRIGGER1,1);
    coil1TriggerTime = millis();
    #ifdef DEBUG
    Serial.print("Coil1 Fired: ");
    Serial.print(coil1TriggerTime-coil0TriggerTime);
    Serial.println(" ms");
    #endif
  }

  // Turn off coils after reaching the set pulseWidth
  if(coil0Active && ((millis() - coil0TriggerTime) > pulseWidth)) {
    coil0Active = 0;
    digitalWrite(TRIGGER0,0); 
    #ifdef DEBUG
    Serial.println("Turn off coil0");
    #endif
  }
  if(coil1Active && ((millis() - coil1TriggerTime) > pulseWidth)) {
    coil1Active = 0;
    digitalWrite(TRIGGER1,0); 
    #ifdef DEBUG
    Serial.println("Turn off coil1");
    #endif
  }
}

// Control the three-color LED which has three cathodes and one common anode (which is hooked to power).   
// Writing 0 to a color pin turns that color led on.
void turnLED(uint8_t color) {
  uint8_t red, green, blue;

  switch(color) {
    case RED:
      red=0; green=1; blue=1;
      break;
    case GREEN:
      red=1; green=0; blue=1;
      break;
    case BLUE:
      red=1; green=1; blue=0;
      break;
    default:  //turn off
      red=1; green=1; blue=1;
  }
  digitalWrite(LED_RED_PIN,red); digitalWrite(LED_GREEN_PIN, green); digitalWrite(LED_BLUE_PIN, blue);
}


// Check the state of a switch after debouncing it
boolean checkSwitch(uint8_t pin) {
    boolean state;      
    boolean prevState;  
    int debounceDelay = 20;
    prevState = digitalRead(pin);
    for(int counter=0; counter < debounceDelay; counter++) {
        delay(1);       
        state = digitalRead(pin);
        if(state != prevState) {
            counter=0;  
            prevState=state;
        }               
    }                       
    // At this point the switch state is stable
    if(state == HIGH) {     
        return true;    
    } else {            
        return false;   
    }                       
}                       