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
#include <EEPROM.h>
#include <CRC8.h>


// #define DEBUG
#ifndef DEBUG
#include <DMXSerial.h>
#endif

struct Config {
  uint8_t selectedChannels;
  uint8_t brightness;
  uint8_t crc;
};

// Pin Config UNO 3
const int LED1_PIN = 12;
const int LED2_PIN = 9;
const int LED3_PIN = 10;
const int LED4_PIN = 11;

const int BUTTON_DIMMER_PIN = 3;  // the pin our push button is on
const int BUTTON_SELECT_PIN = 4;  // the pin our push button is on

// Configuration store
boolean configDirty = false;         // If true, the configuration has changed.
unsigned long configChangeTime = 0;  // Time in which any configuration changed.
const int SAVE_DELAY_TIME = 5000;    // Time to wait until saving the configuration.


// DMX Config
const int DMX_UNIVERSE_SIZE = 14;
int brightness = 10;  // 1-10
int selectedChannels = 1;

// Button
const int DEBOUNCE_DELAY = 50;  // the debounce time; increase if the output flickers



// the setup routine runs once when you press reset:
void setup() {
  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  pinMode(LED3_PIN, OUTPUT);
  pinMode(LED4_PIN, OUTPUT);
  pinMode(BUTTON_DIMMER_PIN, INPUT_PULLUP);  // Set the Tilt Switch as an input
  pinMode(BUTTON_SELECT_PIN, INPUT_PULLUP);  // Set the Tilt Switch as an input

  // Wait for the relay to kick on to avoid dual masters from any attached 
  // controllers
  delay(2000);

#ifndef DEBUG
  DMXSerial.maxChannel(DMX_UNIVERSE_SIZE);
  DMXSerial.init(DMXController);
#else
  Serial.begin(9600);
#endif

  // Load the configuration from EEPROM
  loadConfig();

  // Set our initial state
  applyDMXState();
  lightLEDs();

    // DMXSerial.write(4, 125);
    // // DMXSerial.write(5, 255);
    // DMXSerial.write(6, 125);
    // DMXSerial.write(7, 125);
    // DMXSerial.write(8, 125);
}

void applyDMXState() {
  int brightnessValue = (float)brightness * 25.5;

#ifndef DEBUG
  if (selectedChannels & 0x01) {
    DMXSerial.write(1, brightnessValue);
  }
  if (selectedChannels & 0x02) {
    DMXSerial.write(2, brightnessValue);
  }
  if (selectedChannels & 0x04) {
    DMXSerial.write(3, brightnessValue);
  }
  if (selectedChannels & 0x08) {
    DMXSerial.write(4, brightnessValue);
  }
#else
  Serial.print("applyDMX brightness=");
  Serial.println(brightnessValue);
#endif
}

void switchButtonPress() {
  // Cycle between 4 outputs 1-16 binary in all combinations
  selectedChannels++;
  if (selectedChannels > 0x0F) {
    selectedChannels = 1;
  }
#ifdef DEBUG
  Serial.print("Select Button: ");
  Serial.println(selectedChannels, HEX);
#endif
  lightLEDs();
  applyDMXState();
  configChangeTime = millis();
  configDirty = true;
}


void dimmerButtonPress() {
  // Cycle from 10 to 100% (1-10)
  brightness++;
  if (brightness > 10) {
    brightness = 1;
  }
#ifdef DEBUG
  Serial.print("Dimmer Button: ");
  Serial.println(brightness, DEC);
#endif
  configChangeTime = millis();
  configDirty = true;
  applyDMXState();
}

void lightLEDs() {
  digitalWrite(LED1_PIN, selectedChannels & 0x01);
  digitalWrite(LED2_PIN, selectedChannels & 0x02);
  digitalWrite(LED3_PIN, selectedChannels & 0x04);
  digitalWrite(LED4_PIN, selectedChannels & 0x08);
}

int switchButtonSteadyState = LOW;               // the previous steady state from the input pin
int switchButtonLastFlickerableState = LOW;      // the previous flickerable state from the input pin
int switchButtonCurrentState;                    // the current reading from the input pin
unsigned long switchButtonLastDebounceTime = 0;  // the last time the output pin was toggled

void processSwitchButton() {
  // Button work
  switchButtonCurrentState = digitalRead(BUTTON_SELECT_PIN);
  // If the switch/button changed, due to noise or pressing:
  if (switchButtonCurrentState != switchButtonLastFlickerableState) {
    // reset the debouncing timer
    switchButtonLastDebounceTime = millis();
    // save the the last flickerable state
    switchButtonLastFlickerableState = switchButtonCurrentState;
  }

  if ((millis() - switchButtonLastDebounceTime) > DEBOUNCE_DELAY) {
    // if the button state has changed:
    if (switchButtonSteadyState == LOW && switchButtonCurrentState == HIGH) {
      switchButtonPress();
    }
    switchButtonSteadyState = switchButtonCurrentState;
  }
}

int dimmerButtonSteadyState = LOW;               // the previous steady state from the input pin
int dimmerButtonLastFlickerableState = LOW;      // the previous flickerable state from the input pin
int dimmerButtonCurrentState;                    // the current reading from the input pin
unsigned long dimmerButtonLastDebounceTime = 0;  // the last time the output pin was toggled

void processDimmerButton() {
  // Button work
  dimmerButtonCurrentState = digitalRead(BUTTON_DIMMER_PIN);
  // If the switch/button changed, due to noise or pressing:
  if (dimmerButtonCurrentState != dimmerButtonLastFlickerableState) {
    // reset the debouncing timer
    dimmerButtonLastDebounceTime = millis();
    // save the the last flickerable state
    dimmerButtonLastFlickerableState = dimmerButtonCurrentState;
  }

  if ((millis() - dimmerButtonLastDebounceTime) > DEBOUNCE_DELAY) {
    // if the button state has changed:
    if (dimmerButtonSteadyState == LOW && dimmerButtonCurrentState == HIGH) {
      dimmerButtonPress();
    }
    dimmerButtonSteadyState = dimmerButtonCurrentState;
  }
}

//-------------------------------------------------------------
// CONFIG
//-------------------------------------------------------------

/**
   * Get the CRC from teh Config object minus the CRC value in it.
   */
uint16_t getConfigCRC(Config c) {
  CRC8 crc;
  crc.add(c.selectedChannels);
  crc.add(c.brightness);
  return crc.getCRC();
}

/**
   * Save the config to EEPROM
   */

void saveConfig() {
  // We save the configuration to EEPROM only when the config changes and is stable for
  // a set duration.
  // Once set, we reset teh dirty flag.
  long now = millis();
  if (configDirty && (now - configChangeTime > SAVE_DELAY_TIME)) {

    Config currentConfig = {
      selectedChannels,
      brightness,
      0
    };
    currentConfig.crc = getConfigCRC(currentConfig);

    // Serialize our settings into a set of bits and save to EEPROM
    EEPROM.put(0, currentConfig);

#ifdef DEBUG
    Serial.println("Config saved: ");
    printConfig(currentConfig);
#endif
    configDirty = false;
  }
}

/*
   * Load the configuration from EEPROM
   * If the CRC is invalid, then use defaults
   */
void loadConfig() {

  // Set the config to defaults if the config's saved values are corrupt.
  Config loadedConfig;
  EEPROM.get(0, loadedConfig);
  uint8_t crc = getConfigCRC(loadedConfig);

#ifdef DEBUG
  Serial.print("Loading config from EEPROM...");
  Serial.print("EEPROM Length: ");
  Serial.println(EEPROM.length());
  printConfig(loadedConfig);
#endif


  if (crc != loadedConfig.crc) {
#ifdef DEBUG
    Serial.println("Data in EEPROM is invalid.  Using defaults.");
    Serial.print("Stored CRC: ");
    Serial.println(loadedConfig.crc);
    Serial.print("Target CRC: ");
    Serial.println(crc);
#endif
  } else {
    selectedChannels = loadedConfig.selectedChannels;
    brightness = loadedConfig.brightness;
  }
}

Config printConfig(Config c) {
#ifdef DEBUG
  Serial.print("selectedChannels: ");
  Serial.println(c.selectedChannels, HEX);
  Serial.print("brightness: ");
  Serial.println(c.brightness);
  Serial.print("crc: ");
  Serial.println(c.crc);
#endif
}

int i = 0;
void loop() {
  processSwitchButton();
  processDimmerButton();
  saveConfig();
}