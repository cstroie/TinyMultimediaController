/**
  TinyMultimediaController - Multimedia controller using a Digispark 
                             (ATtiny85) and a rotary encoder

  Copyright (C) 2018 Costin STROIE <costinstroie@eridu.eu.org>

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// Wee need the Trinket library from here:
// https://learn.adafruit.com/trinket-usb-volume-knob/code
#include "TrinketHidCombo.h"

// Attiny85 pins
#define ROTARY_CLOCK  0
#define ROTARY_DATA   2
#define ROTARY_BUTTON 5
#define ROTARY_LED    1

// Prepare the bit masks
byte bitClock   = _BV(ROTARY_CLOCK);
byte bitData    = _BV(ROTARY_DATA);
byte bitButton  = _BV(ROTARY_BUTTON);
byte bitLed     = _BV(ROTARY_LED);
byte bitRotMask = bitClock | bitData;

// Button stuff
bool          btnDown       = false;  // Flag the button is pressed or not
unsigned long btnMillis     = 0UL;    // Time of the last stable change
unsigned long btnDebounce   = 5UL;    // The debounce time to wait for
unsigned long btnDuration   = 0UL;    // The duration of the last action
unsigned long btnLongPress  = 1000UL; // Long press duration
bool          btnLongSent   = false;  // Flag the long press action was sent or not

// Rotary stuff
bool          mixed       = false;  // Flag for the knob rotating while pressed down

// LED stuff
bool          ledLight    = false;  // Status flag for the LED (on or off)
unsigned long ledTimeout  = 0UL;    // The time when the LED will turn off
unsigned long ledDelay    = 20UL;   // Time delay after which the LED turns off

/**
  Read the rotary encoder and return the rotating direction

  @return the rotating direction, positive is CW, negative is CCW
*/
byte readRotary() {
  byte        pinsNow   = PINB & bitRotMask;  // Current value of the pins
  static byte pinsLast  = bitRotMask;         // Previous value of the pins
  static bool locked    = false;              // Bouncing lock
  char        result    = 0;                  // The result

  // To ensure bouncy mechanical switch stuff only makes one positive or negative
  // pulse for each mechanical click, we lock it when it gets half way through
  // a click, that is when both bits are zero
  if (pinsNow == 0) locked = true;

  // Do any action only when the rotary step is complete, that is both bits are one
  if (locked and (pinsNow == bitRotMask)) {
    locked = false;
    if      (pinsLast == bitClock)  result++;
    else if (pinsLast == bitData)   result--;
  }

  // Keep current value
  pinsLast = pinsNow;

  // Return the result
  return result;
}

/**
  Send a media keypress

  @param key the key code to send
*/
void keySend(uint8_t key) {
  // Turn the LED on
  ledOn();
  // Send the key
  TrinketHidCombo.pressMultimediaKey(key);
}

/*
  Turn the LED on and set the timeout
*/
void ledOn() {
  if (!ledLight) {
    ledLight = true;
    PORTB |= bitLed;
  }
  ledTimeout = millis() + ledDelay;
}

/*
  Check the timeout and turn the LED off

  @param nowMillis cached millis()
*/
void ledOff(unsigned long nowMillis) {
  if (ledLight and (nowMillis > ledTimeout)) {
    ledLight = false;
    PORTB &= ~bitLed;
  }
}

void setup() {
  // Set the encoder and button pins to input
  pinMode(ROTARY_BUTTON,  INPUT_PULLUP);
  pinMode(ROTARY_CLOCK,   INPUT);
  pinMode(ROTARY_DATA,    INPUT);

  // Set the LED pin to output and flash it longer
  pinMode(ROTARY_LED,    OUTPUT);
  ledOn();
  ledTimeout += ledDelay;

  // Start the USB device engine and enumerate
  TrinketHidCombo.begin();
}

void loop() {
  // Do the USB stuff
  TrinketHidCombo.poll();

  // Keep the time
  unsigned long nowMillis = millis();

  // Check the button
  if (PINB & bitButton) {
    // Button is up, check if it should have been down
    if (btnDown) {
      // It should have been down, check bouncing
      if (btnMillis + btnDebounce < nowMillis) {
        // This is an action, keep the duration of the action
        btnDuration = nowMillis - btnMillis;
        btnDown = false;
        // And keep the time the button came up
        btnMillis = nowMillis;
      }
      else {
        // It's unstable, increase the time and flag it is not an action
        btnMillis = nowMillis;
        btnDuration = 0UL;
      }
    }
  }
  else {
    // Button is down, check if it should have been down
    if (btnDown) {
      // The button is pressed down, keep the duration
      btnDuration = nowMillis - btnMillis;
    }
    else {
      // The button was up, check bouncing
      if (btnMillis + btnDebounce < nowMillis) {
        // Debounced, flag the button is down and keep the time
        btnDown = true;
        btnMillis = nowMillis;
        btnDuration = 0UL;
      }
    }
  }

  // Check the rotary
  char rotDirection = readRotary();
  if (rotDirection != 0) mixed = true;

  // Do the actions, check if the button is down or up
  if (btnDown) {
    // The button is down, check if there is any rotation
    if      (rotDirection > 0) keySend(MMKEY_SCAN_NEXT_TRACK);    // Next
    else if (rotDirection < 0) keySend(MMKEY_SCAN_PREV_TRACK);    // Prev
    else {
      // The button is down and not rotating
      if (!mixed and !btnLongSent and btnDuration > btnLongPress) {
        // The button is long pressed down, no rotation since, no key sent yet
        btnLongSent = true;
        keySend(MMKEY_STOP);                                      // Stop (long press)
      }
    }
  }
  else {
    // The button is up, check if it was down and for how long
    if (btnDuration > 0UL) {
      // The button was down, check whether it was rotating
      if (mixed) {
        // The button was down and rotating, now it is up, so forget any mixed action
        mixed = false;
      }
      else {
        // No rotating, just pressed down
        if (btnDuration < btnLongPress) keySend(MMKEY_PLAYPAUSE); // PLAY/PAUSE (short press)
      }
      // Reset the press duration
      btnDuration = 0UL;
      // Reset any the long press action flag
      btnLongSent = false;
    }
    else {
      // The button was up and is up, check if there is any rotation
      if      (rotDirection > 0)  keySend(MMKEY_VOL_UP);          // VolUp
      else if (rotDirection < 0)  keySend(MMKEY_VOL_DOWN);        // VolDn
      // No mixed action
      mixed = false;
    }
  }

  // Turn the LED off, if timed out
  ledOff(nowMillis);
}
