#include <Arduino.h>
#include <SdFat.h>
#include <Adafruit_SPIFlash.h>

//#include "main.h"

// file system object from SdFat
File32 myFile;
FatVolume fatfs;

int readFile;
String printData;
/*
void setupIO()
{
    pinMode(BUT_G, INPUT_PULLUP); // PULLUP and switch closes to GND
    pinMode(BUT_R, INPUT_PULLUP); // PULLUP and switch closes to GND
    pinMode(LED_G, OUTPUT);       // Green LED in Button
    digitalWrite(LED_G, LOW);
    pinMode(LED_R, OUTPUT); // Red LED in Button
    digitalWrite(LED_R, LOW);
    pinMode(VENT, OUTPUT); // VENT OUTPUT
    digitalWrite(VENT, LOW);
    pinMode(PUMP, OUTPUT); // PUMP Output
    digitalWrite(PUMP, LOW);
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
}
*/
void writeToFile(Adafruit_SPIFlash flash)
{
    // Open file system on the flash
    if (!fatfs.begin(&flash))
    {
        Serial.println("Error: filesystem is not existed. Please try SdFat_format example to make one.");
        while (1)
        {
            yield();
            delay(1);
        }
    }
    myFile = fatfs.open("test.txt", FILE_WRITE); // opens the file -> declare as ".txt" - file, other file types are also possible
    fatfs.remove("test.txt");                    // deletes the file -> make sure to have a clean file to write to
    myFile = fatfs.open("test.txt", FILE_WRITE);
    // if the file opened okay, write to it:
    if (myFile)
    {
        Serial.print("Writing to test.txt...");
        myFile.print(947);
        myFile.print(","); // write data in a comma-seperated manner, add carriage return if you want
        myFile.print(563);
        myFile.print(",");
        // close the file:
        myFile.close();
        Serial.println("done.");
    }
    else
    {
        // if the file didn’t open, print an error:
        Serial.println("error opening test.txt");
    }
}

void readFromFile(Adafruit_SPIFlash flash)
{
    // Open file system on the flash
    if (!fatfs.begin(&flash))
    {
        Serial.println("Error: filesystem is not existed. Please try SdFat_format example to make one.");
        while (1)
        {
            yield();
            delay(1);
        }
    }

    // re-open the file for reading:
    myFile = fatfs.open("test.txt");
    if (myFile)
    {
        Serial.println("test.txt:");
        // read from the file until there’s nothing else in it:
        while (myFile.available())
        {
            readFile = myFile.read(); // reads single char of files and decodes it in ascii
            if ((char)readFile == ',')
            {                                      // read file until ’,’ is found
                Serial.println(printData.toInt()); // print data as int
                printData = "";
            }
            else
            {
                printData += (char)readFile; // decote as char and concatenate singel positions
            }
        }
        // close the file:
        myFile.close();
    }
    else
    {
        // if the file didn’t open, print an error:
        Serial.println("error opening test.txt");
    }
}