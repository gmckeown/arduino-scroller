#define FASTLED_ESP8266_NODEMCU_PIN_ORDER

#include <FastLED.h>
#include <string>
#include "Resources/8x8_font.h"
//#include "Resources/6x6_font.h"

using namespace std;

#define DEBUG false

// LED Matrix Definition
#define DATA_PIN 4      // ESP2866 D4 = PIN 4
#define NUM_ROWS 16
#define NUM_COLUMNS 32 // If this is odd, you might flip the text upside-down due to the 'snaking' LED handling
#define START_ROW 0
#define NUM_LEDS (NUM_ROWS * NUM_COLUMNS)
#define BRIGHTNESS 20
#define ORIENTATION 0   // 0 = pixel 0 at bottom-right, 1 = pixel 0 at top-right, 2 = pixel 0 at top-left, 3 = pixel 0 at bottom-left

// Font Definition (note: font data must be 8-bits wide even if the glyphs are narrower than this)
#define FONT_WIDTH 8
#define FONT_HEIGHT 8
#define LSB_FONT false // Set to true if the font data has the left-most display pixel in the least-significant (right-most) bit

// Scroller Parameters
#define WAVY true          // 'true' applies the wave distortion to the scroll, 'false' means a simple straight-line scroller
#define DISTORT_FACTOR 3    // Additional wave step per display column -- larger means more compressed waveform, 0 for a 'bouncing straight-line' scroller
#define WAVE_START_INDEX 0  // Position to start in wave function (0-255)
#define WAVE_SPEED_FACTOR 8 // Increment for the wave, larger means faster bounces
#define SCROLL_STEP 1       // Positive scrolls 'normally', negative scrolls 'backwards'
#define LOOP_DELAY 10       // Milliseconds to wait between full matrix updates
#define INITIAL_HUE 0       // Hue to set scroller to on start
#define HUE_INCREMENT 1     // 0 for single colour, higher numbers cycle colours faster

// Define the array of leds
CRGB leds[NUM_LEDS];

static const string scrollyMessage = "      "
                                     "   DEMO OF BASIC SCROLLY TEXT MESSAGES"
                                     "   THIS RUNS COMFORTABLY ON A NODE MCU AND CAN SUPPORT 50+ FPS ON A 512-PIXEL LED MATRIX DISPLAY."
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

void logSerial(char *text)
{
  if (DEBUG == true)
  {
    Serial.print(text);
  }
}

void logSerial(int value)
{
  if (DEBUG == true)
  {
    Serial.print(value);
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Setup method begins.");
  pinMode(DATA_PIN, OUTPUT);
  LEDS.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);
  LEDS.setBrightness(BRIGHTNESS);

  float maxDisplayedCharacters = (float)NUM_COLUMNS / (float)FONT_WIDTH;
  bufferCharacterWidth = (int)(maxDisplayedCharacters + 0.5) + 1;
  matrixBufferWidth = bufferCharacterWidth * FONT_WIDTH;

  // Bit of a hack to cope with some situations where matrix buffer isn't wide enough
  // Seems to occur when font width isn't a factor of matrix width -- needs some more investigation!

  if (matrixBufferWidth < NUM_COLUMNS + FONT_WIDTH - 1)
  {
    bufferCharacterWidth++;
    matrixBufferWidth = bufferCharacterWidth * FONT_WIDTH;
  }

  scrollLengthInPixels = scrollyMessage.length() * FONT_WIDTH;

  Serial.println(bufferCharacterWidth);
  Serial.println(matrixBufferWidth);
  Serial.println("Setup method complete.");
}

// This function translates from x/y coordinates to an index in the LED array.
// It assumes 'snaking' pixel arrangement

int getPixelStripPosition(int x, int y)
{
  int index;
  if (x < 0 || x > NUM_COLUMNS || y < 0 || y > NUM_ROWS)
  {
    index = -1;
  } else {

    if (ORIENTATION == 0) {         // Pixel 0 at bottom-right
      if (x % 2 == 0)
      { // Even rows
        index = (x + 1) * NUM_ROWS - y - 1;
      }
      else
      {
        index = x * NUM_ROWS + y; // Odd rows
      }
      index = NUM_LEDS - 1 - index;
    } else if (ORIENTATION == 1) { // Pixel 0 at top-right
      if (y % 2 == 0)
      { // Even rows
        index = ((y + 1) * NUM_COLUMNS) - x - 1;
      }
      else
      {
        index = y * NUM_COLUMNS + x; // Odd rows
      }
    } else if (ORIENTATION == 2) {         // Pixel 0 at top-left
      if (x % 2 == 0)
      { // Even columns
        index = x * NUM_ROWS + y;
      }
      else
      { // Odd columns
        index = (x + 1) * NUM_ROWS - y - 1;
      }
    } else if (ORIENTATION == 3) { // Pixel 0 at bottom-left
      if (y % 2 == 0)
      { // Even rows
        index = y * NUM_COLUMNS + x;
      }
      else
      {
        index = (y + 1) * NUM_COLUMNS - x - 1;  // Odd rows
      }
      index = NUM_LEDS - 1 - index;

    }
    if (index < 0 || index >= NUM_LEDS)
    {
      index = -1;
    }
  }
  return index;
}

// Calculate a Y position offset along a cubic curve to provide the wave effect

int calcYPos(uint8_t index)
{
  //  uint8_t ypos = map8(cubicwave8(index), 0, NUM_ROWS - FONT_HEIGHT);
  uint8_t ypos = map8(sin8(index), 0, NUM_ROWS - FONT_HEIGHT);
  return (int)ypos;
}

// Maps the character buffer into the LED array, applying Y-axis distortion as needed

void drawMatrix(CRGB (*matrix)[FONT_HEIGHT], int offsetX, int offsetY, int bitshift)
{
  for (int x = 0; x < NUM_COLUMNS; x++)
  {
    int yDrift;
    if (WAVY == true)
    {
      yDrift = calcYPos(yPosIndex + (x * DISTORT_FACTOR * SCROLL_STEP));
    }
    else
    {
      yDrift = 0;
    }
    for (int y = 0; y < FONT_HEIGHT; y++)
    {
      int stripPos = getPixelStripPosition(x + offsetX, y + offsetY + yDrift);
      if (stripPos >= 0 && stripPos < NUM_LEDS)
      {
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

void plotMatrixChar(CRGB (*matrix)[FONT_HEIGHT], int x, char character, int width, int height)
{
  int y = 0;
  if (width > 0 && height > 0)
  {
    int charIndex = (int)character - 32;
    int xBitsToProcess = width;

    for (int i = 0; i < height; i++)
    {
      byte fontLine = FontData[charIndex][i];
      for (int bitCount = 0; bitCount < xBitsToProcess; bitCount++)
      {
        CRGB pixelColour = CRGB(0, 0, 0);

        if (LSB_FONT == true)
        {
          if (fontLine & 0b00000001)
          {
            pixelColour = CHSV(hue, 255, 255); // When using reverse-mapped binary character data (left-most pixel is LSB)
          }
          fontLine = fontLine >> 1;
        }
        else
        {
          if (fontLine & 0b10000000)
          { // When using direct-mapped binary character data (left-most pixel is HSB)
            pixelColour = CHSV(hue, 255, 255);
          }
          fontLine = fontLine << 1;
        }
        int xpos = x + bitCount;
        int ypos = y + i;
        if (xpos < 0 || xpos > matrixBufferWidth || ypos < 0 || ypos > FONT_HEIGHT)
        {
          logSerial("********* Overflowed bounds of Matrix Array!\n");
        }
        else
        {
          matrix[xpos][ypos] = pixelColour;
        }
      }
    }
  }
}

// Update first-column index into the wave so that the wave 'bounces'

void updateWavePosition()
{
  yPosIndex = (yPosIndex + (WAVE_SPEED_FACTOR * SCROLL_STEP));
}

// Fill the strip line-by-line to test orientation and pixel operation

void testMatrix(CRGB colour) {
  memset(leds, 0, NUM_LEDS * 3);
  FastLED.show();
  for (int y = 0; y < NUM_ROWS; y++) {
    for (int x = 0; x < NUM_COLUMNS; x += 2) {
      int pixelPos = getPixelStripPosition(x, y);
      leds[pixelPos] = colour;
      pixelPos = getPixelStripPosition(x + 1, y);
      leds[pixelPos] = colour;
      FastLED.show();
//      Serial.print("x = ");
//      Serial.print(x);
//      Serial.print(", y = ");
//      Serial.print(y);
//      Serial.print(", pos = ");
//      Serial.println(pixelPos);
      delay(1);
//      delay(500);
    }
  }
}

// Main loop

void loop()
{
  Serial.println("Short pause...");
  delay(500); // Slow down execution start (helps avoid flooding serial monitor in case of exceptions)
  Serial.println("Main loop begins.");
//  for (int i = 0; i < NUM_LEDS; i++) {
//    leds[i] = CRGB::Blue;
//    delay(20);
//    FastLED.show();
//  }
  testMatrix(CRGB::Red);
  testMatrix(CRGB::Green);
  testMatrix(CRGB::Blue);
  delay(1000);
  CRGB matrixBuffer[matrixBufferWidth][FONT_HEIGHT];
  currentScrollPosition = 0;
  int messageLength = scrollyMessage.length();
  int lastRenderedCharPos = -1;

  while (1)
  {
    currentScrollPosition += SCROLL_STEP;
    if (currentScrollPosition >= (scrollLengthInPixels - NUM_COLUMNS))
    {
      currentScrollPosition = 0;
    }
    else if (currentScrollPosition < 0)
    {
      currentScrollPosition = scrollLengthInPixels - NUM_COLUMNS;
    }

    int characterPos = currentScrollPosition / FONT_WIDTH;
    int bitshift = currentScrollPosition % FONT_WIDTH;
    if (characterPos != lastRenderedCharPos)
    { // Only render the matrix buffer when character position changes
      lastRenderedCharPos = characterPos;
      for (int x = 0; x < bufferCharacterWidth; x++)
      {
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
  Serial.println("Main loop ends.");

}
