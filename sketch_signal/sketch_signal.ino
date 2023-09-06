#define DEBUG 1
#include <EEPROM.h>
#include <CRC8.h>

#define IR_INPUT_PIN 4      // PCINT2 Use define for library support

#include "TinyIRReceiver.hpp"
/*
 * General Principles:
 * - Data Structure for the Aspect Combinations
 * - Brightness Level
 */

/*
 * Helper macro for getting a macro definition as string
 */
#if !defined(STR_HELPER)
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#endif

/*
 * Saving settings.
 * - After 10 seconds after a config change, save the current settings.
 * - At boot, set the config to the saved settings.
 * - TODO:  How to set to default.
 */

// Light Configuration - HIGH|LOW for each light.
struct Aspect { 
  int WHITE;
  int RED;
  int GREEN;
  int YELLOW;
  boolean YELLOW_BLINK;
};

struct Config{
  uint8_t  brightness;
  uint8_t  aspect;
  uint8_t   cycle;
  uint8_t   crc;
};

// Aspect Modes
const Aspect aspects[] = {
  {LOW, LOW, LOW, LOW, false},   // OFF
  {LOW, LOW, HIGH, LOW, false},  // GREEN
  {LOW, HIGH, LOW, LOW, false},  // RED
  {HIGH, LOW, LOW, HIGH, false}, // WHITE/YELLOW
  {LOW, LOW, LOW, HIGH, false},  // YELLOW
  {LOW, LOW, LOW, HIGH, true},   // YELLOW BLINK
  {HIGH, HIGH, HIGH, HIGH, false}// ON
};

const Aspect ASPECT_ALL_OFF = {LOW, LOW, LOW, LOW, false};
const Aspect ASPECT_ALL_ON = {HIGH, HIGH, HIGH, HIGH, false};

// Pin Assignment
const int ASPECT_GREEN_PIN = 9;
const int ASPECT_RED_PIN = 8;
const int ASPECT_WHITE_PIN = 7;
const int ASPECT_YELLOW_PIN = 6;

const int BRIGHTNESS_LOW_PIN = 10;
const int BRIGHTNESS_MED_PIN = 11;
const int BRIGHTNESS_HIGH_PIN = 12;

const int BUTTON1_PIN = 2;
const int BUTTON2_PIN = 3;

int brightnessLevel = 0; // 0-3

const int cycleDelay = 6000;         // ms
unsigned long cycleStartTime;        // ms
boolean inCycleMode = false;
const int FLASH_DELAY = 300;         // Cycle Acknowlegement flash


int aspectIdx;
const int blinkDelay = 500;          // ms
unsigned long blinkStartTime; 
int blinkState = HIGH;               // HIGH|LOW


// Button configuration:
const int SHORT_PRESS_TIME = 1000;     // milliseconds
const int LONG_PRESS_TIME  = 3000;     // milliseconds
const int DEBOUNCE_DELAY = 50;         // Debounce time

// Button1 Configuration
int lastSteadyStateB1 = LOW;           // the previous steady state from the input pin
int lastFlickerableStateB1 = LOW;      // the previous flickerable state from the input pin
unsigned long lastDebounceTimeB1 = 0;  // the last time the output pin was toggled

int lastButtonStateB1 = LOW;           // the previous reading from the input pin
unsigned long pressedTimeB1  = 0;      // Time in millis taht the button was last pressed
unsigned long releasedTimeB1 = 0;      // Time in millis taht the button was last released
boolean isPressingB1 = false;
boolean isLongDetectedB1 = false;      // Brightness

// Button2 Configuration
int lastSteadyStateB2 = LOW;           // the previous steady state from the input pin
int lastFlickerableStateB2 = LOW;      // the previous flickerable state from the input pin
unsigned long lastDebounceTimeB2 = 0;  // the last time the output pin was toggled

int lastButtonStateB2 = LOW;           // the previous reading from the input pin
unsigned long pressedTimeB2  = 0;      // Time in millis taht the button was last pressed
unsigned long releasedTimeB2 = 0;      // Time in millis taht the button was last released
boolean isPressingB2 = false;
boolean isLongDetectedB2 = false;      // Nothing yet.

// Configuration Save Details
boolean configDirty = false;           // If true, the configuration has changed.
unsigned long configChangeTime = 0;    // Time in which any configuration changed.
const int SAVE_DELAY_TIME = 30000;     // Time to wait until saving the configuration.

// IR Remote Codes:
const uint8_t IR_ADDR = 0;
const uint8_t IR_CODE_1 = 0xBA;
const uint8_t IR_CODE_2 = 0xB9;
const uint8_t IR_CODE_3 = 0xB8;
const uint8_t IR_CODE_4 = 0xBB;
const uint8_t IR_CODE_5 = 0xBF;
const uint8_t IR_CODE_6 = 0xBC;
const uint8_t IR_CODE_7 = 0xF8;
const uint8_t IR_CODE_8 = 0xEA;
const uint8_t IR_CODE_9 = 0xF6;
const uint8_t IR_CODE_0 = 0xE6;
const uint8_t IR_CODE_STAR = 0xE9;
const uint8_t IR_CODE_HASH = 0xF2;
const uint8_t IR_CODE_OK = 0xE3;
const uint8_t IR_CODE_LL = 0xF7;
const uint8_t IR_CODE_RT = 0xA5;
const uint8_t IR_CODE_UP = 0xE7;
const uint8_t IR_CODE_DN = 0xAD;

volatile struct TinyIRReceiverCallbackDataStruct sCallbackData;

void setup() {
  #ifdef DEBUG
    Serial.begin(9600);
  #endif

  aspectIdx=0;
  brightnessLevel = 3;
  pinMode(ASPECT_GREEN_PIN, OUTPUT);
  pinMode(ASPECT_RED_PIN, OUTPUT);
  pinMode(ASPECT_YELLOW_PIN, OUTPUT);
  pinMode(ASPECT_WHITE_PIN, OUTPUT);
  pinMode(BRIGHTNESS_LOW_PIN, OUTPUT);
  pinMode(BRIGHTNESS_MED_PIN, OUTPUT);
  pinMode(BRIGHTNESS_HIGH_PIN, OUTPUT);
  pinMode(BUTTON1_PIN, INPUT);
  pinMode(BUTTON2_PIN, INPUT);

  // Load the configuration from EEPROM
  loadConfig();
  setBrightness(brightnessLevel);

  //setAspect(aspects[0]);
  //test();

  setAspect(aspects[aspectIdx]);
  //delay(5000);

  Serial.println(F("START " __FILE__ " from " __DATE__));
  if (!initPCIInterruptForTinyReceiver()) {
      Serial.println(F("No interrupt available for pin " STR(IR_INPUT_PIN))); // optimized out by the compiler, if not required :-)
  }
  Serial.println(F("Ready to receive NEC IR signals at pin " STR(IR_INPUT_PIN)));
      

}

// Set the aspect combination of lights on/off
void setAspect(Aspect a) {
  // Turn everything off before turning back on to avoid overdraw.
//  digitalWrite(ASPECT_RED_PIN, LOW);
//  digitalWrite(ASPECT_GREEN_PIN, LOW);
//  digitalWrite(ASPECT_YELLOW_PIN, LOW);
//  digitalWrite(ASPECT_WHITE_PIN, LOW);
  
  digitalWrite(ASPECT_WHITE_PIN, a.WHITE);
  digitalWrite(ASPECT_RED_PIN, a.RED);
  digitalWrite(ASPECT_GREEN_PIN, a.GREEN);
  digitalWrite(ASPECT_YELLOW_PIN, a.YELLOW);
}

void setBrightness(int level) {
  // It's important to turn off one relay before turning one on to avoid the possiblity of having two on simultaneously.
  // HIGH is off.
  switch(level) {
    case 0: // off
      digitalWrite(BRIGHTNESS_MED_PIN, HIGH);
      digitalWrite(BRIGHTNESS_HIGH_PIN, HIGH);
      digitalWrite(BRIGHTNESS_LOW_PIN, HIGH);
      break;
    case 1:
      digitalWrite(BRIGHTNESS_MED_PIN, HIGH);
      digitalWrite(BRIGHTNESS_HIGH_PIN, HIGH);
      digitalWrite(BRIGHTNESS_LOW_PIN, LOW);
      break;
    case 2:
      digitalWrite(BRIGHTNESS_LOW_PIN, HIGH);
      digitalWrite(BRIGHTNESS_HIGH_PIN, HIGH);
      digitalWrite(BRIGHTNESS_MED_PIN, LOW);
      break;
    case 3:
      digitalWrite(BRIGHTNESS_LOW_PIN, HIGH);
      digitalWrite(BRIGHTNESS_MED_PIN, HIGH);
      digitalWrite(BRIGHTNESS_HIGH_PIN, LOW);
      break;
  }
  
}

void test() {
  for(int i=0;i<4;i++) {
    setAspect(aspects[i]);
    delay(500);
  }
  setAspect(aspects[aspectIdx]);
}

void setNextBrightness() {
  brightnessLevel = (++brightnessLevel)%3;
  
  #ifdef DEBUG
  Serial.print("Setting brightness ");
  Serial.println(brightnessLevel);
  #endif

  setBrightness(brightnessLevel);
}

void setNextAspectMode() {
  int nextAspectIdx = ++aspectIdx;
  int numAspects = (sizeof(aspects)/sizeof(aspects[0]));
  if(nextAspectIdx >= numAspects ) {
    nextAspectIdx=0;
  }
  setAspectByIndex(nextAspectIdx);
}

void setPrevAspectMode() {
  int prevAspectIdx = --aspectIdx;
  int numAspects = (sizeof(aspects)/sizeof(aspects[0]));
  if(prevAspectIdx < 0 ) {
    prevAspectIdx=numAspects-1;
  }
  setAspectByIndex(prevAspectIdx);
}

void setAspectByIndex(int idx) {
   Aspect a = aspects[idx];
  #ifdef DEBUG
  Serial.print("Setting aspect ");
  Serial.println(idx);
  #endif
  setAspect(a);
  aspectIdx=idx;
  blinkStartTime=millis(); 
}

void flash(Aspect a, int flashes) {
  for(int i=0;i<flashes;i++) {
    if(i != 0) {
      delay(FLASH_DELAY);
    }
    setAspect(ASPECT_ALL_OFF);
    delay(FLASH_DELAY);
    setAspect(aspects[aspectIdx]);
  }
}

void processBlink() {

  Aspect a = aspects[aspectIdx];
  if(a.YELLOW_BLINK) {
    // When the blink time expires, toggle the state.
    long now = millis();
    if((now - blinkStartTime) > blinkDelay) {
      if(blinkState == HIGH) {
        blinkState = LOW;
      } else {
        blinkState = HIGH;
      }
      #ifdef DEBUG
      Serial.print("Blinking Yellow:  ");
      Serial.println(blinkState);
      #endif
      digitalWrite(ASPECT_YELLOW_PIN, blinkState);
      blinkStartTime = now;
    }
  }
}

void processButton1() {
   /*
   * This is the main loop for operating the signal light which has 4 lamps (aspects): Red, Yellow, Green, White
   * There is one button:
   * 1.  Short Press:  Cycle modes.
   * 2.  Long Press:   Cycle Brighness
   */

  /*
   * Mode Cycles:
   * 1. Constant Green
   * 2. Constant Yellow
   * 3. Constant Red
   * 4. Constant White
   * 
   */

   /**
    * How this works. 
    * - Capture the time of each de-boundced press and release.
    * - If the button is released quickly, then this is a short press.
    * - If the button is not released quickly, then this is a long press, and trigger it after the press time expires.
    */

   // Change Modes
  int currentState = digitalRead(BUTTON1_PIN);
  boolean isReleased = false;

  // If the switch/button changed, due to noise or pressing:
  if (currentState != lastFlickerableStateB1) {
    // reset the debouncing timer
    lastDebounceTimeB1 = millis();
    // save the the last flickerable state
    lastFlickerableStateB1 = currentState;
  }

  
  if ((millis() - lastDebounceTimeB1) > DEBOUNCE_DELAY) {
  
    // Track each button change event.
    if(lastButtonStateB1 == LOW && currentState == HIGH) {
      // Pressed
      pressedTimeB1 = millis();
      isPressingB1 = true;
      isLongDetectedB1 = false;
    } else if(lastButtonStateB1 == HIGH && currentState == LOW) {
      // Released
      releasedTimeB1 = millis();
      isPressingB1 = false;
      isReleased = true;
    }
  
    if(isPressingB1 && !isLongDetectedB1) {
      long pressDuration = millis() - pressedTimeB1;
  
      if(pressDuration > SHORT_PRESS_TIME) {
        #ifdef DEBUG
        Serial.println("Long Press Detected.");
        #endif
        isLongDetectedB1 = true;
  
        // Do Long Press Action;
        //setNextBrightness();

      configChangeTime = millis();
      configDirty = true;
      }
    } else if (isReleased && !isLongDetectedB1) {
      // Do Short Press Action.
      isLongDetectedB1 = false;
      button1Action();
    }
  
    // save the reading. Next time through the loop, it'll be the lastButtonStateB1:
    lastButtonStateB1 = currentState;

  }
}

void button1Action() {
  setNextAspectMode();
  configChangeTime = millis();
  configDirty = true;     
}

void processButton2() {
   /*
   * Button 2 turns aspect cycling on/off.
   */
   // Change Modes
  int currentState = digitalRead(BUTTON2_PIN);
  boolean isReleased = false;

  // If the switch/button changed, due to noise or pressing:
  if (currentState != lastFlickerableStateB2) {
    // reset the debouncing timer
    lastDebounceTimeB2 = millis();
    // save the the last flickerable state
    lastFlickerableStateB2 = currentState;
  }

  
  if ((millis() - lastDebounceTimeB2) > DEBOUNCE_DELAY) {
  
    // Track each button change event.
    if(lastButtonStateB2 == LOW && currentState == HIGH) {
      // Pressed
      pressedTimeB2 = millis();
      isPressingB2 = true;
      isLongDetectedB2 = false;
    } else if(lastButtonStateB2 == HIGH && currentState == LOW) {
      // Released
      releasedTimeB2 = millis();
      isPressingB2 = false;
      isReleased = true;
    }
  
    if(isPressingB2 && !isLongDetectedB2) {
      long pressDuration = millis() - pressedTimeB2;
  
      if(pressDuration > SHORT_PRESS_TIME) {
        #ifdef DEBUG
        Serial.println("Long Press Detected.");
        #endif
        isLongDetectedB2 = true;
  
        // Do Long Press Action HERE
      }
    } else if (isReleased && !isLongDetectedB2) {
      // Do Short Press Action.
      isLongDetectedB2 = false;
      button2Action();
    }
  
    // save the reading. Next time through the loop, it'll be the lastButtonStateB2:
    lastButtonStateB2 = currentState;
  }
}

void button2Action() {
  // Toggle cycle mode.
  inCycleMode = !inCycleMode;
  if(inCycleMode) {
    cycleStartTime = millis();
    flash(ASPECT_ALL_OFF, 2);
  } else {
    flash(ASPECT_ALL_OFF, 1);
  }
  
  configChangeTime = millis();
  configDirty = true;
  
  #ifdef DEBUG
  Serial.print("Cycle mode enabled: ");
  Serial.println(inCycleMode);
  #endif  
}

void processCycle() {
 
  if(inCycleMode) {
    long now = millis();
    if (now-cycleStartTime > cycleDelay) {
      cycleStartTime = now;
      setNextAspectMode();
    }
  }
}



Config printConfig(Config c) {
    Serial.print("brightness: " );
    Serial.println(c.brightness);
    Serial.print("aspect: ");
    Serial.println(c.aspect);
    Serial.print("cycle: ");
    Serial.println(c.cycle);
    Serial.print("crc: ");
    Serial.println(c.crc);
}

/**
 * Get the CRC from teh Config object minus the CRC value in it.
 */
uint16_t  getConfigCRC(Config c) {
  CRC8 crc;
  crc.add(c.brightness);
  crc.add(c.aspect);
  crc.add(c.cycle);
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
  if (configDirty && (now-configChangeTime > SAVE_DELAY_TIME)) { 
    
    Config currentConfig = {
      brightnessLevel,
      aspectIdx,
      inCycleMode,
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

  
  if(crc != loadedConfig.crc) {
    #ifdef DEBUG
    Serial.println("Data in EEPROM is invalid.  Using defaults.");
    Serial.print("Stored CRC: ");
    Serial.println(loadedConfig.crc);
    Serial.print("Target CRC: ");
    Serial.println(crc);
    #endif    
  } else {
    brightnessLevel = loadedConfig.brightness;
    aspectIdx = loadedConfig.aspect;
    inCycleMode = loadedConfig.cycle;
  }
 
}

/* 
 * Remote button press 
 */
void handleIR() {
    if (sCallbackData.justWritten) {
        sCallbackData.justWritten = false;
        #ifdef DEBUG
        Serial.print(F("Address=0x"));
        Serial.print(sCallbackData.Address, HEX);
        Serial.print(F(" Command=0x"));
        Serial.print(sCallbackData.Command, HEX);
        if (sCallbackData.Flags == IRDATA_FLAGS_IS_REPEAT) {
            Serial.print(F(" Repeat"));
        }
        if (sCallbackData.Flags == IRDATA_FLAGS_PARITY_FAILED) {
            Serial.print(F(" Parity failed"));
        }
        Serial.println();
        #endif

        // Trigger an operation when the appropriate button is pressed and there are no parity or repeating flags set.
        if(sCallbackData.Address == IR_ADDR && sCallbackData.Flags == IRDATA_FLAGS_EMPTY) {
          switch(sCallbackData.Command) {
            case IR_CODE_UP:
              setNextAspectMode();
              configChangeTime = millis();
              configDirty = true; 
              break;
            case IR_CODE_DN:
              setPrevAspectMode();
              configChangeTime = millis();
              configDirty = true; 
              break;
            case IR_CODE_STAR:
              test();
              break;
            case IR_CODE_HASH:
              button2Action();
              configChangeTime = millis();
              configDirty = true; 
              break;
            case IR_CODE_0:
              setAspectByIndex(0);
              configChangeTime = millis();
              configDirty = true; 
              break;
            case IR_CODE_1:
              setAspectByIndex(1);
              configChangeTime = millis();
              configDirty = true; 
              break;
            case IR_CODE_2:
              setAspectByIndex(2);
              configChangeTime = millis();
              configDirty = true; 
              break;
            case IR_CODE_3:
              setAspectByIndex(3);
              configChangeTime = millis();
              configDirty = true; 
              break;
            case IR_CODE_4:
              setAspectByIndex(4);
              configChangeTime = millis();
              configDirty = true; 
              break;
            case IR_CODE_5:
              setAspectByIndex(5);
              configChangeTime = millis();
              configDirty = true; 
              break;
            case IR_CODE_6:
              setAspectByIndex(6);
              configChangeTime = millis();
              configDirty = true; 
              break;
            }
        }
    }
}

/*
 * The main control loop
 */
void loop() {
  handleIR();
  processButton1();
  processButton2();
  processCycle();
  processBlink();
  saveConfig();
}

/*
 * This is the function is called if a complete IR command was received
 * It runs in an ISR context with interrupts enabled, so functions like delay() etc. are working here
 */
void handleReceivedTinyIRData(uint8_t aAddress, uint8_t aCommand, uint8_t aFlags) {
    // Copy data for main loop, this is the recommended way for handling a callback :-)
    sCallbackData.Address = aAddress;
    sCallbackData.Command = aCommand;
    sCallbackData.Flags = aFlags;
    sCallbackData.justWritten = true;
}
