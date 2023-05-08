#include <Arduino.h>

// Definition of pins
// ItsyBitsyNRF
#define SWITCH 4
// chassis
#define PUMP 9
#define VENT 10
// Buttons & LEDs
#define BUT_G 12
#define LED_G 11
#define BUT_R 7
#define LED_R 5
// Waveshare 17344 Display
#define TFT_CS 17
#define TFT_DC 16
#define TFT_RST 15
#define TFT_BL 14
// MPRLS Sensor
#define MPRLS_RST 19
#define MPRLS_EOC 18
// Pin 2 MUST not be used !
/* **************************************************************** */
/* ********** Don’t change blow ! ******************* */
/* **************************************************************** */
#include <Arduino.h>
#include <Adafruit_TinyUSB.h> // for Serial
#include <Wire.h>
#include <Adafruit_MPRLS.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <SPI.h>
#include <SdFat.h>                // SD card & FAT filesystem library
#include <Adafruit_SPIFlash.h>    // SPI / QSPI flash library
#include <Adafruit_ImageReader.h> // Image-reading functions
// SPI or QSPI flash filesystem (i.e. CIRCUITPY drive)
#if defined(__SAMD51__) || defined(NRF52840_XXAA)
Adafruit_FlashTransport_QSPI flashTransport(PIN_QSPI_SCK,
                                            PIN_QSPI_CS, PIN_QSPI_IO0, PIN_QSPI_IO1, PIN_QSPI_IO2,
                                            PIN_QSPI_IO3);
#else
#if (SPI_INTERFACES_COUNT == 1)
Adafruit_FlashTransport_SPI flashTransport(SS, &SPI);
#else
Adafruit_FlashTransport_SPI flashTransport(SS1, &SPI1);

#endif
#endif
Adafruit_SPIFlash flash(&flashTransport);
FatFileSystem filesys;
Adafruit_ImageReader reader(filesys); // Image-reader
#ifdef MPRLS_RST
#ifdef MPRLS_EOC
Adafruit_MPRLS mpr = Adafruit_MPRLS(MPRLS_RST, MPRLS_EOC);
#endif
#else
Adafruit_MPRLS mpr = Adafruit_MPRLS();
#endif
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
int i = 0;
void setup()
{
  pinMode(PUMP, OUTPUT);
  pinMode(VENT, OUTPUT);
  pinMode(BUT_G, INPUT_PULLUP); // PULLUP and switch closes to GND
  pinMode(LED_G, OUTPUT);
  pinMode(BUT_R, INPUT_PULLUP); // PULLUP and switch closes to GND
  pinMode(LED_R, OUTPUT);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(PUMP, HIGH);
  digitalWrite(VENT, HIGH);
  digitalWrite(LED_G, HIGH);
  digitalWrite(LED_R, HIGH);
  digitalWrite(TFT_BL, LOW);
  delay(300);
  digitalWrite(PUMP, LOW);
  digitalWrite(VENT, LOW);
  digitalWrite(LED_G, LOW);
  digitalWrite(LED_R, LOW);
  digitalWrite(TFT_BL, HIGH);

  Serial.begin(115200);
  delay(100);
  Serial.println("Here we go.");
  tft.init(240, 320);
  tft.setRotation(3);
  tft.setTextSize(3);
  tft.fillScreen(ST77XX_BLACK);
  if (!flash.begin())
  {
    Serial.println(F("flash begin() failed"));
  }
  if (!filesys.begin(&flash))
  {
    Serial.println(F("filesys begin() failed"));
  }
  if (!mpr.begin())
  {
    Serial.println("Failed to communicate with MPRLS sensor, check wiring?");
    tft.setCursor(0, 20);
    tft.print("Failed to communicate with MPRLS sensor\n");

    while (i < 10)
    {
      i++;
      delay(10);
    }
  }
  Serial.println("Found MPRLS sensor");
  Serial.print("Pressure (hPa): ");
  Serial.println(mpr.readPressure());
  tft.setCursor(0, 0);
  tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
  tft.print("Found MPRLS sensor");
  tft.setCursor(0, 20);
  tft.print("Pressure(hPa)\n");
  tft.print(int(mpr.readPressure()));
  tft.print("\n");
  Serial.print("Pressure(hPa)\n");
  Serial.print(int(mpr.readPressure()));
  Serial.print("\n");
  delay(1000);
}
void loop()
{
  if (digitalRead(BUT_G) == LOW)
  { // PULLUP and switch closes to GND
    digitalWrite(LED_G, HIGH);
    digitalWrite(PUMP, HIGH);
  }
  else
  {
    digitalWrite(LED_G, LOW);
    digitalWrite(PUMP, LOW);
  }
  if (digitalRead(BUT_R) == LOW)
  { // PULLUP and switch closes to GND
    digitalWrite(LED_R, HIGH);
    digitalWrite(VENT, HIGH);
  }
  else
  {
    digitalWrite(LED_R, LOW);
    digitalWrite(VENT, LOW);
  }
  tft.setTextColor(ST77XX_RED, ST77XX_WHITE);
  tft.setCursor(0, 0);
  tft.print("Pressure(hPa)\n");
  tft.print(int(mpr.readPressure()));
  tft.print("\n");
  Serial.print("Pressure(hPa)\n");
  Serial.print(int(mpr.readPressure()));
  Serial.print("\n");
  reader.drawBMP("heart.bmp", tft, 0, 0);
  delay(100);
}