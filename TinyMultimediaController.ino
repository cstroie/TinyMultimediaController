#include "TrinketHidCombo.h"

// Attiny85 pins
#define ROTARY_CLOCK  0
#define ROTARY_DATA   2
#define ROTARY_BUTTON 5

// Prepare the bit masks
byte bitClock   = 1 << ROTARY_CLOCK;
byte bitData    = 1 << ROTARY_DATA;
byte bitButton  = 1 << ROTARY_BUTTON;
byte bitRotMask = bitClock | bitData;

// Button stuff
bool          btnDown     = false;  // Flag the button is pressed or not
unsigned long btnMillis   = 0UL;    // Time of the last stable change
unsigned long btnDebounce = 5UL;    // The debounce time to wait for
unsigned long btnDuration = 0UL;    // The duration of the last action
unsigned long btnLong     = 2000UL; // Long press duration

// Rotary stuff
bool          rotActive   = false;  // Flag to know if it's rotating

/**
  Read the rotary encoder and return the rotating direction

  @return the rotating direction
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
    if      (pinsLast == bitClock)  result = +1;
    else if (pinsLast == bitData)   result = -1;
  }

  // Keep current value
  pinsLast = pinsNow;
  //DigiKeyboard.sendKeyStroke(10 + pinsNow); // G, H, K, L

  // Return the result
  return result;
}

void setup() {
  // Set the encoder and button pins to input
  pinMode(ROTARY_BUTTON,  INPUT);
  pinMode(ROTARY_CLOCK,   INPUT);
  pinMode(ROTARY_DATA,    INPUT);

  //pinMode(1,OUTPUT);
  //digitalWrite(1, LOW);

  // Turn on pullup resistors
  //PORTB |= bitButton;
  digitalWrite(ROTARY_BUTTON, HIGH);
  //digitalWrite(ROTARY_CLOCK, HIGH);
  //digitalWrite(ROTARY_DATA, HIGH);

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
      // It should have been down, check if this a real action
      if (btnMillis + btnDebounce > nowMillis) {
        // It's unstable, increase the time and flag it is not an action
        btnMillis = nowMillis;
        btnDuration = 0UL;
      }
      else {
        // This is an action, keep the duration of the action
        btnDuration = nowMillis - btnMillis;
        btnDown = false;
        // And keep the time the button came up
        btnMillis = nowMillis;
      }
    }
  }
  else {
    // Button is down, check if it should have been up
    if (!btnDown and btnMillis + btnDebounce < nowMillis) {
      // Flag the button is down and keep the time
      btnDown = true;
      btnMillis = nowMillis;
      btnDuration = 0UL;
    }
  }

  // Check the rotary
  char rotDirection = readRotary();
  if (rotDirection != 0) rotActive = true;

  // Do the actions
  if (btnDown) {
    // The button is down, check if there is any rotation
    if (rotDirection > 0) {
      // Next
      TrinketHidCombo.pressMultimediaKey(MMKEY_SCAN_NEXT_TRACK);
    }
    else if (rotDirection < 0) {
      // Prev
      TrinketHidCombo.pressMultimediaKey(MMKEY_SCAN_PREV_TRACK);
    }
  }
  else {
    // Check if the button was down and for how long
    if (btnDuration > 0UL) {
      if (not rotActive) {
        if (btnDuration > btnLong) {
          // Long press STOP
          TrinketHidCombo.pressMultimediaKey(MMKEY_STOP);
        }
        else {
          // Short press PLAY/PAUSE
          TrinketHidCombo.pressMultimediaKey(MMKEY_PLAYPAUSE);
        }
      }
      else
        // Reset the rotation flag
        rotActive = false;
      // Reset the press duration
      btnDuration = 0UL;
    }
    else {
      // The button was up, check if there is any rotation
      if (rotDirection > 0) {
        // VolUp
        TrinketHidCombo.pressMultimediaKey(MMKEY_VOL_UP);
      }
      else if (rotDirection < 0)  {
        // VolDn
        TrinketHidCombo.pressMultimediaKey(MMKEY_VOL_DOWN);
      }
    }
  }
}
