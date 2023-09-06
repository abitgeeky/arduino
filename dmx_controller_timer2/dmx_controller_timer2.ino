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

//#define DEBUG = true;
#ifndef DEBUG
  #include <DMXSerial.h>
#endif

// Pin Config UNO
const int LED1_PIN = 7;
const int LED2_PIN = 8;
const int LED3_PIN = 12;
const int LED4_PIN = 13;

const int BUTTON_DIMMER_PIN = 3;  // the pin our push button is on
const int BUTTON_SELECT_PIN = 4;  // the pin our push button is on

// Pin Config Wemos D1
// const int LED_PIN = 13;    // D7
// const int BUTTON_PIN = 2;  // D4


// DMX Config
const int DMX_UNIVERSE_SIZE = 14;

// Button
const int DEBOUNCE_DELAY = 50;  // the debounce time; increase if the output flickers
int lastSteadyState = LOW;           // the previous steady state from the input pin
int lastFlickerableState = LOW;      // the previous flickerable state from the input pin
int currentState;                    // the current reading from the input pin
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled

// the setup routine runs once when you press reset:
void setup() {
  pinMode(LED_PIN, OUTPUT);            // Set the LED Pin as an output
  pinMode(BUTTON_PIN, INPUT_PULLUP);  // Set the Tilt Switch as an input

#ifndef DEBUG
  DMXSerial.maxChannel(DMX_UNIVERSE_SIZE);
  DMXSerial.init(DMXController);
#else
  Serial.begin(9600);
#endif
}

const int MODE_MANUAL = 0;
const int MODE_AUTO = 1;
int currentMode = MODE_MANUAL;

void applyMode(int newMode) {
  if (currentMode == MODE_MANUAL) {
#ifndef DEBUG
    // DMXSerial.write(2, 255);
    // DMXSerial.write(3, 255);
    DMXSerial.write(4, 125);
    // DMXSerial.write(5, 255);
    DMXSerial.write(6, 125);
    DMXSerial.write(7, 125);
    DMXSerial.write(8, 125);
#else
    Serial.println("On");
#endif

  } else {
#ifndef DEBUG
    DMXSerial.write(2, 0);
    DMXSerial.write(3, 0);
    DMXSerial.write(4, 0);
    DMXSerial.write(5, 0);
    DMXSerial.write(6, 0);
#else
    Serial.println("Off");
#endif
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

int i=0;
void loop() {

  // Button work
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

  // LED Output - High if Manual mode.
  if (currentMode == MODE_MANUAL) {
    digitalWrite(LED_PIN, HIGH);
  } else {
    digitalWrite(LED_PIN, LOW);  //Turn the LED on
  }


}