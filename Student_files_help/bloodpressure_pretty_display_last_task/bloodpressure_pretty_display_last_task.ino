//try to draw an arc on the display
#include <Arduino.h>
#include <Adafruit_TinyUSB.h>  // for Serial
#include <Wire.h>

//For accessing the Flash
#include <SdFat.h>
#include <Adafruit_SPIFlash.h>
#include <math.h>
#include <Adafruit_ImageReader.h>  // Image-reading functions
#include <SPI.h>

//For the display
#include <Adafruit_GFX.h>     // Core graphics library
#include <Adafruit_ST7789.h>  // Hardware-specific library for ST7789

//Library object for the flash configuration
Adafruit_FlashTransport_QSPI flashTransport;
Adafruit_SPIFlash flash(&flashTransport);
FatFileSystem filesys;
Adafruit_ImageReader reader(filesys);

// file system object from SdFat
FatVolume fatfs;
File32 myFile;

int readFile;
String printData;

uint16_t measSample[12000];
int16_t HPmeasSample[12000];
int measPoint = 0;
int measPointMax = 0;

// Pump and Valve
#define PUMP 9
#define VALVE 10

// Buttons & LEDs
#define startButton 12
#define greenLED 11

// Waveshare 17344
#define Display_BL 14
#define Display_RST 15
#define Display_DC 16
#define Display_CS 17

Adafruit_ST7789 display = Adafruit_ST7789(Display_CS, Display_DC, Display_RST);

#define pi 3.14159265359

//Position of the center point of the arc (center point of the full circle)
#define arcPosx 160
#define arcPosy 155
#define arcRadx 70
#define arcRady 70
#define arcThick 10

//Position of the center of the bar, which indicates the heart-beats
#define barThreshold 300
#define barPosy 180  //eigentlich 200
#define barWidth 34
#define barHeight 20

int circSeg = 64;            //number of segments of the drawn half-circle
int intermediateSegment[2];  //intermediate segment of pressure value
float circSlope;
float circOffset;

int16_t xCursor;
int16_t yCursor;

int red = 0xF800;
int green = 0x07E0;
int blue = 0x001F;
int black = 0x0000;
int white = 0xFFFF;
int yellow = 0xFFE0;
int magenta = 0xF81F;
int cyan = 0x07FF;

//color of heart-beat strength
int colorGradient[9];
int16_t heartStrength;
int drawBeat = 0;
int drawTime = 0;

unsigned long startTimer;
unsigned long endTimer;

uint16_t maxPressure = 21000;
uint16_t minPressure = 4000;
uint16_t currentPressure;
uint16_t printPressure;
uint16_t printPressure1;
uint16_t printPressure2;
uint16_t printPressure3;
char charPressure;
int16_t HPPressure;
int maxTime;

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(9600);

  circSlope = (circSeg) / ((float)(maxPressure - minPressure));
  circOffset = (circSeg * minPressure) / ((float)(maxPressure - minPressure));
  colorGradient[0] = green;

  pinMode(startButton, INPUT_PULLUP);

  pinMode(greenLED, OUTPUT);
  pinMode(PUMP, OUTPUT);
  pinMode(VALVE, OUTPUT);
  pinMode(Display_BL, OUTPUT);

  digitalWrite(greenLED, LOW);
  digitalWrite(PUMP, LOW);
  digitalWrite(VALVE, LOW);
  digitalWrite(Display_BL, HIGH);

  display.init(240, 320);
  display.setRotation(3);
  display.setTextSize(3);
  display.fillScreen(ST77XX_BLACK);

  if (!flash.begin()) {
    Serial.println(F("flash begin() failed"));
  }
  if (!filesys.begin(&flash)) {
    Serial.println(F("filesys begin() failed"));
  }

  if (!fatfs.begin(&flash)) {
    Serial.println("Error: filesystem is not existed. Please try SdFat_format example to make one.");
    while (1) {
      yield();
      delay(1);
    }
  }
  Serial.println("initialization done.");

  myFile = fatfs.open("pressureMeasurement.txt", FILE_READ);
  if (myFile) {
    Serial.print("Read pressureMeasurement.txt...");
    // read from the file until there's nothing else in it:
    while (myFile.available()) {
      readFile = myFile.read();
      if ((char)readFile == ',') {
        measSample[measPoint] = printData.toInt();
        measPoint += 1;
        printData = "";
      } else {
        printData += (char)readFile;
      }
    }
    // close the file:
    myFile.close();
  } else {
    // if the file didn't open, print an error:
    Serial.println("error opening test.txt");
  }
  Serial.println("done.");
  measPointMax = measPoint;
  measPoint = 0;

  myFile = fatfs.open("HPpressureMeasurement.txt", FILE_READ);
  if (myFile) {
    Serial.print("Read HPpressureMeasurement.txt...");
    // read from the file until there's nothing else in it:
    while (myFile.available()) {
      readFile = myFile.read();
      if ((char)readFile == ',') {
        HPmeasSample[measPoint] = printData.toInt();
        measPoint += 1;
        printData = "";
      } else {
        printData += (char)readFile;
      }
    }
    // close the file:
    myFile.close();
  } else {
    // if the file didn't open, print an error:
    Serial.println("error opening test.txt");
  }
  Serial.println("done.");
  measPoint = 0;

  //Color Gradient of the heart beat strength
  colorGradient[0] = green;
  for (int i = 1; i < 9; i++) {

    colorGradient[i] = colorGradient[i - 1] + 0x00FC;
  }

  //draw a saved bitmap to the display
  reader.drawBMP("heart.bmp", display, 0, 0);
  display.setCursor(5, 5);
  display.setTextSize(3);
  display.setTextColor(ST77XX_BLACK);
  display.print("uC-Projekt");
  display.setCursor(5, 27);
  display.print("2023");
}

void loop() {

  Serial.println("Press the green button to start!");
  while (digitalRead(startButton) == HIGH) { //toggle the LED of the start button

    startTimer = millis();
    endTimer = startTimer;

    while (endTimer - startTimer < 1000 && digitalRead(startButton) == HIGH) {
      endTimer = millis();
    }

    digitalWrite(greenLED, !digitalRead(greenLED));
  }

  digitalWrite(greenLED, LOW);
  reader.drawBMP("matrix.bmp", display, 0, 0);

  //Draw an arc for the pressure meter on the display
  for (int i = 0; i < circSeg; i++) {

    fillArc(arcPosy, arcPosx, arcRady + arcThick, arcRadx + arcThick, circSeg, i, white);
  }

  for (int i = 0; i < circSeg; i++) {

    fillArc(arcPosy, arcPosx, arcRady, arcRadx, circSeg, i, black);
  }

  display.fillRect(arcPosx - (arcRadx + arcThick), arcPosy, 2 * (arcRadx + arcThick), arcThick, white);

  //Fill in the bars for the beart beat strength
  for (int i = 0; i < 8; i++) {

    display.fillRect(barWidth * i + barWidth / 2, barPosy, barWidth - 1, barHeight, black);
  }

  display.setCursor(7, 40);
  display.setTextColor(ST77XX_WHITE);
  display.setTextSize(3);
  display.print("Pressure:");
  xCursor = display.getCursorX();
  yCursor = display.getCursorY();

  //draw placeholder for the numbers
  drawNumber(0, xCursor, yCursor, 3, green, black);  //4*7 grid times 3
  drawNumber(0, xCursor + 3 * 5, yCursor, 3, green, black);
  drawNumber(0, xCursor + 2 * 3 * 5, yCursor, 3, green, black);

  display.setCursor(xCursor + 3 * 3 * 5, yCursor);
  display.setTextColor(ST77XX_WHITE);
  display.print("mmHg");

  display.setTextSize(2);
  display.setCursor(barWidth / 2, barPosy - 15);
  display.print("Heart-Beat");

  display.setTextSize(2);
  display.setTextColor(ST77XX_WHITE, ST77XX_BLACK);

  for (int i = 0; i < measPointMax; i++) {

    startTimer = millis();
    endTimer = startTimer;
    currentPressure = measSample[i];
    HPPressure = HPmeasSample[i];

    if (currentPressure < maxPressure && currentPressure > minPressure) {

      intermediateSegment[1] = intermediateSegment[0];
      intermediateSegment[0] = circSeg - round(circSlope * currentPressure - circOffset);

      if (intermediateSegment[0] >= intermediateSegment[1]) {

        indicator(arcPosy, arcPosx, arcRady, arcRadx, circSeg, intermediateSegment[0], 5, green);

      } else {

        indicator(arcPosy, arcPosx, arcRady, arcRadx, circSeg, intermediateSegment[1], 5, black);
      }

      //Heart beat strength with threshold
      if (HPPressure > barThreshold) {

        heartStrength = HPPressure - barThreshold;
        drawTime = i;

        for (int k = drawBeat; k < 8; k++) {

          if (k * 100 <= heartStrength) {

            display.fillRect(barWidth * k + barWidth / 2, barPosy, barWidth - 1, barHeight, colorGradient[8 - k]);
            drawBeat = k;
          }
        }
      } else if ((i - drawTime) % 10 == 0) {  //remove blocks every 100ms

        display.fillRect(barWidth * drawBeat + barWidth / 2, barPosy, barWidth - 1, barHeight, black);

        if (drawBeat > 0) {

          drawBeat -= 1;
        }
      }

      //Convert decimal places to integers
      printPressure = round(((float)currentPressure) / 100);
      printPressure1 = printPressure / 100;
      printPressure2 = printPressure / 10 - 10 * printPressure1;
      printPressure3 = printPressure - 10 * printPressure2 - 100 * printPressure1;

      if (i % 50 == 0) {

        drawNumber(printPressure1, xCursor, yCursor, 3, green, black);  //4*7 grid times 3
        drawNumber(printPressure2, xCursor + 3 * 5, yCursor, 3, green, black);
        drawNumber(printPressure3, xCursor + 2 * 3 * 5, yCursor, 3, green, black);
      }

      while ((endTimer - startTimer) < 10) {  //update values every 10m
        endTimer = millis();
      }

      //Save the maximum time to see if sampling time is correctly set
      endTimer = millis();
      if ((endTimer - startTimer) > maxTime) {

        maxTime = endTimer - startTimer;
      }

      Serial.print("Time for display: ");
      Serial.println(endTimer - startTimer);
      Serial.print("Segment: ");
      Serial.println(intermediateSegment[0]);
      Serial.print("Maximum sample time: ");
      Serial.println(maxTime);
      Serial.print("Printed value: ");
      Serial.println(printPressure);
    }
  }
}

/* 
x,y: coordinates of the center of the arc
seg_count; number of segments of the circle (power of 2)
seg: segment that is filled (0 - (seg_count-1))
rx = x axis radius 
yx = y axis radius
color = 16bit color value
if rx=yx -> circle of arc
 */

void fillArc(int x, int y, int rx, int ry, int seg_count, int seg, unsigned int colour) {

  // Calculate first pair of coordinates for segment start
  float sx1 = cos(pi * seg / seg_count + pi / 2);
  float sy1 = sin(pi * seg / seg_count + pi / 2);
  uint16_t x1 = sx1 * rx + x;
  uint16_t y1 = sy1 * ry + y;

  float sx2 = cos(pi * (seg + 1) / seg_count + pi / 2);
  float sy2 = sin(pi * (seg + 1) / seg_count + pi / 2);
  uint16_t x2 = sx2 * rx + x;
  uint16_t y2 = sy2 * ry + y;

  display.fillTriangle(y, x, y1, x1, y2, x2, colour);
}

//size: size of one unit square -> unit square must be a divider of max(rx,ry)
void indicator(int x, int y, int rx, int ry, int seg_count, int seg, int size, unsigned int colour) {

  float sx1 = cos(pi * seg / seg_count + pi / 2);
  float sy1 = sin(pi * seg / seg_count + pi / 2);
  uint16_t x1 = sx1 * rx + x;
  uint16_t y1 = sy1 * ry + y;
  int squareNumber;

  if (rx > ry) {

    squareNumber = rx / size;
  } else {

    squareNumber = ry / size;
  }


  for (int l = 0; l <= squareNumber; l++) {

    display.fillRect(sy1 * ry * l / squareNumber + y - size / 2 - 1, sx1 * rx * l / squareNumber + x - size / 2 - 1, size + 2, size + 2, colour);
  }
}

//much quicker printing of numbers compared to display.print() and with constant delay!
void drawNumber(int number, int16_t posX, int16_t posY, int size, unsigned int colour, unsigned int background) {

  if (number == 0) {

    display.fillRect(posX, posY, 4 * size, 7 * size, background);
    display.fillRect(posX + size, posY, 2 * size, size, colour);
    display.fillRect(posX + size, posY + 6 * size, 2 * size, size, colour);
    display.fillRect(posX, posY + size, size, 5 * size, colour);
    display.fillRect(posX + 3 * size, posY + size, size, 5 * size, colour);
  } else if (number == 1) {

    display.fillRect(posX, posY, 4 * size, 7 * size, background);
    display.fillRect(posX + 3 * size, posY, size, 7 * size, colour);
    display.fillRect(posX + 2 * size, posY, size, size, colour);
    display.fillRect(posX + size, posY + size, size, size, colour);
    display.fillRect(posX, posY + 2 * size, size, size, colour);
  } else if (number == 2) {

    display.fillRect(posX, posY, 4 * size, 7 * size, background);
    display.fillRect(posX, posY, 4 * size, size, colour);
    display.fillRect(posX, posY + 3 * size, 4 * size, size, colour);
    display.fillRect(posX, posY + 6 * size, 4 * size, size, colour);
    display.fillRect(posX + 3 * size, posY + size, size, 2 * size, colour);
    display.fillRect(posX, posY + 4 * size, size, 2 * size, colour);
  } else if (number == 3) {

    display.fillRect(posX, posY, 4 * size, 7 * size, background);
    display.fillRect(posX, posY, 4 * size, size, colour);
    display.fillRect(posX, posY + 3 * size, 4 * size, size, colour);
    display.fillRect(posX, posY + 6 * size, 4 * size, size, colour);
    display.fillRect(posX + 3 * size, posY + size, size, 2 * size, colour);
    display.fillRect(posX + 3 * size, posY + 4 * size, size, 2 * size, colour);
  } else if (number == 4) {

    display.fillRect(posX, posY, 4 * size, 7 * size, background);
    display.fillRect(posX, posY, size, 4 * size, colour);
    display.fillRect(posX + 3 * size, posY, size, 7 * size, colour);
    display.fillRect(posX + size, posY + 3 * size, 2 * size, size, colour);
  } else if (number == 5) {

    display.fillRect(posX, posY, 4 * size, 7 * size, background);
    display.fillRect(posX, posY, 4 * size, size, colour);
    display.fillRect(posX, posY + 3 * size, 4 * size, size, colour);
    display.fillRect(posX, posY + 6 * size, 4 * size, size, colour);
    display.fillRect(posX, posY + size, size, 2 * size, colour);
    display.fillRect(posX + 3 * size, posY + 4 * size, size, 2 * size, colour);
  } else if (number == 6) {

    display.fillRect(posX, posY, 4 * size, 7 * size, background);
    display.fillRect(posX, posY, size, 7 * size, colour);
    display.fillRect(posX + 3 * size, posY + 3 * size, size, 4 * size, colour);
    display.fillRect(posX + size, posY + 3 * size, 2 * size, size, colour);
    display.fillRect(posX + size, posY + 6 * size, 2 * size, size, colour);
  } else if (number == 7) {

    display.fillRect(posX, posY, 4 * size, 7 * size, background);
    display.fillRect(posX, posY, 4 * size, size, colour);
    display.fillRect(posX + 3 * size, posY + size, size, 2 * size, colour);
    display.fillRect(posX + 2 * size, posY + 3 * size, size, 2 * size, colour);
    display.fillRect(posX + 1 * size, posY + 5 * size, size, 2 * size, colour);
  } else if (number == 8) {

    display.fillRect(posX, posY, 4 * size, 7 * size, background);
    display.fillRect(posX, posY, 4 * size, size, colour);
    display.fillRect(posX + size, posY + 3 * size, 2 * size, size, colour);
    display.fillRect(posX, posY + 6 * size, 4 * size, size, colour);
    display.fillRect(posX, posY + size, size, 2 * size, colour);
    display.fillRect(posX + 3 * size, posY + size, size, 2 * size, colour);
    display.fillRect(posX, posY + 4 * size, size, 2 * size, colour);
    display.fillRect(posX + 3 * size, posY + 4 * size, size, 2 * size, colour);
  } else if (number == 9) {

    display.fillRect(posX, posY, 4 * size, 7 * size, background);
    display.fillRect(posX, posY, size, 4 * size, colour);
    display.fillRect(posX + 3 * size, posY, size, 7 * size, colour);
    display.fillRect(posX + size, posY, 2 * size, size, colour);
    display.fillRect(posX + size, posY + 3 * size, 2 * size, size, colour);
  } else {

    display.fillRect(posX, posY, 4 * size, 7 * size, colour);
  }
}
