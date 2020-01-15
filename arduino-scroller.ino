#include <FastLED.h>
#include <string>
#include "Resources/8x8_font.h"
//#include "Resources/6x6_font.h"

using namespace std;

#define DEBUG false

// LED Matrix Definition
#define DATA_PIN D2
#define NUM_ROWS 8
#define NUM_COLUMNS 32        // If this is odd, you might flip the text upside-down due to the 'snaking' LED handling
#define START_ROW 0
#define NUM_LEDS 8 * 32
#define BRIGHTNESS 20

// Font Definition (note: font data must be 8-bits wide even if the glyphs are narrower than this)
#define FONT_WIDTH 8
#define FONT_HEIGHT 8
#define LSB_FONT false        // Set to true if the font data has the left-most display pixel in the least-significant (right-most) bit

// Scroller Parameters
#define WAVY false             // 'true' applies the wave distortion to the scroll, 'false' means a simple straight-line scroller
#define DISTORT_FACTOR 3      // Additional wave step per display column -- larger means more compressed waveform, 0 for a 'bouncing straight-line' scroller
#define WAVE_START_INDEX 0    // Position to start in wave function (0-255)
#define WAVE_SPEED_FACTOR 7   // Increment for the wave, larger means faster bounces
#define SCROLL_STEP 1         // Positive scrolls 'normally', negative scrolls 'backwards'
#define LOOP_DELAY 20         // Milliseconds to wait between full matrix updates
#define INITIAL_HUE 0         // Hue to set scroller to on start
#define HUE_INCREMENT 1       // 0 for single colour, higher numbers cycle colours faster

// Define the array of leds
CRGB leds[NUM_ROWS * NUM_COLUMNS];

static const string scrollyMessage = "      "
                                     "   DEMO OF BASIC SCROLLY TEXT MESSAGES"
                                     "   THIS RUNS COMFORTABLY ON A NODE MCU AND CAN SUPPORT 50+ FPS ON A 256-PIXEL LED MATRIX DISPLAY."
                                     "   8x8 AND 6x6 FONTS ARE INCLUDED, with lower-case text, as well as various punctuation characters!"
                                     "   $ % ^ & * ( ) - = _ + [ ] { } ' @ # ~ / ? | \\"
                                     "   No non-English support... sorry!"
                                     "          ";

static uint8_t hue = INITIAL_HUE;
uint8_t yPosIndex = WAVE_START_INDEX;
int matrixBufferWidth;
int bufferCharacterWidth;
int scrollLengthInPixels;
int currentScrollPosition = 0;

void logSerial(char* text) {
  if (DEBUG == true) {
    Serial.print(text);
  }
}

void logSerial(int value) {
  if (DEBUG == true) {
    Serial.print(value);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("resetting");
  LEDS.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);
  LEDS.setBrightness(BRIGHTNESS);

  float maxDisplayedCharacters = (float)NUM_COLUMNS / (float)FONT_WIDTH;
  bufferCharacterWidth = (int)(maxDisplayedCharacters + 0.5) + 1;
  matrixBufferWidth = bufferCharacterWidth * FONT_WIDTH;

  // Bit of a hack to cope with some situations where matrix buffer isn't wide enough
  // Seems to occur when font width isn't a factor of matrix width -- needs some more investigation!

  if (matrixBufferWidth < NUM_COLUMNS + FONT_WIDTH - 1) {
    bufferCharacterWidth++;
    matrixBufferWidth = bufferCharacterWidth * FONT_WIDTH;
  }

  scrollLengthInPixels = scrollyMessage.length() * FONT_WIDTH;

  Serial.println(bufferCharacterWidth);
  Serial.println(matrixBufferWidth);
}

// This function translates from x/y coordinates to an index in the LED array.
// It assumes 'snaking' pixel arrangement in the up-down direction

int getPixelStripPosition(int x, int y) {
  int index;
  if (x < 0 || x > NUM_COLUMNS || y < 0 || y > NUM_ROWS) {
    index = -1;
  } else {
    if (x % 2 == 0) {                         // Even rows
      index = (x + 1) * NUM_ROWS - y - 1;
    } else {
      index = x * NUM_ROWS + y;               // Odd rows
    }
    index = NUM_LEDS - 1 - index;
  }
  if (index < 0 || index >= NUM_LEDS) {
    index = -1;
  }

  return index;
}


// Calculate a Y position offset along a cubic curve to provide the wave effect

int calcYPos(uint8_t index) {
  uint8_t ypos = map8(cubicwave8(index), 0, NUM_ROWS - FONT_HEIGHT);
  return (int)ypos;
}


// Maps the character buffer into the LED array, applying Y-axis distortion as needed

void drawMatrix(CRGB (*matrix)[FONT_HEIGHT], int offsetX, int offsetY, int bitshift) {
  for (int x = 0; x < NUM_COLUMNS; x++) {
    int yDrift;
    if (WAVY == true) {
      yDrift = calcYPos(yPosIndex + (x * DISTORT_FACTOR));
    } else {
      yDrift = 0;
    }
    for (int y = 0; y < FONT_HEIGHT; y++) {
      int stripPos = getPixelStripPosition (x + offsetX, y + offsetY + yDrift);
      if (stripPos >= 0 && stripPos < NUM_LEDS) {
        int xpos = x + bitshift;
        leds[stripPos] = matrix[x + bitshift][y];
      }
    }
  }
}


// Draw font glyphs into a rectangular matrix buffer.
// This simplifies the glyph-rendering process, as distortions can be applied late.
// It does, however, require a little extra memory and means that we effectively
//  draw the character data twice (once into this buffer, then again into the LED array).

void plotMatrixChar(CRGB (*matrix)[FONT_HEIGHT], int x, char character, int width, int height) {
  int y = 0;
  if (width > 0 && height > 0) {
    int charIndex = (int)character - 32;
    int xBitsToProcess = width;

    for (int i = 0; i < height; i ++) {
      byte fontLine = FontData[charIndex][i];
      for (int bitCount = 0; bitCount < xBitsToProcess; bitCount++) {
        CRGB pixelColour = CRGB(0, 0, 0);

        if (LSB_FONT == true) {
          if (fontLine & 0b00000001) {
            pixelColour = CHSV(hue, 255, 255);        // When using reverse-mapped binary character data (left-most pixel is LSB)
          }
          fontLine = fontLine >> 1;
        } else {
          if (fontLine & 0b10000000) {                // When using direct-mapped binary character data (left-most pixel is HSB)
            pixelColour = CHSV(hue, 255, 255);
          }
          fontLine = fontLine << 1;
        }
        int xpos = x + bitCount;
        int ypos = y + i;
        if (xpos < 0 || xpos > matrixBufferWidth || ypos < 0 || ypos > FONT_HEIGHT) {
          logSerial("********* Overflowed bounds of Matrix Array!\n");
        } else {
          matrix[xpos][ypos] = pixelColour;
        }
      }
    }

  }
}


// Update first-column index into the wave so that the wave 'bounces'

void updateWavePosition() {
  yPosIndex = (yPosIndex + WAVE_SPEED_FACTOR);
}

void loop() {
  delay(500);       // Slow down execution start (helps avoid flooding serial monitor in case of exceptions)
  logSerial("Main loop\n");
  CRGB spacer[16 * 3];
  CRGB matrixBuffer[matrixBufferWidth][FONT_HEIGHT];
  currentScrollPosition = 0;
  int messageLength = scrollyMessage.length();
  int lastRenderedCharPos = -1;

  while (1) {
    currentScrollPosition += SCROLL_STEP;
    if (currentScrollPosition >= (scrollLengthInPixels - NUM_COLUMNS)) {
      currentScrollPosition = 0;
    } else if (currentScrollPosition < 0) {
      currentScrollPosition = scrollLengthInPixels - NUM_COLUMNS;
    }

    int characterPos = currentScrollPosition / FONT_WIDTH;
    int bitshift = currentScrollPosition % FONT_WIDTH;
    if (characterPos != lastRenderedCharPos) {          // Only render the matrix buffer when character position changes
      lastRenderedCharPos = characterPos;
      for (int x = 0; x < bufferCharacterWidth; x++) {
        char myChar = scrollyMessage[characterPos + x];
        plotMatrixChar(matrixBuffer, x * FONT_WIDTH, myChar, FONT_WIDTH, FONT_HEIGHT);
      }

    }

    updateWavePosition();
    drawMatrix(matrixBuffer, 0, START_ROW, bitshift);
    FastLED.show();
    memset(leds, 0, NUM_LEDS * 3);
    hue += HUE_INCREMENT;
    delay(LOOP_DELAY);
  }
}
