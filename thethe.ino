#include <MIDI.h>
#include <NewPing.h>

#define MAX_SENSORS                  2
#define TRIGGER1_PIN                 2
#define ECHO1_PIN                    3
#define TRIGGER2_PIN                 4
#define ECHO2_PIN                    5
#define PINGS                        3
#define MAX_DISTANCE_CM             55
#define DEAD_ZONE_CM                 5
#define HIT_VELOCITY_THRESHOLD      20
#define HIT_TIME_THRESHOLD         120

// Sensor Modes
#define NOTE                         0
#define CC                           1
#define BEND                         2
#define HIT                          3
#define DISABLED                    99

// Scales/Pitch class sets
#define CHROMATIC                    0
#define MAJOR                        1
#define MINOR                        2
#define HARMONIC_MINOR               3
#define MAJOR_PENTATONIC             4
#define MINOR_PENTATONIC             5
#define WHOLE_TONE                   6
#define GM_BASSDRUM_SNARE           70
#define ONE_FOUR_FIVE               71

// CC 
#define CC_CENTER                   64
#define CC_NO_RESET                 -1

// GM MIDI Drum Kit instruments
#define BASS_DRUM                   36
#define SNARE                       38
#define CLOSED_HI_HAT               42
#define OPEN_HI_HAT                 46
#define CRASH_1                     49


// Configuration
int CHROMATIC_INTERVALS[]                                         = {1};
int MAJOR_INTERVALS[]                                             = {2, 2, 1, 2, 2, 2, 1};
int MINOR_INTERVALS[]                                             = {2, 1, 2, 2, 1, 2, 2};
int HARMONIC_MINOR_INTERVALS[]                                    = {2, 1, 2, 2, 1, 3, 1};
int MAJOR_PENTATONIC_INTERVALS[]                                  = {2, 2, 3, 2, 3};
int MINOR_PENTATONIC_INTERVALS[]                                  = {3, 2, 2, 3, 2};
int WHOLE_TONE_INTERVALS[]                                        = {2, 2, 2, 2, 2, 2};


// General parameters
boolean activated[MAX_SENSORS]                                    = {true, true};
boolean reversed[MAX_SENSORS]                                     = {false, false};
int steps[MAX_SENSORS]                                            = {6, 3};                                                                    // How many divisions on the measureable space (MAX_DISTANCE_CM)
int channel[MAX_SENSORS]                                          = {1, 10};
int mode[MAX_SENSORS]                                             = {NOTE, HIT};
// CC mode-related parameters
int cc[MAX_SENSORS]                                               = {1, 1};
int ccNoReadingValue[MAX_SENSORS]                                 = {CC_CENTER, CC_NO_RESET};
// NOTE mode-related parameters
int root[MAX_SENSORS]                                             = {60, 36};
int scale[MAX_SENSORS]                                            = {MINOR_PENTATONIC, ONE_FOUR_FIVE};
boolean latch[MAX_SENSORS]                                        = {true, true};
boolean noReadingEndsLatch[MAX_SENSORS]                           = {true, true};                                                             // 'noReadingEndsLatch = false': 100% true latching
int velocity[MAX_SENSORS]                                         = {100, 100};
int fixedDuration[MAX_SENSORS]                                    = {250, -1};                                                                // For non-latching option, HIT mode, etc.
int fixedPitch[MAX_SENSORS]                                       = {-1, OPEN_HI_HAT};                                                        // For HIT operation mode


// Tracking & State
int notes[MAX_SENSORS][128];
int last[MAX_SENSORS] = { -1, -1};
unsigned long times[MAX_SENSORS] = {millis(), millis()};


// Program
MIDI_CREATE_DEFAULT_INSTANCE();

void setup() {
  for (int s = 0; s < MAX_SENSORS; s++)
    defineScaleNotes(s);

  MIDI.begin();
  Serial.begin(115200);
}

NewPing S[] = {NewPing (TRIGGER1_PIN, ECHO1_PIN, MAX_DISTANCE_CM + DEAD_ZONE_CM), NewPing (TRIGGER2_PIN, ECHO2_PIN, MAX_DISTANCE_CM + DEAD_ZONE_CM)};

void loop() {
  int value;

  for (int s = 0; s < MAX_SENSORS; s++) {
    if (!activated[s])
      continue;
      
    value = -1;

    switch (mode[s]) {
      case NOTE:
        value = readNote(s);
        if (value > 0) {
          if (last[s] != value) {
            if (latch[s] && last[s] != -1)
              MIDI.sendNoteOn(last[s], 0, channel[s]);

            last[s] = value;
            //velocity[s] = ???; // read from another sensor!
            MIDI.sendNoteOn(last[s], velocity[s], channel[s]);

            if (!latch[s]) {
              if (fixedDuration[s] > 0)
                delay(fixedDuration[s]);
              MIDI.sendNoteOn(last[s], 0, channel[s]);
            }
          }
        } else {
          if (latch[s] && noReadingEndsLatch[s] && last[s] != -1) {
            MIDI.sendNoteOn(last[s], 0, channel[s]);
            last[s] = -1;
          }
        }
        break;

      case CC:
        value = readCC(s);
        if (value > 0) {
          if (last[s] != value) {
            MIDI.sendControlChange(cc[s], value, channel[s]);
            last[s] = value;
          }
        } else {
          if (ccNoReadingValue[s] != CC_NO_RESET) {
            MIDI.sendControlChange(cc[s], ccNoReadingValue[s], channel[s]);
            last[s] = ccNoReadingValue[s];
          }
        }
        break;

      case BEND:
        value = readPitchBend(s);
        if (last[s] != value) {
          MIDI.sendPitchBend(value, channel[s]);
          last[s] = value;
        }
        break;

      case HIT:
        if (fixedPitch[s] != -1) {
          value = readVelocity(s);
          unsigned long now = millis();
          if ((now - times[s]) > HIT_TIME_THRESHOLD) {
            if (abs(last[s] - value) > HIT_VELOCITY_THRESHOLD) {            
              MIDI.sendNoteOn(fixedPitch[s], value, channel[s]); // Debounce needed?
              if (fixedDuration[s] > 0) {
                delay(fixedDuration[s]);
                MIDI.sendNoteOff(fixedPitch[s], 0, channel[s]);
              }
              last[s] = value;
              times[s] = now;
            }
          }          
        }
        break;
    }
  }
}

int readNote(int sensor) {
  int distance = readDistanceCM(sensor);
  if (distance <= DEAD_ZONE_CM || distance >= MAX_DISTANCE_CM + DEAD_ZONE_CM) return 0;

  int netDistance = distance - DEAD_ZONE_CM;
  return notes[sensor][
           reversed[sensor] ?
           map(netDistance, 0, MAX_DISTANCE_CM, steps[sensor], 0) - 1 :
           map(netDistance, 0, MAX_DISTANCE_CM, 0, steps[sensor])
         ];
}

int readCC(int sensor) {
  int distance = readDistanceCM(sensor);
  if (distance <= DEAD_ZONE_CM) return 0;

  int netDistance = distance - DEAD_ZONE_CM;
  return reversed[sensor] ?
         map(netDistance, 0, MAX_DISTANCE_CM, 127, 0) :
         map(netDistance, 0, MAX_DISTANCE_CM, 0, 127);
}

int readPitchBend(int sensor) {
  int distance = readDistanceCM(sensor);
  if (distance <= DEAD_ZONE_CM) return 0;

  int netDistance = distance - DEAD_ZONE_CM;
  return reversed[sensor] ?
         map(netDistance, 0, MAX_DISTANCE_CM, 8191, -8192) :
         map(netDistance, 0, MAX_DISTANCE_CM, -8192, 8191);
}

int readVelocity(int sensor) {
  int distance = readDistanceCM(sensor);
  if (distance <= DEAD_ZONE_CM) return 0;

  int netDistance = distance - DEAD_ZONE_CM;
  return reversed[sensor] ?
         map(netDistance, 0, MAX_DISTANCE_CM, 127, 0) :
         map(netDistance, 0, MAX_DISTANCE_CM, 0, 127);
}

int readDistanceCM(int sensor) {
  return S[sensor].convert_cm(S[sensor].ping_median(PINGS));
}

void defineScaleNotes(int sensor) {
  notes[sensor][0] = root[sensor];
  int numberOfIntervals = 0;
  int intervals[12];
  switch (scale[sensor]) {
    case CHROMATIC:
      numberOfIntervals = 1;
      memcpy(intervals, CHROMATIC_INTERVALS, sizeof(CHROMATIC_INTERVALS));
      break;
    case MAJOR:
      numberOfIntervals = 7;
      memcpy(intervals, MAJOR_INTERVALS, sizeof(MAJOR_INTERVALS));
      break;
    case MINOR:
      numberOfIntervals = 7;
      memcpy(intervals, MINOR_INTERVALS, sizeof(MINOR_INTERVALS));
      break;
    case HARMONIC_MINOR:
      numberOfIntervals = 7;
      memcpy(intervals, HARMONIC_MINOR_INTERVALS, sizeof(HARMONIC_MINOR_INTERVALS));
      break;
    case MAJOR_PENTATONIC:
      numberOfIntervals = 5;
      memcpy(intervals, MAJOR_PENTATONIC_INTERVALS, sizeof(MAJOR_PENTATONIC_INTERVALS));
      break;
    case MINOR_PENTATONIC:
      numberOfIntervals = 5;
      memcpy(intervals, MINOR_PENTATONIC_INTERVALS, sizeof(MINOR_PENTATONIC_INTERVALS));
      break;
    case WHOLE_TONE:
      numberOfIntervals = 6;
      memcpy(intervals, WHOLE_TONE_INTERVALS, sizeof(WHOLE_TONE_INTERVALS));
      break;
    case GM_BASSDRUM_SNARE:
      steps[sensor] = 2;
      notes[sensor][0] = BASS_DRUM;
      notes[sensor][1] = SNARE;
      return;
    case ONE_FOUR_FIVE:
      steps[sensor] = 3;
      notes[sensor][0] = root[sensor];     // root
      notes[sensor][1] = root[sensor] + 5; // perfect fourth
      notes[sensor][2] = root[sensor] + 7; // perfect fifth;
      return;
  }

  for (int i = 1; i < steps[sensor]; i++)
    notes[sensor][i] = notes[sensor][i - 1] + intervals[(i - 1) % numberOfIntervals];
}
