#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_TinyUSB.h>
#include "Adafruit_MPRLS.h"

#define RESET_PIN -1 // set to any GPIO pin # to hard-reset on begin()
#define EOC_PIN -1 // set to any GPIO pin to read end-of-conversion by pin

#define LED_RED 5
#define LED_GREEN 10
#define BTN_RED 9
#define BTN_GREEN 7

Adafruit_MPRLS pressureSensor = Adafruit_MPRLS(RESET_PIN, EOC_PIN);
float pressure_hPa = 0;
int count = 0; //number of seconds
volatile bool panicFlag = false;  //flag to signal that an interrupt has been detected
unsigned long startTimer = 0;     // Timestamp of the last measurement
unsigned long endTimer = 0;       // 
unsigned long sensorSpeed = 0;    // Time it took the sensor to measure the pressure and communicate to the uC
unsigned long measPeriod = 100;   // Set the measurement interval to 10ms
unsigned long realPeriod = 0;
unsigned long timeStamp = 0;      // Set at the beginning to achieve constant measurement period

void interruptFunction();

void setup() {
  // Inputs
  pinMode(BTN_RED, INPUT_PULLUP);
  pinMode(BTN_GREEN, INPUT_PULLUP);

  // Outputs
  pinMode(LED_GREEN, OUTPUT);
  digitalWrite(LED_GREEN, LOW);
  
  pinMode(LED_RED, OUTPUT);
  digitalWrite(LED_RED, LOW);

  pinMode(LED_BUILTIN, OUTPUT);   
  digitalWrite(LED_BUILTIN, LOW);

  //pinMode(PIN_BUTTON1, INPUT_PULLUP);   //Declare the pin as interrupt, interrupts can be detected also by RISING, FALLING and level
  attachInterrupt(digitalPinToInterrupt(BTN_RED), interruptFunction, CHANGE);
  
  digitalWrite(LED_BUILTIN, LOW);

  Serial.begin(115200);
  Serial.println("MPRLS Simple Test");
  if (! pressureSensor.begin()) {
    Serial.println("Failed to communicate with MPRLS sensor, check wiring?");
    while (1) {
      delay(10);
    }
  }
  Serial.println("Found MPRLS sensor");
}

void loop() {

  if(panicFlag) {
    Serial.println("Panic! Operation has been halted. Reset to continue.");
    digitalWrite(LED_RED, HIGH);
    delay(1000);
    return;
  }

  timeStamp = millis();

  if (timeStamp - startTimer >= measPeriod) {
    startTimer = timeStamp;

    pressure_hPa = pressureSensor.readPressure();
    sensorSpeed = millis() - startTimer;

    Serial.print("Pressure (hPa): "); //Print the measured pressure
    Serial.println(pressure_hPa);
    Serial.print("Read-out speed (ms): "); //Print the read-out time of one measurement
    Serial.println(sensorSpeed);
    Serial.print("Measurement Interval: "); //Print your set measurement interval
    Serial.println(realPeriod);

    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
  }
  /* Put your code here */
  //Serial.print("Time (s): ");
  //Serial.println(count);
}

void interruptFunction(){
  panicFlag = true;
}