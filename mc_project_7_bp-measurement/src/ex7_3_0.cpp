#include <Arduino.h>
#include <Adafruit_TinyUSB.h> // for Serial
#include <Wire.h>
#include "Adafruit_MPRLS.h"
#include <SPI.h>
#include <math.h>
#include "SdFat.h"
#include "Adafruit_SPIFlash.h"

#define PUMP 9 //*put your pin here*
#define VALVE 10 //*put your pin here*
#define startButton 12 //*put your pin here*
#define interruptButton 7 //*put your pin here*
#define pressureIncrease 280 //put this value to 240+40 as overshoot compensation if you want to measure the blood pressure
#define pressureThreshold 40 //lower threshold, when the cuff is deflated,put this value to 40 for blood pressure measurement

#define settleTime 500 //settle time in ms, when pump/valve is turned on/off
#define maxTime 120000 //after 2 minutes stop the measurement
#define measPeriod 10 //10ms of sampling time
#define HPnominator_5Hz 0.864244751836367 //filter coefficient of the nominator of the highpass filter, with f_3dB = 5Hz
#define HPdenominator_5Hz 0.728489503672734 //filter coefficient of the denominator of the highpass filter, with f_3dB = 5Hz
#define HPnominator_0_5Hz 0.984534960996653 //filter coefficient of the nominator of the highpass filter, with f_3dB = 0.5Hz
#define HPdenominator_0_5Hz 0.969069921993306 //filter coefficient of the denominator of the highpass filter, with f_3dB = 0.5Hz

//Library object for the flash configuration
Adafruit_FlashTransport_QSPI flashTransport;
Adafruit_SPIFlash flash(&flashTransport);

// file system object from SdFat
FatVolume fatfs; //file system
File32 myFile; //file format for the SD memory
int readFile; //read the file position, ATTENTION: Output is decoded as integer in ASCII-format!

//Library for the pressure sensor
Adafruit_MPRLS pressureSensor = Adafruit_MPRLS();
float currentPressure[2]; //intermediate pressure values, with buffer of last value
float startPressure = 0; //intermediate pressure values
volatile bool flagInterrupt = false; //emergency flag from the interrupt
unsigned long startTimer = 0; //startpoint of the timer
unsigned long endTimer = 0; //endpoint of the timer
unsigned long startMaxTimer = 0; //start the timer at the beginning of the whole measurement process, to have a maximum time to stop
float HPbuffer_5Hz[2]; //buffer of last two highpass filter values, with f_3dB = 5Hz
float HPbuffer_0_5Hz[2]; //buffer of last two highpass filter values, with f_3dB = 0.5Hz
uint16_t measSample[12000]; //array for the measured samples, maximum 2 minutes every 10ms -> 12000 entries
int16_t HPmeasSample[12000]; //array of the highpass filtered samples
int writeCount = 0; //used for print counter every 500ms and position in array to write to


// ----------------------- Function prototypes ----------------------- //
//Interrupt function
void interruptFunction();

void setup() {
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
  pinMode(startButton, INPUT_PULLUP);
  pinMode(interruptButton, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(interruptButton), interruptFunction, CHANGE);
  pinMode(PUMP, OUTPUT);
  digitalWrite(PUMP, LOW);
  pinMode(VALVE, OUTPUT);
  digitalWrite(VALVE, LOW);

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
  
  while (digitalRead(startButton) == HIGH) {}

  flagInterrupt = false;

  /* PUT YOUR CODE HERE */
  
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


void interruptFunction() {
flagInterrupt = true;
}

//function of the highpass filter
void HPfilter(float* pressure, float* HP_5Hz, float* HP_0_5Hz) {
  *(HP_5Hz + 1) = *(HP_5Hz); //Shift buffer
  *(HP_0_5Hz + 1) = *(HP_0_5Hz); //Shift buffer

  //Highpass filter in time domain
  *(HP_5Hz) = HPnominator_5Hz * (*(pressure) - *(pressure + 1)) +
  HPdenominator_5Hz * (*(HP_5Hz + 1));
  *(HP_0_5Hz) = HPnominator_0_5Hz * (*(HP_5Hz) - *(HP_5Hz + 1)) +
  HPdenominator_0_5Hz * (*(HP_0_5Hz + 1));
}
