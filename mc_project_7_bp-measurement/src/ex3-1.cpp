#include <SPI.h>
#include "SdFat.h"
#include "Adafruit_SPIFlash.h"

#define readPressureButton 12 //*put your pin here*
#define readHPPressureButton 7 //*put your pin here*

//Library object for the flash configuration
Adafruit_FlashTransport_QSPI flashTransport;
Adafruit_SPIFlash flash(&flashTransport);

// file system object from SdFat
FatVolume fatfs; //file system
File32 myFile; //file format for the SD memory
int readFile; //read the file position, ATTENTION: Output is decoded as integer in ASCII-format!
String printData; //print data to console as string
bool choosePressure = false;
bool chooseHPPressure = false;

void setup() {
    //start serial communication and set baud rate
    Serial.begin(9600);
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
    pinMode(readPressureButton, INPUT_PULLUP);
    pinMode(readHPPressureButton, INPUT_PULLUP);
}

void loop() {
    Serial.println("Press the green button to start!");
    choosePressure = false;
    chooseHPPressure = false;
    while (!choosePressure && !chooseHPPressure) {
        if (digitalRead(readPressureButton) == LOW) {
            choosePressure = true;
        }
        if (digitalRead(readHPPressureButton) == LOW) {
            chooseHPPressure = true;
        }
    }

    if (choosePressure) {
        myFile = fatfs.open("pressureMeasurement.txt");
        if (myFile) {
            Serial.println("pressureMeasurement.txt:");
            while (myFile.available()) { // read from the file until there’s nothing else in it
                readFile = myFile.read(); //read every character of pressureTest.txt
                if ((char)readFile ==',') { //read number until a comma is detected
                    Serial.println(printData.toInt()); //print data to the console -> print with "Serial Plotter"
                    printData = "";
                    delay(10); //wait 10ms -> print values with same speed as sampling time
                } else {
                    printData += (char)readFile; //concatenate the char to string
                }
            }
            myFile.close();
            Serial.println("finished printing pressureMeasurement.txt!");
        } else {
            Serial.println("error opening pressureMeasurement.txt");
        }
    } else if (chooseHPPressure) {
        myFile = fatfs.open("HPpressureMeasurement.txt");
        if (myFile) {
            Serial.println("HPpressureMeasurement.txt:");
            while (myFile.available()) { // read from the file until there’s nothing else in it
                readFile = myFile.read(); //read every character of pressureTest.txt
                if ((char)readFile == ',') { //read number until a comma is detected
                    Serial.println(printData.toInt()); //print data to the console -> print with "Serial Plotter"
                    printData = "";
                    delay(10); //wait 10ms -> print values with same speed as sampling time
                } else {
                    printData += (char)readFile; //concatenate the char to string
                }
            }
            myFile.close();
            Serial.println("finished printing HPpressureMeasurement.txt!");
        } else {
            Serial.println("error opening HPpressureMeasurement.txt");
        }
    }
}