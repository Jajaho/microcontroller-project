#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>     // Core graphics library
#include <Adafruit_ST7789.h>  // Hardware-specific library for ST7789
#include <Adafruit_TinyUSB.h>
#include <Adafruit_MPRLS.h>

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

#define SCREEN_HEIGHT 240
#define SCREEN_WIDTH 320
//input: chip select; data/command and reset pin -> reqquired for the library
Adafruit_ST7789 display = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

Adafruit_MPRLS pressureSensor = Adafruit_MPRLS(RESET_PIN, EOC_PIN);

float pressure_hPa = 0;
float targetPressure = 1000;

volatile bool panicFlag = false;  //flag to signal that an interrupt has been detected
volatile bool pumpFlag = false;

unsigned long startTimer = 0;     // Timestamp of the last measurement
unsigned long sensorSpeed = 0;    // Time it took the sensor to measure the pressure and communicate to the uC
unsigned long measPeriod = 300;   // Set the measurement interval to 10ms
unsigned long timeStamp = 0;      // Set at the beginning to achieve constant measurement period

int16_t xCursor = 70;
int16_t yCursor = 130;

void panicISR();
void pumpControlISR();

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

  attachInterrupt(digitalPinToInterrupt(BTN_RED), panicISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(BTN_GREEN), pumpControlISR, FALLING);

  Serial.begin(115200);
  Serial.println("Initializing MPRLS");
  if (! pressureSensor.begin()) {
    Serial.println("Failed to communicate with MPRLS sensor, check wiring?");
    while (1) {
      delay(10);
    }
  }
  Serial.println("MPRLS sensor initialized.");

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

  if(panicFlag) {
    Serial.println("Panic! Operation has been halted. Reset to continue.");
    digitalWrite(LED_RED, HIGH);
    digitalWrite(PUMP, LOW);
    digitalWrite(BTN_GREEN, LOW);
  }
  else {
    digitalWrite(LED_RED, LOW);
  }

  if(pumpFlag)
  {
    digitalWrite(PUMP, HIGH);
    digitalWrite(LED_GREEN, HIGH);
  }
  else {
    digitalWrite(PUMP, LOW);
    digitalWrite(LED_GREEN, LOW);
  }
  
  if (pumpFlag && (pressure_hPa >= targetPressure))
  {
    pumpFlag = false;
  }
  
  

  timeStamp = millis();
  if (timeStamp - startTimer >= measPeriod) {
    if (pumpFlag) {
      digitalWrite(VALVE, LOW);
      delay(6);
    }
    
    startTimer = timeStamp; // set the timestamp of the current measurement

    pressure_hPa = pressureSensor.readPressure();
    sensorSpeed = millis() - startTimer;
    
    display.fillScreen(ST77XX_BLACK);
    display.setCursor(xCursor, yCursor);
    display.print(pressure_hPa);

    Serial.print("Pressure (hPa): "); //Print the measured pressure
    Serial.println(pressure_hPa);
    Serial.print("Read-out speed (ms): "); //Print the read-out time of one measurement
    Serial.println(sensorSpeed);

    /*
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
    */  
    Serial.print("panicFlag: ");
    Serial.println(panicFlag);
    Serial.print("pumpFlag: ");
    Serial.println(pumpFlag);
  }
  
}

void panicISR() {
  panicFlag = true;
  pumpFlag = false;
}

void pumpControlISR() {
  pumpFlag = true;
  panicFlag = false;
}