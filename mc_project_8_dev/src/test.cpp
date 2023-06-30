#include "SPI.h"
#include "SdFat.h"
#include "Adafruit_SPIFlash.h"
#include "Adafruit_TinyUSB.h"
#include <math.h>

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
#define NOS 12000             //number of samples, maximum 2 minutes every 10ms -> 12000 entries
#define NOP 360               //number of peaks in the buffer 

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
// USB Mass Storage object
Adafruit_USBD_MSC usb_msc;
// Check if flash is formatted
bool fs_formatted = false;
// Set to true when PC write to flash
bool fs_changed = true;

int32_t msc_read_cb(uint32_t lba, void* buffer, uint32_t bufsize);
int32_t msc_write_cb(uint32_t lba, uint8_t* buffer, uint32_t bufsize);
void msc_flush_cb(void);
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

    flash.begin();
    // Set disk vendor id, product id and revision with string up to 8, 16, 4 characters respectively
    usb_msc.setID("Adafruit", "External Flash", "1.0");
    // Set callback
    usb_msc.setReadWriteCallback(msc_read_cb, msc_write_cb, msc_flush_cb);
    // Set disk size, block size should be 512 regardless of spi flash page size
    usb_msc.setCapacity(flash.size() / 512, 512);
    // MSC is ready for read/write
    usb_msc.setUnitReady(true);
    usb_msc.begin();
    // Init file system on the flash
    fs_formatted = fatfs.begin(&flash);

    Serial.begin(9600);
    while (!Serial) delay(10); // wait for native usb
    Serial.println("Adafruit TinyUSB Mass Storage External Flash example");
    Serial.print("JEDEC ID: 0x");
    Serial.println(flash.getJEDECID(), HEX);
    Serial.print("Flash size: ");
    Serial.print(flash.size() / 1024);
    Serial.println(" KB");
    
    
}   
void loop() {
    if (digitalRead(BTN_GREEN) == LOW)
    {
        digitalWrite(LED_GREEN, HIGH);
        //*HPmeasSample = read_file("HPpressureMeasurement.txt");
        readHPP_from_file();
        findHeartbeat();

        digitalWrite(LED_GREEN, LOW);
    }
    
}

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
    // Find the largest peak in the previously found and filtered peaks
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
        time += (tPeakMeasTh[k+1] - tPeakMeasTh[k]) * measPeriod;
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

    /*
    Serial.println("PeakMeas:");
    print_array(peakMeas, NOP);

    Serial.println("tPeakMeas:");
    print_array(tPeakMeas, NOP);

    Serial.println("PeakMeasTh:");
    print_array(peakMeasTh, NOP);

    Serial.println("tPeakMeasTh:");
    print_array(tPeakMeasTh, NOP);
    */
}

void print_array(int16_t *array, int16_t size) {
    for (size_t i = 0; i < size; i++)
    {
        if (array[i] != 0)
            Serial.println(array[i]);
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

// Callback invoked when received READ10 command.
// Copy disk’s data to buffer (up to bufsize) and
// return number of copied bytes (must be multiple of block size)
int32_t msc_read_cb(uint32_t lba, void* buffer, uint32_t bufsize) {
    // Note: SPIFLash Block API: readBlocks/writeBlocks/syncBlocks
    // already include 4K sector caching internally. We don’t need to cache it, yahhhh!!
    return flash.readBlocks(lba, (uint8_t*)buffer, bufsize / 512) ? bufsize : -1;
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk’s storage and
// return number of written bytes (must be multiple of block size)
int32_t msc_write_cb(uint32_t lba, uint8_t* buffer, uint32_t bufsize) {
    digitalWrite(LED_BUILTIN, HIGH);
    // Note: SPIFLash Block API: readBlocks/writeBlocks/syncBlocks
    // already include 4K sector caching internally. We don’t need to cache it, yahhhh!!
    return flash.writeBlocks(lba, buffer, bufsize / 512) ? bufsize : -1;
}

// Callback invoked when WRITE10 command is completed (status received and accepted by host).
// used to flush any pending cache.
void msc_flush_cb(void) {
    // sync with flash
    flash.syncBlocks();
    // clear file system’s cache to force refresh
    fatfs.cacheClear();
    fs_changed = true;
    digitalWrite(LED_BUILTIN, LOW);
}