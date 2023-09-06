/*
 * Testing the ESP-8266 LOLIN(WEMOS) D1 R2 Mini
 * Use 3.3v
 */

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <Pushsafer.h>

#define PushsaferKey "2IA61Q4XHLTRhhjrjWDk"  // Private Key: http://pushsafer.com

#define WIFI

// char ssid[] = "deadcalm";
// char password[] = "n0sep1ck3r";
char ssid[] = "HosannaNet";
char password[] = "graceful";


WiFiClient client;
Pushsafer pushsafer(PushsaferKey, client);

//Constants:
const int led1 = 16;  // D0
const int led2 = 0;   // D3
const int mic = A0;   //pin A0 to read analog input (max 3.2 volts)
// const int sensor = 15; // D8

const int numleds = 4;
int sampleDelta = 100;
const int sound_on_trigger = 5000;   //ms
const int sound_off_trigger = 5000;  //ms

int lastSampleTime = 0;
// int value;  //save analog value
bool triggered = false;
int sound_on_trigger_time = 0;
int sound_off_trigger_time = 0;

const int numReadings = 20;
int readings[numReadings];  // the readings from the analog input
int readIndex = 0;          // the index of the current reading
int total = 0;              // the running total

void setup() {

  Serial.begin(9600);
  delay(1000);
  Serial.println("Running...");

  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
  pinMode(mic, INPUT);
  Serial.println("Pin modes set");
  
  digitalWrite(led1, LOW);
  digitalWrite(led2, LOW);

  #ifdef WIFI

    // Setup wifi
    WiFi.mode(WIFI_STA);
    Serial.println("WiFi Mode Set");
    delay(100);
    WiFi.begin(ssid, password);
    Serial.println("Connecting to WiFi...");

    // Connect and blink the light while trying.
    boolean ledOn=false;
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      if(ledOn) {
        digitalWrite(led2, HIGH);
      } else {
        digitalWrite(led2, LOW);
      }
      ledOn=!ledOn;
    }

    Serial.println("Connected to WiFi");
  #endif

  digitalWrite(led2, HIGH);

  pushMessage("Activated", "#0000FF");

  // initialize all the readings to 0:
  for (int i = 0; i < numReadings; i++) {
    readings[i] = 0;
  }

  delay(5000);
}

void soundDetected() {
  digitalWrite(led1, HIGH);
  Serial.println("Sound Detected");
  pushMessage("Sound Detected", "#00FF00");
}

void soundStopped() {
  digitalWrite(led1, LOW);
  Serial.println("Sound Stopped");
  pushMessage("Sound Stopped", "#FF0000");
}

void pushMessage(String msg, String color) {
  #ifdef WIFI
  struct PushSaferInput input;
  input.title = "Sound Detector";
  input.message = msg;
  input.sound = "6";
  input.vibration = "1";
  input.icon = "1";
  input.iconcolor = color;
  input.priority = "1";
  input.device = "71217";  // Device ID:  http://pushsafer.com
  pushsafer.sendEvent(input);
  #endif
}


void loop() {

  // If a loud noise is detected for N seconds, then send a message
  // Once the noice stops for N seconds, send a message

  // Check every sample_delta milliseconds
  int t=millis();
  if(t-lastSampleTime > sampleDelta) {
    // We're using a digital cutoff microphone.  When monitoring for certain
    // Types of sound, such as an Air Compressor, we need to take an average 
    // of the last few samplings, and use that to smooth out the transitions.
    // We use a sliding window of numReadings.

    total = total - readings[readIndex];
    int value = analogRead(mic);
    // The mic, although a digital model, is hooked-up to the analog input which reads from 0-1024, where 1024 is no sound
    if(value < 1024) {
      readings[readIndex] = 1; // 1==sound
    } else {
      readings[readIndex] = 0; // 0==no sound
    }
    total = total + readings[readIndex];
    readIndex = readIndex + 1;
    if (readIndex >= numReadings) {
      readIndex = 0;
    }

    float average = (float) total / (float) numReadings;

    lastSampleTime=t;
    Serial.println(average);    
    if (average >= 0.05) {
      // Sound
      sound_off_trigger_time = 0;
      if (sound_on_trigger_time == 0) {
        sound_on_trigger_time = millis();
      }
      int duration = millis() - sound_on_trigger_time;
      if (duration > sound_on_trigger && !triggered) {
        triggered = true;
        soundDetected();
      }
    } else {
      // No Sound
      sound_on_trigger_time = 0;
      if (sound_off_trigger_time == 0) {
        sound_off_trigger_time = millis();
      }

      int duration = millis() - sound_off_trigger_time;
      if (duration > sound_off_trigger && triggered) {
        triggered = false;
        soundStopped();
      }
    }
  }
}
