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
bool          btnDown     = false;  // Flag the button is pressed or not
unsigned long btnMillis   = 0UL;    // Time of the last stable change
unsigned long btnDebounce = 5UL;    // The debounce time to wait for
unsigned long btnDuration = 0UL;    // The duration of the last action
unsigned long btnLong     = 1000UL; // Long press duration

// Rotary stuff
bool          rotActive   = false;  // Flag to know if it's rotating

// LED stuff
bool          ledLight    = false;  // Flag to know the LED is on of off
unsigned long ledTimeout  = 0UL;    // The time when the LED turns off
unsigned long ledDelay    = 100UL;  // Time delay after which the LED turns off

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
    if      (pinsLast == bitClock)  result++;
    else if (pinsLast == bitData)   result--;
  }

  // Keep current value
  pinsLast = pinsNow;

  // Return the result
  return result;
}

/**
  Send a keypress

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
  if (ledLight and nowMillis > ledTimeout) {
    ledLight = false;
    PORTB &= ~bitLed;
  }
}

void setup() {
  // Set the encoder and button pins to input
  pinMode(ROTARY_BUTTON,  INPUT_PULLUP);
  pinMode(ROTARY_CLOCK,   INPUT);
  pinMode(ROTARY_DATA,    INPUT);

  // Set the LED pin to output and flash it
  pinMode(ROTARY_LED,    OUTPUT);
  ledOn();

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
    if      (rotDirection > 0) keySend(MMKEY_SCAN_NEXT_TRACK);  // Next
    else if (rotDirection < 0) keySend(MMKEY_SCAN_PREV_TRACK);  // Prev
  }
  else {
    // Check if the button was down and for how long
    if (btnDuration > 0UL) {
      if (not rotActive) {
        if (btnDuration > btnLong)  keySend(MMKEY_STOP);        // Long press STOP
        else                        keySend(MMKEY_PLAYPAUSE);   // Short press PLAY/PAUSE
      }
      else
        // Reset the rotation flag
        rotActive = false;
      // Reset the press duration
      btnDuration = 0UL;
    }
    else {
      // The button was up, check if there is any rotation
      if      (rotDirection > 0)  keySend(MMKEY_VOL_UP);        // VolUp
      else if (rotDirection < 0)  keySend(MMKEY_VOL_DOWN);      // VolDn
    }
  }

  // Turn the LED off, if timed out
  ledOff(nowMillis);
}
