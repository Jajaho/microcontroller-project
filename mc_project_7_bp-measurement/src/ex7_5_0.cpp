#include <Arduino.h>
#include <Adafruit_TinyUSB.h> // for Serial
#include <Wire.h>
//For the display
#include <Adafruit_GFX.h> // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <SPI.h>
//library of the pressure sensor
#include "Adafruit_MPRLS.h"

//actors: pump and valve
#define PUMP 9
#define VALVE 10
//Buttons and leds
#define startButton 12 //green button
#define interruptButton 7 //red button
//Display: Waveshare 17344
#define Display_CS 17 //chip select
#define Display_DC 16 // data/command
#define Display_RST 15 // reset
#define Display_BL 14 // backlight
Adafruit_MPRLS pressureSensor = Adafruit_MPRLS();
Adafruit_ST7789 display = Adafruit_ST7789(Display_CS, Display_DC, Display_RST);

//input: chip select; data/command and reset pin -> reqquired for the library
//threshold values for the measurement
#define maxPressure 240 //max. 180mmHg of cuff pressure
#define overshootPressure 40 //overhead added to maxPressure to compensate for pressure ovreshoot while pumping
#define minPressure 40 //min. 30mmHg of cuff pressure
#define settleTime 500 //settle time when pump or valve is turned on/off -> time to let the air in the cuff settle
#define maxTimeMeas 120000 //max time until measurement is stopped
#define maxTimePump 30000 //max time until pumping is stopped
#define measPeriod 10 //sample time of the sensor

//Highpass filter coefficients
#define HPnominator_5Hz 0.864244751836367 //filter coefficient of the nominator of the highpass filter, with f_3dB = 5Hz
#define HPdenominator_5Hz 0.728489503672734 //filter coefficient of the denominator of the highpass filter, with f_3dB = 5Hz
#define HPnominator_0_5Hz 0.984534960996653 //filter coefficient of the nominator of the highpass filter, with f_3dB = 0.5Hz
#define HPdenominator_0_5Hz 0.969069921993306 //filter coefficient of the denominator of the highpass filter, with f_3dB = 0.5Hz

//declare variables for the pressure measurement
float currentPressure[2]; //intermediate pressure values
float startPressure = 0; //intermediate pressure values
volatile bool flagInterrupt = false; //emergency flag from the interrupt

//declare variables for time relevant uses
unsigned long startTimer = 0;
unsigned long endTimer = 0;
unsigned long startMaxTimer = 0;

//buffer for the measurement of the total pressure
uint16_t measSample[12000];
int printCount = 0;
float printFloat = 0;
int indexSweep = 0;

//buffer for the highpass filtered pressure
float HPbuffer_5Hz[2];
float HPbuffer_0_5Hz[2];
int16_t HPmeasSample[12000];

//variables for the cursor of the display (0,0) is the top-left corner of the display in our case
int16_t xCursor = 0;
int16_t yCursor = 0;

//Data evaluation
#define peakTh 100 //threshold if peak is detected
#define peakFilter 0.3 //factor of minimum peak pressure
#define sysTh 0.5 //threshold value for the systolic pressure
#define diaTh 0.8 //threshold value for the diasolic pressure
int16_t HPsample; //Average filter of the highpass filtered data
int16_t peakMeas[360]; //Measured peaks, for meas time of 2min and heartrate of 180 -> max. 360 (also enough overhead for false noise-peaks)
uint16_t tPeakMeas[360]; //time-stamp of the detected peak
int16_t peakMeasTh[360]; //higher threshold of peakdetection, depending on the maxima from peakMeas array
uint16_t tPeakMeasTh[360]; //corresponding time stamp
bool flagHeart = false; //flag if heart beat is detected
float absMax = 0; //biggest peak
int16_t indexAbsMax = 0; //index in the peak array of the biggest one
int16_t peakMax = 0; //maximum intermediate value
int16_t peakMin = 0; //smallest intermediate value
float dataSlope = 0; //slope between two peaks -> used for evaluation
float dataTime = 0; //point in time when threshold is reached
float sysPressure = 0; //systolic blood pressure
float diaPressure = 0; //diastolic blodd pressure
float heartRate = 0; //heart rate

//Numbers and conversion factors
#define hPa_mmHg 0.75006375541921

void setup() {
    Serial.begin(9600);
    Serial.print("Wait for the serial port...");
    while (!Serial) {
        delay(10);
    }
    Serial.print("done!");
    Serial.print("Start communication with pressure sensor...");
    if (!pressureSensor.begin()) {
        Serial.println("Failed to communicate with MPRLS sensor, check wiring?");
    while (1) {
    delay(10);
    }
    }
    Serial.println("done!");
    Serial.print("Initializing Filesystem on external flash...");

    //GPIO setup
    pinMode(PUMP, OUTPUT);
    pinMode(VALVE, OUTPUT);
    digitalWrite(PUMP, LOW);
    digitalWrite(VALVE, LOW);
    pinMode(Display_BL, OUTPUT);
    digitalWrite(Display_BL, HIGH);
    pinMode(startButton, INPUT_PULLUP);
    pinMode(interruptButton, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(interruptButton), interruptFunction, CHANGE); //attach interrupt to red button -> emergency deflate button

    //Initiate the display
    display.init(240, 320);
    display.setRotation(3);
    display.fillScreen(ST77XX_BLACK);

    //Home screen of the display
    display.setCursor(0, 0);
    display.setTextSize(3);
    display.setTextColor(ST77XX_WHITE);
    display.print("uC-Projekt");
    display.setCursor(5, 27);
    display.print("2023");
}

void loop() {
    digitalWrite(PUMP, LOW);
    digitalWrite(VALVE, LOW);
    Serial.println("Press the green button to start!");

    while (digitalRead(startButton) == HIGH) {
    }

    flagInterrupt = false;

    //reset variables
    startPressure = 0;
    startTimer = 0;
    endTimer = 0;
    startMaxTimer = 0;
    memset(measSample, 0, sizeof(measSample)); //reset array
    printCount = 0;
    printFloat = 0;
    memset(currentPressure, 0, sizeof(currentPressure));
    memset(HPbuffer_5Hz, 0, sizeof(HPbuffer_5Hz));
    memset(HPbuffer_0_5Hz, 0, sizeof(HPbuffer_0_5Hz));
    memset(HPmeasSample, 0, sizeof(HPmeasSample));
    xCursor = 0;
    yCursor = 0;
    HPsample = 0;
    memset(peakMeas, 0, sizeof(peakMeas));
    memset(tPeakMeas, 0, sizeof(tPeakMeas));
    memset(peakMeasTh, 0, sizeof(peakMeasTh));
    memset(tPeakMeasTh, 0, sizeof(tPeakMeasTh));
    indexSweep = 0;
    flagHeart = false;
    absMax = 0;
    indexAbsMax = 0;
    peakMax = 0;
    peakMin = 0;
    dataSlope = 0;
    dataTime = 0;
    sysPressure = 0;
    diaPressure = 0;
    heartRate = 0;

    display.fillScreen(ST77XX_BLACK);
    display.setCursor(0, 0);
    display.print("Pressure (mmHg):\n");
    xCursor = display.getCursorX();
    yCursor = display.getCursorY();

    /*put your code here*/
    
    Serial.println("finished printing!");
    display.setTextColor(ST77XX_CYAN);
    display.print("Systolic(mmHg)\n");
    display.print(round(hPa_mmHg * sysPressure));
    display.print('\n');
    display.setTextColor(ST77XX_MAGENTA);
    display.print("Diastolic(mmHg)\n");
    display.print(round(hPa_mmHg * diaPressure));
    display.print('\n');
    display.setTextColor(ST77XX_YELLOW);
    display.print("Heart-Rate(1/min)\n");
    display.print(round(heartRate));
}

//Interrupt function for the red button -> set flag always true and toggle the LED
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
