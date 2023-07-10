#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>     // Core graphics library
#include <Adafruit_ST7789.h>  // Hardware-specific library for ST7789
#include <Adafruit_TinyUSB.h>
#include <Adafruit_MPRLS.h>
#include "SdFat.h"
#include "Adafruit_SPIFlash.h"

// -----  Pins  -------- //

#define RESET_PIN -1 // set to any GPIO pin # to hard-reset on begin()
#define EOC_PIN -1 // set to any GPIO pin to read end-of-conversion by pin

#define LED_RED 5
#define LED_GREEN 10
#define BTN_RED 9
#define BTN_GREEN 7

#define VALVE A0
#define PUMP A1

// Waveshare 17344
#define TFT_CS A5   //chip select *put your pin here*
#define TFT_DC A2    // data/command *put your pin here*
#define TFT_RST A4   // reset *put your pin here*
#define TFT_BL A3    // backlight *put your pin here*

// -----  Constants  -------- //

// Display
#define SCREEN_HEIGHT 240
#define SCREEN_WIDTH 320

#define NOS 12000             //number of samples, maximum 2 minutes every 10ms -> 12000 entries
#define NOP 360               //number of peaks in the buffer 

#define pressureIncrease 280  //put this value to 240+40 as overshoot compensation if you want to measure the blood pressure
#define pressureThreshold 40  //lower threshold, when the cuff is deflated, put this value to 40 for blood pressure measurement
#define settleTime 500        //settle time in ms, when pump/valve is turned on/off
#define maxTime 120000        //after 2 minutes stop the measurement
#define measPeriod 10         //10ms of sampling time

// Highpass filter
#define HPnominator_5Hz 0.864244751836367      //filter coefficient of the nominator of the highpass filter, with f_3dB = 5Hz
#define HPdenominator_5Hz 0.728489503672734    //filter coefficient of the denominator of the highpass filter, with f_3dB = 5Hz
#define HPnominator_0_5Hz 0.984534960996653    //filter coefficient of the nominator of the highpass filter, with f_3dB = 0.5Hz
#define HPdenominator_0_5Hz 0.969069921993306  //filter coefficient of the denominator of the highpass filter, with f_3dB = 0.5Hz

// -----  Objects -------- //

//input: chip select; data/command and reset pin -> reqquired for the library
Adafruit_ST7789 display = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
Adafruit_MPRLS pressureSensor = Adafruit_MPRLS(RESET_PIN, EOC_PIN);

//Library object for the flash configuration
Adafruit_FlashTransport_QSPI flashTransport;
Adafruit_SPIFlash flash(&flashTransport);

// file system object from SdFat
FatVolume fatfs;  //file system
File32 myFile;    //file format for the SD memory
int readFile;     //read the file position, ATTENTION: Output is decoded as integer in ASCII-format!

float pressure_hPa = 0;
float targetPressure = 1000;

float heartRate = 0;  //final heart rate

// Program fllow control flags
volatile bool panicFlag = false;  // flag to signal that an interrupt has been detected
volatile bool pumpFlag = false;   // flag to signal that the pump is turned on
volatile bool measFlag = false;   // flag to signal that a measurement is ongoing
volatile bool mecpFlag = false;  // flag to signal that the measurement is completed

unsigned long startTimer = 0;     // Timestamp of the last measurement
unsigned long sensorSpeed = 0;    // Time it took the sensor to measure the pressure and communicate to the uC
unsigned long timeStamp = 0;      // Set at the beginning to achieve constant measurement period

int writeCount = 0;  //used for print counter every 500ms and position in array to write to

float HPbuffer_5Hz[2];        //buffer of last two highpass filter values, with f_3dB = 5Hz
float HPbuffer_0_5Hz[2];      //buffer of last two highpass filter values, with f_3dB = 0.5Hz
uint16_t measSample[NOS];     //array for the measured samples, maximum 2 minutes every 10ms -> 12000 entries
int16_t HPmeasSample[NOS];    //array of the highpass filtered samples

int16_t xCursor = 70;
int16_t yCursor = 130;

void redISR();
void greenISR();
void calcHeartbeat();
void HPfilter(float* pressure, float* HP_5Hz, float* HP_0_5Hz);
void print_array(int16_t *array, int16_t size);

void setup() {
  // -----  Inputs  -------- //
  pinMode(BTN_RED, INPUT_PULLUP);   // Button is active low!
  pinMode(BTN_GREEN, INPUT_PULLUP); // Button is active low!

  // -----  Outputs  -------- //
  // LEDs:
  pinMode(LED_GREEN, OUTPUT);
  digitalWrite(LED_GREEN, LOW);

  pinMode(LED_RED, OUTPUT);
  digitalWrite(LED_RED, LOW);

  pinMode(LED_BUILTIN, OUTPUT);   
  digitalWrite(LED_BUILTIN, LOW);

  // ----- Actuators --------- //
  pinMode(VALVE, OUTPUT);
  digitalWrite(VALVE, HIGH);  // close valve

  pinMode(PUMP, OUTPUT);   
  digitalWrite(PUMP, LOW);

  attachInterrupt(digitalPinToInterrupt(BTN_RED), redISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(BTN_GREEN), greenISR, FALLING);

  Serial.begin(115200);
  Serial.println("Initializing MPRLS");
  if (! pressureSensor.begin()) {
    Serial.println("Failed to communicate with MPRLS sensor, check wiring?");
    while (1) {
      delay(10);
    }
  }
  Serial.println("MPRLS sensor initialized.");

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
  display.init(SCREEN_WIDTH, SCREEN_HEIGHT);
  display.setRotation(3);
  display.fillScreen(ST77XX_BLACK);
  display.setCursor(xCursor, yCursor);
  display.setTextWrap(true);
  display.setTextSize(4);

  //display.print("Hello World!");

}

void loop() {

  // -----  Panic Flag -------- //
  if(panicFlag) {
    Serial.println("Panic! Operation has been halted. Reset to continue.");
    digitalWrite(LED_RED, HIGH);
    digitalWrite(PUMP, LOW);
    digitalWrite(BTN_GREEN, LOW);
  }
  else {
    digitalWrite(LED_RED, LOW);
  }

  // -----  Pump Flag -------- //
  if(pumpFlag)
  {
    digitalWrite(PUMP, HIGH);
    digitalWrite(VALVE, HIGH);
    digitalWrite(LED_GREEN, HIGH);
  }
  else {
    digitalWrite(PUMP, LOW);
    digitalWrite(LED_GREEN, LOW);
  }
  
  // Stop pump once the target pressure has been reached
  if (pumpFlag && (pressure_hPa >= targetPressure))
  {
    pumpFlag = false;
  }
  
  
  // -----  Read Pressure -------- //

  timeStamp = millis();
  if ((timeStamp - startTimer >= measPeriod) && !mecpFlag) {
    if (pumpFlag) {
      digitalWrite(PUMP, LOW);
      delay(6);
    }
    
    startTimer = timeStamp; // set the timestamp of the current measurement

    pressure_hPa = pressureSensor.readPressure();
    sensorSpeed = millis() - startTimer;
    
    display.fillScreen(ST77XX_BLACK);
    display.setCursor(xCursor, yCursor);
    display.print(pressure_hPa);

    # if DEBUG
    Serial.print("Pressure (hPa): "); //Print the measured pressure
    Serial.println(pressure_hPa);
    Serial.print("Read-out speed (ms): "); //Print the read-out time of one measurement
    Serial.println(sensorSpeed);

    Serial.print("panicFlag: ");
    Serial.println(panicFlag);
    Serial.print("pumpFlag: ");
    Serial.println(pumpFlag);
    #endif //debug
  }

  // -----  Recording Data -------- //
  if (measFlag) {
    if (writeCount < NOS) {
      measSample[writeCount] = pressure_hPa;
    }
    else {
      measFlag = false;
      writeCount = 0;
      mecpFlag = true;
    }
    writeCount++;
  }

  // -----  Computation & Display -------- //
  if (mecpFlag) {
    calcHeartbeat();

    display.fillScreen(ST77XX_BLACK);
    display.setCursor(xCursor, yCursor + 50);
    display.print(heartRate);
    mecpFlag = false;
  }
}


void calcHeartbeat() {

    int16_t tPeakMeas[NOP];       //timestamp of the detected peak
    int16_t tPeakMeasTh[NOP];     //timestamp of the buffer
    int16_t peakMeas[NOP];    //buffer for the detected peak, maximum 2 minutes with heart rate of 180/min
    int16_t peakMeasTh[NOP];  //buffer for the detected peak, after threshold detection

    
    int peakMax = 0;      //intermediate maximum value of the peak
    int peakMin = 100;    //intermediate minimum value of the peak
    int absMax = 0;       //absolut maximum of the peaks
    u_int8_t indexAbsMax = 0;  //index of the maximum peak

    bool flagHeart = false; //flag if heartbeat is detected
    float heartRate = 0;  //final heart rate
    
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

    size_t heartrate_index = absMax;
    /*
    for (int l = 0; l < NOP; l++) {
        if (peakMeasTh[l] == 0) {
            heartrate_index = l / 2;

            break;
        }
    }
*/
    const int count = 5;
    int16_t time = 0;
        

    for (size_t k = heartrate_index; k < heartrate_index + count; k++)
    {
        if (k + 1 >= NOP) {
            break;
        }
        
        time += (tPeakMeasTh[k+1] - tPeakMeasTh[k]) * measPeriod;
        #ifdef DEBUG
        Serial.print("index ");
        Serial.print(k);
        Serial.print(": ");
        Serial.println(time);
        #endif // DEBUG
    }
    // Calculate the average time between the peaks
    time = time / count;
    heartRate = 60000 / time;
    #ifdef DEBUG

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

void calcBloodPressure() {

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

void redISR() {
  panicFlag = true;
  pumpFlag = false;
  measFlag = false;
  mecpFlag = false;
}

void greenISR() {
  pumpFlag = true;
  panicFlag = false;
  measFlag = true;
  mecpFlag = false;
}