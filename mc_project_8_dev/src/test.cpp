#include "SPI.h"
#include "SdFat.h"
#include "Adafruit_SPIFlash.h"
#include "Adafruit_TinyUSB.h"
#include <math.h>

#define DEBUG

#define NOS 12000
#define NOP 360

#define LED_RED 5
#define LED_GREEN 10
#define BTN_RED 9
#define BTN_GREEN 7

#define VALVE A0
#define PUMP A1

#define pressureIncrease 280  //put this value to 240+40 as overshoot compensation if you want to measure the blood pressure
#define pressureThreshold 40  //lower threshold, when the cuff is deflated, put this value to 40 for blood pressure measurement
#define settleTime 500        //settle time in ms, when pump/valve is turned on/off
#define maxTime 120000        //after 2 minutes stop the measurement
#define measPeriod 10         //10ms of sampling time
#define NOS 12000             // number of samples, maximum 2 minutes every 10ms -> 12000 entries
#define NOP 360               // "Number Of Peaks" - Array size of the buffer for the detected peaks, maximum 2 minutes with heart rate of 180/min

#define HPnominator_5Hz 0.864244751836367      //filter coefficient of the nominator of the highpass filter, with f_3dB = 5Hz
#define HPdenominator_5Hz 0.728489503672734    //filter coefficient of the denominator of the highpass filter, with f_3dB = 5Hz
#define HPnominator_0_5Hz 0.984534960996653    //filter coefficient of the nominator of the highpass filter, with f_3dB = 0.5Hz
#define HPdenominator_0_5Hz 0.969069921993306  //filter coefficient of the denominator of the highpass filter, with f_3dB = 0.5Hz

// mass storage - DO NOT TOUCH -----------------------------------------------
//Library object for the flash configuration
Adafruit_FlashTransport_QSPI flashTransport;
Adafruit_SPIFlash flash(&flashTransport);
// file system object from SdFat
FatVolume fatfs;
FatFile root;
FatFile file;

// ----------------------------------------------------------------------------

File32 myFile;     //file format for the SD memory
int readFile;      //read the file position, ATTENTION: Output is decoded as integer in ASCII-format!
String printData;  //print data to console as string

uint16_t measSample[NOS];   //array for the measured samples, maximum 2 minutes every 10ms -> 12000 entries
int16_t HPmeasSample[NOS];  //array of the highpass filtered samples

void findHeartbeat();
int16_t * read_file(String filename);
void print_array(int16_t *array, int16_t length);
void readHPP_from_file();

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
    digitalWrite(VALVE, HIGH);

    pinMode(PUMP, OUTPUT);   
    digitalWrite(PUMP, LOW);

    Serial.begin(9600);
    while (!Serial) delay(10); // wait for native usb

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
}
 

void loop() {
    // send data only when you receive data:
  if (Serial.available() > 0) {
    Serial.read();
    digitalWrite(LED_BUILTIN, HIGH);
    readHPP_from_file();
    findHeartbeat();
    digitalWrite(LED_BUILTIN, LOW);
    
  }
    if (digitalRead(BTN_GREEN) == LOW)
    {
        digitalWrite(LED_GREEN, HIGH);
        //*HPmeasSample = read_file("HPpressureMeasurement.txt");
        readHPP_from_file();
        findHeartbeat();

        digitalWrite(LED_GREEN, LOW);
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
    Serial.println("PeakMeas;");
    print_array(peakMeas, NOP);
    Serial.println(";");

    Serial.println("tPeakMeas;");
    print_array(tPeakMeas, NOP);

    Serial.println("PeakMeasTh;");
    print_array(peakMeasTh, NOP);

    Serial.println("tPeakMeasTh;");
    print_array(tPeakMeasTh, NOP);

    Serial.println("$");
    #endif // DEBUG
}

void print_array(int16_t *array, int16_t size) {
    for (size_t i = 0; i < size; i++)
    {
        if (array[i] != 0) {
            Serial.print(array[i]);
            Serial.println(",");
        }    
    }
    
}

void readHPP_from_file() {
    int indexSweep = 0;
    myFile = fatfs.open("HPpressureMeasurement.txt", FILE_READ);
    if (myFile) {
    Serial.print("Read HPpressureMeasurement.txt...");
    // read from the file until there’s nothing else in it:
    while (myFile.available()) {
        readFile = myFile.read();
        if ((char) readFile == ',') {
            HPmeasSample[indexSweep] = printData.toInt();
            indexSweep += 1;
            printData = "";
        } else {
            printData += (char) readFile;
        }
    }
    myFile.close();
    } else {
    Serial.println("error opening test.txt");
    }
    Serial.println("done.");
}