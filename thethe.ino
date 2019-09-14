#include <NewPing.h>
#include <MIDI.h>

// General constants
const int ANTENNA1 = 1;                         // Antenna 1
const int ANTENNA2 = 2;                         // Antenna 2
const int SENSE_DELAY_MS = 15;                  // Pause in millis before reading antennas again
const int NO_NOTE = -1;                         // No note detected
const int NO_CONTROLLER_VALUE = -1;             // No controller value detected
const double FREE_FIRST_CM = 4.0;               // Free space before start sensing antennas 
const double MAX_DISTANCE_CM = 60.0;            // Max recognized/allowed distance for both antennas
const int DEFAULT_VELOCITY = 100;               // Default velocity
const int SONAR_ITERATIONS = 5;                 // Number of sonar iterations to loop for better measurements
const int MAX_CONTROLLER_VALUE = 127;           // Max value for the controller (antenna 2) value
const int INTERVALS_PER_SCALE = 12;             // Max number of intervals from root of every scale

// Scales
const int CHROMATIC = 0;
const int MAJOR = 1;
const int MINOR = 2;
const int HARMONIC_MINOR = 3;
const int MELODIC_MINOR = 4;
const int PENTATONIC_MAJOR = 5;
const int PENTATONIC_MINOR = 6;
const int BLUES = 7;

const int SCALE_INTERVALS_TO_OCTAVE[] = {  
  12,
  7,
  7,
  7,
  7,
  5,
  5,
  6
};

const int SCALE_INTERVALS[][12] = {  
  {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
  {2, 2, 1, 2, 2, 2, 1, 0, 0, 0, 0, 0},
  {2, 1, 2, 2, 1, 2, 2, 0, 0, 0, 0, 0},
  {2, 1, 2, 2, 1, 3, 1, 0, 0, 0, 0, 0},
  {2, 1, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0},
  {2, 2, 3, 2, 3, 0, 0, 0, 0, 0, 0, 0},
  {3, 2, 2, 3, 2, 0, 0, 0, 0, 0, 0, 0},
  {3, 2, 1, 1, 3, 2, 0, 0, 0, 0, 0, 0}
};

// Antenna 1 (note) pin numbers
const int ANTENNA1_TRIGGER_PIN = 2;
const int ANTENNA1_ECHO_PIN = 3;

// Antenna 2 (volume/other) pin numbers
const int ANTENNA2_TRIGGER_PIN = 7;
const int ANTENNA2_ECHO_PIN = 8;

// Sonars
NewPing SONAR1 (ANTENNA1_TRIGGER_PIN, ANTENNA1_ECHO_PIN, MAX_DISTANCE_CM + FREE_FIRST_CM);
NewPing SONAR2 (ANTENNA2_TRIGGER_PIN, ANTENNA2_ECHO_PIN, MAX_DISTANCE_CM + FREE_FIRST_CM);

// Controllers
const int VELOCITY = -1;
const int MODULATION = 1;
const int PORTAMENTO_TIME = 5;
const int VOLUME = 7;
const int BALANCE = 8;
const int PAN = 10;
const int EXPRESSION = 11;


// ---------------------------------------------
// ---------------- State ----------------------
// ---------------------------------------------
// Scale
int scale = PENTATONIC_MINOR;
// Octaves
int octaves = 1;
// This is what antenna 2 is controlling
int controllerNumber = MODULATION;
// Last played note
int lastNote = NO_NOTE;
// MIDI output chanel
int midiChannel = 1;
// Base MIDI note
int baseNote = 60; // Middle C
// Last controller value
int lastControllerValue = NO_CONTROLLER_VALUE;
// Reverse distance meaning for antenna 1?
boolean reverseDistanceMeaningAntenna1 = false;
// Reverse distance meaning for antenna 2?
boolean reverseDistanceMeaningAntenna2 = false;
// Flip antennas (antenna 1 becomes antenna 2 and viceversa)
boolean flipAntennas = false;
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
  int note = readNote();
  int controllerNumber = readControllerNumber();
  int controllerValue = readControllerValue();

  int velocity = controllerNumber == VELOCITY ? controllerValue : DEFAULT_VELOCITY;

  // Clear current note, if any, if needed
  if (lastNote != NO_NOTE && note != lastNote) 
    noteOn(lastNote, 0, midiChannel);
    
  // Play new note, if any
  if (note != NO_NOTE && note != lastNote)
    noteOn(note, velocity, midiChannel);  
  lastNote = note;

  if (controllerNumber != VELOCITY && controllerValue != NO_CONTROLLER_VALUE && controllerValue != lastControllerValue) {
    controlChange(controllerNumber, controllerValue, midiChannel);
    lastControllerValue = controllerValue;
  }
  
  delay(SENSE_DELAY_MS);
}

void noteOn(int note, int velocity, int channel) {
  MIDI.sendNoteOn(note, velocity, channel);
}

void controlChange(int controller, int value, int channel) {
 MIDI.sendControlChange(controller, value, channel);
}

int readNote() {
  double distanceInCm = distance(ANTENNA1) - FREE_FIRST_CM;
  if (distanceInCm <= 0 || distanceInCm >= MAX_DISTANCE_CM) return NO_NOTE;

  const int intervalsToOctave = SCALE_INTERVALS_TO_OCTAVE[scale];
  const int totalIntervals = intervalsToOctave * octaves;
  const double intervalSpaceInCm = MAX_DISTANCE_CM / (totalIntervals + octaves);
  const int intervals = floor(distanceInCm / intervalSpaceInCm);

  int note = baseNote;
  for(int i = 0; i < intervals; i++)
    note += SCALE_INTERVALS[scale][i % intervalsToOctave];
    
  return note;
}

int readControllerNumber() {
  return controllerNumber;
}

int readControllerValue() {
  double realDistance = distance(ANTENNA2);
  if (realDistance == 0.0) return NO_CONTROLLER_VALUE;
  
  double distanceInCm = realDistance - FREE_FIRST_CM;
  if (distanceInCm <= 0 || distanceInCm >= MAX_DISTANCE_CM) return NO_CONTROLLER_VALUE;
  
  int value = distanceInCm / MAX_DISTANCE_CM * MAX_CONTROLLER_VALUE;
  return reverseDistanceMeaningAntenna2 ? (MAX_CONTROLLER_VALUE - value) : value;
}

double distance(int antenna) {
  int reading = 0;
  if (flipAntennas)
    reading = (antenna == 2) ? 
      (SONAR1.ping_median(SONAR_ITERATIONS)) : 
      (SONAR2.ping_median(SONAR_ITERATIONS));
  else
    reading = (antenna == 1) ? 
        (SONAR1.ping_median(SONAR_ITERATIONS)) : 
        (SONAR2.ping_median(SONAR_ITERATIONS));
  
  return reading / 29.0 / 2.0;
}
