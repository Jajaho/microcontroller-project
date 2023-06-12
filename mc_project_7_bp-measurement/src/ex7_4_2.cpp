#include <SPI.h>
#include "SdFat.h"
#include "Adafruit_SPIFlash.h"
#include <math.h>

#define startButton 12 //*put your pin here*
#define peakTh 100 //threshold if peak is detected
#define peakFilter 0.3 //factor of minimum peak pressure
#define sysTh 0.5 //threshold value for the systolic pressure
#define diaTh 0.8 //threshold value for the diasolic pressure
#define hPa_mmHg 0.75006375541921 //convert hPa to mmHg

//Library object for the flash configuration
Adafruit_FlashTransport_QSPI flashTransport;
Adafruit_SPIFlash flash(&flashTransport);

// file system object from SdFat
FatVolume fatfs; //file system
File32 myFile; //file format for the SD memory

int readFile; //read the file position, ATTENTION: Output is decoded as integer in ASCII-format!
String printData; //print data to console as string
uint16_t measSample[12000]; //array for the measured samples, maximum 2 minutes every 10ms -> 12000 entrie
int16_t HPmeasSample[12000]; //buffer for the highpass filtered samples from the flash memory
int16_t HPsample; //intermediate buffer value from HPmeasSample
int16_t peakMeas[360]; //buffer for the detected peak, maximum 2 minutes with heart rate of 180/min
int tPeakMeas[360]; //timestamp of the detected peak
int16_t peakMeasTh[360]; //buffer for the detected peak, after threshold detection
int tPeakMeasTh[360]; //timestamp of the buffer
int indexSweep = 0; //index sweep to write in the buffer
bool flagHeart = false; //flag if heartbeat is detected
int peakMax = 0; //intermediate maximum value of the peak
int peakMin = 0; //intermediate minimum value of the peak
int absMax = 0; //absolut maximum of the peaks
int indexAbsMax = 0; //index of the maximum peak
float dataSlope = 0; //slope between two peaks -> used for evaluation
float dataTime = 0; //point in time when threshold is reached
float sysPressure = 0; //systolic blood pressure
float diaPressure = 0; //diastolic blodd pressure
float heartRate = 0; //final heart rate

void setup() {
    // Open serial communications and wait for port to open:
    Serial.begin(9600);
    Serial.println("Initializing Filesystem on external flash...");

    // Init external flash
    flash.begin();

    // Open file system on the flash
    if (!fatfs.begin(&flash)) {
        Serial.println("Error: filesystem is not existed. Please try SdFat_format example to make one.");
        while (1) {
            yield();
            delay(10);
        }
    }

    Serial.println("initialization done.");
    myFile = fatfs.open("pressureMeasurement.txt", FILE_READ);

    if (myFile) {
        Serial.print("Read pressureMeasurement.txt...");
        // read from the file until there’s nothing else in it:
        while (myFile.available()) {
            readFile = myFile.read();
            if ((char)readFile == ',') {
                measSample[indexSweep] = printData.toInt();
                indexSweep += 1;
                printData = "";
            } else {
                printData += (char)readFile;
            }
        }
        myFile.close();
    } else {
        Serial.println("error opening test.txt");
    }

    Serial.println("done.");
    indexSweep = 0;
    myFile = fatfs.open("HPpressureMeasurement.txt", FILE_READ);

    if (myFile) {
        Serial.print("Read HPpressureMeasurement.txt...");

        // read from the file until there’s nothing else in it:
        while (myFile.available()) {
            readFile = myFile.read();
            if ((char)readFile == ',') {
                HPmeasSample[indexSweep] = printData.toInt();
                indexSweep += 1;
                printData = "";
            } else {
                printData += (char)readFile;
            }
        }
        myFile.close();
    } else {
        Serial.println("error opening test.txt");
    }
    Serial.println("done.");
    indexSweep = 0;
    pinMode(startButton, INPUT_PULLUP);
}

void loop() {
    flagHeart = false;
    absMax = 0;
    indexAbsMax = 0;
    peakMax = 0;
    peakMin = 0;
    indexSweep = 0;
    heartRate = 0;
    memset(peakMeas, 0, sizeof(peakMeas));
    memset(tPeakMeas, 0, sizeof(tPeakMeas));
    Serial.println("Press the green button to start!");
    while (digitalRead(startButton) == HIGH) {
        }
    Serial.println("Searching for the peaks in the measurement data ... ");

    /*search peak algorithm*/
    /*put your code here*/

    Serial.println("done!");
    Serial.print("Systolic blood pressure (mmHg): ");
    Serial.println(hPa_mmHg * sysPressure);
    Serial.print("Diastolic blood pressure (mmHg): ");
    Serial.println(hPa_mmHg * diaPressure);
    Serial.print("Heart-rate /1/min): ");
    Serial.println(round(heartRate));
    Serial.println("Finished printing!");
    Serial.println("\n ################### \n");
    delay(1000);
}
