/*
 *  TinyReceiver.cpp
 *
 *  Small memory footprint and no timer usage!
 *
 *  Receives IR protocol data of NEC protocol using pin change interrupts.
 *  On complete received IR command the function handleReceivedIRData(uint16_t aAddress, uint8_t aCommand, uint8_t aFlags)
 *  is called in Interrupt context but with interrupts being enabled to enable use of delay() etc.
 *  !!!!!!!!!!!!!!!!!!!!!!
 *  Functions called in interrupt context should be running as short as possible,
 *  so if you require longer action, save the data (address + command) and handle it in the main loop.
 *  !!!!!!!!!!!!!!!!!!!!!
 *
 *
 *  Copyright (C) 2020-2022  Armin Joachimsmeyer
 *  armin.joachimsmeyer@gmail.com
 *
 *  This file is part of IRMP https://github.com/IRMP-org/IRMP.
 *  This file is part of Arduino-IRremote https://github.com/Arduino-IRremote/Arduino-IRremote.
 *
 *  TinyIRReceiver is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/gpl.html>.
 *
 */

#include <Arduino.h>

/*
 * Set sensible receive pin for different CPU's
 */
#define IR_INPUT_PIN    4   // INT0
//#define NO_LED_FEEDBACK_CODE   // Activate this if you want to suppress LED feedback or if you do not have a LED. This saves 14 bytes code and 2 clock cycles per interrupt.

#define DEBUG // to see if attachInterrupt is used
//#define TRACE // to see the state of the ISR state machine

/*
 * Second: include the code and compile it.
 */
#include "TinyIRReceiver.hpp"

/*
 * Helper macro for getting a macro definition as string
 */
#if !defined(STR_HELPER)
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#endif

volatile struct TinyIRReceiverCallbackDataStruct sCallbackData;

void setup() {
    Serial.begin(115200);
#if defined(__AVR_ATmega32U4__) || defined(SERIAL_PORT_USBVIRTUAL) || defined(SERIAL_USB) /*stm32duino*/|| defined(USBCON) /*STM32_stm32*/|| defined(SERIALUSB_PID) || defined(ARDUINO_attiny3217)
    delay(4000); // To be able to connect Serial monitor after reset or power up and before first print out. Do not wait for an attached Serial Monitor!
#endif
    // Just to know which program is running on my Arduino
#if defined(ESP8266) || defined(ESP32)
    Serial.println();
#endif
    Serial.println(F("START " __FILE__ " from " __DATE__));
    if (!initPCIInterruptForTinyReceiver()) {
        Serial.println(F("No interrupt available for pin " STR(IR_INPUT_PIN))); // optimized out by the compiler, if not required :-)
    }
    Serial.println(F("Ready to receive NEC IR signals at pin " STR(IR_INPUT_PIN)));
}

void loop() {
    if (sCallbackData.justWritten) {
        sCallbackData.justWritten = false;
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
    }
    /*
     * Put your code here
     */
}

/*
 * This is the function is called if a complete command was received
 * It runs in an ISR context with interrupts enabled, so functions like delay() etc. are working here
 */
#if defined(ESP8266) || defined(ESP32)
IRAM_ATTR
#endif

void handleReceivedTinyIRData(uint8_t aAddress, uint8_t aCommand, uint8_t aFlags)
        {
#if defined(ARDUINO_ARCH_MBED) || defined(ESP32)
    // Copy data for main loop, this is the recommended way for handling a callback :-)
    sCallbackData.Address = aAddress;
    sCallbackData.Command = aCommand;
    sCallbackData.Flags = aFlags;
    sCallbackData.justWritten = true;
#else
    /*
     * Printing is not allowed in ISR context for any kind of RTOS
     * For Mbed we get a kernel panic and "Error Message: Semaphore: 0x0, Not allowed in ISR context" for Serial.print()
     * for ESP32 we get a "Guru Meditation Error: Core  1 panic'ed" (we also have an RTOS running!)
     */
    // Print only very short output, since we are in an interrupt context and do not want to miss the next interrupts of the repeats coming soon
    printTinyReceiverResultMinimal(aAddress, aCommand, aFlags, &Serial);
#endif
}
