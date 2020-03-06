// Written by till busch <till@bux.at>
// Adapted from code written by Tony DiCola for Adafruit Industries.
// Released under a MIT license: https://opensource.org/licenses/MIT

#include <Wire.h>
#include <Adafruit_LEDBackpack.h>
#include <MIDI.h>
#include <Bounce2.h>

//#define DEBUG
#define SLEEP_DELAY 60000

#define MIDI_CHANNEL 16

#define DISPLAY_ADDRESS 0x70
/*
    —       1
   | |    6   2
    –       7
   | |    5   3
    — .     4   8
*/
#define CHAR_A 0b01110111
#define CHAR_B 0b01111100
#define CHAR_D 0b01011110
#define CHAR_E 0b01111001
#define CHAR_R 0b01010000
#define CHAR_T 0b00110001

#ifdef DEBUG
#define DEBUG_PRINT(x)    Serial.print(x)
#define DEBUG_PRINTLN(x)  Serial.println(x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#endif

Adafruit_7segment clockDisplay = Adafruit_7segment();
MIDI_CREATE_DEFAULT_INSTANCE();
Bounce mode_button;

bool sleeping = true;

enum SequenceState {
  Stop,
  Start,
  Continue
};

SequenceState sequence_state = Stop;

unsigned long clock_millis = 0;
unsigned long offset_millis = 0;
unsigned long millis_at_stop = 0;
unsigned long current_millis = 0;
unsigned long millis_after_active = 0;

enum DisplayMode {
  ModeTimer,
  ModePulse
};

DisplayMode display_mode = ModeTimer;

void setup() {
  Serial.begin(115200);

  clockDisplay.begin(DISPLAY_ADDRESS);
  clockDisplay.setBrightness(0);

  uint8_t s_red[] =  { CHAR_R, CHAR_E, CHAR_D };
  uint8_t s_beat[] = { CHAR_B, CHAR_E, CHAR_A, CHAR_T };

  uint8_t pos = 0;
  for (uint8_t i = 0; i < sizeof(s_red); i++) {
    clockDisplay.writeDigitRaw(pos, s_red[i]);
    clockDisplay.writeDisplay();
    pos++;
    if (pos == 2)
      pos++;

    delay(100);
  }
  delay(400);

  clockDisplay.writeDigitRaw(1, 0);
  clockDisplay.writeDigitRaw(3, 0);
  clockDisplay.writeDigitRaw(4, 0);

  pos = 0;
  for (uint8_t i = 0; i < sizeof(s_beat); i++) {
    clockDisplay.writeDigitRaw(pos, s_beat[i]);
    clockDisplay.writeDisplay();
    pos++;
    if (pos == 2)
      pos++;

    delay(100);
  }
  delay(400);

  //MIDI.setHandleNoteOn(handleNoteOn);
  //MIDI.setHandleNoteOff(handleNoteOff);
  MIDI.setHandleControlChange(handleControlChange);

  MIDI.setHandleStart(handleStart);
  MIDI.setHandleStop(handleStop);
  MIDI.setHandleContinue(handleContinue);

  MIDI.setHandleClock(handleClock);
  MIDI.begin(MIDI_CHANNEL);
  MIDI.turnThruOff();
  clockDisplay.clear();
  clockDisplay.writeDisplay();
  pinMode(22, INPUT_PULLUP);
  mode_button.attach(22);
}

void loop() {
  static unsigned long last_t = 0xffffffff;
  current_millis = millis();

  MIDI.read();
  mode_button.update();
  
  if(mode_button.fell()) {
    Serial.println("display_mode");
    display_mode = display_mode == ModeTimer ? ModePulse : ModeTimer;
    last_t = 0xffffffff;
    millis_after_active = current_millis;
    sleeping = false;
    updateBarDisplay();
    updateQuarterDisplay();
  }

  switch (sequence_state) {
    case Start:
    case Continue:
      clock_millis = current_millis - offset_millis;
      break;
    case Stop:
      if (!sleeping && current_millis > millis_after_active + SLEEP_DELAY) {
        DEBUG_PRINT(current_millis);
        DEBUG_PRINTLN(" sleep");
        sleeping = true;
        last_t = 0xffffffff; // force immediate update after wake
        clockDisplay.clear();
        clockDisplay.writeDisplay();
      }
      break;
  }

  if (!sleeping && display_mode == ModeTimer) {
    unsigned long t = clock_millis / 1000;
    if (t != last_t) {
      //t += (3600UL*15 - 5);
      unsigned long hours = t / 3600;
      unsigned long minutes = t / 60;
      unsigned long seconds = t - minutes * 60;
      minutes -=  (hours * 60);
      unsigned long displayValue = minutes * 100 + seconds;

      clockDisplay.writeDigitNum(0, (displayValue / 1000), (hours & 0x08) != 0);
      clockDisplay.writeDigitNum(1, (displayValue / 100) % 10, (hours & 0x04) != 0);
      clockDisplay.drawColon(true);
      clockDisplay.writeDigitNum(3, (displayValue / 10) % 10, (hours & 0x02) != 0);
      clockDisplay.writeDigitNum(4, displayValue % 10, hours & 0x01);

      clockDisplay.writeDisplay();
      last_t = t;
    }
  }
}

unsigned long pulses_per_quarter = 24;   // pulses per quater
unsigned long quarters_per_bar = 4;      // quaters per bar
unsigned long pulses = 1;
unsigned long bars = 1;
unsigned long quarters = 1;

void handleClock()
{
  if (sequence_state == Start || sequence_state == Continue)
    pulses++;
  if (pulses == pulses_per_quarter + 1) {
    pulses = 1;
    quarters++;
    if (quarters == quarters_per_bar + 1) {
      quarters = 1;
      bars++;
      updateBarDisplay();
    }
    updateQuarterDisplay();
  }
}

void updateBarDisplay()
{
  if (display_mode == ModePulse)
  {
    clockDisplay.writeDigitNum(0, (bars / 100) % 10);
    clockDisplay.writeDigitNum(1, (bars / 10) % 10);
    clockDisplay.writeDigitNum(3, (bars) % 10, true);
    clockDisplay.drawColon(false);
  }
}

void updateQuarterDisplay()
{
  if (display_mode == ModePulse)
  {
    clockDisplay.writeDigitNum(4, quarters);
    clockDisplay.writeDisplay();
  }
}

void handleStart()
{
  DEBUG_PRINT(current_millis);
  DEBUG_PRINTLN("\tstart()");
  sleeping = false;
  if (sequence_state == Stop) {
    offset_millis = current_millis;
  }
  sequence_state = Start;

  bars = 1;
  quarters = 1;
  pulses = 1;
  updateBarDisplay();
  updateQuarterDisplay();
}

void handleStop()
{
  DEBUG_PRINT(current_millis);
  DEBUG_PRINTLN("\tstop()");
  millis_at_stop = current_millis;
  millis_after_active = current_millis;
  sequence_state = Stop;
  updateBarDisplay();
}

void handleContinue()
{
  DEBUG_PRINT(current_millis);
  DEBUG_PRINTLN("\tcontinue()");

  if (sequence_state == Stop) {
    offset_millis += current_millis - millis_at_stop;
    DEBUG_PRINTLN(current_millis);
  }
  sequence_state = Continue;
  sleeping = false;
}

void handleControlChange(byte channel, byte control, byte value)
{
  DEBUG_PRINT("handleControlChange(");
  DEBUG_PRINT(channel);
  DEBUG_PRINT(", ");
  DEBUG_PRINT(control);
  DEBUG_PRINT(", ");
  DEBUG_PRINT(value >> 3);
  DEBUG_PRINTLN(")");
  if (control == midi::SoundController5) { /* 74 ///< Synth: Brightness        FX: Expander On/Off */
    clockDisplay.setBrightness(value >> 3);
  }
  if (control == midi::SoundController6) { /* 75 ///< Synth: Decay Time        FX: Reverb On/Off */
    display_mode = value ? ModePulse : ModeTimer;
  }
}

void handleNoteOn(byte channel, byte pitch, byte velocity)
{
  DEBUG_PRINT(millis());
  DEBUG_PRINT("\tnoteOn (");
  DEBUG_PRINT(channel);
  DEBUG_PRINT(", ");
  DEBUG_PRINT(pitch);
  DEBUG_PRINT(", ");
  DEBUG_PRINT(velocity);
  DEBUG_PRINTLN(")");
}

void handleNoteOff(byte channel, byte pitch, byte velocity)
{
  DEBUG_PRINT(millis());
  DEBUG_PRINT("\tnoteOff(");
  DEBUG_PRINT(channel);
  DEBUG_PRINT(", ");
  DEBUG_PRINT(pitch);
  DEBUG_PRINT(", ");
  DEBUG_PRINT(velocity);
  DEBUG_PRINTLN(")");
}
