#include <NewPing.h>
#include <MIDI.h>

// General constants
const int ANTENNA1 = 1;                         // Antenna 1
const int ANTENNA2 = 2;                         // Antenna 2
const int SENSE_DELAY_MS = 40;                  // Pause in millis before reading antennas again
const int NO_PITCH = -1;                        // No pitch detected
const int MAX_DISTANCE_CM = 60;                 // Max recognized/allowed distance for both antennas
const int DEFAULT_VELOCITY = 100;               // Default velocity
const int SONAR_ITERATIONS = 5;                 // Number of sonar iterations to loop for better measurements


// Scales
const int CHROMATIC = 0;
const int MAJOR = 1;
const int MINOR = 2;
const int HARMONIC_MINOR = 3;
const int MELODIC_MINOR = 4;
const int PENTATONIC_MAJOR = 5;
const int PENTATONIC_MINOR = 6;

// Scale intervals
/*
const int SCALE_INTERVALS[][] = {  
  {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
  {2, 2, 1, 2, 2, 2, 1},
  {2, 1, 2, 2, 1, 2, 2},
  {2, 1, 2, 2, 1, 3, 1},
  {2, 1, 2, 2, 2, 2, 1},
  {2, 2, 3, 2, 3},
  {3, 2, 2, 3, 2}
};
*/

// Antenna 1 (pitch) pin numbers
const int ANTENNA1_TRIGGER_PIN = 2;
const int ANTENNA1_ECHO_PIN = 3;

// Antenna 2 (volume/other) pin numbers
const int ANTENNA2_TRIGGER_PIN = 7;
const int ANTENNA2_ECHO_PIN = 8;

// Sonars
NewPing SONAR1 (ANTENNA1_TRIGGER_PIN, ANTENNA1_ECHO_PIN, MAX_DISTANCE_CM);
NewPing SONAR2 (ANTENNA2_TRIGGER_PIN, ANTENNA2_ECHO_PIN, MAX_DISTANCE_CM);

// Modifiers
const int VOLUME = 1;


// ---------------------------------------------
// ---------------- State ----------------------
// ---------------------------------------------
// Octaves
int octaves = 1;
// This is what antenna 2 is controlling
int modifierType = VOLUME;
// Last played pitch
int lastPitch = NO_PITCH;
// MIDI output chanel
int midiChannel = 1;
// ---------------------------------------------

// MIDI init
MIDI_CREATE_DEFAULT_INSTANCE();

void setup()
{
  // Setup pins for both antennas
  pinMode(ANTENNA1_TRIGGER_PIN, OUTPUT);
  pinMode(ANTENNA1_ECHO_PIN, INPUT);
  pinMode(ANTENNA2_TRIGGER_PIN, OUTPUT);
  pinMode(ANTENNA2_ECHO_PIN, INPUT);
  
  // Prepare for MIDI I/O
  MIDI.begin(); 
  Serial.begin(115200);
}


void loop()
{
  int pitch = readPitch();
  int modifierType = readModifierType();
  int modifierValue = 64; //readModifierValue();

  int velocity = modifierType == VOLUME ? modifierValue : DEFAULT_VELOCITY;

  if (pitch == NO_PITCH) {
    // Clear current pitch, if any
    if (lastPitch != NO_PITCH)
      MIDI.sendNoteOn(lastPitch, 0, midiChannel);      
  } else {
    // Clear if new pitch is different from previous one, if any
    if (pitch != lastPitch) {
      if (lastPitch != NO_PITCH)
        MIDI.sendNoteOn(lastPitch, 0, midiChannel);
      MIDI.sendNoteOn(pitch, velocity, midiChannel);
    }
  }

  lastPitch = pitch;
  delay(SENSE_DELAY_MS);
}

int readPitch() {
  double distanceInCm = distance(ANTENNA1);
  if (distanceInCm <= 0 || distanceInCm > MAX_DISTANCE_CM) return NO_PITCH;

  // TODO: Pitch
  const int BASE = 72;
  switch((int)(distanceInCm / 10)) {
    case 0: return BASE;
    case 1: return BASE+3;
    case 2: return BASE+5;
    case 3: return BASE+7;
    case 4: return BASE+10;
    case 5: return BASE+12;
  }
}

int readModifierType() {
  return modifierType;
}

int readModifierValue() {
  double distanceInCm = distance(ANTENNA2);
  // TODO: Check range for the current modifier type
  return distanceInCm;
}

double distance(int antenna) {
  return (antenna == 1) ? 
    SONAR1.convert_cm(SONAR1.ping_median(SONAR_ITERATIONS)) : 
    SONAR2.convert_cm(SONAR2.ping_median(SONAR_ITERATIONS));
}
