// Core functionality for the Apis eyes. Manages hardware controls, requests frames from
// a set of hardcoded "programs", manages interpolation between requested frames, and
// handles communication with the RGB LED strips.
// JBK (jb@afternight.org)

#include <SPI.h>
#include <TCL.h>
// Definition of interrupt names
#include < avr/io.h >
// ISR interrupt service routine
#include < avr/interrupt.h >

#define NUM_EYES 2
#define NUM_LEDS 100
#define FRAME_WIDTH 9
#define FRAME_HEIGHT 13
#define FRAME_DURATION_MAX_MS 2000
#define FRAME_DURATION_MIN_MS 35
#define FRAME_DURATION_DIF_MS 1965
#define NUM_PATTERNS 3

#define RESTART_BUTT_PIN 5
#define FPS_POT_PIN 6
#define PAT_SEL_POT_PIN 7
#define FPS_POT_MAX_VAL 1023
#define PAT_SEL_POT_MAX_VAL 1023

typedef struct
{
  byte r;
  byte g;
  byte b;
} 
ColorRGB;

// Some basic colors
const ColorRGB WHITE_RGB = { 0xff, 0xff, 0xff };
const ColorRGB BLACK_RGB = { 0xff, 0xff, 0xff };

ColorRGB prevFrameMatrix[FRAME_WIDTH][FRAME_HEIGHT];
ColorRGB nextFrameMatrix[FRAME_WIDTH][FRAME_HEIGHT];
ColorRGB currFrameMatrix[FRAME_WIDTH][FRAME_HEIGHT];
ColorRGB frameStream[100];

//// FOR INTIALIZATION TESTING
const byte red[] = { 0xff, 0xff, 0xff, 0x00, 0x00};
const byte green[] = { 0x00, 0x60, 0xb0, 0x80, 0x00};
const byte blue[] = { 0x00, 0x00, 0x00, 0x00, 0xff};
//// END TESTING

// Making these vars global to avoid allocation time on each loop. Poor practice,
// but more efficient.
unsigned long frameTimeOffsetMs = 0UL;
unsigned int frameMs = 0UL;

// Controlled by potentiometer- optional for use by frame display patterns to control an arbitrary parameter.
//volatile byte modValue = 0;
unsigned int frameDurationMs = 100;

volatile int frameCount = 0;
volatile byte currentPattern = 3;
int patternPotWindow = 1023 / NUM_PATTERNS;

void setup() {
//  attachInterrupt(0, prevPatternInterrupt, RISING);
//  attachInterrupt(1, nextPatternInterrupt, RISING);
  attachInterrupt(RESTART_BUTT_PIN, restartPatternInterrupt, CHANGE);

  // initMatrixBlank();
  initNextFrameBlank();
  Serial.begin(9600);
  TCL.begin();
}

void loop() {
  frameTimeOffsetMs = millis();
  frameMs = 0UL;

  // Check the position of the pattern select knob  
  int patternSelectPotValue = analogRead(PAT_SEL_POT_PIN);
  currentPattern = patternSelectPotValue / patternPotWindow;
  if(currentPattern >= NUM_PATTERNS)
    currentPattern = NUM_PATTERNS - 1;

  // FIXME shouldn't be necessary to perform this conditional. I'm being stupid.

  createNextFrame();

  // While waiting for the next full frame transition, interpolate LED values to smooth out the animation.
  float interpolationFactor = 0.0f;
  int fpsPotVal = 0;
  while (frameMs < frameDurationMs) {
    // Check the FPS potentiometer
    int fpsPotVal = analogRead(FPS_POT_PIN);
    frameDurationMs = FRAME_DURATION_MIN_MS + ((float)fpsPotVal / (float)FPS_POT_MAX_VAL) * (float)FRAME_DURATION_DIF_MS;
    interpolationFactor = min(1.0f, (float)frameMs / (float)frameDurationMs);

    interpolateFrames(interpolationFactor);
    buildPixelStream();

    TCL.sendEmptyFrame();
    // Send the frame data twice- once for each eye.
    for(byte numEyes = 0; numEyes < NUM_EYES; numEyes++) {
      for(byte i = 0; i < NUM_LEDS; i++) {
        TCL.sendColor(frameStream[i].r, frameStream[i].g, frameStream[i].b);
      }
    }
    TCL.sendEmptyFrame();

    frameMs = millis() - frameTimeOffsetMs;
  }
}

void initNextFrameBlank() {
  ColorRGB pxColor = { 0, 0, 0 };

  for(byte col = 0; col < FRAME_WIDTH; col++) {
    for(byte row = 0; row < FRAME_HEIGHT; row++) {
      nextFrameMatrix[col][row] = pxColor;
    }
  }
}

void initNextFrameWithRandomColors() {
  for(byte col = 0; col < FRAME_WIDTH; col++) {
    for(byte row = 0; row < FRAME_HEIGHT; row++) {
      ColorRGB pxColor = { random(256), random(256), random(256) };
      nextFrameMatrix[col][row] = pxColor;
    }
  }
}

void initNextFrameWithColorRows() {
  byte newR;
  byte newG;
  byte newB;
  ColorRGB newLedVal;

  // Set each column to a single color for testing.
  for(byte col = 0; col < FRAME_WIDTH; col++) {
    newLedVal.r = red[col%5];
    newLedVal.g = green[col%5];
    newLedVal.b = blue[col%5];

    for(byte row = 0; row < FRAME_HEIGHT; row++) {
      prevFrameMatrix[col][row] = newLedVal;
      nextFrameMatrix[col][row] = newLedVal;
      currFrameMatrix[col][row] = newLedVal;
    }
  }
}

void createNextFrame() {
  for(byte col = 0; col < FRAME_WIDTH; col++) {
    for(byte row = 0; row < FRAME_HEIGHT; row++) {
      prevFrameMatrix[col][row] = nextFrameMatrix[col][row];
    }
  }

  switch(currentPattern) {
  case 0: 
    rainbowScrollPatternHorizontal();
    break;
  case 1: 
    nextFrameConway();
    break;
  case 2:
    staticPattern();
    break;
    case 3:
    allWhite();
    break;
  }
  
  frameCount++;
}

void interpolateFrames(float interpolationFactor) {
  for(byte col = 0; col < FRAME_WIDTH; col++) {
    for(byte row = 0; row < FRAME_HEIGHT; row++) {
      currFrameMatrix[col][row].r = (byte)((float)prevFrameMatrix[col][row].r + (interpolationFactor * (float)(nextFrameMatrix[col][row].r - prevFrameMatrix[col][row].r)));
      currFrameMatrix[col][row].g = (byte)((float)prevFrameMatrix[col][row].g + (interpolationFactor * (float)(nextFrameMatrix[col][row].g - prevFrameMatrix[col][row].g)));
      currFrameMatrix[col][row].b = (byte)((float)prevFrameMatrix[col][row].b + (interpolationFactor * (float)(nextFrameMatrix[col][row].b - prevFrameMatrix[col][row].b)));
    }
  }
}

/**
 * Called when the "restart pattern" button is pressed on the control box.
 */
void restartPatternInterrupt() {
  static unsigned long lastRestartPatternInterruptTime = 0;
  unsigned long interruptTime = millis();
  // If interrupts come faster than 200ms, assume it's a bounce and ignore
  if (interruptTime - lastRestartPatternInterruptTime > 200)
  {
    frameCount = 0;
    Serial.println("Button Press");
  }
  lastRestartPatternInterruptTime = interruptTime;
}

/**
 * Called when the "previous pattern" button is pressed on the control box.
 */
void prevPatternInterrupt() {
  static unsigned long lastPrevPatternInterruptTime = 0;
  unsigned long interruptTime = millis();
  // If interrupts come faster than 200ms, assume it's a bounce and ignore
  if (interruptTime - lastPrevPatternInterruptTime > 200)
  {
    currentPattern = (currentPattern - 1) % NUM_PATTERNS;
    frameCount = 0;
  }
  lastPrevPatternInterruptTime = interruptTime; 
}

/**
 * Called when the "next pattern" button is pressed on the control box.
 */
void nextPatternInterrupt() {
  static unsigned long lastNextPatternInterruptTime = 0;
  unsigned long interruptTime = millis();
  // If interrupts come faster than 200ms, assume it's a bounce and ignore
  if (interruptTime - lastNextPatternInterruptTime > 200)
  {
    currentPattern = (currentPattern - 1) % NUM_PATTERNS;
    frameCount = 0;
  }
  lastNextPatternInterruptTime = interruptTime; 
}

/**
 * Takes contents of frameMatrix and generates frameStream.
 */
void buildPixelStream() {
  frameStream[0] = currFrameMatrix[5][0];
  frameStream[1] = currFrameMatrix[4][0];
  frameStream[2] = currFrameMatrix[3][0];
  frameStream[3] = currFrameMatrix[2][0];
  frameStream[4] = currFrameMatrix[1][1];
  frameStream[5] = currFrameMatrix[2][1];
  frameStream[6] = currFrameMatrix[3][1];
  frameStream[7] = currFrameMatrix[4][1];
  frameStream[8] = currFrameMatrix[5][1];
  frameStream[9] = currFrameMatrix[6][1];
  frameStream[10] = currFrameMatrix[7][1];  
  frameStream[11] = currFrameMatrix[7][2];
  frameStream[12] = currFrameMatrix[6][2];
  frameStream[13] = currFrameMatrix[5][2];
  frameStream[14] = currFrameMatrix[4][2];
  frameStream[15] = currFrameMatrix[3][2];
  frameStream[16] = currFrameMatrix[2][2];
  frameStream[17] = currFrameMatrix[1][2];
  ////

  byte frameNum = 18;
  byte col = 0;
  byte increment = 1;
  for(byte row = 3; row <= 10; row++) {
    if(row % 2 == 0) {
      // Even row: decrement
      increment = -1;
      col = 8;
    } 
    else {
      // Odd row: increment
      increment = 1;
      col = 0;
    }

    while(col >= 0 && col < FRAME_WIDTH) {
      frameStream[frameNum] = currFrameMatrix[col][row];
      col += increment;
      frameNum++;
    }
  }

  ////
  frameStream[81] = currFrameMatrix[7][11];
  frameStream[82] = currFrameMatrix[6][11];
  frameStream[83] = currFrameMatrix[5][11];
  frameStream[84] = currFrameMatrix[4][11];
  frameStream[85] = currFrameMatrix[3][11];
  frameStream[86] = currFrameMatrix[2][11];
  frameStream[87] = currFrameMatrix[1][11];
  frameStream[88] = currFrameMatrix[1][12];
  frameStream[89] = currFrameMatrix[2][12];
  frameStream[90] = currFrameMatrix[3][12];
  frameStream[91] = currFrameMatrix[4][12];
  frameStream[92] = currFrameMatrix[5][12];
  frameStream[93] = currFrameMatrix[6][12];
  frameStream[94] = currFrameMatrix[7][12];
  frameStream[95] = currFrameMatrix[5][13];
  frameStream[96] = currFrameMatrix[4][13];
  frameStream[97] = currFrameMatrix[3][13];
  frameStream[98] = currFrameMatrix[2][13];
  frameStream[99] = currFrameMatrix[1][13];
}

//////// FRAME MODIFICATION PATTERNS ////////
void scrollFrameHorizontal() {  
  for(byte col = 0; col < FRAME_WIDTH; col++) {
    for(byte row = 0; row < FRAME_HEIGHT; row++) {
      nextFrameMatrix[col][row] = nextFrameMatrix[(col + 1) % FRAME_WIDTH][row];
    }
  }
}

void scrollFrameVertical() {
  for(byte row = 0; row < FRAME_HEIGHT; row++) {
    for(byte col = 0; col < FRAME_WIDTH; col++) {  
      nextFrameMatrix[col][row] = nextFrameMatrix[col][(row + 1) % FRAME_HEIGHT];
    }
  }
}

//////// FRAME GENERATION PATTERNS ////////
void strobePattern() {
  static boolean whiteFrame = true;
  for(byte row = 0; row < FRAME_HEIGHT; row++) {
    for(byte col = 0; col < FRAME_WIDTH; col++) { 
      nextFrameMatrix[col][row] = whiteFrame ? BLACK_RGB : WHITE_RGB;
      whiteFrame = !whiteFrame;
    }
  }
}

void staticPattern() {
  for(byte row = 0; row < FRAME_HEIGHT; row++) {
    for(byte col = 0; col < FRAME_WIDTH; col++) {
      byte randNum = random(256);
      nextFrameMatrix[col][row].r = randNum;
      nextFrameMatrix[col][row].g = randNum;
      nextFrameMatrix[col][row].b = randNum;
    }
  } 
}

void rainbowScrollPatternHorizontal() {
  if(frameCount == 0)
    initNextFrameWithColorRows();
  
  scrollFrameHorizontal();
}

void randomScrollPatternVertical() {
  if(frameCount == 0)
    initNextFrameWithRandomColors();
  
  scrollFrameVertical();
}

void allWhite() {
  for(byte row = 0; row < FRAME_HEIGHT; row++) {
    for(byte col = 0; col < FRAME_WIDTH; col++) { 
      nextFrameMatrix[col][row] = WHITE_RGB;
    }
  }
}



