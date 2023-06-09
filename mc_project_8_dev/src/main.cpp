#include <Arduino.h>
#include <Adafruit_TinyUSB.h>  // for Serial
#include <Wire.h>
#include <Adafruit_ST7789.h>  // Hardware-specific library for ST7789
#include "Adafruit_MPRLS.h"
#include <SPI.h>
#include <math.h>
#include "SdFat.h"
#include "Adafruit_SPIFlash.h"

// ------------------ Global Defines ------------------ //

#define DEBUG

#define PUMP A1    
#define VALVE A0  

#define startButton 7     
#define interruptButton 9  

#define LED_GREEN 10
#define LED_RED 5

// Waveshare 17344
#define TFT_CS A5   //chip select 
#define TFT_DC A2    // data/command 
#define TFT_RST A4   // reset 
#define TFT_BL A3    // backlight 

// Display
#define SCREEN_HEIGHT 240
#define SCREEN_WIDTH 320

#define pressureIncrease 280  //put this value to 240+40 as overshoot compensation if you want to measure the blood pressure
#define pressureThreshold 40  //lower threshold, when the cuff is deflated, put this value to 40 for blood pressure measurement
#define settleTime 500        //settle time in ms, when pump/valve is turned on/off
#define maxTime 120000        //after 2 minutes stop the measurement
#define measPeriod 10         //10ms of sampling time
#define NOS 12000             //number of samples, maximum 2 minutes every 10ms -> 12000 entries
#define NOP 360               //number of peaks in the buffer 

#define HPnominator_5Hz 0.864244751836367      //filter coefficient of the nominator of the highpass filter, with f_3dB = 5Hz
#define HPdenominator_5Hz 0.728489503672734    //filter coefficient of the denominator of the highpass filter, with f_3dB = 5Hz
#define HPnominator_0_5Hz 0.984534960996653    //filter coefficient of the nominator of the highpass filter, with f_3dB = 0.5Hz
#define HPdenominator_0_5Hz 0.969069921993306  //filter coefficient of the denominator of the highpass filter, with f_3dB = 0.5Hz

// ------------------ Global Variables ------------------ //

//Library object for the flash configuration
Adafruit_FlashTransport_QSPI flashTransport;
Adafruit_SPIFlash flash(&flashTransport);

Adafruit_ST7789 display = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
int16_t xCursor = 70;
int16_t yCursor = 70;

// file system object from SdFat
FatVolume fatfs;  //file system
File32 myFile;    //file format for the SD memory
int readFile;     //read the file position, ATTENTION: Output is decoded as integer in ASCII-format!

//Library for the pressure sensor
Adafruit_MPRLS pressureSensor = Adafruit_MPRLS();

float currentPressure[2];             //intermediate pressure values, with buffer of last value
float startPressure = 0;              //intermediate pressure values
volatile bool flagInterrupt = false;  //emergency flag from the interrupt

unsigned long startTimer = 0;     //startpoint of the timer
unsigned long endTimer = 0;       //endpoint of the timer
unsigned long startMaxTimer = 0;  //start the timer at the beginning of the whole measurement process, to have a maximum time to stop

float HPbuffer_5Hz[2];        //buffer of last two highpass filter values, with f_3dB = 5Hz
float HPbuffer_0_5Hz[2];      //buffer of last two highpass filter values, with f_3dB = 0.5Hz
uint16_t measSample[NOS];     //array for the measured samples, maximum 2 minutes every 10ms -> 12000 entries
int16_t HPmeasSample[NOS];    //array of the highpass filtered samples

int writeCount = 0;  //used for print counter every 500ms and position in array to write to

float heartRate = 0;  //final heart rate


// ------------------ Function Declaration ------------------ //

void interruptFunction();
void HPfilter(float* pressure, float* HP_5Hz, float* HP_0_5Hz);
void findHeartbeat();
void print_array(int16_t *array, int16_t size);
void printToScreen();

// ------------------ Main Programm ------------------ //

void setup() {
  pinMode(startButton, INPUT_PULLUP);
  pinMode(interruptButton, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(interruptButton), interruptFunction, CHANGE);

  pinMode(LED_GREEN, OUTPUT);
  digitalWrite(LED_GREEN, HIGH);

  pinMode(PUMP, OUTPUT);
  digitalWrite(PUMP, LOW);

  pinMode(VALVE, OUTPUT);
  digitalWrite(VALVE, LOW);
  //start serial communication and set baud rate
  Serial.begin(9600);

  Serial.println("MPRLS Simple Test");
  if (!pressureSensor.begin()) {
    Serial.println("Failed to communicate with MPRLS sensor, check wiring?");
    while (1) {
      delay(10);
    }
  }
  Serial.println("Found MPRLS sensor");

  Serial.println("Initializing Filesystem on external flash...");
  // Init external flash
  flash.begin();
  // Open file system on the flash
  if (!fatfs.begin(&flash)) {
    Serial.println("Error: filesystem is not existed. Please try SdFat_format example to make one.");
    while (1) {
      delay(10);
    }
  }
  Serial.println("initialization done.");

  pinMode(TFT_BL, OUTPUT);
  pinMode(TFT_CS, OUTPUT);
  pinMode(TFT_RST, OUTPUT);
  //initializing of display
  display.init(SCREEN_HEIGHT, SCREEN_WIDTH);
  display.setRotation(3);
  display.fillScreen(ST77XX_BLACK);
  display.setCursor(xCursor, yCursor);
  display.setTextWrap(true);
  display.setTextSize(3);

  //display.print("Hello World!");
}

void loop() {

  //refresh memory of sample-buffer
  memset(currentPressure, 0, sizeof(currentPressure));
  memset(measSample, 0, sizeof(measSample));
  memset(HPbuffer_5Hz, 0, sizeof(HPbuffer_5Hz));
  memset(HPbuffer_0_5Hz, 0, sizeof(HPbuffer_0_5Hz));
  memset(HPmeasSample, 0, sizeof(HPmeasSample));

  writeCount = 0;

  Serial.println("Press the green button to start!");
  while (digitalRead(startButton) == HIGH) {
    digitalWrite(LED_GREEN, HIGH);
    delay(500);
    digitalWrite(LED_GREEN, LOW);
    delay(500);
  }
  flagInterrupt = false;

  //close valve and let the air settle
  digitalWrite(VALVE, HIGH);

  startTimer = millis();
  endTimer = startTimer;

  while (endTimer - startTimer < settleTime && !flagInterrupt) {
    endTimer = millis();
  }

  //take start pressure as reference
  startPressure = pressureSensor.readPressure();
  currentPressure[0] = startPressure;

  //start pumping the cuff and monitor it
  digitalWrite(PUMP, HIGH);

  startTimer = millis();
  endTimer = startTimer;
  startMaxTimer = startTimer;

  while (currentPressure[0] - startPressure < pressureIncrease && !flagInterrupt && endTimer - startMaxTimer < maxTime) {

    startTimer = millis();
    endTimer = startTimer;

    currentPressure[0] = pressureSensor.readPressure();

    while (endTimer - startTimer < measPeriod && !flagInterrupt) {
      endTimer = millis();
    }

    writeCount += 1;
  }
  writeCount = 0;

  //turn of the pump and let the air settle
  digitalWrite(PUMP, LOW);

  startTimer = millis();
  endTimer = startTimer;

  while (endTimer - startTimer < settleTime && !flagInterrupt) {
    endTimer = millis();
  }

  //shift buffer of the current pressure
  currentPressure[0] = pressureSensor.readPressure();
  currentPressure[1] = currentPressure[0];

  //finished with pumping -> steady pressure release
  startMaxTimer = millis();
  endTimer = startMaxTimer;

  while (endTimer - startMaxTimer < maxTime && !flagInterrupt && currentPressure[0] - startPressure > pressureThreshold) {

    currentPressure[1] = currentPressure[0];
    currentPressure[0] = pressureSensor.readPressure();

    HPfilter(&currentPressure[0], &HPbuffer_5Hz[0], &HPbuffer_0_5Hz[0]);

    *(&measSample[0] + writeCount) = (uint16_t)(round(100 * (currentPressure[0] - startPressure)));   //write samples to the measurement array
    *(&HPmeasSample[0] + writeCount) = (int16_t)(round(1000 * HPbuffer_0_5Hz[0]));                    //write filtered samples to the hp measurement array

    startTimer = millis();
    endTimer = startTimer;

    writeCount += 1;

    Serial.print("Pressure (hPa): ");
    Serial.println(currentPressure[0] - startPressure);

    while (endTimer - startTimer < measPeriod && !flagInterrupt) {
      endTimer = millis();
    }
  }

  digitalWrite(VALVE, LOW);  //deflate the cuff

  /*put your code here*/
  findHeartbeat();
  printToScreen();

  //write measurement array to a .txt file
  if (!flagInterrupt) {
    fatfs.remove("pressureMeasurement.txt");
    myFile = fatfs.open("pressureMeasurement.txt", FILE_WRITE);
    if (myFile) {
      Serial.print("Writing to pressureMeasurement.txt...");
      for (int i = 0; i < sizeof(measSample) / 2; i++) {
        myFile.print(measSample[i]);
        myFile.print(",");
      }
      myFile.close();
      Serial.println("done!");
    } else {
      Serial.println("error opening pressureTest.txt");
    }

    fatfs.remove("HPpressureMeasurement.txt");
    myFile = fatfs.open("HPpressureMeasurement.txt", FILE_WRITE);
    if (myFile) {
      Serial.print("Writing to HPpressureMeasurement.txt...");
      for (int i = 0; i < sizeof(measSample) / 2; i++) {
        myFile.print(HPmeasSample[i]);
        myFile.print(",");
      }
      myFile.close();
      Serial.println("done!");
    } else {
      Serial.println("error opening pressureTest.txt");
    }
  }
}


/// @brief Uses the buffered data to find the heartbeat
void findHeartbeat() {

    int16_t tPeakMeas[NOP];       //timestamp of the detected peak
    int16_t tPeakMeasTh[NOP];     //timestamp of the buffer
    int16_t peakMeas[NOP];    //buffer for the detected peak, maximum 2 minutes with heart rate of 180/min
    int16_t peakMeasTh[NOP];  //buffer for the detected peak, after threshold detection

    
    int peakMax = 0;      //intermediate maximum value of the peak
    int peakMin = 100;    //intermediate minimum value of the peak
    int absMax = 0;       //absolut maximum of the peaks
    u_int8_t indexAbsMax = 0;  //index of the maximum peak

    bool flagHeart = false; //flag if heartbeat is detected
    
    
    int indexSweep = 0;   //index sweep to write in the buffer
    int i = 0;

    // reset buffer arrays
    memset(peakMeas, 0, sizeof(peakMeas));
    memset(tPeakMeas, 0, sizeof(tPeakMeas));
    memset(peakMeasTh, 0, sizeof(peakMeas));
    memset(tPeakMeasTh, 0, sizeof(tPeakMeas));

    // go through the first 360 values of the filtered data, 360 is the size of the buffer (peakMeas, tPeakMeas, ...)
    while (i < NOS && indexSweep < NOP) {

        // find the maximum value of the next peak in the filtered data
        // The value '100' refers to 0.1 hPa and is used as a threshold to decern peaks from valleys
        while (HPmeasSample[i] > 100) {
        if (peakMax < HPmeasSample[i]) {
            peakMax = HPmeasSample[i];
            tPeakMeas[indexSweep] = i;
        }
        i++;
        }

        // find the minimum of the following valley in the filtered data
        while (HPmeasSample[i] <= 100 && i < NOS) {
            if (peakMin > HPmeasSample[i]) {
                peakMin = HPmeasSample[i];
        }
        i++;
        }
        peakMeas[indexSweep] = peakMax - peakMin; // Safe the difference between the maximum and the minimum of the heartbeat
        peakMax = 0;
        peakMin = 100;

        //Serial.println(peakMeas[indexSweep]);
        indexSweep++;
    }

    // Find the largest peak in the previously found peaks
    for (int l = 0; l < NOP; l++) {
        if (absMax < peakMeas[l]) {
        absMax = peakMeas[l];
        indexAbsMax = l;
        }
    }

    // Removing false positives due to noise by threshold detection
    i = 0;
    for (int a = 0; a < NOP; a++) {
        if (peakMeas[a] > 0.4 * absMax) {
        peakMeasTh[i] = peakMeas[a];
        tPeakMeasTh[i] = tPeakMeas[a];
        i++;      
        }
    }

    size_t heartrate_index = 0;

    for (int l = 0; l < NOP; l++) {
        if (peakMeasTh[l] == 0) {
            heartrate_index = l / 2;

            break;
        }
    }

    //Serial.print("heart rate index:");
    //Serial.println(heartrate_index);

    // Calculate the heart rate in bpm
    const int count = 5;
    int16_t time = 0;
        

    for (size_t k = heartrate_index; k < heartrate_index + count; k++)
    {
        if (k + 1 >= NOP) {
            break;
        }
        #ifdef DEBUG
        time += (tPeakMeasTh[k+1] - tPeakMeasTh[k]) * measPeriod;
        Serial.print("index ");
        Serial.print(k);
        Serial.print(": ");
        Serial.println(time);
        #endif // DEBUG
    }
    // Calculate the average time between the peaks
    time = time / count;
    heartRate = 60000 / time;

    Serial.println("done!");
    Serial.print("Number of Peaks: ");
    Serial.println(indexSweep);
    Serial.print("Absolut maxima (hPa): ");
    Serial.println(((float)absMax) / 1000.0);
    Serial.print("Index absolut maxima: ");
    Serial.println(indexAbsMax);
    Serial.print("Heart-rate /1/min): ");
    Serial.println(round(heartRate));
    Serial.println("Finished printing!");
    Serial.println("\n ################### \n");
    delay(1000);

    #ifdef DEBUG
    Serial.println("PeakMeas:");
    print_array(peakMeas, NOP);

    Serial.println("tPeakMeas:");
    print_array(tPeakMeas, NOP);

    Serial.println("PeakMeasTh:");
    print_array(peakMeasTh, NOP);

    Serial.println("tPeakMeasTh:");
    print_array(tPeakMeasTh, NOP);
    #endif // DEBUG
}

void printToScreen() {
  display.fillScreen(ST77XX_BLACK);

  display.print("Heart rate: ");
  display.print(heartRate);

  yCursor += 10;
  display.setCursor(xCursor, yCursor);


}

void interruptFunction() {

  flagInterrupt = true;
}

//function of the highpass filter
void HPfilter(float* pressure, float* HP_5Hz, float* HP_0_5Hz) {

  *(HP_5Hz + 1) = *(HP_5Hz);      //Shift buffer
  *(HP_0_5Hz + 1) = *(HP_0_5Hz);  //Shift buffer

  //Highpass filter in time domain
  *(HP_5Hz) = HPnominator_5Hz * (*(pressure) - *(pressure + 1)) + HPdenominator_5Hz * (*(HP_5Hz + 1));
  *(HP_0_5Hz) = HPnominator_0_5Hz * (*(HP_5Hz) - *(HP_5Hz + 1)) + HPdenominator_0_5Hz * (*(HP_0_5Hz + 1));
}

void print_array(int16_t *array, int16_t size) {
    for (size_t i = 0; i < size; i++)
    {
        if (array[i] != 0)
            Serial.println(array[i]);
    }
    
}