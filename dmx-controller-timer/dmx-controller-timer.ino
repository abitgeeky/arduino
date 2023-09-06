/**
 * This is a project for an Arduino Nano R3 with the DMX CTRC-DRA-10-R2 shield.
 *
 * Purpose:
 * - Inputs: 
 *   - 5 analog pots to feed the dimming range for specific DMX chanels and one master.
 *   - 1 switch to enable Timer on/off.
 * Timer:
 * - The timer function disables all input pots and automatically turns on a set of DMX outputs from Dusk until Dawn.
 * - TODO:  Tie into IFTTT or Tuya to drive the timer.
 **/

#include <ArduinoRS485.h>  // the ArduinoDMX library depends on ArduinoRS485
#include <ArduinoDMX.h>

const int ledPin = 13;     // We will use the internal LED
const int BUTTON_PIN = 7;  // the pin our push button is on
const int universeSize = 14;
const int DEBOUNCE_DELAY = 50;  // the debounce time; increase if the output flickers

// Variables will change:
int lastSteadyState = LOW;           // the previous steady state from the input pin
int lastFlickerableState = LOW;      // the previous flickerable state from the input pin
int currentState;                    // the current reading from the input pin
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled

// the setup routine runs once when you press reset:
void setup() {
  pinMode(ledPin, OUTPUT);            // Set the LED Pin as an output
  pinMode(BUTTON_PIN, INPUT_PULLUP);  // Set the Tilt Switch as an input

  Serial.begin(9600);
  while (!Serial)
    ;

  // initialize the DMX library with the universe size
  if (!DMX.begin(universeSize)) {
    Serial.println("Failed to initialize DMX!");
    while (1)
      ;  // wait for ever
  }
}

const int MODE_MANUAL = 0;
const int MODE_AUTO = 1;
int currentMode = MODE_MANUAL;

void applyMode(int newMode) {
  if (currentMode == MODE_MANUAL) {
    // Disable the automatic timer.
    // Enable the manual DMX controller
    // set channel 1 value to 255
    DMX.beginTransmission();
    DMX.write(3, 100);
    DMX.write(4, 255);
    DMX.write(6, 255);
    DMX.endTransmission();

  } else {
    // Enable the automatic timer.
    // Disable the manual DMX controller
    DMX.beginTransmission();
    DMX.write(3, 0);
    DMX.write(4, 0);
    DMX.write(6, 0);
    DMX.endTransmission();
  }
}

void toggleMode() {
  if (currentMode == MODE_MANUAL) {
    currentMode = MODE_AUTO;
  } else {
    currentMode = MODE_MANUAL;
  }
  applyMode(currentMode);
}

void loop() {

  currentState = digitalRead(BUTTON_PIN);
  // If the switch/button changed, due to noise or pressing:
  if (currentState != lastFlickerableState) {
    // reset the debouncing timer
    lastDebounceTime = millis();
    // save the the last flickerable state
    lastFlickerableState = currentState;
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    // if the button state has changed:
    if (lastSteadyState == LOW && currentState == HIGH) {
      toggleMode();
    }
    lastSteadyState = currentState;
  }
  if (currentMode == MODE_MANUAL) {
    digitalWrite(ledPin, LOW);  //Turn the LED off
  } else {
    digitalWrite(ledPin, HIGH);  //Turn the LED on
  }
}